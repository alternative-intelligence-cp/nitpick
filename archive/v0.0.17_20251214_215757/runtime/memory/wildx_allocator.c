/**
 * src/runtime/memory/wildx_allocator.c
 * 
 * Wildx Allocator: Executable Memory for JIT Compilation
 * Implements W^X (Write XOR Execute) security model
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
#endif

/**
 * aria_alloc_exec - Allocate executable-capable memory
 * 
 * Allocates page-aligned memory that can transition between:
 * - RW (Read/Write) state for code generation
 * - RX (Read/Execute) state for code execution
 * 
 * Initial state: RW (writable, not executable)
 * 
 * @param size: Number of bytes to allocate
 * @return: Pointer to allocated memory, or NULL on failure
 * 
 * Implementation Notes:
 * - Memory is page-aligned (typically 4096 bytes)
 * - Rounds size up to nearest page boundary
 * - Uses mmap on POSIX systems
 * - Uses VirtualAlloc on Windows
 * - On macOS ARM64, uses MAP_JIT for fast permission toggling
 */
void* aria_alloc_exec(size_t size) {
    if (size == 0) return NULL;
    
#ifdef _WIN32
    // Windows: VirtualAlloc
    // Allocate with PAGE_READWRITE initially
    void* ptr = VirtualAlloc(
        NULL,                      // Let system choose address
        size,                      // Size in bytes
        MEM_COMMIT | MEM_RESERVE,  // Commit and reserve
        PAGE_READWRITE             // Initial protection: RW
    );
    
    return ptr; // NULL on failure
    
#else
    // POSIX: mmap
    // Get page size
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) page_size = 4096; // Fallback
    
    // Round size up to page boundary
    size_t alloc_size = (size + page_size - 1) & ~(page_size - 1);
    
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    
    // macOS ARM64: Use MAP_JIT for fast permission toggling
    #if defined(__APPLE__) && defined(__aarch64__)
        flags |= MAP_JIT;
    #endif
    
    // Allocate RW memory
    void* ptr = mmap(
        NULL,                          // Let system choose address
        alloc_size,                    // Rounded size
        PROT_READ | PROT_WRITE,        // Initial protection: RW
        flags,                         // Private anonymous (+ MAP_JIT on macOS ARM64)
        -1,                            // No file descriptor
        0                              // No offset
    );
    
    if (ptr == MAP_FAILED) {
        return NULL;
    }
    
    return ptr;
#endif
}

/**
 * aria_free_exec - Deallocate executable memory
 * 
 * @param ptr: Pointer returned by aria_alloc_exec
 * @param size: Original size passed to aria_alloc_exec
 */
void aria_free_exec(void* ptr, size_t size) {
    if (!ptr) return;
    
#ifdef _WIN32
    // Windows: VirtualFree
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    // POSIX: munmap
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) page_size = 4096;
    
    size_t alloc_size = (size + page_size - 1) & ~(page_size - 1);
    munmap(ptr, alloc_size);
#endif
}

/**
 * aria_mem_protect_exec - Transition memory to executable (RX) state
 * 
 * Changes protection from RW -> RX and flushes instruction cache
 * After calling this, writes to the memory will cause access violations
 * 
 * @param ptr: Pointer to memory allocated with aria_alloc_exec
 * @param size: Size of the region to protect
 * @return: 0 on success, -1 on failure
 */
int aria_mem_protect_exec(void* ptr, size_t size) {
    if (!ptr || size == 0) return -1;
    
#ifdef _WIN32
    // Windows: VirtualProtect to PAGE_EXECUTE_READ
    DWORD old_protect;
    if (!VirtualProtect(ptr, size, PAGE_EXECUTE_READ, &old_protect)) {
        return -1; // Failed
    }
    
    // Windows: Flush instruction cache
    FlushInstructionCache(GetCurrentProcess(), ptr, size);
    return 0;
    
#else
    // POSIX: mprotect to RX
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) page_size = 4096;
    
    // Round size up to page boundary
    size_t alloc_size = (size + page_size - 1) & ~(page_size - 1);
    
    if (mprotect(ptr, alloc_size, PROT_READ | PROT_EXEC) != 0) {
        return -1; // Failed
    }
    
    // Flush instruction cache (architecture-specific)
    #if defined(__GNUC__) || defined(__clang__)
        // GCC/Clang builtin
        __builtin___clear_cache((char*)ptr, (char*)ptr + alloc_size);
    #endif
    
    return 0;
#endif
}

/**
 * aria_mem_protect_write - Transition memory to writable (RW) state
 * 
 * Changes protection from RX -> RW for patching/updating code
 * After calling this, the memory can be written but not executed
 * 
 * @param ptr: Pointer to memory allocated with aria_alloc_exec
 * @param size: Size of the region to protect
 * @return: 0 on success, -1 on failure
 */
int aria_mem_protect_write(void* ptr, size_t size) {
    if (!ptr || size == 0) return -1;
    
#ifdef _WIN32
    // Windows: VirtualProtect to PAGE_READWRITE
    DWORD old_protect;
    if (!VirtualProtect(ptr, size, PAGE_READWRITE, &old_protect)) {
        return -1;
    }
    return 0;
    
#else
    // POSIX: mprotect to RW
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) page_size = 4096;
    
    size_t alloc_size = (size + page_size - 1) & ~(page_size - 1);
    
    if (mprotect(ptr, alloc_size, PROT_READ | PROT_WRITE) != 0) {
        return -1;
    }
    
    return 0;
#endif
}
