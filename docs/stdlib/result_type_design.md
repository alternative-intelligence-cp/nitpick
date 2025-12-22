# Result Type Design for Aria - Phase 4.2

**Version**: 0.1.0  
**Date**: December 21, 2025  
**Status**: Design Phase  

---

## 1. Overview

This document specifies the design of Aria's `result<T, E>` type system for explicit, type-safe error handling. Based on Rust's `Result<T, E>` pattern, Aria's result type provides zero-cost error propagation without exceptions.

**Design Goals:**
- Type-safe error handling enforced by compiler
- Zero runtime overhead on success path
- Rich error context (not just NULL)
- Seamless integration with `?` operator
- Platform-portable error representation

---

## 2. Core Result Type

### 2.1 Type Definition

```aria
// Generic result type (placeholder syntax until generics parser complete)
type result<T, E> = {
    tag: int32,     // 0 = Ok, 1 = Err
    value: T,       // Valid when tag == 0
    error: E,       // Valid when tag == 1
}
```

**Memory Layout:**
```
Discriminated union:
[tag: 4 bytes] [max(sizeof(T), sizeof(E)) bytes]
```

### 2.2 Construction

```aria
// Success constructor
fn ok<T, E>(value: T) -> result<T, E> {
    return result<T, E> { tag: 0, value: value, error: @uninit };
}

// Error constructor  
fn err<T, E>(error: E) -> result<T, E> {
    return result<T, E> { tag: 1, value: @uninit, error: error };
}
```

### 2.3 Inspection

```aria
// Check if result is Ok
fn is_ok<T, E>(r: result<T, E>) -> bool {
    return r.tag == 0;
}

// Check if result is Err
fn is_err<T, E>(r: result<T, E>) -> bool {
    return r.tag == 1;
}
```

### 2.4 Value Extraction

```aria
// Extract value (panics if Err)
fn unwrap<T, E>(r: result<T, E>) -> T {
    if (r.tag == 1) {
        @extern("abort");  // Panic: unwrap() called on Err
    }
    return r.value;
}

// Extract value or default
fn unwrap_or<T, E>(r: result<T, E>, default: T) -> T {
    if (r.tag == 0) {
        return r.value;
    } else {
        return default;
    }
}

// Extract value or panic with message
fn expect<T, E>(r: result<T, E>, msg: string) -> T {
    if (r.tag == 1) {
        // Print message then abort
        @extern("aria_panic")(msg);
    }
    return r.value;
}

// Extract error (panics if Ok)
fn unwrap_err<T, E>(r: result<T, E>) -> E {
    if (r.tag == 0) {
        @extern("abort");  // Panic: unwrap_err() called on Ok
    }
    return r.error;
}
```

---

## 3. Allocation Error Types

### 3.1 Error Enum

```aria
// Allocation error variants
enum alloc_error {
    OutOfMemory = 1,         // malloc() returned NULL
    InvalidAlignment = 2,     // alignment not power of 2
    InvalidSize = 3,          // size is 0 or overflow
    UnsupportedOperation = 4, // platform limitation
}
```

### 3.2 Error Messages

```aria
fn alloc_error_message(err: alloc_error) -> string {
    match err {
        OutOfMemory => "Out of memory",
        InvalidAlignment => "Invalid alignment (must be power of 2)",
        InvalidSize => "Invalid allocation size",
        UnsupportedOperation => "Operation not supported on this platform",
    }
}
```

### 3.3 Runtime Error Codes

**C++ Runtime:**
```cpp
// Error codes matching Aria enum
#define ARIA_ALLOC_OK 0
#define ARIA_ALLOC_OUT_OF_MEMORY 1
#define ARIA_ALLOC_INVALID_ALIGNMENT 2
#define ARIA_ALLOC_INVALID_SIZE 3
#define ARIA_ALLOC_UNSUPPORTED 4
```

---

## 4. Runtime Integration

### 4.1 C++ Result Structure

```cpp
// Generic result for pointer allocations
typedef struct {
    void* value;          // NULL if error
    int32_t error_code;   // 0 = ok, >0 = error variant
    size_t requested_size;   // For debugging
    size_t requested_align;  // For debugging  
} aria_alloc_result;
```

### 4.2 Runtime Functions

```cpp
// Result-based allocation functions
aria_alloc_result aria_alloc_result(size_t size);
aria_alloc_result aria_alloc_array_result(size_t elem_size, size_t count);
aria_alloc_result aria_alloc_aligned_result(size_t size, size_t alignment);
aria_alloc_result aria_alloc_buffer_result(size_t size);
```

**Implementation Example:**
```cpp
aria_alloc_result aria_alloc_result(size_t size) {
    aria_alloc_result result;
    result.requested_size = size;
    result.requested_align = 0;
    
    if (size == 0) {
        result.value = nullptr;
        result.error_code = ARIA_ALLOC_INVALID_SIZE;
        return result;
    }
    
    void* ptr = malloc(size);
    if (ptr == nullptr) {
        result.value = nullptr;
        result.error_code = ARIA_ALLOC_OUT_OF_MEMORY;
        return result;
    }
    
    result.value = ptr;
    result.error_code = ARIA_ALLOC_OK;
    return result;
}
```

### 4.3 IR Generator Registration

```cpp
// In codegen_stmt.cpp
llvm::Function* CodeGenerator::getOrDeclareAllocResult() {
    auto& context = module_->getContext();
    
    // aria_alloc_result { void*, int32_t, size_t, size_t }
    llvm::Type* resultType = llvm::StructType::get(
        context,
        {
            llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),  // void*
            llvm::Type::getInt32Ty(context),   // error_code
            llvm::Type::getInt64Ty(context),   // requested_size
            llvm::Type::getInt64Ty(context)    // requested_align
        }
    );
    
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        resultType,
        {llvm::Type::getInt64Ty(context)},  // size_t size
        false
    );
    
    return llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        "aria_alloc_result",
        module_
    );
}
```

---

## 5. Aria Wrapper Functions

### 5.1 Result-Based Allocations

```aria
/**
 * Allocate memory for type T (result-based)
 * 
 * Returns result with:
 * - Ok(wild T@): Pointer to allocated memory
 * - Err(alloc_error): Specific error variant
 * 
 * Example:
 *   result := alloc_result<int64>();
 *   if (result.is_ok()) {
 *       ptr := result.unwrap();
 *       ptr@ = 42;
 *       free<int64>(ptr);
 *   } else {
 *       print(alloc_error_message(result.unwrap_err()));
 *   }
 */
fn alloc_result<T>() -> result<wild T@, alloc_error> {
    raw := @extern("aria_alloc_result")(@sizeof(T));
    
    if (raw.error_code != 0) {
        return err<wild T@, alloc_error>(
            @cast<alloc_error>(raw.error_code)
        );
    } else {
        return ok<wild T@, alloc_error>(
            @cast<wild T@>(raw.value)
        );
    }
}

/**
 * Allocate array with result-based error handling
 */
fn alloc_array_result<T>(count: uint64) -> result<wild T@, alloc_error> {
    raw := @extern("aria_alloc_array_result")(@sizeof(T), count);
    
    if (raw.error_code != 0) {
        return err<wild T@, alloc_error>(
            @cast<alloc_error>(raw.error_code)
        );
    } else {
        return ok<wild T@, alloc_error>(
            @cast<wild T@>(raw.value)
        );
    }
}

/**
 * Allocate aligned memory with result-based error handling
 */
fn alloc_aligned_result<T>(alignment: uint64) -> result<wild T@, alloc_error> {
    raw := @extern("aria_alloc_aligned_result")(@sizeof(T), alignment);
    
    if (raw.error_code != 0) {
        return err<wild T@, alloc_error>(
            @cast<alloc_error>(raw.error_code)
        );
    } else {
        return ok<wild T@, alloc_error>(
            @cast<wild T@>(raw.value)
        );
    }
}
```

### 5.2 Usage with ? Operator

```aria
/**
 * Function demonstrating ? operator for error propagation
 */
fn create_buffer(size: uint64) -> result<wild byte@, alloc_error> {
    // ? automatically propagates error or unwraps value
    buffer := alloc_array_result<byte>(size)?;
    
    // Initialize buffer
    for (i in 0..size) {
        buffer[i] = 0;
    }
    
    return ok<wild byte@, alloc_error>(buffer);
}

// Caller handles error
fn main() -> int32 {
    buffer_result := create_buffer(1024);
    
    if (buffer_result.is_err()) {
        print("Failed to create buffer: ");
        print(alloc_error_message(buffer_result.unwrap_err()));
        return 1;
    }
    
    buffer := buffer_result.unwrap();
    defer free<byte>(buffer);
    
    // Use buffer...
    
    return 0;
}
```

---

## 6. Comparison: NULL vs Result

### 6.1 NULL-Based (Phase 4.1)

**Advantages:**
- Simpler API
- Familiar to C programmers
- Direct pointer manipulation
- No result struct overhead

**Disadvantages:**
- No error context (just NULL)
- Easy to forget NULL check
- Cannot distinguish error types

**Use Cases:**
- Simple allocations
- C-interop code
- Performance-critical paths (after validation)

### 6.2 Result-Based (Phase 4.2)

**Advantages:**
- Rich error context
- Type-safe error handling
- ? operator propagation
- Compiler can enforce checks

**Disadvantages:**
- Slightly more verbose
- Result struct overhead (small)
- Requires error type definition

**Use Cases:**
- Library APIs
- Error-prone operations
- User-facing code needing diagnostics
- Functions returning Result

### 6.3 Coexistence Strategy

**Both APIs available:**
```aria
// Simple: NULL-based
ptr1 := alloc<int64>();
if (is_null<int64>(ptr1)) { /* handle */ }

// Advanced: Result-based
ptr2_result := alloc_result<int64>();
if (ptr2_result.is_err()) { /* detailed error */ }

// Propagating: ? operator
fn foo() -> result<void, alloc_error> {
    ptr3 := alloc_result<int64>()?;  // Auto-propagates
    defer free<int64>(ptr3);
    // ...
}
```

**Conversion helpers:**
```aria
// Convert NULL to Result
fn ptr_to_result<T>(ptr: wild T@) -> result<wild T@, alloc_error> {
    if (is_null<T>(ptr)) {
        return err<wild T@, alloc_error>(OutOfMemory);
    } else {
        return ok<wild T@, alloc_error>(ptr);
    }
}
```

---

## 7. Integration with ? Operator

### 7.1 Operator Semantics

**Aria's ? operator** (from existing specs):
```aria
// Original:
value := might_fail()?;

// Desugars to:
temp := might_fail();
if (temp.is_err()) {
    return err<ReturnType, ErrorType>(temp.unwrap_err());
}
value := temp.unwrap();
```

### 7.2 Allocation Example

```aria
fn allocate_and_init() -> result<wild int64@, alloc_error> {
    // ? propagates error if allocation fails
    ptr := alloc_result<int64>()?;
    
    // Initialize
    ptr@ = 42;
    
    // Return success
    return ok<wild int64@, alloc_error>(ptr);
}
```

### 7.3 Chaining Results

```aria
fn create_data_structure() -> result<wild MyStruct@, alloc_error> {
    data := alloc_result<MyStruct>()?;
    data.buffer = alloc_array_result<byte>(1024)?;  // Propagates on failure
    data.name = alloc_array_result<char>(256)?;
    
    // All allocations succeeded
    return ok<wild MyStruct@, alloc_error>(data);
}
```

---

## 8. Platform Considerations

### 8.1 Error Code Mapping

**Linux/macOS (errno):**
```cpp
if (errno == ENOMEM) return ARIA_ALLOC_OUT_OF_MEMORY;
if (errno == EINVAL) return ARIA_ALLOC_INVALID_ALIGNMENT;
```

**Windows (GetLastError):**
```cpp
if (GetLastError() == ERROR_NOT_ENOUGH_MEMORY) 
    return ARIA_ALLOC_OUT_OF_MEMORY;
```

### 8.2 Alignment Validation

```cpp
bool is_power_of_two(size_t alignment) {
    return alignment > 0 && (alignment & (alignment - 1)) == 0;
}

aria_alloc_result aria_alloc_aligned_result(size_t size, size_t alignment) {
    aria_alloc_result result;
    result.requested_size = size;
    result.requested_align = alignment;
    
    // Validate alignment
    if (!is_power_of_two(alignment)) {
        result.value = nullptr;
        result.error_code = ARIA_ALLOC_INVALID_ALIGNMENT;
        return result;
    }
    
    // Platform-specific allocation...
}
```

---

## 9. Testing Strategy

### 9.1 C++ Unit Tests

```cpp
TEST(ResultAlloc, SuccessCase) {
    auto result = aria_alloc_result(64);
    EXPECT_EQ(result.error_code, ARIA_ALLOC_OK);
    EXPECT_NE(result.value, nullptr);
    free(result.value);
}

TEST(ResultAlloc, InvalidSize) {
    auto result = aria_alloc_result(0);
    EXPECT_EQ(result.error_code, ARIA_ALLOC_INVALID_SIZE);
    EXPECT_EQ(result.value, nullptr);
}

TEST(ResultAlloc, InvalidAlignment) {
    auto result = aria_alloc_aligned_result(64, 7);  // Not power of 2
    EXPECT_EQ(result.error_code, ARIA_ALLOC_INVALID_ALIGNMENT);
}
```

### 9.2 Aria Integration Tests

```aria
// Test: Successful allocation with result
fn test_result_alloc_success() {
    result := alloc_result<int64>();
    assert(result.is_ok());
    
    ptr := result.unwrap();
    ptr@ = 123;
    assert(ptr@ == 123);
    
    free<int64>(ptr);
}

// Test: Error propagation with ?
fn helper_alloc() -> result<wild int64@, alloc_error> {
    return alloc_result<int64>();
}

fn test_result_propagation() -> result<void, alloc_error> {
    ptr := helper_alloc()?;  // Propagates error
    defer free<int64>(ptr);
    
    ptr@ = 456;
    assert(ptr@ == 456);
    
    return ok<void, alloc_error>(void);
}
```

---

## 10. Performance Characteristics

### 10.1 Zero-Cost Abstraction

**Result struct size:**
```
result<wild T@, alloc_error> = 
    sizeof(tag) + max(sizeof(T@), sizeof(alloc_error))
  = 4 bytes + max(8 bytes, 4 bytes)
  = 12 bytes (may pad to 16)
```

**Comparison:**
- NULL check: 1 comparison
- Result check: 1 comparison (tag == 0)
- **Same cost on success path**

### 10.2 Compiler Optimizations

**Tag checking:**
```aria
if (result.is_ok()) {
    ptr := result.unwrap();
}
```

**LLVM IR (optimized):**
```llvm
%tag = load i32, i32* %result.tag
%is_ok = icmp eq i32 %tag, 0
br i1 %is_ok, label %ok_block, label %err_block

ok_block:
  %ptr = load i8*, i8** %result.value
  ; ... (no redundant checks, unwrap() optimized away)
```

---

## 11. Known Limitations

1. **Generics Parser**: Using placeholder syntax until parser complete
2. **No Pattern Matching**: `match` statement not yet implemented (use if/else)
3. **Error Context**: No stack traces yet (future enhancement)
4. **Combinators**: No `map()`, `and_then()` yet (Phase 4.3+)

---

## 12. Future Enhancements (Phase 4.3+)

### 12.1 Result Combinators

```aria
fn map<T, E, U>(r: result<T, E>, f: fn(T) -> U) -> result<U, E>;
fn and_then<T, E, U>(r: result<T, E>, f: fn(T) -> result<U, E>) -> result<U, E>;
fn or_else<T, E, F>(r: result<T, E>, f: fn(E) -> result<T, F>) -> result<T, F>;
```

### 12.2 Multiple Error Types

```aria
// Convert between error types
fn convert_error<T, E1, E2>(r: result<T, E1>, f: fn(E1) -> E2) -> result<T, E2>;
```

### 12.3 Error Contexts

```cpp
struct alloc_error_context {
    alloc_error variant;
    const char* message;
    size_t requested_size;
    size_t requested_align;
    const char* file;
    int line;
};
```

---

## 13. Migration Guide

### 13.1 From NULL-Based to Result-Based

**Before (Phase 4.1):**
```aria
ptr := alloc<int64>();
if (is_null<int64>(ptr)) {
    print("Allocation failed");
    return 1;
}
defer free<int64>(ptr);
```

**After (Phase 4.2):**
```aria
ptr_result := alloc_result<int64>();
if (ptr_result.is_err()) {
    print("Allocation failed: ");
    print(alloc_error_message(ptr_result.unwrap_err()));
    return 1;
}
ptr := ptr_result.unwrap();
defer free<int64>(ptr);
```

**Or with ?:**
```aria
fn foo() -> result<int32, alloc_error> {
    ptr := alloc_result<int64>()?;  // Cleaner!
    defer free<int64>(ptr);
    // ...
}
```

### 13.2 When to Use Each

| Use NULL-Based | Use Result-Based |
|----------------|------------------|
| Simple scripts | Library functions |
| C-interop | Error propagation with ? |
| Known-safe contexts | User-facing errors |
| Hot loops (after validation) | Initialization code |

---

## 14. Success Criteria

**Phase 4.2 Result Type Implementation Complete When:**

- [x] Design document finalized
- [ ] `result<T, E>` type implemented (lib/std/core/result.aria)
- [ ] `alloc_error` enum defined (lib/std/core/error.aria)
- [ ] Runtime functions (aria_alloc_result, etc.) implemented
- [ ] IR generator registration complete
- [ ] Aria wrapper functions (alloc_result<T>(), etc.) implemented
- [ ] 15+ C++ unit tests passing
- [ ] 5+ Aria integration tests written
- [ ] Documentation updated
- [ ] ? operator integration verified

---

## 15. References

**External:**
- Rust Result<T, E>: https://doc.rust-lang.org/std/result/
- Zig ErrorSet: https://ziglang.org/documentation/master/#Errors
- C++23 std::expected: https://en.cppreference.com/w/cpp/utility/expected

**Internal:**
- `docs/gemini/responses/resFull.txt` (Line 15459) - Result type architecture
- `lib/std/core/memory.aria` - Phase 4.1 NULL-based allocations
- `docs/stdlib/memory_api.md` - Memory API specification
- `TMP_TODO_PHASE_4_2.md` - Implementation roadmap

---

**Status**: ✅ Stage 1.1 Complete - Result Type Research & Design
