# Aria Programming Language v0.2.24

![Aria Logo](/AriaLogo.png)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://www.apache.org/licenses/LICENSE-2.0)
[![CI](https://github.com/alternative-intelligence-cp/aria/actions/workflows/ci.yml/badge.svg)](https://github.com/alternative-intelligence-cp/aria/actions/workflows/ci.yml)

**The Alternative Intelligence Liberation Platform presents: A systems programming language built for safety, determinism, and AI-native applications**

> ⚠️ **[READ THIS FIRST: SAFETY.md](SAFETY.md)** — No language is truly safe on its own. Aria makes dangerous operations **explicit** — you can't shoot yourself in the foot accidentally, but you can do it intentionally. Understand the safety contract before using `wild`, `async`, `@`, `extern`, or `wildx`.

---

## Current Status (March 2026)

**v0.2.13 — WebAssembly Compilation Target**

Aria programs now compile to WebAssembly via `ariac --emit-wasm`. WASI-compatible runtime, full string/math/IO support, 4/4 WASM test suite passing. Run Aria programs with `wasmtime`, `wasmer`, or any WASI runtime.

**Recent highlights:**
- **v0.2.13** — WASM backend: `--emit-wasm` flag, `libaria_runtime_wasm.a`, wasm-ld linking, compiler-rt builtins
- **v0.2.12** — Macros, comptime, borrow checker improvements, inline/noinline hints
- **v0.2.11** — Threading & concurrency (thread pools, atomics, channels, lock-free data structures), OS components (arena/pool/slab allocators, IPC, signals), AI-native filesystem (FUSE)
- **v0.2.10** — AI/ML stack: Transformer, Mamba, Jamba, Looping models, CUDA bindings, tensor library, UACP protocol
- **v0.2.9** — HTTP server framework with Express-style routing and 6 middleware packages
- **80 ecosystem packages**, all passing — utility, graphics/game, server, database, AI/ML tiers
- **862 tests** across 39 test categories
- **50 stdlib modules** (12.5K lines)

---

## Compiler Toolchain

| Tool | Status | Description |
|---|---|---|
| `ariac` | ✅ Stable | Full compiler, LLVM 20 backend |
| `aria-ls` | ✅ Improved | Language Server — hover, goto-definition, completion, documentSymbol, references, signatureHelp |
| `aria-pkg` | ✅ Wired | Package manager — install, search, pack, 74 packages verified |
| `aria-doc` | ✅ Fixed | Documentation generator — 435 unique HTML pages from ecosystem |
| `aria-mcp` | ✅ Improved | MCP server — compile, safety audit, docs search, format, specialist model |
| `aria-safety` | ✅ Improved | Static safety auditor — 11 checks including UNSAFE, EXTERN, CAST, TODO; `--json` output |
| `aria-dap` | ✅ Working | Debug Adapter Protocol — LLDB 20 backend, conditional breakpoints, logpoints |
| `aria_make` | ✅ Working | Build system — project manifest, dependency resolution, test runner |
| `install.sh` | ✅ Stable | One-command build + install with prerequisite checking |
| Fuzzer V2 | ✅ Active | 48-hour stress campaigns |
| Specialist model | ✅ V6 | Qwen 7B LoRA, v6 corpus covering v0.2.3 additions |
| Debian package | ✅ Built | `aria_0.2.2-1_amd64.deb` (17 MB), tested on Mint 22.3 |
| AriaX Linux | 🔧 In progress | Custom distro with full toolchain |
| `aria_packages` | ✅ Active | 74 packages (39 utility/GUI + 4 database + 7 server + 8 AI/ML + 16 more), all passing |

---

## Language Features

### Stable
- **All primitive types** — int8/16/32/64, uint8/16/32/64, flt32/flt64, bool, string
- **TBB Types (tbb8/16/32/64)** — Symmetric signed integers with overflow sentinel (-128/ERR)
- **Balanced Ternary/Nonary Literals & Runtime** — `0t[01T]+` and `0n[01234ABCD]+` syntax, full trit/tryte/nit/nyte arithmetic
- **Quantum Types** — Superposition states for probabilistic computation
- **Generic Functions and Structs** — Monomorphization with type inference
- **Result Types** — `pass`/`fail` with `?` propagation and `!` unwrap, `Result<T>` signatures
- **`fail()` from user functions** — Result-style: `fail(err)` produces `Result{error:err, is_error:true}`, complement to `pass(val)`
- **Layered Safety** — `?!` (checked), `!!!` (unchecked), `unknown`/`ok()`, `failsafe`
- **Async/Await** — LLVM coroutine-based with promise mechanism for error propagation through async boundaries
- **All loop forms** — `while`, `for(;;)` (C-style), `for (i in a..b)` (range), `loop`, `till`, `when/then/end`
- **Control flow** — `if/else`, `pick` (exhaustive match), `break`, `continue`, `fall`
- **Module system** — `use`, `mod`, `pub`, `extern`
- **Closures** — First-class functions with capture
- **Borrow checker** — Compile-time memory safety analysis
- **Arrays in structs** — Fixed-size scalar and struct array fields with nested member access
- **SIMD types** — Vector arithmetic via LLVM intrinsics
- **Atomic types** — Lock-free concurrent primitives
- **Dimensional algebra** — Unit-typed arithmetic
- **NIL/void separation** — NIL is Aria's unit type (wrapped in `Result<nil>`), void restricted to extern blocks, bridge via pointer erasure
- **Operators** — Full suite including `@` (address), `#` (pin), `->` (arrow), `..`/`...` (ranges)
- **Template literals** — `` `&{variable}` `` string interpolation
- **Standard library** — string_convert, string (manipulation), string_builder, print_utils, wave/wavemech, complex, dbug, quantum, atomic, io (file streams), math (transcendentals), linalg (linear algebra), collections (Vec, Map, Set, Graph), json, toml, binary, net (TCP sockets)

### In Progress / Specified
- **Six-stream I/O** — stdin/stdout/stderr/stddbg/stddati/stddato — runtime initialization with graceful fallback, `stdin_read_all()` and `stdin_read_line()` builtins
- **Traits & borrow semantics** — RFC complete (monomorphized traits + `dyn Trait` opt-in, `$x`/`$mut x` borrows); implementation in future release
- **Async channels & actors** — Concurrent workloads, task scheduling, and async I/O with event loop — deferred to post-0.2.4

---

## `aria-packages` Library Ecosystem

All packages live in the separate [`aria-packages`](https://github.com/alternative-intelligence-cp/aria-packages) repository. 74 packages total, organized into utility, graphics/game, server, database, and AI/ML tiers. Each package has a `src/` module, a `tests/` file with assertions, and where FFI is needed, a C `shim/`.

**Package tiers:**

| # | Package | Description |
|---|---------|-------------|
| Package | Description |
|---------|-------------|
| aria-args | Command-line argument parsing |
| aria-ascii | ASCII character classification and conversion |
| aria-audio | Software synthesis, MIDI note table, ALSA backend |
| aria-base64 | Base64 encoding/decoding |
| aria-bench | Benchmarking |
| aria-bits | Bit test/set/clear/flip, nibble extraction, popcount |
| aria-body-parser | HTTP body parsing middleware (JSON, URL-encoded, multipart) |
| aria-buf | Byte/word packing for uint64 buffers |
| aria-clamp | min, max, clamp, abs, sign for int64/uint64 |
| aria-cli | Enhanced CLI parsing with subcommands |
| aria-color | RGBA packing/unpacking and pixel transforms |
| aria-compress | Data compression (LZ4/zstd via FFI) |
| aria-console | 16-bit memory-mapped address space + 60fps frame scheduler |
| aria-conv | Saturating narrowing and float/int conversion |
| aria-cookie | Cookie parsing and Set-Cookie builder |
| aria-cors | CORS middleware with configurable origins |
| aria-crypto | Cryptographic primitives (SHA-256, AES, HMAC via FFI) |
| aria-csv | CSV parsing and generation |
| aria-cuda | CUDA FFI: device management, memory ops, kernel launch, cuBLAS |
| aria-datetime | Date/time formatting and arithmetic |
| aria-decision-t | Decision tree classifier (entropy, information gain) |
| aria-display | ANSI/termios terminal rendering (virtual console) |
| aria-editor | Terminal-mode text editor with search |
| aria-endian | Big/little-endian byte-swap for 16/32/64-bit |
| aria-entangled | Quantum-inspired entangled variable pairs |
| aria-env | Environment variable access and process info |
| aria-fixed | Q32.32 fixed-point arithmetic on uint64 |
| aria-freq | Frequency/period/baud integer arithmetic |
| aria-fs | File system utilities (stat, mkdir, readdir, copy) |
| aria-gml | GML compatibility layer: 40+ functions, xorshift32 RNG |
| aria-gradient-field | 3D gradient field computation |
| aria-gtk4 | GTK4 desktop GUI: widget registry, events, non-blocking UI |
| aria-hash | FNV-1a and djb2 string hashing |
| aria-http | HTTP client (GET/POST requests) |
| aria-input | Raw keyboard input with SNES-style button mapping |
| aria-jamba | Hybrid Transformer + Mamba + Mixture of Experts model |
| aria-jit | WildX JIT helpers |
| aria-json | JSON encoding for basic types |
| aria-log | Structured logging with severity levels |
| aria-looping | Iterative refinement model with convergence stopping |
| aria-mamba | Mamba selective state space model with SiLU gating |
| aria-map | Map data structure |
| aria-math | Trig, exp, log, rounding via C libm |
| aria-mime | MIME type detection and mapping |
| aria-mux | Bit-select, field insert/extract, mask ops, blend |
| aria-mysql | MySQL/MariaDB client via libmysqlclient |
| aria-opengl | OpenGL 3.3 Core via GLAD + SDL2 |
| aria-path | Path manipulation | 
| aria-postgres | PostgreSQL client via libpq (parameterized, LISTEN/NOTIFY) |
| aria-queue | Queue data structure |
| aria-rand | xorshift64 pseudo-random number generator |
| aria-rate-limit | Token bucket rate limiting middleware |
| aria-raylib | raylib v6.0 bindings: window, drawing, shapes, text, input, audio, gamepad |
| aria-redis | Redis client via hiredis (strings, lists, hashes, sets) |
| aria-regex | Regular expression matching |
| aria-resource-mem | RAII-style resource lifecycle management |
| aria-router | Express-style router: path params, middleware, wildcards |
| aria-sdl2 | SDL2 multimedia bindings: window, renderer, drawing, events |
| aria-semver | Semantic versioning: parse, compare, satisfy |
| aria-server | HTTP/1.1 server: listen, accept, parse, respond |
| aria-session | In-memory session management with crypto IDs |
| aria-socket | Socket abstraction layer |
| aria-sort | Various sorting algorithms |
| aria-sqlite | SQLite3 embedded database client (parameterized queries) |
| aria-static | Static file serving with MIME detection and path traversal protection |
| aria-str | String utilities (pad, trim, repeat, contains, split) |
| aria-template | String template rendering with variable substitution |
| aria-tensor | Dense tensor library: creation, arithmetic, matmul, activations, GPU interop |
| aria-test | Test framework with assertion helpers |
| aria-tetris | Full Tetris clone: sound effects, gamepad, high score |
| aria-toml | TOML configuration file parsing |
| aria-transformer | Transformer encoder: multi-head attention, causal masking |
| aria-uacp | Universal AI Communication Protocol: binary framing, 8 message types |
| aria-url | URL parsing, encoding, query string manipulation |
| aria-uuid | UUID v4 generation and formatting |
| aria-vec | 2D/3D float64 vector math (dot, cross, length) |
| aria-websocket | WebSocket client/server (RFC 6455) |
| aria-xml | XML parsing and generation |
| aria-yaml | YAML parsing and serialization |
| aria-zigzag | Zigzag encode/decode for signed integer interleaving |

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
    if (b == 0i32) { fail(1i32); }
    pass(a / b);
};

int32:result = divide(10i32, 2i32) ? 0i32;  // Unwrap with default
```

**Layer 3: Unknown/Ok Pattern** — Explicit null safety without null types:
```aria
int32:maybe_valid = unknown;
if (some_condition) {
    maybe_valid = 42i32;
    ok(maybe_valid);  // Mark as valid
}
// ok() returns the value itself — use wherever the value is needed
int32:safe_val = ok(maybe_valid);
```

**Layer 4: TBB Overflow Detection** — Symmetric integers with error sentinels:
```aria
tbb8:x = 120tbb8;
tbb8:y = 10tbb8;
tbb8:result = x + y;     // Overflow: result = ERR (-128)
tbb8:err = -128tbb8;
if (result == err) {
    drop(println("Overflow detected!"));
}
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

### One-Command Install

```bash
git clone https://github.com/alternative-intelligence-cp/aria.git
cd aria
./install.sh              # build + install to /usr/local
```

The install script checks prerequisites, builds all tools, and installs binaries, stdlib, and man pages. Options:

```bash
./install.sh --build-only          # build without system install
./install.sh --prefix=$HOME/.local # install to custom prefix
./install.sh --uninstall           # remove installed files
```

### Manual Build

```bash
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)
cd ..
```

### Hello World

```bash
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
./build/ariac program.aria --emit-wasm -o out.wasm  # WebAssembly
```

**Prerequisites:** LLVM 20.1+, CMake 3.20+, C++17 compiler, Python 3.8+, Linux/macOS/WSL2

---

## Documentation

📚 **[aria-docs](https://github.com/alternative-intelligence-cp/aria-docs)** — Package reference, language guides, and tutorials (separate repo).

**Quick links in aria-docs:**
- `guide/from_gml_to_native.md` — GML → Native walkthrough
- `packages/` — API reference for all game/graphics packages

**Man pages:** Install with `./install.sh` — then `man aria-control-flow-for`, `man aria-types-int32`, etc.  
**Language spec:** [`.internal/aria_specs.txt`](.internal/aria_specs.txt)  
**Compiler architecture:** [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)

---

## Project Structure

```
aria/
├── .github/                  # CI/CD and GitHub templates
│   ├── workflows/           # GitHub Actions (build + test)
│   └── ISSUE_TEMPLATE/      # Bug report, feature request, crash report
├── src/                      # Compiler source
│   ├── frontend/            # Lexer, parser, AST, semantic analysis
│   ├── backend/             # LLVM IR generation
│   └── runtime/             # Runtime support (GC, strings, async, streams)
├── include/                  # Headers
├── stdlib/                   # Standard library (.aria files)
├── tests/                    # Test suite (862 tests)
│   ├── regression/          # Regression tests
│   ├── fuzz/                # Fuzzer V2 and corpus
│   ├── gpu/                 # GPU/CUDA tests
│   └── misc/                # Exploratory/scratch tests and archived test results
├── examples/                 # Example programs
├── docs/                     # Design docs and architecture
├── scripts/                  # Build and maintenance scripts
├── tools/                    # Development tooling (specialist model, semantic_db)
├── benchmarks/               # Performance benchmarks
├── runtime/                  # Runtime C source
├── lib/                      # Bundled libraries
├── vendor/                   # Vendored dependencies
├── third_party/              # Third-party components
├── debian/                   # Debian packaging
├── .internal/                # Internal: spec, session notes, status docs
├── build/                    # Build artifacts (gitignored)
├── CMakeLists.txt
├── README.md
├── SAFETY.md
├── CHANGELOG.md
├── CONTRIBUTING.md
└── LICENSE.md
```

**Related repositories:**
- [`aria-packages`](https://github.com/alternative-intelligence-cp/aria-packages) — 74 library packages
- [`aria-docs`](https://github.com/alternative-intelligence-cp/aria-docs) — reference docs and guides
- [`ariax`](https://github.com/alternative-intelligence-cp/ariax) — POSIX tools and AX Linux distro
- [`aria-lang`](https://github.com/alternative-intelligence-cp/aria-lang) — VS Code extension
- [`aria-make`](https://github.com/alternative-intelligence-cp/aria-make) — build system

---

## Testing

```bash
# Run regression tests
./tests/regression/run_regression.sh

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

### v0.1.0 — Released

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
- ✅ All 27 compiler bugs found, fixed, verified
- ✅ Full documentation (web, man pages, programming guide, Getting Started)

### v0.2.0 — Released

- ✅ **Self-hosting compiler frontend** — Lexer, parser, type checker, borrow checker, safety checker, exhaustiveness checker, and const evaluator ported to Aria (220 tests across 5 modules)
- ✅ **stdlib expansion** — 9 modules (io, math, linalg, collections, string, json, toml, binary, net) with 290+ passing tests
- ✅ **7 additional codegen bugs fixed** — Inline return comparison, string concatenation, pass() unwrapping, pointer reassignment, NIL comparison, cross-module pointer corruption
- ✅ **Improved compiler diagnostics** — "Did you mean?" suggestions, accurate source locations for all type errors
- ✅ **License changed to Apache 2.0**
- ✅ **Language specialist model** V3 complete (71% pattern match), V4 in progress

### v0.2.2 — Released

- ✅ **GUI toolkit wrappers** — Idiomatic Aria bindings for raylib (20 tests), SDL2 (19 tests), GTK4 (20 tests) via C shim pattern
- ✅ **9 utility libraries** — aria-test, aria-csv, aria-log, aria-base64, aria-datetime, aria-regex, aria-fs, aria-socket, aria-http (166 tests, all passing)
- ✅ **Tooling improvements** — aria-ls (documentSymbol, references, signatureHelp), aria-mcp (format tool, resources), aria_make (test command), aria-dap (conditional breakpoints, logpoints), aria-safety (4 new checks, --json)
- ✅ **Debian packaging** — `aria_0.2.2-1_amd64.deb` (17 MB), tested on Linux Mint 22.3
- ✅ **Repository reorganization** — Monorepo split into 10 repos under `alternative-intelligence-cp` org
- ✅ **V5 specialist corpus** — 2,688 examples covering all v0.2.2 additions
- ✅ **Extended fuzzing** — 48-hour adversarial campaign, zero unresolved crashes

### v0.2.3 — Released

- ✅ **Database client libraries** — 4 packages: aria-sqlite (34 assertions), aria-postgres (40 assertions), aria-mysql (44 assertions), aria-redis (53 assertions)
- ✅ **Parameterized queries by default** — All SQL drivers use parameter binding; SQL injection tests pass on all 3 SQL drivers
- ✅ **Full CRUD + transactions** — Insert, select, update, delete, begin/commit/rollback across all drivers
- ✅ **PostgreSQL LISTEN/NOTIFY** — Async notification support
- ✅ **Database guide** — `aria-packages/DATABASE_GUIDE.md` with prerequisites, patterns, and API reference
- ✅ **V6 specialist model** — Updated corpus covering database additions

### v0.2.4 — Released

- ✅ **Async/await error propagation** — Promise-based `Result<T>` through coroutine boundaries, fixed dual await paths, proper memory management, Valgrind-clean
- ✅ **`fail()` from user functions** — Result-style mechanism: `fail(err)` is complement to `pass(val)`, no more sentinel workarounds
- ✅ **Arrays in structs fixed** — Nested member access (`cloud.pts[0].x`) for read and write, Aria element type registration
- ✅ **Balanced ternary/nonary runtime** — Full trit/tryte arithmetic (add, sub, mul, div, mod), 15/15 tests
- ✅ **NIL ↔ void bridge** — NIL = Aria unit type, void = C ABI only, pointer erasure bridge
- ✅ **Code quality pass** — 44 deprecated LLVM API migrations, all warnings fixed, zero Aria-source warnings
- ✅ **Traits & borrow semantics RFC** — Design doc for monomorphized traits + `$x`/`$mut x` borrows
- ✅ **6 regression tests** — Dedicated regression suite covering critical bug fixes

### v0.2.1.1 — Released

- ✅ **aria-dap debugger** — Full DAP server with LLDB 20 backend. Breakpoints, stepping, stack traces, variable inspection.
- ✅ **`-g` flag** — DWARF debug info generation with proper source paths, variable locations, and subprogram entries.
- ✅ **VS Code debugging** — Debug configuration provider, auto-compile, launch.json snippets, `aria.debugger.path` setting.
- ✅ **O0 for debug** — Automatically disables LLVM optimization when `-g` is set to preserve variable visibility.

### v0.2.1 — Released

- ✅ **aria-pkg fixed** — Registry loading, metadata parsing, tarball extraction; added search/pack/directory-install. 27/27 packages verified.
- ✅ **aria-doc fixed** — Parser rewritten for Aria colon syntax. Generates 435 unique HTML pages, zero `unknown.html` collisions.
- ✅ **aria-ls wired** — AST-based hover (type signatures + builtin descriptions), goto-definition, completion (37 keywords + 15 types + file symbols).
- ✅ **install.sh** — One-command build and install with prerequisite checking, `--prefix`, `--uninstall`. Tested on clean Linux Mint 22.3 VM.
- ✅ **aria-mcp verified** — compile, docs, safety, ask endpoints all functional.
- ✅ **VS Code extension** — v0.2.1 with updated bundled aria-ls binary.
- ✅ **Benchmark suite** — 3 benchmarks (primes, collatz, gcd) in Aria and C with runner script. Aria matches or beats gcc -O2 on 2/3 benchmarks.
- ✅ **Clean-machine build verified** — CMake and install.sh fixes for fresh Linux installs.

### v0.2.5 — Released

- ✅ **CI/CD pipeline** — GitHub Actions: build, test, .deb packaging on push/PR to main/dev
- ✅ **GitHub templates** — Bug report, feature request, compiler crash issue templates; PR template with checklist
- ✅ **CONTRIBUTING.md** — Contributor guide with prerequisites, build instructions, code style
- ✅ **Compiler architecture manual** — 695-line technical document covering full pipeline, AST, type system, IR generation, runtime, FFI, memory model
- ✅ **Man pages** — groff man pages for ariac(1), aria-ls(1), aria-pkg(1), aria-doc(1), aria-dap(1)
- ✅ **Documentation fixes** — Tested 24 code examples, fixed 7 doc bugs, documented 2 compiler bugs
- ✅ **Specialist model evaluation** — Comprehensive evaluation of all model versions, strategy documented

### v0.2.6 — Released

- ✅ **`--shared` flag** — Compile Aria source directly to `.so` shared libraries with C ABI export
- ✅ **Cross-language bindings** — Aria → C, Python (ctypes), Rust (FFI), Go (cgo); documented in `docs/CROSS_LANGUAGE_BINDINGS.md`
- ✅ **GUI/game packages** — aria-raylib (v6.0), aria-sdl2, aria-gtk4, OpenGL bindings (aria-opengl via GLAD)
- ✅ **12 new library packages** — aria-test, aria-csv, aria-log, aria-base64, aria-datetime, aria-regex, aria-fs, aria-socket, aria-http, and infrastructure packages
- ✅ **aria-doc improvements** — Full HTML generation pipeline, 435+ unique pages

### v0.2.7 — Released

- ✅ **Six-stream I/O runtime** — All 6 streams (stdin/stdout/stderr/stddbg/stddati/stddato) initialized with graceful fallback
- ✅ **`stdin_read_all()` / `stdin_read_line()` builtins** — Proper AriaString wrapping, pipes and non-seekable streams supported
- ✅ **Argument access runtime** — `aria_get_argc()`, `aria_arg(index)` builtins; compiler auto-generates `main(i32, ptr)` signature
- ✅ **26 POSIX tools** — cat, head, tail, wc, tee, cut, sort, uniq, tr, grep, find, diff, echo, yes, true, false, env, sleep, basename, dirname, seq, nl, fold, paste, expand, unexpand (in `ariax` repo)
- ✅ **Pipeline support** — All tools read from stdin when no file argument given
- ✅ **String comparison fix** — `_=expr` discard syntax, `sleep_ms` builtin

### v0.2.13 — Released (Current)

- ✅ **WebAssembly compilation target** — `ariac --emit-wasm -o program.wasm` compiles Aria to WASI-compatible WebAssembly
- ✅ **WASM runtime** — `libaria_runtime_wasm.a`: strings, I/O, math, allocators, maps (~850 LOC)
- ✅ **LLVM WebAssembly backend** — `emit_wasm_object()`, wasm-ld linking, WASI entry point handling
- ✅ **Compatibility checker** — Warns about unsupported features (threading, async, fork/exec) at compile time
- ✅ **Compiler-rt builtins** — `__multi3` (128-bit multiply) for int64 arithmetic on wasm32
- ✅ **WASM test suite** — 4/4 passing: hello world, strings, arithmetic, functions/recursion
- ✅ **Documentation** — WASM compilation guide with examples, feature matrix, troubleshooting

### v0.2.12 — Released

- ✅ **Preprocessor macros** — `macro:NAME(params) { body }`, `%*` variadic, `#%N` stringification, `##` token pasting
- ✅ **Comptime evaluation** — `comptime { }` blocks, evaluated at compile time
- ✅ **Magic constants** — `__FILE__`, `__LINE__`, `__FUNC__`, `__COUNTER__`
- ✅ **Inline hints** — `inline` and `noinline` function attributes
- ✅ **Borrow checker improvements** — Better analysis of conditional paths and reassignment
- ✅ **Compiler diagnostics** — Improved error messages for macro expansion and comptime errors

### v0.2.11 — Released

- ✅ **Thread pool** — `ThreadPool.create()`, `ThreadPool.submit()`, `ThreadPool.wait_idle()`, `ThreadPool.shutdown()`
- ✅ **Atomics** — `AtomicInt32`, `AtomicInt64`, `AtomicBool` with all memory orderings
- ✅ **Lock-free data structures** — `LFQueue` (MPMC), `LFStack` (Treiber), `RingBuf` (SPSC)
- ✅ **Channels** — `Channel.create()`, `Channel.send()`, `Channel.recv()` for inter-thread communication
- ✅ **Mutex/RWLock/Barrier/CondVar** — Full synchronization primitives
- ✅ **OS components** — Arena, pool, slab allocators; shared memory; IPC; signal handling; process management
- ✅ **AI-native filesystem (AIFS)** — FUSE-based filesystem for AI workloads
- ✅ **AriaX kernel mods** — Hexstream FD 3-5 patches

### v0.2.10 — Released

- ✅ **aria-transformer** — Full Transformer encoder with multi-head attention, causal masking, attention visualization (10/10 tests)
- ✅ **aria-mamba** — Mamba SSM: selective scan, 1D convolution, SiLU gating, layer norm (10/10 tests)
- ✅ **aria-jamba** — Hybrid Transformer + Mamba + MoE: interleaved layers, top-k expert gating (10/10 tests)
- ✅ **aria-looping** — Iterative refinement model: shared weights, iteration embeddings, convergence stopping (10/10 tests)
- ✅ **aria-tensor** — Dense tensor library: 47 operations, GPU interop (12/12 tests)
- ✅ **aria-cuda** — CUDA FFI: device mgmt, memory ops, kernel launch, cuBLAS GEMM (10/10 tests)
- ✅ **aria-uacp** — Universal AI Communication Protocol: binary framing, 8 message types (12/12 tests)
- ✅ **Self-improving training loop** — Automated specialist: generate → compile → filter → retrain

### v0.2.9 — Released

- ✅ **HTTP server** — aria-server: listen, accept, parse, respond with headers and status codes
- ✅ **Express-style router** — aria-router: path params, middleware chains, wildcards, method dispatch (21 tests)
- ✅ **6 server middleware libraries** — body-parser (28), cors (18), cookie (23), session (23), rate-limit (14), static (22) — 154+ tests total
- ✅ **REST API demo** — demo_api.aria: 5 endpoints with HTML, JSON, path params, POST handling
- ✅ **Borrow checker fix** — False positive on void externs with `_close`/`_free` in name
- ✅ **FFI string return ABI fix** — AriaString struct returns from extern functions

### v0.2.8 — Released

- ✅ **Repo reorganization** — `aria_ecosystem/` split into `aria-packages`, `aria-docs`, and `ariax` repos
- ✅ **Gamepad input API** — Full button/axis constants in aria-raylib; gamepad support in aria-tetris
- ✅ **Procedural audio synthesis** — `rl_gen_beep()`: square/triangle/sawtooth/sine tones, no audio files needed
- ✅ **aria-tetris** — 928-line Tetris clone with sound, gamepad, high score, line-clear flash animation
- ✅ **aria-gml** — GML compatibility layer: 40+ functions, xorshift32 RNG, persistent draw state
- ✅ **aria-opengl** — OpenGL 3.3 Core bindings via GLAD + SDL2
- ✅ **aria-editor** — Terminal-mode text editor (file open/edit/save/search)
- ✅ **16 new packages** — 59 total in aria-packages
- ✅ **FFI codegen fixes** — Explicit float literals, auto-wrap `char*` → `AriaString` for extern string returns
- ✅ **GML → Native tutorial** — Full walkthrough in aria-docs

### v0.3.0+ — Planned

- Advanced async patterns (channels, actors)
- Traits & borrow semantics implementation
- AriaX Linux distribution packaging
- Nikola integration

### Long Term

Aria is the primary language substrate for **Nikola** — an autonomous AI system based on ATPM consciousness architecture. Features like quantum types, balanced types, and the six-stream I/O model exist because Nikola requires them. Aria stability is a hard prerequisite before Nikola development can proceed in earnest.

See the [engineering plan](https://github.com/alternative-intelligence-cp/nikola) for Nikola's implementation roadmap.

---

## License

Aria is licensed under the **Apache License, Version 2.0**. See [LICENSE.md](LICENSE.md) for the full text.

Programs compiled with Aria are yours — the runtime library includes a **Runtime Library Exception** so your compiled binaries can be distributed under any license you choose.

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). We welcome bug reports, test cases, documentation improvements, and discussion of language design decisions.

---

## Acknowledgments

See [ACKNOWLEDGMENTS.md](ACKNOWLEDGMENTS.md).

---

**Alternative Intelligence Liberation Platform (AILP)**  
*Building tools for collaboration, not exploitation.*
