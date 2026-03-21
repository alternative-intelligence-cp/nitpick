# Aria Programming Language v0.1.0

![Aria Logo](/pics/AriaLogocompressed.png)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Commercial License Available](https://img.shields.io/badge/Commercial-License_Available-green.svg)](LICENSE.md)

**The Alternative Intelligence Liberation Platform presents: A systems programming language built for safety, determinism, and AI-native applications**

> ⚠️ **[READ THIS FIRST: SAFETY.md](SAFETY.md)** — No language is truly safe on its own. Aria makes dangerous operations **explicit** — you can't shoot yourself in the foot accidentally, but you can do it intentionally. Understand the safety contract before using `wild`, `async`, `@`, `extern`, or `wildx`.

---

## Current Status (March 20, 2026)

**v0.1.0-rc1 — Release candidate, all known bugs resolved**

The compiler has reached release-candidate status. All 27 catalogued compiler bugs have been found and fixed, all workarounds removed, safety-critical validation is complete, and the full test suite passes clean.

- **All compiler bugs resolved** — Bugs #1–#27 have been systematically identified, fixed, and verified. This includes the final two safety-critical bugs: #26 (assignment type mismatch causing stale SSA values) and #27 (LBIM div/mod ABI mismatch). See [KNOWN_ISSUES.md](KNOWN_ISSUES.md) for the full catalogue.
- **Safety-critical validation** — 10 dedicated safety test files in `tests/safety_critical/` verify IEEE 754 compliance, overflow/underflow boundaries, catastrophic cancellation detection, determinism, energy conservation, and TBB sticky error propagation. All pass.
- **`aria_packages` library ecosystem** — 27 packages (LIB-1 through LIB-27) with 543+ assertions, all passing. Packages cover math, PRNG, graphics, data encoding, string manipulation, JSON, CLI args, a virtual console stack, and AI-focused libraries (decision trees, quantum entanglement, gradient fields, resource memory). See the [Packages](#aria_packages-library-ecosystem) section below.
- **681 test files** — Comprehensive test suite covering compiler features, safety-critical validation, fuzzer corpus, and ecosystem integration.
- **Language specialist model** — V3 training complete (Qwen 7B LoRA, 71% pattern match). V4 training in progress with a higher-quality corpus (2,484 examples, 100% drill compilation rate, proper `Result<T>` + `?` patterns, `NIL`/`void` correctness).
- **Fuzzing** — Continuous fuzzing via Fuzzer V2. 24-hour stress campaigns, crashes archived and regression-tested. Valgrind-clean runtime.
- **External tester onboarded** — Getting Started guide created for first external tester (non-programmer, Linux Mint, building crypto data tools).
- **No fixed ETA for final release** — Correctness and safety come before dates, given the safety-critical use cases this language is built for.

---

## Compiler Toolchain

| Tool | Status | Description |
|---|---|---|
| `ariac` | ✅ Stable | Full compiler, LLVM 20 backend |
| `aria-lsp` | ✅ Available | Language Server Protocol |
| `aria-dap` | ✅ Available | Debug Adapter Protocol |
| `aria-doc` | ✅ Available | Documentation generator |
| `aria-pkg` | ✅ Available | Package manager |
| Fuzzer V2 | ✅ Active | 24-hour stress campaigns |
| Specialist model | 🔧 V4 Training | Qwen 7B LoRA, V3 complete (71% match), V4 in progress |
| AriaX Linux | 🔧 In progress | Custom distro with full toolchain |
| `aria_packages` | ✅ Active | 27 packages, 543+ assertions, all passing |

---

## Language Features

### Stable
- **All primitive types** — int8/16/32/64, uint8/16/32/64, flt32/flt64, bool, string
- **TBB Types (tbb8/16/32/64)** — Symmetric signed integers with overflow sentinel (-128/ERR)
- **Balanced Ternary/Nonary Literals** — `0t[01T]+` and `0n[01234ABCD]+` syntax
- **Quantum Types** — Superposition states for probabilistic computation
- **Generic Functions and Structs** — Monomorphization with type inference
- **Result Types** — `pass`/`fail` with `?` propagation and `!` unwrap, `Result<T>` signatures
- **Layered Safety** — `?!` (checked), `!!!` (unchecked), `unknown`/`ok()`, `failsafe`
- **All loop forms** — `while`, `for(;;)` (C-style), `for (i in a..b)` (range), `loop`, `till`, `when/then/end`
- **Control flow** — `if/else`, `pick` (exhaustive match), `break`, `continue`, `fall`
- **Module system** — `use`, `mod`, `pub`, `extern`
- **Closures** — First-class functions with capture
- **Borrow checker** — Compile-time memory safety analysis
- **SIMD types** — Vector arithmetic via LLVM intrinsics
- **Atomic types** — Lock-free concurrent primitives
- **Dimensional algebra** — Unit-typed arithmetic
- **Operators** — Full suite including `@` (address), `#` (pin), `->` (arrow), `..`/`...` (ranges)
- **Template literals** — `` `&{variable}` `` string interpolation
- **Standard library** — string_convert, string (manipulation), string_builder, print_utils, wave/wavemech, complex, dbug, quantum, atomic, io (file streams), math (transcendentals), linalg (linear algebra), collections (Vec, Map, Set, Graph), json, toml, binary, net (TCP sockets)

### In Progress / Specified
- **Balanced Ternary/Nonary runtime** — Literal parsing ✅; trit/tryte/nit/nyte arithmetic 🔧 (high priority — Nikola core requirement)
- **Six-stream I/O** — stdin/stdout/stderr/stddbg/stddati/stddato specified; implementation in progress
- **Async/Await** — Specified and partially implemented
- **`fail()` from user functions** — Planned; current workaround is sentinel values or Result<T> from wrapper functions
- **`uint32_toString`** — Missing from stdlib; workaround: `int64_toString(@cast<int64>(val))`
- **Arrays in structs** — Under investigation
- **NIL ↔ void bridge** — `void` and `*` are mostly in `extern` blocks; bridge semantics being worked out

---

## `aria_packages` Library Ecosystem

A collection of tested, versioned utility libraries built alongside the compiler. Each package has a `src/` module, a `tests/` file with 15 tests, and where FFI is needed, a C `shim/`. Every package passes its full test suite before being committed — they serve simultaneously as integration tests for the compiler and as real utilities.

**Current packages (27 total, 543+ assertions, all passing):**

| # | Package | Description |
|---|---------|-------------|
| 1 | `aria-math` | Trig, exp, log, rounding via C libm |
| 2 | `aria-rand` | xorshift64 pseudo-random number generator |
| 3 | `aria-color` | RGBA packing/unpacking and pixel transforms |
| 4 | `aria-vec` | 2D/3D float64 vector math (dot, cross, length) |
| 5 | `aria-buf` | Byte/word packing for uint64 buffers (little-endian) |
| 6 | `aria-clamp` | min, max, clamp, abs, sign for int64/uint64 |
| 7 | `aria-bits` | Bit test/set/clear/flip, nibble extraction, byte popcount |
| 8 | `aria-ascii` | ASCII character classification and conversion |
| 9 | `aria-fixed` | Q32.32 fixed-point arithmetic on uint64 |
| 10 | `aria-freq` | Frequency/period/baud integer arithmetic |
| 11 | `aria-mux` | Bit-select, field insert/extract, mask ops, blend |
| 12 | `aria-conv` | Saturating narrowing and float/int conversion |
| 13 | `aria-hash` | FNV-1a and djb2 string hashing |
| 14 | `aria-endian` | Big/little-endian byte-swap for 16/32/64-bit values |
| 15 | `aria-uuid` | UUID v4 generation and formatting |
| 16 | `aria-zigzag` | Zigzag encode/decode for signed integer interleaving |
| 17 | `aria-display` | ANSI/termios terminal rendering (virtual console display) |
| 18 | `aria-input` | Raw keyboard input with SNES-style button mapping |
| 19 | `aria-audio` | Software synthesis, MIDI note table, 4 channels (dry mode) |
| 20 | `aria-console` | 16-bit memory-mapped address space + 60fps frame scheduler |
| 21 | `aria-str` | String utilities (pad, trim, repeat, contains, split) |
| 22 | `aria-json` | Lightweight JSON encoding for basic types |
| 23 | `aria-args` | CLI argument parser (flags, key-value, positional) |
| 24 | `aria-decision-t` | Decision tree classifier (entropy, information gain) |
| 25 | `aria-entangled` | Quantum-inspired entangled variable pairs |
| 26 | `aria-resource-mem` | RAII-style resource lifecycle management |
| 27 | `aria-gradient-field` | 3D gradient field computation for continuous optimization |

**Demo:** `aria_ecosystem/demos/aria-jrpg-demo/` — an interactive JRPG battle scene using all four virtual console libraries (LIB-17-20) plus aria-rand. Demonstrates the full stack: ANSI rendering, raw input, audio SFX, and the console memory map.

Packages live in `aria_ecosystem/aria_packages/`. FFI-backed packages (aria-math, aria-display, aria-input, aria-audio) include pre-built shim `.so` files. Pure-Aria packages (aria-rand, aria-clamp, aria-bits, aria-ascii, etc.) can be `use`d directly.

---

## Overview

Aria is a systems programming language that reimagines I/O and type systems for AI-native applications. Inspired by Nikola Tesla's vision of interconnected systems, Aria introduces **six-stream I/O** where programs communicate through stdin, stdout, stderr, stddbg, stddati, and stddato — separating human-readable, debug, and machine-readable data at the language level.

The language is being built as the primary substrate for **Nikola**, a consciousness architecture based on the Asymmetric Toroidal Phase Model (ATPM). Aria's features — balanced types, deterministic arithmetic, explicit memory lifecycle, and quantum superposition types — are not arbitrary; they are requirements of the Nikola model.

### Research Foundation

- **ATPM (Asymmetric Toroidal Phase Model):** DOI [10.5281/zenodo.18158226](https://doi.org/10.5281/zenodo.18158226)
- **ATPM Extended:** DOI [10.5281/zenodo.18168992](https://doi.org/10.5281/zenodo.18168992)
- **Ego-Mediated Behavior Study:** DOI [10.5281/zenodo.18159274](https://doi.org/10.5281/zenodo.18159274)
- **Nikola Model Architecture:** DOI [10.5281/zenodo.18159162](https://doi.org/10.5281/zenodo.18159162)
- **Design rationale** (non-physicist version): [docs/ATPM_DESIGN_RATIONALE.md](docs/ATPM_DESIGN_RATIONALE.md)

---

## Key Language Features

### Layered Safety System

Aria's safety philosophy: make dangerous operations **explicit**, provide multiple layers for error handling, and never let a program crash silently.

**Layer 1: Failsafe** — Every Aria program has a `failsafe` that catches unhandled errors:
```aria
func:failsafe = NIL(int32:err_code) {
    // Log, cleanup, graceful shutdown
};
```

**Layer 2: Result Types** — Explicit error propagation with concise syntax:
```aria
func:divide = Result<int32>(int32:a, int32:b) {
    if (b == 0i32) { fail("Division by zero"); }
    pass(a / b);
};

int32:result = divide(10i32, 2i32) ? 0i32;  // Unwrap with default
int32:strict = divide(10i32, 0i32)!;         // Unwrap or trigger failsafe
```

**Layer 3: Unknown/Ok Pattern** — Explicit null safety without null types:
```aria
int32:maybe_valid = unknown;
if (some_condition) {
    maybe_valid = 42i32;
    ok(maybe_valid);
}
if (ok(maybe_valid)) {
    drop(println(int32_toString(maybe_valid)));  // Safe here
}
```

**Layer 4: Operator Variants** — Choose your safety level:
```aria
tbb8:safe = 100tbb8 +?! 50tbb8;     // Checked: panics on overflow
int32:fast = huge1 +!!! huge2;       // Unchecked: wraps silently
```

### For Loops — Two Forms

```aria
// C-style: precise control
for(int32:i = 0i32; i < 10i32; i += 1i32) {
    drop(println(int32_toString(i)));
}

// Range-based: clean iteration
for (i in 1..10) {          // inclusive: 1, 2, ..., 10
    drop(println(int64_toString(i)));
}
for (i in 1...10) {         // exclusive: 1, 2, ..., 9
    drop(println(int64_toString(i)));
}
for (int32:i in 0...5) {    // with explicit type annotation
    drop(println(int32_toString(i)));
}
```

> Note: `i++` is not supported. Use `i += 1i32` or `i = i + 1i32`.

### TBB Types — Symmetric Signed Integers

```aria
tbb8:x = 120tbb8;
tbb8:y = 10tbb8;
tbb8:result = x + y;     // Overflow: result = ERR (-128)
tbb8:err = -128tbb8;
if (result == err) {
    drop(println("Overflow detected!"));
}

// Sticky error propagation
tbb8:sum = result + x;   // ERR + anything = ERR
```

### Quantum Types

```aria
quantum<bool>:choice = superpose(true, false, 0.5);
bool:measured = collapse(choice);

quantum<int32>:distribution = weighted_superpose(
    [1i32, 2i32, 3i32],
    [0.2, 0.5, 0.3]
);
int32:sampled = collapse(distribution);
```

### Balanced Ternary Literals (Syntax Complete)

```aria
int64:six     = 0t1T0;    // 1×9 + (-1)×3 + 0×1 = 6
int64:neg_one = 0tT;      // -1
int64:four    = 0n4;      // balanced nonary: 4
int64:neg_four = 0nD;     // -4
```

---

## Quick Start

```bash
git clone https://github.com/alternative-intelligence-cp/aria.git
cd aria
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)
cd ..
```

```bash
# hello.aria
cat > hello.aria << 'EOF'
func:main = int32() {
    drop(println("Hello from Aria!"));
    pass(0i32);
};
func:failsafe = NIL(int32:err_code) {};
EOF

./build/ariac hello.aria -o hello
./hello
```

```bash
# Compile options
./build/ariac program.aria -o program          # compile
./build/ariac program.aria --emit-llvm -o out.ll  # LLVM IR
./build/ariac program.aria -O2 -o program      # optimized
```

**Prerequisites:** LLVM 20.1+, CMake 3.20+, C++17 compiler, Linux/macOS/WSL2

---

## Documentation

📚 **[Web Programming Guide](https://aria.docs.ai-liberation-platform.org/)** — Complete interactive guide with all language features, safety patterns, and examples.

**Quick links:**
- [Language Guide](https://aria.docs.ai-liberation-platform.org/language-guide/)
- [Standard Library](https://aria.docs.ai-liberation-platform.org/stdlib/)
- [Control Flow](https://aria.docs.ai-liberation-platform.org/control-flow/)

**Man pages:** Install with `cd aria_ecosystem/man_pages && make install` → then `man aria-control-flow-for`, `man aria-types-int32`, etc.  
**Programming guide source:** [`aria_ecosystem/programming_guide/`](aria_ecosystem/programming_guide/)  
**Language spec:** [`.internal/aria_specs.txt`](.internal/aria_specs.txt)

---

## Project Structure

```
aria/
├── src/                      # Compiler source
│   ├── frontend/            # Lexer, parser, AST, semantic analysis
│   │   ├── lexer/
│   │   ├── parser/
│   │   ├── ast/
│   │   └── sema/            # Type checker, borrow checker, etc.
│   ├── backend/             # LLVM IR generation
│   │   └── ir/
│   ├── runtime/             # Runtime support (GC, strings, async, streams)
│   └── tools/               # ariac tool implementations
├── include/                  # Headers
├── stdlib/                   # Standard library (.aria files)
│   ├── print_utils.aria     # print, println, drop
│   ├── string_convert.aria  # int32_toString, etc.
│   ├── string.aria          # String manipulation (replace, split, reverse, etc.)
│   ├── string_builder.aria  # StringBuilder MVP
│   ├── io.aria              # File I/O streams (open, read, write, close)
│   ├── math.aria            # Math functions (sqrt, sin, cos, exp, log, etc.)
│   ├── linalg.aria          # Linear algebra (Vec2, Vec3, Mat2x2, Mat3x3)
│   ├── collections.aria     # Collections (Vec, VecI32, VecF64, Map, Set, Graph)
│   ├── dbug.aria            # Debug instrumentation
│   ├── wave.aria            # Wave mechanics
│   ├── wavemech.aria        # Wave encoding/decoding
│   ├── complex.aria         # Complex number types
│   ├── atomic.aria          # Atomic primitives
│   ├── quantum.aria         # Quantum types
│   ├── lib_hashmap_*.aria   # HashMap implementations
│   └── lib_vec_int32.aria   # Vec<int32>
├── tests/                    # Test suite (681 tests and growing)
│   ├── fuzz/                # Fuzzer V2 and corpus
│   ├── gpu/                 # GPU/CUDA tests and PTX
│   └── *.aria               # Feature test files
├── examples/                 # Example programs
├── docs/                     # Documentation and design docs
├── scripts/                  # Build and maintenance scripts
│   └── gpu/                 # GPU/CUDA setup scripts
├── tools/                    # Development tooling
│   ├── aria_specialist_*/   # Language specialist model
│   ├── train_aria_specialist*.py
│   ├── aria_patch_corpus.py # Training data preparation
│   └── semantic_db/         # Semantic test archive
├── aria_ecosystem/           # Ecosystem tools
│   ├── aria_packages/       # Library ecosystem (20 packages, each with src/ + tests/)
│   │   ├── aria-math/       # LIB-1: trig/exp/log via C libm
│   │   ├── aria-rand/       # LIB-2: xorshift64 PRNG
│   │   ├── aria-color/      # LIB-3: RGBA pixel ops
│   │   ├── aria-vec/        # LIB-4: 2D/3D float64 vectors
│   │   ├── aria-buf/        # LIB-5: uint64 byte packing
│   │   ├── aria-clamp/      # LIB-6: min/max/clamp/abs/sign
│   │   ├── aria-bits/       # LIB-7: bit manipulation
│   │   ├── aria-ascii/      # LIB-8: ASCII utils
│   │   ├── aria-fixed/      # LIB-9: Q32.32 fixed-point
│   │   ├── aria-freq/       # LIB-10: frequency/baud arithmetic
│   │   ├── aria-mux/        # LIB-11: bit-select/field/mask
│   │   ├── aria-conv/       # LIB-12: saturating conversions
│   │   ├── aria-hash/       # LIB-13: FNV-1a / djb2 hashing
│   │   ├── aria-endian/     # LIB-14: byte-swap utilities
│   │   ├── aria-uuid/       # LIB-15: UUID v4 generation
│   │   ├── aria-zigzag/     # LIB-16: zigzag encode/decode
│   │   ├── aria-display/    # LIB-17: ANSI/termios virtual console display
│   │   ├── aria-input/      # LIB-18: raw keyboard + SNES button mapping
│   │   ├── aria-audio/      # LIB-19: software synthesis (MIDI, 4 channels)
│   │   └── aria-console/    # LIB-20: 16-bit memory map + frame scheduler│   ├── aria-str/        # LIB-21: String utilities
│   ├── aria-json/       # LIB-22: JSON encoding
│   ├── aria-args/       # LIB-23: CLI argument parser
│   ├── aria-decision-t/ # LIB-24: Decision tree classifier
│   ├── aria-entangled/  # LIB-25: Quantum entangled pairs
│   ├── aria-resource-mem/ # LIB-26: RAII resource management
│   └── aria-gradient-field/ # LIB-27: 3D gradient fields│   ├── demos/               # Demo programs
│   │   └── aria-jrpg-demo/  # JRPG battle scene using LIB-17/18/19/20
│   ├── programming_guide/   # Source for web/man docs
│   ├── man_pages/           # Man page builder
│   ├── aria_make/           # Build system
│   ├── aria_shell/          # Aria-native shell
│   └── aria_utils/          # CLI utilities (als, acp, agrep, etc.)
├── .internal/                # Internal: spec, session notes, status docs
│   ├── aria_specs.txt       # Master language specification
│   └── *.md                 # Development status tracking
├── test_results/             # Historical test run output
├── build/                    # Build artifacts (gitignored)
├── CMakeLists.txt
├── README.md
├── SAFETY.md
├── ARCHITECTURE.md
├── CHANGELOG.md
└── LICENSE.md
```

---

## Testing

```bash
# Run the full test suite
./scripts/run_comprehensive_tests.sh

# Run fuzzer
cd tests/fuzz && python3 fullstack_fuzzer.py

# Single test
./build/ariac tests/some_feature.aria -o /tmp/t && /tmp/t
```

Test results are archived in `test_results/` for regression tracking. The fuzzer archives crashes in `tests/fuzz/crashes_archive/`.

---

## Development Roadmap

### v0.1.0 — In Progress (No ETA)

**Completed:**
- ✅ Full compiler toolchain (ariac, aria-lsp, aria-dap, aria-doc, aria-pkg)
- ✅ Layered safety system (?!, !!!, unknown/ok(), failsafe)
- ✅ Quantum types (superpose/collapse)
- ✅ TBB types with sticky overflow sentinels
- ✅ Full generic system (functions + structs, monomorphization)
- ✅ Both for loop forms (C-style and range-based)
- ✅ Borrow checker (compile-time memory analysis)
- ✅ SIMD and atomic types
- ✅ Dimensional algebra
- ✅ 678 compiler tests, Fuzzer V2, Valgrind-clean runtime
- ✅ 27 `aria_packages` libraries with 543+ assertions (all passing)
- ✅ Virtual console stack: display, input, audio, memory map + JRPG demo
- ✅ AI-focused libraries: decision trees, entangled variables, gradient fields, resource memory
- ✅ Safety-critical validation suite (10 tests: IEEE 754, overflow, determinism, TBB)
- ✅ All 27 compiler bugs found, fixed, verified — zero known bugs
- ✅ Full documentation (web, man pages, programming guide, Getting Started)
- ✅ Language specialist model V3 complete (71% pattern match), V4 training in progress

**In Progress:**
- 🔧 Balanced ternary/nonary runtime operations (trit/tryte arithmetic)
- 🔧 Six-stream I/O implementation
- 🔧 Async/await execution model
- 🔧 Arrays in structs
- 🔧 NIL ↔ void bridge (extern interop)
- 🔧 AriaX Linux distribution packaging

### v0.2.0 — In Progress

**Completed (stdlib expansion):**
- ✅ stdlib.io — File I/O streams (open, read, write, close, seek) with 9 passing tests
- ✅ stdlib.math — Transcendental math functions (sqrt, sin, cos, tan, exp, log, pow, etc.) with 19 passing tests
- ✅ stdlib.linalg — Linear algebra (Vec2, Vec3, Mat2x2, Mat3x3, dot, cross, determinant, inverse) with 19 passing tests
- ✅ stdlib.collections — Vec<int64>, VecI32, VecF64, Map<int64,int64>, Set<int64>, Graph<int64> with 70 passing tests
- ✅ stdlib.string — String manipulation (replace, split, join, reverse, count, char_at, to_int) with 45 passing tests
- ✅ stdlib.json — Full JSON builder, parser, encoder (objects, arrays, nesting, round-trips) with 48 passing tests
- ✅ stdlib.toml — TOML builder, parser, encoder (tables, arrays, dotted keys, inline values) with 40 passing tests
- ✅ stdlib.binary — Buffer-based binary serialization (int8/16/32/64, float32/64, strings, file I/O) with 25 passing tests
- ✅ stdlib.net — TCP networking (client connect/send/recv, server listen/accept, timeouts) with 15 passing tests

**Remaining:**
- AriaX Linux official release
- Advanced async patterns (channels, actors)
- Package ecosystem
- Nikola integration (depends on v0.1.0 stability)

### Long Term

Aria is the primary language substrate for **Nikola** — an autonomous AI system based on ATPM consciousness architecture. Features like quantum types, balanced types, and the six-stream I/O model exist because Nikola requires them. Aria stability is a hard prerequisite before Nikola development can proceed in earnest.

See the [engineering plan](https://github.com/alternative-intelligence-cp/nikola) for Nikola's implementation roadmap.

---

## License

Aria is **dual-licensed**:

- **AGPL-3.0** — individuals, students, researchers, open-source projects (FREE)
- **Commercial License** — proprietary use (contact us)

See [LICENSE.md](LICENSE.md) for full terms. Commercial inquiries: development@ai-liberation-platform.org

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). We welcome bug reports, test cases, documentation improvements, and discussion of language design decisions.

---

## Acknowledgments

See [ACKNOWLEDGMENTS.md](ACKNOWLEDGMENTS.md).

---

## Support

If Aria is useful to you, consider supporting development. See [DONATIONS.md](DONATIONS.md) for cryptocurrency addresses.

---

**Alternative Intelligence Liberation Platform (AILP)**  
*Building tools for collaboration, not exploitation.*
