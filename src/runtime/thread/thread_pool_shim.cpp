/**
 * Aria Thread Pool Shim — Handle-Based Work-Stealing Pool
 *
 * Implements a fixed-size thread pool with a shared task queue.
 * Workers pull tasks from the queue and execute them. Uses pthreads
 * directly for maximum control.
 *
 * Same handle pattern as thread_shim / atomic_shim.
 * All functions prefixed with aria_shim_pool_*.
 */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Aria lambda calling convention: void func(void* env, int64_t arg)
typedef void (*AriaLambdaFunc)(void* env, int64_t arg);

// ============================================================================
// Task Queue (circular buffer)
// ============================================================================

typedef struct {
    AriaLambdaFunc func;
    int64_t        arg;
} PoolTask;

#define TASK_QUEUE_CAP 4096

typedef struct {
    PoolTask       tasks[TASK_QUEUE_CAP];
    int            head;
    int            tail;
    int            count;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
    bool           shutdown;
} TaskQueue;

static void queue_init(TaskQueue* q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->shutdown = false;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

static void queue_destroy(TaskQueue* q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

// Push task, blocking if full. Returns 0 on success, -1 if shutdown.
static int queue_push(TaskQueue* q, AriaLambdaFunc func, int64_t arg) {
    pthread_mutex_lock(&q->lock);
    while (q->count == TASK_QUEUE_CAP && !q->shutdown) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    if (q->shutdown) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }
    q->tasks[q->tail].func = func;
    q->tasks[q->tail].arg  = arg;
    q->tail = (q->tail + 1) % TASK_QUEUE_CAP;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

// Pop task, blocking if empty. Returns 0 on success, -1 if shutdown + empty.
static int queue_pop(TaskQueue* q, PoolTask* out) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0 && !q->shutdown) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    if (q->count == 0 && q->shutdown) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }
    *out = q->tasks[q->head];
    q->head = (q->head + 1) % TASK_QUEUE_CAP;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

// ============================================================================
// Thread Pool
// ============================================================================

#define MAX_POOL_WORKERS 256

typedef struct {
    pthread_t   workers[MAX_POOL_WORKERS];
    int         num_workers;
    TaskQueue   queue;
    volatile int64_t active_tasks;  // tasks currently being executed
    pthread_mutex_t active_lock;
    bool        alive;
} ThreadPool;

static void* pool_worker(void* raw) {
    ThreadPool* pool = (ThreadPool*)raw;
    PoolTask task;
    while (queue_pop(&pool->queue, &task) == 0) {
        __sync_fetch_and_add(&pool->active_tasks, 1);
        task.func(NULL, task.arg);  // NULL env for non-closure functions
        __sync_fetch_and_sub(&pool->active_tasks, 1);
    }
    return NULL;
}

// ============================================================================
// Handle Pool
// ============================================================================

#define MAX_POOLS 32

static ThreadPool* g_pools[MAX_POOLS];

static int64_t alloc_pool_slot(ThreadPool* p) {
    for (int i = 0; i < MAX_POOLS; i++) {
        if (!g_pools[i]) { g_pools[i] = p; return (int64_t)i; }
    }
    return -1;
}

// ============================================================================
// Public API
// ============================================================================

/**
 * Create a thread pool with num_workers threads.
 * If num_workers <= 0, uses a default of 4.
 * Returns pool handle (>= 0) on success, -1 on error.
 */
int64_t aria_shim_pool_create(int32_t num_workers) {
    if (num_workers <= 0) num_workers = 4;
    if (num_workers > MAX_POOL_WORKERS) num_workers = MAX_POOL_WORKERS;

    ThreadPool* pool = (ThreadPool*)calloc(1, sizeof(ThreadPool));
    if (!pool) return -1;

    pool->num_workers = num_workers;
    pool->active_tasks = 0;
    pool->alive = true;
    pthread_mutex_init(&pool->active_lock, NULL);
    queue_init(&pool->queue);

    for (int i = 0; i < num_workers; i++) {
        if (pthread_create(&pool->workers[i], NULL, pool_worker, pool) != 0) {
            // Partial creation — shut down what we have
            pool->queue.shutdown = true;
            pthread_cond_broadcast(&pool->queue.not_empty);
            for (int j = 0; j < i; j++) {
                pthread_join(pool->workers[j], NULL);
            }
            queue_destroy(&pool->queue);
            pthread_mutex_destroy(&pool->active_lock);
            free(pool);
            return -1;
        }
    }

    return alloc_pool_slot(pool);
}

/**
 * Submit a task to the pool. func is an Aria method_ptr (extracted from fat pointer).
 * Returns 0 on success, -1 on error.
 */
int32_t aria_shim_pool_submit(int64_t handle, void* func, int64_t arg) {
    if (handle < 0 || handle >= MAX_POOLS || !g_pools[handle]) return -1;
    if (!func) return -1;
    ThreadPool* pool = g_pools[handle];
    if (!pool->alive) return -1;
    return queue_push(&pool->queue, (AriaLambdaFunc)func, arg) == 0 ? 0 : -1;
}

/**
 * Shutdown the pool: signal workers to stop, wait for all to finish.
 * Returns 0 on success, -1 on error.
 */
int32_t aria_shim_pool_shutdown(int64_t handle) {
    if (handle < 0 || handle >= MAX_POOLS || !g_pools[handle]) return -1;
    ThreadPool* pool = g_pools[handle];
    if (!pool->alive) return -1;

    pool->alive = false;

    // Signal shutdown
    pthread_mutex_lock(&pool->queue.lock);
    pool->queue.shutdown = true;
    pthread_cond_broadcast(&pool->queue.not_empty);
    pthread_cond_broadcast(&pool->queue.not_full);
    pthread_mutex_unlock(&pool->queue.lock);

    // Join all workers
    for (int i = 0; i < pool->num_workers; i++) {
        pthread_join(pool->workers[i], NULL);
    }

    queue_destroy(&pool->queue);
    pthread_mutex_destroy(&pool->active_lock);
    free(pool);
    g_pools[handle] = NULL;
    return 0;
}

/**
 * Get the number of tasks currently being executed.
 */
int64_t aria_shim_pool_active_tasks(int64_t handle) {
    if (handle < 0 || handle >= MAX_POOLS || !g_pools[handle]) return -1;
    return __sync_fetch_and_add(&g_pools[handle]->active_tasks, 0);
}

/**
 * Get the number of pending tasks in the queue.
 */
int64_t aria_shim_pool_pending_tasks(int64_t handle) {
    if (handle < 0 || handle >= MAX_POOLS || !g_pools[handle]) return -1;
    ThreadPool* pool = g_pools[handle];
    pthread_mutex_lock(&pool->queue.lock);
    int count = pool->queue.count;
    pthread_mutex_unlock(&pool->queue.lock);
    return (int64_t)count;
}

/**
 * Get the number of worker threads.
 */
int32_t aria_shim_pool_worker_count(int64_t handle) {
    if (handle < 0 || handle >= MAX_POOLS || !g_pools[handle]) return -1;
    return g_pools[handle]->num_workers;
}

/**
 * Wait until all submitted tasks have been processed.
 * Polls queue count + active tasks, sleeping briefly between checks.
 */
int32_t aria_shim_pool_wait_idle(int64_t handle) {
    if (handle < 0 || handle >= MAX_POOLS || !g_pools[handle]) return -1;
    ThreadPool* pool = g_pools[handle];
    while (1) {
        pthread_mutex_lock(&pool->queue.lock);
        int pending = pool->queue.count;
        pthread_mutex_unlock(&pool->queue.lock);
        int64_t active = __sync_fetch_and_add(&pool->active_tasks, 0);
        if (pending == 0 && active == 0) break;
        // Brief yield to avoid busy-spinning
        struct timespec ts = {0, 1000000}; // 1ms
        nanosleep(&ts, NULL);
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
