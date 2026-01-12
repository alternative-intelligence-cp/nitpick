# Aria Intrinsics Reference

**Version**: 0.0.7-dev  
**Last Updated**: January 5, 2026  
**Total Intrinsics**: 186 (168 existing + 18 TBB intrinsics)

## Table of Contents

1. [Memory Management](#memory-management)
2. [Type Conversions & Widening](#type-conversions--widening)
3. [TBB (Tri-State Binary) Operations](#tbb-operations)
4. [Integer Operations](#integer-operations)
5. [Performance & Debugging](#performance--debugging)
6. [Arena Allocators](#arena-allocators)
7. [Pool Allocators](#pool-allocators)
8. [Slab Allocators](#slab-allocators)

---

## Memory Management

### Wild Memory (Manual Heap)

#### `aria_alloc`
**Signature**: `void* aria_alloc(int64 size)`  
**Purpose**: Allocate unmanaged heap memory  
**Returns**: Pointer to allocated memory or NULL on failure  
**Note**: Must be freed with `aria_free` or memory leak will occur

**Example**:
```aria
// Allocate 1024 bytes
int8*:buffer = aria_alloc(1024);
if (buffer == null) {
    // Handle allocation failure
} else {
    // Use buffer
    aria_free(buffer);
}
```

**Performance**: O(1) typical, O(n) worst case (heap fragmentation)

---

#### `aria_free`
**Signature**: `void aria_free(void* ptr)`  
**Purpose**: Free memory allocated by `aria_alloc`  
**Parameters**:
- `ptr`: Pointer to free (NULL-safe - calling with NULL is a no-op)

**Example**:
```aria
int32*:value = aria_alloc(4);
*value = 42;
aria_free(value);  // Safe
aria_free(null);   // Also safe (no-op)
```

**Safety**: Double-free detection in debug builds only

---

#### `aria_realloc`
**Signature**: `void* aria_realloc(void* ptr, int64 new_size)`  
**Purpose**: Resize existing allocation  
**Returns**: New pointer (may be different from original)  
**Note**: Original pointer becomes invalid after realloc

**Example**:
```aria
int32*:arr = aria_alloc(40);  // 10 int32s
// ...
arr = aria_realloc(arr, 80);   // Resize to 20 int32s
```

---

#### `aria_alloc_array`
**Signature**: `void* aria_alloc_array(int64 elem_size, int64 count)`  
**Purpose**: Allocate array with overflow protection  
**Returns**: Pointer to array or NULL on overflow/failure  
**Safety**: Checks for `elem_size * count` overflow before allocation

**Example**:
```aria
// Allocate array of 1000 int64s
int64*:numbers = aria_alloc_array(8, 1000);
```

---

#### `aria_alloc_buffer`
**Signature**: `void* aria_alloc_buffer(int64 size, int64 alignment, int8 zero_init)`  
**Purpose**: Allocate aligned memory with optional zero-initialization  
**Parameters**:
- `alignment`: Must be power of 2 (typically 16, 32, or 64)
- `zero_init`: 1 = zero memory, 0 = uninitialized

**Example**:
```aria
// Allocate 512-byte buffer aligned to 64 bytes (AVX-512)
void*:aligned = aria_alloc_buffer(512, 64, 1);
```

---

#### `aria_alloc_string`
**Signature**: `int8* aria_alloc_string(int64 size)`  
**Purpose**: Allocate memory for C-style string (includes +1 for null terminator)  
**Returns**: Pointer to character buffer

**Example**:
```aria
int8*:str = aria_alloc_string(100);  // 101 bytes allocated
```

---

### GC Memory (Garbage Collected)

#### `aria_gc_alloc`
**Signature**: `void* aria_gc_alloc(int64 size, int64 type_id)`  
**Purpose**: Allocate garbage-collected memory  
**Parameters**:
- `size`: Bytes to allocate
- `type_id`: Type identifier for conservative scanning (0 = unknown)

**Returns**: Pointer that will be automatically freed by GC  
**Note**: Do NOT call `aria_free` on GC-allocated memory

**Example**:
```aria
// Allocate GC'd buffer
void*:gc_data = aria_gc_alloc(256, 0);
// No explicit free needed - GC will clean up
```

---

#### `aria_std_gc_alloc`
**Signature**: `void* aria_std_gc_alloc(int64 size)`  
**Purpose**: Simplified GC allocation (wrapper with type_id=0)  
**Returns**: GC-managed pointer

---

### Executable Memory (JIT/Code Generation)

#### `aria_alloc_exec`
**Signature**: `void* aria_alloc_exec(int64 size)`  
**Purpose**: Allocate memory with execute permissions  
**Returns**: Pointer to executable memory region  
**Use Case**: JIT compilation, runtime code generation

**Example**:
```aria
int8*:code = aria_alloc_exec(4096);
// Write machine code to buffer
// Jump to code address to execute
```

---

#### `aria_free_exec`
**Signature**: `void aria_free_exec(void* ptr)`  
**Purpose**: Free executable memory  
**Note**: Platform-specific (uses `munmap` on POSIX, `VirtualFree` on Windows)

---

## Type Conversions & Widening

### TBB Widening

#### `@widen`
**Signature**: `tbbN @widen(tbbM value)` where N > M  
**Purpose**: Widen smaller TBB type to larger TBB type  
**Sentinel Propagation**: Sticky - sentinels propagate to wider type

**Supported Conversions**:
- `tbb8 → tbb16` (8-bit to 16-bit)
- `tbb8 → tbb32` (8-bit to 32-bit)
- `tbb8 → tbb64` (8-bit to 64-bit)
- `tbb16 → tbb32` (16-bit to 32-bit)
- `tbb16 → tbb64` (16-bit to 64-bit)
- `tbb32 → tbb64` (32-bit to 64-bit)

**Example**:
```aria
tbb8:small = ERR8;
tbb32:wide = @widen(small);  // wide = ERR32 (sentinel propagates)

tbb16:val = 100;
tbb64:big = @widen(val);     // big = 100 (numeric value widens)
```

---

### Integer Conversions

#### `@i64`
**Signature**: `int64 @i64(intX value)`  
**Purpose**: Convert any integer type to int64  
**Sign Extension**: Preserves sign for signed types

**Example**:
```aria
int32:x = -42;
int64:y = @i64(x);  // y = -42 (sign extended)
```

---

#### `@i32`, `@i16`, `@i8`
**Signature**: `intN @iN(intX value)`  
**Purpose**: Narrow or convert integer types  
**Truncation**: Truncates higher bits if narrowing

---

## TBB Operations

### Arithmetic Intrinsics

#### `@add_tbb32`
**Signature**: `tbb32 @add_tbb32(tbb32 a, tbb32 b)`  
**Purpose**: TBB-aware addition with sentinel propagation  
**Sentinel Rules**:
- `ERR + x = ERR`
- `x + ERR = ERR`
- `INF + x = INF` (if x ≠ -INF)
- `NULL + x = NULL`

**Example**:
```aria
tbb32:x = 10;
tbb32:y = 20;
tbb32:z = @add_tbb32(x, y);  // z = 30

tbb32:err = ERR32;
tbb32:bad = @add_tbb32(err, x);  // bad = ERR32 (sticky)
```

---

#### `@sub_tbb32`, `@mul_tbb32`, `@div_tbb32`
**Signature**: `tbb32 @op_tbb32(tbb32 a, tbb32 b)`  
**Purpose**: TBB subtraction, multiplication, division  
**Sentinel Propagation**: Same sticky rules as addition

**Division Special Cases**:
- `x / 0 = ERR32` (division by zero)
- `ERR / x = ERR`

---

#### `@mod_tbb32`
**Signature**: `tbb32 @mod_tbb32(tbb32 a, tbb32 b)`  
**Purpose**: TBB modulo operation  
**Special Cases**:
- `x % 0 = ERR32`
- `ERR % x = ERR`

---

#### `@neg_tbb32`
**Signature**: `tbb32 @neg_tbb32(tbb32 a)`  
**Purpose**: TBB negation  
**Sentinel Handling**:
- `-ERR = ERR`
- `-INF = -INF`
- `-NULL = NULL`

---

### 64-bit TBB Operations

All TBB32 operations have TBB64 equivalents:
- `@add_tbb64`, `@sub_tbb64`, `@mul_tbb64`
- `@div_tbb64`, `@mod_tbb64`, `@neg_tbb64`

---

### Comparison Intrinsics

#### `@eq_tbb32`, `@ne_tbb32`
**Signature**: `int8 @eq_tbb32(tbb32 a, tbb32 b)`  
**Purpose**: TBB equality/inequality comparison  
**Returns**: 1 (true) or 0 (false)

**Sentinel Handling**:
- `ERR == ERR` → 1
- `NULL == NULL` → 1
- `INF == INF` → 1
- `ERR == numeric` → 0

---

#### `@lt_tbb32`, `@le_tbb32`, `@gt_tbb32`, `@ge_tbb32`
**Signature**: `int8 @op_tbb32(tbb32 a, tbb32 b)`  
**Purpose**: TBB relational comparisons  
**Sentinel Ordering**: `ERR < NULL < -INF < numeric < INF`

**Example**:
```aria
tbb32:a = 10;
tbb32:b = 20;
int8:result = @lt_tbb32(a, b);  // result = 1 (10 < 20)

tbb32:err = ERR32;
int8:check = @lt_tbb32(err, a);  // check = 1 (ERR < numeric)
```

---

### Sentinel Detection

#### `@is_sentinel_tbb32`
**Signature**: `int8 @is_sentinel_tbb32(tbb32 value)`  
**Purpose**: Check if value is a sentinel (ERR, NULL, or INF)  
**Returns**: 1 if sentinel, 0 if numeric

**Example**:
```aria
tbb32:x = 42;
tbb32:y = ERR32;
int8:check1 = @is_sentinel_tbb32(x);  // 0
int8:check2 = @is_sentinel_tbb32(y);  // 1
```

---

#### `@is_err_tbb32`, `@is_null_tbb32`, `@is_inf_tbb32`
**Signature**: `int8 @is_TYPE_tbb32(tbb32 value)`  
**Purpose**: Check for specific sentinel values  
**Returns**: 1 if matches, 0 otherwise

---

## Integer Operations

### Bit Manipulation

#### `@clz`
**Signature**: `int32 @clz(int64 value)`  
**Purpose**: Count Leading Zeros  
**Returns**: Number of zero bits before first 1 bit (0-63)

**Example**:
```aria
int64:x = 0x0000_0000_FFFF_FFFF;  // 32 leading zeros
int32:count = @clz(x);  // count = 32
```

**Performance**: Single CPU instruction (BSR/LZCNT on x86)

---

#### `@ctz`
**Signature**: `int32 @ctz(int64 value)`  
**Purpose**: Count Trailing Zeros  
**Returns**: Number of zero bits after last 1 bit (0-63)

---

#### `@popcount`
**Signature**: `int32 @popcount(int64 value)`  
**Purpose**: Population count (number of 1 bits)  
**Returns**: Count of set bits (0-64)

**Example**:
```aria
int64:x = 0x00FF;  // 8 bits set
int32:bits = @popcount(x);  // bits = 8
```

---

## Performance & Debugging

### Alignment

#### `@sizeof`
**Signature**: `int64 @sizeof(type)`  
**Purpose**: Get size of type in bytes  
**Compile-Time**: Resolved at compile time (constant)

**Example**:
```aria
int64:int_size = @sizeof(int32);    // 4
int64:float_size = @sizeof(flt64);  // 8
int64:tbb_size = @sizeof(tbb32);    // 4
```

---

#### `@alignof`
**Signature**: `int64 @alignof(type)`  
**Purpose**: Get alignment requirement of type  
**Compile-Time**: Constant expression

**Example**:
```aria
int64:align = @alignof(fix256);  // 32 (256-bit alignment)
```

---

### Panic & Error Handling

#### `@panic`
**Signature**: `void @panic(int8* message)`  
**Purpose**: Trigger runtime panic with error message  
**Behavior**: Prints message and aborts program

**Example**:
```aria
if (ptr == null) {
    @panic("Null pointer dereference!");
}
```

---

## Arena Allocators

### Arena Lifecycle

#### `aria_arena_new_handle`
**Signature**: `int64 aria_arena_new_handle(int64 capacity)`  
**Purpose**: Create new arena allocator  
**Returns**: Opaque handle (0 on error)  
**Use Case**: Fast bump allocator for temporary objects

**Example**:
```aria
int64:arena = aria_arena_new_handle(4096);
if (arena == 0) {
    // Error handling
} else {
}
```

---

#### `aria_arena_alloc_handle`
**Signature**: `int64 aria_arena_alloc_handle(int64 handle, int64 size)`  
**Purpose**: Allocate from arena  
**Returns**: Pointer as int64 (0 on error)  
**Performance**: O(1) - simple pointer bump

---

#### `aria_arena_reset_handle`
**Signature**: `void aria_arena_reset_handle(int64 handle)`  
**Purpose**: Reset arena for reuse (invalidates all pointers)  
**Warning**: All previous allocations become invalid

---

#### `aria_arena_destroy_handle`
**Signature**: `void aria_arena_destroy_handle(int64 handle)`  
**Purpose**: Free entire arena and all allocations  
**Cleanup**: Frees all internal blocks

---

### Arena Queries

#### `aria_arena_get_allocated_handle`
**Signature**: `int64 aria_arena_get_allocated_handle(int64 handle)`  
**Purpose**: Get total bytes allocated from arena  
**Returns**: Current allocation size

---

#### `aria_arena_get_reserved_handle`
**Signature**: `int64 aria_arena_get_reserved_handle(int64 handle)`  
**Purpose**: Get total capacity reserved (including unused blocks)  
**Returns**: Total memory reserved

---

## Pool Allocators

### Pool Lifecycle

#### `aria_pool_new_handle`
**Signature**: `int64 aria_pool_new_handle(int64 block_size, int64 initial_capacity)`  
**Purpose**: Create fixed-size block pool  
**Returns**: Opaque pool handle (0 on error)  
**Use Case**: Frequent allocation/deallocation of same-sized objects

**Example**:
```aria
// Pool for 64-byte objects, initial 100 blocks
int64:pool = aria_pool_new_handle(64, 100);
```

---

#### `aria_pool_alloc_handle`
**Signature**: `int64 aria_pool_alloc_handle(int64 handle)`  
**Purpose**: Allocate block from pool  
**Returns**: Pointer to block (0 on error)  
**Performance**: O(1) - free list pop

---

#### `aria_pool_free_handle`
**Signature**: `void aria_pool_free_handle(int64 handle, int64 ptr)`  
**Purpose**: Return block to pool  
**Performance**: O(1) - free list push

---

#### `aria_pool_reset_handle`
**Signature**: `void aria_pool_reset_handle(int64 handle)`  
**Purpose**: Mark all blocks as free (doesn't release memory)

---

#### `aria_pool_destroy_handle`
**Signature**: `void aria_pool_destroy_handle(int64 handle)`  
**Purpose**: Destroy pool and free all memory

---

### Pool Queries

#### `aria_pool_get_total_blocks_handle`
**Signature**: `int64 aria_pool_get_total_blocks_handle(int64 handle)`  
**Purpose**: Get total number of blocks in pool

---

#### `aria_pool_get_used_blocks_handle`
**Signature**: `int64 aria_pool_get_used_blocks_handle(int64 handle)`  
**Purpose**: Get number of currently allocated blocks

---

## Slab Allocators

### Slab Lifecycle

#### `aria_slab_cache_new_handle`
**Signature**: `int64 aria_slab_cache_new_handle(int64 object_size, int64 slab_size)`  
**Purpose**: Create slab allocator cache  
**Returns**: Opaque cache handle (0 on error)  
**Use Case**: Kernel-style object caching (like Linux SLAB)

**Parameters**:
- `object_size`: Size of objects to allocate
- `slab_size`: Size of each slab page (typically 4096)

---

#### `aria_slab_cache_alloc_handle`
**Signature**: `int64 aria_slab_cache_alloc_handle(int64 handle)`  
**Purpose**: Allocate object from slab cache  
**Returns**: Pointer to object (0 on error)

---

#### `aria_slab_cache_free_handle`
**Signature**: `void aria_slab_cache_free_handle(int64 handle, int64 ptr)`  
**Purpose**: Return object to slab cache

---

#### `aria_slab_cache_destroy_handle`
**Signature**: `void aria_slab_cache_destroy_handle(int64 handle)`  
**Purpose**: Destroy slab cache and free all slabs

---

### Slab Queries

#### `aria_slab_cache_get_total_objects_handle`
**Signature**: `int64 aria_slab_cache_get_total_objects_handle(int64 handle)`  
**Purpose**: Get total capacity of all slabs

---

#### `aria_slab_cache_get_allocated_objects_handle`
**Signature**: `int64 aria_slab_cache_get_allocated_objects_handle(int64 handle)`  
**Purpose**: Get number of currently allocated objects

---

## Performance Notes

### Allocation Performance Comparison

| Allocator | Alloc Time | Free Time | Fragmentation | Best Use Case |
|-----------|------------|-----------|---------------|---------------|
| **Wild** (malloc) | O(1)-O(n) | O(1) | High | General purpose |
| **Arena** | O(1) | O(1)* | None | Temporary/batch allocations |
| **Pool** | O(1) | O(1) | Low | Fixed-size objects |
| **Slab** | O(1) | O(1) | Low | Kernel-style caching |
| **GC** | O(1) | O(n)† | Low | Long-lived objects |

*Arena free is O(1) for entire arena, individual objects can't be freed  
†GC free time is amortized over collection cycles

---

### Memory Alignment Requirements

| Type | Size | Alignment | Notes |
|------|------|-----------|-------|
| int8 | 1 | 1 | Single byte |
| int16 | 2 | 2 | Half word |
| int32 | 4 | 4 | Word |
| int64 | 8 | 8 | Double word |
| flt32 | 4 | 4 | IEEE 754 single |
| flt64 | 8 | 8 | IEEE 754 double |
| tbb32 | 4 | 4 | Tri-state binary |
| tbb64 | 8 | 8 | Tri-state binary |
| fix256 | 32 | 32 | 256-bit fixed-point |
| fix512 | 64 | 64 | 512-bit fixed-point |
| fix1024 | 128 | 64 | 1024-bit fixed-point |
| fix2048 | 256 | 64 | 2048-bit fixed-point |
| fix4096 | 512 | 64 | 4096-bit fixed-point |

---

## Safety Guidelines

### Memory Safety

1. **Always check allocation results**
   ```aria
   void*:ptr = aria_alloc(1024);
   if (ptr == null) {
       @panic("Out of memory!");
   } else {}
   ```

2. **Match allocator with deallocator**
   - `aria_alloc` → `aria_free`
   - `aria_gc_alloc` → automatic GC
   - Never mix different allocator types

3. **Avoid use-after-free**
   ```aria
   void*:ptr = aria_alloc(100);
   aria_free(ptr);
   // Don't use ptr here - undefined behavior!
   ```

4. **Arena reset invalidates pointers**
   ```aria
   int64:arena = aria_arena_new_handle(4096);
   int32*:x = aria_arena_alloc_handle(arena, 4);
   aria_arena_reset_handle(arena);
   // x is now invalid - don't dereference!
   ```

---

### TBB Safety

1. **Check for sentinels before arithmetic**
   ```aria
   if (@is_sentinel_tbb32(value)) {
       // Handle special value
   } else {
       // Safe to use as number
   }
   ```

2. **Sentinel propagation is sticky**
   ```aria
   tbb32:err = ERR32;
   tbb32:result = @add_tbb32(err, 100);  // result = ERR32
   // ERR propagates through entire computation!
   ```

3. **Use explicit sentinel checks**
   ```aria
   if (@is_err_tbb32(value)) {
       // Error handling
   } else if (@is_null_tbb32(value)) {
       // Null handling
   } else {
       // Normal processing
   }
   ```

---

## Future Intrinsics (Not Yet Implemented)

- SIMD vector operations (AVX-512)
- Atomic operations (compare-and-swap, fetch-and-add)
- String manipulation (strcpy, strcat, strcmp)
- Math functions (sin, cos, sqrt - use extern "libm" for now)
- File I/O builtins

---

## See Also

- [Stdlib API Reference](API.md) - High-level Aria wrappers
- [Type System Guide](../guides/TYPE_SYSTEM.md) - TBB, LBIM, fix256 details
- [FFI Guide](../guides/FFI.md) - Calling C libraries
- [Safety Guidelines](../guides/SAFETY.md) - Production best practices

---

**Maintained by**: Aria Compiler Team  
**License**: MIT  
**Contact**: https://ai-liberation-platform.org
