# Contributing to Aria

Thank you for your interest in contributing to the Aria programming language!

## Getting Started

### Prerequisites

- **LLVM 20.1+** — Install from [apt.llvm.org](https://apt.llvm.org) or build from source
- **CMake 3.20+**
- **C++17 compiler** (clang-20 recommended)
- **liburing** — `sudo apt install liburing-dev`

### Building

```bash
git clone git@github.com:alternative-intelligence-cp/aria.git
cd aria
cmake -B build_tmp -DCMAKE_BUILD_TYPE=Debug
cmake --build build_tmp -j$(nproc)
```

The compiler binary is at `build_tmp/ariac`.

### Running Tests

```bash
# Copy binary to expected location
cp build_tmp/ariac build/ariac

# Run regression tests
bash tests/regression/run_regression.sh
```

All 5 regression tests should pass before submitting a PR.

## Project Structure

```
aria/
├── include/           # Header files
│   ├── frontend/      # Lexer, parser, type checker, borrow checker
│   └── backend/       # IR generator, codegen
├── src/               # Implementation
│   ├── frontend/      # Parser, type system, semantic analysis
│   ├── backend/ir/    # LLVM IR generation (main codegen)
│   ├── runtime/       # Runtime library (async, GC, I/O)
│   └── tools/         # LSP, DAP debugger, package manager, doc gen
├── tests/             # Test suites
│   └── regression/    # Regression tests (must pass on every commit)
├── stdlib/            # Standard library (.aria files)
├── aria_ecosystem/    # Package registry and ecosystem packages
└── debian/            # Debian packaging
```

## Code Style

- C++17, follow existing style in the file you're editing
- No Aria-source compiler warnings (check with `grep "warning:" | grep -v "/usr/lib/llvm"`)
- Use `Intrinsic::getOrInsertDeclaration` (not deprecated `getDeclaration`)
- Use `CreateGlobalString` (not deprecated `CreateGlobalStringPtr`)

## Submitting Changes

1. Fork the repository
2. Create a branch from `dev` (not `main`)
3. Make your changes
4. Ensure regression tests pass
5. Submit a pull request to `dev`

## Reporting Bugs

Use the [issue templates](https://github.com/alternative-intelligence-cp/aria/issues/new/choose) — there are templates for bugs, feature requests, and compiler crashes.

## License

By contributing, you agree that your contributions will be licensed under the project's existing license.
