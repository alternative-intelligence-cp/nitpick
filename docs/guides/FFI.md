# Aria FFI/ABI Safety Guide

**Version**: 0.0.7-dev  
**Last Updated**: January 5, 2026  
**Audience**: Systems Programmers, Library Integrators, FFI Authors

## Table of Contents

1. [Introduction](#introduction)
2. [The FFI Safety Contract](#the-ffi-safety-contract)
3. [Extern Block Syntax](#extern-block-syntax)
4. [Type Mapping to C ABI](#type-mapping-to-c-abi)
5. [Calling Conventions](#calling-conventions)
6. [Memory Ownership Rules](#memory-ownership-rules)
7. [Structure ABI Compatibility](#structure-abi-compatibility)
8. [Opaque Types](#opaque-types)
9. [Return Value Handling](#return-value-handling)
10. [Large Type Passing](#large-type-passing)
11. [TBB Across FFI Boundaries](#tbb-across-ffi-boundaries)
12. [Common Pitfalls](#common-pitfalls)
13. [Best Practices](#best-practices)

---

## Introduction

The Foreign Function Interface (FFI) allows Aria to call external C libraries and be called from C code. This document defines the **safety contract** and **ABI (Application Binary Interface) rules** that govern cross-language interoperability.

**Warning**: FFI is the boundary where Aria's safety guarantees end. All calls across this boundary are inherently **unsafe**.

---

## The FFI Safety Contract

### What `extern` Means

When you write:

```aria
extern "libc" {
    func:malloc = void*(uint64:size);
}
```

You are making an **explicit safety contract**:

| Safety Feature | Aria Native | Across FFI |
|----------------|-------------|------------|
| Memory safety | ✅ Enforced | ❌ **DISABLED** |
| Type safety | ✅ Checked | ❌ **TRUST-BASED** |
| Null safety | ✅ Optional<T> | ❌ **UNCHECKED** |
| Overflow protection | ✅ TBB types | ❌ **C semantics** |
| GC integration | ✅ Automatic | ❌ **MANUAL** |
| Error handling | ✅ result<T> | ❌ **errno/NULL** |

### Compiler Behavior

In strict mode, the compiler warns about FFI usage:

```
warning: extern function 'malloc' bypasses Aria safety guarantees
  --> src/main.aria:12:5
   |
12 |     void*:ptr = malloc(1024);
   |                 ^^^^^^ FFI call: no memory safety
   |
help: consider wrapping in a safe Aria function with proper error handling
```

**Acknowledgment**: By using `extern`, you acknowledge these risks and take responsibility for safety.

---

## Extern Block Syntax

### Basic Syntax

```aria
extern "library_name" {
    // Function declarations
    func:function_name = return_type(param1_type:param1_name, ...);
    
    // Global variables
    wild var_type:var_name;
    
    // Opaque struct types
    opaque struct:TypeName;
}
```

### Library Names

The library name string determines linking behavior:

| Library Name | Linking Behavior | Example |
|--------------|------------------|---------|
| `"libc"` | Standard C library | `-lc` (automatic) |
| `"m"` | Math library | `-lm` |
| `"pthread"` | POSIX threads | `-lpthread` |
| `"kernel32"` | Windows kernel (Win32) | `kernel32.lib` |
| Custom name | Custom library | `-l<name>` |

### Complete Example

```aria
extern "libc" {
    // File I/O
    opaque struct:FILE;
    func:fopen = FILE*(int8*:path, int8*:mode);
    func:fclose = int32(FILE*:fp);
    func:fread = uint64(void*:ptr, uint64:size, uint64:nmemb, FILE*:stream);
    
    // Memory
    func:malloc = void*(uint64:size);
    func:free = void(void*:ptr);
    func:memcpy = void*(void*:dest, void*:src, uint64:n);
    
    // Global variables
    wild int32:errno;
}
```

**Note**: All items in `extern` blocks are implicitly unsafe - no additional markers needed.

---

## Type Mapping to C ABI

### Scalar Type Mapping

Aria types map to C types with **guaranteed ABI compatibility**:

| Aria Type | C Type | Size | Alignment | Notes |
|-----------|--------|------|-----------|-------|
| `int8` | `int8_t` | 1 byte | 1 | Signed 8-bit |
| `uint8` | `uint8_t` | 1 byte | 1 | Unsigned 8-bit |
| `int16` | `int16_t` | 2 bytes | 2 | Signed 16-bit |
| `uint16` | `uint16_t` | 2 bytes | 2 | Unsigned 16-bit |
| `int32` | `int32_t` | 4 bytes | 4 | Signed 32-bit |
| `uint32` | `uint32_t` | 4 bytes | 4 | Unsigned 32-bit |
| `int64` | `int64_t` | 8 bytes | 8 | Signed 64-bit |
| `uint64` | `uint64_t` | 8 bytes | 8 | Unsigned 64-bit |
| `flt32` | `float` | 4 bytes | 4 | IEEE 754 single |
| `flt64` | `double` | 8 bytes | 8 | IEEE 754 double |
| `void*` | `void*` | 8 bytes | 8 | Pointer (64-bit) |

### TBB Type Mapping

TBB types **do not have direct C equivalents** - they map to standard signed integers:

| Aria TBB | C Type | ABI Representation | Sentinel Handling |
|----------|--------|-------------------|-------------------|
| `tbb8` | `int8_t` | 8-bit signed | **ERR lost** (-128 = -128) |
| `tbb16` | `int16_t` | 16-bit signed | **ERR lost** (-32768 = -32768) |
| `tbb32` | `int32_t` | 32-bit signed | **ERR lost** |
| `tbb64` | `int64_t` | 64-bit signed | **ERR lost** |

**Critical**: TBB sentinel values (ERR, NULL, INF) are **not preserved** across FFI boundaries. The C side sees them as ordinary integer values.

### LBIM and Fixed-Point Types

Large integer and fixed-point types **cannot be passed directly**:

| Aria Type | FFI Status | Workaround |
|-----------|-----------|------------|
| `int128` | ❌ No direct support | Pass as `void*` to struct |
| `int256`-`int4096` | ❌ No direct support | Pass as `void*` to struct |
| `fix256`-`fix4096` | ❌ No direct support | Pass as `void*` to struct |

**Example**:

```aria
// WRONG: int256 is not FFI-compatible
extern "mylib" {
    func:process = void(int256:value);  // COMPILE ERROR
}

// RIGHT: Pass pointer to int256
extern "mylib" {
    func:process = void(void*:value_ptr);
}

func:main = int32() {
    int256:value = ...;
    process(&value);  // Pass address
    pass(0);
};
```

### Pointer Types

**Critical**: Aria has two pointer syntaxes - only one is FFI-compatible:

| Pointer Syntax | Usage | FFI Compatible |
|----------------|-------|----------------|
| `int8*` (C-style) | **ONLY in extern blocks** | ✅ Yes |
| `int8@` (Aria-style) | Everywhere else | ❌ No |

```aria
// CORRECT: C-style pointers in extern
extern "libc" {
    func:strlen = uint64(int8*:str);  // OK
}

// WRONG: Aria pointers in extern
extern "libc" {
    func:strlen = uint64(int8@:str);  // COMPILE ERROR
}

// CORRECT: Aria pointers in Aria code
func:process_string = void(int8@:str) {
    // ...
}
```

### The `void` Type

`void` is **ONLY valid in FFI** - it cannot be used in Aria native code:

```aria
// CORRECT: void return in extern
extern "libc" {
    func:free = void(void*:ptr);      // OK
    func:exit = void(int32:code);     // OK
}

// WRONG: void in Aria functions
func:my_func = void() {  // COMPILE ERROR: Use result type instead
    // ...
}

// CORRECT: Aria functions use result type
func:my_func = result() {
    pass(NIL);
};
```

---

## Calling Conventions

### Default: C Calling Convention

Aria defaults to the **C calling convention** (`cdecl` on x86-64, AAPCS on ARM64):

```aria
extern "libc" {
    // Implicitly uses C calling convention
    func:printf = int32(int8*:format, ...);
}
```

### Explicit Calling Convention (Planned)

**Status**: Not yet implemented in v0.0.7

Future syntax for Windows compatibility:

```aria
extern "stdcall" {
    func:Sleep = void(uint32:dwMilliseconds);
}

extern "fastcall" {
    func:optimized_func = int32(int32:x, int32:y);
}
```

### Register Usage (SysV AMD64 ABI)

On x86-64 Linux/macOS, arguments are passed in registers:

| Argument | Integer/Pointer | Floating-Point |
|----------|----------------|----------------|
| 1st | RDI | XMM0 |
| 2nd | RSI | XMM1 |
| 3rd | RDX | XMM2 |
| 4th | RCX | XMM3 |
| 5th | R8 | XMM4 |
| 6th | R9 | XMM5 |
| 7th+ | Stack | Stack |

Return values:
- Integer/pointer: RAX
- Floating-point: XMM0
- Large structs: Hidden pointer in RDI (see Large Type Passing)

---

## Memory Ownership Rules

### The Golden Rule

**"Caller Owns Memory"** - whoever allocates memory is responsible for freeing it.

### Pattern 1: C Allocates, C Frees

```aria
extern "libc" {
    func:malloc = void*(uint64:size);
    func:free = void(void*:ptr);
}

func:use_c_memory = void() {
    void*:ptr = malloc(1024);  // C allocates
    
    // Use memory...
    
    free(ptr);  // C frees (MANDATORY!)
}
```

**Warning**: Memory allocated by C `malloc` is **NOT tracked by Aria's GC**. You must explicitly call `free`.

### Pattern 2: Aria Allocates, Aria Frees

```aria
extern "libc" {
    func:strlen = uint64(int8*:str);
}

func:use_aria_memory = void() {
    wild int8*:str = aria_alloc_string(100);
    
    // Pass to C function (read-only)
    uint64:len = strlen(str);
    
    aria_free(str);  // Aria frees
}
```

### Pattern 3: Pinning GC Objects for FFI

**Critical**: GC pointers can **move** during collection - they are unsafe for FFI.

```aria
// WRONG: Passing GC pointer to C
gc int8@:str = gc_alloc_string(100);
strlen(str);  // UNSAFE! GC may move str during strlen()

// RIGHT: Pin GC object before FFI call (future syntax)
gc int8@:str = gc_alloc_string(100);
strlen(#str);  // '#' operator pins object during call
```

**Status**: Pinning operator `#` not yet implemented in v0.0.7. **Avoid passing GC pointers to C** for now.

### The FFI Firewall Pattern

Wrap all FFI calls in safe Aria functions:

```aria
extern "libc" {
    func:fopen = FILE*(int8*:path, int8*:mode);
    func:fclose = int32(FILE*:fp);
}

// Safe wrapper
func:open_file = result<FILE*>(int8*:path, int8*:mode) {
    FILE*:fp = fopen(path, mode);
    
    if (fp == NULL) {
        pass(ERR);  // Return error sentinel
    } else {
        pass(fp);   // Return valid pointer
    };
};
```

---

## Structure ABI Compatibility

### The Alignment Problem

C structs and Aria structs **do not have guaranteed compatible layout** unless explicitly controlled.

```c
// C struct
struct Point {
    int32_t x;
    int32_t y;
};
```

```aria
// Aria struct (INCOMPATIBLE by default!)
struct Point {
    int32:x;
    int32:y;
}
```

**Why incompatible?**
- Field reordering for optimization
- Padding differences
- Alignment requirements

### Extern Struct Pattern

Declare structs inside `extern` blocks for C compatibility:

```aria
extern "mylib" {
    // This struct MUST match C layout exactly
    struct:Point = {
        int32:x;
        int32:y;
    };
    
    func:process_point = void(Point*:p);
}
```

**Compiler guarantee**: Structs in `extern` blocks use **C-compatible layout** (no reordering, C padding rules).

### Verifying Layout with `@sizeof` and `@alignof`

```aria
extern "libc" {
    struct:stat = {
        uint64:st_dev;
        uint64:st_ino;
        uint64:st_mode;
        // ... (many more fields)
    };
}

func:check_layout = void() {
    // Verify struct size matches C
    int64:size = @sizeof(stat);
    
    if (size != 144) {  // Expected size from C header
        @panic("struct stat size mismatch!");
    } else {};
}
```

### Wild Pointers in Structs

Use `wild` pointers in FFI structs for C compatibility:

```aria
extern "libc" {
    struct:mntent = {
        wild int8*:mnt_fsname;   // C char*, not managed by GC
        wild int8*:mnt_dir;      // C char*
        wild int8*:mnt_type;     // C char*
        int32:mnt_freq;
    };
}
```

---

## Opaque Types

### The Problem of Incomplete Types

C libraries often hide implementation details:

```c
// C header: only declares the type
typedef struct FILE FILE;  // Definition hidden!

FILE* fopen(const char* path, const char* mode);
int fclose(FILE* stream);
```

Aria code needs to reference `FILE*` without knowing its size or layout.

### Solution: Opaque Struct Declarations

```aria
extern "libc" {
    opaque struct:FILE;  // "FILE exists, but I don't know its internals"
    
    func:fopen = FILE*(int8*:path, int8*:mode);
    func:fclose = int32(FILE*:stream);
}
```

**Compiler behavior**:
- Treats `FILE` as an incomplete type
- Allows `FILE*` (pointers) but not `FILE` (values)
- Prevents `@sizeof(FILE)` - size unknown!

### Working with Opaque Types

```aria
func:read_config = result() {
    // Create opaque FILE pointer
    FILE*:fp = fopen("config.txt", "r");
    
    if (fp == NULL) {
        pass(ERR);
    } else {};
    
    // Use opaque pointer (valid)
    // Read operations...
    
    fclose(fp);  // Release opaque resource
    pass(NIL);
};
```

**You can**:
- Pass opaque pointers to other functions
- Store them in variables
- Compare them (pointer equality)

**You cannot**:
- Dereference them
- Get their size (`@sizeof`)
- Access their fields

---

## Return Value Handling

### Small Return Values

Values fitting in registers (≤8 bytes) are returned directly:

```aria
extern "libc" {
    func:getpid = int32();  // Returns in EAX/RAX
    func:time = int64(int64*:tloc);  // Returns in RAX
}
```

### Null Returns Require Checking

**Critical**: C functions return `NULL` on error - this is **not checked automatically**:

```aria
extern "libc" {
    func:malloc = void*(uint64:size);
}

// UNSAFE: No null check
func:unsafe_alloc = void*() {
    pass(malloc(1024));  // What if malloc returns NULL?
};

// SAFE: Explicit null check
func:safe_alloc = result<void*>() {
    void*:ptr = malloc(1024);
    
    if (ptr == NULL) {
        pass(ERR);  // Signal allocation failure
    } else {
        pass(ptr);
    };
};
```

### Automatic Optional Wrapping (BUG-09-001 Fix)

**New in v0.0.7**: FFI functions returning pointers are **automatically wrapped** in `Optional<T*>`:

```aria
extern "libc" {
    func:getenv = int8*(int8*:name);
}

func:get_home_dir = result<int8*>() {
    Optional<int8*>:opt = getenv("HOME");  // Automatic wrapping!
    
    if (opt.hasValue == 0) {
        pass(ERR);  // Environment variable not set
    } else {
        pass(opt.value);
    };
};
```

**Optional struct layout**:

```aria
struct Optional<T*> {
    int8:hasValue;  // 0 = none, 1 = some
    T*:value;       // Valid only if hasValue == 1
}
```

---

## Large Type Passing

### The sret Convention

Types larger than 16 bytes (two registers) use **"sret"** (structure return):

```c
// C function
struct LargeResult {
    int64_t data[10];  // 80 bytes
};

struct LargeResult compute(int x);
```

**ABI transformation**:

```c
// What the compiler actually generates:
void compute(struct LargeResult* __sret, int x);

// Caller allocates space on stack:
struct LargeResult result;
compute(&result, 42);  // Pass pointer as first arg
```

### Aria Handling (ARIA-026 Fix)

The compiler **automatically** handles sret for large types:

```aria
extern "mylib" {
    struct:LargeResult = {
        int64:data[10];  // 80 bytes
    };
    
    func:compute = LargeResult(int32:x);
}

func:use_large = void() {
    // Compiler automatically:
    // 1. Allocates stack space for LargeResult
    // 2. Passes pointer as first argument
    // 3. Returns result in allocated space
    LargeResult:result = compute(42);
}
```

**Affected types**:
- Structs > 16 bytes
- `int128` (16 bytes - borderline, platform-dependent)
- `int256`, `int512`, `int1024`, `int2048`, `int4096`
- `fix256`, `fix512`, `fix1024`, `fix2048`, `fix4096`

### LBIM Scalarization

For extern functions, large integers are **scalarized** (broken into smaller pieces):

```aria
// C function expecting two 64-bit values
extern "mylib" {
    func:process_128 = void(uint64:low, uint64:high);
}

func:send_int128 = void() {
    int128:value = ...;
    
    // Compiler scalarizes int128 into low/high parts
    process_128(value);  // Automatic scalarization
}
```

**Status**: Partial implementation - works for int128, not yet for int256+.

---

## TBB Across FFI Boundaries

### The Fundamental Problem

TBB types rely on **sentinel values** (ERR, NULL, INF) that C does not understand:

```aria
tbb32:x = ERR32;  // -2,147,483,648 in Aria (special sentinel)

// Passing to C:
extern "libc" {
    func:process = void(int32:value);
}

process(x);  // C sees -2,147,483,648 as a normal integer!
```

**C does not know**:
- ERR32 is an error sentinel
- Arithmetic should propagate ERR
- Division by zero should produce ERR

### Solution 1: Check Before FFI Call

```aria
tbb32:result = compute();

if (@is_err_tbb32(result)) {
    // Handle error in Aria
    @panic("Computation failed");
} else {
    // Safe to pass to C (valid value)
    extern_c_func(result);
};
```

### Solution 2: Convert to Standard Integer

```aria
tbb32:value = ...;

// Convert TBB to standard int (loses sentinel semantics)
int32:safe_value = @i32(value);

extern_c_func(safe_value);
```

### Solution 3: Provide TBB Runtime to C

For C code that needs TBB semantics, link against `tbb_runtime.h`:

```c
// C code using TBB
#include "runtime/tbb.h"

void process_tbb(tbb32 value) {
    if (tbb32_is_err(value)) {
        // Handle error
        return;
    }
    
    // Use TBB arithmetic
    tbb32 result = tbb32_add(value, 100);
}
```

**Aria FFI**:

```aria
extern "mylib" {
    func:process_tbb = void(tbb32:value);
}

// TBB semantics preserved (C code uses tbb_runtime.h)
```

---

## Common Pitfalls

### Pitfall 1: Forgetting to Free C Memory

```aria
extern "libc" {
    func:malloc = void*(uint64:size);
    func:free = void(void*:ptr);
}

// WRONG: Memory leak!
func:leak = void() {
    void*:ptr = malloc(1024);
    // ... use ptr ...
    // OOPS! Forgot free(ptr)
}

// RIGHT: Always free
func:no_leak = void() {
    void*:ptr = malloc(1024);
    // ... use ptr ...
    free(ptr);  // Cleanup
}
```

### Pitfall 2: Struct Layout Mismatch

```c
// C header
struct Point {
    int x;
    int y;
};
```

```aria
// WRONG: Not in extern block, may have different layout!
struct Point {
    int32:x;
    int32:y;
}

// RIGHT: Declare in extern block
extern "mylib" {
    struct:Point = {
        int32:x;
        int32:y;
    };
}
```

### Pitfall 3: Passing GC Pointers to C

```aria
// WRONG: GC pointer to C function
gc int8@:str = gc_alloc_string(100);
strlen(str);  // CRASH! GC may move str during strlen()

// RIGHT: Use wild pointers for FFI
wild int8*:str = aria_alloc_string(100);
strlen(str);  // Safe
aria_free(str);
```

### Pitfall 4: Ignoring NULL Returns

```aria
// WRONG: Assumes malloc succeeds
void*:ptr = malloc(1024);
memset(ptr, 0, 1024);  // CRASH if ptr == NULL!

// RIGHT: Check for NULL
void*:ptr = malloc(1024);
if (ptr == NULL) {
    @panic("Allocation failed");
} else {
    memset(ptr, 0, 1024);
};
```

### Pitfall 5: TBB Sentinels Lost in FFI

```aria
tbb32:x = ERR32;

extern "libc" {
    func:abs = int32(int32:x);
}

int32:result = abs(x);  // C sees -2147483648, returns -2147483648
                        // ERR semantics LOST!

// RIGHT: Check before FFI
if (@is_err_tbb32(x)) {
    // Handle error
} else {
    abs(x);  // Safe
};
```

---

## Best Practices

### 1. **Always Wrap FFI in Safe Functions**

```aria
// Raw FFI (unsafe)
extern "libc" {
    func:fopen = FILE*(int8*:path, int8*:mode);
}

// Safe wrapper (recommended)
func:open_file = result<FILE*>(int8*:path, int8*:mode) {
    FILE*:fp = fopen(path, mode);
    
    if (fp == NULL) {
        pass(ERR);
    } else {
        pass(fp);
    };
};
```

### 2. **Use Opaque Types for Implementation Hiding**

```aria
// Don't guess at C struct layout
extern "libc" {
    opaque struct:FILE;  // Let C manage it
}
```

### 3. **Document Memory Ownership**

```aria
// Returns malloc'd memory - CALLER MUST FREE
func:allocate_buffer = void*(uint64:size) {
    pass(malloc(size));
};
```

### 4. **Verify Struct Layouts**

```aria
func:verify_ffi_structs = void() {
    // Check size matches C header
    if (@sizeof(MyStruct) != 64) {
        @panic("FFI struct size mismatch");
    } else {};
}
```

### 5. **Prefer Wild Pointers for FFI**

```aria
// For FFI, use wild (unmanaged) allocations
wild int8*:buffer = aria_alloc_buffer(1024, 8, 1);

// Not GC (which can move)
gc int8@:buffer = gc_alloc(...);  // DON'T use for FFI
```

### 6. **Check All Pointer Returns**

```aria
// Every FFI call returning a pointer needs a null check
void*:ptr = c_function();

if (ptr == NULL) {
    // Handle error
} else {
    // Use ptr
};
```

### 7. **Test FFI Boundaries Thoroughly**

```aria
// Integration tests for every extern function
func:test_fopen = void() {
    // Test success case
    FILE*:fp = fopen("test.txt", "r");
    if (fp == NULL) { @panic("fopen failed"); } else {};
    fclose(fp);
    
    // Test failure case
    FILE*:bad = fopen("/nonexistent/path", "r");
    if (bad != NULL) { @panic("fopen should fail"); } else {};
}
```

---

## Summary Checklist

Before writing FFI code, ask yourself:

- [ ] Are all extern types properly declared?
- [ ] Do struct layouts match C exactly?
- [ ] Are all pointer returns null-checked?
- [ ] Is memory ownership clearly documented?
- [ ] Are TBB sentinels checked before FFI calls?
- [ ] Are large types (int256+, fix256+) passed by pointer?
- [ ] Are opaque types used for incomplete C types?
- [ ] Is all C memory properly freed?
- [ ] Are GC pointers avoided in FFI?
- [ ] Have I wrapped unsafe FFI in safe Aria functions?

---

## See Also

- [Type System Guide](TYPE_SYSTEM.md) - TBB, LBIM, alignment requirements
- [Intrinsics Reference](../stdlib/INTRINSICS.md) - FFI-related intrinsics
- [Stdlib API](../stdlib/API.md) - Safe wrappers over FFI
- [Safety Guidelines](SAFETY.md) - Production FFI safety patterns

---

**Maintained by**: Aria Compiler Team  
**License**: MIT  
**Contact**: https://ai-liberation-platform.org
