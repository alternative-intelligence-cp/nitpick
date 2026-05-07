/**
 * Aria User Hash Table (ahash) — Runtime Implementation
 * v0.4.5: Scope-local, type-safe, capacity-bounded hash table with string keys.
 *
 * Storage: heap-allocated open-addressing hash table with linear probing.
 * Each bucket stores: key (heap-allocated copy), value (int64), tag (int64).
 * Tombstone deletion is NOT supported (no ahdelete yet).
 *
 * Hash function: FNV-1a 64-bit.
 * Load factor: grows at 75% occupancy, doubling bucket count.
 * All fatal errors call exit(1) with diagnostic to stderr.
 */

#include "runtime/uhash.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>

/* ═══════════════════════════════════════════════════════════════════════
 * Internal structures
 * ═══════════════════════════════════════════════════════════════════════ */

struct UHashEntry {
    char*   key;        /* Heap-allocated copy, NULL = empty bucket */
    int64_t value;
    int64_t type_tag;
    int64_t value_size; /* Logical size for capacity accounting */
};

struct AriaUHash {
    UHashEntry* buckets;
    int64_t     bucket_count;
    int64_t     entry_count;    /* Number of live entries */
    int64_t     capacity_bytes; /* Byte budget (0 = unbounded) */
    int64_t     used_bytes;     /* Current payload bytes consumed */
};

/* ═══════════════════════════════════════════════════════════════════════
 * FNV-1a hash
 * ═══════════════════════════════════════════════════════════════════════ */

static uint64_t fnv1a(const char* key) {
    uint64_t hash = 14695981039346656037ULL; /* FNV offset basis */
    for (const char* p = key; *p; ++p) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(*p));
        hash *= 1099511628211ULL; /* FNV prime */
    }
    return hash;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Initial bucket count — start small, grow on demand
 * ═══════════════════════════════════════════════════════════════════════ */

#define UHASH_INITIAL_BUCKETS 16
#define UHASH_LOAD_FACTOR_NUM 3  /* Grow when entry_count * 4 >= bucket_count * 3 */
#define UHASH_LOAD_FACTOR_DEN 4  /* i.e., 75% */

/* Tombstone marker — invalid pointer (0x1) marks deleted buckets.
 * uhash_find skips tombstones during probing; insert reuses them. */
#define UHASH_TOMBSTONE ((char*)1)

/* ═══════════════════════════════════════════════════════════════════════
 * Regular (type-checked) implementation
 * ═══════════════════════════════════════════════════════════════════════ */

static void uhash_grow(AriaUHash* ht) {
    int64_t new_count = ht->bucket_count * 2;
    UHashEntry* new_buckets = static_cast<UHashEntry*>(
        calloc(static_cast<size_t>(new_count), sizeof(UHashEntry)));
    if (!new_buckets) {
        fprintf(stderr, "aria: uhash grow failed — out of memory\n");
        exit(1);
    }

    /* Rehash all live entries (skip tombstones) */
    for (int64_t i = 0; i < ht->bucket_count; ++i) {
        if (ht->buckets[i].key && ht->buckets[i].key != UHASH_TOMBSTONE) {
            uint64_t h = fnv1a(ht->buckets[i].key);
            int64_t idx = static_cast<int64_t>(h % static_cast<uint64_t>(new_count));
            while (new_buckets[idx].key) {
                idx = (idx + 1) % new_count;
            }
            new_buckets[idx] = ht->buckets[i]; /* Move entry (key pointer transfers) */
        }
    }

    free(ht->buckets);
    ht->buckets = new_buckets;
    ht->bucket_count = new_count;
}

static int64_t uhash_find(AriaUHash* ht, const char* key) {
    uint64_t h = fnv1a(key);
    int64_t idx = static_cast<int64_t>(h % static_cast<uint64_t>(ht->bucket_count));
    int64_t start = idx;
    do {
        if (!ht->buckets[idx].key) return -1; /* Empty — not found */
        if (ht->buckets[idx].key != UHASH_TOMBSTONE &&
            strcmp(ht->buckets[idx].key, key) == 0) return idx;
        idx = (idx + 1) % ht->bucket_count;
    } while (idx != start);
    return -1; /* Full table, not found */
}

extern "C" int64_t npk_uhash_new(int64_t capacity_bytes) {
    AriaUHash* ht = static_cast<AriaUHash*>(malloc(sizeof(AriaUHash)));
    if (!ht) return 0;

    ht->buckets = static_cast<UHashEntry*>(
        calloc(UHASH_INITIAL_BUCKETS, sizeof(UHashEntry)));
    if (!ht->buckets) {
        free(ht);
        return 0;
    }

    ht->bucket_count   = UHASH_INITIAL_BUCKETS;
    ht->entry_count    = 0;
    ht->capacity_bytes = capacity_bytes;
    ht->used_bytes     = 0;

    return reinterpret_cast<int64_t>(ht);
}

extern "C" void npk_uhash_destroy(int64_t handle) {
    if (handle == 0) return;
    AriaUHash* ht = reinterpret_cast<AriaUHash*>(handle);
    for (int64_t i = 0; i < ht->bucket_count; ++i) {
        if (ht->buckets[i].key && ht->buckets[i].key != UHASH_TOMBSTONE) {
            free(ht->buckets[i].key);
        }
    }
    free(ht->buckets);
    free(ht);
}

extern "C" int32_t npk_uhash_set(int64_t handle, const char* key, int64_t value,
                                   int64_t type_tag, int64_t value_size) {
    if (handle == 0) {
        fprintf(stderr, "aria: uhash error — set on null handle\n");
        exit(1);
    }
    AriaUHash* ht = reinterpret_cast<AriaUHash*>(handle);

    /* Check for existing key (upsert) */
    int64_t existing = uhash_find(ht, key);
    if (existing >= 0) {
        /* Upsert — update value, tag, adjust capacity accounting */
        int64_t old_size = ht->buckets[existing].value_size;
        if (ht->capacity_bytes > 0) {
            int64_t delta = value_size - old_size;
            if (delta > 0 && ht->used_bytes + delta > ht->capacity_bytes) {
                /* B7 FIX: Auto-grow capacity instead of failing */
                int64_t needed = ht->used_bytes + delta;
                while (ht->capacity_bytes < needed) {
                    ht->capacity_bytes = (ht->capacity_bytes < 64) ? 64 : ht->capacity_bytes * 2;
                }
            }
            ht->used_bytes += delta;
        }
        ht->buckets[existing].value      = value;
        ht->buckets[existing].type_tag   = type_tag;
        ht->buckets[existing].value_size = value_size;
        return 0;
    }

    /* New entry — check capacity, auto-grow if needed (B7 FIX) */
    if (ht->capacity_bytes > 0 && ht->used_bytes + value_size > ht->capacity_bytes) {
        int64_t needed = ht->used_bytes + value_size;
        while (ht->capacity_bytes < needed) {
            ht->capacity_bytes = (ht->capacity_bytes < 64) ? 64 : ht->capacity_bytes * 2;
        }
    }

    /* Grow if needed (check load factor before insert) */
    if (ht->entry_count * UHASH_LOAD_FACTOR_DEN >= ht->bucket_count * UHASH_LOAD_FACTOR_NUM) {
        uhash_grow(ht);
    }

    /* Find empty bucket or tombstone slot */
    uint64_t h = fnv1a(key);
    int64_t idx = static_cast<int64_t>(h % static_cast<uint64_t>(ht->bucket_count));
    while (ht->buckets[idx].key && ht->buckets[idx].key != UHASH_TOMBSTONE) {
        idx = (idx + 1) % ht->bucket_count;
    }

    /* Insert */
    size_t key_len = strlen(key);
    ht->buckets[idx].key = static_cast<char*>(malloc(key_len + 1));
    if (!ht->buckets[idx].key) {
        fprintf(stderr, "aria: uhash error — key allocation failed\n");
        exit(1);
    }
    memcpy(ht->buckets[idx].key, key, key_len + 1);
    ht->buckets[idx].value      = value;
    ht->buckets[idx].type_tag   = type_tag;
    ht->buckets[idx].value_size = value_size;

    ht->entry_count++;
    if (ht->capacity_bytes > 0) {
        ht->used_bytes += value_size;
    } else {
        ht->used_bytes += value_size; /* Track even when unbounded for ahsize() */
    }

    return 0;
}

extern "C" int64_t npk_uhash_get(int64_t handle, const char* key, int64_t expected_tag) {
    if (handle == 0) {
        fprintf(stderr, "aria: uhash error — get on null handle\n");
        exit(1);
    }
    AriaUHash* ht = reinterpret_cast<AriaUHash*>(handle);

    int64_t idx = uhash_find(ht, key);
    if (idx < 0) {
        return 0; /* Key not found — return NIL/zero sentinel */
    }

    if (expected_tag >= 0 && ht->buckets[idx].type_tag != expected_tag) {
        fprintf(stderr, "aria: uhash type mismatch — key \"%s\" stored tag %ld, expected %ld\n",
                key, (long)ht->buckets[idx].type_tag, (long)expected_tag);
        /* Trigger failsafe */
        exit(1);
    }

    return ht->buckets[idx].value;
}

extern "C" void* npk_uhash_keys(int64_t handle, int64_t* out_count) {
    if (handle == 0) {
        *out_count = 0;
        return nullptr;
    }
    AriaUHash* ht = reinterpret_cast<AriaUHash*>(handle);

    if (ht->entry_count == 0) {
        *out_count = 0;
        return nullptr;
    }

    /* AriaString layout: { char* data, int64_t length } = 16 bytes each */
    struct AriaString {
        char*   data;
        int64_t length;
    };

    AriaString* result = static_cast<AriaString*>(
        malloc(static_cast<size_t>(ht->entry_count) * sizeof(AriaString)));
    if (!result) {
        fprintf(stderr, "aria: uhash keys allocation failed\n");
        exit(1);
    }

    int64_t n = 0;
    for (int64_t i = 0; i < ht->bucket_count && n < ht->entry_count; ++i) {
        if (ht->buckets[i].key && ht->buckets[i].key != UHASH_TOMBSTONE) {
            size_t len = strlen(ht->buckets[i].key);
            char* copy = static_cast<char*>(malloc(len + 1));
            if (!copy) {
                fprintf(stderr, "aria: uhash key copy allocation failed\n");
                exit(1);
            }
            memcpy(copy, ht->buckets[i].key, len + 1);
            result[n].data   = copy;
            result[n].length = static_cast<int64_t>(len);
            n++;
        }
    }

    *out_count = n;
    return static_cast<void*>(result);
}

extern "C" int64_t npk_uhash_size(int64_t handle) {
    if (handle == 0) return 0;
    AriaUHash* ht = reinterpret_cast<AriaUHash*>(handle);
    return ht->used_bytes;
}

extern "C" int64_t npk_uhash_fits(int64_t handle, int64_t value_size) {
    if (handle == 0) return 0;
    AriaUHash* ht = reinterpret_cast<AriaUHash*>(handle);
    if (ht->capacity_bytes <= 0) return 1; /* Unbounded — always fits */
    return (ht->used_bytes + value_size <= ht->capacity_bytes) ? 1 : 0;
}

extern "C" int32_t npk_uhash_type(int64_t handle, const char* key) {
    if (handle == 0) return -1;
    AriaUHash* ht = reinterpret_cast<AriaUHash*>(handle);
    int64_t idx = uhash_find(ht, key);
    if (idx < 0) return -1;
    return static_cast<int32_t>(ht->buckets[idx].type_tag);
}

extern "C" int64_t npk_uhash_count(int64_t handle) {
    if (handle == 0) return 0;
    AriaUHash* ht = reinterpret_cast<AriaUHash*>(handle);
    return ht->entry_count;
}

extern "C" int32_t npk_uhash_delete(int64_t handle, const char* key) {
    if (handle == 0) return -1;
    AriaUHash* ht = reinterpret_cast<AriaUHash*>(handle);
    int64_t idx = uhash_find(ht, key);
    if (idx < 0) return -1; /* Key not found */

    /* Free the key string and mark as tombstone */
    int64_t freed_size = ht->buckets[idx].value_size;
    free(ht->buckets[idx].key);
    ht->buckets[idx].key        = UHASH_TOMBSTONE;
    ht->buckets[idx].value      = 0;
    ht->buckets[idx].type_tag   = 0;
    ht->buckets[idx].value_size = 0;

    ht->entry_count--;
    ht->used_bytes -= freed_size;
    return 0; /* Success */
}

extern "C" int32_t npk_uhash_has(int64_t handle, const char* key) {
    if (handle == 0) return 0;
    AriaUHash* ht = reinterpret_cast<AriaUHash*>(handle);
    return (uhash_find(ht, key) >= 0) ? 1 : 0;
}

extern "C" void npk_uhash_clear(int64_t handle) {
    if (handle == 0) return;
    AriaUHash* ht = reinterpret_cast<AriaUHash*>(handle);

    /* Free all live keys (skip tombstones and empty) */
    for (int64_t i = 0; i < ht->bucket_count; ++i) {
        if (ht->buckets[i].key && ht->buckets[i].key != UHASH_TOMBSTONE) {
            free(ht->buckets[i].key);
        }
        ht->buckets[i].key        = nullptr;
        ht->buckets[i].value      = 0;
        ht->buckets[i].type_tag   = 0;
        ht->buckets[i].value_size = 0;
    }

    ht->entry_count = 0;
    ht->used_bytes  = 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Fast-path (SMT-proven type-homogeneous) implementation
 *
 * Uses the SAME AriaUHash/UHashEntry structs as the regular path so that
 * handles are fully interchangeable between fast and non-fast functions.
 * The only difference: fast variants skip writing/reading type_tag and
 * omit the tag parameter, saving codegen overhead (fewer IR instructions,
 * no tag computation at call sites).
 * ═══════════════════════════════════════════════════════════════════════ */

extern "C" int64_t npk_uhash_new_fast(int64_t capacity_bytes) {
    /* Identical to npk_uhash_new — same struct, interchangeable handle */
    return npk_uhash_new(capacity_bytes);
}

extern "C" void npk_uhash_destroy_fast(int64_t handle) {
    npk_uhash_destroy(handle);
}

extern "C" int32_t npk_uhash_set_fast(int64_t handle, const char* key, int64_t value,
                                        int64_t value_size) {
    /* Delegate to regular set with type_tag = -1 (untagged) */
    return npk_uhash_set(handle, key, value, -1, value_size);
}

extern "C" int64_t npk_uhash_get_fast(int64_t handle, const char* key) {
    /* Delegate to regular get with expected_tag = -1 (skip type check) */
    return npk_uhash_get(handle, key, -1);
}

extern "C" void* npk_uhash_keys_fast(int64_t handle, int64_t* out_count) {
    return npk_uhash_keys(handle, out_count);
}

extern "C" int64_t npk_uhash_size_fast(int64_t handle) {
    return npk_uhash_size(handle);
}

extern "C" int64_t npk_uhash_fits_fast(int64_t handle, int64_t value_size) {
    return npk_uhash_fits(handle, value_size);
}

extern "C" int64_t npk_uhash_count_fast(int64_t handle) {
    return npk_uhash_count(handle);
}

extern "C" int32_t npk_uhash_delete_fast(int64_t handle, const char* key) {
    return npk_uhash_delete(handle, key);
}

extern "C" int32_t npk_uhash_has_fast(int64_t handle, const char* key) {
    return npk_uhash_has(handle, key);
}

extern "C" void npk_uhash_clear_fast(int64_t handle) {
    npk_uhash_clear(handle);
}
