/**
 * Aria Threading Shim — Handle-Based API for Aria Stdlib
 *
 * Wraps the existing AriaResult*-based thread/sync runtime with simple
 * int64 handles and int32 error codes that Aria extern blocks can consume
 * directly. Uses static handle pools (same pattern as aria-packages).
 *
 * Pool sizes are generous for general-purpose applications.
 * All functions prefixed with aria_shim_* to distinguish from the
 * underlying runtime API (aria_thread_*, aria_mutex_*, etc.).
 */

#include "runtime/thread.h"
#include "runtime/io.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Handle Pools
// ============================================================================

#define MAX_THREADS   256
#define MAX_MUTEXES   256
#define MAX_CONDVARS  128
#define MAX_RWLOCKS   128
#define MAX_BARRIERS   64
#define MAX_TLS_KEYS   64

static AriaThread*   g_threads[MAX_THREADS];
static AriaMutex*    g_mutexes[MAX_MUTEXES];
static AriaCondVar*  g_condvars[MAX_CONDVARS];
static AriaRWLock*   g_rwlocks[MAX_RWLOCKS];
static AriaBarrier*  g_barriers[MAX_BARRIERS];
static AriaThreadLocal* g_tls_keys[MAX_TLS_KEYS];

// Simple allocators — find first empty slot
static int64_t alloc_thread_slot(AriaThread* t) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (!g_threads[i]) { g_threads[i] = t; return (int64_t)i; }
    }
    return -1;
}

static int64_t alloc_mutex_slot(AriaMutex* m) {
    for (int i = 0; i < MAX_MUTEXES; i++) {
        if (!g_mutexes[i]) { g_mutexes[i] = m; return (int64_t)i; }
    }
    return -1;
}

static int64_t alloc_condvar_slot(AriaCondVar* c) {
    for (int i = 0; i < MAX_CONDVARS; i++) {
        if (!g_condvars[i]) { g_condvars[i] = c; return (int64_t)i; }
    }
    return -1;
}

static int64_t alloc_rwlock_slot(AriaRWLock* r) {
    for (int i = 0; i < MAX_RWLOCKS; i++) {
        if (!g_rwlocks[i]) { g_rwlocks[i] = r; return (int64_t)i; }
    }
    return -1;
}

static int64_t alloc_barrier_slot(AriaBarrier* b) {
    for (int i = 0; i < MAX_BARRIERS; i++) {
        if (!g_barriers[i]) { g_barriers[i] = b; return (int64_t)i; }
    }
    return -1;
}

static int64_t alloc_tls_slot(AriaThreadLocal* k) {
    for (int i = 0; i < MAX_TLS_KEYS; i++) {
        if (!g_tls_keys[i]) { g_tls_keys[i] = k; return (int64_t)i; }
    }
    return -1;
}

// ============================================================================
// Thread Callback Trampoline
// ============================================================================

// Aria lambda functions are compiled with a hidden env_ptr first parameter:
//   void func(void* env, int64_t arg)
// For non-closure functions, env is always NULL.
// The codegen extracts method_ptr from the fat pointer {method_ptr, env_ptr}
// and passes it as void* to this shim.
typedef void (*AriaLambdaFunc)(void* env, int64_t arg);

typedef struct {
    AriaLambdaFunc func;
    int64_t        arg;
} ThreadTrampData;

static void* thread_trampoline(void* raw) {
    ThreadTrampData* data = (ThreadTrampData*)raw;
    AriaLambdaFunc func = data->func;
    int64_t arg = data->arg;
    free(data);
    func(NULL, arg);  // NULL env for non-closure functions
    return NULL;
}

// ============================================================================
// Thread Lifecycle — aria_shim_thread_*
// ============================================================================

/**
 * Spawn a new thread running func(NULL, arg).
 * func is a method_ptr extracted from an Aria fat pointer,
 * with calling convention: void func(void* env, int64_t arg).
 * Returns thread handle (>= 0) on success, -1 on error.
 */
int64_t aria_shim_thread_spawn(void* func, int64_t arg) {
    if (!func) return -1;
    ThreadTrampData* data = (ThreadTrampData*)malloc(sizeof(ThreadTrampData));
    if (!data) return -1;
    data->func = (AriaLambdaFunc)func;
    data->arg = arg;

    AriaResult* res = aria_thread_create(thread_trampoline, data);
    if (!res || res->err) {
        free(data);
        if (res) aria_result_free(res);
        return -1;
    }
    AriaThread* t = (AriaThread*)res->val;
    res->val = NULL; // prevent result_free from freeing the thread
    aria_result_free(res);
    int64_t handle = alloc_thread_slot(t);
    if (handle < 0) {
        aria_thread_detach(t);
        return -1;
    }
    return handle;
}

/**
 * Join (wait for) a thread. Returns 0 on success, -1 on error.
 */
int32_t aria_shim_thread_join(int64_t handle) {
    if (handle < 0 || handle >= MAX_THREADS || !g_threads[handle]) return -1;
    AriaThread* t = g_threads[handle];
    AriaResult* res = aria_thread_join(t, NULL);
    g_threads[handle] = NULL;
    if (!res || res->err) {
        if (res) aria_result_free(res);
        return -1;
    }
    aria_result_free(res);
    return 0;
}

/**
 * Detach a thread (auto-cleanup on exit). Returns 0 on success, -1 on error.
 */
int32_t aria_shim_thread_detach(int64_t handle) {
    if (handle < 0 || handle >= MAX_THREADS || !g_threads[handle]) return -1;
    AriaThread* t = g_threads[handle];
    AriaResult* res = aria_thread_detach(t);
    g_threads[handle] = NULL;
    if (!res || res->err) {
        if (res) aria_result_free(res);
        return -1;
    }
    aria_result_free(res);
    return 0;
}

/** Yield CPU to other threads. */
void aria_shim_thread_yield(void) {
    aria_thread_yield();
}

/** Sleep for given nanoseconds. */
void aria_shim_thread_sleep_ns(int64_t ns) {
    aria_thread_sleep_ns((uint64_t)ns);
}

/** Sleep for given milliseconds (convenience). */
void aria_shim_thread_sleep_ms(int64_t ms) {
    aria_thread_sleep_ns((uint64_t)ms * 1000000ULL);
}

/** Set current thread's name. */
void aria_shim_thread_set_name(const char* name) {
    aria_thread_set_name(name);
}

/** Get hardware concurrency (number of CPUs). */
int32_t aria_shim_thread_hardware_concurrency(void) {
    return (int32_t)aria_thread_hardware_concurrency();
}

/** Get current thread ID as int64. */
int64_t aria_shim_thread_current_id(void) {
    AriaThreadId tid = aria_thread_current_id();
    return (int64_t)tid.id;
}

// ============================================================================
// Mutex — aria_shim_mutex_*
// ============================================================================

/** Create a mutex. type: 0=normal, 1=recursive. Returns handle or -1. */
int64_t aria_shim_mutex_create(int32_t type) {
    AriaResult* res = aria_mutex_create((AriaMutexType)type);
    if (!res || res->err) {
        if (res) aria_result_free(res);
        return -1;
    }
    AriaMutex* m = (AriaMutex*)res->val;
    res->val = NULL;
    aria_result_free(res);
    return alloc_mutex_slot(m);
}

/** Lock a mutex. Returns 0 on success, -1 on error. */
int32_t aria_shim_mutex_lock(int64_t handle) {
    if (handle < 0 || handle >= MAX_MUTEXES || !g_mutexes[handle]) return -1;
    AriaResult* res = aria_mutex_lock(g_mutexes[handle]);
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

/** Try to lock a mutex. Returns 1 if locked, 0 if busy, -1 on error. */
int32_t aria_shim_mutex_trylock(int64_t handle) {
    if (handle < 0 || handle >= MAX_MUTEXES || !g_mutexes[handle]) return -1;
    AriaResult* res = aria_mutex_trylock(g_mutexes[handle]);
    if (!res || res->err) {
        if (res) aria_result_free(res);
        return -1;
    }
    // result.data indicates success: non-null means locked
    int32_t got_lock = res->val ? 1 : 0;
    aria_result_free(res);
    return got_lock;
}

/** Unlock a mutex. Returns 0 on success, -1 on error. */
int32_t aria_shim_mutex_unlock(int64_t handle) {
    if (handle < 0 || handle >= MAX_MUTEXES || !g_mutexes[handle]) return -1;
    AriaResult* res = aria_mutex_unlock(g_mutexes[handle]);
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

/** Destroy a mutex. Returns 0 on success, -1 on error. */
int32_t aria_shim_mutex_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_MUTEXES || !g_mutexes[handle]) return -1;
    AriaResult* res = aria_mutex_destroy(g_mutexes[handle]);
    g_mutexes[handle] = NULL;
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

// ============================================================================
// Condition Variables — aria_shim_condvar_*
// ============================================================================

/** Create a condition variable. Returns handle or -1. */
int64_t aria_shim_condvar_create(void) {
    AriaResult* res = aria_condvar_create();
    if (!res || res->err) {
        if (res) aria_result_free(res);
        return -1;
    }
    AriaCondVar* cv = (AriaCondVar*)res->val;
    res->val = NULL;
    aria_result_free(res);
    return alloc_condvar_slot(cv);
}

/** Wait on condition variable (must hold mutex_handle). Returns 0 on success. */
int32_t aria_shim_condvar_wait(int64_t cv_handle, int64_t mutex_handle) {
    if (cv_handle < 0 || cv_handle >= MAX_CONDVARS || !g_condvars[cv_handle]) return -1;
    if (mutex_handle < 0 || mutex_handle >= MAX_MUTEXES || !g_mutexes[mutex_handle]) return -1;
    AriaResult* res = aria_condvar_wait(g_condvars[cv_handle], g_mutexes[mutex_handle]);
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

/** Timed wait. Returns 1 if signaled, 0 if timed out, -1 on error. */
int32_t aria_shim_condvar_timedwait(int64_t cv_handle, int64_t mutex_handle, int64_t timeout_ns) {
    if (cv_handle < 0 || cv_handle >= MAX_CONDVARS || !g_condvars[cv_handle]) return -1;
    if (mutex_handle < 0 || mutex_handle >= MAX_MUTEXES || !g_mutexes[mutex_handle]) return -1;
    AriaResult* res = aria_condvar_timedwait(g_condvars[cv_handle], g_mutexes[mutex_handle], (uint64_t)timeout_ns);
    if (!res || res->err) {
        if (res) aria_result_free(res);
        return -1;
    }
    int32_t signaled = res->val ? 1 : 0;
    aria_result_free(res);
    return signaled;
}

/** Signal one waiter. Returns 0 on success. */
int32_t aria_shim_condvar_signal(int64_t handle) {
    if (handle < 0 || handle >= MAX_CONDVARS || !g_condvars[handle]) return -1;
    AriaResult* res = aria_condvar_signal(g_condvars[handle]);
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

/** Broadcast to all waiters. Returns 0 on success. */
int32_t aria_shim_condvar_broadcast(int64_t handle) {
    if (handle < 0 || handle >= MAX_CONDVARS || !g_condvars[handle]) return -1;
    AriaResult* res = aria_condvar_broadcast(g_condvars[handle]);
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

/** Destroy a condition variable. */
int32_t aria_shim_condvar_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_CONDVARS || !g_condvars[handle]) return -1;
    AriaResult* res = aria_condvar_destroy(g_condvars[handle]);
    g_condvars[handle] = NULL;
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

// ============================================================================
// Read-Write Locks — aria_shim_rwlock_*
// ============================================================================

/** Create a read-write lock. Returns handle or -1. */
int64_t aria_shim_rwlock_create(void) {
    AriaResult* res = aria_rwlock_create();
    if (!res || res->err) {
        if (res) aria_result_free(res);
        return -1;
    }
    AriaRWLock* rw = (AriaRWLock*)res->val;
    res->val = NULL;
    aria_result_free(res);
    return alloc_rwlock_slot(rw);
}

int32_t aria_shim_rwlock_rdlock(int64_t handle) {
    if (handle < 0 || handle >= MAX_RWLOCKS || !g_rwlocks[handle]) return -1;
    AriaResult* res = aria_rwlock_rdlock(g_rwlocks[handle]);
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

int32_t aria_shim_rwlock_tryrdlock(int64_t handle) {
    if (handle < 0 || handle >= MAX_RWLOCKS || !g_rwlocks[handle]) return -1;
    AriaResult* res = aria_rwlock_tryrdlock(g_rwlocks[handle]);
    if (!res || res->err) {
        if (res) aria_result_free(res);
        return -1;
    }
    int32_t got = res->val ? 1 : 0;
    aria_result_free(res);
    return got;
}

int32_t aria_shim_rwlock_wrlock(int64_t handle) {
    if (handle < 0 || handle >= MAX_RWLOCKS || !g_rwlocks[handle]) return -1;
    AriaResult* res = aria_rwlock_wrlock(g_rwlocks[handle]);
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

int32_t aria_shim_rwlock_trywrlock(int64_t handle) {
    if (handle < 0 || handle >= MAX_RWLOCKS || !g_rwlocks[handle]) return -1;
    AriaResult* res = aria_rwlock_trywrlock(g_rwlocks[handle]);
    if (!res || res->err) {
        if (res) aria_result_free(res);
        return -1;
    }
    int32_t got = res->val ? 1 : 0;
    aria_result_free(res);
    return got;
}

int32_t aria_shim_rwlock_unlock(int64_t handle) {
    if (handle < 0 || handle >= MAX_RWLOCKS || !g_rwlocks[handle]) return -1;
    AriaResult* res = aria_rwlock_unlock(g_rwlocks[handle]);
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

int32_t aria_shim_rwlock_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_RWLOCKS || !g_rwlocks[handle]) return -1;
    AriaResult* res = aria_rwlock_destroy(g_rwlocks[handle]);
    g_rwlocks[handle] = NULL;
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

// ============================================================================
// Barriers — aria_shim_barrier_*
// ============================================================================

/** Create barrier for count threads. Returns handle or -1. */
int64_t aria_shim_barrier_create(int32_t count) {
    AriaResult* res = aria_barrier_create((uint32_t)count);
    if (!res || res->err) {
        if (res) aria_result_free(res);
        return -1;
    }
    AriaBarrier* b = (AriaBarrier*)res->val;
    res->val = NULL;
    aria_result_free(res);
    return alloc_barrier_slot(b);
}

/** Wait at barrier. Blocks until all threads arrive. Returns 0. */
int32_t aria_shim_barrier_wait(int64_t handle) {
    if (handle < 0 || handle >= MAX_BARRIERS || !g_barriers[handle]) return -1;
    AriaResult* res = aria_barrier_wait(g_barriers[handle]);
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

int32_t aria_shim_barrier_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_BARRIERS || !g_barriers[handle]) return -1;
    AriaResult* res = aria_barrier_destroy(g_barriers[handle]);
    g_barriers[handle] = NULL;
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

// ============================================================================
// Thread-Local Storage — aria_shim_tls_*
// ============================================================================

/** Create TLS key. Returns handle or -1. */
int64_t aria_shim_tls_create(void) {
    AriaResult* res = aria_thread_local_create(NULL); // no destructor
    if (!res || res->err) {
        if (res) aria_result_free(res);
        return -1;
    }
    AriaThreadLocal* k = (AriaThreadLocal*)res->val;
    res->val = NULL;
    aria_result_free(res);
    return alloc_tls_slot(k);
}

/** Set TLS value (as int64). Returns 0 on success. */
int32_t aria_shim_tls_set(int64_t handle, int64_t value) {
    if (handle < 0 || handle >= MAX_TLS_KEYS || !g_tls_keys[handle]) return -1;
    AriaResult* res = aria_thread_local_set(g_tls_keys[handle], (void*)(uintptr_t)value);
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

/** Get TLS value (as int64). Returns stored value or 0. */
int64_t aria_shim_tls_get(int64_t handle) {
    if (handle < 0 || handle >= MAX_TLS_KEYS || !g_tls_keys[handle]) return 0;
    void* val = aria_thread_local_get(g_tls_keys[handle]);
    return (int64_t)(uintptr_t)val;
}

int32_t aria_shim_tls_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_TLS_KEYS || !g_tls_keys[handle]) return -1;
    AriaResult* res = aria_thread_local_destroy(g_tls_keys[handle]);
    g_tls_keys[handle] = NULL;
    int32_t ret = (!res || res->err) ? -1 : 0;
    if (res) aria_result_free(res);
    return ret;
}

// ============================================================================
// Channel — Lock-based MPMC channel for int64 values
//   Supports 3 modes: BUFFERED, UNBUFFERED (rendezvous), ONESHOT
// ============================================================================

#include <pthread.h>

#define MAX_CHANNELS     64
#define CHANNEL_BUF_SIZE 1024

// Channel modes
#define CHANNEL_MODE_BUFFERED   0
#define CHANNEL_MODE_UNBUFFERED 1  // rendezvous — sender blocks until receiver takes
#define CHANNEL_MODE_ONESHOT    2  // single-use — auto-closes after first send

typedef struct {
    int64_t         buf[CHANNEL_BUF_SIZE];
    int32_t         capacity;
    int32_t         count;
    int32_t         head;
    int32_t         tail;
    int32_t         mode;
    pthread_mutex_t mtx;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
    pthread_cond_t  rendezvous;  // for unbuffered: sender waits until receiver takes
    bool            closed;
    bool            has_rendezvous_value;
    int64_t         rendezvous_value;
} AriaChannel;

static AriaChannel* g_channels[MAX_CHANNELS];

static int64_t alloc_channel_slot(AriaChannel* ch) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (!g_channels[i]) { g_channels[i] = ch; return (int64_t)i; }
    }
    return -1;
}

/** Create a bounded channel with given capacity (max CHANNEL_BUF_SIZE). Returns handle or -1. */
int64_t aria_shim_channel_create(int32_t capacity) {
    if (capacity <= 0 || capacity > CHANNEL_BUF_SIZE) capacity = CHANNEL_BUF_SIZE;
    AriaChannel* ch = (AriaChannel*)calloc(1, sizeof(AriaChannel));
    if (!ch) return -1;
    ch->capacity = capacity;
    ch->mode = CHANNEL_MODE_BUFFERED;
    pthread_mutex_init(&ch->mtx, NULL);
    pthread_cond_init(&ch->not_empty, NULL);
    pthread_cond_init(&ch->not_full, NULL);
    pthread_cond_init(&ch->rendezvous, NULL);
    return alloc_channel_slot(ch);
}

/** Create an unbuffered (rendezvous) channel. Sender blocks until receiver takes. */
int64_t aria_shim_channel_create_unbuffered(void) {
    AriaChannel* ch = (AriaChannel*)calloc(1, sizeof(AriaChannel));
    if (!ch) return -1;
    ch->capacity = 0;
    ch->mode = CHANNEL_MODE_UNBUFFERED;
    ch->has_rendezvous_value = false;
    pthread_mutex_init(&ch->mtx, NULL);
    pthread_cond_init(&ch->not_empty, NULL);
    pthread_cond_init(&ch->not_full, NULL);
    pthread_cond_init(&ch->rendezvous, NULL);
    return alloc_channel_slot(ch);
}

/** Create a oneshot channel. Auto-closes after one value is sent. */
int64_t aria_shim_channel_create_oneshot(void) {
    AriaChannel* ch = (AriaChannel*)calloc(1, sizeof(AriaChannel));
    if (!ch) return -1;
    ch->capacity = 1;
    ch->mode = CHANNEL_MODE_ONESHOT;
    pthread_mutex_init(&ch->mtx, NULL);
    pthread_cond_init(&ch->not_empty, NULL);
    pthread_cond_init(&ch->not_full, NULL);
    pthread_cond_init(&ch->rendezvous, NULL);
    return alloc_channel_slot(ch);
}

/** Get channel mode. Returns 0=buffered, 1=unbuffered, 2=oneshot, -1=error. */
int32_t aria_shim_channel_get_mode(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return -1;
    return g_channels[handle]->mode;
}

/** Get channel capacity. Returns capacity or -1 on error. */
int32_t aria_shim_channel_capacity(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return -1;
    return g_channels[handle]->capacity;
}

/** Send value to channel. Blocks if full. Returns 0 on success, -1 if closed/error. */
int32_t aria_shim_channel_send(int64_t handle, int64_t value) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return -1;
    AriaChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);

    if (ch->closed) {
        pthread_mutex_unlock(&ch->mtx);
        return -1;
    }

    if (ch->mode == CHANNEL_MODE_UNBUFFERED) {
        // Rendezvous: deposit value and wait for receiver to take it
        while (ch->has_rendezvous_value && !ch->closed) {
            pthread_cond_wait(&ch->rendezvous, &ch->mtx);
        }
        if (ch->closed) {
            pthread_mutex_unlock(&ch->mtx);
            return -1;
        }
        ch->rendezvous_value = value;
        ch->has_rendezvous_value = true;
        pthread_cond_signal(&ch->not_empty);  // wake receiver
        // Wait until receiver takes the value
        while (ch->has_rendezvous_value && !ch->closed) {
            pthread_cond_wait(&ch->rendezvous, &ch->mtx);
        }
        pthread_mutex_unlock(&ch->mtx);
        return ch->closed && ch->has_rendezvous_value ? -1 : 0;
    }

    // Buffered / oneshot: standard ring buffer
    while (ch->count >= ch->capacity && !ch->closed) {
        pthread_cond_wait(&ch->not_full, &ch->mtx);
    }
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mtx);
        return -1;
    }
    ch->buf[ch->tail] = value;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->not_empty);

    // Oneshot: auto-close after first send
    if (ch->mode == CHANNEL_MODE_ONESHOT) {
        ch->closed = true;
        pthread_cond_broadcast(&ch->not_empty);
        pthread_cond_broadcast(&ch->not_full);
    }

    pthread_mutex_unlock(&ch->mtx);
    return 0;
}

/** Try to send without blocking. Returns 0 on success, -1 if full or closed. */
int32_t aria_shim_channel_try_send(int64_t handle, int64_t value) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return -1;
    AriaChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);

    if (ch->closed) {
        pthread_mutex_unlock(&ch->mtx);
        return -1;
    }

    if (ch->mode == CHANNEL_MODE_UNBUFFERED) {
        // Non-blocking rendezvous: only succeeds if a receiver is waiting
        if (ch->has_rendezvous_value) {
            pthread_mutex_unlock(&ch->mtx);
            return -1;  // previous value not yet taken
        }
        ch->rendezvous_value = value;
        ch->has_rendezvous_value = true;
        pthread_cond_signal(&ch->not_empty);
        // Don't wait — return immediately (receiver will signal rendezvous)
        pthread_mutex_unlock(&ch->mtx);
        return 0;
    }

    if (ch->count >= ch->capacity) {
        pthread_mutex_unlock(&ch->mtx);
        return -1;
    }
    ch->buf[ch->tail] = value;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->not_empty);

    if (ch->mode == CHANNEL_MODE_ONESHOT) {
        ch->closed = true;
        pthread_cond_broadcast(&ch->not_empty);
        pthread_cond_broadcast(&ch->not_full);
    }

    pthread_mutex_unlock(&ch->mtx);
    return 0;
}

/**
 * Receive value from channel. Blocks if empty.
 * Returns the value. Returns 0 if channel is closed and empty.
 */
int64_t aria_shim_channel_recv(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return 0;
    AriaChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);

    if (ch->mode == CHANNEL_MODE_UNBUFFERED) {
        // Rendezvous: wait for sender to deposit value
        while (!ch->has_rendezvous_value && !ch->closed) {
            pthread_cond_wait(&ch->not_empty, &ch->mtx);
        }
        if (!ch->has_rendezvous_value) {
            // closed and no value
            pthread_mutex_unlock(&ch->mtx);
            return 0;
        }
        int64_t value = ch->rendezvous_value;
        ch->has_rendezvous_value = false;
        pthread_cond_signal(&ch->rendezvous);  // wake sender
        pthread_mutex_unlock(&ch->mtx);
        return value;
    }

    // Buffered / oneshot
    while (ch->count == 0 && !ch->closed) {
        pthread_cond_wait(&ch->not_empty, &ch->mtx);
    }
    if (ch->count == 0) {
        // closed and empty
        pthread_mutex_unlock(&ch->mtx);
        return 0;
    }
    int64_t value = ch->buf[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->not_full);
    pthread_mutex_unlock(&ch->mtx);
    return value;
}

/** Try receive without blocking. Returns value, or 0 if empty/closed. */
int64_t aria_shim_channel_try_recv(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return 0;
    AriaChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);

    if (ch->mode == CHANNEL_MODE_UNBUFFERED) {
        if (!ch->has_rendezvous_value) {
            pthread_mutex_unlock(&ch->mtx);
            return 0;
        }
        int64_t value = ch->rendezvous_value;
        ch->has_rendezvous_value = false;
        pthread_cond_signal(&ch->rendezvous);
        pthread_mutex_unlock(&ch->mtx);
        return value;
    }

    if (ch->count == 0) {
        pthread_mutex_unlock(&ch->mtx);
        return 0;
    }
    int64_t value = ch->buf[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->not_full);
    pthread_mutex_unlock(&ch->mtx);
    return value;
}

/** Check if channel has data. Returns count of pending items. */
int32_t aria_shim_channel_count(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return 0;
    AriaChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);
    int32_t c = ch->count;
    if (ch->mode == CHANNEL_MODE_UNBUFFERED && ch->has_rendezvous_value) c = 1;
    pthread_mutex_unlock(&ch->mtx);
    return c;
}

/** Close a channel (no more sends; existing data can still be received). */
int32_t aria_shim_channel_close(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return -1;
    AriaChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);
    ch->closed = true;
    pthread_cond_broadcast(&ch->not_empty);
    pthread_cond_broadcast(&ch->not_full);
    pthread_cond_broadcast(&ch->rendezvous);
    pthread_mutex_unlock(&ch->mtx);
    return 0;
}

/** Destroy a channel (close + free). */
int32_t aria_shim_channel_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return -1;
    AriaChannel* ch = g_channels[handle];
    aria_shim_channel_close(handle);
    pthread_mutex_destroy(&ch->mtx);
    pthread_cond_destroy(&ch->not_empty);
    pthread_cond_destroy(&ch->not_full);
    pthread_cond_destroy(&ch->rendezvous);
    free(ch);
    g_channels[handle] = NULL;
    return 0;
}

/** Check if channel is closed. */
bool aria_shim_channel_is_closed(int64_t handle) {
    if (handle < 0 || handle >= MAX_CHANNELS || !g_channels[handle]) return true;
    AriaChannel* ch = g_channels[handle];
    pthread_mutex_lock(&ch->mtx);
    bool c = ch->closed;
    pthread_mutex_unlock(&ch->mtx);
    return c;
}

// ============================================================================
// Channel Select — Wait on multiple channels for first ready
// ============================================================================

/**
 * Select: poll up to `n` channels, return index of first channel with data.
 * channels[] is an array of channel handles (int64) stored at ptr.
 * Spins with condvar wait. Returns index (0..n-1) of ready channel, or -1 on timeout/error.
 * timeout_ms: 0 = non-blocking poll, -1 = block forever, >0 = block up to N ms.
 */
int32_t aria_shim_channel_select(int64_t* channels, int32_t n, int64_t timeout_ms) {
    if (!channels || n <= 0 || n > MAX_CHANNELS) return -1;

    // Quick non-blocking scan
    for (int32_t i = 0; i < n; i++) {
        int64_t h = channels[i];
        if (h < 0 || h >= MAX_CHANNELS || !g_channels[h]) continue;
        AriaChannel* ch = g_channels[h];
        pthread_mutex_lock(&ch->mtx);
        bool has_data = false;
        if (ch->mode == CHANNEL_MODE_UNBUFFERED) {
            has_data = ch->has_rendezvous_value;
        } else {
            has_data = (ch->count > 0);
        }
        pthread_mutex_unlock(&ch->mtx);
        if (has_data) return i;
    }

    if (timeout_ms == 0) return -1;  // non-blocking, nothing ready

    // Blocking: spin-poll with short sleeps
    struct timespec ts;
    int64_t elapsed_ms = 0;
    const int64_t poll_interval_ms = 1;  // 1ms poll interval

    while (timeout_ms < 0 || elapsed_ms < timeout_ms) {
        ts.tv_sec = 0;
        ts.tv_nsec = poll_interval_ms * 1000000L;
        nanosleep(&ts, NULL);
        elapsed_ms += poll_interval_ms;

        for (int32_t i = 0; i < n; i++) {
            int64_t h = channels[i];
            if (h < 0 || h >= MAX_CHANNELS || !g_channels[h]) continue;
            AriaChannel* ch = g_channels[h];
            pthread_mutex_lock(&ch->mtx);
            bool has_data = false;
            if (ch->mode == CHANNEL_MODE_UNBUFFERED) {
                has_data = ch->has_rendezvous_value;
            } else {
                has_data = (ch->count > 0);
            }
            bool is_closed = ch->closed;
            pthread_mutex_unlock(&ch->mtx);
            if (has_data) return i;
            // If all channels are closed, stop waiting
        }

        // Check if all channels are closed
        bool all_closed = true;
        for (int32_t i = 0; i < n; i++) {
            int64_t h = channels[i];
            if (h >= 0 && h < MAX_CHANNELS && g_channels[h]) {
                AriaChannel* ch = g_channels[h];
                pthread_mutex_lock(&ch->mtx);
                if (!ch->closed) all_closed = false;
                pthread_mutex_unlock(&ch->mtx);
            }
        }
        if (all_closed) return -1;
    }

    return -1;  // timeout
}

/**
 * Select with array passed as handle to a pre-allocated int64 array.
 * Aria doesn't pass C arrays directly — this variant takes individual handles.
 * select2: poll 2 channels. Returns 0 or 1 for ready index, -1 on timeout.
 */
int32_t aria_shim_channel_select2(int64_t ch0, int64_t ch1, int64_t timeout_ms) {
    int64_t chs[2] = { ch0, ch1 };
    return aria_shim_channel_select(chs, 2, timeout_ms);
}

/** select3: poll 3 channels. */
int32_t aria_shim_channel_select3(int64_t ch0, int64_t ch1, int64_t ch2, int64_t timeout_ms) {
    int64_t chs[3] = { ch0, ch1, ch2 };
    return aria_shim_channel_select(chs, 3, timeout_ms);
}

/** select4: poll 4 channels. */
int32_t aria_shim_channel_select4(int64_t ch0, int64_t ch1, int64_t ch2, int64_t ch3, int64_t timeout_ms) {
    int64_t chs[4] = { ch0, ch1, ch2, ch3 };
    return aria_shim_channel_select(chs, 4, timeout_ms);
}

// ============================================================================
// Actor System — Lightweight message-passing actors
// ============================================================================

#define MAX_ACTORS       64
#define ACTOR_MAILBOX_CAP 256

// Actor states
#define ACTOR_STATE_INIT    0
#define ACTOR_STATE_RUNNING 1
#define ACTOR_STATE_STOPPED 2

typedef struct {
    int64_t         mailbox_handle;  // channel handle for incoming messages
    int64_t         thread_handle;   // thread handle
    void*           behavior_func;   // user's message handler: void(void* env, int64_t msg)
    int32_t         state;
    pthread_mutex_t state_mtx;
} AriaActor;

static AriaActor* g_actors[MAX_ACTORS];

static int64_t alloc_actor_slot(AriaActor* a) {
    for (int i = 0; i < MAX_ACTORS; i++) {
        if (!g_actors[i]) { g_actors[i] = a; return (int64_t)i; }
    }
    return -1;
}

// Actor thread loop: receive messages from mailbox, call behavior_func
static void actor_loop_wrapper(void* env, int64_t actor_handle) {
    (void)env;
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return;
    AriaActor* actor = g_actors[actor_handle];
    AriaLambdaFunc handler = (AriaLambdaFunc)actor->behavior_func;

    pthread_mutex_lock(&actor->state_mtx);
    actor->state = ACTOR_STATE_RUNNING;
    pthread_mutex_unlock(&actor->state_mtx);

    while (1) {
        // Check if stopped
        pthread_mutex_lock(&actor->state_mtx);
        if (actor->state == ACTOR_STATE_STOPPED) {
            pthread_mutex_unlock(&actor->state_mtx);
            break;
        }
        pthread_mutex_unlock(&actor->state_mtx);

        // Check if mailbox is closed and empty
        if (aria_shim_channel_is_closed(actor->mailbox_handle) &&
            aria_shim_channel_count(actor->mailbox_handle) == 0) {
            break;
        }

        // Receive message (blocks if empty, returns 0 if closed+empty)
        int64_t msg = aria_shim_channel_recv(actor->mailbox_handle);

        // Re-check closed+empty (recv returns 0 for closed channel)
        pthread_mutex_lock(&actor->state_mtx);
        if (actor->state == ACTOR_STATE_STOPPED) {
            pthread_mutex_unlock(&actor->state_mtx);
            break;
        }
        pthread_mutex_unlock(&actor->state_mtx);

        if (aria_shim_channel_is_closed(actor->mailbox_handle) &&
            aria_shim_channel_count(actor->mailbox_handle) == 0 && msg == 0) {
            break;
        }

        // Dispatch to user handler
        handler(NULL, msg);
    }

    pthread_mutex_lock(&actor->state_mtx);
    actor->state = ACTOR_STATE_STOPPED;
    pthread_mutex_unlock(&actor->state_mtx);
}

/**
 * Spawn an actor with given behavior function.
 * behavior_func: void handler(void* env, int64_t message) — called for each message.
 * Returns actor handle (>= 0) on success, -1 on error.
 */
int64_t aria_shim_actor_spawn(void* behavior_func) {
    if (!behavior_func) return -1;

    // Create mailbox channel
    int64_t mailbox = aria_shim_channel_create((int32_t)ACTOR_MAILBOX_CAP);
    if (mailbox < 0) return -1;

    AriaActor* actor = (AriaActor*)calloc(1, sizeof(AriaActor));
    if (!actor) {
        aria_shim_channel_destroy(mailbox);
        return -1;
    }
    actor->mailbox_handle = mailbox;
    actor->behavior_func = behavior_func;
    actor->state = ACTOR_STATE_INIT;
    pthread_mutex_init(&actor->state_mtx, NULL);

    int64_t actor_handle = alloc_actor_slot(actor);
    if (actor_handle < 0) {
        aria_shim_channel_destroy(mailbox);
        free(actor);
        return -1;
    }

    // Spawn actor thread — pass actor_handle so the loop can look itself up
    int64_t tid = aria_shim_thread_spawn((void*)actor_loop_wrapper, actor_handle);
    if (tid < 0) {
        aria_shim_channel_destroy(mailbox);
        g_actors[actor_handle] = NULL;
        free(actor);
        return -1;
    }
    actor->thread_handle = tid;

    return actor_handle;
}

/** Send a message to an actor's mailbox. Returns 0 on success, -1 on error. */
int32_t aria_shim_actor_send(int64_t actor_handle, int64_t message) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return -1;
    AriaActor* actor = g_actors[actor_handle];
    return aria_shim_channel_send(actor->mailbox_handle, message);
}

/** Try to send a message to actor without blocking. Returns 0 on success, -1 if full/stopped. */
int32_t aria_shim_actor_try_send(int64_t actor_handle, int64_t message) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return -1;
    AriaActor* actor = g_actors[actor_handle];
    return aria_shim_channel_try_send(actor->mailbox_handle, message);
}

/** Get the mailbox channel handle for an actor (for advanced channel ops). */
int64_t aria_shim_actor_mailbox(int64_t actor_handle) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return -1;
    return g_actors[actor_handle]->mailbox_handle;
}

/** Get pending message count in actor's mailbox. */
int32_t aria_shim_actor_pending(int64_t actor_handle) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return 0;
    return aria_shim_channel_count(g_actors[actor_handle]->mailbox_handle);
}

/** Check if actor is still running. Returns 1 if running, 0 if stopped/init, -1 on error. */
int32_t aria_shim_actor_is_alive(int64_t actor_handle) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return -1;
    AriaActor* actor = g_actors[actor_handle];
    pthread_mutex_lock(&actor->state_mtx);
    int32_t alive = (actor->state == ACTOR_STATE_RUNNING) ? 1 : 0;
    pthread_mutex_unlock(&actor->state_mtx);
    return alive;
}

/** Stop an actor gracefully (close mailbox, join thread). Returns 0 on success. */
int32_t aria_shim_actor_stop(int64_t actor_handle) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return -1;
    AriaActor* actor = g_actors[actor_handle];

    pthread_mutex_lock(&actor->state_mtx);
    actor->state = ACTOR_STATE_STOPPED;
    pthread_mutex_unlock(&actor->state_mtx);

    // Close mailbox to unblock recv
    aria_shim_channel_close(actor->mailbox_handle);

    // Join actor thread
    int32_t ret = aria_shim_thread_join(actor->thread_handle);
    return ret;
}

/** Destroy an actor (stop + free resources). Returns 0 on success. */
int32_t aria_shim_actor_destroy(int64_t actor_handle) {
    if (actor_handle < 0 || actor_handle >= MAX_ACTORS || !g_actors[actor_handle]) return -1;
    AriaActor* actor = g_actors[actor_handle];

    // Stop if still running
    pthread_mutex_lock(&actor->state_mtx);
    bool running = (actor->state == ACTOR_STATE_RUNNING);
    pthread_mutex_unlock(&actor->state_mtx);

    if (running) {
        aria_shim_actor_stop(actor_handle);
    }

    // Destroy mailbox
    aria_shim_channel_destroy(actor->mailbox_handle);

    pthread_mutex_destroy(&actor->state_mtx);
    free(actor);
    g_actors[actor_handle] = NULL;
    return 0;
}

// ============================================================================
// Global Reply Channel — for actor reply/ack pattern
// ============================================================================
// Actors can't capture closures (env is always NULL). This provides a
// well-known global channel for actor handlers to send results back.

static int64_t g_reply_channel = -1;

/** Set the global reply channel handle. */
void aria_shim_set_reply_channel(int64_t ch) {
    g_reply_channel = ch;
}

/** Get the global reply channel handle. Returns -1 if not set. */
int64_t aria_shim_get_reply_channel(void) {
    return g_reply_channel;
}

/** Send a value to the global reply channel. Convenience for actor handlers. */
int32_t aria_shim_reply(int64_t value) {
    if (g_reply_channel < 0) return -1;
    return aria_shim_channel_send(g_reply_channel, value);
}

#ifdef __cplusplus
}
#endif
