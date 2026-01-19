# Error Handling in Aria - Technical Specification

**Document Type**: Technical Specification  
**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Core Design (Implementation in Progress)

---

## Table of Contents

1. [Overview](#overview)
2. [Result<T> Monad](#resultt-monad)
3. [TBB Overflow Handling](#tbb-overflow-handling)
4. [Error Propagation](#error-propagation)
5. [Best Practices](#best-practices)
6. [Integration with I/O](#integration-with-io)
7. [Compiler Enforcement](#compiler-enforcement)

---

## Overview

Aria's error handling is built on two pillars:

| Mechanism | Use Case | Enforcement |
|-----------|----------|-------------|
| **Result<T>** | Function errors, I/O failures | Compile-time (must check) |
| **TBB Overflow** | Arithmetic errors, range violations | Runtime (ERR sentinel) |

**Philosophy**: **Make errors explicit and unavoidable**

- No hidden control flow (exceptions)
- No ambiguous return values (magic numbers)
- No crashes (null pointer exceptions)
- Compiler forces error checking

---

## Result<T> Monad

### Design Philosophy

**Traditional Approaches**:

**1. Magic Values (C)**:
```c
int divide(int a, int b) {
    if (b == 0) return -1;  // What if -1 is valid result?
    return a / b;
}
```
**Problem**: Ambiguous (-1 could be legitimate result)

**2. Exceptions (C++, Java, Python)**:
```cpp
int divide(int a, int b) {
    if (b == 0) throw std::runtime_error("Division by zero");
    return a / b;
}
```
**Problems**:
- Hidden control flow (can throw anywhere)
- Performance cost (stack unwinding)
- Easy to ignore (no compile-time enforcement)

**3. Null Values (Many languages)**:
```java
String getName() {
    return null;  // Oops!
}
```
**Problem**: Null pointer exceptions (billion-dollar mistake)

---

**Aria's Solution: Result<T>**

```aria
func:divide = result<i32>(a: i32, b: i32) {
    if (b == 0) {
        err("Division by zero");
    }
    pass(a / b);
}
```

**Benefits**:
- **Explicit**: Return type shows function can fail
- **Type-safe**: Compiler prevents unchecked use
- **No exceptions**: Control flow is visible
- **Composable**: Error propagation via `?` operator

---

### Type Definition

**Full Syntax**: `result<T, E>`

- `T`: Type of successful value
- `E`: Type of error value

**Default**: `result<T>` = `result<T, string>`

**Examples**:
```aria
result<i32>:r1 = divide(10, 2);  // Ok(5) or Err(string)
result<i32, error_code>:r2 = parse_number(input);  // Custom error type
result<void>:r3 = write_file(path, data);  // No value on success
```

---

### Internal Representation

**Memory Layout** (tagged union):
```c
// Generated C code for result<i32>
typedef struct {
    bool is_ok;  // Tag: true = Ok, false = Err
    union {
        int32_t ok_value;
        struct {
            char* msg;
            size_t len;
        } err_value;
    } data;
} result_i32;
```

**Size**: `sizeof(result<T>)` = `max(sizeof(T), sizeof(E))` + alignment + tag

**Example**:
- `result<i32>`: 16-24 bytes (4 for i32, 8+8 for string pointer+length, 1 for tag)
- `result<void>`: 8-16 bytes (0 for void, 8+8 for string, 1 for tag)

---

### Constructors

**Ok Variant**:
```aria
func:divide = result<i32>(a: i32, b: i32) {
    pass(a / b);  // Implicitly creates Ok(a / b)
}
```

**Generated Code**:
```c
result_i32 divide(int32_t a, int32_t b) {
    result_i32 ret;
    ret.is_ok = true;
    ret.data.ok_value = a / b;
    return ret;
}
```

**Err Variant**:
```aria
func:divide = result<i32>(a: i32, b: i32) {
    if (b == 0) {
        err("Division by zero");
    }
    // ...
}
```

**Generated Code**:
```c
result_i32 divide(int32_t a, int32_t b) {
    if (b == 0) {
        result_i32 ret;
        ret.is_ok = false;
        const char* msg = "Division by zero";
        ret.data.err_value.msg = strdup(msg);
        ret.data.err_value.len = strlen(msg);
        return ret;
    }
    // ...
}
```

---

### Checking Results

**Method 1: Explicit Check**:
```aria
result<i32>:r = divide(10, 2);

if (r.is_ok()) {
    io.stdout.write("Result: " + r.ok() + "\n");
} else {
    io.stderr.write("Error: " + r.err() + "\n");
}
```

**Method 2: Match Expression** (future enhancement):
```aria
result<i32>:r = divide(10, 2);

match (r) {
    Ok(val) => io.stdout.write("Result: " + val + "\n"),
    Err(msg) => io.stderr.write("Error: " + msg + "\n"),
}
```

---

### Compiler Enforcement

**Error: Unchecked Result**:
```aria
result<i32>:r = divide(10, 2);
io.stdout.write(r.ok());  // ERROR: Must check is_ok() first
```

**Compiler Message**:
```
error: Result<i32> must be checked before calling ok()
  --> file.aria:3:18
   |
3  | io.stdout.write(r.ok());
   |                  ^^^^^^
   | help: Add a check: if (r.is_ok()) { ... }
```

**Correct**:
```aria
result<i32>:r = divide(10, 2);
if (r.is_ok()) {
    io.stdout.write(r.ok());  // OK
}
```

---

## TBB Overflow Handling

### ERR Sentinel

**Definition**: Each TBB type has an ERR value (minimum value of base type)

| Base Type | ERR Value | Decimal |
|-----------|-----------|---------|
| `i8[...]` | -128 | -2^7 |
| `i16[...]` | -32,768 | -2^15 |
| `i32[...]` | -2,147,483,648 | -2^31 |
| `i64[...]` | -9,223,372,036,854,775,808 | -2^63 |

**Rationale**: Use minimum value as sentinel (never valid for positive-only ranges)

---

### Overflow Detection

**Compile-Time Safe** (no overflow possible):
```aria
var:x = i32[0..10];
x = i32(5);  // OK: 5 is in range

var:y = i32[0..20];
y = i32(x);  // OK: [0, 10] ⊆ [0, 20]
```

**Runtime Check Required**:
```aria
var:input = i32(user_input());
var:bounded = i32[0..100];
bounded = i32(input);  // Runtime check inserted
```

**Generated Code**:
```c
int32_t input = get_user_input();
int32_t bounded = tbb_assign_i32(input, 0, 100);

// Runtime function
int32_t tbb_assign_i32(int32_t value, int32_t lower, int32_t upper) {
    if (value < lower || value > upper) {
        return TBB_ERR_I32;  // -2147483648
    }
    return value;
}
```

---

### Checking for ERR

**Method 1: Direct Comparison**:
```aria
var:x = i32[0..100];
x = i32(user_input());

if (x == TBB_ERR) {
    io.stderr.write("Input out of range!\n");
}
```

**Method 2: Helper Function** (future enhancement):
```aria
var:x = i32[0..100];
x = i32(user_input());

if (x.is_err()) {
    io.stderr.write("Input out of range!\n");
}
```

---

### Arithmetic Overflow

**Addition**:
```aria
var:x = i32[0..100];
x = i32(50);
x = i32(x + 60);  // x = ERR (overflow: 110 > 100)
```

**Multiplication**:
```aria
var:y = i32[0..10];
y = i32(5);
y = i32(y * 3);  // y = ERR (overflow: 15 > 10)
```

**Subtraction**:
```aria
var:z = i32[0..100];
z = i32(10);
z = i32(z - 20);  // z = ERR (underflow: -10 < 0)
```

---

### Compiler Warnings

**Warning: Unchecked TBB**:
```aria
var:x = i32[0..100];
x = i32(user_input());
// WARNING: Variable 'x' may be ERR but is never checked

var:y = i32(x + 10);  // Using potentially invalid value
```

**Correct**:
```aria
var:x = i32[0..100];
x = i32(user_input());

if (x == TBB_ERR) {
    io.stderr.write("Invalid input\n");
    pass(1);  // Exit early
}

var:y = i32(x + 10);  // OK: x is guaranteed valid here
```

---

## Error Propagation

### The `?` Operator

**Purpose**: Propagate errors up the call stack

**Syntax**: `expr?`

**Behavior**:
- If `expr` is `Ok(value)`, unwrap and use `value`
- If `expr` is `Err(e)`, return early with `Err(e)`

---

### Example: Manual Propagation

**Without `?`**:
```aria
func:compute = result<i32>(x: i32, y: i32) {
    result<i32>:r1 = divide(x, y);
    if (r1.is_err()) {
        err(r1.err());  // Propagate error
    }
    
    result<i32>:r2 = divide(r1.ok(), 2);
    if (r2.is_err()) {
        err(r2.err());
    }
    
    pass(r2.ok());
}
```

**With `?`**:
```aria
func:compute = result<i32>(x: i32, y: i32) {
    result<i32>:r1 = divide(x, y)?;  // Auto-propagate
    result<i32>:r2 = divide(r1, 2)?;
    pass(r2);
}
```

---

### Desugaring

**Aria Code**:
```aria
result<i32>:r = divide(x, y)?;
```

**Desugared**:
```aria
result<i32>:_tmp = divide(x, y);
if (_tmp.is_err()) {
    err(_tmp.err());  // Early return
}
result<i32>:r = _tmp.ok();
```

---

### Chaining

**Multiple `?` Operators**:
```aria
func:process = result<string>(path: string) {
    result<file>:f = open(path)?;
    result<string>:contents = f.read()?;
    result<string>:processed = transform(contents)?;
    pass(processed);
}
```

**Benefits**:
- Concise error handling
- Clear control flow
- Type-safe (compiler checks return type matches)

---

## Best Practices

### When to Use Result<T>

**Use `result<T>` when**:
1. **I/O Operations**: File reading, network requests, database queries
2. **Parsing**: Converting strings to numbers, JSON parsing
3. **Validation**: Checking user input, configuration validation
4. **External Calls**: FFI, system calls

**Example**:
```aria
func:read_config = result<config>(path: string) {
    result<file>:f = io.open(path)?;
    result<string>:contents = f.read_all()?;
    result<config>:cfg = parse_config(contents)?;
    pass(cfg);
}
```

---

### When to Use TBB

**Use TBB when**:
1. **Bounded Domains**: Ages, percentages, array indices
2. **Configuration**: Port numbers, timeouts, limits
3. **Dates/Times**: Days, months, hours, minutes

**Example**:
```aria
var:port = i32[1..65535];
var:timeout = i32[0..3600];  // Max 1 hour
var:month = i32[1..12];
var:day = i32[1..31];
```

---

### Combining Result<T> and TBB

**Pattern**: Parse input → Validate bounds → Use safely

**Example**:
```aria
func:set_volume = result<void>(input: string) {
    result<i32>:parsed = parse_i32(input)?;
    
    var:volume = i32[0..100];
    volume = i32(parsed);
    
    if (volume == TBB_ERR) {
        err("Volume must be 0-100");
    }
    
    audio.set_volume(volume);  // Safe: volume is guaranteed in range
    pass();
}
```

---

### Error Messages

**Be Specific**:
```aria
// Bad
if (value < 0) {
    err("Invalid");
}

// Good
if (value < 0) {
    err("Port number must be positive, got: " + value);
}
```

**Include Context**:
```aria
func:read_file = result<string>(path: string) {
    result<file>:f = io.open(path);
    if (f.is_err()) {
        err("Failed to open file '" + path + "': " + f.err());
    }
    // ...
}
```

---

## Integration with I/O

### I/O Functions Return Results

**File I/O**:
```aria
result<file>:f = io.open("config.txt");
result<string>:contents = f?.read_all();
```

**Network I/O** (future):
```aria
result<socket>:sock = net.connect("example.com", 80);
result<void>:sent = sock?.send(request);
result<bytes>:response = sock?.recv(1024);
```

---

### Stream I/O

**stdin**:
```aria
result<string>:line = io.stdin.read_line();
if (line.is_err()) {
    io.stderr.write("Failed to read input: " + line.err() + "\n");
}
```

**stddati** (binary):
```aria
wild byte*:buffer = alloc(1024);
result<size>:n = io.stddati.read(buffer, 1024);

if (n.is_err()) {
    io.stderr.write("Read error: " + n.err() + "\n");
} else if (n.ok() == 0) {
    io.stddbg.write("EOF reached\n");
} else {
    io.stddbg.write("Read " + n.ok() + " bytes\n");
}
```

---

## Compiler Enforcement

### Mandatory Checks

**Enforced Rules**:
1. **Cannot call `.ok()` or `.err()` without checking**
2. **Cannot ignore `result<T>` return value**
3. **Cannot use TBB value without checking for ERR** (warning)

---

### Static Analysis

**Flow Analysis**:
```aria
result<i32>:r = divide(10, 2);

if (r.is_ok()) {
    io.stdout.write(r.ok());  // OK: Checked in this branch
} else {
    io.stderr.write(r.ok());  // ERROR: Not checked in this branch
}
```

**Compiler Tracks**:
- Which variables have been checked
- Which branches are safe/unsafe
- Which paths lead to early returns

---

### Warning Levels

**Error (blocks compilation)**:
- Unchecked `.ok()` or `.err()` call
- Ignoring `result<T>` return value

**Warning (still compiles)**:
- TBB value potentially ERR but never checked
- Unused result<T> variable

---

## Related Documents

- **[TYPE_SYSTEM](./TYPE_SYSTEM.md)**: Complete type system specification
- **[IO_TOPOLOGY](./IO_TOPOLOGY.md)**: 6-stream I/O design
- **[ARIA_COMPILER](../components/ARIA_COMPILER.md)**: Compiler error checking
- **[ARIA_RUNTIME](../components/ARIA_RUNTIME.md)**: Runtime error handling
- **[INTERFACES](../INTERFACES.md)**: Application ↔ Runtime error propagation

---

**Document Version**: 1.0  
**Author**: Aria Ecosystem Documentation  
**Status**: Core specification (research complete, Result<T> implementation in progress)

**Research Complete** ✅ (December 22, 2025):
- **Result<T> monad**: Two-field struct (err: error code or NULL, val: value or NULL)
- **TBB verification**: Sticky error propagation via sentinel checks + overflow detection
- **Error Sinks**: Comparison operators transition from TBB domain to Boolean domain
- **Unwrap operator (?)**: Syntactic sugar for error checking with default values

**Key Findings** (from language_core_research_task.txt):
- **TBB Sticky Propagation**: ERR + 1 = ERR, errors absorb all operations
- **TBBLowerer logic**: Sentinel check → Arithmetic → Result validation → Selection
- **Error Sinks**: `val == ERR` returns clean boolean, forces explicit handling
- **Result structure**: Likely `struct { i8 err, [padding], T val }`
- **Optimized return**: Fits in registers (System V ABI), zero-cost vs exceptions
- **Auto-wrapping**: currentFunctionReturnType + currentFunctionAutoWrap
- **Sentinel values**: tbb8=-128, tbb16=-32768, tbb32=0x80000000, tbb64=INT64_MIN
- **Propagation algebra**: ERR is absorbing element in arithmetic

**Implementation Notes**:
- exprTypeMap tracks whether LLVM Value* is TBB type ("critical for TBB safety")
- Every arithmetic op on TBB injects check-and-propagate sequence
- Comparison ops are transition points where errors must be handled
- `?` operator provides ergonomic error handling without verbosity
