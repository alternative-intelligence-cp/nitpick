# Aria Standard Library - Memory API Design
**Phase 4.1**: Core Memory Functions  
**Version**: 0.1.0-dev  
**Date**: December 21, 2025

---

## 1. Overview

This document specifies the memory allocation API for Aria's standard library. The API provides **wild (manual) memory management** for use cases requiring explicit control, complementing Aria's garbage-collected heap.

### Design Principles

1. **Safety through Explicitness**: Manual memory requires deliberate choices
2. **Integration with Existing Systems**: Works with wild pointers and defer
3. **Incremental Implementation**: Start simple, add features progressively
4. **Platform Abstraction**: Hide OS-specific details

---

## 2. Runtime C Functions (Phase 4.1)

These functions are implemented in C++ and linked into compiled Aria programs.

### 2.1 Basic Allocation

```c
void* aria_alloc(int64_t size);
```

**Purpose**: Allocate `size` bytes on the wild heap  
**Returns**: Pointer to allocated memory, or NULL on failure  
**Alignment**: Natural pointer alignment (8 bytes on 64-bit systems)  
**Implementation**: Wrapper around `malloc(size)`  

**Example C++ Implementation**:
```cpp
extern "C" {
    void* aria_alloc(int64_t size) {
        if (size <= 0) return nullptr;
        return malloc(size);
    }
}
```

---

### 2.2 Aligned Allocation

```c
void* aria_alloc_aligned(int64_t size, int64_t alignment);
```

**Purpose**: Allocate `size` bytes with specific alignment  
**Returns**: Pointer to allocated memory, or NULL on failure  
**Alignment**: Must be power of 2  
**Use Cases**: SIMD vectors, page-aligned buffers, cache-line optimization  

**Example C++ Implementation**:
```cpp
extern "C" {
    void* aria_alloc_aligned(int64_t size, int64_t alignment) {
        if (size <= 0 || alignment <= 0) return nullptr;
        
        #ifdef _WIN32
            return _aligned_malloc(size, alignment);
        #else
            void* ptr = nullptr;
            if (posix_memalign(&ptr, alignment, size) != 0) {
                return nullptr;
            }
            return ptr;
        #endif
    }
}
```

---

### 2.3 Array Allocation

```c
void* aria_alloc_array(int64_t elem_size, int64_t count);
```

**Purpose**: Allocate array of `count` elements, each `elem_size` bytes  
**Returns**: Pointer to allocated memory, or NULL on failure  
**Safety**: Checks for integer overflow in `elem_size * count`  

**Example C++ Implementation**:
```cpp
extern "C" {
    void* aria_alloc_array(int64_t elem_size, int64_t count) {
        if (elem_size <= 0 || count <= 0) return nullptr;
        
        // Check for overflow
        if (count > INT64_MAX / elem_size) {
            return nullptr;  // Would overflow
        }
        
        int64_t total_size = elem_size * count;
        return aria_alloc(total_size);
    }
}
```

---

### 2.4 Deallocation

```c
void aria_free(void* ptr);
```

**Purpose**: Free memory allocated by aria_alloc/aria_alloc_aligned/aria_alloc_array  
**Parameters**: Pointer to free (can be NULL - no-op)  
**Implementation**: Wrapper around `free(ptr)` with platform-specific handling for aligned memory  

**Example C++ Implementation**:
```cpp
extern "C" {
    void aria_free(void* ptr) {
        if (!ptr) return;  // NULL is safe to free
        
        #ifdef _WIN32
            // On Windows, check if this was aligned_malloc
            // For simplicity, we'll use free() and assume proper pairing
            free(ptr);
        #else
            free(ptr);
        #endif
    }
}
```

**Note**: Aligned allocations on Windows require `_aligned_free()`. For Phase 4.1, we'll document this limitation and address in Phase 4.2 with tracking.

---

## 3. Aria Language Wrappers (Phase 4.1)

Generic, type-safe wrappers implemented in `stdlib/core/memory.aria`.

### 3.1 Generic Allocation

```aria
func:alloc<T> = wild T@() {
    wild void@:raw_ptr = extern("aria_alloc", sizeof(T));
    wild T@:typed_ptr = cast<wild T@>(raw_ptr);
    return typed_ptr;
}
```

**Usage**:
```aria
wild int64@:num = alloc<int64>();
```

**Type Safety**: Returns `wild T@` instead of `void@`  
**Size Calculation**: Uses compile-time `sizeof(T)`

---

### 3.2 Array Allocation

```aria
func:alloc_array<T> = wild T@(count: int64) {
    wild void@:raw_ptr = extern("aria_alloc_array", sizeof(T), count);
    wild T@:typed_ptr = cast<wild T@>(raw_ptr);
    return typed_ptr;
}
```

**Usage**:
```aria
wild int64@:numbers = alloc_array<int64>(100);
// Access: (numbers + i)@ or numbers[i] (if indexing syntax exists)
```

**Overflow Protection**: Runtime function checks `sizeof(T) * count` for overflow

---

### 3.3 Deallocation

```aria
func:free<T> = void(ptr: wild T@) {
    wild void@:raw_ptr = cast<wild void@>(ptr);
    extern("aria_free", raw_ptr);
}
```

**Usage**:
```aria
wild int64@:ptr = alloc<int64>();
defer free<int64>(ptr);  // Automatic cleanup at scope exit
```

**Defer Integration**: Works seamlessly with existing defer keyword

---

## 4. Error Handling (Phase 4.1)

**Strategy**: Return NULL on failure (C-style)

**Rationale**:
- Simple to implement
- Consistent with C/C++ conventions
- Easy to test with NULL checks

**Usage Pattern**:
```aria
wild int64@:ptr = alloc<int64>();
if (ptr == null) {
    // Handle allocation failure
    return error("Out of memory");
}
defer free<int64>(ptr);

// Use ptr safely...
```

**Future Enhancement (Phase 4.2+)**: Upgrade to `result<wild T@, error>` for richer error information.

---

## 5. Platform-Specific Considerations

### 5.1 Alignment

| Platform | Function | Notes |
|----------|----------|-------|
| Linux/macOS | `posix_memalign()` | POSIX standard |
| Windows | `_aligned_malloc()` | MSVC-specific |
| Windows (MinGW) | `aligned_alloc()` | C11 standard |

**Phase 4.1 Approach**: Use preprocessor directives (`#ifdef _WIN32`) to select correct function.

### 5.2 NULL Handling

All platforms: `free(NULL)` is safe (no-op).  
Aria mirrors this: `aria_free(NULL)` is safe.

### 5.3 Size Limits

- **32-bit systems**: Maximum allocation ~2-4 GB
- **64-bit systems**: Limited by available memory
- **Overflow protection**: `aria_alloc_array` checks `elem_size * count`

---

## 6. Integration with Existing Systems

### 6.1 IR Generation

**Already implemented** (src/backend/ir/codegen_stmt.cpp):
```cpp
if (stmt->isWild) {
    llvm::Function* wild_alloc = getOrDeclareWildAlloc();
    uint64_t type_size = data_layout.getTypeAllocSize(var_type);
    llvm::Value* size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), type_size);
    llvm::Value* raw_ptr = builder.CreateCall(wild_alloc, {size});
    var_ptr = builder.CreateBitCast(raw_ptr, llvm::PointerType::get(var_type, 0));
}
```

**What's missing**: The actual runtime functions (`aria_alloc`, etc.)

### 6.2 Borrow Checker

**Existing tracking** (include/frontend/sema/borrow_checker.h):
- `pending_wild_frees`: Tracks allocations needing cleanup
- `wild_states`: Tracks allocated/freed/moved states
- Methods: `recordWildAlloc()`, `recordWildFree()`, `checkUseAfterFree()`

**Phase 4.1 Integration**: Runtime functions will be called by code the borrow checker validates.

### 6.3 Type System

**Existing** (include/frontend/sema/type.h):
```cpp
class PointerType : public Type {
    Type* pointeeType;
    bool isMutable;
    bool isWild;  // <-- Identifies wild pointers
    
public:
    bool isWildPointer() const { return isWild; }
};
```

**Integration**: Functions return `wild T@` types.

---

## 7. Testing Strategy (Phase 4.1)

### 7.1 Unit Tests (C++ Runtime)

```cpp
// tests/runtime/test_memory.cpp
TEST(Memory, BasicAllocation) {
    void* ptr = aria_alloc(1024);
    ASSERT_NE(ptr, nullptr);
    aria_free(ptr);
}

TEST(Memory, AlignedAllocation) {
    void* ptr = aria_alloc_aligned(1024, 64);
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(ptr) % 64, 0);  // Check alignment
    aria_free(ptr);
}

TEST(Memory, ArrayOverflow) {
    void* ptr = aria_alloc_array(INT64_MAX, 2);  // Should overflow
    ASSERT_EQ(ptr, nullptr);
}
```

### 7.2 Integration Tests (Aria)

```aria
// tests/stdlib/memory_basic.aria
use core.memory;

func:main = int64() {
    // Test 1: Basic allocation
    wild int64@:num = alloc<int64>();
    if (num == null) { return 1; }
    
    num@ = 42;
    if (num@ != 42) { return 2; }
    
    free<int64>(num);
    
    return 0;  // Success
}
```

```aria
// tests/stdlib/memory_array.aria
use core.memory;

func:main = int64() {
    wild int64@:arr = alloc_array<int64>(10);
    if (arr == null) { return 1; }
    defer free<int64>(arr);
    
    // Fill array
    int64:i = 0;
    loop {
        if (i >= 10) { break; }
        (arr + i)@ = i * 2;
        i = i + 1;
    }
    
    // Verify
    if ((arr + 5)@ != 10) { return 2; }
    
    return 0;
}
```

### 7.3 Memory Leak Tests (Valgrind)

```bash
# Compile test
./build/ariac tests/stdlib/memory_leak_test.aria -o test_leak

# Run with Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./test_leak

# Expected output:
# ==12345== HEAP SUMMARY:
# ==12345==     in use at exit: 0 bytes in 0 blocks
# ==12345==   total heap usage: 100 allocs, 100 frees, X bytes allocated
# ==12345== All heap blocks were freed -- no leaks are possible
```

---

## 8. Implementation Checklist

- [ ] **Stage 2.2**: Create `include/runtime/memory.h`
- [ ] **Stage 2.2**: Create `src/runtime/memory.cpp`
- [ ] **Stage 2.2**: Implement `aria_alloc()`
- [ ] **Stage 2.2**: Implement `aria_alloc_aligned()`
- [ ] **Stage 2.2**: Implement `aria_alloc_array()`
- [ ] **Stage 2.2**: Implement `aria_free()`
- [ ] **Stage 2.2**: Add to CMakeLists.txt
- [ ] **Stage 2.2**: Verify compilation
- [ ] **Stage 2.3**: Register functions in IR generator (already done)
- [ ] **Stage 3.1**: Create `stdlib/core/` directory
- [ ] **Stage 3.2**: Create `stdlib/core/memory.aria`
- [ ] **Stage 3.2**: Implement Aria wrappers
- [ ] **Stage 3.3**: Implement/find `sizeof` operator
- [ ] **Stage 4**: Create and run all tests
- [ ] **Stage 6**: Update documentation

---

## 9. Future Enhancements (Not Phase 4.1)

### 9.1 Result-Based Error Handling
```aria
func:alloc<T> = result<wild T@, error>() {
    // Returns detailed error information
}
```

### 9.2 Debug Tracking
```cpp
#ifdef ARIA_DEBUG
    std::unordered_map<void*, AllocationInfo> g_allocations;
    void aria_dump_allocations();
#endif
```

### 9.3 Custom Allocators
```aria
arena:allocator = Arena.new(1024 * 1024);  // 1MB arena
wild T@:ptr = arena.alloc<T>();
// All freed when arena is destroyed
```

### 9.4 Specialized Allocators
```aria
buffer:b = alloc_buffer(size);  // Buffer type (Phase 4.2)
string:s = alloc_string(capacity);  // String type (Phase 4.2)
```

---

## 10. Deferred Features

❌ **GC Allocation**: `aria_gc_alloc()` already exists (separate system)  
❌ **Executable Memory**: `aria_alloc_exec()` for JIT (Phase 5+)  
❌ **TBB Integration**: TBB-sized allocations (Phase 5+)  
❌ **W^X Compliance**: Executable protection transitions (Phase 5+)

---

## 11. References

- **Research**: `docs/gemini/responses/research_031_essential_stdlib.txt`
- **Wild Pointers**: `docs/gemini/responses/research_022_wild_wildx_memory.txt`
- **Type System**: `include/frontend/sema/type.h`
- **IR Generation**: `src/backend/ir/codegen_stmt.cpp`
- **Borrow Checker**: `include/frontend/sema/borrow_checker.h`

---

**Document Status**: ✅ Complete  
**Ready for**: Stage 2 Implementation  
**Estimated Time**: 10-13 hours total (all stages)
