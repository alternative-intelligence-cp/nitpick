/**
 * src/backend/platform_constants.h
 * 
 * Aria Compiler - Platform-Specific Constants
 * Version: 0.0.7
 * 
 * Defines platform-specific syscall numbers and system constants.
 * Currently supports x86-64 Linux. Other platforms will need conditional compilation.
 * 
 * Dependencies: None (platform headers only)
 */

#ifndef ARIA_BACKEND_PLATFORM_CONSTANTS_H
#define ARIA_BACKEND_PLATFORM_CONSTANTS_H

#include <cstdint>

namespace aria {
namespace backend {
namespace platform {

// =============================================================================
// Platform Detection
// =============================================================================

#if defined(__linux__) && defined(__x86_64__)
    #define ARIA_PLATFORM_LINUX_X86_64 1
#elif defined(__linux__) && defined(__aarch64__)
    #define ARIA_PLATFORM_LINUX_AARCH64 1
#elif defined(_WIN32) || defined(_WIN64)
    #define ARIA_PLATFORM_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    #define ARIA_PLATFORM_MACOS 1
#else
    #error "Unsupported platform for Aria compiler"
#endif

// =============================================================================
// Linux x86-64 Syscall Numbers
// =============================================================================

#ifdef ARIA_PLATFORM_LINUX_X86_64

constexpr uint64_t SYSCALL_READ = 0;
constexpr uint64_t SYSCALL_WRITE = 1;
constexpr uint64_t SYSCALL_OPEN = 2;
constexpr uint64_t SYSCALL_CLOSE = 3;
constexpr uint64_t SYSCALL_LSEEK = 8;
constexpr uint64_t SYSCALL_MMAP = 9;
constexpr uint64_t SYSCALL_MPROTECT = 10;

// Memory protection flags (for mmap/mprotect)
constexpr uint64_t PROT_NONE = 0x0;
constexpr uint64_t PROT_READ = 0x1;
constexpr uint64_t PROT_WRITE = 0x2;
constexpr uint64_t PROT_EXEC = 0x4;

// Memory mapping flags (for mmap)
constexpr uint64_t MAP_SHARED = 0x01;
constexpr uint64_t MAP_PRIVATE = 0x02;
constexpr uint64_t MAP_ANONYMOUS = 0x20;
constexpr uint64_t MAP_FIXED = 0x10;

#endif // ARIA_PLATFORM_LINUX_X86_64

// =============================================================================
// Linux ARM64 Syscall Numbers
// =============================================================================

#ifdef ARIA_PLATFORM_LINUX_AARCH64

constexpr uint64_t SYSCALL_READ = 63;
constexpr uint64_t SYSCALL_WRITE = 64;
constexpr uint64_t SYSCALL_OPEN = 1024;  // openat syscall
constexpr uint64_t SYSCALL_CLOSE = 57;
constexpr uint64_t SYSCALL_LSEEK = 62;
constexpr uint64_t SYSCALL_MMAP = 222;
constexpr uint64_t SYSCALL_MPROTECT = 226;

// Memory protection flags (same as x86-64)
constexpr uint64_t PROT_NONE = 0x0;
constexpr uint64_t PROT_READ = 0x1;
constexpr uint64_t PROT_WRITE = 0x2;
constexpr uint64_t PROT_EXEC = 0x4;

// Memory mapping flags (same as x86-64)
constexpr uint64_t MAP_SHARED = 0x01;
constexpr uint64_t MAP_PRIVATE = 0x02;
constexpr uint64_t MAP_ANONYMOUS = 0x20;
constexpr uint64_t MAP_FIXED = 0x10;

#endif // ARIA_PLATFORM_LINUX_AARCH64

// =============================================================================
// Windows Constants (Placeholder - Requires Different Approach)
// =============================================================================

#ifdef ARIA_PLATFORM_WINDOWS

// Windows doesn't use syscalls the same way. We'd need to use Win32 API calls.
// This is a placeholder for future Windows support.
// Would need VirtualAlloc, VirtualProtect, CreateFile, ReadFile, WriteFile, etc.

#endif // ARIA_PLATFORM_WINDOWS

// =============================================================================
// macOS Constants (Placeholder)
// =============================================================================

#ifdef ARIA_PLATFORM_MACOS

// macOS syscall numbers differ from Linux
// Reference: https://github.com/opensource-apple/xnu/blob/master/bsd/kern/syscalls.master
constexpr uint64_t SYSCALL_READ = 3;
constexpr uint64_t SYSCALL_WRITE = 4;
constexpr uint64_t SYSCALL_OPEN = 5;
constexpr uint64_t SYSCALL_CLOSE = 6;
constexpr uint64_t SYSCALL_LSEEK = 199;
constexpr uint64_t SYSCALL_MMAP = 197;
constexpr uint64_t SYSCALL_MPROTECT = 74;

// Memory protection flags (BSD-compatible)
constexpr uint64_t PROT_NONE = 0x0;
constexpr uint64_t PROT_READ = 0x1;
constexpr uint64_t PROT_WRITE = 0x2;
constexpr uint64_t PROT_EXEC = 0x4;

// Memory mapping flags (BSD-compatible)
constexpr uint64_t MAP_SHARED = 0x0001;
constexpr uint64_t MAP_PRIVATE = 0x0002;
constexpr uint64_t MAP_ANONYMOUS = 0x1000;
constexpr uint64_t MAP_FIXED = 0x0010;

#endif // ARIA_PLATFORM_MACOS

} // namespace platform
} // namespace backend
} // namespace aria

#endif // ARIA_BACKEND_PLATFORM_CONSTANTS_H
