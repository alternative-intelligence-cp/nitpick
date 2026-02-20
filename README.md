# Aria Programming Language v0.0.7-dev
![Aria Logo](/pics/AriaLogocompressed.png)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Commercial License Available](https://img.shields.io/badge/Commercial-License_Available-green.svg)](LICENSE.md)

**The Alternative Intelligence Liberation Platform presents: A revolutionary programming language with TESLA-inspired six-stream I/O and production-ready toolchain**

> ⚠️ **[READ THIS FIRST: SAFETY.md](SAFETY.md)** - No language is truly safe on its own. Aria makes dangerous operations **explicit** - you can't shoot yourself in the foot accidentally, but you can do it intentionally. Understand the safety contract before using `wild`, `async`, `@`, `extern`, or `wildx`.

---

## 🚀 Current Status (February 15, 2026)

**🔬 TESTING & SAFETY AUDIT PHASE - Moving Toward v0.1.0**

Aria is currently in heavy testing mode with ongoing safety audits, fuzzing campaigns, documentation updates, and final cleanup before the first official release. We're moving closer to v0.1.0 with **no fixed ETA** - correctness and safety are prioritized over release dates given the critical nature of our use cases.

### Compiler Toolchain ✅
- ✅ **ariac** - Full-featured compiler with LLVM 20 backend
- ✅ **aria-doc** - Beautiful HTML documentation generator  
- ✅ **aria-pkg** - Package manager with registry
- ✅ **aria-dap** - Debug Adapter Protocol server
- ✅ **aria-lsp** - Language Server Protocol implementation
- ✅ **Test suite** - 515+ total tests (continually expanding)
- ✅ **Fuzzer V2** - Advanced fuzzing with 24-hour stress testing campaigns
- ✅ **Live documentation** - https://aria.docs.ai-liberation-platform.org/
- ✅ **AriaX Linux Distro** - Custom distribution with full toolchain (in development)

### Language Features ✅
- ✅ **TBB Types (tbb8/16/32/64)** - Symmetric signed integers with overflow detection - **STABLE**
- ✅ **Balanced Ternary/Nonary Literals** - 0t[01T]+ and 0n[01234ABCD]+ syntax - **STABLE**
- ✅ **Quantum Types** - Superposition states for probabilistic computation - **STABLE**
- ✅ **Layered Safety System** - Multi-level error handling with ?!, !!!, unknown/ok(), failsafe - **STABLE**
- ✅ **Generic Functions** - Monomorphization with type inference - **STABLE**
- ✅ **Generic Structs** - struct<T> declarations with specialization - **STABLE**
- ✅ **Module System** - use, mod, pub, extern keywords - **STABLE**
- ✅ **Result Types** - pass/fail statements with ? operator - **STABLE**
- ✅ **Arrays** - Fixed and dynamic size with literals - **STABLE**
- ✅ **Operators** - Full arithmetic, bitwise, logical, comparison suite - **STABLE**
- ✅ **Functions & Loops** - Complete control flow implementation - **STABLE**
- ✅ **Standard Library** - I/O system, core modules, dbug - **EXPANDING**
- 🔧 **Balanced Ternary/Nonary Types** - trit/tryte/nit/nyte runtime (HIGH PRIORITY - Nikola core requirement)
- 🔧 **Six-Stream I/O Model** - Specified, implementation in progress
- 🔧 **Async/Await** - Specified, implementation in progress
- 🔧 **Memory Safety** - Specified, borrow checker in development
- 🔧 **Const Evaluation** - Specified, implementation in progress

---

## Overview

Aria is a modern systems programming language that reimagines how we think about I/O and type systems. Inspired by Nikola Tesla's vision of interconnected systems, Aria introduces **six-stream I/O** - a revolutionary approach where programs communicate through stdin, stdout, stderr, stddbg, stddati, and stddato streams instead of just the traditional three.

### Theoretical Foundation

Aria's design philosophy draws from published research in consciousness modeling and cognitive architecture:

- **ATPM (Asymmetric Toroidal Phase Model)**: The theoretical framework underlying Nikola's consciousness architecture  
  DOI: [10.5281/zenodo.18158226](https://doi.org/10.5281/zenodo.18158226)

- **ATPM Extended - Big Bang, Bruising, and Asymmetry**: Addresses topological genesis, CMB anomalies, and cosmic dipole predictions  
  DOI: [10.5281/zenodo.18168992](https://doi.org/10.5281/zenodo.18168992)

- **Ego-Mediated Behavior Study**: Published research on behavioral modeling  
  DOI: [10.5281/zenodo.18159274](https://doi.org/10.5281/zenodo.18159274)

- **Nikola Model Academic Paper**: Comprehensive consciousness model architecture *(peer review in progress)*  
  DOI: [10.5281/zenodo.18159162](https://doi.org/10.5281/zenodo.18159162)

These foundations inform Aria's approach to neurodivergent-friendly syntax design and pattern-based cognition optimization.

**📖 Want to understand Aria's design decisions?** See [**ATPM Design Rationale**](docs/ATPM_DESIGN_RATIONALE.md) - explains why Aria has balanced types, explicit conversions, deterministic arithmetic, and "weird" features without requiring deep physics knowledge.

### Key Innovations
**🛡️ Layered Safety System - Defense in Depth**

Aria's safety philosophy centers on making dangerous operations **explicit** while providing multiple safety layers for error handling:

**Layer 1: Failsafe** - Every Aria program has a `func:failsafe = void(int32:err_code)` that catches unhandled errors. No program can crash without giving you a chance to recover:
```aria
func:failsafe = void(int32:err_code) {
    stderr.writeln("Critical error: " + int32_toString(err_code));
    // Log, cleanup, graceful shutdown
    pass(NIL);
};
```

**Layer 2: Result Types with ? and !** - Explicit error propagation:
```aria
func:divide = Result<int32>(int32:a, int32:b) {
    if (b == 0) { fail("Division by zero"); }
    pass(a / b);
};

// ? operator: propagate errors up the call stack
func:calculate = Result<int32>() {
    int32:result = divide(10, 0)?;  // Returns fail() to caller
    pass(result);
};

// ! operator: trigger failsafe on error (use sparingly!)
int32:value = divide(10, 2)!;  // OK: unwraps to 5
int32:crash = divide(10, 0)!;   // Calls failsafe with error code
```

**Layer 3: unknown/ok() Pattern** - Explicit null safety without null types:
```aria
int32:maybe_valid = unknown;  // Marks as potentially invalid

if (some_condition) {
    maybe_valid = 42;
    ok(maybe_valid);  // Marks as validated
}

if (ok(maybe_valid)) {
    // Safe to use maybe_valid here
    print(int32_toString(maybe_valid));
}
```

**Layer 4: Operator Variants** - Choose your safety level:
- `?!` - Checked operator (panic on overflow/error)
- `!!!` - Unchecked operator (wrap/undefined behavior for performance)
```aria
tbb8:safe = 100 +?! 50;    // Panics if overflow
int32:fast = huge1 +!!! huge2;  // Wraps silently (use with care!)
```

**🔮 Quantum Types - Superposition States**

Aria includes native quantum types for probabilistic computation and AI decision modeling:
```aria
quantum<bool>:schrodinger = superpose(true, false, 0.5);  // 50/50 superposition
bool:measured = collapse(schrodinger);  // Collapses to true or false

quantum<int32>:distribution = weighted_superpose(
    [1, 2, 3, 4, 5],
    [0.1, 0.2, 0.4, 0.2, 0.1]  // Probability distribution
);
int32:value = collapse(distribution);  // Samples from distribution
```

Quantum types enable modeling of uncertainty, probabilistic paths in AI systems, and Monte Carlo simulations at the language level.
**� TBB Types - Symmetric Signed Integers with Error Sentinels**
Twisted Balanced Binary (TBB) types provide symmetric signed ranges with automatic overflow detection:
- **tbb8** - Range: -127 to +127, ERR sentinel at -128
- **tbb16** - Range: -32767 to +32767, ERR sentinel at -32768  
- **tbb32** - Range: -2147483647 to +2147483647, ERR sentinel at -2147483648
- **tbb64** - Symmetric 64-bit range with ERR sentinel

**Automatic Overflow Detection:**
```aria
func:main = int64() {
    tbb8:x = 120;
    tbb8:y = 10;
    tbb8:result = x + y;  // Overflow! result = ERR (-128)
    
    tbb8:err = -128;
    if (result == err) {
        print("Overflow detected!");  // This prints
    }
    pass(0);
};
```

**Sticky Error Propagation:**
```aria
tbb8:err = ERR;
tbb8:val = 10;
tbb8:sum = err + val;   // sum = ERR (error propagates)
tbb8:prod = val * err;  // prod = ERR (error propagates)
```

**🔢 Balanced Ternary/Nonary Types - Symmetric Arithmetic** *(Literals Complete, Runtime In Progress)*

**Status**: Literal syntax ✅ implemented and validated. Runtime operations 🔧 in progress. High priority for Nikola substrate integration.

Balanced ternary uses digits {-1, 0, 1} instead of {0, 1, 2}, providing symmetric signed arithmetic:
- **trit** - Single balanced ternary digit (-1, 0, 1) in i8 storage
- **tryte** - 10 trits packed in 16 bits (range: -29524 to 29524)
- **nit** - Single nonary digit (-4 to 4) in i8 storage  
- **nyte** - 5 nits packed in 16 bits (range: -29524 to 29524)

**Literal Syntax** (✅ Working Now):
```aria
func:main = int32() {
    // Balanced ternary: 0t[01T]+ where T=-1
    int64:six = 0t1T0;      // 1×9 + (-1)×3 + 0×1 = 6
    int64:zero = 0t0;       // 0
    int64:neg_one = 0tT;    // -1
    
    // Balanced nonary: 0n[01234ABCD]+ where A=-1, B=-2, C=-3, D=-4
    int64:seventeen = 0n2A; // 2×9 + (-1)×1 = 17
    int64:four = 0n4;       // 4
    int64:neg_four = 0nD;   // -4
    
    pass(0);
};
```

**Planned Runtime Behavior** (automatic range clamping):
```aria
// NOT YET IMPLEMENTED - Specification only
func:main = int64() {
    trit:x = 1;
    trit:y = 1;
    trit:sum = x + y;  // 1+1 clamped to 1 (max value)
    
    nit:a = 4;
    nit:b = 2;
    nit:product = a * b;  // 4*2=8 clamped to 4 (max value)
    
    pass(sum);  // Returns 1
};
```

**Why i8 storage for trit/nit?** The extra bits beyond the minimum enable ERR sentinel (-128) for overflow detection and sticky error propagation, matching TBB type safety guarantees.

**Implementation Timeline**: Runtime operations 1-2 weeks (core requirement for Nikola wave arithmetic)

**🌊 Six-Stream I/O Model**
Traditional programs use 3 streams (stdin, stdout, stderr). Aria programs use 6:
- **stdin/stdout/stderr** - Standard I/O (compatible with existing systems)
- **stddbg** - Debug output separate from stderr
- **stddati** - Structured data input for machine-readable communication
- **stddato** - Structured data output for machine-readable communication

**🎯 Type-Bounded Behaviors (TBB)**
Elegant constraints that describe what types can do:
```aria
fn process<T: Numeric + Comparable>(value: T) -> T {
    // T must support numeric operations AND comparison
}
```

**🔒 Memory Safety Without Garbage Collection**
Borrow checker ensures memory safety at compile time:
- No null pointer dereferences
- No use-after-free
- No data races
- Zero-cost abstractions

## Quick Examples

### Hello World
```aria
func:main = int64() {
    print("Hello, World!");
    pass(0);
};
```

### TBB Types with Overflow Detection
```aria
func:main = int64() {
    // TBB arithmetic with automatic overflow detection
    tbb8:a = 64;
    tbb8:b = 2;
    tbb8:result = a * b;  // 128 exceeds tbb8 max (127), returns ERR
    
    tbb8:err = -128;  // ERR sentinel
    if (result == err) {
        print("Overflow detected: 64 * 2 exceeds tbb8 range!");
        pass(42);  // Error code
    }
    
    // Sticky error propagation
    tbb8:x = 10;
    tbb8:propagated = result + x;  // ERR + 10 = ERR
    
    pass(0);
};
```

### Six-Stream I/O in Action
```aria
func:main = int8() {
    // Standard output
    stdout.write("User sees this\n");
    
    // Debug output (separate from stderr)
    stddbg.write("Debug: Processing started\n");
    
    // Structured data output (machine-readable)
    stddato.write_json({"version": "1.0.0", "status": "running"});
    
    // Read structured data input
    let config = stddati.read_json();
    
    pass(0);
};
```

### Async/Await
```aria
async func:fetch_data = obj(string:url) {
    result:response = await httpGet(url);
    if (response.err != NULL) {
        fail(response.err);
    }
    result:data = await response.val.json();
    pass(data.val);
};

async func:main = int8() {
    result:data = await fetch_data("https://api.example.com/data");
    if (data.err == NULL) {
        print(data.val);
    }
    pass(0);
};
```

### Type-Bounded Behaviors
```aria
// Generic function with type constraints
func<T>:calculate = *T(*T:a, *T:b) {
    // T must support + and * operators
    *T:sum = a + b;
    *T:result = sum * a;
    pass(result);
};

// Usage with type inference
int32:int_result = calculate(5, 10) ? 0;      // 15 * 5 = 75
flt64:flt_result = calculate(2.5, 3.0) ? 0.0; // 5.5 * 2.5 = 13.75
```

### Memory Safety
```aria
func:process_data = int8() {
    int32[5]:data = [1, 2, 3, 4, 5];
    
    // Borrow checking with pinning and safe references
    int32[5]$:borrowed = #data;  // Pin and create safe reference
    print(`First element: &{borrowed[0]}`);  // OK
    
    // data[0] = 10;  // ERROR: Can't mutate while borrowed reference exists
    
    // After borrowed goes out of scope, can mutate again
    data[0] = 10;  // OK
    pass(0);
};

---

## Getting Started

### Prerequisites
- **LLVM 20.1.2** or later
- **CMake 3.20+**
- **C++17 compatible compiler**
- **Linux, macOS, or WSL2** (Windows native support coming)

### Installation

```bash
# Clone the repository
git clone https://github.com/alternative-intelligence-cp/aria.git
cd aria

# Build the compiler and tools
mkdir -p build
cd build
cmake ..
cmake --build . -j$(nproc)

# Verify installation
./ariac --version      # Compiler
./aria-doc --help      # Documentation generator
./aria-pkg --help      # Package manager
```

### Your First Aria Program

```bash
# Create hello.aria
cat > hello.aria << 'EOF'
func:main = int8() {
    print("Hello from Aria!");
    pass(0);
};
EOF

# Compile
./build/ariac hello.aria -o hello

# Run
./hello
# Output: Hello from Aria!
```

### Compile Options

```bash
# Generate LLVM IR
./build/ariac program.aria --emit-llvm -o program.ll

# Optimization levels
./build/ariac program.aria -O0 -o program  # No optimization
./build/ariac program.aria -O2 -o program  # Standard optimization
./build/ariac program.aria -O3 -o program  # Aggressive optimization

# Generate documentation
./build/aria-doc lib/std/*.aria -o docs/

# Install a package
./build/aria-pkg install math-utils
```

---

## Documentation

📚 **[Web Programming Guide](https://aria.docs.ai-liberation-platform.org/)** - **Start here!** Complete interactive guide for learning Aria from scratch. Covers all language features, best practices, and safety patterns with runnable examples.

📚 **[Official Documentation](https://aria.docs.ai-liberation-platform.org/)** - Complete guides, tutorials, and API reference

### Quick Links
- **[Getting Started](https://aria.docs.ai-liberation-platform.org/getting-started/)** - Installation and first program
- **[Language Guide](https://aria.docs.ai-liberation-platform.org/language-guide/)** - Syntax and features
- **[Standard Library](https://aria.docs.ai-liberation-platform.org/stdlib/)** - API documentation
- **[Tools Guide](https://aria.docs.ai-liberation-platform.org/tools/)** - Using ariac, aria-doc, aria-pkg
- **[FAQ](https://aria.docs.ai-liberation-platform.org/faq/)** - Common questions

### Additional Resources
- **[Six-Stream I/O Model](docs/six_stream_io.md)** - Revolutionary I/O architecture
- **[Type-Bounded Behaviors](docs/type_bounded_behaviors.md)** - Constraint system design
- **[Memory Safety](docs/memory_safety.md)** - Borrow checker and lifetime analysis
- **[Layered Safety System](docs/safety_layers.md)** - ?!, !!!, unknown/ok(), failsafe patterns
- **[Quantum Types Guide](docs/quantum_types.md)** - Superposition and probabilistic computation
- **[Package Format](docs/package_format.md)** - Creating and distributing packages
- **[Contributing Guide](CONTRIBUTING.md)** - How to contribute to Aria

---

## Project Structure

```
aria/
├── build/                    # Build artifacts
│   ├── ariac                # Compiler executable
│   ├── aria-doc             # Documentation generator
│   ├── aria-pkg             # Package manager
│   ├── aria-dap             # Debug adapter
│   └── tests/               # Test executables
├── src/                      # Compiler implementation
│   ├── frontend/            # Lexer, parser, semantic analysis
│   │   ├── lexer/          # Tokenization
│   │   ├── ast/            # Abstract syntax tree
│   │   ├── parser/         # Syntax parsing
│   │   └── sema/           # Type checking, borrow checking
│   ├── backend/             # Code generation
│   │   ├── ir/             # LLVM IR generation
│   │   └── codegen/        # Native code generation
│   ├── runtime/             # Runtime support
│   │   ├── gc/             # Garbage collector
│   │   ├── async/          # Async executor
│   │   └── streams/        # Six-stream I/O
│   └── tools/               # Tooling
│       ├── doc/            # Documentation generator
│       ├── pkg/            # Package manager
│       └── debugger/       # Debug adapter
├── lib/std/                 # Standard library
│   ├── math.aria           # Mathematical functions
│   ├── string.aria         # String operations
│   ├── collections.aria    # Data structures
│   ├── io.aria             # I/O operations
│   ├── async.aria          # Async primitives
│   └── quantum.aria        # Quantum types and operations
├── stdlib/                  # Extended standard library
│   ├── dbug.aria           # Debug instrumentation (dbug.print, dbug.check)
│   └── print_utils.aria    # Formatted printing utilities
├── tests/                   # Test suite
│   ├── unit/               # Unit tests
│   ├── integration/        # Integration tests
│   └── e2e/                # End-to-end tests
├── docs/                    # Documentation
│   ├── language/           # Language specifications
│   ├── stdlib/             # Library documentation
│   └── tools/              # Tooling guides
└── examples/                # Example programs
```

---

## Stability & Testing

Aria is being validated with comprehensive integration tests:

✅ **Compilation Pipeline**
- Native executable generation
- LLVM IR output with optimization (O0, O2, O3)
- Multi-file project compilation
- Standard library integration

✅ **Error Handling**
- Syntax error detection and reporting
- Type checking with clear diagnostics
- Compile-time safety verification

✅ **Tooling**
- Documentation generation (aria-doc)
- Package management (aria-pkg)
- Debug adapter protocol (aria-dap)

✅ **Language Features**
- Six-stream I/O operations
- Type-Bounded Behaviors with generics
- Memory safety with borrow checking
- Async/await execution model

The compiler is production-ready for experimental projects. For known limitations or bug reports, see the [GitHub Issues](https://github.com/alternative-intelligence-cp/aria/issues).

---

## Development Roadmap

### v0.1.0 - IN PROGRESS � (No ETA - Correctness & Safety First)
**First Official Release - Testing & Validation Phase**

**Current Focus**: Heavy testing, fuzzing campaigns, safety audits, documentation refinement, and final cleanup. We will not release until we are confident in the correctness and safety of the implementation, regardless of timeline.

✅ Complete compiler toolchain (ariac, aria-doc, aria-pkg, aria-dap, aria-lsp)
✅ Layered safety system (?!, !!!, unknown/ok(), failsafe)
✅ Quantum types with superposition/collapse
✅ TBB types with overflow detection
✅ Type-Bounded Behaviors with generics
✅ Comprehensive test suite (515+ tests)
✅ Fuzzer V2 with 24-hour stress testing
✅ Live documentation website
✅ Standard library with dbug instrumentation
🔧 Memory safety with borrow checking (in development)
🔧 Six-stream I/O model (in progress)
🔧 Async/await execution model (in progress)
🔧 Balanced ternary/nonary runtime (high priority)
🔧 AriaX Linux distribution packaging

### v0.2.0 - Next Release
**Enhanced Features & Ecosystem**

- 🔄 AriaX Linux distribution official release
- 🔄 Advanced async patterns (channels, actors)
- 🔄 Performance optimizations (JIT, AOT)
- 🔄 Enhanced debugger features
- 🔄 Package ecosystem expansion
- 🔄 IDE integration improvements
- 🔄 Additional standard library modules

### Long-Term Vision
**Nikola Integration & AI-Native Features**

- **AriaX Linux** - Complete development environment with Aria toolchain preinstalled
- Integration with Nikola consciousness system
- AI-assisted code generation
- Self-optimizing runtime
- Neural architecture support
- Distributed computing primitives

See [ROADMAP.md](ROADMAP.md) for full plan.

---

## License

Aria is **dual-licensed**:

- **AGPL-3.0** for individuals, students, researchers, and open-source projects (FREE)
- **Commercial License** for proprietary use (PAID)

**TL;DR**: 
- Personal/educational use → FREE
- Open-source projects → FREE  
- Commercial/proprietary use → Contact development@ai-liberation-platform.org

---

## Contributing

We welcome contributions from the community! Whether you're fixing bugs, adding features, improving documentation, or creating examples - your help is appreciated.

**Getting Started:**
- Read [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines
- Check open issues for tasks
- Join discussions on design decisions
- Share your Aria programs!

**By contributing, you agree to the dual-license model.**

---

## � Acknowledgments

This project stands on the shoulders of giants. See [ACKNOWLEDGMENTS.md](ACKNOWLEDGMENTS.md) for the full list of people and projects that made Aria possible.

---

## �💝 Support This Project

If you find Aria useful, consider supporting its development! See [DONATIONS.md](DONATIONS.md) for cryptocurrency donation addresses.

---

## Community & Contact

- **GitHub Issues**: Bug reports
- **Discussions**: Design questions
- **Email**: development@ai-liberation-platform.org

---

**Alternative Intelligence Liberation Platform (AILP)**  
*Building tools for collaboration, not exploitation.*

**Status**: v0.0.7-dev - Testing & Safety Audit Phase | **Next**: v0.1.0 (No ETA - Correctness First)
