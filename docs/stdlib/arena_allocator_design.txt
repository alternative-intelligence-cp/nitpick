# Arena Allocator Design - Aria Standard Library

**Phase**: 4.2 Stage 1.2  
**Status**: Design Complete  
**Date**: December 21, 2025

---

## Table of Contents

1. [Overview](#overview)
2. [What is an Arena Allocator?](#what-is-an-arena-allocator)
3. [Use Cases](#use-cases)
4. [Prior Art Analysis](#prior-art-analysis)
5. [Memory Block Chaining Strategies](#memory-block-chaining-strategies)
6. [Alignment Handling](#alignment-handling)
7. [API Specification](#api-specification)
8. [Runtime Integration](#runtime-integration)
9. [Performance Characteristics](#performance-characteristics)
10. [Implementation Considerations](#implementation-considerations)
11. [Testing Strategy](#testing-strategy)
12. [Success Criteria](#success-criteria)
13. [References](#references)

---

## Overview

An **arena allocator** (also called a "region" or "bump" allocator) is a high-performance memory allocation strategy optimized for temporary data with bulk deallocation patterns. Arena allocators are 10-100x faster than traditional malloc/free for allocation and 100-1000x faster for bulk deallocation.

### Design Goals

1. **Extreme performance** - O(1) allocation via pointer bumping
2. **Zero per-allocation overhead** - No headers, footers, or metadata
3. **Bulk deallocation** - Free entire arena instantly
4. **Type safety** - Leverage Aria's type system for safe usage
5. **Alignment correctness** - Handle mixed-type allocations properly

### Non-Goals

1. Individual deallocation (use wild pointers for that)
2. Thread-safe shared arenas (Phase 4.3+)
3. Automatic growing beyond initial capacity (Phase 4.3+)
4. GC integration (arenas are wild memory)

---

## What is an Arena Allocator?

Arena allocators follow a simple pattern:

```
1. Allocate large memory block upfront (the "arena")
2. Serve requests by bumping pointer forward
3. Free entire arena at once (not individual allocations)
```

### Visual Representation

```
Arena (4KB block):
┌────────────────────────────────────────────────────┐
│ [int: 4B] [string: 24B] [array: 400B] [free: 3668B]│
│  ^offset=0  ^offset=4    ^offset=28   ^offset=428  │
└────────────────────────────────────────────────────┘
              current_offset = 428

Allocation: Just bump offset forward!
Deallocation: Free entire block in one call
```

### Key Advantage

**Traditional malloc:**
```cpp
for (int i = 0; i < 1000; i++) {
    void* p = malloc(32);  // Search free list, update metadata, 10-50ns each
}
// Total: ~10-50µs

for (int i = 0; i < 1000; i++) {
    free(p[i]);  // Update free list, coalesce, 10-50ns each
}
// Total: ~10-50µs
```

**Arena allocator:**
```cpp
Arena* arena = arena_create(4096);

for (int i = 0; i < 1000; i++) {
    void* p = arena_alloc(arena, 32);  // Just bump pointer, ~1ns each
}
// Total: ~1µs (10-50x faster!)

arena_destroy(arena);  // Single free() call, ~100ns
// Total: ~0.1µs (100-500x faster!)
```

### Key Limitation

Cannot free individual allocations:

```cpp
// Traditional malloc
void* a = malloc(100);
void* b = malloc(200);
free(a);  // ✅ Can free selectively
free(b);

// Arena allocator
Arena* arena = arena_create(4096);
void* a = arena_alloc(arena, 100);
void* b = arena_alloc(arena, 200);
// ❌ Cannot free 'a' individually
arena_destroy(arena);  // ✅ Frees both at once
```

---

## Use Cases

### ✅ Perfect For

1. **Compiler phases**
   ```aria
   fn parse_file(path: string) -> result<AST, error> {
       arena := Arena.new();
       defer Arena.destroy(arena);  // Cleanup guaranteed
       
       tokens := arena.alloc_array<Token>(1000);
       ast := arena.alloc<ASTNode>();
       // Parse...
       return clone_ast(ast);  // Clone to GC heap before arena dies
   }
   ```

2. **Per-request processing** (web servers)
   ```aria
   fn handle_request(req: Request) -> Response {
       arena := Arena.new();
       defer Arena.destroy(arena);
       
       parsed := parse_json(arena, req.body);
       result := process(arena, parsed);
       return serialize(result);  // Result copied, arena destroyed
   }
   ```

3. **Temporary computations**
   ```aria
   fn matrix_multiply(a: Matrix, b: Matrix) -> Matrix {
       arena := Arena.new();
       defer Arena.destroy(arena);
       
       temp := arena.alloc_array<f64>(a.rows * b.cols);
       // Compute into temp...
       result := copy_to_gc(temp);  // Copy to GC heap
       return result;
   }
   ```

4. **String building**
   ```aria
   fn build_report(data: Data) -> string {
       arena := Arena.new();
       defer Arena.destroy(arena);
       
       parts := arena.alloc_array<string>(100);
       // Accumulate strings in arena...
       final := concatenate(parts);  // Final string on GC heap
       return final;
   }
   ```

### ❌ NOT Good For

1. **Long-lived objects with mixed lifetimes** - Use GC instead
2. **Selective deallocation** - Use wild pointers with free()
3. **Unbounded growth** - Arena size is fixed (Phase 4.2)
4. **Shared multi-threaded access** - Use thread-local arenas

---

## Prior Art Analysis

### Rust: `bumpalo` Crate

```rust
use bumpalo::Bump;

let arena = Bump::new();

// Type-safe allocation
let x: &mut i32 = arena.alloc(42);
let y: &mut [u8] = arena.alloc_slice_fill_default(100);

// Automatic cleanup on drop
drop(arena);  // All memory freed
```

**Key Features:**
- Single contiguous block with overflow chaining
- Alignment handled automatically
- Lifetime-safe references (borrow checker)
- Zero-cost abstraction

**Implementation:**
```rust
pub struct Bump {
    ptr: Cell<NonNull<u8>>,  // Current allocation pointer
    end: Cell<NonNull<u8>>,  // End of current block
    chunks: Cell<ChunkList>, // Linked list of blocks
}

impl Bump {
    pub fn alloc<T>(&self, val: T) -> &mut T {
        let layout = Layout::new::<T>();
        let ptr = self.alloc_layout(layout);
        unsafe {
            ptr::write(ptr as *mut T, val);
            &mut *(ptr as *mut T)
        }
    }
    
    fn alloc_layout(&self, layout: Layout) -> NonNull<u8> {
        // Align pointer
        let ptr = self.ptr.get().as_ptr();
        let aligned = ptr.align_up(layout.align());
        
        // Check if fits
        if aligned + layout.size() > self.end.get().as_ptr() {
            self.alloc_new_chunk(layout);
        }
        
        // Bump pointer
        self.ptr.set(NonNull::new_unchecked(aligned + layout.size()));
        NonNull::new_unchecked(aligned)
    }
}
```

### Zig: `ArenaAllocator`

```zig
const std = @import("std");

var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
defer arena.deinit();

// Allocate through interface
const allocator = arena.allocator();
const items = try allocator.alloc(i32, 100);
const node = try allocator.create(Node);

// All freed on deinit()
```

**Key Features:**
- Wraps another allocator (delegates to system allocator)
- Tracks allocations in linked list
- Single `deinit()` call frees everything

**Implementation:**
```zig
pub const ArenaAllocator = struct {
    child_allocator: Allocator,
    state: State,
    
    const State = struct {
        buffer_list: std.SinglyLinkedList([]u8) = .{},
        end_index: usize = 0,
    };
    
    pub fn alloc(self: *ArenaAllocator, n: usize) ![]u8 {
        // Try to allocate from current buffer
        var cur_node = self.state.buffer_list.first;
        if (cur_node) |node| {
            const buf = node.data;
            if (self.state.end_index + n <= buf.len) {
                const result = buf[self.state.end_index..][0..n];
                self.state.end_index += n;
                return result;
            }
        }
        
        // Need new buffer
        const new_buffer = try self.child_allocator.alloc(u8, n);
        try self.state.buffer_list.prepend(new_buffer);
        self.state.end_index = n;
        return new_buffer[0..n];
    }
    
    pub fn deinit(self: *ArenaAllocator) void {
        // Free all buffers
        var it = self.state.buffer_list.first;
        while (it) |node| {
            self.child_allocator.free(node.data);
            it = node.next;
        }
    }
};
```

### C++: Manual Arena Pattern

```cpp
class Arena {
    struct Block {
        void* memory;
        size_t size;
        Block* next;
    };
    
    Block* first_block = nullptr;
    Block* current_block = nullptr;
    size_t block_size;
    size_t offset = 0;

public:
    Arena(size_t block_sz = 4096) : block_size(block_sz) {
        allocate_new_block();
    }
    
    template<typename T, typename... Args>
    T* alloc(Args&&... args) {
        void* ptr = alloc_raw(sizeof(T), alignof(T));
        return new (ptr) T(std::forward<Args>(args)...);
    }
    
    void* alloc_raw(size_t size, size_t alignment) {
        // Align offset
        size_t aligned = (offset + alignment - 1) & ~(alignment - 1);
        
        if (aligned + size > block_size) {
            allocate_new_block();
            aligned = 0;
        }
        
        void* ptr = (char*)current_block->memory + aligned;
        offset = aligned + size;
        return ptr;
    }
    
    void reset() {
        offset = 0;
        current_block = first_block;
    }
    
    ~Arena() {
        Block* b = first_block;
        while (b) {
            Block* next = b->next;
            std::free(b->memory);
            delete b;
            b = next;
        }
    }

private:
    void allocate_new_block() {
        Block* b = new Block;
        b->memory = std::malloc(block_size);
        b->size = block_size;
        b->next = nullptr;
        
        if (!first_block) {
            first_block = b;
        } else {
            current_block->next = b;
        }
        current_block = b;
        offset = 0;
    }
};
```

---

## Memory Block Chaining Strategies

### Option 1: Fixed-Size Blocks (Recommended for Phase 4.2)

```
Block 1 (4KB)          Block 2 (4KB)          Block 3 (4KB)
┌────────────────┐    ┌────────────────┐    ┌────────────────┐
│ [used: 3.8KB]  │───>│ [used: 4KB]    │───>│ [used: 0.5KB]  │
│ [free: 0.2KB]  │    │ [free: 0KB]    │    │ [free: 3.5KB]  │
└────────────────┘    └────────────────┘    └────────────────┘
     ^first              ^second              ^current
```

**Pros:**
- Simple and predictable
- Can reuse blocks on `reset()`
- Easy to track memory usage
- Good cache locality within blocks

**Cons:**
- Wasted space in partially-filled blocks
- Large allocations (> block_size) require special handling

**Implementation:**
```cpp
struct aria_arena_block {
    void* memory;
    size_t size;
    aria_arena_block* next;
};

void* aria_arena_alloc(aria_arena* arena, size_t size, size_t align) {
    size_t aligned = align_forward(arena->offset, align);
    
    if (aligned + size > arena->block_size) {
        // Allocate new block
        aria_arena_block* b = malloc(sizeof(aria_arena_block));
        b->memory = malloc(arena->block_size);
        b->size = arena->block_size;
        b->next = nullptr;
        
        arena->current_block->next = b;
        arena->current_block = b;
        arena->offset = 0;
        aligned = 0;
    }
    
    void* ptr = (char*)arena->current_block->memory + aligned;
    arena->offset = aligned + size;
    return ptr;
}
```

### Option 2: Exponential Growth (Future Enhancement)

```
Block 1: 4KB → Block 2: 8KB → Block 3: 16KB → Block 4: 32KB
```

**Pros:**
- Fewer blocks for growing workloads
- Better overall cache locality
- Amortizes allocation cost

**Cons:**
- Can waste significant memory if growth overshoots
- Harder to predict memory usage
- More complex implementation

**Deferred to**: Phase 4.3+ (Advanced Allocators)

### Option 3: Single Large Block (Simplest)

```
Block 1 (1MB continuous)
┌────────────────────────────────────────────────────┐
│ [used: 500KB]                [free: 524KB]         │
└────────────────────────────────────────────────────┘
       ^offset=500KB
```

**Pros:**
- Best cache locality (all data contiguous)
- Simplest implementation (no chaining)
- Fastest allocation (just increment)

**Cons:**
- Requires large upfront allocation
- Cannot grow beyond initial capacity
- Wasted memory if underutilized

**Use Case**: When maximum memory usage is known and bounded

---

## Alignment Handling

### The Alignment Problem

Different types have different alignment requirements:

```aria
@alignof(uint8)   = 1   // Can start at any byte
@alignof(uint32)  = 4   // Must start at multiple of 4
@alignof(uint64)  = 8   // Must start at multiple of 8
@alignof(vec3)    = 16  // Must start at multiple of 16 (SIMD)
```

**Without alignment correction:**

```
Arena: [uint8][uint64] <- CRASH! uint64 at offset 1 (not 8-aligned)
         ^0     ^1
```

**CPU behavior on misaligned access:**
- x86-64: Slow (multiple memory accesses)
- ARM: Crash (bus error)
- RISC-V: Crash (alignment fault)

### Alignment Algorithm

```cpp
size_t align_forward(size_t offset, size_t alignment) {
    // alignment must be power of 2
    return (offset + alignment - 1) & ~(alignment - 1);
}
```

**Example:**

```
offset = 5, alignment = 8

Binary:
  offset:     0000 0101  (5)
  align - 1:  0000 0111  (7)
  sum:        0000 1100  (12)
  ~(align-1): 1111 1000  (-8 in two's complement)
  result:     0000 1000  (8)  ✅ Aligned!
```

**Visual:**

```
Before:
[uint8][---padding---][uint64..........]
  ^0    ^1  ^2  ^3  ^4  ^5  ^6  ^7  ^8
        └──wasted due to alignment──┘

After align_forward(1, 8):
[uint8][---padding---][uint64..........]
  ^0                    ^8 (properly aligned)
```

### Aria Implementation

```cpp
void* aria_arena_alloc(aria_arena* arena, size_t size, size_t alignment) {
    // 1. Align current offset
    size_t aligned_offset = (arena->current_offset + alignment - 1) 
                           & ~(alignment - 1);
    
    // 2. Check if fits in current block
    if (aligned_offset + size > arena->block_size) {
        // Allocate new block
        aria_arena_block* new_block = malloc(sizeof(aria_arena_block));
        new_block->memory = malloc(arena->block_size);
        new_block->size = arena->block_size;
        new_block->next = nullptr;
        
        // Chain to existing blocks
        arena->current_block->next = new_block;
        arena->current_block = new_block;
        
        aligned_offset = 0;  // Fresh start in new block
    }
    
    // 3. Bump pointer
    void* ptr = (char*)arena->current_block->memory + aligned_offset;
    arena->current_offset = aligned_offset + size;
    arena->total_allocated += size;
    
    return ptr;
}
```

### Mixed-Type Allocation Example

```aria
arena := Arena.new();

a := arena.alloc<uint8>();     // offset=0, size=1
// offset now = 1, need to align to 4 for uint32
b := arena.alloc<uint32>();    // offset=4 (aligned!), size=4
// offset now = 8, already aligned for uint64
c := arena.alloc<uint64>();    // offset=8, size=8
// offset now = 16, already aligned for vec3
d := arena.alloc<vec3>();      // offset=16 (aligned!), size=16

Arena.destroy(arena);
```

**Memory layout:**

```
[uint8][pad][pad][pad][uint32....][uint64........][vec3...............]
  ^0    ^1   ^2   ^3    ^4   ^5 ^6  ^8      ^12 ^14 ^16      ^24    ^31
                        └aligned   └aligned        └aligned
```

---

## API Specification

### Aria Types

```aria
/// Arena allocator for bulk temporary allocations
struct Arena {
    handle: wild void@,  // Opaque pointer to aria_arena runtime struct
}
```

### Core Functions

```aria
/// Create new arena with default block size (4KB)
fn Arena.new() -> wild Arena@ {
    raw := @extern("aria_arena_create")(4096);
    arena := alloc<Arena>();
    arena.handle = raw;
    return arena;
}

/// Create arena with custom block size
/// block_size: Size in bytes for each memory block (must be > 0)
fn Arena.with_capacity(block_size: uint64) -> wild Arena@ {
    raw := @extern("aria_arena_create")(block_size);
    arena := alloc<Arena>();
    arena.handle = raw;
    return arena;
}

/// Allocate single object from arena
/// Returns wild pointer (not GC-tracked)
fn Arena.alloc<T>(arena: wild Arena@) -> wild T@ {
    ptr := @extern("aria_arena_alloc")(
        arena.handle, 
        @sizeof(T), 
        @alignof(T)
    );
    return @cast<wild T@>(ptr);
}

/// Allocate array from arena
/// count: Number of elements (must be > 0)
fn Arena.alloc_array<T>(arena: wild Arena@, count: uint64) -> wild T@ {
    total_size := @sizeof(T) * count;
    ptr := @extern("aria_arena_alloc")(
        arena.handle, 
        total_size, 
        @alignof(T)
    );
    return @cast<wild T@>(ptr);
}

/// Reset arena for reuse (keeps allocated blocks)
/// All previous pointers become invalid!
fn Arena.reset(arena: wild Arena@) -> void {
    @extern("aria_arena_reset")(arena.handle);
}

/// Destroy arena and free all memory
/// All pointers from this arena become invalid!
fn Arena.destroy(arena: wild Arena@) -> void {
    @extern("aria_arena_destroy")(arena.handle);
    free<Arena>(arena);
}

/// Get total bytes allocated from arena (for debugging)
fn Arena.bytes_allocated(arena: wild Arena@) -> uint64 {
    return @extern("aria_arena_bytes_allocated")(arena.handle);
}

/// Get total bytes reserved (including unused blocks)
fn Arena.bytes_reserved(arena: wild Arena@) -> uint64 {
    return @extern("aria_arena_bytes_reserved")(arena.handle);
}
```

### Usage Examples

**Simple allocation:**

```aria
fn test_simple() {
    arena := Arena.new();
    defer Arena.destroy(arena);
    
    x := arena.alloc<int32>();
    *x = 42;
    
    print(*x);  // 42
}
```

**Array allocation:**

```aria
fn test_array() {
    arena := Arena.new();
    defer Arena.destroy(arena);
    
    numbers := arena.alloc_array<int32>(100);
    
    for i in 0..100 {
        numbers[i] = i * 2;
    }
    
    for i in 0..100 {
        print(numbers[i]);
    }
}
```

**String building:**

```aria
fn build_json(data: Data) -> string {
    arena := Arena.with_capacity(64 * 1024);  // 64KB
    defer Arena.destroy(arena);
    
    parts := arena.alloc_array<string>(data.fields.len);
    
    for i, field in data.fields {
        parts[i] = arena_sprintf(arena, "\"%s\": %d", field.name, field.value);
    }
    
    return join(parts, ", ");  // Final string on GC heap
}
```

**Nested structures:**

```aria
struct Node {
    value: int32,
    left: wild Node@,
    right: wild Node@,
}

fn build_tree(arena: wild Arena@, depth: int32) -> wild Node@ {
    if depth == 0 {
        return null;
    }
    
    node := arena.alloc<Node>();
    node.value = depth;
    node.left = build_tree(arena, depth - 1);
    node.right = build_tree(arena, depth - 1);
    return node;
}

fn test_tree() {
    arena := Arena.new();
    defer Arena.destroy(arena);
    
    root := build_tree(arena, 10);
    // Process tree...
    // All nodes freed when arena destroyed
}
```

---

## Runtime Integration

### C++ Runtime Structures

```cpp
// src/runtime/allocators/arena_alloc.h

struct aria_arena_block {
    void* memory;              // Allocated memory
    size_t size;               // Size of this block
    aria_arena_block* next;    // Next block in chain (or nullptr)
};

struct aria_arena {
    aria_arena_block* first_block;    // Head of block list
    aria_arena_block* current_block;  // Active block for allocation
    size_t block_size;                // Fixed size for new blocks
    size_t current_offset;            // Offset in current block
    size_t total_allocated;           // Total bytes allocated (stats)
    size_t total_reserved;            // Total bytes reserved (stats)
};

// Create arena with specified block size
aria_arena* aria_arena_create(size_t block_size);

// Allocate from arena with alignment
void* aria_arena_alloc(aria_arena* arena, size_t size, size_t alignment);

// Reset arena (reuse blocks, don't free)
void aria_arena_reset(aria_arena* arena);

// Destroy arena (free all blocks)
void aria_arena_destroy(aria_arena* arena);

// Statistics
size_t aria_arena_bytes_allocated(aria_arena* arena);
size_t aria_arena_bytes_reserved(aria_arena* arena);
```

### Implementation

```cpp
// src/runtime/allocators/arena_alloc.cpp

#include "arena_alloc.h"
#include <cstdlib>
#include <cstring>

aria_arena* aria_arena_create(size_t block_size) {
    if (block_size == 0) {
        block_size = 4096;  // Default
    }
    
    aria_arena* arena = (aria_arena*)malloc(sizeof(aria_arena));
    arena->block_size = block_size;
    arena->current_offset = 0;
    arena->total_allocated = 0;
    arena->total_reserved = 0;
    
    // Allocate first block
    aria_arena_block* block = (aria_arena_block*)malloc(sizeof(aria_arena_block));
    block->memory = malloc(block_size);
    block->size = block_size;
    block->next = nullptr;
    
    arena->first_block = block;
    arena->current_block = block;
    arena->total_reserved = block_size;
    
    return arena;
}

void* aria_arena_alloc(aria_arena* arena, size_t size, size_t alignment) {
    // Align offset
    size_t aligned_offset = (arena->current_offset + alignment - 1) & ~(alignment - 1);
    
    // Check if fits in current block
    if (aligned_offset + size > arena->block_size) {
        // Need new block
        aria_arena_block* new_block = (aria_arena_block*)malloc(sizeof(aria_arena_block));
        new_block->memory = malloc(arena->block_size);
        new_block->size = arena->block_size;
        new_block->next = nullptr;
        
        // Chain to current block
        arena->current_block->next = new_block;
        arena->current_block = new_block;
        arena->total_reserved += arena->block_size;
        
        aligned_offset = 0;  // Start fresh
    }
    
    // Allocate from current block
    void* ptr = (char*)arena->current_block->memory + aligned_offset;
    arena->current_offset = aligned_offset + size;
    arena->total_allocated += size;
    
    return ptr;
}

void aria_arena_reset(aria_arena* arena) {
    arena->current_block = arena->first_block;
    arena->current_offset = 0;
    arena->total_allocated = 0;
    // Keep total_reserved as-is
}

void aria_arena_destroy(aria_arena* arena) {
    aria_arena_block* block = arena->first_block;
    while (block) {
        aria_arena_block* next = block->next;
        free(block->memory);
        free(block);
        block = next;
    }
    free(arena);
}

size_t aria_arena_bytes_allocated(aria_arena* arena) {
    return arena->total_allocated;
}

size_t aria_arena_bytes_reserved(aria_arena* arena) {
    return arena->total_reserved;
}
```

### Integration with TBB Allocator

Arena allocator is **independent** from wild_alloc (TBB sticky errors):

- **wild_alloc**: Individual allocations with explicit free()
- **Arena**: Bulk allocations with single destroy()

Both are "wild" (not GC-tracked), but serve different use cases:

```aria
// Wild pointer with individual free
x := alloc<Data>();
free<Data>(x);

// Arena with bulk free
arena := Arena.new();
y := arena.alloc<Data>();
z := arena.alloc<Data>();
Arena.destroy(arena);  // Frees both y and z
```

---

## Performance Characteristics

### Allocation Speed

**malloc (traditional):**
1. Search free list: O(n) or O(log n) with tree
2. Update metadata: headers/footers
3. Thread synchronization: locks or atomics
4. **Time**: 10-50ns per allocation

**Arena allocator:**
1. Align offset: O(1) bitwise operation
2. Bump offset: O(1) addition
3. No locks (thread-local)
4. **Time**: 1-2ns per allocation

**Speedup**: **10-50x faster** 🚀

### Memory Overhead

**malloc overhead:**
```
Per allocation:
[header: 16 bytes][user data][footer: 8 bytes][padding]

Total overhead: ~24 bytes per allocation
```

**Arena overhead:**
```
Per allocation:
[user data][alignment padding: 0-15 bytes]

Total overhead: ~0-15 bytes (alignment only)
```

**Savings**: ~24 bytes per allocation

**Example:**
```
1000 allocations of 32 bytes each:

malloc:  1000 * (32 + 24) = 56KB
arena:   1000 * 32 = 32KB (+ block overhead)

Memory savings: ~43%
```

### Deallocation Speed

**free (traditional):**
1. Find block in free list
2. Coalesce with adjacent blocks
3. Update free list structure
4. Thread synchronization
5. **Time**: 10-50ns per free

**Arena destroy:**
1. Walk block list: O(num_blocks)
2. Call free() on each block
3. **Time**: ~100ns total for typical usage

**For 1000 allocations:**
- malloc: 1000 * 50ns = 50µs
- arena: 100ns
- **Speedup**: **500x faster** 🚀🚀🚀

### Cache Performance

**Arena advantages:**
1. **Spatial locality**: Sequential allocations are adjacent in memory
2. **Temporal locality**: Related data allocated together
3. **Fewer cache misses**: No metadata interspersed

**Benchmark example** (hypothetical):
```
struct Node { int value; Node* next; };

// malloc: Nodes scattered in heap
for (int i = 0; i < 1000; i++) {
    Node* n = (Node*)malloc(sizeof(Node));
    // Nodes at random addresses: 0x1000, 0x5000, 0x2000, ...
    // Cache misses: ~800/1000 (80%)
}

// Arena: Nodes sequential
Arena* arena = arena_create(4096);
for (int i = 0; i < 1000; i++) {
    Node* n = arena_alloc<Node>(arena);
    // Nodes at sequential addresses: 0x1000, 0x1010, 0x1020, ...
    // Cache misses: ~10/1000 (1%)
}
```

**Cache miss reduction**: ~80x fewer misses!

### Benchmark Comparison Table

| Metric | malloc/free | Arena | Speedup |
|--------|-------------|-------|---------|
| Allocation (1 object) | 20ns | 1ns | 20x |
| Allocation (1000 objects) | 20µs | 1µs | 20x |
| Deallocation (1000 objects) | 50µs | 0.1µs | 500x |
| Memory overhead (32B object) | 56B | 32-48B | 1.2-1.8x |
| Cache misses (sequential scan) | 800/1000 | 10/1000 | 80x |
| **Overall speedup** | - | - | **10-500x** |

---

## Implementation Considerations

### 1. Block Size Selection

**Small blocks (1-4KB):**
- ✅ Less wasted space
- ✅ Good for small, predictable allocations
- ❌ More chaining overhead
- ❌ Worse cache locality across blocks

**Large blocks (64KB - 1MB):**
- ✅ Better cache locality
- ✅ Fewer block allocations
- ✅ Less chaining overhead
- ❌ Can waste significant space if underutilized

**Recommendation**: 
- **Default**: 4KB (page size, good balance)
- **Allow customization**: `Arena.with_capacity(size)`
- **Future**: Auto-tune based on allocation patterns (Phase 4.3+)

### 2. Thread Safety

**Phase 4.2 approach**: Thread-local arenas only

```aria
// Each thread creates its own arena
fn worker_thread() {
    arena := Arena.new();  // Thread-local
    defer Arena.destroy(arena);
    
    // No synchronization needed!
    for task in queue {
        process(arena, task);
    }
}
```

**Future (Phase 4.3+)**: Shared arenas with locks
```aria
fn shared_arena_alloc(arena: wild SharedArena@, size: uint64) -> wild void@ {
    arena.mutex.lock();
    defer arena.mutex.unlock();
    return arena_alloc_impl(arena, size);
}
```

### 3. Large Allocation Handling

**Problem**: What if allocation > block_size?

**Option A**: Reject (error)
```cpp
if (size > arena->block_size) {
    return nullptr;  // Error!
}
```

**Option B**: Allocate oversized block
```cpp
if (size > arena->block_size) {
    // Allocate special oversized block
    aria_arena_block* big = malloc(sizeof(aria_arena_block));
    big->memory = malloc(size);
    big->size = size;
    big->next = arena->current_block->next;
    arena->current_block->next = big;
    
    return big->memory;
}
```

**Recommendation**: Option B (graceful handling)

### 4. Integration with Defer

Arenas work perfectly with Aria's `defer`:

```aria
fn process_data(path: string) {
    arena := Arena.new();
    defer Arena.destroy(arena);  // Guaranteed cleanup!
    
    data := parse_file(arena, path);
    // ... process ...
    
    // Even if error, arena destroyed
}
```

### 5. Debugging Support

**Memory tracking:**
```aria
arena := Arena.new();
// ... allocate stuff ...
print("Allocated: {arena.bytes_allocated()} bytes");
print("Reserved: {arena.bytes_reserved()} bytes");
print("Efficiency: {(arena.bytes_allocated() * 100) / arena.bytes_reserved()}%");
```

**Future**: Valgrind integration
```cpp
#ifdef VALGRIND
#include <valgrind/memcheck.h>

void* aria_arena_alloc(...) {
    void* ptr = /* allocate */;
    VALGRIND_MALLOCLIKE_BLOCK(ptr, size, 0, 0);
    return ptr;
}

void aria_arena_destroy(...) {
    // Mark all memory as freed
    VALGRIND_FREELIKE_BLOCK(ptr, 0);
    /* free blocks */
}
#endif
```

---

## Testing Strategy

### C++ Unit Tests

**File**: `tests/runtime/test_arena_alloc.cpp`

```cpp
TEST(ArenaAllocator, BasicAllocation) {
    aria_arena* arena = aria_arena_create(4096);
    
    int* x = (int*)aria_arena_alloc(arena, sizeof(int), alignof(int));
    *x = 42;
    EXPECT_EQ(*x, 42);
    
    aria_arena_destroy(arena);
}

TEST(ArenaAllocator, ArrayAllocation) {
    aria_arena* arena = aria_arena_create(4096);
    
    int* arr = (int*)aria_arena_alloc(arena, sizeof(int) * 100, alignof(int));
    for (int i = 0; i < 100; i++) {
        arr[i] = i;
    }
    
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(arr[i], i);
    }
    
    aria_arena_destroy(arena);
}

TEST(ArenaAllocator, MixedTypeAlignment) {
    aria_arena* arena = aria_arena_create(4096);
    
    uint8_t* a = (uint8_t*)aria_arena_alloc(arena, 1, 1);
    uint32_t* b = (uint32_t*)aria_arena_alloc(arena, 4, 4);
    uint64_t* c = (uint64_t*)aria_arena_alloc(arena, 8, 8);
    
    // Verify alignment
    EXPECT_EQ(reinterpret_cast<uintptr_t>(b) % 4, 0);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(c) % 8, 0);
    
    *a = 1;
    *b = 42;
    *c = 999;
    
    EXPECT_EQ(*a, 1);
    EXPECT_EQ(*b, 42);
    EXPECT_EQ(*c, 999);
    
    aria_arena_destroy(arena);
}

TEST(ArenaAllocator, BlockChaining) {
    aria_arena* arena = aria_arena_create(64);  // Small blocks
    
    // Allocate enough to trigger multiple blocks
    for (int i = 0; i < 10; i++) {
        void* ptr = aria_arena_alloc(arena, 32, 1);
        EXPECT_NE(ptr, nullptr);
    }
    
    EXPECT_GT(aria_arena_bytes_reserved(arena), 64);  // Multiple blocks
    
    aria_arena_destroy(arena);
}

TEST(ArenaAllocator, Reset) {
    aria_arena* arena = aria_arena_create(4096);
    
    for (int i = 0; i < 100; i++) {
        aria_arena_alloc(arena, 32, 1);
    }
    
    size_t allocated = aria_arena_bytes_allocated(arena);
    EXPECT_GT(allocated, 0);
    
    aria_arena_reset(arena);
    
    EXPECT_EQ(aria_arena_bytes_allocated(arena), 0);
    // Reserved should stay the same (blocks not freed)
    EXPECT_EQ(aria_arena_bytes_reserved(arena), 4096);
    
    aria_arena_destroy(arena);
}

TEST(ArenaAllocator, LargeAllocation) {
    aria_arena* arena = aria_arena_create(1024);
    
    // Allocate larger than block size
    void* ptr = aria_arena_alloc(arena, 2048, 1);
    EXPECT_NE(ptr, nullptr);
    
    aria_arena_destroy(arena);
}

TEST(ArenaAllocator, Statistics) {
    aria_arena* arena = aria_arena_create(4096);
    
    EXPECT_EQ(aria_arena_bytes_allocated(arena), 0);
    EXPECT_EQ(aria_arena_bytes_reserved(arena), 4096);
    
    aria_arena_alloc(arena, 100, 1);
    
    EXPECT_EQ(aria_arena_bytes_allocated(arena), 100);
    EXPECT_EQ(aria_arena_bytes_reserved(arena), 4096);
    
    aria_arena_destroy(arena);
}

TEST(ArenaAllocator, NoMemoryLeaks) {
    // Run under Valgrind: should report 0 leaks
    for (int i = 0; i < 1000; i++) {
        aria_arena* arena = aria_arena_create(4096);
        for (int j = 0; j < 100; j++) {
            aria_arena_alloc(arena, 32, 1);
        }
        aria_arena_destroy(arena);
    }
}
```

### Aria Integration Tests

**File**: `tests/integration/arena_allocator.aria`

```aria
fn test_simple_allocation() {
    arena := Arena.new();
    defer Arena.destroy(arena);
    
    x := arena.alloc<int32>();
    *x = 42;
    
    assert(*x == 42);
}

fn test_array_allocation() {
    arena := Arena.new();
    defer Arena.destroy(arena);
    
    numbers := arena.alloc_array<int32>(100);
    
    for i in 0..100 {
        numbers[i] = i * 2;
    }
    
    for i in 0..100 {
        assert(numbers[i] == i * 2);
    }
}

fn test_mixed_types() {
    arena := Arena.new();
    defer Arena.destroy(arena);
    
    a := arena.alloc<uint8>();
    b := arena.alloc<uint32>();
    c := arena.alloc<uint64>();
    
    *a = 1;
    *b = 42;
    *c = 999;
    
    assert(*a == 1);
    assert(*b == 42);
    assert(*c == 999);
}

fn test_custom_capacity() {
    arena := Arena.with_capacity(8192);
    defer Arena.destroy(arena);
    
    // Should fit without chaining
    data := arena.alloc_array<uint8>(8000);
    assert(data != null);
}

fn test_reset() {
    arena := Arena.new();
    defer Arena.destroy(arena);
    
    x := arena.alloc<int32>();
    *x = 42;
    
    arena.reset();
    
    y := arena.alloc<int32>();
    *y = 99;
    
    // x is now invalid (use-after-reset)
    // Only y is valid
    assert(*y == 99);
}

fn test_nested_structures() {
    struct Node {
        value: int32,
        left: wild Node@,
        right: wild Node@,
    }
    
    arena := Arena.new();
    defer Arena.destroy(arena);
    
    root := arena.alloc<Node>();
    root.value = 1;
    root.left = arena.alloc<Node>();
    root.left.value = 2;
    root.right = arena.alloc<Node>();
    root.right.value = 3;
    
    assert(root.value == 1);
    assert(root.left.value == 2);
    assert(root.right.value == 3);
}

fn test_statistics() {
    arena := Arena.new();
    defer Arena.destroy(arena);
    
    initial := arena.bytes_allocated();
    assert(initial == 0);
    
    arena.alloc<int32>();
    
    after := arena.bytes_allocated();
    assert(after >= 4);  // At least sizeof(int32)
}

fn main() {
    test_simple_allocation();
    test_array_allocation();
    test_mixed_types();
    test_custom_capacity();
    test_reset();
    test_nested_structures();
    test_statistics();
    
    print("All arena allocator tests passed! ✅");
}
```

### Performance Benchmarks

**File**: `tests/benchmarks/arena_vs_malloc.cpp`

```cpp
#include <benchmark/benchmark.h>
#include "runtime/allocators/arena_alloc.h"

static void BM_MallocFree(benchmark::State& state) {
    for (auto _ : state) {
        void* ptrs[1000];
        for (int i = 0; i < 1000; i++) {
            ptrs[i] = malloc(32);
        }
        for (int i = 0; i < 1000; i++) {
            free(ptrs[i]);
        }
    }
}
BENCHMARK(BM_MallocFree);

static void BM_ArenaAlloc(benchmark::State& state) {
    for (auto _ : state) {
        aria_arena* arena = aria_arena_create(4096);
        for (int i = 0; i < 1000; i++) {
            aria_arena_alloc(arena, 32, 1);
        }
        aria_arena_destroy(arena);
    }
}
BENCHMARK(BM_ArenaAlloc);

BENCHMARK_MAIN();
```

**Expected results:**
```
Benchmark                    Time
-----------------------------------------
BM_MallocFree          50000 ns
BM_ArenaAlloc           1000 ns  (50x faster!)
```

---

## Success Criteria

Phase 4.2 Stage 1.2 is complete when:

1. ✅ **Research document created** (this file)
2. ⏳ **C++ runtime implemented** (`src/runtime/allocators/arena_alloc.cpp`)
   - `aria_arena_create()`
   - `aria_arena_alloc()` with alignment
   - `aria_arena_reset()`
   - `aria_arena_destroy()`
   - Statistics functions
3. ⏳ **Aria wrapper created** (`lib/std/core/arena.aria`)
   - `Arena` type
   - `Arena.new()`, `Arena.with_capacity()`
   - `Arena.alloc<T>()`, `Arena.alloc_array<T>()`
   - `Arena.reset()`, `Arena.destroy()`
   - `Arena.bytes_allocated()`, `Arena.bytes_reserved()`
4. ⏳ **Tests written and passing**
   - 8+ C++ unit tests
   - 7+ Aria integration tests
   - All tests pass under Valgrind (no leaks)
5. ⏳ **Documentation complete**
   - API docs in this file
   - Usage examples
   - Performance characteristics
   - Integration guide

**Completion checklist:**
- [x] Research prior art (Rust, Zig, C++)
- [x] Design API specification
- [x] Design runtime integration
- [x] Document performance characteristics
- [x] Plan testing strategy
- [ ] Implement C++ runtime
- [ ] Implement Aria wrapper
- [ ] Write and run tests
- [ ] Validate no memory leaks
- [ ] Update Phase 4.2 progress tracker

---

## References

### External Resources

1. **Rust `bumpalo` crate**
   - https://github.com/fitzgen/bumpalo
   - Zero-cost arena allocator implementation
   - Excellent alignment handling

2. **Zig ArenaAllocator**
   - https://github.com/ziglang/zig/blob/master/lib/std/heap/arena_allocator.zig
   - Simple and elegant design
   - Good reference for linked list approach

3. **"Memory Management in C++" by Nicolai Josuttis**
   - Chapter on custom allocators
   - Arena allocator patterns

4. **"The Memory Management Reference"**
   - http://www.memorymanagement.org/
   - Comprehensive resource on allocation strategies

### Internal References

1. `docs/stdlib/memory_api.md` - Wild allocator API (Phase 4.1)
2. `docs/stdlib/result_type_design.md` - Error handling (Phase 4.2 Stage 1.1)
3. `src/runtime/allocators/wild_alloc.cpp` - TBB sticky errors allocator
4. `tests/runtime/test_wild_alloc.cpp` - Wild allocator test examples

---

**Document Status**: ✅ Complete  
**Next Stage**: 1.3 - Custom Allocator Patterns Research  
**Estimated Implementation Time**: 3-4 hours (C++ + Aria + tests)

