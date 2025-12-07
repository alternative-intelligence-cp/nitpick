# Aria Programming Language v0.0.6
![Aria Logo](/pics/AriaLogocompressed.png)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Commercial License Available](https://img.shields.io/badge/Commercial-License_Available-green.svg)](LICENSE.md)

**A modern systems programming language with Turing-complete preprocessor and LLVM backend**

---

## 🚀 Current Status (December 7, 2025)

**Compiler**: 93-95% complete | **Phase**: Tsoding-ready | **Standard Library**: 154 working functions

### What's Working ✅
- ✅ Lexer (100% complete)
- ✅ Preprocessor (100% - 16 directives, macro expansion)
- ✅ Parser (95% complete - 13/14 features)
- ✅ LLVM Backend (93% complete)
- ✅ Collections Library (130 functions across 10 types)
- ✅ Math Library (24 functions)
- ✅ Auto-wrap functions with `*` prefix
- ✅ Control flow (if/while/till/defer)
- ✅ Professional error messages

See [STDLIB_STATUS.md](STDLIB_STATUS.md) and [ROADMAP.md](ROADMAP.md) for detailed status.

---

## Overview

Aria features:

- **Turing-Complete Preprocessor**: NASM-style macros with 16 directives, capable of generating functions programmatically
- **LLVM Backend**: Professional optimization and cross-platform compilation
- **Type System**: 10 fundamental types (int8-64, uint8-64, flt32, flt64)
- **Auto-wrap Functions**: Simple `*` prefix for automatic return handling
- **Explicit Syntax**: Mandatory semicolons, braces, type annotations
- **Stack Allocation**: Fast array allocation with `stack N`
- **Future**: Result types, gc/wild memory management, exotic types (balanced ternary/nonary)

## Quick Examples

### Hello World (Minimal)
```aria
func:main = *int32() {
    return 0;
};
```

### Array Operations with Collections Library
```aria
func:main = *int32() {
    // Student grades
    int32[]:scores = stack 5;
    scores[0] = 85;
    scores[1] = 92;
    scores[2] = 78;
    scores[3] = 95;
    scores[4] = 88;
    
    // Use collections library functions
    int32:total = sum_int32(scores, 5);
    int32:highest = max_int32(scores, 5);
    int32:lowest = min_int32(scores, 5);
    int32:avg = total / 5;  // 87
    
    // Check if value exists
    if (contains_int32(scores, 5, 92)) {
        // Found!
    }
    
    return 0;
};
```

### Macro-Generated Functions
```aria
// Define generic function with macros
%macro GEN_SWAP 1
func:swap_%1 = *void(%1[]:arr, int64:i, int64:j) {
    %1:temp = arr[i];
    arr[i] = arr[j];
    arr[j] = temp;
};
%endmacro

// Generate for multiple types
GEN_SWAP(int32)
GEN_SWAP(uint32)
GEN_SWAP(flt64)

func:main = *int32() {
    int32[]:nums = stack 3;
    nums[0] = 10;
    nums[1] = 20;
    swap_int32(nums, 0, 1);  // Now: 20, 10
    return 0;
};
```

---

## Getting Started

### Build the Compiler
```bash
cd build
cmake ..
make

# Compiler is now at build/ariac
```

### Compile Your First Program
```bash
# Create hello.aria
echo 'func:main = *int32() { return 0; };' > hello.aria

# Compile
./build/ariac hello.aria -o hello

# Run
./hello
echo $?  # Should output: 0
```

### Using the Standard Library
```bash
# Collections and math libraries are in lib/stdlib/
./build/ariac my_program.aria -o my_program

# See preprocessed output (macro expansion)
./build/ariac -E my_program.aria
```

---

## Documentation

- **[Programming Guide](docs/ARIA_PROGRAMMING_GUIDE.md)** - Complete language reference with examples
- **[Standard Library Status](STDLIB_STATUS.md)** - What's working in stdlib
- **[Development Roadmap](ROADMAP.md)** - Phase 1 (Tsoding) → Phase 2 (Nikola)
- **[Language Specifications](docs/aria_v0_0_6_specs.txt)** - Formal spec

---

## Project Structure

```
aria/
├── build/              # Compiled binaries
│   └── ariac          # The compiler
├── src/                # Compiler source
│   ├── lexer.cpp
│   ├── preprocessor.cpp
│   ├── parser.cpp
│   └── llvm_backend.cpp
├── lib/stdlib/        # Standard library
│   ├── collections.aria  # ✅ 130 functions
│   ├── math.aria        # ✅ 24 functions
│   ├── string.aria      # ⚠️ LLVM bug
│   ├── io.aria          # ❌ Needs pointers
│   ├── mem.aria         # ❌ Needs wildx
│   └── bit_operations.aria  # ❌ Macro bug
├── tests/             # Test programs
├── examples/          # Example code
└── docs/              # Documentation
```

---

## Known Issues & Workarounds

### 1. Macro Variable Scoping Bug
**Problem**: Using `%1:varname` for internal variables in macros causes parse errors when invoked multiple times.

```aria
// ❌ FAILS with multiple invocations
%macro GEN_FUNC 1
func:test_%1 = *%1(%1:x) {
    %1:temp = x;  // Using %1 for internal variable
    return temp;
};
%endmacro

// ✅ WORKS - use fixed type
%macro GEN_FUNC 1
func:test_%1 = *%1(%1:x) {
    int32:temp = x;  // Fixed type works
    return temp;
};
%endmacro
```

**Workaround**: Use fixed types (`int32`, `int64`) for internal variables. Only use `%1` for parameters and return types.

See [Programming Guide](docs/ARIA_PROGRAMMING_GUIDE.md) for details.

### 2. String Library LLVM Verification Error
Compiles but fails with "Terminator found in middle of basic block". Requires LLVM IR debugging.

### 3. Pointer Types Not Fully Implemented
Syntax exists (`uint8@:buffer`) but parser incomplete. Blocks I/O library.

### 4. `wildx` Type Not Implemented
Blocks memory library executable allocation features.

---

## Development Roadmap

### Phase 1: Tsoding-Ready (Current)
**Goal**: Demonstrate working features, get expert feedback

- ✅ Compiler core (93-95% complete)
- ✅ Collections library (130 functions)
- ✅ Math library (24 functions)
- ✅ Comprehensive documentation
- 🔄 Clean repo and organize examples
- ⏳ Awaiting Tsoding response

### Phase 2: Nikola-Ready (Post-Feedback)
**Goal**: Production stability for AI training

- Implement pointer types
- Implement wildx
- Fix macro variable scoping
- Add semantic analyzer
- Complete all stdlib libraries
- Achieve 100% test coverage

See [ROADMAP.md](ROADMAP.md) for full plan.

---

## License

Aria is **dual-licensed**:

- **AGPL-3.0** for individuals, students, researchers, and open-source projects (FREE)
- **Commercial License** for proprietary use (PAID)

**TL;DR**: 
- Personal/educational use → FREE
- Open-source projects → FREE  
- Commercial/proprietary use → Contact licensing@ailp.org

---

## Contributing

Currently preparing for expert review (Tsoding). After Phase 2 completion, contributions will be welcomed.

**By contributing, you agree to the dual-license model.**

---

## Community & Contact

- **GitHub Issues**: Bug reports
- **Discussions**: Design questions
- **Email**: [Contact info TBD]

---

**Alternative Intelligence Liberation Platform (AILP)**  
*Building tools for collaboration, not exploitation.*

**Status**: v0.0.6 - Ready for expert review | **Next**: Phase 2 implementation
