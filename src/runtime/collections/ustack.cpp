/**
 * Aria User Stack (astack) — Runtime Implementation
 * v0.4.3: Per-scope implicit typed LIFO scratch pad
 *
 * Storage: contiguous mmap region, 16 bytes per slot (8 value + 8 tag).
 * Uses a simple bump pointer for O(1) push/pop.
 * All errors are fatal (exit(1)) — the compiler guarantees type safety
 * via tags, and the user is responsible for not overflowing/underflowing.
 */

#include "runtime/ustack.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>

/* Internal stack structure — pointed to by the opaque handle */
struct AriaUStack {
    int64_t* data;       /* Interleaved [value, tag, value, tag, ...] */
    int64_t  capacity;   /* Max slots */
    int64_t  size;       /* Current slot count */
    int64_t  data_bytes; /* mmap region size for munmap */
};

/* ═══════════════════════════════════════════════════════════════════════
 * Creation / Destruction
 * ═══════════════════════════════════════════════════════════════════════ */

extern "C" int64_t npk_ustack_new(int64_t capacity) {
    if (capacity <= 0) {
        return 0;
    }

    /* Allocate the control struct on the heap */
    AriaUStack* stk = static_cast<AriaUStack*>(malloc(sizeof(AriaUStack)));
    if (!stk) {
        return 0;
    }

    /* Allocate the data region via mmap (lazy page allocation, no page faults
       until actually touched — this is the "zero cost when unused" part) */
    int64_t data_bytes = capacity * 16; /* 16 bytes per slot */
    void* region = mmap(nullptr, static_cast<size_t>(data_bytes),
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        free(stk);
        return 0;
    }

    stk->data       = static_cast<int64_t*>(region);
    stk->capacity   = capacity;
    stk->size       = 0;
    stk->data_bytes = data_bytes;

    return reinterpret_cast<int64_t>(stk);
}

extern "C" void npk_ustack_destroy(int64_t handle) {
    if (handle == 0) return;

    AriaUStack* stk = reinterpret_cast<AriaUStack*>(handle);
    if (stk->data) {
        munmap(stk->data, static_cast<size_t>(stk->data_bytes));
    }
    free(stk);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Push / Pop / Peek
 * ═══════════════════════════════════════════════════════════════════════ */

extern "C" void npk_ustack_push(int64_t handle, int64_t value, int64_t type_tag) {
    if (handle == 0) {
        fprintf(stderr, "aria: user stack error — push on null handle (missing astack() in scope?)\n");
        exit(1);
    }

    AriaUStack* stk = reinterpret_cast<AriaUStack*>(handle);

    if (stk->size >= stk->capacity) {
        fprintf(stderr, "aria: user stack overflow — capacity %lld exceeded\n",
                (long long)stk->capacity);
        exit(1);
    }

    int64_t idx = stk->size * 2; /* 2 int64s per slot */
    stk->data[idx]     = value;
    stk->data[idx + 1] = type_tag;
    stk->size++;
}

extern "C" int64_t npk_ustack_pop(int64_t handle, int64_t expected_tag) {
    if (handle == 0) {
        fprintf(stderr, "aria: user stack error — pop on null handle\n");
        exit(1);
    }

    AriaUStack* stk = reinterpret_cast<AriaUStack*>(handle);

    if (stk->size <= 0) {
        fprintf(stderr, "aria: user stack underflow — pop on empty stack\n");
        exit(1);
    }

    stk->size--;
    int64_t idx   = stk->size * 2;
    int64_t value = stk->data[idx];
    int64_t tag   = stk->data[idx + 1];

    if (expected_tag >= 0 && tag != expected_tag) {
        static const char* tag_names[] = {
            "int8", "int16", "int32", "int64",
            "flt32", "flt64", "bool", "string", "pointer"
        };
        const char* expected_name = (expected_tag >= 0 && expected_tag <= 8)
            ? tag_names[expected_tag] : "unknown";
        const char* actual_name = (tag >= 0 && tag <= 8)
            ? tag_names[tag] : "unknown";
        fprintf(stderr,
            "aria: user stack type mismatch — expected %s (tag %lld), got %s (tag %lld)\n",
            expected_name, (long long)expected_tag, actual_name, (long long)tag);
        exit(1);
    }

    return value;
}

extern "C" int64_t npk_ustack_peek(int64_t handle, int64_t expected_tag) {
    if (handle == 0) {
        fprintf(stderr, "aria: user stack error — peek on null handle\n");
        exit(1);
    }

    AriaUStack* stk = reinterpret_cast<AriaUStack*>(handle);

    if (stk->size <= 0) {
        fprintf(stderr, "aria: user stack underflow — peek on empty stack\n");
        exit(1);
    }

    int64_t idx   = (stk->size - 1) * 2;
    int64_t value = stk->data[idx];
    int64_t tag   = stk->data[idx + 1];

    if (expected_tag >= 0 && tag != expected_tag) {
        static const char* tag_names[] = {
            "int8", "int16", "int32", "int64",
            "flt32", "flt64", "bool", "string", "pointer"
        };
        const char* expected_name = (expected_tag >= 0 && expected_tag <= 8)
            ? tag_names[expected_tag] : "unknown";
        const char* actual_name = (tag >= 0 && tag <= 8)
            ? tag_names[tag] : "unknown";
        fprintf(stderr,
            "aria: user stack type mismatch — expected %s (tag %lld), got %s (tag %lld)\n",
            expected_name, (long long)expected_tag, actual_name, (long long)tag);
        exit(1);
    }

    return value;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Query
 * ═══════════════════════════════════════════════════════════════════════ */

extern "C" int64_t npk_ustack_size(int64_t handle) {
    if (handle == 0) return 0;
    AriaUStack* stk = reinterpret_cast<AriaUStack*>(handle);
    return stk->size;
}

extern "C" int64_t npk_ustack_capacity_bytes(int64_t handle) {
    if (handle == 0) return 0;
    AriaUStack* stk = reinterpret_cast<AriaUStack*>(handle);
    return stk->data_bytes;
}

extern "C" int64_t npk_ustack_bytes_used(int64_t handle) {
    if (handle == 0) return 0;
    AriaUStack* stk = reinterpret_cast<AriaUStack*>(handle);
    return stk->size * 16;  /* 16 bytes per slot (value + tag) */
}

extern "C" int64_t npk_ustack_fits(int64_t handle) {
    if (handle == 0) return 0;
    AriaUStack* stk = reinterpret_cast<AriaUStack*>(handle);
    return (stk->size < stk->capacity) ? 1 : 0;
}

extern "C" int64_t npk_ustack_top_type(int64_t handle) {
    if (handle == 0) return -1;
    AriaUStack* stk = reinterpret_cast<AriaUStack*>(handle);
    if (stk->size <= 0) return -1;
    int64_t idx = (stk->size - 1) * 2;
    return stk->data[idx + 1];  /* tag is second int64 in each slot */
}

/* ═══════════════════════════════════════════════════════════════════════
 * SMT-Optimized Fast Variants (v0.4.3+)
 *
 * When Z3 proves type homogeneity for all pushes in a scope, the compiler
 * emits these instead of the tagged versions.  Layout: 8 bytes/slot
 * (values only, no tag array).  No bounds checking, no type validation.
 * The proof guarantees safety.
 * ═══════════════════════════════════════════════════════════════════════ */

struct AriaUStackFast {
    int64_t* data;       /* Values only — no tag interleaving */
    int64_t  capacity;
    int64_t  size;
    int64_t  data_bytes; /* mmap region size for munmap */
};

extern "C" int64_t npk_ustack_new_fast(int64_t capacity) {
    if (capacity <= 0) return 0;

    AriaUStackFast* stk = static_cast<AriaUStackFast*>(malloc(sizeof(AriaUStackFast)));
    if (!stk) return 0;

    int64_t data_bytes = capacity * 8;  /* 8 bytes per slot (no tags) */
    void* region = mmap(nullptr, static_cast<size_t>(data_bytes),
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        free(stk);
        return 0;
    }

    stk->data       = static_cast<int64_t*>(region);
    stk->capacity   = capacity;
    stk->size       = 0;
    stk->data_bytes = data_bytes;

    return reinterpret_cast<int64_t>(stk);
}

extern "C" void npk_ustack_destroy_fast(int64_t handle) {
    if (handle == 0) return;
    AriaUStackFast* stk = reinterpret_cast<AriaUStackFast*>(handle);
    if (stk->data) {
        munmap(stk->data, static_cast<size_t>(stk->data_bytes));
    }
    free(stk);
}

extern "C" void npk_ustack_push_fast(int64_t handle, int64_t value) {
    AriaUStackFast* stk = reinterpret_cast<AriaUStackFast*>(handle);
    stk->data[stk->size++] = value;
}

extern "C" int64_t npk_ustack_pop_fast(int64_t handle) {
    AriaUStackFast* stk = reinterpret_cast<AriaUStackFast*>(handle);
    return stk->data[--stk->size];
}

extern "C" int64_t npk_ustack_peek_fast(int64_t handle) {
    AriaUStackFast* stk = reinterpret_cast<AriaUStackFast*>(handle);
    return stk->data[stk->size - 1];
}

extern "C" int64_t npk_ustack_bytes_used_fast(int64_t handle) {
    AriaUStackFast* stk = reinterpret_cast<AriaUStackFast*>(handle);
    return stk->size * 8;  /* 8 bytes per slot (value only, no tag) */
}

extern "C" int64_t npk_ustack_top_type_fast(int64_t handle) {
    (void)handle;
    return -1;  /* Fast mode has no type tags — Z3 proved homogeneity */
}
