# Aria Programming Language v0.0.7-dev
![Aria Logo](/pics/AriaLogocompressed.png)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Commercial License Available](https://img.shields.io/badge/Commercial-License_Available-green.svg)](LICENSE.md)

**The Alternative Intelligence Liberation Platform presents: A revolutionary programming language with TESLA-inspired six-stream I/O and production-ready toolchain**

> ⚠️ **[READ THIS FIRST: SAFETY.md](SAFETY.md)** - No language is truly safe on its own. Aria makes dangerous operations **explicit** - you can't shoot yourself in the foot accidentally, but you can do it intentionally. Understand the safety contract before using `wild`, `async`, `@`, `extern`, or `wildx`.

---

## 🚀 Current Status (December 19, 2025)

**⚠️ ACTIVE DEVELOPMENT - Core Features Implementation In Progress**

### Compiler Toolchain 🔧
- ✅ **ariac** - Full-featured compiler with LLVM 20 backend
- ✅ **aria-doc** - Beautiful HTML documentation generator  
- ✅ **aria-pkg** - Package manager with registry
- ✅ **aria-dap** - Debug Adapter Protocol server
- ✅ **aria-lsp** - Language Server Protocol implementation
- ✅ **Comprehensive test suite** - 48/64 tests passing
- ✅ **Live documentation** - https://aria.docs.ai-liberation-platform.org/

### Language Features (Status) 🔧
- ✅ **TBB Types (tbb8/16/32/64)** - Symmetric signed integers with overflow detection
- ✅ **Balanced Ternary/Nonary** - trit/tryte/nit/nyte with automatic range clamping ⭐ NEW!
- ✅ **Generic Functions** - Monomorphization with type inference
- ✅ **Generic Structs** - struct<T> declarations with specialization
- ✅ **Module System** - use, mod, pub, extern keywords
- ✅ **Result Types** - pass/fail statements with ? operator
- ✅ **Arrays** - Fixed and dynamic size with literals
- 🔧 **Six-Stream I/O Model** - Specified, implementation in progress
- 🔧 **Async/Await** - Specified, implementation in progress
- 🔧 **Memory Safety** - Specified, borrow checker in development
- 🔧 **Const Evaluation** - Specified, implementation in progress

---

## Overview

Aria is a modern systems programming language that reimagines how we think about I/O and type systems. Inspired by Nikola Tesla's vision of interconnected systems, Aria introduces **six-stream I/O** - a revolutionary approach where programs communicate through stdin, stdout, stderr, stddbg, stddati, and stddato streams instead of just the traditional three.

### Key Innovations

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
**🔢 Balanced Ternary/Nonary Types - Symmetric Arithmetic**
Balanced ternary uses digits {-1, 0, 1} instead of {0, 1, 2}, providing symmetric signed arithmetic:
- **trit** - Single balanced ternary digit (-1, 0, 1) in 3 bits
- **tryte** - 10 trits packed in 16 bits (range: -29524 to 29524)
- **nit** - Single nonary digit (-4 to 4) in 4 bits
- **nyte** - 5 nits packed in 16 bits (range: -29524 to 29524)

**Automatic Range Clamping:**
```aria
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

**Why 3 bits for trit?**
The extra bit beyond the minimum 2 enables overflow detection before clamping. Without it, signed arithmetic wraps (1+1=2 becomes -2 in i2), breaking the clamping logic.
**�🌊 Six-Stream I/O Model**
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
│   └── async.aria          # Async primitives
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

### v0.1.0 - IN PROGRESS 🔧 (Target: Q1 2026)
**First Official Release - Feature Implementation**

✅ Complete compiler toolchain (ariac, aria-doc, aria-pkg, aria-dap)
✅ Six-stream I/O model implementation
✅ Type-Bounded Behaviors with generics
✅ Memory safety with borrow checking
✅ Async/await execution model
✅ Comprehensive test suite (Phase 7.7)
✅ Live documentation website
✅ Package management system

### v0.2.0 - Next Release (Q1 2026)
**Enhanced Language Features & Performance**

- 🔄 Advanced async patterns (channels, actors)
- 🔄 Performance optimizations (JIT, AOT)
- 🔄 Enhanced debugger features
- 🔄 Package ecosystem expansion
- 🔄 IDE integration improvements
- 🔄 Additional standard library modules

### Long-Term Vision
**Nikola Integration & AI-Native Features**

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

**Status**: v0.0.6 - Phase 7 Complete, Feature Implementation In Progress | **Next**: Parser Enhancements & Standard Library
