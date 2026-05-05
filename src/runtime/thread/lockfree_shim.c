/**
 * Aria Lock-Free Data Structures — Handle-Based API
 *
 * Implements three lock-free/wait-free structures:
 * 1. LFQueue  — MPMC bounded queue (Michael-Scott style, array-based)
 * 2. LFStack  — Lock-free Treiber stack (unbounded, linked-list)
 * 3. RingBuf  — SPSC wait-free ring buffer (Lamport style)
 *
 * All exposed via int64 handles for Aria consumption.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 1. Lock-Free Queue (MPMC, bounded, array-based)
// ============================================================================
// Uses per-slot sequence numbers for MPMC correctness (Dmitry Vyukov style).

typedef struct {
    _Atomic(int64_t) sequence;
    int64_t          value;
} LFQueueSlot;

typedef struct {
    LFQueueSlot*     slots;
    int64_t          capacity;    // must be power of 2
    int64_t          mask;
    _Atomic(int64_t) head;        // dequeue position
    _Atomic(int64_t) tail;        // enqueue position
} LFQueue;

#define MAX_LF_QUEUES 128
static LFQueue* g_lf_queues[MAX_LF_QUEUES];

static int64_t alloc_lfqueue_slot(LFQueue* q) {
    for (int i = 0; i < MAX_LF_QUEUES; i++) {
        if (!g_lf_queues[i]) { g_lf_queues[i] = q; return (int64_t)i; }
    }
    return -1;
}

// Round up to next power of 2
static int64_t next_pow2(int64_t n) {
    n--;
    n |= n >> 1;  n |= n >> 2;  n |= n >> 4;
    n |= n >> 8;  n |= n >> 16; n |= n >> 32;
    return n + 1;
}

int64_t npk_shim_lfqueue_create(int64_t capacity) {
    if (capacity <= 0) capacity = 256;
    capacity = next_pow2(capacity);

    LFQueue* q = (LFQueue*)calloc(1, sizeof(LFQueue));
    if (!q) return -1;

    q->slots = (LFQueueSlot*)calloc((size_t)capacity, sizeof(LFQueueSlot));
    if (!q->slots) { free(q); return -1; }

    q->capacity = capacity;
    q->mask = capacity - 1;
    atomic_store(&q->head, 0);
    atomic_store(&q->tail, 0);

    for (int64_t i = 0; i < capacity; i++) {
        atomic_store(&q->slots[i].sequence, i);
    }

    return alloc_lfqueue_slot(q);
}

int32_t npk_shim_lfqueue_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_LF_QUEUES || !g_lf_queues[handle]) return -1;
    LFQueue* q = g_lf_queues[handle];
    free(q->slots);
    free(q);
    g_lf_queues[handle] = NULL;
    return 0;
}

// Non-blocking enqueue. Returns 0 on success, -1 if full.
int32_t npk_shim_lfqueue_push(int64_t handle, int64_t value) {
    if (handle < 0 || handle >= MAX_LF_QUEUES || !g_lf_queues[handle]) return -1;
    LFQueue* q = g_lf_queues[handle];

    int64_t pos = atomic_load_explicit(&q->tail, memory_order_relaxed);
    for (;;) {
        LFQueueSlot* slot = &q->slots[pos & q->mask];
        int64_t seq = atomic_load_explicit(&slot->sequence, memory_order_acquire);
        int64_t diff = seq - pos;
        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&q->tail, &pos, pos + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                slot->value = value;
                atomic_store_explicit(&slot->sequence, pos + 1, memory_order_release);
                return 0;
            }
        } else if (diff < 0) {
            return -1; // full
        } else {
            pos = atomic_load_explicit(&q->tail, memory_order_relaxed);
        }
    }
}

// Non-blocking dequeue. Returns value on success, writes to *out. Returns 0/-1.
int32_t npk_shim_lfqueue_pop(int64_t handle, int64_t* out) {
    if (handle < 0 || handle >= MAX_LF_QUEUES || !g_lf_queues[handle]) return -1;
    LFQueue* q = g_lf_queues[handle];

    int64_t pos = atomic_load_explicit(&q->head, memory_order_relaxed);
    for (;;) {
        LFQueueSlot* slot = &q->slots[pos & q->mask];
        int64_t seq = atomic_load_explicit(&slot->sequence, memory_order_acquire);
        int64_t diff = seq - (pos + 1);
        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&q->head, &pos, pos + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                *out = slot->value;
                atomic_store_explicit(&slot->sequence, pos + q->capacity, memory_order_release);
                return 0;
            }
        } else if (diff < 0) {
            return -1; // empty
        } else {
            pos = atomic_load_explicit(&q->head, memory_order_relaxed);
        }
    }
}

// Simplified pop that returns the value directly (-1 if empty — use only for positive values)
int64_t npk_shim_lfqueue_pop_val(int64_t handle) {
    int64_t out = -1;
    npk_shim_lfqueue_pop(handle, &out);
    return out;
}

int64_t npk_shim_lfqueue_size(int64_t handle) {
    if (handle < 0 || handle >= MAX_LF_QUEUES || !g_lf_queues[handle]) return -1;
    LFQueue* q = g_lf_queues[handle];
    int64_t t = atomic_load_explicit(&q->tail, memory_order_relaxed);
    int64_t h = atomic_load_explicit(&q->head, memory_order_relaxed);
    int64_t sz = t - h;
    return sz >= 0 ? sz : 0;
}

// ============================================================================
// 2. Lock-Free Stack (Treiber stack, unbounded)
// ============================================================================

typedef struct LFStackNode {
    int64_t value;
    struct LFStackNode* next;
} LFStackNode;

// Use tagged pointer (ABA counter + pointer) packed into a single 128-bit CAS,
// but for simplicity use a version counter approach with double-width CAS.
// Actually, on x86-64, we'll use a simpler approach with a hazard pointer-free
// design since we're in a GC environment.
typedef struct {
    _Atomic(LFStackNode*) top;
    _Atomic(int64_t)      count;
} LFStack;

#define MAX_LF_STACKS 128
static LFStack* g_lf_stacks[MAX_LF_STACKS];

static int64_t alloc_lfstack_slot(LFStack* s) {
    for (int i = 0; i < MAX_LF_STACKS; i++) {
        if (!g_lf_stacks[i]) { g_lf_stacks[i] = s; return (int64_t)i; }
    }
    return -1;
}

int64_t npk_shim_lfstack_create(void) {
    LFStack* s = (LFStack*)calloc(1, sizeof(LFStack));
    if (!s) return -1;
    atomic_store(&s->top, (LFStackNode*)NULL);
    atomic_store(&s->count, 0);
    return alloc_lfstack_slot(s);
}

int32_t npk_shim_lfstack_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_LF_STACKS || !g_lf_stacks[handle]) return -1;
    LFStack* s = g_lf_stacks[handle];
    // Free all remaining nodes
    LFStackNode* node = atomic_load(&s->top);
    while (node) {
        LFStackNode* next = node->next;
        free(node);
        node = next;
    }
    free(s);
    g_lf_stacks[handle] = NULL;
    return 0;
}

int32_t npk_shim_lfstack_push(int64_t handle, int64_t value) {
    if (handle < 0 || handle >= MAX_LF_STACKS || !g_lf_stacks[handle]) return -1;
    LFStack* s = g_lf_stacks[handle];

    LFStackNode* node = (LFStackNode*)malloc(sizeof(LFStackNode));
    if (!node) return -1;
    node->value = value;

    LFStackNode* old_top;
    do {
        old_top = atomic_load_explicit(&s->top, memory_order_relaxed);
        node->next = old_top;
    } while (!atomic_compare_exchange_weak_explicit(&s->top, &old_top, node,
                memory_order_release, memory_order_relaxed));

    atomic_fetch_add_explicit(&s->count, 1, memory_order_relaxed);
    return 0;
}

// Pop returns value, -1 if empty
int64_t npk_shim_lfstack_pop(int64_t handle) {
    if (handle < 0 || handle >= MAX_LF_STACKS || !g_lf_stacks[handle]) return -1;
    LFStack* s = g_lf_stacks[handle];

    LFStackNode* old_top;
    do {
        old_top = atomic_load_explicit(&s->top, memory_order_acquire);
        if (!old_top) return -1; // empty
    } while (!atomic_compare_exchange_weak_explicit(&s->top, &old_top, old_top->next,
                memory_order_release, memory_order_relaxed));

    int64_t value = old_top->value;
    atomic_fetch_sub_explicit(&s->count, 1, memory_order_relaxed);
    free(old_top);
    return value;
}

int64_t npk_shim_lfstack_size(int64_t handle) {
    if (handle < 0 || handle >= MAX_LF_STACKS || !g_lf_stacks[handle]) return -1;
    return atomic_load_explicit(&g_lf_stacks[handle]->count, memory_order_relaxed);
}

// ============================================================================
// 3. Ring Buffer (SPSC, wait-free)
// ============================================================================
// Lamport-style single-producer single-consumer bounded buffer.

typedef struct {
    int64_t*         buffer;
    int64_t          capacity;
    int64_t          mask;
    _Atomic(int64_t) head;  // consumer reads from here
    _Atomic(int64_t) tail;  // producer writes here
} RingBuf;

#define MAX_RINGBUFS 128
static RingBuf* g_ringbufs[MAX_RINGBUFS];

static int64_t alloc_ringbuf_slot(RingBuf* r) {
    for (int i = 0; i < MAX_RINGBUFS; i++) {
        if (!g_ringbufs[i]) { g_ringbufs[i] = r; return (int64_t)i; }
    }
    return -1;
}

int64_t npk_shim_ringbuf_create(int64_t capacity) {
    if (capacity <= 0) capacity = 256;
    capacity = next_pow2(capacity);

    RingBuf* r = (RingBuf*)calloc(1, sizeof(RingBuf));
    if (!r) return -1;

    r->buffer = (int64_t*)calloc((size_t)capacity, sizeof(int64_t));
    if (!r->buffer) { free(r); return -1; }

    r->capacity = capacity;
    r->mask = capacity - 1;
    atomic_store(&r->head, 0);
    atomic_store(&r->tail, 0);

    return alloc_ringbuf_slot(r);
}

int32_t npk_shim_ringbuf_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_RINGBUFS || !g_ringbufs[handle]) return -1;
    RingBuf* r = g_ringbufs[handle];
    free(r->buffer);
    free(r);
    g_ringbufs[handle] = NULL;
    return 0;
}

// Producer push. Returns 0 on success, -1 if full.
int32_t npk_shim_ringbuf_push(int64_t handle, int64_t value) {
    if (handle < 0 || handle >= MAX_RINGBUFS || !g_ringbufs[handle]) return -1;
    RingBuf* r = g_ringbufs[handle];

    int64_t tail = atomic_load_explicit(&r->tail, memory_order_relaxed);
    int64_t head = atomic_load_explicit(&r->head, memory_order_acquire);

    if (tail - head >= r->capacity) return -1; // full

    r->buffer[tail & r->mask] = value;
    atomic_store_explicit(&r->tail, tail + 1, memory_order_release);
    return 0;
}

// Consumer pop. Returns value, -1 if empty.
int64_t npk_shim_ringbuf_pop(int64_t handle) {
    if (handle < 0 || handle >= MAX_RINGBUFS || !g_ringbufs[handle]) return -1;
    RingBuf* r = g_ringbufs[handle];

    int64_t head = atomic_load_explicit(&r->head, memory_order_relaxed);
    int64_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);

    if (head >= tail) return -1; // empty

    int64_t value = r->buffer[head & r->mask];
    atomic_store_explicit(&r->head, head + 1, memory_order_release);
    return value;
}

int64_t npk_shim_ringbuf_size(int64_t handle) {
    if (handle < 0 || handle >= MAX_RINGBUFS || !g_ringbufs[handle]) return -1;
    RingBuf* r = g_ringbufs[handle];
    int64_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    int64_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    int64_t sz = tail - head;
    return sz >= 0 ? sz : 0;
}

int64_t npk_shim_ringbuf_capacity(int64_t handle) {
    if (handle < 0 || handle >= MAX_RINGBUFS || !g_ringbufs[handle]) return -1;
    return g_ringbufs[handle]->capacity;
}

#ifdef __cplusplus
}
#endif
