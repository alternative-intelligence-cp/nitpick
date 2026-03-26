# Contributing to Aria

Thank you for your interest in contributing to the Aria programming language!

## Getting Started

### Prerequisites

- **LLVM 20.1+** — Install from [apt.llvm.org](https://apt.llvm.org) or build from source
- **CMake 3.20+**
- **C++17 compiler** (clang-20 recommended)
- **liburing** — `sudo apt install liburing-dev`
- **wasi-libc** (optional, for WASM target) — `sudo apt install wasi-libc`
- **lld** (optional, for WASM target) — `sudo apt install lld`

### Building

```bash
git clone git@github.com:alternative-intelligence-cp/aria.git
cd aria
git checkout dev
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

The compiler binary is at `build/ariac`. The WASM runtime (`build/libaria_runtime_wasm.a`) is built automatically if wasi-libc is installed.

### Running Tests

```bash
# Run regression tests
bash tests/regression/run_regression.sh

# Run WASM tests (requires wasmtime)
bash tests/wasm/run_wasm_tests.sh

# Run a single test
./build/ariac tests/some_test.aria -o /tmp/test && /tmp/test
```

## Project Structure

```
aria/
├── src/
│   ├── frontend/      # Lexer, parser, AST, preprocessor, semantic analysis
│   ├── backend/       # LLVM IR generation, codegen
│   ├── runtime/       # Runtime library (GC, strings, async, streams, allocators)
│   │   └── wasm/      # WASM-specific runtime (wasm_runtime.c)
│   └── tools/         # LSP, DAP, package manager, doc generator
├── include/           # Header files
├── stdlib/            # Standard library (.aria files, 50 modules)
├── tests/             # Test suite (862 tests across 39 categories)
├── docs/              # Release notes and guides
├── debian/            # Debian packaging
└── CMakeLists.txt
```

**Related repositories:**
- [`aria-packages`](https://github.com/alternative-intelligence-cp/aria-packages) — 74 library packages
- [`aria-docs`](https://github.com/alternative-intelligence-cp/aria-docs) — Programming guide, reference docs, examples
- [`aria-tools`](https://github.com/alternative-intelligence-cp/aria-tools) — VS Code extension, MCP server, safety auditor
- [`aria-make`](https://github.com/alternative-intelligence-cp/aria-make) — Build system
- [`ariax`](https://github.com/alternative-intelligence-cp/ariax) — POSIX tools and AriaX Linux distro

## Architecture Overview

The compiler pipeline:

```
Source (.aria) → Preprocessor → Lexer → Parser → AST
    → Type Checker → Borrow Checker → Const Evaluator
    → Generic Resolver → IR Generator (LLVM IR)
    → LLVM Optimization → Native Object / WASM Object
    → Linker (clang++ / wasm-ld) → Executable / .wasm
```

Key source files:
- `src/main.cpp` — Compiler driver, CLI, output pipeline
- `src/backend/ir/ir_generator.cpp` — LLVM IR generation (largest file)
- `src/backend/ir/codegen_expr.cpp` — Expression codegen
- `src/frontend/parser/parser.cpp` — Parser
- `src/frontend/lexer/lexer.cpp` — Lexer
- `src/frontend/sema/` — Type checker, borrow checker, safety checker
- `src/runtime/` — Native runtime library (30+ modules)
- `src/runtime/wasm/wasm_runtime.c` — WASM runtime

## Code Style

- C++17, follow existing style in the file you're editing
- No Aria-source compiler warnings (check with `grep "warning:" | grep -v "/usr/lib/llvm"`)
- Use `Intrinsic::getOrInsertDeclaration` (not deprecated `getDeclaration`)
- Use `CreateGlobalString` (not deprecated `CreateGlobalStringPtr`)

## Commit Message Format

```
component: brief description

Longer explanation if needed. Reference issue numbers with #N.
```

Examples: `parser: fix array-in-struct member access`, `runtime/wasm: add __multi3 builtin`, `stdlib: add channel module`

## Submitting Changes

1. Fork the repository
2. Create a branch from `dev` (not `main` — main is for releases only)
3. Make your changes
4. Ensure regression tests pass
5. Submit a pull request to `dev`

## Reporting Bugs

Use the [issue templates](https://github.com/alternative-intelligence-cp/aria/issues/new/choose) — there are templates for bugs, feature requests, and compiler crashes.

## Adding a Package

Packages live in the [`aria-packages`](https://github.com/alternative-intelligence-cp/aria-packages) repo. Each package has:
```
packages/aria-PKGNAME/
├── src/PKGNAME.aria    # Module source
├── tests/test.aria     # Test suite with assertions
├── shim/               # C shim (if FFI needed)
└── aria_package.toml   # Package manifest
```

## License

By contributing, you agree that your contributions will be licensed under the Apache License 2.0.
