# Aria WebAssembly Compilation Guide

## Overview

Aria v0.2.13 adds WebAssembly (WASM) as a compilation target. Compile Aria programs to `.wasm` files that run in WASI-compatible runtimes like `wasmtime`, `wasmer`, or `wasm3`.

## Prerequisites

- **wasi-libc**: WASI sysroot with libc for wasm32
  ```bash
  sudo apt install wasi-libc
  ```
- **wasm-ld**: WebAssembly linker (part of LLVM/LLD)
  ```bash
  sudo apt install lld
  ```
- **wasmtime** (or wasmer): To run the compiled `.wasm` files
  ```bash
  curl https://wasmtime.dev/install.sh -sSf | bash
  ```

## Usage

### Basic Compilation

```bash
ariac program.aria --emit-wasm -o program.wasm
```

### With Explicit Target

```bash
ariac program.aria --emit-wasm --target=wasm32-wasi -o program.wasm
```

### Running

```bash
wasmtime program.wasm
```

## Supported Features

| Feature | WASM Support | Notes |
|---|---|---|
| Integer arithmetic (int32, int64) | ✅ Full | |
| Float arithmetic (flt32, flt64) | ✅ Full | |
| Strings | ✅ Full | concat, compare, trim, case, search, pad |
| Control flow (if/else, while) | ✅ Full | |
| Functions & recursion | ✅ Full | |
| Structs | ✅ Full | |
| Arrays | ✅ Full | |
| Pattern matching | ✅ Full | |
| Error handling (Result types) | ✅ Full | |
| File I/O | ⚠️ WASI only | Needs WASI runtime with filesystem access |
| stdin/stdout | ✅ Full | Via WASI fd_read/fd_write |
| Math functions | ✅ Full | Software implementations |
| Generics | ✅ Full | |

## Unsupported Features (WASM Target)

These features are **not available** when compiling to WASM. The compiler will emit warnings if your code uses them:

| Feature | Reason |
|---|---|
| Async/await | No io_uring in WASM |
| Threading & thread pools | WASM is single-threaded (for now) |
| Mutex/atomics | No threading support |
| Process spawning (fork/exec) | WASM sandbox restriction |
| Signals | WASM has no signal model |
| mmap/mprotect | WASM uses linear memory |
| Raw syscalls | WASM sandbox restriction |
| FFI to native .so/.dll | WASM has its own import model |

The compiler's compatibility checker scans your code and warns about unsupported features before linking.

## Architecture

```
                    ┌─────────────┐
 program.aria  ──>  │  Aria Parser │
                    └──────┬──────┘
                           │ AST
                    ┌──────▼──────┐
                    │  IR Generator│
                    └──────┬──────┘
                           │ LLVM IR
                    ┌──────▼──────┐
                    │ WASM Codegen │  (target: wasm32-wasi)
                    └──────┬──────┘
                           │ .o (WASM object)
              ┌────────────▼────────────┐
              │        wasm-ld          │
              │  + libaria_runtime_wasm │
              │  + wasi-libc            │
              └────────────┬────────────┘
                           │
                    program.wasm
```

The WASM backend:
1. Sets target triple to `wasm32-wasi`
2. Renames `main` → `__main_argc_argv` (WASI convention)
3. Emits a WASM object file via LLVM's WebAssembly backend
4. Links with `wasm-ld` against `libaria_runtime_wasm.a` and WASI libc

## WASM Runtime

`libaria_runtime_wasm.a` is a stripped-down, WASI-compatible replacement for the native `libaria_runtime`. It provides:

- **String operations**: All `_simple` string functions (concat, trim, case, search, pad, substring, etc.)
- **I/O**: Print, file read/write (via WASI), stdin
- **Math**: All math functions (pow, sqrt, trig, abs, min/max)
- **Memory**: Arena, pool, slab allocators (simplified)
- **Maps**: Hash map with FNV-1a hashing
- **Compiler builtins**: `__multi3` (128-bit multiply for int64 arithmetic)

## Examples

### Hello World

```aria
func:main = int32() {
    out("Hello from WebAssembly!");
    return 0i32;
};

func:failsafe = void(int32:err_code) {};
```

```bash
ariac hello.aria --emit-wasm -o hello.wasm
wasmtime hello.wasm
# Output: Hello from WebAssembly!
```

### String Operations

```aria
func:main = int32() {
    str:greeting = "Hello, ";
    str:name = "World";
    str:message = greeting + name + "!";
    out(message);
    return 0i32;
};

func:failsafe = void(int32:err_code) {};
```

### Integer Math & Control Flow

```aria
func:factorial = int64(int64:n) {
    if n <= 1i64 {
        return 1i64;
    };
    int64:sub = n - 1i64;
    int64:rec = factorial(sub);
    int64:result = n * rec;
    return result;
};

func:main = int32() {
    int64:fact5 = factorial(5i64);
    // fact5 == 120
    return 0i32;
};

func:failsafe = void(int32:err_code) {};
```

## Binary Size

Typical WASM binary sizes:
- Hello world: ~15-20 KB
- String operations: ~25-35 KB
- Math-heavy programs: ~20-30 KB

These sizes include the WASM runtime and WASI libc stubs.

## Troubleshooting

### "undefined symbol" errors
Your program may use a feature not available in the WASM runtime. Check the unsupported features table above.

### "function signature mismatch"
This typically indicates an ABI mismatch between your code and the WASM runtime. Ensure you're using the latest `libaria_runtime_wasm.a` built alongside the compiler.

### wasmtime permission errors for file I/O
WASI sandboxes filesystem access. Grant directory access:
```bash
wasmtime --dir=. program.wasm
```
