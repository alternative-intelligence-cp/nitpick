# Cross-Language Bindings

Aria can interoperate with C, Python, and any language that supports C ABI calling conventions.

---

## Overview

Aria's compilation pipeline (Aria → LLVM IR → native code) produces symbols with standard C ABI linkage. This means Aria functions are callable from any language that can load shared libraries and call C functions.

**Three binding directions:**

| Direction | Mechanism | Status |
|-----------|-----------|--------|
| C → Aria | `extern` FFI blocks | Stable (v0.1.0+) |
| Aria → C | `--shared` flag produces `.so` | New in v0.2.6 |
| Aria → Python | `ctypes.CDLL()` on `.so` | New in v0.2.6 |

---

## Aria → C (Exporting Aria Functions)

Compile Aria source to a shared library that C programs can link against.

### Step 1: Write Aria Library

```aria
// mathlib.aria
func:add = int32(int32:a, int32:b) {
    pass(a + b);
};

func:multiply = int32(int32:x, int32:y) {
    pass(x * y);
};

func:square = int32(int32:n) {
    pass(n * n);
};
```

### Step 2: Compile to Shared Library

```bash
ariac mathlib.aria --shared -o libmathlib.so
```

The `--shared` flag:
- Skips the `failsafe` requirement (library mode)
- Compiles with Position-Independent Code (PIC)
- Links as a shared library (.so)

### Step 3: Use from C

```c
#include <stdio.h>

// Declare the Aria functions
extern int add(int a, int b);
extern int multiply(int x, int y);
extern int square(int n);

int main() {
    printf("add(3, 4) = %d\n", add(3, 4));
    printf("multiply(5, 6) = %d\n", multiply(5, 6));
    printf("square(9) = %d\n", square(9));
    return 0;
}
```

```bash
gcc main.c -L. -lmathlib -Wl,-rpath,. -o main
./main
```

### Type Mapping (Aria → C)

| Aria Type | C Type | Notes |
|-----------|--------|-------|
| `int8` | `int8_t` / `char` | |
| `int16` | `int16_t` / `short` | |
| `int32` | `int32_t` / `int` | |
| `int64` | `int64_t` / `long long` | |
| `float32` | `float` | |
| `float64` | `double` | |
| `bool` | `int` | 0 = false, 1 = true |
| `string` | `char*` | Null-terminated C string |

---

## C → Aria (Consuming C Libraries)

Use `extern` blocks to declare C functions, then call them from Aria.

### Example: Using a C Math Library

```aria
extern "m" {
    func:sqrt = float64(float64:x);
    func:pow = float64(float64:base, float64:exp);
    func:sin = float64(float64:x);
    func:cos = float64(float64:x);
};

func:failsafe = NIL(int32:code) { pass(NIL); };

func:main = NIL() {
    var:result = sqrt(144.0);
    emit(result);
};
```

```bash
ariac main.aria -o main -lm
```

### Using Custom C Shared Libraries

```aria
extern "mylib" {
    func:do_work = int32(int32:input);
    func:get_version = int32();
};
```

```bash
ariac main.aria -o main -L./libs -lmylib
```

The `extern "name"` string corresponds to the library name: `libname.so`.

---

## Aria → Python (via ctypes)

Python can call Aria shared libraries using the built-in `ctypes` module.

### Step 1: Compile Aria to Shared Library

```bash
ariac mathlib.aria --shared -o libmathlib.so
```

### Step 2: Call from Python

```python
import ctypes

# Load the Aria shared library
lib = ctypes.CDLL('./libmathlib.so')

# Declare function signatures
lib.add.restype = ctypes.c_int
lib.add.argtypes = [ctypes.c_int, ctypes.c_int]

lib.multiply.restype = ctypes.c_int
lib.multiply.argtypes = [ctypes.c_int, ctypes.c_int]

lib.square.restype = ctypes.c_int
lib.square.argtypes = [ctypes.c_int]

# Call Aria functions from Python
print(lib.add(10, 20))        # 30
print(lib.multiply(7, 8))     # 56
print(lib.square(9))          # 81
```

### Python Type Mapping

| Aria Type | ctypes Type |
|-----------|-------------|
| `int8` | `ctypes.c_int8` |
| `int16` | `ctypes.c_int16` |
| `int32` | `ctypes.c_int` |
| `int64` | `ctypes.c_longlong` |
| `float32` | `ctypes.c_float` |
| `float64` | `ctypes.c_double` |
| `bool` | `ctypes.c_int` |
| `string` | `ctypes.c_char_p` |

---

## Aria → Other Languages

Any language that supports C FFI can call Aria shared libraries:

| Language | Mechanism |
|----------|-----------|
| **Rust** | `extern "C"` + `#[link]` |
| **Go** | `cgo` with `// #cgo LDFLAGS: -lmathlib` |
| **Java** | JNI or JNA |
| **Node.js** | `ffi-napi` package |
| **Ruby** | `fiddle` or `ffi` gem |
| **C#** | `[DllImport]` P/Invoke |
| **Zig** | `@cImport` or `extern` |

Since Aria produces standard C ABI symbols, any language with C interop works.

---

## Compiler Flags Reference

| Flag | Output | Use Case |
|------|--------|----------|
| (default) | Executable | Standalone programs |
| `-c` | Object file (.o) | Intermediate compilation |
| `--shared` | Shared library (.so) | Cross-language bindings |
| `--emit-llvm` | LLVM IR (.ll) | Debugging, inspection |
| `--emit-asm` | Assembly (.s) | Low-level inspection |

---

## Symbol Naming

Aria functions use their declared name directly as the symbol name. There is no name mangling.

```aria
func:my_function = int32(int32:x) { pass(x); };
```

Produces symbol: `my_function` (visible via `nm -D libname.so`).

---

## Limitations

- **Strings**: Aria string values passed across the boundary are C-style `char*` pointers. The caller is responsible for memory management of string arguments.
- **Complex types**: Structs and arrays are not yet supported across the FFI boundary. Use scalar types or pointers.
- **Async functions**: Async Aria functions cannot be called from C/Python directly (they return coroutine handles).
- **No header generation**: C header files must be written manually. Planned for future release.
