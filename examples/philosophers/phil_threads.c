/*
 * phil_threads.c — POSIX-thread wrappers for Aria Dining Philosophers (EXP-6)
 *
 * Workaround for two Aria compiler bugs found during EXP-6:
 *   Bug #11 – `_ = non_void_extern_call(...)` silently removes the entire
 *              call from the generated LLVM IR.
 *   Bug #12 – `(T)(T):fn = named_func` typed fn-ptr variable assignment
 *              never stores the function address in the generated IR.
 *
 * Thread logic is implemented here in C using the Aria runtime atomic API
 * that philosophers.aria allocates and stores as int64 globals.
 * philosophers.aria main() initialises shared state, then calls
 *   aria_spawn_philosophers()  — void, not subject to Bug #11
 *   aria_join_philosophers()   — void, not subject to Bug #11
 */

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

/* Declare runtime functions with C linkage (compiled with clang++ in C++ mode) */
#ifdef __cplusplus
extern "C" {
#endif

/* ── Run-time types (opaque, matching runtime/atomic.h) ─────────────────── */
typedef struct AriaAtomicInt32 AriaAtomicInt32;
typedef int AriaMemoryOrder;

#define ARIA_MEMORY_ORDER_RELAXED  0
#define ARIA_MEMORY_ORDER_ACQUIRE  1
#define ARIA_MEMORY_ORDER_RELEASE  2
#define ARIA_MEMORY_ORDER_ACQ_REL  3
#define ARIA_MEMORY_ORDER_SEQ_CST  4

/* ── Runtime function declarations ──────────────────────────────────────── */
void*          aria_int64_to_wild(int64_t addr);
int32_t        aria_atomic_int32_load(AriaAtomicInt32* a, AriaMemoryOrder o);
void           aria_atomic_int32_store(AriaAtomicInt32* a, int32_t v,
                                       AriaMemoryOrder o);
bool           aria_atomic_int32_compare_exchange_strong(AriaAtomicInt32* a,
                   int32_t* expected, int32_t desired,
                   AriaMemoryOrder succ, AriaMemoryOrder fail);
int32_t        aria_atomic_int32_fetch_add(AriaAtomicInt32* a, int32_t delta,
                                           AriaMemoryOrder o);
void           aria_thread_sleep_ns(uint64_t nanoseconds);

/* ── Shared state — globals defined in philosophers.aria compiled output ─── */
extern int64_t g_fork_0, g_fork_1, g_fork_2, g_fork_3, g_fork_4;
extern int64_t g_eat_0,  g_eat_1,  g_eat_2,  g_eat_3,  g_eat_4;
extern int64_t g_running;
extern int64_t g_tid_0,  g_tid_1,  g_tid_2,  g_tid_3,  g_tid_4;

#ifdef __cplusplus
} /* extern "C" */
#endif

static int64_t * const g_forks_arr[5] = {
    &g_fork_0, &g_fork_1, &g_fork_2, &g_fork_3, &g_fork_4
};
static int64_t * const g_eats_arr[5] = {
    &g_eat_0, &g_eat_1, &g_eat_2, &g_eat_3, &g_eat_4
};

#define N_PHIL    5
#define THINK_NS  5000000ULL   /*  5 ms */
#define EAT_NS   10000000ULL   /* 10 ms */
#define RETRY_NS   500000ULL   /* 0.5 ms */

/* ── Philosopher body ─────────────────────────────────────────────────────── */
static void* philosopher_thread(void* arg)
{
    int32_t id     = (int32_t)(intptr_t)arg;
    int32_t left   = id;
    int32_t right  = (id + 1) % N_PHIL;
    /* Resource ordering: always acquire the lower-numbered fork first */
    int32_t first  = left, second = right;
    if (right < left) { int32_t tmp = right; first = tmp; second = left; }

    for (;;) {
        /* ── check stop flag ──────────────────────────────────────────── */
        AriaAtomicInt32* r =
            (AriaAtomicInt32*)aria_int64_to_wild(g_running);
        if (aria_atomic_int32_load(r, ARIA_MEMORY_ORDER_RELAXED) != 1)
            break;

        aria_thread_sleep_ns(THINK_NS);

        /* ── acquire first fork ───────────────────────────────────────── */
        for (;;) {
            AriaAtomicInt32* f =
                (AriaAtomicInt32*)aria_int64_to_wild(*g_forks_arr[first]);
            int32_t exp = 0;
            bool got = aria_atomic_int32_compare_exchange_strong(
                f, &exp, 1,
                ARIA_MEMORY_ORDER_ACQ_REL, ARIA_MEMORY_ORDER_ACQUIRE);
            if (got) break;
            aria_thread_sleep_ns(RETRY_NS);
        }

        /* ── acquire second fork ──────────────────────────────────────── */
        for (;;) {
            AriaAtomicInt32* f =
                (AriaAtomicInt32*)aria_int64_to_wild(*g_forks_arr[second]);
            int32_t exp = 0;
            bool got = aria_atomic_int32_compare_exchange_strong(
                f, &exp, 1,
                ARIA_MEMORY_ORDER_ACQ_REL, ARIA_MEMORY_ORDER_ACQUIRE);
            if (got) break;
            aria_thread_sleep_ns(RETRY_NS);
        }

        /* ── eat ──────────────────────────────────────────────────────── */
        aria_thread_sleep_ns(EAT_NS);

        AriaAtomicInt32* e =
            (AriaAtomicInt32*)aria_int64_to_wild(*g_eats_arr[id]);
        aria_atomic_int32_fetch_add(e, 1, ARIA_MEMORY_ORDER_RELAXED);

        /* ── release forks ────────────────────────────────────────────── */
        AriaAtomicInt32* ff =
            (AriaAtomicInt32*)aria_int64_to_wild(*g_forks_arr[first]);
        aria_atomic_int32_store(ff, 0, ARIA_MEMORY_ORDER_RELEASE);
        AriaAtomicInt32* sf =
            (AriaAtomicInt32*)aria_int64_to_wild(*g_forks_arr[second]);
        aria_atomic_int32_store(sf, 0, ARIA_MEMORY_ORDER_RELEASE);
    }

    return NULL;
}

/* ── Public API (declared as `extern func:X = void()` in philosophers.aria) ─ */

extern "C" void aria_spawn_philosophers(void)
{
    pthread_t t0, t1, t2, t3, t4;
    pthread_create(&t0, NULL, philosopher_thread, (void*)(intptr_t)0);
    pthread_create(&t1, NULL, philosopher_thread, (void*)(intptr_t)1);
    pthread_create(&t2, NULL, philosopher_thread, (void*)(intptr_t)2);
    pthread_create(&t3, NULL, philosopher_thread, (void*)(intptr_t)3);
    pthread_create(&t4, NULL, philosopher_thread, (void*)(intptr_t)4);
    /* Store opaque thread handles as int64 so join helper can read them */
    g_tid_0 = (int64_t)(uintptr_t)t0;
    g_tid_1 = (int64_t)(uintptr_t)t1;
    g_tid_2 = (int64_t)(uintptr_t)t2;
    g_tid_3 = (int64_t)(uintptr_t)t3;
    g_tid_4 = (int64_t)(uintptr_t)t4;
}

extern "C" void aria_join_philosophers(void)
{
    pthread_join((pthread_t)(uintptr_t)g_tid_0, NULL);
    pthread_join((pthread_t)(uintptr_t)g_tid_1, NULL);
    pthread_join((pthread_t)(uintptr_t)g_tid_2, NULL);
    pthread_join((pthread_t)(uintptr_t)g_tid_3, NULL);
    pthread_join((pthread_t)(uintptr_t)g_tid_4, NULL);
}

