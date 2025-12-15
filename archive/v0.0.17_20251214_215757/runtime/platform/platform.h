/**
 * src/runtime/platform/platform.h
 * 
 * Cross-Platform Abstraction Layer
 * Aria Runtime Platform Interface
 * 
 * Provides unified interface for platform-specific operations across:
 * - Linux (x86-64, ARM64)
 * - macOS (x86-64, ARM64/Apple Silicon)
 * - Windows (x86-64, ARM64)
 * 
 * This header defines the platform detection macros and common interfaces
 * that abstract OS-specific functionality.
 */

#ifndef ARIA_RUNTIME_PLATFORM_H
#define ARIA_RUNTIME_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Platform Detection Macros
// =============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define ARIA_PLATFORM_WINDOWS 1
    #define ARIA_PLATFORM_POSIX 0
    #define ARIA_PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
    #define ARIA_PLATFORM_MACOS 1
    #define ARIA_PLATFORM_POSIX 1
    #define ARIA_PLATFORM_NAME "macOS"
    #ifdef __aarch64__
        #define ARIA_PLATFORM_APPLE_SILICON 1
    #else
        #define ARIA_PLATFORM_APPLE_SILICON 0
    #endif
#elif defined(__linux__)
    #define ARIA_PLATFORM_LINUX 1
    #define ARIA_PLATFORM_POSIX 1
    #define ARIA_PLATFORM_NAME "Linux"
#else
    #define ARIA_PLATFORM_UNKNOWN 1
    #define ARIA_PLATFORM_POSIX 0
    #define ARIA_PLATFORM_NAME "Unknown"
    #warning "Unknown platform - some features may not work"
#endif

// Architecture detection
#if defined(__x86_64__) || defined(_M_X64)
    #define ARIA_ARCH_X86_64 1
    #define ARIA_ARCH_NAME "x86-64"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ARIA_ARCH_ARM64 1
    #define ARIA_ARCH_NAME "ARM64"
#else
    #define ARIA_ARCH_UNKNOWN 1
    #define ARIA_ARCH_NAME "Unknown"
#endif

// =============================================================================
// Platform Constants
// =============================================================================

// Page size (typically 4096, but can be queried at runtime)
#define ARIA_DEFAULT_PAGE_SIZE 4096

// File descriptor constants (POSIX)
#if ARIA_PLATFORM_POSIX
    #define ARIA_STDIN_FD  0
    #define ARIA_STDOUT_FD 1
    #define ARIA_STDERR_FD 2
    #define ARIA_STDDBG_FD 3
    #define ARIA_DATI_FD   4
    #define ARIA_DATO_FD   5
#endif

// =============================================================================
// Platform-Agnostic Types
// =============================================================================

// Process handle (platform-specific implementation)
#ifdef _WIN32
    typedef void* aria_process_handle_t;  // HANDLE on Windows
#else
    typedef int aria_process_handle_t;    // pid_t on POSIX
#endif

// Thread handle (platform-specific implementation)
#ifdef _WIN32
    typedef void* aria_thread_handle_t;   // HANDLE on Windows
#else
    typedef unsigned long aria_thread_handle_t;  // pthread_t on POSIX
#endif

// =============================================================================
// Memory Management (Cross-Platform)
// =============================================================================

/**
 * Get system page size
 * @return Page size in bytes (typically 4096)
 */
size_t aria_platform_get_page_size(void);

/**
 * Allocate page-aligned memory
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void* aria_platform_alloc_pages(size_t size);

/**
 * Free page-aligned memory
 * @param ptr Pointer returned by aria_platform_alloc_pages
 * @param size Original size passed to aria_platform_alloc_pages
 */
void aria_platform_free_pages(void* ptr, size_t size);

// =============================================================================
// Executable Memory (Cross-Platform Wildx Support)
// =============================================================================
// Implemented in wildx_allocator.c - these are already cross-platform

// aria_alloc_exec() - Allocate executable memory (RW initially)
// aria_free_exec() - Free executable memory
// aria_mem_protect_exec() - Transition to RX (executable)
// aria_mem_protect_write() - Transition to RW (writable)

// =============================================================================
// Process and I/O (Cross-Platform)
// =============================================================================

/**
 * Spawn child process with 6-channel I/O
 * Channels: stdin, stdout, stderr, stddbg, dati, dato
 * 
 * Platform implementations:
 * - Linux: io_linux.cpp (fork + exec + dup2)
 * - Windows: io_windows.cpp (CreateProcess + lpReserved2)
 * - macOS: Same as Linux (POSIX)
 */
int aria_platform_spawn_process(const char* cmd, const char* const* argv,
                                 int fd_stdin, int fd_stdout, int fd_stderr,
                                 int fd_dbg, int fd_dati, int fd_dato);

// =============================================================================
// Threading (Cross-Platform)
// =============================================================================

/**
 * Get number of hardware threads (CPU cores)
 * @return Number of logical cores, or 4 as fallback
 */
unsigned int aria_platform_get_cpu_count(void);

/**
 * Create a thread
 * @param start_routine Function to execute
 * @param arg Argument to pass to thread function
 * @return Thread handle, or 0 on failure
 */
aria_thread_handle_t aria_platform_create_thread(void* (*start_routine)(void*), void* arg);

/**
 * Wait for thread to complete
 * @param handle Thread handle returned by aria_platform_create_thread
 * @return 0 on success, -1 on failure
 */
int aria_platform_join_thread(aria_thread_handle_t handle);

/**
 * Yield CPU to other threads
 */
void aria_platform_thread_yield(void);

// =============================================================================
// Synchronization Primitives
// =============================================================================

// Mutex (cross-platform)
typedef struct aria_mutex_t aria_mutex_t;

aria_mutex_t* aria_platform_mutex_create(void);
void aria_platform_mutex_destroy(aria_mutex_t* mutex);
void aria_platform_mutex_lock(aria_mutex_t* mutex);
void aria_platform_mutex_unlock(aria_mutex_t* mutex);

// =============================================================================
// Time and Performance Counters
// =============================================================================

/**
 * Get high-resolution timestamp (nanoseconds)
 * @return Current time in nanoseconds since arbitrary epoch
 */
uint64_t aria_platform_get_time_ns(void);

/**
 * Sleep for specified duration
 * @param milliseconds Duration to sleep in milliseconds
 */
void aria_platform_sleep_ms(uint64_t milliseconds);

// =============================================================================
// Filesystem Utilities
// =============================================================================

/**
 * Check if file exists
 * @param path File path
 * @return 1 if exists, 0 if not
 */
int aria_platform_file_exists(const char* path);

/**
 * Get file size
 * @param path File path
 * @return File size in bytes, or -1 on error
 */
int64_t aria_platform_file_size(const char* path);

// =============================================================================
// Error Handling
// =============================================================================

/**
 * Get last platform error code
 * @return Error code (errno on POSIX, GetLastError() on Windows)
 */
int aria_platform_get_last_error(void);

/**
 * Get error message for error code
 * @param error_code Error code
 * @param buffer Buffer to store error message
 * @param size Size of buffer
 * @return Pointer to buffer
 */
const char* aria_platform_error_string(int error_code, char* buffer, size_t size);

// =============================================================================
// Platform-Specific Notes
// =============================================================================

/*
 * WINDOWS-SPECIFIC CONSIDERATIONS:
 * - Uses VirtualAlloc/VirtualFree for memory
 * - Uses CreateThread/WaitForSingleObject for threading
 * - Uses CreateProcess with lpReserved2 for 6-channel I/O
 * - Uses QueryPerformanceCounter for high-res time
 * 
 * MACOS-SPECIFIC CONSIDERATIONS:
 * - ARM64 (Apple Silicon) uses MAP_JIT for fast permission toggling
 * - Uses pthread_jit_write_protect_np for W^X transitions
 * - Requires special entitlements for JIT compilation
 * 
 * LINUX-SPECIFIC CONSIDERATIONS:
 * - Uses mmap/munmap for memory
 * - Uses pthread for threading
 * - Uses fork/exec for process spawning
 * - Uses clock_gettime for high-res time
 */

// =============================================================================
// Work Package 005: Platform Abstraction Layer Enhancements
// =============================================================================

/// Unified file metadata structure
/// All timestamps are Unix epoch (seconds since 1970-01-01 00:00:00 UTC)
typedef struct aria_file_stat {
    uint64_t size;           ///< File size in bytes
    uint64_t created_time;   ///< Creation time (Unix timestamp)
    uint64_t modified_time;  ///< Last modification time (Unix timestamp)
    uint64_t accessed_time;  ///< Last access time (Unix timestamp)
    int is_directory;        ///< Non-zero if this is a directory
    int is_readonly;         ///< Non-zero if readonly attribute is set
} aria_file_stat_t;

/// Get file metadata in a platform-independent way
/// 
/// @param path File path (UTF-8 encoded)
/// @param out Output structure to fill
/// @return Non-zero on success, zero on error
int aria_file_stat(const char* path, aria_file_stat_t* out);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_PLATFORM_H
