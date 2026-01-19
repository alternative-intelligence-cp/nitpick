# Aria Runtime Library (libaria_runtime.a)

**Component Type**: Static Runtime Library  
**Language**: C (C11 standard)  
**Link Name**: `libaria_runtime.a`  
**Location**: `/usr/local/lib/libaria_runtime.a`  
**Headers**: `/usr/local/include/aria/runtime/`  
**Status**: Active Development

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Module Breakdown](#module-breakdown)
4. [6-Stream I/O System](#6-stream-io-system)
5. [Memory Allocators](#memory-allocators)
6. [Process Management](#process-management)
7. [Error Handling](#error-handling)
8. [Collections](#collections)
9. [Async Runtime](#async-runtime)
10. [Platform Support](#platform-support)

---

## Overview

The Aria Runtime Library provides the core functionality needed by all Aria programs. It is **statically linked** into every Aria executable, providing:

- **6-stream I/O**: stdin/stdout/stderr + stddbg/stddati/stddato (FDs 0-5)
- **Memory allocators**: Arena, Pool, Slab, Wild, Garbage Collection
- **Process management**: Spawning, waiting, signaling
- **Error handling**: Result<T> type implementation
- **Collections**: Dynamic arrays, hash maps, linked lists
- **Async runtime**: Futures, async/await, task scheduling

### Why Static Linking?

- **Simplicity**: No shared library version conflicts
- **Portability**: Single executable, no runtime dependencies
- **Performance**: Better inlining and link-time optimization
- **Control**: Exact runtime behavior known at compile time

---

## Architecture

```
┌───────────────────────────────────────────────────────────────┐
│                   libaria_runtime.a                           │
├───────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │  6-Stream I/O Module                                    │ │
│  │  - stdin (FD 0), stdout (FD 1), stderr (FD 2)          │ │
│  │  - stddbg (FD 3), stddati (FD 4), stddato (FD 5)       │ │
│  │  - Buffering, line reading, binary I/O                 │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │  Memory Allocators                                      │ │
│  │  - Arena: Fast bump allocation                          │ │
│  │  - Pool: Fixed-size object pools                        │ │
│  │  - Slab: Kernel-style slab allocator                    │ │
│  │  - Wild: malloc/realloc/free wrapper                    │ │
│  │  - GC: Mark-and-sweep garbage collector                 │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │  Process Management                                     │ │
│  │  - aria_spawn: Create child processes                   │ │
│  │  - aria_wait: Wait for process termination              │ │
│  │  - aria_kill: Send signals                              │ │
│  │  - 6-stream pipe setup for children                     │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │  Error Handling (Result<T>)                             │ │
│  │  - aria_result_ok: Create success result                │ │
│  │  - aria_result_err: Create error result                 │ │
│  │  - aria_result_free: Clean up result                    │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │  Collections                                            │ │
│  │  - Vector: Dynamic arrays                               │ │
│  │  - HashMap: Hash tables                                 │ │
│  │  - LinkedList: Doubly-linked lists                      │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │  Async Runtime                                          │ │
│  │  - Task scheduler (M:N threading)                       │ │
│  │  - Futures, async/await support                         │ │
│  │  - Event loop (epoll/kqueue)                            │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │  Platform Abstraction Layer                             │ │
│  │  - Linux: POSIX syscalls, epoll                         │ │
│  │  - Windows: Win32 API, IOCP                             │ │
│  │  - macOS: POSIX + kqueue                                │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
└───────────────────────────────────────────────────────────────┘
```

---

## Module Breakdown

### Header Files

Located in `/usr/local/include/aria/runtime/`:

| Header | Purpose | Key Types/Functions |
|--------|---------|---------------------|
| `io.h` | Core I/O, Result<T> | `AriaResult`, `aria_read_file`, `aria_write_file` |
| `streams.h` | 6-stream handles | `AriaTextStream`, `aria_stdin_read_line` |
| `allocators.h` | Allocator registry | Allocator function pointers |
| `arena.h` | Arena allocator | `AriaArena`, `arena_new`, `arena_alloc` |
| `pool.h` | Pool allocator | `AriaPool`, `pool_new`, `pool_alloc` |
| `slab.h` | Slab allocator | `AriaSlab`, `slab_new`, `slab_alloc` |
| `wild.h` | Wild allocator | `aria_alloc`, `aria_realloc`, `aria_free` |
| `gc.h` | Garbage collector | `gc_alloc`, `gc_collect`, `gc_enable` |
| `process.h` | Process management | `AriaProcessInfo`, `aria_spawn`, `aria_wait` |
| `collections.h` | Data structures | `AriaVector`, `AriaHashMap`, `AriaLinkedList` |
| `async.h` | Async runtime | `AriaFuture`, `aria_async`, `aria_await` |

### Source Organization

```
src/runtime/
├── io/
│   ├── io.c              (File I/O, Result<T>)
│   ├── streams.c         (6-stream handles)
│   └── buffering.c       (Stream buffering logic)
├── allocators/
│   ├── arena.c           (Arena allocator)
│   ├── pool.c            (Pool allocator)
│   ├── slab.c            (Slab allocator)
│   ├── wild.c            (malloc wrapper)
│   └── gc.c              (Mark-and-sweep GC)
├── process/
│   ├── spawn_linux.c     (Linux fork/exec)
│   ├── spawn_windows.c   (Windows CreateProcess)
│   └── wait.c            (Process waiting)
├── collections/
│   ├── vector.c          (Dynamic arrays)
│   ├── hashmap.c         (Hash tables)
│   └── list.c            (Linked lists)
├── async/
│   ├── scheduler.c       (Task scheduler)
│   ├── futures.c         (Future implementation)
│   └── event_loop.c      (epoll/kqueue/IOCP)
└── init.c                (Runtime init/shutdown)
```

---

## 6-Stream I/O System

### Stream Handles

The runtime maintains global handles for 6 streams:

```c
// Global state (internal)
static int io_stdin_fd = 0;
static int io_stdout_fd = 1;
static int io_stderr_fd = 2;
static int io_stddbg_fd = 3;
static int io_stddati_fd = 4;
static int io_stddato_fd = 5;
```

### Stream Types

```c
typedef struct {
    int fd;
    // Internal buffering state
    char* buffer;
    size_t buffer_size;
    size_t buffer_pos;
} AriaTextStream;

typedef struct {
    int fd;
    // No buffering for binary streams
} AriaBinaryStream;
```

### Stream Getters

```c
AriaTextStream* aria_stdin(void);
AriaTextStream* aria_stdout(void);
AriaTextStream* aria_stderr(void);
AriaTextStream* aria_stddbg(void);
AriaBinaryStream* aria_stddati(void);
AriaBinaryStream* aria_stddato(void);
```

### Stream Operations

#### Text Stream API

```c
// Read a line from text stream (blocks until \n or EOF)
AriaResult* aria_stdin_read_line(void);

// Write string to text stream
AriaResult* aria_stdout_write(const char* str, size_t len);
AriaResult* aria_stderr_write(const char* str, size_t len);
AriaResult* aria_stddbg_write(const char* str, size_t len);
```

#### Binary Stream API

```c
// Read bytes from binary stream
AriaResult* aria_stddati_read(uint8_t* buffer, size_t count);

// Write bytes to binary stream
AriaResult* aria_stddato_write(const uint8_t* data, size_t count);
```

### Initialization

**When**: `__attribute__((constructor))` before `main()`

**What it does**:
```c
void aria_runtime_init(void) {
    // Verify FDs 0-5 are open (or open /dev/null as fallback)
    // Set up buffering for text streams
    // Initialize other subsystems (allocators, async runtime)
}
```

### Shutdown

**When**: `__attribute__((destructor))` after `main()` returns

**What it does**:
```c
void aria_runtime_shutdown(void) {
    // Flush all stream buffers
    // Close FDs (if owned by runtime)
    // Clean up allocators
    // Shut down async runtime
}
```

---

## Memory Allocators

### Philosophy: Explicit Memory Models

Aria provides **multiple allocators** for different use cases. User code **explicitly chooses** which allocator to use.

### Arena Allocator

**Use Case**: Bulk allocation, deallocate all at once

**Characteristics**:
- **Fast**: Bump-pointer allocation (just increment pointer)
- **No fragmentation**: Sequential allocation
- **No individual free**: Free entire arena at once

**API**:
```c
AriaArena* arena_new(size_t initial_size);
void* arena_alloc(AriaArena* arena, size_t size);
void arena_reset(AriaArena* arena);    // Clear all allocations
void arena_free(AriaArena* arena);     // Destroy arena
```

**Example Use**:
```aria
arena:myarena = arena_new(4096);
wild byte*:data1 = arena_alloc(myarena, 100);
wild byte*:data2 = arena_alloc(myarena, 200);
// ... use data ...
arena_reset(myarena);  // Free all at once
```

**Implementation**:
```c
struct AriaArena {
    uint8_t* base;      // Start of memory region
    size_t size;        // Total size
    size_t offset;      // Current allocation offset
};

void* arena_alloc(AriaArena* arena, size_t size) {
    if (arena->offset + size > arena->size) {
        return NULL;  // Out of memory
    }
    void* ptr = arena->base + arena->offset;
    arena->offset += size;
    return ptr;
}
```

---

### Pool Allocator

**Use Case**: Fixed-size object allocation (e.g., nodes in a tree)

**Characteristics**:
- **Fast allocation/deallocation**: O(1) using free list
- **No fragmentation**: All objects same size
- **Memory reuse**: Freed objects go back to free list

**API**:
```c
AriaPool* pool_new(size_t object_size, size_t pool_size);
void* pool_alloc(AriaPool* pool);
void pool_free(AriaPool* pool, void* ptr);
void pool_destroy(AriaPool* pool);
```

**Example Use**:
```aria
struct:Node = { int64:val; wild Node*:next; }
pool:node_pool = pool_new(sizeof(Node), 100);

wild Node*:n1 = pool_alloc(node_pool);
wild Node*:n2 = pool_alloc(node_pool);

pool_free(node_pool, n1);  // Return to free list
pool_free(node_pool, n2);
```

**Implementation**:
```c
struct AriaPool {
    void* base;           // Start of pool
    size_t object_size;   // Size of each object
    size_t capacity;      // Max objects
    void** free_list;     // Free list head
    size_t free_count;    // Number of free objects
};

void* pool_alloc(AriaPool* pool) {
    if (pool->free_count == 0) return NULL;
    void* ptr = pool->free_list[--pool->free_count];
    return ptr;
}

void pool_free(AriaPool* pool, void* ptr) {
    pool->free_list[pool->free_count++] = ptr;
}
```

---

### Slab Allocator

**Use Case**: Kernel-style caching of frequently allocated objects

**Characteristics**:
- **Fast**: Pre-allocated slabs of memory
- **Cache-friendly**: Objects aligned for cache lines
- **Object reuse**: Freed objects returned to slab

**API**:
```c
AriaSlab* slab_new(size_t object_size);
void* slab_alloc(AriaSlab* slab);
void slab_free(AriaSlab* slab, void* ptr);
void slab_destroy(AriaSlab* slab);
```

---

### Wild Allocator

**Use Case**: General-purpose heap allocation (like malloc)

**Characteristics**:
- **Flexible**: Variable-size allocations
- **Realloc support**: Resize allocations
- **Standard semantics**: Drop-in replacement for malloc/free

**API**:
```c
void* aria_alloc(size_t size);
void* aria_realloc(void* ptr, size_t new_size);
void aria_free(void* ptr);
```

**Example Use**:
```aria
wild byte*:buffer = aria_alloc(1024);
buffer = aria_realloc(buffer, 2048);  // Resize
aria_free(buffer);
```

**Implementation**: Wraps system malloc/free (or custom allocator if needed)

---

### Garbage Collector (GC)

**Use Case**: Automatic memory management (opt-in)

**Characteristics**:
- **Mark-and-sweep**: Traces reachable objects, frees unreachable
- **Conservative**: Treats anything that looks like a pointer as a pointer
- **Opt-in**: Must explicitly enable and use GC allocator

**API**:
```c
void gc_enable(void);
void gc_disable(void);
void* gc_alloc(size_t size);
void gc_collect(void);  // Force collection
```

**Example Use**:
```aria
gc_enable();
wild byte*:data = gc_alloc(1024);
// ... use data ...
// No explicit free, GC will collect when unreachable
gc_collect();  // Optional: force collection now
```

**When to use**:
- Prototyping (don't worry about memory management)
- Long-lived processes with complex object graphs
- Scripts and utilities

**When NOT to use**:
- Real-time systems (GC pauses)
- Performance-critical code (overhead)
- Embedded systems (limited memory)

---

## Process Management

### Spawning Processes

**Purpose**: Create child processes with 6-stream I/O topology

**API**:
```c
typedef struct {
    int pid;           // Process ID (Linux) or HANDLE (Windows)
    int stdin_fd;      // Write to child's stdin
    int stdout_fd;     // Read from child's stdout
    int stderr_fd;     // Read from child's stderr
    int stddbg_fd;     // Read from child's stddbg
    int stddati_fd;    // Write to child's stddati
    int stddato_fd;    // Read from child's stddato
} AriaProcessInfo;

AriaResult* aria_spawn(const char* executable, char* const argv[], char* const envp[]);
```

**Linux Implementation** (fork + exec):
```c
AriaResult* aria_spawn(const char* executable, char* const argv[], char* const envp[]) {
    // Create 6 pipes
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    int stddbg_pipe[2], stddati_pipe[2], stddato_pipe[2];
    
    pipe2(stdin_pipe, O_CLOEXEC);
    pipe2(stdout_pipe, O_CLOEXEC);
    pipe2(stderr_pipe, O_CLOEXEC);
    pipe2(stddbg_pipe, O_CLOEXEC);
    pipe2(stddati_pipe, O_CLOEXEC);
    pipe2(stddato_pipe, O_CLOEXEC);
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        dup2(stdin_pipe[0], 0);   // stdin
        dup2(stdout_pipe[1], 1);  // stdout
        dup2(stderr_pipe[1], 2);  // stderr
        dup2(stddbg_pipe[1], 3);  // stddbg
        dup2(stddati_pipe[0], 4); // stddati
        dup2(stddato_pipe[1], 5); // stddato
        
        // Close all pipe ends (now duplicated)
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        close(stddbg_pipe[0]); close(stddbg_pipe[1]);
        close(stddati_pipe[0]); close(stddati_pipe[1]);
        close(stddato_pipe[0]); close(stddato_pipe[1]);
        
        execve(executable, argv, envp);
        _exit(127);  // execve failed
    } else {
        // Parent process
        close(stdin_pipe[0]);   // Close child ends
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        close(stddbg_pipe[1]);
        close(stddati_pipe[0]);
        close(stddato_pipe[1]);
        
        AriaProcessInfo* info = malloc(sizeof(AriaProcessInfo));
        info->pid = pid;
        info->stdin_fd = stdin_pipe[1];
        info->stdout_fd = stdout_pipe[0];
        info->stderr_fd = stderr_pipe[0];
        info->stddbg_fd = stddbg_pipe[0];
        info->stddati_fd = stddati_pipe[1];
        info->stddato_fd = stddato_pipe[0];
        
        return aria_result_ok(info, sizeof(AriaProcessInfo));
    }
}
```

**Windows Implementation**: Uses `CreateProcess` with `STARTUPINFOEX` and custom protocol (see INTERFACES.md)

---

### Waiting for Processes

**API**:
```c
AriaResult* aria_wait(int pid, int* exit_code);
```

**Linux**:
```c
AriaResult* aria_wait(int pid, int* exit_code) {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return aria_result_err(strerror(errno));
    }
    *exit_code = WEXITSTATUS(status);
    return aria_result_ok(NULL, 0);
}
```

---

### Signaling Processes

**API**:
```c
AriaResult* aria_kill(int pid, int signal);
```

**Example**:
```aria
aria_kill(child_pid, 9);  // SIGKILL (Linux)
```

---

## Error Handling

### Result<T> Implementation

**Structure**:
```c
typedef struct {
    char* err;       // Error message (NULL if success)
    void* val;       // Success value (NULL if error)
    size_t val_size; // Size of value (for copying)
} AriaResult;
```

### Constructor Functions

```c
// Create success result
AriaResult* aria_result_ok(void* value, size_t size) {
    AriaResult* r = malloc(sizeof(AriaResult));
    r->err = NULL;
    r->val = value;  // Ownership transfer? Or copy?
    r->val_size = size;
    return r;
}

// Create error result
AriaResult* aria_result_err(const char* error_message) {
    AriaResult* r = malloc(sizeof(AriaResult));
    r->err = strdup(error_message);
    r->val = NULL;
    r->val_size = 0;
    return r;
}

// Free result
void aria_result_free(AriaResult* r) {
    if (r->err) free(r->err);
    if (r->val) free(r->val);  // Only if owns value!
    free(r);
}
```

### Usage Pattern

```c
AriaResult* result = aria_read_file("file.txt");
if (result->err != NULL) {
    // Error case
    fprintf(stderr, "Error: %s\n", result->err);
    aria_result_free(result);
    return 1;
}

// Success case
char* content = (char*)result->val;
printf("Content: %s\n", content);
aria_result_free(result);
```

---

## Collections

### Vector (Dynamic Array)

**API**:
```c
typedef struct {
    void* data;
    size_t size;
    size_t capacity;
    size_t element_size;
} AriaVector;

AriaVector* aria_vector_new(size_t element_size);
void aria_vector_push(AriaVector* vec, void* element);
void* aria_vector_get(AriaVector* vec, size_t index);
void aria_vector_free(AriaVector* vec);
```

---

### HashMap (Hash Table)

**API**:
```c
typedef struct {
    // Internal hash table implementation
} AriaHashMap;

AriaHashMap* aria_hashmap_new(void);
void aria_hashmap_insert(AriaHashMap* map, const char* key, void* value);
void* aria_hashmap_get(AriaHashMap* map, const char* key);
void aria_hashmap_remove(AriaHashMap* map, const char* key);
void aria_hashmap_free(AriaHashMap* map);
```

---

### LinkedList (Doubly-Linked List)

**API**:
```c
typedef struct AriaListNode {
    void* data;
    struct AriaListNode* prev;
    struct AriaListNode* next;
} AriaListNode;

typedef struct {
    AriaListNode* head;
    AriaListNode* tail;
    size_t size;
} AriaLinkedList;

AriaLinkedList* aria_list_new(void);
void aria_list_push_back(AriaLinkedList* list, void* data);
void aria_list_push_front(AriaLinkedList* list, void* data);
void* aria_list_pop_back(AriaLinkedList* list);
void* aria_list_pop_front(AriaLinkedList* list);
void aria_list_free(AriaLinkedList* list);
```

---

## Async Runtime

### Task Scheduler

**Purpose**: M:N threading - M user tasks on N OS threads

**API**:
```c
typedef struct AriaFuture AriaFuture;

AriaFuture* aria_async(void (*task_fn)(void*), void* arg);
void* aria_await(AriaFuture* future);
```

**Example Use** (conceptual Aria code):
```aria
async func:fetch_data = result<string>(url: string) {
    // This runs asynchronously
    result<string>:data = http.get(url);
    pass(data);
}

func:main = int64() {
    future<result<string>>:f = async fetch_data("https://example.com");
    // ... do other work ...
    result<string>:data = await f;
    io.stdout.write(data.ok());
    pass(0);
}
```

### Event Loop

**Platform-Specific**:
- **Linux**: epoll
- **macOS**: kqueue
- **Windows**: IOCP (I/O Completion Ports)

**Purpose**: Efficient I/O multiplexing for async operations

---

## Platform Support

### Linux

- **FD 0-5**: Native support via POSIX
- **Process spawning**: `fork()` + `execve()`
- **Async I/O**: `epoll`
- **Dynamic linking**: Not used (static linking only)

### Windows

- **FD 3-5**: Custom bootstrap protocol (see INTERFACES.md)
- **Process spawning**: `CreateProcess` with `STARTUPINFOEX`
- **Async I/O**: IOCP
- **Path handling**: Windows path normalization (`\` → `/`)

### macOS

- **FD 0-5**: POSIX support
- **Process spawning**: `fork()` + `execve()`
- **Async I/O**: `kqueue`

---

## Related Components

- **[Aria Compiler](ARIA_COMPILER.md)**: Generates calls to runtime functions
- **[aria_shell](ARIA_SHELL.md)**: Uses runtime for process spawning
- **[AriaBuild](ARIABUILD.md)**: Links applications with runtime
- **[AriaX](ARIAX.md)**: Distributes runtime library as system package

---

**Document Version**: 1.0  
**Last Updated**: December 22, 2025
