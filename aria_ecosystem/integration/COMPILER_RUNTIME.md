# Compiler ↔ Runtime Integration Guide

**Document Type**: Integration Guide  
**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Reference Documentation

---

## Table of Contents

1. [Overview](#overview)
2. [Linking Process](#linking-process)
3. [Builtin Lowering](#builtin-lowering)
4. [ABI Specification](#abi-specification)
5. [Type Mapping](#type-mapping)
6. [Symbol Resolution](#symbol-resolution)
7. [Debug Information](#debug-information)

---

## Overview

The Aria compiler (`ariac`) and runtime library (`libaria_runtime.a`) form a tightly integrated pair. Understanding their interface is critical for:

- **Compiler Developers**: Implementing code generation
- **Runtime Developers**: Providing correct functionality
- **Advanced Users**: Understanding low-level behavior

---

## Linking Process

### Compilation Pipeline

```
Source Code (.aria)
    ↓
[Lexer] → Tokens
    ↓
[Parser] → AST
    ↓
[Semantic Analysis] → Typed AST
    ↓
[IR Generation] → LLVM IR
    ↓
[Optimization] → Optimized IR
    ↓
[Code Generation] → Object File (.o)
    ↓
[Linker] → Link with libaria_runtime.a
    ↓
Executable (a.out)
```

---

### Automatic Linking

**Default Behavior**: `ariac` automatically links against `libaria_runtime.a`

**Example**:
```bash
$ ariac hello.aria
# Equivalent to:
# clang hello.o -L/usr/local/lib/aria -laria_runtime -o a.out
```

**Runtime Search Paths**:
1. `$ARIA_RUNTIME_PATH` (environment variable)
2. `/usr/local/lib/aria` (default install location)
3. `~/.aria/runtime` (user-local install)

---

### Manual Linking

**Custom Runtime Location**:
```bash
$ ariac hello.aria --runtime-path=/path/to/custom/runtime
```

**Skip Runtime Linking** (for bare-metal):
```bash
$ ariac hello.aria --no-runtime
# Produces object file only, no executable
```

---

## Builtin Lowering

### What are Builtins?

**Builtins**: High-level Aria constructs that map to runtime functions

**Example**: `io.stdout.write(text)`

```aria
// Aria code
io.stdout.write("Hello, World!\n");
```

**Lowered to**:
```c
// C runtime call
aria_stdout_write("Hello, World!\n", 14);
```

---

### Builtin Catalog

**I/O Builtins**:

| Aria Code | Runtime Function | Signature |
|-----------|------------------|-----------|
| `io.stdin.read_line()` | `aria_stdin_read_line()` | `result_string(void)` |
| `io.stdout.write(text)` | `aria_stdout_write(text, len)` | `void(const char*, size_t)` |
| `io.stderr.write(text)` | `aria_stderr_write(text, len)` | `void(const char*, size_t)` |
| `io.stddbg.write(text)` | `aria_stddbg_write(text, len)` | `void(const char*, size_t)` |
| `io.stddati.read(buf, n)` | `aria_stddati_read(buf, n)` | `result_size(void*, size_t)` |
| `io.stddato.write(buf, n)` | `aria_stddato_write(buf, n)` | `result_size(const void*, size_t)` |

**Memory Allocation Builtins**:

| Aria Code | Runtime Function | Signature |
|-----------|------------------|-----------|
| `alloc<T>()` | `aria_malloc(sizeof(T))` | `void*(size_t)` |
| `alloc_array<T>(n)` | `aria_malloc(sizeof(T) * n)` | `void*(size_t)` |
| `free(ptr)` | `aria_free(ptr)` | `void(void*)` |
| `realloc(ptr, size)` | `aria_realloc(ptr, size)` | `void*(void*, size_t)` |

**String Builtins**:

| Aria Code | Runtime Function | Signature |
|-----------|------------------|-----------|
| `string("hello")` | `aria_string_from_literal(text, len)` | `aria_string(const char*, size_t)` |
| `s1 + s2` | `aria_string_concat(s1, s2)` | `aria_string(aria_string, aria_string)` |
| `s.len()` | `aria_string_len(s)` | `size_t(aria_string)` |

**Array Builtins**:

| Aria Code | Runtime Function | Signature |
|-----------|------------------|-----------|
| `array<T>(n)` | `aria_array_alloc_<T>(n)` | `aria_array_T(size_t)` |
| `arr[i]` | `aria_array_get_<T>(arr, i)` | `T(aria_array_T, size_t)` |
| `arr[i] = val` | `aria_array_set_<T>(arr, i, val)` | `void(aria_array_T, size_t, T)` |

**Result<T> Builtins**:

| Aria Code | Runtime Function | Signature |
|-----------|------------------|-----------|
| `pass(value)` | `aria_result_ok_<T>(value)` | `result_T(T)` |
| `err(message)` | `aria_result_err_<T>(msg, len)` | `result_T(const char*, size_t)` |
| `r.is_ok()` | `aria_result_is_ok_<T>(r)` | `bool(result_T)` |
| `r.ok()` | `aria_result_unwrap_<T>(r)` | `T(result_T)` |
| `r.err()` | `aria_result_get_err_<T>(r)` | `aria_string(result_T)` |

**TBB Builtins**:

| Aria Code | Runtime Function | Signature |
|-----------|------------------|-----------|
| `i32[L..U]` assignment | `aria_tbb_assign_i32(val, L, U)` | `int32_t(int32_t, int32_t, int32_t)` |
| `TBB_ERR` constant | `ARIA_TBB_ERR_I32` | `#define ARIA_TBB_ERR_I32 INT32_MIN` |

---

### IR Generation Example

**Aria Code**:
```aria
func:main = i64() {
    io.stdout.write("Hello\n");
    pass(0);
}
```

**LLVM IR** (simplified):
```llvm
@.str = private unnamed_addr constant [7 x i8] c"Hello\0A\00"

define i64 @main() {
entry:
  call void @aria_stdout_write(i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str, i32 0, i32 0), i64 6)
  ret i64 0
}

declare void @aria_stdout_write(i8*, i64)
```

**Key Points**:
- String literal stored in `.data` section
- Function call to runtime (`aria_stdout_write`)
- Declaration tells linker to find symbol in `libaria_runtime.a`

---

## ABI Specification

### Calling Convention

**Platform-Specific**:
- **Linux x86_64**: System V AMD64 ABI
- **macOS x86_64**: Same as Linux (System V)
- **Windows x86_64**: Microsoft x64 calling convention

**Aria Convention**: Follow platform default (no custom ABI)

---

### Argument Passing

**Primitive Types**:
- `i8`, `u8`, `i16`, `u16`, `i32`, `u32`: Passed in registers (or stack if exhausted)
- `i64`, `u64`, `f64`: Passed in registers
- `bool`: Passed as `i8` (0 = false, 1 = true)

**Composite Types**:
- **Small structs** (≤16 bytes): Passed in registers (split across multiple)
- **Large structs** (>16 bytes): Passed by pointer (caller allocates)

**Example**:
```aria
struct:point = { x: i32, y: i32 };  // 8 bytes total

func:distance = f64(p1: point, p2: point) {
    // p1, p2 passed in registers (8 bytes each)
}
```

---

### Return Values

**Primitive Types**: Returned in `rax` (x86_64) or equivalent

**Small Structs**: Returned in registers

**Large Structs**: Caller allocates space, callee writes to it

**Result<T>**:
- **result<i32>** (16-24 bytes): Returned via pointer (caller allocates)
- **result<void>** (8-16 bytes): Returned in registers

---

### Stack Alignment

**Requirement**: 16-byte alignment on function entry

**Maintained By**: Compiler (LLVM handles this automatically)

---

## Type Mapping

### Aria Type → C Type

| Aria Type | C Type | Size (bytes) |
|-----------|--------|--------------|
| `i8` | `int8_t` | 1 |
| `u8` | `uint8_t` | 1 |
| `i16` | `int16_t` | 2 |
| `u16` | `uint16_t` | 2 |
| `i32` | `int32_t` | 4 |
| `u32` | `uint32_t` | 4 |
| `i64` | `int64_t` | 8 |
| `u64` | `uint64_t` | 8 |
| `f32` | `float` | 4 |
| `f64` | `double` | 8 |
| `bool` | `uint8_t` | 1 |
| `char` | `uint32_t` | 4 |
| `wild T*` | `T*` | 8 (64-bit) |

---

### Complex Types

**string**:
```c
// Runtime representation
typedef struct {
    char* data;
    size_t len;
    size_t capacity;
} aria_string;
```

**array<T>**:
```c
// Runtime representation (monomorphized)
typedef struct {
    T* data;
    size_t len;
    size_t capacity;
} aria_array_T;
```

**result<T>**:
```c
// Runtime representation (monomorphized)
typedef struct {
    bool is_ok;
    union {
        T ok_value;
        struct {
            char* err_msg;
            size_t err_len;
        } err_value;
    } data;
} aria_result_T;
```

**TBB (i32[L..U])**:
```c
// Same as base type (bounds checked at assignment)
typedef int32_t aria_tbb_i32;
```

---

## Symbol Resolution

### Symbol Naming Convention

**Functions**: `aria_<category>_<name>`

**Examples**:
- I/O: `aria_stdout_write`, `aria_stdin_read_line`
- Memory: `aria_malloc`, `aria_free`
- Strings: `aria_string_concat`, `aria_string_len`
- Arrays: `aria_array_alloc_i32`, `aria_array_get_i32`

**Type-Specific**: Generic functions are monomorphized

```aria
array<i32>:arr = array<i32>(10);
```

**Lowers to**:
```c
aria_array_i32 arr = aria_array_alloc_i32(10);
```

---

### Symbol Visibility

**Runtime Exports**: All `aria_*` symbols are public

**Internal Symbols**: Prefixed with `_aria_internal_*` (not for direct use)

**Link-Time Optimization**: Runtime compiled with `-fvisibility=hidden` + explicit exports

---

### Symbol Versioning (Future)

**Challenge**: Runtime updates may break ABI

**Solution**: Symbol versioning

```c
// Version 1
void aria_stdout_write_v1(const char* text, size_t len);

// Version 2 (added error handling)
int aria_stdout_write_v2(const char* text, size_t len);

// Default (latest version)
#define aria_stdout_write aria_stdout_write_v2
```

**Compiler Compatibility**: Emit versioned symbols to ensure compatibility

---

## Debug Information

### DWARF Generation

**Compiler Emits**: DWARF debug info in object files

**Includes**:
- Source file paths
- Line number mappings
- Variable names and types
- Function names and signatures

**Usage**: GDB, LLDB, DAP (Debug Adapter Protocol)

---

### Type Information

**Aria Types → DWARF Types**:

**Primitives**:
```
DW_TAG_base_type
  DW_AT_name: "i32"
  DW_AT_encoding: DW_ATE_signed
  DW_AT_byte_size: 4
```

**Structs**:
```
DW_TAG_structure_type
  DW_AT_name: "point"
  DW_AT_byte_size: 8
  DW_TAG_member
    DW_AT_name: "x"
    DW_AT_type: <i32>
    DW_AT_data_member_location: 0
  DW_TAG_member
    DW_AT_name: "y"
    DW_AT_type: <i32>
    DW_AT_data_member_location: 4
```

**Result<T>**:
```
DW_TAG_structure_type
  DW_AT_name: "result<i32>"
  DW_AT_byte_size: 16
  DW_TAG_member
    DW_AT_name: "is_ok"
    DW_AT_type: <bool>
  DW_TAG_member
    DW_AT_name: "data"
    DW_AT_type: <union>
```

---

### Source Mapping

**Line Tables**: Map machine code addresses to source lines

**Example**:
```
Address           Line  Source
0x401000          1     func:main = i64() {
0x401005          2         io.stdout.write("Hello\n");
0x401020          3         pass(0);
0x401025          4     }
```

**Usage**: Debuggers use this to show source code during stepping

---

## Related Documents

- **[ARIA_COMPILER](../components/ARIA_COMPILER.md)**: Compiler architecture
- **[ARIA_RUNTIME](../components/ARIA_RUNTIME.md)**: Runtime library implementation
- **[TYPE_SYSTEM](../specs/TYPE_SYSTEM.md)**: Type definitions
- **[ERROR_HANDLING](../specs/ERROR_HANDLING.md)**: Result<T> and TBB
- **[ARIA_DAP](../components/ARIA_DAP.md)**: Debug adapter integration

---

**Document Version**: 1.0  
**Author**: Aria Ecosystem Documentation  
**Status**: Reference guide (implementation in progress)
