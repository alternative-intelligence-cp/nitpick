/**
 * Aria OS Components Shim — Memory Allocators, IPC, Signals, Process Mgmt
 *
 * Memory Allocators:
 * - Arena: fast bump allocator, bulk free
 * - Pool: fixed-size block allocator
 *
 * IPC:
 * - Shared memory regions (mmap-based)
 * - Pipe pairs
 *
 * Signals:
 * - Signal handler registration + pending signal query
 *
 * Process:
 * - Fork/exec/wait, getpid, environment
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 1. Arena Allocator
// ============================================================================

#define ARENA_DEFAULT_SIZE (1024 * 1024)  // 1 MB

typedef struct {
    char*    base;
    int64_t  capacity;
    int64_t  offset;
} Arena;

#define MAX_ARENAS 64
static Arena* g_arenas[MAX_ARENAS];

static int64_t alloc_arena_slot(Arena* a) {
    for (int i = 0; i < MAX_ARENAS; i++) {
        if (!g_arenas[i]) { g_arenas[i] = a; return (int64_t)i; }
    }
    return -1;
}

int64_t npk_shim_arena_create(int64_t capacity) {
    if (capacity <= 0) capacity = ARENA_DEFAULT_SIZE;
    Arena* a = (Arena*)malloc(sizeof(Arena));
    if (!a) return -1;
    a->base = (char*)malloc((size_t)capacity);
    if (!a->base) { free(a); return -1; }
    a->capacity = capacity;
    a->offset = 0;
    return alloc_arena_slot(a);
}

int32_t npk_shim_arena_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_ARENAS || !g_arenas[handle]) return -1;
    Arena* a = g_arenas[handle];
    free(a->base);
    free(a);
    g_arenas[handle] = NULL;
    return 0;
}

// Allocate n bytes from arena, returns offset (or -1 if full)
int64_t npk_shim_arena_alloc(int64_t handle, int64_t nbytes) {
    if (handle < 0 || handle >= MAX_ARENAS || !g_arenas[handle]) return -1;
    Arena* a = g_arenas[handle];
    // Align to 8 bytes
    int64_t aligned = (a->offset + 7) & ~7;
    if (aligned + nbytes > a->capacity) return -1; // full
    int64_t result = aligned;
    a->offset = aligned + nbytes;
    return result;
}

// Reset arena (bulk free)
int32_t npk_shim_arena_reset(int64_t handle) {
    if (handle < 0 || handle >= MAX_ARENAS || !g_arenas[handle]) return -1;
    g_arenas[handle]->offset = 0;
    return 0;
}

// Get used bytes
int64_t npk_shim_arena_used(int64_t handle) {
    if (handle < 0 || handle >= MAX_ARENAS || !g_arenas[handle]) return -1;
    return g_arenas[handle]->offset;
}

// Get remaining capacity
int64_t npk_shim_arena_remaining(int64_t handle) {
    if (handle < 0 || handle >= MAX_ARENAS || !g_arenas[handle]) return -1;
    Arena* a = g_arenas[handle];
    return a->capacity - a->offset;
}

// Write int64 at offset
int32_t npk_shim_arena_write_int64(int64_t handle, int64_t offset, int64_t value) {
    if (handle < 0 || handle >= MAX_ARENAS || !g_arenas[handle]) return -1;
    Arena* a = g_arenas[handle];
    if (offset < 0 || offset + 8 > a->capacity) return -1;
    memcpy(a->base + offset, &value, sizeof(int64_t));
    return 0;
}

// Read int64 at offset
int64_t npk_shim_arena_read_int64(int64_t handle, int64_t offset) {
    if (handle < 0 || handle >= MAX_ARENAS || !g_arenas[handle]) return 0;
    Arena* a = g_arenas[handle];
    if (offset < 0 || offset + 8 > a->capacity) return 0;
    int64_t value;
    memcpy(&value, a->base + offset, sizeof(int64_t));
    return value;
}

// ============================================================================
// 2. Pool Allocator (fixed-size blocks)
// ============================================================================

typedef struct {
    char*    base;
    int32_t* free_list;    // stack of free block indices
    int64_t  block_size;
    int64_t  block_count;
    int64_t  free_top;     // top of free stack
} Pool;

#define MAX_POOLS_ALLOC 64
static Pool* g_pool_allocs[MAX_POOLS_ALLOC];

static int64_t alloc_pool_alloc_slot(Pool* p) {
    for (int i = 0; i < MAX_POOLS_ALLOC; i++) {
        if (!g_pool_allocs[i]) { g_pool_allocs[i] = p; return (int64_t)i; }
    }
    return -1;
}

int64_t npk_shim_pool_alloc_create(int64_t block_size, int64_t block_count) {
    if (block_size <= 0) block_size = 64;
    if (block_count <= 0) block_count = 256;
    // Align block_size to 8 bytes
    block_size = (block_size + 7) & ~7;

    Pool* p = (Pool*)malloc(sizeof(Pool));
    if (!p) return -1;

    p->base = (char*)calloc((size_t)block_count, (size_t)block_size);
    p->free_list = (int32_t*)malloc(sizeof(int32_t) * (size_t)block_count);
    if (!p->base || !p->free_list) {
        free(p->base);
        free(p->free_list);
        free(p);
        return -1;
    }

    p->block_size = block_size;
    p->block_count = block_count;
    p->free_top = block_count;

    // Initialize free list with all blocks
    for (int64_t i = 0; i < block_count; i++) {
        p->free_list[i] = (int32_t)i;
    }

    return alloc_pool_alloc_slot(p);
}

int32_t npk_shim_pool_alloc_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_POOLS_ALLOC || !g_pool_allocs[handle]) return -1;
    Pool* p = g_pool_allocs[handle];
    free(p->base);
    free(p->free_list);
    free(p);
    g_pool_allocs[handle] = NULL;
    return 0;
}

// Allocate a block, returns block index (or -1 if exhausted)
int64_t npk_shim_pool_alloc_get(int64_t handle) {
    if (handle < 0 || handle >= MAX_POOLS_ALLOC || !g_pool_allocs[handle]) return -1;
    Pool* p = g_pool_allocs[handle];
    if (p->free_top == 0) return -1; // exhausted
    p->free_top--;
    return (int64_t)p->free_list[p->free_top];
}

// Return a block to the pool
int32_t npk_shim_pool_alloc_put(int64_t handle, int64_t block_index) {
    if (handle < 0 || handle >= MAX_POOLS_ALLOC || !g_pool_allocs[handle]) return -1;
    Pool* p = g_pool_allocs[handle];
    if (block_index < 0 || block_index >= p->block_count) return -1;
    if (p->free_top >= p->block_count) return -1; // double free
    p->free_list[p->free_top] = (int32_t)block_index;
    p->free_top++;
    return 0;
}

// Write int64 into a pool block at offset within the block
int32_t npk_shim_pool_alloc_write_int64(int64_t handle, int64_t block_index, int64_t offset, int64_t value) {
    if (handle < 0 || handle >= MAX_POOLS_ALLOC || !g_pool_allocs[handle]) return -1;
    Pool* p = g_pool_allocs[handle];
    if (block_index < 0 || block_index >= p->block_count) return -1;
    if (offset < 0 || offset + 8 > p->block_size) return -1;
    char* ptr = p->base + (block_index * p->block_size) + offset;
    memcpy(ptr, &value, sizeof(int64_t));
    return 0;
}

// Read int64 from a pool block at offset
int64_t npk_shim_pool_alloc_read_int64(int64_t handle, int64_t block_index, int64_t offset) {
    if (handle < 0 || handle >= MAX_POOLS_ALLOC || !g_pool_allocs[handle]) return 0;
    Pool* p = g_pool_allocs[handle];
    if (block_index < 0 || block_index >= p->block_count) return 0;
    if (offset < 0 || offset + 8 > p->block_size) return 0;
    int64_t value;
    char* ptr = p->base + (block_index * p->block_size) + offset;
    memcpy(&value, ptr, sizeof(int64_t));
    return value;
}

// Get number of free blocks
int64_t npk_shim_pool_alloc_available(int64_t handle) {
    if (handle < 0 || handle >= MAX_POOLS_ALLOC || !g_pool_allocs[handle]) return -1;
    return g_pool_allocs[handle]->free_top;
}

// ============================================================================
// 3. IPC — Shared Memory
// ============================================================================

typedef struct {
    void*   addr;
    int64_t size;
} ShmRegion;

#define MAX_SHM 64
static ShmRegion* g_shm[MAX_SHM];

static int64_t alloc_shm_slot(ShmRegion* s) {
    for (int i = 0; i < MAX_SHM; i++) {
        if (!g_shm[i]) { g_shm[i] = s; return (int64_t)i; }
    }
    return -1;
}

int64_t npk_shim_shm_create(int64_t size) {
    if (size <= 0) return -1;
    void* addr = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) return -1;

    ShmRegion* r = (ShmRegion*)malloc(sizeof(ShmRegion));
    if (!r) { munmap(addr, (size_t)size); return -1; }
    r->addr = addr;
    r->size = size;
    return alloc_shm_slot(r);
}

int32_t npk_shim_shm_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_SHM || !g_shm[handle]) return -1;
    ShmRegion* r = g_shm[handle];
    munmap(r->addr, (size_t)r->size);
    free(r);
    g_shm[handle] = NULL;
    return 0;
}

int32_t npk_shim_shm_write_int64(int64_t handle, int64_t offset, int64_t value) {
    if (handle < 0 || handle >= MAX_SHM || !g_shm[handle]) return -1;
    ShmRegion* r = g_shm[handle];
    if (offset < 0 || offset + 8 > r->size) return -1;
    memcpy((char*)r->addr + offset, &value, sizeof(int64_t));
    return 0;
}

int64_t npk_shim_shm_read_int64(int64_t handle, int64_t offset) {
    if (handle < 0 || handle >= MAX_SHM || !g_shm[handle]) return 0;
    ShmRegion* r = g_shm[handle];
    if (offset < 0 || offset + 8 > r->size) return 0;
    int64_t value;
    memcpy(&value, (char*)r->addr + offset, sizeof(int64_t));
    return value;
}

int64_t npk_shim_shm_size(int64_t handle) {
    if (handle < 0 || handle >= MAX_SHM || !g_shm[handle]) return -1;
    return g_shm[handle]->size;
}

// ============================================================================
// 4. IPC — Pipes
// ============================================================================

typedef struct {
    int read_fd;
    int write_fd;
} PipePair;

#define MAX_PIPES 64
static PipePair* g_pipes[MAX_PIPES];

static int64_t alloc_pipe_slot(PipePair* p) {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!g_pipes[i]) { g_pipes[i] = p; return (int64_t)i; }
    }
    return -1;
}

int64_t npk_shim_pipe_create(void) {
    int fds[2];
    if (pipe(fds) != 0) return -1;
    PipePair* p = (PipePair*)malloc(sizeof(PipePair));
    if (!p) { close(fds[0]); close(fds[1]); return -1; }
    p->read_fd = fds[0];
    p->write_fd = fds[1];
    return alloc_pipe_slot(p);
}

int32_t npk_shim_pipe_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_PIPES || !g_pipes[handle]) return -1;
    PipePair* p = g_pipes[handle];
    if (p->read_fd >= 0) close(p->read_fd);
    if (p->write_fd >= 0) close(p->write_fd);
    free(p);
    g_pipes[handle] = NULL;
    return 0;
}

int32_t npk_shim_pipe_write_int64(int64_t handle, int64_t value) {
    if (handle < 0 || handle >= MAX_PIPES || !g_pipes[handle]) return -1;
    PipePair* p = g_pipes[handle];
    if (p->write_fd < 0) return -1;
    ssize_t n = write(p->write_fd, &value, sizeof(int64_t));
    return (n == sizeof(int64_t)) ? 0 : -1;
}

int64_t npk_shim_pipe_read_int64(int64_t handle) {
    if (handle < 0 || handle >= MAX_PIPES || !g_pipes[handle]) return -1;
    PipePair* p = g_pipes[handle];
    if (p->read_fd < 0) return -1;
    int64_t value;
    ssize_t n = read(p->read_fd, &value, sizeof(int64_t));
    return (n == sizeof(int64_t)) ? value : -1;
}

int32_t npk_shim_pipe_close_write(int64_t handle) {
    if (handle < 0 || handle >= MAX_PIPES || !g_pipes[handle]) return -1;
    PipePair* p = g_pipes[handle];
    if (p->write_fd >= 0) { close(p->write_fd); p->write_fd = -1; }
    return 0;
}

int32_t npk_shim_pipe_close_read(int64_t handle) {
    if (handle < 0 || handle >= MAX_PIPES || !g_pipes[handle]) return -1;
    PipePair* p = g_pipes[handle];
    if (p->read_fd >= 0) { close(p->read_fd); p->read_fd = -1; }
    return 0;
}

// ============================================================================
// 5. Signal Handling
// ============================================================================

#define MAX_SIGNAL_COUNT 64
static volatile sig_atomic_t g_signal_pending[MAX_SIGNAL_COUNT];

static void signal_handler(int signo) {
    if (signo >= 0 && signo < MAX_SIGNAL_COUNT) {
        g_signal_pending[signo] = 1;
    }
}

int32_t npk_shim_signal_register(int32_t signo) {
    if (signo < 0 || signo >= MAX_SIGNAL_COUNT) return -1;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(signo, &sa, NULL) != 0) return -1;
    return 0;
}

// Check if signal is pending, clears the flag
int32_t npk_shim_signal_pending(int32_t signo) {
    if (signo < 0 || signo >= MAX_SIGNAL_COUNT) return 0;
    if (g_signal_pending[signo]) {
        g_signal_pending[signo] = 0;
        return 1;
    }
    return 0;
}

int32_t npk_shim_signal_ignore(int32_t signo) {
    if (signo < 0 || signo >= MAX_SIGNAL_COUNT) return -1;
    signal(signo, SIG_IGN);
    return 0;
}

int32_t npk_shim_signal_restore(int32_t signo) {
    if (signo < 0 || signo >= MAX_SIGNAL_COUNT) return -1;
    signal(signo, SIG_DFL);
    return 0;
}

// ============================================================================
// 6. Process Management
// ============================================================================

int64_t npk_shim_process_getpid(void) {
    return (int64_t)getpid();
}

int64_t npk_shim_process_getppid(void) {
    return (int64_t)getppid();
}

// Simple system() call — returns exit status
int32_t npk_shim_process_system(const char* cmd) {
    int status = system(cmd);
    if (status == -1) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Get environment variable, returns "" if not found
// (Aria will get this as a string via the extern ABI)
const char* npk_shim_process_getenv(const char* name) {
    const char* val = getenv(name);
    return val ? val : "";
}

#ifdef __cplusplus
}
#endif
