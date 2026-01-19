# Aria Type System - Technical Specification

**Document Type**: Technical Specification  
**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Core Design (Implementation in Progress)

---

## Table of Contents

1. [Overview](#overview)
2. [Primitive Types](#primitive-types)
3. [Restricted Bounds (TBB)](#restricted-bounds-tbb)
4. [Result Types](#result-types)
5. [Wild Memory](#wild-memory)
6. [Generic Types](#generic-types)
7. [Type Inference](#type-inference)
8. [Ownership & Lifetimes](#ownership--lifetimes)
9. [Integration with 6-Stream I/O](#integration-with-6-stream-io)

---

## Overview

The Aria type system is designed around **safety**, **correctness**, and **control**:

| Feature | Purpose | Benefit |
|---------|---------|---------|
| **Primitives** | Explicit sizes (i8, u32, f64) | Cross-platform consistency |
| **TBB (Restricted Bounds)** | Range-limited integers | Overflow protection |
| **Result<T>** | Explicit error handling | No null, no exceptions |
| **Wild Memory** | Unmanaged pointers | Low-level control when needed |
| **Generics** | Type-safe code reuse | Zero-cost abstractions |
| **Inference** | Type deduction | Reduced boilerplate |
| **Ownership** | Memory safety | No use-after-free |

---

## Primitive Types

### Integers (Fixed-Width)

| Type | Size | Range | Use Case |
|------|------|-------|----------|
| `i8` | 1 byte | -128 to 127 | Signed byte |
| `u8` | 1 byte | 0 to 255 | Unsigned byte, raw data |
| `i16` | 2 bytes | -32,768 to 32,767 | Small signed values |
| `u16` | 2 bytes | 0 to 65,535 | Small unsigned values |
| `i32` | 4 bytes | -2^31 to 2^31-1 | Default signed integer |
| `u32` | 4 bytes | 0 to 2^32-1 | Default unsigned integer |
| `i64` | 8 bytes | -2^63 to 2^63-1 | Large signed values |
| `u64` | 8 bytes | 0 to 2^64-1 | Large unsigned values, pointers |

**No Implicit Conversions**:
```aria
var:x = i32(10);
var:y = i64(x);  // Explicit conversion required
```

**Rationale**: Prevent accidental truncation or sign extension

---

### Floating-Point

| Type | Size | Precision | Use Case |
|------|------|-----------|----------|
| `f32` | 4 bytes | ~7 decimal digits | Graphics, speed > precision |
| `f64` | 8 bytes | ~15 decimal digits | Scientific computing |

**No `float`/`double`**: Use explicit sizes

---

### Boolean

| Type | Size | Values | Use Case |
|------|------|--------|----------|
| `bool` | 1 byte | `true`, `false` | Conditionals |

**No Truthy Values**: Only `true` and `false` are valid in conditions

```aria
var:x = i32(10);
if (x) { ... }  // ERROR: i32 is not bool

if (x > 0) { ... }  // OK: comparison produces bool
```

---

### Character & String

| Type | Size | Encoding | Use Case |
|------|------|----------|----------|
| `char` | 4 bytes | Unicode code point | Single character |
| `string` | Variable | UTF-8 | Text data |

**String Internals**:
```c
// Runtime representation (heap-allocated)
struct string {
    char* data;
    size_t len;
    size_t capacity;
};
```

**Immutable by Default**:
```aria
var:s = string("Hello");
s = s + " World";  // Creates new string, old one freed
```

---

## Restricted Bounds (TBB)

### Syntax

```aria
var:age = i32[0..150];  // Age must be 0-150
var:grade = i32[0..100];  // Grade must be 0-100
var:day = i32[1..31];  // Day of month
```

**Generic Syntax**: `base_type[lower..upper]`

---

### Compile-Time Verification

**Safe Operations** (compiler proves bounds):
```aria
var:x = i32[0..10];
x = i32(5);  // OK: 5 is in [0, 10]

var:y = i32[0..20];
y = i32(x);  // OK: [0, 10] ⊆ [0, 20]
```

**Unsafe Operations** (runtime check injected):
```aria
var:z = i32(user_input());
var:bounded = i32[0..100];
bounded = i32(z);  // Runtime check: if (z < 0 || z > 100) ERR
```

---

### Runtime Behavior

**Overflow Sentinel**: `ERR` (constant defined in runtime)

```c
// Runtime (C code)
#define TBB_ERR INT32_MIN  // -2147483648 for i32

int32_t tbb_assign(int32_t value, int32_t lower, int32_t upper) {
    if (value < lower || value > upper) {
        return TBB_ERR;
    }
    return value;
}
```

**Arithmetic**:
```aria
var:x = i32[0..10];
x = i32(5);
x = i32(x + 3);  // x = 8 (OK)
x = i32(x + 10);  // x = ERR (overflow)
```

**Checking for Errors**:
```aria
var:x = i32[0..10];
x = i32(user_input());

if (x == TBB_ERR) {
    io.stderr.write("Input out of range!\n");
}
```

**Compiler Warning**: Unchecked TBB values

```aria
var:x = i32[0..10];
x = i32(user_input());
// WARNING: Variable 'x' may be ERR but is never checked
```

---

### Use Cases

**1. Array Indexing**:
```aria
var:arr = array<i32>(100);
var:index = i32[0..99];
index = i32(50);
arr[index] = 42;  // No bounds check needed (compiler proves safety)
```

**2. Configuration Values**:
```aria
var:volume = i32[0..100];  // Volume percentage
var:brightness = i32[0..255];  // RGB value
```

**3. Dates & Times**:
```aria
var:month = i32[1..12];
var:day = i32[1..31];
var:hour = i32[0..23];
var:minute = i32[0..59];
```

---

## Result Types

### Motivation

**Traditional Error Handling**:
```c
// C code: Magic values
int divide(int a, int b) {
    if (b == 0) return -1;  // What if -1 is valid?
    return a / b;
}

// Exceptions (C++/Java)
int divide(int a, int b) {
    if (b == 0) throw std::runtime_error("Division by zero");
    return a / b;
}
```

**Problems**:
1. Magic values: Ambiguous (-1 could be valid result)
2. Exceptions: Hidden control flow, performance cost, panic in some languages
3. Null pointers: Billion-dollar mistake

---

### Result<T> Design

**Syntax**: `result<T>` or `result<T, E>` (with custom error type)

**Default**: `result<T>` = `result<T, string>` (error is string)

**Example**:
```aria
func:divide = result<i32>(a: i32, b: i32) {
    if (b == 0) {
        err("Division by zero");
    }
    pass(a / b);
}
```

**Usage**:
```aria
result<i32>:r = divide(10, 0);

if (r.is_err()) {
    io.stderr.write("Error: " + r.err() + "\n");
} else {
    io.stdout.write("Result: " + r.ok() + "\n");
}
```

---

### Internal Representation

**Runtime Layout** (tagged union):
```c
// Generated C code
struct result_i32 {
    bool is_ok;
    union {
        int32_t ok_value;
        struct {
            char* err_msg;
            size_t err_len;
        } err_value;
    } data;
};
```

**Constructor Functions**:
```c
result_i32 result_ok_i32(int32_t value) {
    return (result_i32){ .is_ok = true, .data.ok_value = value };
}

result_i32 result_err_i32(const char* msg, size_t len) {
    return (result_i32){ .is_ok = false, .data.err_value = { .err_msg = strdup(msg), .err_len = len } };
}
```

---

### Compiler Checks

**Must Check Before Use**:
```aria
result<i32>:r = divide(10, 2);
io.stdout.write(r.ok());  // ERROR: Must check is_ok() first
```

**Correct**:
```aria
result<i32>:r = divide(10, 2);
if (r.is_ok()) {
    io.stdout.write(r.ok());  // OK: Checked
} else {
    io.stderr.write(r.err());
}
```

**Question Mark Operator** (propagate errors):
```aria
func:compute = result<i32>(x: i32, y: i32) {
    result<i32>:r1 = divide(x, y)?;  // If err, return early
    result<i32>:r2 = divide(r1, 2)?;
    pass(r2);
}
```

**Desugared**:
```aria
func:compute = result<i32>(x: i32, y: i32) {
    result<i32>:r1 = divide(x, y);
    if (r1.is_err()) err(r1.err());  // Early return
    
    result<i32>:r2 = divide(r1.ok(), 2);
    if (r2.is_err()) err(r2.err());
    
    pass(r2.ok());
}
```

---

## Wild Memory

### Motivation

**Memory Management Spectrum**:
1. **Garbage Collection**: Automatic, but unpredictable pauses (Java, Python)
2. **Ownership/RAII**: Safe, but restrictive (Rust)
3. **Manual Management**: Unsafe, but full control (C, C++)

**Aria's Approach**: **Hybrid**

- **Default**: Automatic (reference counting, arena allocation)
- **Escape Hatch**: `wild` pointers (manual management)

---

### Wild Pointer Syntax

```aria
wild i32*:ptr = alloc<i32>();
*ptr = 42;
free(ptr);
```

**Type**: `wild T*` (mutable raw pointer)

**Semantics**:
- No automatic deallocation
- No borrow checking
- No lifetime tracking
- **You are responsible for correctness**

---

### Use Cases

**1. Low-Level Data Structures**:
```aria
struct:node = {
    value: i32,
    wild node*:next,  // Linked list node
};

func:create_list = wild node*() {
    wild node*:head = alloc<node>();
    head->value = 1;
    head->next = alloc<node>();
    head->next->value = 2;
    head->next->next = null;
    pass(head);
}
```

**2. FFI (Foreign Function Interface)**:
```aria
extern func:malloc = wild byte*(size: u64);
extern func:free = void(ptr: wild byte*);

wild byte*:buffer = malloc(1024);
// Use buffer...
free(buffer);
```

**3. Arena Allocators**:
```aria
struct:arena = {
    wild byte*:memory,
    size: u64,
    offset: u64,
};

func:arena_alloc = wild byte*(a: wild arena*, size: u64) {
    if (a->offset + size > a->size) {
        pass(null);
    }
    wild byte*:ptr = a->memory + a->offset;
    a->offset = a->offset + size;
    pass(ptr);
}
```

---

### Safety Considerations

**Compiler Warnings**:
- Warn on wild pointer dereference without null check
- Warn on potential use-after-free (basic analysis)

**Runtime Checks** (debug builds):
- Null pointer checks
- Allocator tracking (detect double-free)

**Best Practices**:
1. Use safe types by default (`string`, `array<T>`, `result<T>`)
2. Only use `wild` when necessary (FFI, performance-critical code)
3. Encapsulate `wild` pointers in safe abstractions

---

## Generic Types

### Syntax

**Function Generics**:
```aria
func:identity = <T>(x: T) -> T {
    pass(x);
}

var:x = identity<i32>(42);
var:y = identity<string>("hello");
```

**Type Inference**:
```aria
var:x = identity(42);  // T inferred as i32
```

---

### Struct Generics

```aria
struct:pair<T, U> = {
    first: T,
    second: U,
};

var:p = pair<i32, string> { first: 42, second: "answer" };
```

---

### Array Generic

**Built-in Type**: `array<T>`

```aria
var:numbers = array<i32>(10);  // Array of 10 i32s
numbers[0] = 42;
```

**Internal Representation**:
```c
// Runtime (C code)
struct array_i32 {
    int32_t* data;
    size_t len;
    size_t capacity;
};
```

---

### Monomorphization vs Type Erasure

**Current Design**: **Monomorphization** (like C++ templates, Rust)

**Behavior**:
- Each instantiation creates a separate function
- `identity<i32>` and `identity<string>` are different compiled functions

**Benefits**:
- Zero runtime cost (no vtables, no boxing)
- Full optimization per type

**Drawback**:
- Code size (each instantiation increases binary size)

**Alternative** (future): Type erasure for interfaces (like Go, Java generics)

**Research Needed**: See Gemini task `language_core_research_task.txt`

---

## Type Inference

### Local Variables

**Explicit Type**:
```aria
var:x = i32(10);
```

**Inferred Type**:
```aria
var:x = 10;  // Type inferred as i32 (default integer)
var:y = 3.14;  // Type inferred as f64 (default float)
var:s = "hello";  // Type inferred as string
```

---

### Function Return Types

**Explicit**:
```aria
func:add = i32(a: i32, b: i32) {
    pass(a + b);
}
```

**Inferred** (future enhancement):
```aria
func:add = (a: i32, b: i32) {
    pass(a + b);  // Return type inferred as i32
}
```

**Limitations**:
- Must be unambiguous (single return type)
- Recursive functions need explicit types

---

### Generic Inference

**Explicit**:
```aria
var:x = identity<i32>(42);
```

**Inferred**:
```aria
var:x = identity(42);  // T inferred as i32 from argument
```

---

## Ownership & Lifetimes

### Motivation

**Problem**: Use-after-free, double-free, dangling pointers

**Example (C)**:
```c
int* create() {
    int x = 42;
    return &x;  // Dangling pointer (x destroyed)
}

int* ptr = create();
printf("%d\n", *ptr);  // Undefined behavior
```

---

### Aria Ownership Rules

**Rule 1**: Every value has a single owner

**Rule 2**: When owner goes out of scope, value is destroyed

**Rule 3**: Values can be moved (transfer ownership) or borrowed (temporary access)

---

### Move Semantics

```aria
var:x = string("hello");
var:y = x;  // x moved to y (x is now invalid)

io.stdout.write(y);  // OK
io.stdout.write(x);  // ERROR: x was moved
```

**Rationale**: Prevents double-free

---

### Borrowing (Future Enhancement)

**Immutable Borrow**:
```aria
func:print_len = void(s: &string) {
    io.stdout.write(s.len());
}

var:s = string("hello");
print_len(&s);  // Borrow s (doesn't move)
io.stdout.write(s);  // OK: s still valid
```

**Mutable Borrow**:
```aria
func:append = void(s: &mut string, suffix: string) {
    s.append(suffix);
}

var:s = string("hello");
append(&mut s, " world");
io.stdout.write(s);  // "hello world"
```

**Lifetime Rules**:
- Only one mutable borrow OR multiple immutable borrows at a time
- Borrows must not outlive the owner

---

## Integration with 6-Stream I/O

### Stream Handle Types

**Definition** (in runtime):
```aria
struct:input_stream = {
    fd: i32,
};

struct:output_stream = {
    fd: i32,
};
```

**Global Instances**:
```aria
const:io.stdin = input_stream { fd: 0 };
const:io.stdout = output_stream { fd: 1 };
const:io.stderr = output_stream { fd: 2 };
const:io.stddbg = output_stream { fd: 3 };
const:io.stddati = input_stream { fd: 4 };
const:io.stddato = output_stream { fd: 5 };
```

---

### I/O Methods

**Read (Text)**:
```aria
func:input_stream.read_line = result<string>(self: &input_stream) {
    // Implementation in runtime
}
```

**Write (Text)**:
```aria
func:output_stream.write = result<void>(self: &output_stream, text: string) {
    // Implementation in runtime
}
```

**Read (Binary)**:
```aria
func:input_stream.read = result<size>(self: &input_stream, buffer: wild byte*, count: size) {
    // Implementation in runtime
}
```

**Write (Binary)**:
```aria
func:output_stream.write = result<size>(self: &output_stream, data: wild byte*, count: size) {
    // Implementation in runtime
}
```

---

### Example: Type-Safe I/O

```aria
func:main = i64() {
    result<string>:line = io.stdin.read_line();
    
    if (line.is_err()) {
        io.stderr.write("Error reading input: " + line.err() + "\n");
        pass(1);
    }
    
    io.stdout.write("You entered: " + line.ok() + "\n");
    pass(0);
}
```

**Type Safety**:
- `read_line()` returns `result<string>` (can fail)
- Must check `is_err()` before using value
- Compiler enforces checking

---

## Related Documents

- **[ECOSYSTEM_OVERVIEW](../ECOSYSTEM_OVERVIEW.md)**: System-wide design philosophy
- **[ARIA_COMPILER](../components/ARIA_COMPILER.md)**: Type checking implementation
- **[ARIA_RUNTIME](../components/ARIA_RUNTIME.md)**: Runtime type representations
- **[IO_TOPOLOGY](./IO_TOPOLOGY.md)**: 6-stream I/O design
- **[ERROR_HANDLING](./ERROR_HANDLING.md)**: Result<T> patterns

---

**Document Version**: 1.0  
**Author**: Aria Ecosystem Documentation  
**Status**: Core specification (research complete, implementation in progress)

**Research Complete** ✅ (December 22, 2025):
- **Generic monomorphization**: Creates specialized copy for each concrete type
- **Type inference**: Contextual deduction, getLLVMType recursive substitution
- **Ownership tracking**: Appendage Theory (Host/Appendage model)
- **Type substitution**: typeSubstitution map (T → int8)
- **Name mangling**: Unique symbols per specialization (vector_int8_push vs vector_float_push)

**Key Findings** (from language_core_research_task.txt & language_advanced_research_task.txt):
- **Monomorphization**: Zero runtime overhead, LLVM optimizer can inline per-type specializations
- **CodeGenContext**: Maintains typeSubstitution map + currentMangledName for generics
- **Type tracking**: exprTypeMap critical for TBB safety, tracks LLVM Value* → Aria type
- **Fat pointers**: 32 bytes in debug (ptr + base + size + alloc_id) for bounds checking
- **Borrow checker**: Enforces Depth(H) ≤ Depth(A) invariant (Host outlives Appendage)
- **Pinning**: Shadow Stack (1024 roots), prevents GC from moving pinned objects
- **Safe reference ($)**: Creates checked borrow, validated by borrow checker
- **Scope tracking**: scope_depth field in CodeGenContext for lifetime analysis
