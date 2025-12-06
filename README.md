# Aria Programming Language
![Aria Logo](/pics/AriaLogocompressed.png)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Commercial License Available](https://img.shields.io/badge/Commercial-License_Available-green.svg)](LICENSE.md)

**A high-performance systems programming language with hybrid memory management, explicit syntax, and construction-grade safety.**

---

## Overview

Aria bridges the gap between managed safety and raw metal control through:

- **Hybrid Memory Model**: Garbage collection (gc) by default, manual memory (wild) when needed, explicit stack allocation
- **Explicit Syntax**: Mandatory semicolons, braces, and type annotations - no ambiguity
- **Result Types**: All functions return Result objects - no bypassing error handling
- **Comptime Evaluation**: Zig-style compile-time execution for zero-cost abstractions
- **NASM-Style Macros**: Powerful preprocessor with context stacks
- **Exotic Types**: Native support for balanced ternary (trit/tryte) and nonary (nit/nyte)

## Quick Example

```aria
func:fibonacci = int64(int64:n) {
    if (n <= 1) {
        return { err: 0, val: n };
    }
    
    result:a = fibonacci(n - 1);
    result:b = fibonacci(n - 2);
    
    if (a.err != 0) { return a; }
    if (b.err != 0) { return b; }
    
    return { err: 0, val: a.val + b.val };
};
```

## Philosophy

Aria is designed for **construction-grade safety** - code where someone's life might depend on getting it right:

- **Explicit over implicit**: Type specifications like material specs on blueprints
- **No shortcuts allowed**: Mandatory error handling, explicit terminators
- **Progressive disclosure**: Safe defaults (gc), power when needed (wild)

## Installation

*Installation instructions coming soon - compiler currently in late-stage development*

## Documentation

- [Language Specification](docs/research/Aria_v0.0.6_Specs.txt)
- [Contributing Guidelines](CONTRIBUTING.md)
- Examples: `build/` directory contains test cases

## License

Aria is **dual-licensed**:

- **AGPL-3.0** for individuals, students, researchers, and open-source projects (FREE)
- **Commercial License** for proprietary use in commercial products (PAID)

See [LICENSE.md](LICENSE.md) for full details.

**TL;DR**: 
- Personal/educational use → FREE
- Open-source projects → FREE  
- Commercial/proprietary use → Contact licensing@ailp.org

## Status

🚧 **Active Development** - Compiler is feature-complete (14/14 core features), entering bootstrap phase.

- ✅ Hybrid memory management (gc/wild/stack)
- ✅ Result types with mandatory error handling
- ✅ NASM-style macro preprocessor
- ✅ String interpolation, lambdas, closures
- ✅ Pattern matching (pick/fall)
- ✅ LLVM IR code generation
- 🚧 Standard library (in progress)
- 🚧 Self-hosting compiler (planned)

## Contributing

We welcome contributions! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

By contributing, you agree to the dual-license model, ensuring Aria remains free for education while supporting sustainable development.

## Community

- **GitHub Issues**: Bug reports and feature requests
- **Discussions**: Design philosophy, use cases, questions

---

**Alternative Intelligence Liberation Platform (AILP)**  
*Building tools for collaboration, not exploitation.*
