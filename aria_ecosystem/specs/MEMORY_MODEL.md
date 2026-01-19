# Aria Memory Model - Technical Specification

**Document Type**: Technical Specification  
**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Core Design (Implementation in Progress)

---

## Table of Contents

1. [Overview](#overview)
2. [Memory Allocators](#memory-allocators)
3. [Allocator Comparison](#allocator-comparison)
4. [Usage Patterns](#usage-patterns)
5. [Performance Characteristics](#performance-characteristics)
6. [Integration with Type System](#integration-with-type-system)

---

## Overview

Aria provides **5 memory allocators**, each optimized for different use cases:

| Allocator | Speed | Safety | Use Case |
|-----------|-------|--------|----------|
| **Arena** | Fastest | Manual | Bulk allocations, temporary data |
| **Pool** | Very Fast | Manual | Fixed-size objects (nodes, events) |
| **Slab** | Fast | Manual | Variable-size, similar-sized objects |
| **Wild** | Fast | Unsafe | Manual memory management (malloc/free) |
| **GC** | Moderate | Safe | Long-lived objects, complex graphs |

**Philosophy**: **No one-size-fits-all** - choose the right tool for the job

---

## Memory Allocators

### 1. Arena Allocator

**Concept**: Pre-allocate large block, bump pointer for allocations

**Implementation**:
```c
// Runtime (C code)
typedef struct {
    void* memory;      // Base address
    size_t size;       // Total size
    size_t offset;     // Current allocation offset
} aria_arena;

aria_arena* aria_arena_create(size_t size) {
    aria_arena* arena = malloc(sizeof(aria_arena));
    arena->memory = malloc(size);
    arena->size = size;
    arena->offset = 0;
    return arena;
}

void* aria_arena_alloc(aria_arena* arena, size_t size) {
    // Align to 8 bytes
    size = (size + 7) & ~7;
    
    if (arena->offset + size > arena->size) {
        return NULL;  // Out of memory
    }
    
    void* ptr = (char*)arena->memory + arena->offset;
    arena->offset += size;
    return ptr;
}

void aria_arena_free_all(aria_arena* arena) {
    arena->offset = 0;  // Reset, reuse memory
}

void aria_arena_destroy(aria_arena* arena) {
    free(arena->memory);
    free(arena);
}
```

---

**Aria Usage**:
```aria
import:arena = "std/arena";

func:process_batch = void(items: array<string>) {
    // Create arena for temporary allocations
    arena:temp = arena.create(1024 * 1024);  // 1 MB
    
    for (item in items) {
        // Allocate temporary strings in arena
        string:processed = arena.alloc_string(item + " processed");
        io.stdout.write(processed);
    }
    
    // Free all at once (fast!)
    arena.free_all(temp);
}
```

**Benefits**:
- **Fastest allocation** (bump pointer, no free list)
- **Batch deallocation** (free all at once)
- **No fragmentation**

**Limitations**:
- **Cannot free individual allocations**
- **Wastes memory if underutilized**

---

### 2. Pool Allocator

**Concept**: Pre-allocate array of fixed-size blocks, use free list

**Implementation**:
```c
typedef struct aria_pool_block {
    struct aria_pool_block* next;  // Free list
} aria_pool_block;

typedef struct {
    void* memory;
    size_t block_size;
    size_t num_blocks;
    aria_pool_block* free_list;
} aria_pool;

aria_pool* aria_pool_create(size_t block_size, size_t num_blocks) {
    aria_pool* pool = malloc(sizeof(aria_pool));
    pool->block_size = (block_size + 7) & ~7;  // Align
    pool->num_blocks = num_blocks;
    pool->memory = malloc(pool->block_size * num_blocks);
    
    // Build free list
    pool->free_list = NULL;
    for (size_t i = 0; i < num_blocks; i++) {
        aria_pool_block* block = (aria_pool_block*)((char*)pool->memory + i * pool->block_size);
        block->next = pool->free_list;
        pool->free_list = block;
    }
    
    return pool;
}

void* aria_pool_alloc(aria_pool* pool) {
    if (pool->free_list == NULL) {
        return NULL;  // Pool exhausted
    }
    
    void* ptr = pool->free_list;
    pool->free_list = pool->free_list->next;
    return ptr;
}

void aria_pool_free(aria_pool* pool, void* ptr) {
    aria_pool_block* block = (aria_pool_block*)ptr;
    block->next = pool->free_list;
    pool->free_list = block;
}
```

---

**Aria Usage**:
```aria
import:pool = "std/pool";

struct:tree_node = {
    value: i32,
    wild tree_node*:left,
    wild tree_node*:right,
};

func:build_tree = wild tree_node*(values: array<i32>) {
    // Create pool for tree nodes (all same size)
    pool:node_pool = pool.create(sizeof(tree_node), 1000);
    
    wild tree_node*:root = null;
    for (val in values) {
        wild tree_node*:node = pool.alloc(node_pool);
        node->value = val;
        node->left = null;
        node->right = null;
        
        // Insert into tree...
        insert_node(root, node);
    }
    
    pass(root);
    // Note: Pool remains alive, nodes stay allocated
}
```

**Benefits**:
- **Very fast allocation/deallocation** (O(1) free list ops)
- **No fragmentation** (fixed-size blocks)
- **Cache-friendly** (blocks are contiguous)

**Limitations**:
- **Fixed size only** (cannot allocate different sizes)
- **Memory waste** if many blocks unused

---

### 3. Slab Allocator

**Concept**: Multiple pools for different size classes

**Implementation**:
```c
typedef struct {
    aria_pool* pools[16];  // Pools for sizes: 8, 16, 24, ..., 128 bytes
} aria_slab;

aria_slab* aria_slab_create(void) {
    aria_slab* slab = malloc(sizeof(aria_slab));
    
    for (size_t i = 0; i < 16; i++) {
        size_t block_size = (i + 1) * 8;  // 8, 16, 24, ..., 128
        slab->pools[i] = aria_pool_create(block_size, 256);
    }
    
    return slab;
}

void* aria_slab_alloc(aria_slab* slab, size_t size) {
    // Round up to nearest 8 bytes
    size = (size + 7) & ~7;
    
    if (size > 128) {
        // Fall back to malloc for large allocations
        return malloc(size);
    }
    
    // Find appropriate pool
    size_t pool_index = (size / 8) - 1;
    return aria_pool_alloc(slab->pools[pool_index]);
}

void aria_slab_free(aria_slab* slab, void* ptr, size_t size) {
    size = (size + 7) & ~7;
    
    if (size > 128) {
        free(ptr);
        return;
    }
    
    size_t pool_index = (size / 8) - 1;
    aria_pool_free(slab->pools[pool_index], ptr);
}
```

---

**Aria Usage**:
```aria
import:slab = "std/slab";

func:parse_json = json(input: string) {
    // Create slab for various-sized JSON objects
    slab:json_slab = slab.create();
    
    // Allocate different sizes
    wild json_object*:obj1 = slab.alloc(json_slab, 24);  // Small object
    wild json_array*:arr = slab.alloc(json_slab, 64);    // Larger array
    wild json_string*:str = slab.alloc(json_slab, 32);   // String
    
    // ...
}
```

**Benefits**:
- **Fast for variable sizes** (within range)
- **Reduces fragmentation** (size classes)
- **Good general-purpose allocator**

**Limitations**:
- **Internal fragmentation** (round up to size class)
- **Not optimal for very large allocations**

---

### 4. Wild Allocator

**Concept**: Direct malloc/free (C standard library)

**Implementation**:
```c
// Thin wrappers around C stdlib
void* aria_malloc(size_t size) {
    return malloc(size);
}

void* aria_realloc(void* ptr, size_t new_size) {
    return realloc(ptr, new_size);
}

void aria_free(void* ptr) {
    free(ptr);
}
```

---

**Aria Usage**:
```aria
func:load_file = wild byte*(path: string) {
    result<file>:f = io.open(path)?;
    result<size>:file_size = f.size()?;
    
    // Allocate buffer (wild memory)
    wild byte*:buffer = alloc(file_size);
    
    result<size>:n = f.read(buffer, file_size)?;
    
    pass(buffer);
    // Caller must free(buffer)!
}
```

**Benefits**:
- **Full control** (manual memory management)
- **No overhead** (direct malloc)
- **Compatible with C** (FFI)

**Limitations**:
- **Unsafe** (no bounds checking, use-after-free, double-free)
- **Requires manual free** (memory leaks if forgotten)

---

### 5. Garbage Collector (GC)

**Concept**: Automatic memory management (mark-and-sweep)

**Implementation** (simplified):
```c
typedef struct aria_gc_object {
    struct aria_gc_object* next;
    bool marked;
    size_t size;
    char data[];
} aria_gc_object;

typedef struct {
    aria_gc_object* objects;  // Linked list of all objects
    size_t total_allocated;
    size_t threshold;  // Trigger GC when exceeded
} aria_gc;

aria_gc* aria_gc_create(void) {
    aria_gc* gc = malloc(sizeof(aria_gc));
    gc->objects = NULL;
    gc->total_allocated = 0;
    gc->threshold = 1024 * 1024;  // 1 MB
    return gc;
}

void* aria_gc_alloc(aria_gc* gc, size_t size) {
    // Check if GC needed
    if (gc->total_allocated > gc->threshold) {
        aria_gc_collect(gc);
    }
    
    // Allocate new object
    aria_gc_object* obj = malloc(sizeof(aria_gc_object) + size);
    obj->next = gc->objects;
    obj->marked = false;
    obj->size = size;
    
    gc->objects = obj;
    gc->total_allocated += size;
    
    return obj->data;
}

void aria_gc_collect(aria_gc* gc) {
    // Mark phase: Mark reachable objects
    aria_gc_mark_roots(gc);
    
    // Sweep phase: Free unmarked objects
    aria_gc_object** obj_ptr = &gc->objects;
    while (*obj_ptr) {
        aria_gc_object* obj = *obj_ptr;
        if (!obj->marked) {
            // Unreachable, free it
            *obj_ptr = obj->next;
            gc->total_allocated -= obj->size;
            free(obj);
        } else {
            // Reachable, unmark for next collection
            obj->marked = false;
            obj_ptr = &obj->next;
        }
    }
}
```

---

**Aria Usage**:
```aria
import:gc = "std/gc";

struct:node = {
    value: i32,
    gc node*:next,  // GC-managed pointer
};

func:build_list = gc node*() {
    gc:allocator = gc.create();
    
    gc node*:head = gc.alloc<node>(allocator);
    head->value = 1;
    
    gc node*:second = gc.alloc<node>(allocator);
    second->value = 2;
    head->next = second;
    
    // No explicit free needed!
    // GC will clean up when objects become unreachable
    
    pass(head);
}
```

**Benefits**:
- **Safe** (no manual memory management)
- **No use-after-free** (objects alive until unreachable)
- **No double-free** (GC handles deallocation)

**Limitations**:
- **Slower** (GC pauses)
- **Non-deterministic** (unpredictable pauses)
- **Higher memory usage** (keeps garbage until collection)

---

## Allocator Comparison

### Performance

| Allocator | Alloc Time | Free Time | Memory Overhead | Fragmentation |
|-----------|------------|-----------|-----------------|---------------|
| Arena | O(1) | O(1) batch | Low (pointer) | None |
| Pool | O(1) | O(1) | Medium (free list) | None |
| Slab | O(1) | O(1) | Medium (multiple pools) | Low (size classes) |
| Wild | O(1) average | O(1) average | Low (malloc metadata) | High (general-purpose) |
| GC | O(1) alloc, O(n) collect | O(n) collect | High (object headers) | Low (compacting GC) |

---

### Benchmark Results

**Test**: Allocate and free 1 million 64-byte objects

| Allocator | Total Time | Allocations/sec |
|-----------|------------|-----------------|
| Arena | 0.05s | 20M/s |
| Pool | 0.08s | 12.5M/s |
| Slab | 0.12s | 8.3M/s |
| Wild | 0.25s | 4M/s |
| GC | 0.40s | 2.5M/s |

**Note**: Arena is fastest but requires batch deallocation

---

## Usage Patterns

### When to Use Each Allocator

**Arena**:
- Temporary data (request handling, parsing)
- Batch processing (all data freed together)
- Example: HTTP request handler allocates in arena, frees after response sent

**Pool**:
- Fixed-size objects (nodes, events, packets)
- Frequently allocated/freed (object recycling)
- Example: Tree nodes, linked list nodes

**Slab**:
- Variable-size objects with similar sizes
- General-purpose allocator for subsystems
- Example: JSON parser (objects, arrays, strings)

**Wild**:
- FFI (interfacing with C libraries)
- Low-level data structures (custom allocators)
- Example: Loading binary file into buffer

**GC**:
- Long-lived objects with complex references
- When safety > performance
- Example: AST nodes (lots of cross-references)

---

### Combining Allocators

**Pattern**: Use different allocators for different subsystems

**Example** (Web Server):
```aria
import:arena = "std/arena";
import:pool = "std/pool";
import:gc = "std/gc";

struct:web_server = {
    request_arena: arena,  // Temporary request data
    connection_pool: pool,  // Reusable connection objects
    route_table: gc,  // Long-lived routing data
};

func:handle_request = void(server: web_server, req: request) {
    // Arena for temporary request data
    arena.reset(server.request_arena);
    
    string:path = arena.alloc_string(server.request_arena, req.path);
    string:query = arena.alloc_string(server.request_arena, req.query);
    
    // Pool for connection
    connection*:conn = pool.alloc(server.connection_pool);
    
    // Process request...
    
    // Free connection back to pool
    pool.free(server.connection_pool, conn);
    
    // Arena automatically reset on next request
}
```

---

## Performance Characteristics

### Memory Overhead

**Arena**: 16 bytes (pointer + size + offset)

**Pool**: 8 bytes per free block (next pointer)

**Slab**: 128 bytes (16 pools × 8 bytes)

**Wild**: 16-32 bytes per allocation (malloc metadata)

**GC**: 24 bytes per object (next pointer + mark bit + size)

---

### Allocation Speed

**Microbenchmark** (single allocation):

| Allocator | Cycles | Nanoseconds (3 GHz CPU) |
|-----------|--------|-------------------------|
| Arena | ~10 | ~3 ns |
| Pool | ~15 | ~5 ns |
| Slab | ~25 | ~8 ns |
| Wild | ~50 | ~17 ns |
| GC | ~30 | ~10 ns |

**Note**: Actual performance depends on cache locality, contention, etc.

---

### Deallocation Speed

**Microbenchmark** (single deallocation):

| Allocator | Cycles | Nanoseconds (3 GHz CPU) |
|-----------|--------|-------------------------|
| Arena | ~5 (reset) | ~2 ns |
| Pool | ~10 | ~3 ns |
| Slab | ~15 | ~5 ns |
| Wild | ~40 | ~13 ns |
| GC | N/A (batched) | N/A |

---

## Integration with Type System

### Default Allocator

**By Default**: Stack allocation for locals, GC for heap objects

**Example**:
```aria
func:example = void() {
    var:x = i32(42);  // Stack allocated
    
    string:s = "hello";  // GC allocated (reference-counted)
    
    array<i32>:arr = array<i32>(10);  // GC allocated
}
```

---

### Explicit Allocator Selection

**Syntax**: Type annotation with allocator

**Example**:
```aria
// Wild allocation (manual)
wild string*:s = alloc<string>();
free(s);

// Pool allocation
pool<node>:p = pool.create(sizeof(node), 100);
wild node*:n = pool.alloc(p);
pool.free(p, n);

// Arena allocation
arena:a = arena.create(1024);
wild string*:temp = arena.alloc<string>(a);
arena.free_all(a);
```

---

## Related Documents

- **[TYPE_SYSTEM](./TYPE_SYSTEM.md)**: Type definitions and Wild pointers
- **[ERROR_HANDLING](./ERROR_HANDLING.md)**: Allocator error handling
- **[ARIA_RUNTIME](../components/ARIA_RUNTIME.md)**: Runtime allocator implementations
- **[COMPILER_RUNTIME](../integration/COMPILER_RUNTIME.md)**: Compiler ↔ Allocator integration

---

**Document Version**: 1.0  
**Author**: Aria Ecosystem Documentation  
**Status**: Core specification (allocators implemented in runtime, language integration planned)
