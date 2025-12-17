/**
 * Aria Threading Library Implementation
 * Cross-platform threading primitives wrapping POSIX threads and Windows threads.
 */

#include "runtime/thread.h"
#include "runtime/io.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <time.h>
    #include <errno.h>
    #include <sys/time.h>
#endif

/* ============================================================================
 * Platform-Specific Structures
 * ============================================================================ */

#ifdef _WIN32
struct AriaThread {
    HANDLE handle;
    DWORD thread_id;
    AriaThreadFunc func;
    void* arg;
    void* return_value;
    bool detached;
};

struct AriaMutex {
    CRITICAL_SECTION cs;
    bool recursive;
};

struct AriaCondVar {
    CONDITION_VARIABLE cv;
};

struct AriaThreadLocal {
    DWORD key;
    AriaThreadLocalDestructor destructor;
};

struct AriaRWLock {
    SRWLOCK lock;
};

struct AriaBarrier {
    SYNCHRONIZATION_BARRIER barrier;
};

#else // POSIX

struct AriaThread {
    pthread_t handle;
    AriaThreadFunc func;
    void* arg;
    void* return_value;
    bool detached;
    bool joined;
};

struct AriaMutex {
    pthread_mutex_t mutex;
    AriaMutexType type;
};

struct AriaCondVar {
    pthread_cond_t cond;
};

struct AriaThreadLocal {
    pthread_key_t key;
    AriaThreadLocalDestructor destructor;
};

struct AriaRWLock {
    pthread_rwlock_t lock;
};

struct AriaBarrier {
    pthread_barrier_t barrier;
};

#endif

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static const char* get_error_message(const char* operation) {
    static char buffer[256];
#ifdef _WIN32
    DWORD error = GetLastError();
    snprintf(buffer, sizeof(buffer), "%s failed with error %lu", operation, error);
#else
    snprintf(buffer, sizeof(buffer), "%s failed: %s", operation, strerror(errno));
#endif
    return buffer;
}

/* ============================================================================
 * Thread Lifecycle Management
 * ============================================================================ */

#ifdef _WIN32
static DWORD WINAPI thread_wrapper(LPVOID arg) {
    AriaThread* thread = (AriaThread*)arg;
    thread->return_value = thread->func(thread->arg);
    return 0;
}
#else
static void* thread_wrapper(void* arg) {
    AriaThread* thread = (AriaThread*)arg;
    return thread->func(thread->arg);
}
#endif

AriaResult* aria_thread_create(AriaThreadFunc func, void* arg) {
    AriaThreadOptions default_options = {0};
    return aria_thread_create_with_options(func, arg, &default_options);
}

AriaResult* aria_thread_create_with_options(AriaThreadFunc func, void* arg,
                                             const AriaThreadOptions* options) {
    if (!func) {
        return aria_result_err("Thread function cannot be NULL");
    }

    AriaThread* thread = (AriaThread*)calloc(1, sizeof(AriaThread));
    if (!thread) {
        return aria_result_err("Out of memory");
    }

    thread->func = func;
    thread->arg = arg;
    thread->detached = options->detached;
    thread->return_value = NULL;

#ifdef _WIN32
    SIZE_T stack_size = options->stack_size;
    thread->handle = CreateThread(
        NULL,
        stack_size,
        thread_wrapper,
        thread,
        0,
        &thread->thread_id
    );

    if (thread->handle == NULL) {
        free(thread);
        return aria_result_err(get_error_message("CreateThread"));
    }

    if (options->priority >= 0) {
        SetThreadPriority(thread->handle, options->priority);
    }

    if (options->detached) {
        CloseHandle(thread->handle);
        thread->handle = NULL;
    }

#else // POSIX
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    if (options->stack_size > 0) {
        pthread_attr_setstacksize(&attr, options->stack_size);
    }

    if (options->detached) {
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    }

    int result = pthread_create(&thread->handle, &attr, thread_wrapper, thread);
    pthread_attr_destroy(&attr);

    if (result != 0) {
        free(thread);
        return aria_result_err(get_error_message("pthread_create"));
    }

    if (options->name) {
#ifdef __linux__
        pthread_setname_np(thread->handle, options->name);
#elif defined(__APPLE__)
        // macOS requires thread to set its own name from within
#endif
    }

    thread->joined = false;
#endif

    return aria_result_ok(thread, sizeof(AriaThread));
}

AriaResult* aria_thread_join(AriaThread* thread, void** ret_val) {
    if (!thread) {
        return aria_result_err("Thread handle cannot be NULL");
    }

    if (thread->detached) {
        return aria_result_err("Cannot join a detached thread");
    }

#ifdef _WIN32
    if (thread->handle == NULL) {
        return aria_result_err("Thread handle is invalid");
    }

    DWORD wait_result = WaitForSingleObject(thread->handle, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        return aria_result_err(get_error_message("WaitForSingleObject"));
    }

    if (ret_val) {
        *ret_val = thread->return_value;
    }

    CloseHandle(thread->handle);
    thread->handle = NULL;

#else // POSIX
    if (thread->joined) {
        return aria_result_err("Thread already joined");
    }

    void* return_value = NULL;
    int result = pthread_join(thread->handle, &return_value);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_join"));
    }

    thread->joined = true;

    if (ret_val) {
        *ret_val = return_value;
    }
#endif

    free(thread);
    return aria_result_ok(NULL, 0);
}

AriaResult* aria_thread_detach(AriaThread* thread) {
    if (!thread) {
        return aria_result_err("Thread handle cannot be NULL");
    }

    if (thread->detached) {
        return aria_result_err("Thread already detached");
    }

#ifdef _WIN32
    if (thread->handle) {
        CloseHandle(thread->handle);
        thread->handle = NULL;
    }
#else
    int result = pthread_detach(thread->handle);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_detach"));
    }
#endif

    thread->detached = true;
    return aria_result_ok(NULL, 0);
}

AriaThreadId aria_thread_current_id(void) {
    AriaThreadId tid;
#ifdef _WIN32
    tid.id = GetCurrentThreadId();
#else
    tid.id = (uint64_t)pthread_self();
#endif
    return tid;
}

AriaThreadId aria_thread_get_id(AriaThread* thread) {
    AriaThreadId tid;
    if (!thread) {
        tid.id = 0;
        return tid;
    }
#ifdef _WIN32
    tid.id = thread->thread_id;
#else
    tid.id = (uint64_t)thread->handle;
#endif
    return tid;
}

bool aria_thread_id_equal(AriaThreadId tid1, AriaThreadId tid2) {
    return tid1.id == tid2.id;
}

void aria_thread_yield(void) {
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}

void aria_thread_sleep_ns(uint64_t nanoseconds) {
#ifdef _WIN32
    DWORD milliseconds = (DWORD)(nanoseconds / 1000000);
    if (milliseconds == 0 && nanoseconds > 0) {
        milliseconds = 1;
    }
    Sleep(milliseconds);
#else
    struct timespec ts;
    ts.tv_sec = nanoseconds / 1000000000ULL;
    ts.tv_nsec = nanoseconds % 1000000000ULL;
    nanosleep(&ts, NULL);
#endif
}

void aria_thread_set_name(const char* name) {
    if (!name) return;

#ifdef _WIN32
    // Windows thread naming requires special handling
    // Omitted for brevity
#elif defined(__linux__)
    pthread_setname_np(pthread_self(), name);
#elif defined(__APPLE__)
    pthread_setname_np(name);
#endif
}

/* ============================================================================
 * Mutex Synchronization
 * ============================================================================ */

AriaResult* aria_mutex_create(AriaMutexType type) {
    AriaMutex* mutex = (AriaMutex*)calloc(1, sizeof(AriaMutex));
    if (!mutex) {
        return aria_result_err("Out of memory");
    }

    mutex->type = type;

#ifdef _WIN32
    InitializeCriticalSection(&mutex->cs);
    mutex->recursive = (type == ARIA_MUTEX_RECURSIVE);
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);

    if (type == ARIA_MUTEX_RECURSIVE) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    } else {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    }

    int result = pthread_mutex_init(&mutex->mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    if (result != 0) {
        free(mutex);
        return aria_result_err(get_error_message("pthread_mutex_init"));
    }
#endif

    return aria_result_ok(mutex, sizeof(AriaMutex));
}

AriaResult* aria_mutex_destroy(AriaMutex* mutex) {
    if (!mutex) {
        return aria_result_err("Mutex handle cannot be NULL");
    }

#ifdef _WIN32
    DeleteCriticalSection(&mutex->cs);
#else
    int result = pthread_mutex_destroy(&mutex->mutex);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_mutex_destroy"));
    }
#endif

    free(mutex);
    return aria_result_ok(NULL, 0);
}

AriaResult* aria_mutex_lock(AriaMutex* mutex) {
    if (!mutex) {
        return aria_result_err("Mutex handle cannot be NULL");
    }

#ifdef _WIN32
    EnterCriticalSection(&mutex->cs);
#else
    int result = pthread_mutex_lock(&mutex->mutex);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_mutex_lock"));
    }
#endif

    return aria_result_ok(NULL, 0);
}

AriaResult* aria_mutex_trylock(AriaMutex* mutex) {
    if (!mutex) {
        return aria_result_err("Mutex handle cannot be NULL");
    }

#ifdef _WIN32
    BOOL acquired = TryEnterCriticalSection(&mutex->cs);
    if (!acquired) {
        return aria_result_ok((void*)0, 0); // success=false indicates lock held
    }
#else
    int result = pthread_mutex_trylock(&mutex->mutex);
    if (result == EBUSY) {
        return aria_result_ok((void*)0, 0); // success=false indicates lock held
    }
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_mutex_trylock"));
    }
#endif

    return aria_result_ok((void*)1, 1); // success=true indicates lock acquired
}

AriaResult* aria_mutex_unlock(AriaMutex* mutex) {
    if (!mutex) {
        return aria_result_err("Mutex handle cannot be NULL");
    }

#ifdef _WIN32
    LeaveCriticalSection(&mutex->cs);
#else
    int result = pthread_mutex_unlock(&mutex->mutex);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_mutex_unlock"));
    }
#endif

    return aria_result_ok(NULL, 0);
}

/* ============================================================================
 * Condition Variables
 * ============================================================================ */

AriaResult* aria_condvar_create(void) {
    AriaCondVar* condvar = (AriaCondVar*)calloc(1, sizeof(AriaCondVar));
    if (!condvar) {
        return aria_result_err("Out of memory");
    }

#ifdef _WIN32
    InitializeConditionVariable(&condvar->cv);
#else
    int result = pthread_cond_init(&condvar->cond, NULL);
    if (result != 0) {
        free(condvar);
        return aria_result_err(get_error_message("pthread_cond_init"));
    }
#endif

    return aria_result_ok(condvar, sizeof(AriaCondVar));
}

AriaResult* aria_condvar_destroy(AriaCondVar* condvar) {
    if (!condvar) {
        return aria_result_err("Condition variable handle cannot be NULL");
    }

#ifdef _WIN32
    // Windows condition variables don't need explicit destruction
#else
    int result = pthread_cond_destroy(&condvar->cond);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_cond_destroy"));
    }
#endif

    free(condvar);
    return aria_result_ok(NULL, 0);
}

AriaResult* aria_condvar_wait(AriaCondVar* condvar, AriaMutex* mutex) {
    if (!condvar || !mutex) {
        return aria_result_err("Condition variable and mutex handles cannot be NULL");
    }

#ifdef _WIN32
    BOOL result = SleepConditionVariableCS(&condvar->cv, &mutex->cs, INFINITE);
    if (!result) {
        return aria_result_err(get_error_message("SleepConditionVariableCS"));
    }
#else
    int result = pthread_cond_wait(&condvar->cond, &mutex->mutex);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_cond_wait"));
    }
#endif

    return aria_result_ok(NULL, 0);
}

AriaResult* aria_condvar_timedwait(AriaCondVar* condvar, AriaMutex* mutex,
                                    uint64_t timeout_ns) {
    if (!condvar || !mutex) {
        return aria_result_err("Condition variable and mutex handles cannot be NULL");
    }

#ifdef _WIN32
    DWORD timeout_ms = (DWORD)(timeout_ns / 1000000);
    BOOL result = SleepConditionVariableCS(&condvar->cv, &mutex->cs, timeout_ms);
    if (!result) {
        if (GetLastError() == ERROR_TIMEOUT) {
            return aria_result_ok((void*)0, 0); // success=false indicates timeout
        }
        return aria_result_err(get_error_message("SleepConditionVariableCS"));
    }
#else
    struct timespec abstime;
    struct timeval now;
    gettimeofday(&now, NULL);

    abstime.tv_sec = now.tv_sec + (timeout_ns / 1000000000ULL);
    abstime.tv_nsec = (now.tv_usec * 1000) + (timeout_ns % 1000000000ULL);

    if (abstime.tv_nsec >= 1000000000) {
        abstime.tv_sec++;
        abstime.tv_nsec -= 1000000000;
    }

    int result = pthread_cond_timedwait(&condvar->cond, &mutex->mutex, &abstime);
    if (result == ETIMEDOUT) {
        return aria_result_ok((void*)0, 0); // success=false indicates timeout
    }
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_cond_timedwait"));
    }
#endif

    return aria_result_ok((void*)1, 1); // success=true indicates signaled
}

AriaResult* aria_condvar_signal(AriaCondVar* condvar) {
    if (!condvar) {
        return aria_result_err("Condition variable handle cannot be NULL");
    }

#ifdef _WIN32
    WakeConditionVariable(&condvar->cv);
#else
    int result = pthread_cond_signal(&condvar->cond);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_cond_signal"));
    }
#endif

    return aria_result_ok(NULL, 0);
}

AriaResult* aria_condvar_broadcast(AriaCondVar* condvar) {
    if (!condvar) {
        return aria_result_err("Condition variable handle cannot be NULL");
    }

#ifdef _WIN32
    WakeAllConditionVariable(&condvar->cv);
#else
    int result = pthread_cond_broadcast(&condvar->cond);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_cond_broadcast"));
    }
#endif

    return aria_result_ok(NULL, 0);
}

/* ============================================================================
 * Thread-Local Storage
 * ============================================================================ */

AriaResult* aria_thread_local_create(AriaThreadLocalDestructor destructor) {
    AriaThreadLocal* tls = (AriaThreadLocal*)calloc(1, sizeof(AriaThreadLocal));
    if (!tls) {
        return aria_result_err("Out of memory");
    }

    tls->destructor = destructor;

#ifdef _WIN32
    tls->key = TlsAlloc();
    if (tls->key == TLS_OUT_OF_INDEXES) {
        free(tls);
        return aria_result_err(get_error_message("TlsAlloc"));
    }
#else
    int result = pthread_key_create(&tls->key, destructor);
    if (result != 0) {
        free(tls);
        return aria_result_err(get_error_message("pthread_key_create"));
    }
#endif

    return aria_result_ok(tls, sizeof(AriaThreadLocal));
}

AriaResult* aria_thread_local_destroy(AriaThreadLocal* tls) {
    if (!tls) {
        return aria_result_err("TLS key handle cannot be NULL");
    }

#ifdef _WIN32
    TlsFree(tls->key);
#else
    int result = pthread_key_delete(tls->key);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_key_delete"));
    }
#endif

    free(tls);
    return aria_result_ok(NULL, 0);
}

void* aria_thread_local_get(AriaThreadLocal* tls) {
    if (!tls) {
        return NULL;
    }

#ifdef _WIN32
    return TlsGetValue(tls->key);
#else
    return pthread_getspecific(tls->key);
#endif
}

AriaResult* aria_thread_local_set(AriaThreadLocal* tls, void* value) {
    if (!tls) {
        return aria_result_err("TLS key handle cannot be NULL");
    }

#ifdef _WIN32
    if (!TlsSetValue(tls->key, value)) {
        return aria_result_err(get_error_message("TlsSetValue"));
    }
#else
    int result = pthread_setspecific(tls->key, value);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_setspecific"));
    }
#endif

    return aria_result_ok(NULL, 0);
}

/* ============================================================================
 * Read-Write Locks
 * ============================================================================ */

AriaResult* aria_rwlock_create(void) {
    AriaRWLock* rwlock = (AriaRWLock*)calloc(1, sizeof(AriaRWLock));
    if (!rwlock) {
        return aria_result_err("Out of memory");
    }

#ifdef _WIN32
    InitializeSRWLock(&rwlock->lock);
#else
    int result = pthread_rwlock_init(&rwlock->lock, NULL);
    if (result != 0) {
        free(rwlock);
        return aria_result_err(get_error_message("pthread_rwlock_init"));
    }
#endif

    return aria_result_ok(rwlock, sizeof(AriaRWLock));
}

AriaResult* aria_rwlock_destroy(AriaRWLock* rwlock) {
    if (!rwlock) {
        return aria_result_err("RW lock handle cannot be NULL");
    }

#ifdef _WIN32
    // Windows SRW locks don't need explicit destruction
#else
    int result = pthread_rwlock_destroy(&rwlock->lock);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_rwlock_destroy"));
    }
#endif

    free(rwlock);
    return aria_result_ok(NULL, 0);
}

AriaResult* aria_rwlock_rdlock(AriaRWLock* rwlock) {
    if (!rwlock) {
        return aria_result_err("RW lock handle cannot be NULL");
    }

#ifdef _WIN32
    AcquireSRWLockShared(&rwlock->lock);
#else
    int result = pthread_rwlock_rdlock(&rwlock->lock);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_rwlock_rdlock"));
    }
#endif

    return aria_result_ok(NULL, 0);
}

AriaResult* aria_rwlock_tryrdlock(AriaRWLock* rwlock) {
    if (!rwlock) {
        return aria_result_err("RW lock handle cannot be NULL");
    }

#ifdef _WIN32
    BOOL acquired = TryAcquireSRWLockShared(&rwlock->lock);
    if (!acquired) {
        return aria_result_ok((void*)0, 0); // success=false indicates lock held
    }
#else
    int result = pthread_rwlock_tryrdlock(&rwlock->lock);
    if (result == EBUSY) {
        return aria_result_ok((void*)0, 0); // success=false indicates lock held
    }
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_rwlock_tryrdlock"));
    }
#endif

    return aria_result_ok((void*)1, 1); // success=true indicates lock acquired
}

AriaResult* aria_rwlock_wrlock(AriaRWLock* rwlock) {
    if (!rwlock) {
        return aria_result_err("RW lock handle cannot be NULL");
    }

#ifdef _WIN32
    AcquireSRWLockExclusive(&rwlock->lock);
#else
    int result = pthread_rwlock_wrlock(&rwlock->lock);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_rwlock_wrlock"));
    }
#endif

    return aria_result_ok(NULL, 0);
}

AriaResult* aria_rwlock_trywrlock(AriaRWLock* rwlock) {
    if (!rwlock) {
        return aria_result_err("RW lock handle cannot be NULL");
    }

#ifdef _WIN32
    BOOL acquired = TryAcquireSRWLockExclusive(&rwlock->lock);
    if (!acquired) {
        return aria_result_ok((void*)0, 0); // success=false indicates lock held
    }
#else
    int result = pthread_rwlock_trywrlock(&rwlock->lock);
    if (result == EBUSY) {
        return aria_result_ok((void*)0, 0); // success=false indicates lock held
    }
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_rwlock_trywrlock"));
    }
#endif

    return aria_result_ok((void*)1, 1); // success=true indicates lock acquired
}

AriaResult* aria_rwlock_unlock(AriaRWLock* rwlock) {
    if (!rwlock) {
        return aria_result_err("RW lock handle cannot be NULL");
    }

#ifdef _WIN32
    // Windows requires knowing whether to unlock shared or exclusive
    // For simplicity, we track this in implementation-specific way
    // Here we assume caller manages this correctly
    ReleaseSRWLockExclusive(&rwlock->lock);
#else
    int result = pthread_rwlock_unlock(&rwlock->lock);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_rwlock_unlock"));
    }
#endif

    return aria_result_ok(NULL, 0);
}

/* ============================================================================
 * Barriers
 * ============================================================================ */

AriaResult* aria_barrier_create(uint32_t count) {
    if (count == 0) {
        return aria_result_err("Barrier count must be greater than zero");
    }

    AriaBarrier* barrier = (AriaBarrier*)calloc(1, sizeof(AriaBarrier));
    if (!barrier) {
        return aria_result_err("Out of memory");
    }

#ifdef _WIN32
    InitializeSynchronizationBarrier(&barrier->barrier, count, -1);
#else
    int result = pthread_barrier_init(&barrier->barrier, NULL, count);
    if (result != 0) {
        free(barrier);
        return aria_result_err(get_error_message("pthread_barrier_init"));
    }
#endif

    return aria_result_ok(barrier, sizeof(AriaBarrier));
}

AriaResult* aria_barrier_destroy(AriaBarrier* barrier) {
    if (!barrier) {
        return aria_result_err("Barrier handle cannot be NULL");
    }

#ifdef _WIN32
    DeleteSynchronizationBarrier(&barrier->barrier);
#else
    int result = pthread_barrier_destroy(&barrier->barrier);
    if (result != 0) {
        return aria_result_err(get_error_message("pthread_barrier_destroy"));
    }
#endif

    free(barrier);
    return aria_result_ok(NULL, 0);
}

AriaResult* aria_barrier_wait(AriaBarrier* barrier) {
    if (!barrier) {
        return aria_result_err("Barrier handle cannot be NULL");
    }

#ifdef _WIN32
    EnterSynchronizationBarrier(&barrier->barrier, 0);
#else
    int result = pthread_barrier_wait(&barrier->barrier);
    if (result != 0 && result != PTHREAD_BARRIER_SERIAL_THREAD) {
        return aria_result_err(get_error_message("pthread_barrier_wait"));
    }
#endif

    return aria_result_ok(NULL, 0);
}

/* ============================================================================
 * Hardware Information
 * ============================================================================ */

uint32_t aria_thread_hardware_concurrency(void) {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#else
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    return (nproc > 0) ? (uint32_t)nproc : 1;
#endif
}
