# Vec<T> Implementation Plan

**Status**: Planning Phase  
**Date**: February 16, 2026  
**Priority**: P0 CRITICAL (Required for Nikola 0.1.0)  
**Estimated Time**: 4-6 hours

## Overview

Implement `Vec<T>` generic dynamic array type for Aria. **Runtime already exists** - we're creating language-level wrapper around existing `AriaArray` C implementation.

## Existing Infrastructure ✅

**C Runtime** (`src/runtime/collections/collections.cpp`):
- `AriaArray` struct with capacity/length tracking
- `aria_array_new(element_size, capacity, type_id)` - creation
- `aria_array_push(array, value)` - add element (auto-growth 1.5x)
- `aria_array_pop(array, out_value)` - remove last
- `aria_array_get(array, index)` - bounds-checked access
- `aria_array_set(array, index, value)` - bounds-checked assignment
- `aria_array_length(array)` - get size
- `aria_array_slice(array, start, end)` - create subarray
- GC integration built-in
- Result<T> error handling

**What We Need to Build**:
- Generic type `Vec<T>` in type system
- Constructor methods
- Method call syntax sugar
- LLVM codegen for method calls
- Integration with existing runtime

## Nikola Use Cases

From `NIKOLA_REQUIREMENTS_AUDIT.md`:

```cpp
// 1. Dynamic computation graph
std::vector<ComputeNode> tape;

// 2. Gradient checkpointing
std::vector<Checkpoint> checkpoints;
checkpoints.push_back(std::move(cp));

// 3. File processing
std::vector<std::filesystem::path> extract_recursive(...);

// 4. Text chunking
std::vector<std::string> words = tokenize(full_text);
std::vector<Chunk> chunks;
chunks.push_back(Chunk{chunk_str, chunk_idx++, 0});

// 5. Binary data
std::vector<uint8_t> preserved_state;
```

**Required Operations**:
- `Vec::new()` - create empty vector
- `Vec::with_capacity(n)` - preallocate capacity
- `.push(value)` - add element
- `.pop()` - remove last element (returns Option<T>)
- `[index]` - access by index
- `.len()` - get size
- `.capacity()` - get allocated capacity
- `.clear()` - remove all elements

## Implementation Phases

### Phase 1: Type System (2 hours)

**1.1 Add Generic Type**
- File: `src/frontend/sema/type.cpp`
- Add `Vec` as builtin generic type alongside `atomic`, `Result`, etc.
- Support `Vec<int32>`, `Vec<flt64>`, `Vec<MyStruct>`, etc.

**1.2 Parser Support**
- File: `src/frontend/parser/parser.cpp`
- Recognize `Vec<T>` syntax in type annotations
- Handle `Vec<T>:name = Vec::new();` syntax

**1.3 Type Checking**
- File: `src/frontend/sema/type_checker.cpp`
- Validate generic type parameters
- Ensure type safety for method calls

### Phase 2: Syntax & Methods (2 hours)

**2.1 Constructor Syntax**
```aria
// Static method call syntax
Vec<int32>:numbers = Vec::new();
Vec<flt64>:buffer = Vec::with_capacity(1024);
```

**2.2 Method Call Syntax** 
```aria
numbers.push(42);
numbers.push(100);

int32:last = numbers.pop()?;  // Returns Option<int32>
int32:value = numbers[0];      // Indexing
int64:size = numbers.len();
```

**2.3 UFCS Integration**
- Use existing UFCS (Uniform Function Call Syntax) infrastructure
- Methods become function calls: `numbers.push(42)` → `Vec_push(&numbers, 42)`

### Phase 3: CodeGen (1.5 hours)

**3.1 LLVM Type Mapping**
```cpp
// Vec<T> becomes pointer to AriaArray struct
Vec<int32> → %struct.AriaArray*

struct.AriaArray {
    void* data;      // i8*
    size_t length;   // i64
    size_t capacity; // i64
    size_t element_size; // i64
    int type_id;     // i32
}
```

**3.2 Method Codegen**
- File: `src/backend/ir/codegen_expr.cpp`
- Generate LLVM calls to runtime functions:
  - `Vec::new()` → `call @aria_array_new(sizeof(T), 0, type_id)`
  - `.push(value)` → `call @aria_array_push(vec, &value)`
  - `[index]` → `call @aria_array_get_unchecked(vec, index)` + load
  
**3.3 Runtime Function Declarations**
- File: `src/backend/ir/ir_generator.cpp`
- Declare external functions:
```cpp
llvm::Function* getOrDeclareArrayNew();
llvm::Function* getOrDeclareArrayPush();
llvm::Function* getOrDeclareArrayGet();
// etc.
```

### Phase 4: Testing (0.5 hours)

**Test File**: `tests/test_vec_basic.aria`

```aria
func:test_vec_basic = int32() {
    // Create empty vector
    Vec<int32>:numbers = Vec::new();
    
    // Push elements
    numbers.push(10);
    numbers.push(20);
    numbers.push(30);
    
    // Check length
    if (numbers.len() != 3) {
        pass(1);  // FAIL
    }
    
    // Check indexing
    if (numbers[0] != 10) { pass(2); }
    if (numbers[1] != 20) { pass(3); }
    if (numbers[2] != 30) { pass(4); }
    
    // Pop element
    int32:last = numbers.pop_unchecked();
    if (last != 30) { pass(5); }
    if (numbers.len() != 2) { pass(6); }
    
    pass(0);  // SUCCESS
};
```

**Advanced Test**: `tests/test_vec_nikola.aria`
```aria
// Mimics Nikola's ComputeNode tape usage
struct:ComputeNode = {
    flt64:value,
    flt64:gradient,
    int32:op_type
};

func:test_autodiff_tape = int32() {
    Vec<ComputeNode>:tape = Vec::with_capacity(1000);
    
    // Add nodes
    ComputeNode:node1 = { value: 3.14, gradient: 0.0, op_type: 1 };
    tape.push(node1);
    
    ComputeNode:node2 = { value: 2.71, gradient: 0.0, op_type: 2 };
    tape.push(node2);
    
    // Access nodes
    ComputeNode:first = tape[0];
    if (first.value != 3.14) { pass(1); }
    
    pass(0);
};
```

## Design Decisions

### 1. Error Handling Strategy

**Option A: Panic on bounds errors** (Rust-like)
```aria
numbers[100];  // Runtime panic if out of bounds
```

**Option B: Return Result<T>** (safe but verbose)
```aria
Result<int32>:value = numbers.get(100);
```

**Recommendation**: 
- Use `[]` for unchecked access (panic on error)
- Provide `.get(index)` for Result-based safe access
- Matches Rust's `vec[i]` vs `vec.get(i)` pattern

### 2. Pop Semantics

**Option A: Return Option<T>**
```aria
Option<int32>:value = numbers.pop();
if (value.is_some()) { ... }
```

**Option B: Panic on empty**
```aria
int32:value = numbers.pop();  // Panics if empty
```

**Recommendation**: 
- Provide both: `.pop_unchecked()` (panics) and `.pop()` (returns Result)
- Start with unchecked version for simplicity

### 3. Memory Management

**Decision**: Use GC (already integrated in runtime)
- `aria_array_new()` uses `aria_gc_alloc()`
- Automatic cleanup when Vec goes out of scope
- No manual `free()` needed
- Growth creates new allocation, GC cleans old one

### 4. Generic Type Representation

```aria
// User syntax
Vec<int32>:numbers = Vec::new();

// AST representation
TypeNode {
    name: "Vec",
    generic_args: [TypeNode { name: "int32" }]
}

// LLVM representation
%struct.AriaArray* (all Vec<T> have same struct type, differ in element_size)
```

## Integration Points

### Existing Systems to Leverage

1. **Atomic<T> Pattern** - Already have generic types working
2. **UFCS** - Uniform Function Call Syntax for methods
3. **Result<T>** - Error handling infrastructure exists
4. **GC** - Memory management infrastructure exists
5. **Type System** - Generic type infrastructure exists

### Files to Modify

1. `include/frontend/sema/type.h` - Add VecType class
2. `src/frontend/sema/type.cpp` - Implement VecType
3. `src/frontend/parser/parser.cpp` - Parse Vec<T> syntax
4. `src/frontend/sema/type_checker.cpp` - Type check Vec operations
5. `src/backend/ir/ir_generator.cpp` - Declare runtime functions
6. `src/backend/ir/codegen_expr.cpp` - Generate method calls
7. `tests/test_vec_*.aria` - Test files

## Success Criteria

- [x] Runtime functions exist (already done!)
- [ ] Parse `Vec<T>` type syntax
- [ ] Type check Vec creation and methods
- [ ] Codegen for Vec::new() 
- [ ] Codegen for .push() method
- [ ] Codegen for [index] access
- [ ] Codegen for .len() method
- [ ] Test: Create vec, push, index, len
- [ ] Test: Nikola-style usage with structs

## Estimated Timeline

- Phase 1 (Type System): 2 hours
- Phase 2 (Syntax): 2 hours  
- Phase 3 (CodeGen): 1.5 hours
- Phase 4 (Testing): 0.5 hours

**Total**: 6 hours

## Notes

The fact that runtime is already implemented saves us ~8 hours of work. We're basically just creating a nice type-safe interface to existing functionality.

Similar to how we wrapped LLVM's atomics with `atomic<T>`, we're wrapping `AriaArray` with `Vec<T>`.
