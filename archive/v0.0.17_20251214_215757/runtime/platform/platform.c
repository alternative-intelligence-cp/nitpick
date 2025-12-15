/**
 * src/runtime/platform/platform.c
 * 
 * Cross-Platform Abstraction Layer Implementation
 * Provides unified implementations for platform-specific operations
 */

#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
#else
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/mman.h>
    #include <time.h>
    #include <sys/stat.h>
#endif

// =============================================================================
// Memory Management
// =============================================================================

size_t aria_platform_get_page_size(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t)si.dwPageSize;
#else
    long page_size = sysconf(_SC_PAGESIZE);
    return (page_size > 0) ? (size_t)page_size : ARIA_DEFAULT_PAGE_SIZE;
#endif
}

void* aria_platform_alloc_pages(size_t size) {
    if (size == 0) return NULL;
    
#ifdef _WIN32
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    size_t page_size = aria_platform_get_page_size();
    size_t alloc_size = (size + page_size - 1) & ~(page_size - 1);
    
    void* ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (ptr == MAP_FAILED) ? NULL : ptr;
#endif
}

void aria_platform_free_pages(void* ptr, size_t size) {
    if (!ptr) return;
    
#ifdef _WIN32
    (void)size;  // Unused on Windows
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    size_t page_size = aria_platform_get_page_size();
    size_t alloc_size = (size + page_size - 1) & ~(page_size - 1);
    munmap(ptr, alloc_size);
#endif
}

// =============================================================================
// Threading
// =============================================================================

unsigned int aria_platform_get_cpu_count(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (unsigned int)si.dwNumberOfProcessors;
#else
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    return (nprocs > 0) ? (unsigned int)nprocs : 4;
#endif
}

#ifdef _WIN32
// Windows thread wrapper to match POSIX signature
struct thread_start_wrapper {
    void* (*start_routine)(void*);
    void* arg;
};

static unsigned __stdcall thread_start_wrapper_func(void* arg) {
    struct thread_start_wrapper* wrapper = (struct thread_start_wrapper*)arg;
    void* (*start_routine)(void*) = wrapper->start_routine;
    void* thread_arg = wrapper->arg;
    free(wrapper);
    
    start_routine(thread_arg);
    return 0;
}
#endif

aria_thread_handle_t aria_platform_create_thread(void* (*start_routine)(void*), void* arg) {
#ifdef _WIN32
    struct thread_start_wrapper* wrapper = (struct thread_start_wrapper*)malloc(sizeof(struct thread_start_wrapper));
    if (!wrapper) return 0;
    
    wrapper->start_routine = start_routine;
    wrapper->arg = arg;
    
    HANDLE handle = (HANDLE)_beginthreadex(NULL, 0, thread_start_wrapper_func, wrapper, 0, NULL);
    return (aria_thread_handle_t)handle;
#else
    pthread_t thread;
    if (pthread_create(&thread, NULL, start_routine, arg) != 0) {
        return 0;
    }
    return (aria_thread_handle_t)thread;
#endif
}

int aria_platform_join_thread(aria_thread_handle_t handle) {
#ifdef _WIN32
    DWORD result = WaitForSingleObject((HANDLE)handle, INFINITE);
    CloseHandle((HANDLE)handle);
    return (result == WAIT_OBJECT_0) ? 0 : -1;
#else
    int result = pthread_join((pthread_t)handle, NULL);
    return (result == 0) ? 0 : -1;
#endif
}

void aria_platform_thread_yield(void) {
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}

// =============================================================================
// Synchronization Primitives
// =============================================================================

struct aria_mutex_t {
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t mutex;
#endif
};

aria_mutex_t* aria_platform_mutex_create(void) {
    aria_mutex_t* mutex = (aria_mutex_t*)malloc(sizeof(aria_mutex_t));
    if (!mutex) return NULL;
    
#ifdef _WIN32
    InitializeCriticalSection(&mutex->cs);
#else
    pthread_mutex_init(&mutex->mutex, NULL);
#endif
    
    return mutex;
}

void aria_platform_mutex_destroy(aria_mutex_t* mutex) {
    if (!mutex) return;
    
#ifdef _WIN32
    DeleteCriticalSection(&mutex->cs);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif
    
    free(mutex);
}

void aria_platform_mutex_lock(aria_mutex_t* mutex) {
    if (!mutex) return;
    
#ifdef _WIN32
    EnterCriticalSection(&mutex->cs);
#else
    pthread_mutex_lock(&mutex->mutex);
#endif
}

void aria_platform_mutex_unlock(aria_mutex_t* mutex) {
    if (!mutex) return;
    
#ifdef _WIN32
    LeaveCriticalSection(&mutex->cs);
#else
    pthread_mutex_unlock(&mutex->mutex);
#endif
}

// =============================================================================
// Time and Performance Counters
// =============================================================================

uint64_t aria_platform_get_time_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    
    // Convert to nanoseconds
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

void aria_platform_sleep_ms(uint64_t milliseconds) {
#ifdef _WIN32
    Sleep((DWORD)milliseconds);
#else
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}

// =============================================================================
// Filesystem Utilities
// =============================================================================

int aria_platform_file_exists(const char* path) {
    if (!path) return 0;
    
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) ? 1 : 0;
#else
    struct stat st;
    return (stat(path, &st) == 0) ? 1 : 0;
#endif
}

int64_t aria_platform_file_size(const char* path) {
    if (!path) return -1;
    
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) {
        return -1;
    }
    
    LARGE_INTEGER size;
    size.HighPart = fad.nFileSizeHigh;
    size.LowPart = fad.nFileSizeLow;
    return (int64_t)size.QuadPart;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    return (int64_t)st.st_size;
#endif
}

// =============================================================================
// Error Handling
// =============================================================================

int aria_platform_get_last_error(void) {
#ifdef _WIN32
    return (int)GetLastError();
#else
    return errno;
#endif
}

const char* aria_platform_error_string(int error_code, char* buffer, size_t size) {
    if (!buffer || size == 0) return NULL;
    
#ifdef _WIN32
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        (DWORD)error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buffer,
        (DWORD)size,
        NULL
    );
#else
    // POSIX strerror_r
    #if defined(__GLIBC__) && !defined(_GNU_SOURCE)
        // XSI-compliant version
        if (strerror_r(error_code, buffer, size) == 0) {
            return buffer;
        }
    #else
        // GNU version or other systems
        char* msg = strerror(error_code);
        if (msg) {
            strncpy(buffer, msg, size - 1);
            buffer[size - 1] = '\0';
        }
    #endif
#endif
    
    return buffer;
}

// =============================================================================
// Process Spawning
// =============================================================================

int aria_platform_spawn_process(const char* cmd, const char* const* argv,
                                 int fd_stdin, int fd_stdout, int fd_stderr,
                                 int fd_dbg, int fd_dati, int fd_dato) {
#ifdef _WIN32
    // Windows implementation would use CreateProcess with lpReserved2
    // See io_windows.cpp for full implementation
    (void)cmd; (void)argv; (void)fd_stdin; (void)fd_stdout; (void)fd_stderr;
    (void)fd_dbg; (void)fd_dati; (void)fd_dato;
    return -1;  // Not implemented in this abstraction layer yet
#else
    // POSIX implementation using fork + exec
    // See io_linux.cpp for full implementation
    pid_t pid = fork();
    if (pid < 0) return -1;
    
    if (pid == 0) {
        // Child process: remap file descriptors
        if (fd_stdin != 0) {
            dup2(fd_stdin, 0);
            close(fd_stdin);
        }
        if (fd_stdout != 1) {
            dup2(fd_stdout, 1);
            close(fd_stdout);
        }
        if (fd_stderr != 2) {
            dup2(fd_stderr, 2);
            close(fd_stderr);
        }
        if (fd_dbg != 3) {
            dup2(fd_dbg, 3);
            close(fd_dbg);
        }
        if (fd_dati != 4) {
            dup2(fd_dati, 4);
            close(fd_dati);
        }
        if (fd_dato != 5) {
            dup2(fd_dato, 5);
            close(fd_dato);
        }
        
        execvp(cmd, (char* const*)argv);
        _exit(127);  // exec failed
    }
    
    return (int)pid;  // Parent: return child PID
#endif
}

// =============================================================================
// Work Package 005: File System Abstraction
// =============================================================================

#ifdef _WIN32

// Windows epoch is 1601-01-01, Unix epoch is 1970-01-01
// Difference is 116444736000000000 ticks (100ns units)
#define UNIX_TIME_START 0x019DB1DED53E8000ULL
#define TICKS_PER_SECOND 10000000ULL

static uint64_t filetime_to_unix(const FILETIME* ft) {
    ULARGE_INTEGER li;
    li.LowPart = ft->dwLowDateTime;
    li.HighPart = ft->dwHighDateTime;
    
    // Saturating subtract to avoid underflow if file predates 1970
    if (li.QuadPart < UNIX_TIME_START) return 0;
    return (li.QuadPart - UNIX_TIME_START) / TICKS_PER_SECOND;
}

int aria_file_stat(const char* path, aria_file_stat_t* out) {
    if (!path || !out) return 0;
    
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
        return 0;
    }
    
    ULARGE_INTEGER size;
    size.LowPart = attrs.nFileSizeLow;
    size.HighPart = attrs.nFileSizeHigh;
    out->size = size.QuadPart;
    
    out->created_time = filetime_to_unix(&attrs.ftCreationTime);
    out->modified_time = filetime_to_unix(&attrs.ftLastWriteTime);
    out->accessed_time = filetime_to_unix(&attrs.ftLastAccessTime);
    
    out->is_directory = (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    out->is_readonly = (attrs.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
    
    return 1;
}

#else // POSIX (Linux/macOS)

#ifdef __linux__
    #define _GNU_SOURCE
    #include <fcntl.h>
    #include <sys/syscall.h>
    
    // Linux has statx for creation time (kernel 4.11+)
    // But we need to handle older systems gracefully
    #ifndef STATX_BTIME
        #define STATX_BTIME 0x0800
    #endif
#endif

#ifdef __APPLE__
    #include <sys/stat.h>
#endif

int aria_file_stat(const char* path, aria_file_stat_t* out) {
    if (!path || !out) return 0;
    
#ifdef __linux__
    // Try statx first for creation time support
    struct statx stx;
    if (syscall(SYS_statx, AT_FDCWD, path, 0, 
                STATX_BTIME | STATX_MTIME | STATX_ATIME | STATX_SIZE, 
                &stx) == 0) {
        out->size = stx.stx_size;
        out->modified_time = stx.stx_mtime.tv_sec;
        out->accessed_time = stx.stx_atime.tv_sec;
        
        // Check if birth time is available
        if (stx.stx_mask & STATX_BTIME) {
            out->created_time = stx.stx_btime.tv_sec;
        } else {
            // Fallback: use ctime (status change time)
            out->created_time = stx.stx_ctime.tv_sec;
        }
        
        out->is_directory = S_ISDIR(stx.stx_mode);
        out->is_readonly = (stx.stx_mode & S_IWUSR) == 0;
        return 1;
    }
    
    // Fallback to regular stat if statx not available
#endif
    
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    
    out->size = st.st_size;
    out->modified_time = st.st_mtime;
    out->accessed_time = st.st_atime;
    
#ifdef __APPLE__
    // macOS has st_birthtime for creation time
    out->created_time = st.st_birthtime;
#else
    // Linux: ctime is status change time, not creation time
    // Best we can do without statx
    out->created_time = st.st_ctime;
#endif
    
    out->is_directory = S_ISDIR(st.st_mode);
    out->is_readonly = (st.st_mode & S_IWUSR) == 0;
    
    return 1;
}

#endif // POSIX
