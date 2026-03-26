# Aria v0.2.13 — WebAssembly Compilation Target

## What's New

### WASM Backend

Aria programs can now compile to WebAssembly for WASI-compatible runtimes:

```bash
ariac program.aria --emit-wasm -o program.wasm
wasmtime program.wasm
```

#### Compiler Changes
- New `--emit-wasm` flag to target WebAssembly
- New `--target=wasm32-wasi` option (default WASM target)
- Also supports `--target=wasm32-unknown-unknown` for bare WASM
- WASM compatibility checker warns about unsupported features at compile time
- Links via `wasm-ld` with WASI libc

#### WASM Runtime (`libaria_runtime_wasm.a`)
A stripped-down, WASI-compatible runtime library providing:
- Full string operations (concat, trim, case, search, pad, substring, format)
- Print and file I/O (via WASI syscalls)
- Math functions (pow, sqrt, trig, abs, min/max)
- Arena, pool, and slab allocators
- Hash maps with FNV-1a hashing
- Compiler builtins (`__multi3` for 128-bit multiply)

#### What Works in WASM
- ✅ Integer and float arithmetic
- ✅ Strings and string operations
- ✅ Functions, recursion, closures
- ✅ Structs, arrays, generics
- ✅ Control flow, pattern matching
- ✅ Error handling (Result types)
- ✅ File I/O (WASI runtimes only)
- ✅ stdin/stdout/stderr

#### WASM Limitations
- ❌ Threading, async/await, mutexes
- ❌ Process spawning (fork/exec)
- ❌ Signals, mmap, raw syscalls
- ❌ FFI to native shared libraries

### Test Suite
- New `tests/wasm/` directory with WASM-specific tests
- Test runner script (`run_wasm_tests.sh`)
- Tests cover: hello world, string ops, integer math, control flow, functions, recursion

### Documentation
- New WASM compilation guide: `docs/WASM_GUIDE.md`
- Feature compatibility matrix
- Architecture overview and troubleshooting

## Prerequisites for WASM

```bash
sudo apt install wasi-libc lld
curl https://wasmtime.dev/install.sh -sSf | bash
```

## Build

```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)
```

The WASM runtime (`libaria_runtime_wasm.a`) is built automatically if `wasi-libc` is installed.

## Full Changelog
- CMakeLists.txt: Added LLVM WebAssembly components + WASM runtime cross-compilation target
- main.cpp: emit_wasm_object(), link_wasm(), check_wasm_compatibility(), CLI flags
- New: src/runtime/wasm/wasm_runtime.c (~850 lines)
- New: tests/wasm/ (4 test files + runner)
- New: docs/WASM_GUIDE.md
