# Aria Programming Language v0.18.x

![Aria Logo](/AriaLogo.png)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://www.apache.org/licenses/LICENSE-2.0)
[![CI](https://github.com/alternative-intelligence-cp/aria/actions/workflows/ci.yml/badge.svg)](https://github.com/alternative-intelligence-cp/aria/actions/workflows/ci.yml)

**The Alternative Intelligence Liberation Platform presents: A systems programming language built for safety, determinism, and AI-native applications**

> ⚠️ **[READ THIS FIRST: SAFETY.md](SAFETY.md)** — No language is truly safe on its own. Aria makes dangerous operations **explicit** — you can't shoot yourself in the foot accidentally, but you can do it intentionally. Understand the safety contract before using `wild`, `async`, `@`, `extern`, or `wildx`.

---

## Current Status (May 3, 2026)

**Stable release: v0.18.0 — K Framework executable semantics seed complete**

**Next transition: Nitpick rebrand planning and repository/README cleanup**

Aria is now feature-frozen for broad language-surface expansion and focused on
distribution, formal semantics, and certification-grade validation. The v0.17.x
series made Aria installable through mainstream Linux paths; v0.18.0 delivered a
formal K Framework oracle seed for “what should this program do?” independent of
`ariac`.

**Current validation snapshot:** CTest **8/8 passing** with K semantics enabled;
`k_semantics_core` **105/105** under K Framework v7.1.320;
`k_semantics_proofs` **10/10 proof modules**;
v0.16/v0.17 compiler audit baseline **1,015 tests** with 0 genuine regressions;
**800K+ fuzz tests** with 0 crashes; **103 packages**; **72 stdlib modules**.

**v0.18.0 complete:**
- **K executable semantics seed** — `k-semantics/aria.k` defines a
    first executable subset: `func:main`/`func:failsafe`, variable binding,
    `fixed`, bounded `int32`/`int64`, explicit `tbb32` min-sentinel behavior,
    integer arithmetic/comparisons, sticky `ERR`, `Unknown`, Result operators,
    zero-/one-/two-argument helper calls returning through `pass`/`fail`,
    isolated helper call frames with pinned-host preservation,
    one-/two-/three-field structs with field reads and writes, string literals,
    `print`/`println` stdout modeling, `if`/`else`,
    `pick`/`fall` value and wildcard dispatch, untyped `Rules:` declarations,
    integer `limit<Rules>` declaration and reassignment checks with failsafe
    routing, cascaded rules, initial memory allocation qualifier semantics for
    `stack`/`gc`/`wild`, `alloc`/`free` wild cleanup and leak routing, initial
    `wildx` declarations as explicit executable-memory allocations sharing the
    currently modeled wild cleanup/leak lifecycle, initial `defer { ... }`
    block-scoped LIFO cleanup semantics, local pointer
    address/dereference/store-through semantics for `@value`, `<-ptr`, and
    `<-ptr = value`, pointer-member reads and store-through via `ptr->field`,
    including nested pointer-valued paths such as `ptr->leaf->x`,
    `#value` pin registration with pin dereference,
    read-only pin store-through, pin-member store-through enforcement,
    pin-derived nested path mutation blocking, pinned-host field mutation blocking,
    pinned-host by-value call and terminal-exit blocking,
    and block-scoped pin release,
    initial borrow permission semantics for `$$i`/`$$m` aliases and helper parameters,
    direct struct-field borrow path tracking that distinguishes disjoint fields
    such as `obj.a` and `obj.b`, blocks exact borrowed-field mutation, and keeps
    conservative host-level conflicts,
    nested two-level struct-field borrow path tracking such as `box.leaf.x`,
    including sibling split borrows, parent/child conflict checks, nested-field
    mutation, and parent-field mutation blocking while a child field is borrowed,
    local mutable field-alias writeback for direct and two-level paths such as
    `$$m int32:x = pair.a` and `$$m int32:x = box.leaf.x`,
    positive `$$m` call-by-reference mutation/writeback,
    block-scoped borrow release for nested statement blocks,
    `loop(start,end,step)`, and `exit`.
- **K runner integrated with CTest** — `run_k_tests.sh` compiles with `kompile`,
    executes core programs with `krun`, and skips cleanly when K is unavailable.
- **K proof runner integrated with CTest** — `run_k_proofs.sh` compiles the
    semantics with the Haskell backend required by `kprove` and proves the core
    arithmetic, field-alias, pin read-only, pinned by-value, and local pointer
    pointer-path, borrow-path, control/rules, arithmetic, and Result claim modules.
- **Current wall documented** — array/index field borrow paths remain blocked
    until the compiler surface accepts indexed borrow initializers and K grows
    array/index-path semantics.

**Recently completed series:**
- **v0.17.x** — Installers, packaging, and distribution: enhanced `install.sh`,
    Debian `.deb` builder, RPM builder, `aria-pkg` remote fetch from GitHub, and
    refreshed install/tooling docs.
- **v0.16.x** — Full compiler/runtime/tooling audit: 0 TODO/FIXME/HACK in C++
    source, 72 stdlib modules audited, 33 examples tested, 171-page manual
    regenerated, improved diagnostics, and 800K+ fuzz tests with 0 crashes.

**Previous series highlights:**
- **v0.15.x** — Self-hosting: 12 compiler/tool modules ported to Aria (lexer, parser, type checker, borrow checker, safety checker, exhaustiveness, const evaluator, module resolver, doc generator, package manager, project config), final census
- **v0.14.x** — SMT solver expansion: contract proofs, range inference, data race analysis, fast-paths, documentation
- **v0.13.x** — Traits, enums, generics, deferred language features, @ function pointers, 234-page PDF manual, final audit (959 tests, 17h+ fuzzing)
- **v0.12.x** — Networking & middleware: HTTP, DNS, socket, server, URL, cookie, CORS, body-parser, session, static, rate-limit, FTP, SMTP, WebSocket, display, input, LRU, glob, retry
- **v0.11.x** — Threading & concurrency: thread pool, atomics, lock-free structures, channels, mutex/rwlock/barrier, arena/pool/slab allocators, shared memory, IPC, signal handling, AIFS, AriaX kernel mods
- **v0.7.x** — JIT: 45+ x86-64 instructions, linear scan register allocator, peephole optimizer, WildX security, 0.66x native C -O2
- **v0.5.x** — Z3 SMT formal verification: contracts, overflow proofs, concurrency/memory safety verification
- **v0.3.x–v0.4.x** — Ecosystem packages, Z3 Phase 1–3, AriaX kernel mods

---

## Compiler Toolchain

| Tool | Status | Description |
|---|---|---|
| `ariac` | ✅ Stable | Full compiler, LLVM 20 backend |
| `aria-ls` | ✅ Stable | Language Server — hover, goto-definition, completion, documentSymbol, references, signatureHelp |
| `aria-pkg` | ✅ Stable | Package manager — install, search, pack, 103 packages verified |
| `aria-doc` | ✅ Stable | Documentation generator — 435 unique HTML pages from ecosystem |
| `aria-mcp` | ✅ Stable | MCP server — compile, safety audit, docs search, format, specialist model |
| `aria-safety` | ✅ Stable | Static safety auditor — 11 checks including UNSAFE, EXTERN, CAST, TODO; `--json` output |
| Z3 Verifier | ✅ Stable | SMT-based formal verification — contracts, overflow, concurrency, memory safety, `prove`/`assert_static`, `--smt-opt` |
| K semantics | ✅ v0.18.0 seed | Executable formal semantics seed — `kompile`/`krun` core oracle, `kprove` proof hook, CTest integration, 105/105 core tests, 10/10 proof modules |
| `aria-dap` | ✅ Stable | Debug Adapter Protocol — LLDB 20 backend, conditional breakpoints, logpoints |
| `aria_make` | ✅ Stable | Build system — project manifest, dependency resolution, test runner |
| `install.sh` | ✅ Stable | One-command build + install with prerequisite checking |
| Fuzzer V2 | ✅ Active | 27 generators, 100% compile rate, zero unresolved crashes |
| Specialist model | ✅ V6+SMT | Qwen 7B LoRA, v6 corpus + v7 SMT verification examples |
| Linux packages | ✅ Stable | Debian `.deb` and RPM builders, plus source/install-script paths |
| AriaX Linux | 🔧 In progress | Custom distro with full toolchain |
| `aria_packages` | ✅ Active | 103 packages, all passing |
| PDF Manual | ✅ v1.1 | [171-page programming manual](https://github.com/alternative-intelligence-cp/aria-docs/releases/tag/v0.16.11) — specs, guide, safety walkthrough, reference |

---

## Language Features

### Stable
- **All primitive types** — int1/2/4/8/16/32/64/128/256/512/1024/2048/4096, uint1/2/4/8/16/32/64/128/256/512/1024/2048/4096, flt32/64/128/256/512, bool, string
- **TBB Types (tbb8/16/32/64)** — Symmetric signed integers with overflow sentinel (-128/ERR)
- **TFP TYPES (tfp16/32)** — Twisted Floating Point
- **any** — type erased pointer, equivalent to void* in C
- **Balanced Ternary/Nonary Literals & Runtime** — `0t[01T]+` and `0n[01234ABCD]+` syntax, full trit/tryte/nit/nyte arithmetic
- **Quantum Types (Q3/Q9/Q21)** — Gradient thinking: two-hypothesis superposition with confidence levels and crystallization thresholds
- **Generic Functions and Structs** — Monomorphization with type inference
- **Result Types** — `pass`/`fail` with `?` propagation and `!` unwrap, `Result<T>` signatures, `_?`/`_!` shorthand operators
- **`fail()` from user functions** — Result-style: `fail(err)` produces `Result{error:err, is_error:true}`, complement to `pass(val)`
- **Layered Safety** — `?!` (checked), `!!!` (unchecked), `unknown`/`ok()`, `failsafe`
- **Async/Await** — LLVM coroutine-based with promise mechanism for error propagation through async boundaries
- **All loop forms** — `while`, `for(;;)` (C-style), `for (i in a..b)` (range), `loop`, `till`, `when/then/end`
- **Control flow** — `if/else`, `pick` (exhaustive match), `break`, `continue`, `fall`
- **Module system** — `use`, `mod`, `pub`, `extern`
- **Closures** — First-class functions with capture
- **Z3 Formal Verification** — SMT-based compile-time proofs: contracts (`requires`/`ensures`), overflow elimination, data race detection, deadlock detection, use-after-free proofs, `prove`/`assert_static` builtins, `--smt-opt` optimizations, configurable `--smt-timeout`
- **Borrow checker** — Compile-time memory safety analysis
- **Arrays in structs** — Fixed-size scalar and struct array fields with nested member access
- **SIMD types** — Vector arithmetic via LLVM intrinsics
- **Atomic types** — Lock-free concurrent primitives
- **Dimensional algebra** — Unit-typed arithmetic
- **NIL/void separation** — NIL is Aria's unit type (wrapped in `Result<nil>`), void restricted to extern blocks, bridge via pointer erasure
- **NIL/NULL separation** — NIL for no value, NULL for no reference
- **Operators** — Full suite including `+` (string concatenation), `@` (address), `#` (pin), `->` (arrow), `..`/`...` (ranges)
- **Template literals** — `` `&{variable}` `` string interpolation
- **Six-stream I/O** — stdin/stdout/stderr/stddbg/stddati/stddato with runtime initialization, graceful fallback, `stdin_read_all()` and `stdin_read_line()` builtins
- **sys() syscall interface** — Three-tier direct Linux syscall invocation (`sys`/`sys!!`/`sys!!!`) with compile-time safety whitelist, Result<int64> wrapping, and TOS escalation integration
- **Traits** — Definition, implementation, UFCS on primitive types, `dyn Trait` vtable dispatch, trait bounds on generics
- **Optional types** — `T?` with `??` nil-coalescing, `?.` safe navigation; working for primitives and custom types
- **Async channels & actors** — Buffered/unbuffered/oneshot channels, actor spawn/send/stop, fan-out/fan-in/pipeline patterns
- **Standard library** — string_convert, string (manipulation), string_builder, print_utils, wave/wavemech, complex, dbug, quantum, atomic, io (file streams), math (transcendentals), linalg (linear algebra), collections (Vec, Map, Set, Graph), json, toml, binary, net (TCP sockets)

### In Progress / Specified
- **AriaX Linux** — Custom distro with full toolchain pre-installed
- **Specialist model V7** — Next training corpus covering v0.16.x additions

---

## `aria-packages` Library Ecosystem

All packages live in the separate [`aria-packages`](https://github.com/alternative-intelligence-cp/aria-packages) repository. 103 packages total, organized into utility, graphics/game, server, database, and AI/ML tiers. Each package has a `src/` module, a `tests/` file with assertions, and where FFI is needed, a C `shim/`.

**Package tiers:**

| Package | Description |
|---------|-------------|
| aria-actor | Extended actor patterns (pool, router, supervisor) |
| aria-aifs | AI filesystem utilities (POSIX ops + metadata tagging) |
| aria-args | Command-line argument parsing |
| aria-ascii | ASCII character classification and conversion |
| aria-audio | Software synthesis, MIDI note table, ALSA backend |
| aria-base64 | Base64 encoding/decoding |
| aria-bench | Benchmarking |
| aria-bigdecimal | Arbitrary precision decimal arithmetic |
| aria-bits | Bit test/set/clear/flip, nibble extraction, popcount |
| aria-body-parser | HTTP body parsing middleware (JSON, URL-encoded, multipart) |
| aria-buf | Byte/word packing for uint64 buffers |
| aria-channel | High-level channel patterns (fan-out, fan-in, pipeline) |
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
| aria-diff | Text diffing |
| aria-display | ANSI/termios terminal rendering (virtual console) |
| aria-dns | DNS resolution |
| aria-editor | Terminal-mode text editor with search |
| aria-endian | Big/little-endian byte-swap for 16/32/64-bit |
| aria-entangled | Quantum-inspired entangled variable pairs |
| aria-env | Environment variable access and process info |
| aria-fixed | Q32.32 fixed-point arithmetic on uint64 |
| aria-freq | Frequency/period/baud integer arithmetic |
| aria-fs | File system utilities (stat, mkdir, readdir, copy) |
| aria-ftp | FTP client session management |
| aria-gml | GML compatibility layer: 40+ functions, xorshift32 RNG |
| aria-gradient-field | 3D gradient field computation |
| aria-gtk4 | GTK4 desktop GUI: widget registry, events, non-blocking UI |
| aria-hash | FNV-1a and djb2 string hashing |
| aria-hex | Hexadecimal encoding/decoding |
| aria-http | HTTP client (GET/POST requests) |
| aria-ini | INI config file parsing |
| aria-input | Raw keyboard input with SNES-style button mapping |
| aria-jamba | Hybrid Transformer + Mamba + Mixture of Experts model |
| aria-jit | WildX JIT helpers |
| aria-json | JSON encoding for basic types |
| aria-libc | Standard C library wrappers |
| aria-log | Structured logging with severity levels |
| aria-looping | Iterative refinement model with convergence stopping |
| aria-mamba | Mamba selective state space model with SiLU gating |
| aria-map | Map data structure |
| aria-math | Trig, exp, log, rounding via C libm |
| aria-matrix | Matrix operations |
| aria-mime | MIME type detection and mapping |
| aria-mock | Test mocking framework |
| aria-msgpack | MessagePack binary serialization |
| aria-mux | Bit-select, field insert/extract, mask ops, blend |
| aria-mysql | MySQL/MariaDB client via libmysqlclient |
| aria-opengl | OpenGL 3.3 Core via GLAD + SDL2 |
| aria-path | Path manipulation | 
| aria-postgres | PostgreSQL client via libpq (parameterized, LISTEN/NOTIFY) |
| aria-pqueue | Priority queue (min-heap) |
| aria-qt6 | Qt6 Widgets GUI toolkit bindings |
| aria-queue | Queue data structure |
| aria-rand | xorshift64 pseudo-random number generator |
| aria-rate-limit | Token bucket rate limiting middleware |
| aria-raylib | raylib v6.0 bindings: window, drawing, shapes, text, input, audio, gamepad |
| aria-redis | Redis client via hiredis (strings, lists, hashes, sets) |
| aria-regex | Regular expression matching |
| aria-resource-mem | RAII-style resource lifecycle management |
| aria-ringbuf | Ring buffer (circular buffer) |
| aria-router | Express-style router: path params, middleware, wildcards |
| aria-rules-common | Commonly used Rules<T> declarations |
| aria-sdl2 | SDL2 multimedia bindings: window, renderer, drawing, events |
| aria-sdl3 | SDL3 bindings |
| aria-semver | Semantic versioning: parse, compare, satisfy |
| aria-server | HTTP/1.1 server: listen, accept, parse, respond |
| aria-session | In-memory session management with crypto IDs |
| aria-smtp | SMTP email composition |
| aria-socket | Socket abstraction layer |
| aria-sort | Various sorting algorithms |
| aria-sqlite | SQLite3 embedded database client (parameterized queries) |
| aria-static | Static file serving with MIME detection and path traversal protection |
| aria-stats | Statistics functions |
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
| aria-webkit-gtk | WebKitGTK web content bindings |
| aria-wxwidgets | wxWidgets GUI toolkit bindings |
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
func:failsafe = int32(tbb32:err) {
    // Log, cleanup, graceful shutdown
    exit(1);
};
```

**Layer 2: Result Types** — Explicit error propagation with concise syntax:
```aria
func:divide = int32(int32:a, int32:b) {
    if (b == 0i32) { fail(-1i32); }
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

**Layer 5: Z3 SMT Formal Verification** — Compile-time mathematical proofs via Z3 solver:
```aria
// Design-by-Contract: requires/ensures are verified at compile time
func:safe_divide = int32(int32:a, int32:b)
    requires b != 0i32
    ensures $ >= 0i32
{
    pass(a / b);
};

// Integer overflow proofs: Z3 bitvector analysis proves absence of overflow
func:safe_add = int32(int32:a, int32:b)
    requires a > 0i32, a < 1000i32, b > 0i32, b < 1000i32
{
    pass(a + b);  // Z3 proves: no overflow possible in int32 range
};
```
```bash
# Verify contracts and overflow safety at compile time
ariac program.aria --verify-contracts --verify-overflow --verify-report -o program
```
Aria uses the Z3 SMT solver to mathematically *prove* the absence of contract violations and arithmetic overflow across all possible execution paths — the same class of static formal verification used by Ada/SPARK for safety-critical aerospace and automotive systems.

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

### Quantum Types — Gradient Thinking

Quantum types track two hypotheses simultaneously with confidence levels, crystallizing to a definite value when evidence accumulates. Three granularity levels: Q3 (ternary), Q9 (nonary), Q21 (21-state with saturation barriers).

```aria
use "stdlib/quantum.aria".*;

// Q3<T>: Simple 3-state confidence (trit: -1, 0, +1)
Q3<int32>:sensor = {a: 20i32, b: 25i32, c: 0};  // Two hypotheses, unknown confidence

// Both hypotheses evolve together
Q3<int32>:updated = {a: sensor.a + 5i32, b: sensor.b + 5i32, c: 1};

// Crystallize when confident
int32:result = 0i32;
if (updated.c > 0) { result = updated.b; }  // Evidence favors B → 30
if (updated.c < 0) { result = updated.a; }  // Evidence favors A → 25

// Q9<T>: 9-state confidence (nit: -4 to +4) for finer gradients
Q9<int32>:decision = {a: 100i32, b: 200i32, c: -3};  // Moderately favor A

// Q21<T>: 21-state confidence (tbb8: -10 to +10, saturation barriers at ±6)
Q21<int32>:complex = {a: 42i32, b: 84i32, c: 8};  // Past barrier, crystallizable
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
./install.sh --install-deps        # auto-install missing packages
./install.sh --uninstall           # remove installed files
```

### Package Install

```bash
# Debian / Ubuntu / Mint
sudo dpkg -i aria-lang_*_amd64.deb

# Fedora / RHEL
sudo rpm -ivh aria-lang-*.x86_64.rpm
```

Build packages with `./packaging/build-deb.sh` or `./packaging/build-rpm.sh`.

See [INSTALL.md](INSTALL.md) for all installation methods.

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
    exit(0);
};
func:failsafe = int32(tbb32:err) { exit(1); };
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

📖 **[Aria Programming Manual v1.1 (PDF)](https://github.com/alternative-intelligence-cp/aria-docs/releases/tag/v0.16.11)** — 171-page offline manual: specs cheat sheet, full programming guide, safety walkthrough, and reference appendix.

**Quick links in aria-docs:**
- `guide/` — Full programming guide (12 sections)
- `reference/` — Language reference (20+ documents)
- `safety-walkthrough/` — Safety system walkthrough
- `packages/` — API reference for all packages

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
├── tests/                    # Test suite
│   ├── regression/          # Regression tests
│   ├── fuzz/                # Fuzzer V2 and corpus
│   ├── gpu/                 # GPU/CUDA tests
│   └── misc/                # Exploratory/scratch tests and archived test results
├── examples/                 # Example programs
├── docs/                     # Design docs and architecture
├── k-semantics/              # K Framework executable semantics and core tests
├── scripts/                  # Build and maintenance scripts
├── tools/                    # Development tooling (specialist model, semantic_db)
├── benchmarks/               # Performance benchmarks
├── runtime/                  # Runtime C source
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
- [`aria-packages`](https://github.com/alternative-intelligence-cp/aria-packages) — 103 library packages
- [`aria-docs`](https://github.com/alternative-intelligence-cp/aria-docs) — reference docs, guides, and [171-page PDF manual](https://github.com/alternative-intelligence-cp/aria-docs/releases/tag/v0.16.11)
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

# Run K executable-semantics core tests (requires K Framework)
./k-semantics/run_k_tests.sh --require-k

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
- ✅ **Traits & borrow semantics RFC** — Design doc for monomorphized traits + `$$i`/`$$m` borrow qualifiers
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

### v0.2.39 — Released

- ✅ **Enums** — `enum:Name = { VARIANT1, VARIANT2 = 42 };` with auto-numbering (0, or last+1), explicit values, mixed
- ✅ **EnumType** — Proper type identity in type system (not just int64 constants)
- ✅ **Enum-typed variables** — `Color:my_color = Color.RED;`
- ✅ **Enum comparison** — `==` and `!=` between enum values
- ✅ **Enum exhaustiveness** — `pick` statements can check coverage of all variants
- ✅ **Parser pre-pass** — Correctly distinguishes `Enum.VARIANT` from UFCS static calls

### v0.2.38 — Released

- ✅ **AI-Native Filesystem** — aria-aifs package: POSIX file ops + AI metadata tagging via C shim
- ✅ **Shim layer** — 16 C functions for create/read/write/delete/stat/list + tag/untag/find_by_tag

### v0.2.37 — Released

- ✅ **Channels** — `Channel.create(cap)`, `send()`, `recv()`, `try_send()`, `try_recv()`, `select2()`, `close()`
- ✅ **Channel modes** — Buffered, unbuffered (rendezvous), oneshot (auto-close)
- ✅ **Actors** — `actor_spawn(handler)`, `actor_send()`, `actor_stop()`, `actor_destroy()`
- ✅ **Actor patterns** — Reply channels, mailbox access, lifecycle management
- ✅ **aria-channel package** — FanOut, FanIn, Pipeline high-level patterns
- ✅ **aria-actor package** — Actor pool, router, supervisor patterns

### v0.2.36 — Released

- ✅ **dyn Trait** — Dynamic dispatch via vtables, object-safe trait checking
- ✅ **Vtable generation** — Runtime polymorphism for trait objects

### v0.2.35 — Released

- ✅ **Borrow semantics** — `$$i` (immutable) and `$$m` (mutable) borrow qualifiers
- ✅ **Compile-time safety** — N immutable OR 1 mutable borrow enforced
- ✅ **`any` type** — Universal type erased container
- ✅ **Pinning improvements** — `#` operator integration with borrow checker

### v0.2.34 — Released

- ✅ **Type: system** — Composable types with `struct:internal`, `struct:interface`, `struct:type`, methods
- ✅ **Trait bounds on generics** — `func<T: Addable>:name = ...` constrained generics
- ✅ **instance<T>** — Constructor syntax: `instance<Counter>(args)` → `Counter_create(args)`

### v0.2.33 — Released

- ✅ **Generic stdlib** — Generic containers, iterators, optional monads
- ✅ **Foundation packages** — aria-container, aria-iter, aria-optional

### v0.2.30-v0.2.32 — Released

- ✅ **Incremental improvements** — Parser fixes, type system refinements, test coverage expansion

### v0.2.29 — Released

- ✅ **String `+` operator** — `"hello " + "world"` concatenation, chaining, type checker + codegen
- ✅ **`wildx` parser fix** — Qualifier missing from 7 parser locations; added `isWildx` to AST nodes
- ✅ **6 new packages** — aria-jit (8/8), aria-bench (4/4), aria-path (10/10), aria-sort (8/8), aria-queue (7/7), aria-map (8/8)
- ✅ **Legacy stdlib removed** — Entire `lib/_legacy/std/` deleted (42 files, 22 modules); all covered by packages/builtins
- ✅ **80 ecosystem packages** — 45 new tests, all passing

### v0.2.28 — Released

- ✅ **fix256** — 256-bit deterministic fixed-point: arithmetic, comparisons, conversions; SysV ABI fixed for 32-byte structs
- ✅ **frac32** — Fraction type with function-call API (from_parts, add/sub/mul/div)
- ✅ **SIMD builtins audit** — Deferred test audit, 841 tests total, 838 passing (99%)
- ✅ **Dimensional type stubs** — Type infrastructure for unit-typed arithmetic

### v0.2.27 — Released

- ✅ **Complex number stdlib** — Generic complex API (new, add, sub, mul, conjugate) with int32/int64/flt64
- ✅ **Compound generic type inference** — GenericResolver extracts type args from monomorphized struct names

### v0.2.26 — Released

- ✅ **Module resolution fixes** — Restored `std` → `stdlib` symlink, created `stdlib/mem.aria`
- ✅ **4 tests promoted** — 830/833 passing (99%)

### v0.2.25 — Released

- ✅ **SIMD builtins fixed** — Critical bug: LLVM float comparisons (fcmp vs icmp) for vector types, fixed in 10 locations
- ✅ **7 tests unblocked** — simd_sum, simd_broadcast, simd_product, element access, reductions

### v0.2.23 — Released

- ✅ **Trait system** — Definition parsing, implementation, UFCS on primitive types
- ✅ **Parser infinite loop fix** — Trait blocks no longer hang the compiler
- ✅ **815/818 passing** (99%)

### v0.2.22 — Released

- ✅ **Optional types** — `T?` syntax, `NIL` literal, `??` nil-coalescing, `?.` safe navigation, `?` unwrap-with-default
- ✅ **ABI** — Tagged struct `{i1 hasValue, T value}` for optionals

### v0.2.19 — Released

- ✅ **Generic monomorphization fix** — `substituteTypeNode()` for recursive type parameter substitution
- ✅ **Compound types** — GenericType, ArrayType, PointerType substitution in generics

### v0.2.18 — Released

- ✅ **Test-side fixes** — 19 tests promoted to main suite, no compiler changes
- ✅ **Discovery** — `instance<T>()` and `Type.Member` desugaring already work

### v0.2.16 — Released

- ✅ **Spec compliance pass** — 122 test fixes (void→NIL, failsafe, return→pass)
- ✅ **5 compiler bugs fixed** — Plus 47 TODOs audited
- ✅ **Partial feature completion** — cast `=>`, pipelines `|>`, safe nav `?.`, pub use, invariants

### v0.2.15 — Released

- ✅ **Ecosystem & distribution polish** — Tooling updates, LSP grammar +49 keywords, package registry synced (55→74)
- ✅ **Website and repo presentation** — Version strings, code examples, install scripts refreshed

### v0.2.14 — Released

- ✅ **Documentation & quality** — Comprehensive doc review, extended fuzzing, full test suite hardening, code audit across all repos

### v0.2.13 — Released

- ✅ **WebAssembly compilation target** — `ariac --emit-wasm -o program.wasm` compiles Aria to WASI-compatible WebAssembly
- ✅ **WASM runtime** — `libaria_runtime_wasm.a`: strings, I/O, math, allocators, maps (~850 LOC)
- ✅ **LLVM WebAssembly backend** — `emit_wasm_object()`, wasm-ld linking, WASI entry point handling
- ✅ **Compatibility checker** — Warns about unsupported features (threading, async, fork/exec) at compile time
- ✅ **Compiler-rt builtins** — `__multi3` (128-bit multiply) for int64 arithmetic on wasm32
- ✅ **WASM test suite** — 4/4 passing: hello world, strings, arithmetic, functions/recursion

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

### v0.3.0 — Released

- ✅ **`--static` flag** — Compile static executables with all dependencies linked
- ✅ **Fuzzer rewrite** — 27 generators, 100% compile rate, zero unresolved crashes
- ✅ **BUG-001 through BUG-004 fixes** — Module resolution for FFI packages, linker flag ordering, failsafe/main/exit contract enforcement
- ✅ **92 ecosystem packages** — 5 new: rules-common, ini, hex, ringbuf, pqueue

### v0.3.1 — Released

- ✅ **Version bump** — Package ecosystem expansion and stability improvements

### v0.3.2 — Released

- ✅ **101 ecosystem packages** — aria-stats, aria-matrix, aria-mock, aria-bigdecimal + SDL3, wxWidgets, WebKitGTK wrappers
- ✅ **GUI framework expansion** — SDL3, wxWidgets, WebKitGTK bindings via C shim pattern

### v0.3.3 — Released

- ✅ **Safety showcase & tooling polish** — CI/CD badges, comprehensive documentation fixes (695+)
- ✅ **Dead code removal** — 860 lines of dead code removed from compiler
- ✅ **Debug print gating** — 118 debug prints behind `ARIA_DEBUG_CODEGEN` flag
- ✅ **LSP version bump** — Language server updated to 0.3.3

### v0.3.4 — Released

- ✅ **Z3 SMT Phase 2: Design-by-Contract verification** — `requires`/`ensures` clauses verified at compile time via Z3 solver. Function preconditions and postconditions are mathematically proven across all execution paths.
- ✅ **Z3 SMT Phase 3: Arithmetic overflow proofs** — Integer arithmetic operations verified using Z3 bitvector overflow intrinsics (`bvadd_no_overflow`, `bvsub_no_underflow`). Proves absence of overflow for bounded inputs.
- ✅ **`--verify-contracts` flag** — Enable contract verification pass
- ✅ **`--verify-overflow` flag** — Enable overflow verification pass
- ✅ **`--verify-report` flag** — Emit detailed proof results (proven/unproven/skipped per function)
- ✅ **Regression fix** — `test_nested_struct_array` updated: `pass()` → `exit()` in main

### v0.4.x–v0.10.x — Released

- ✅ **v0.4.x** — Z3 Phase 1 (Rules/limit), ecosystem to 103 packages
- ✅ **v0.5.x** — Z3 Phase 2–3 (contracts, overflow, concurrency, memory verification), `prove`/`assert_static`, `--smt-opt`
- ✅ **v0.6.x** — TFP types, compiler hardening
- ✅ **v0.7.x** — JIT: 45+ instructions, register allocator, peephole optimizer, WildX security
- ✅ **v0.8.x–v0.9.x** — Runtime improvements, GC tuning
- ✅ **v0.10.x** — 8 stdlib modules ported to aria-libc, 12 pure Aria packages, tests/docs/audit

### v0.11.x — Released

- ✅ **Threading & concurrency** — Thread pool, atomics, lock-free structures (MPMC queue, Treiber stack, SPSC ring buffer)
- ✅ **Synchronization** — Mutex, RWLock, Barrier, CondVar
- ✅ **OS components** — Arena/pool/slab allocators, shared memory, IPC, signal handling, process management
- ✅ **AI-native filesystem (AIFS)** — FUSE-based filesystem for AI workloads
- ✅ **AriaX kernel mods** — Hexstream FD 3-5 patches

### v0.12.x — Released

- ✅ **Networking** — HTTP client, DNS, socket, server, URL parsing
- ✅ **Middleware** — Cookie, CORS, body-parser, session, static file serving, rate limiting
- ✅ **Protocols & terminal** — FTP, SMTP, WebSocket, ANSI display, raw keyboard input
- ✅ **Utilities** — LRU cache, glob matching, retry with backoff

### v0.13.x — Released

- ✅ **Trait system** — Definitions, `impl` blocks, `$i`/`$m` borrows, vtable dispatch, trait bounds on generics
- ✅ **Enum types** — Tagged unions, pattern matching, exhaustiveness checking
- ✅ **Generic containers** — Type-parameterized structs/functions, monomorphization
- ✅ **Deferred features** — Raw strings, pipeline operator, extended escape sequences
- ✅ **@ function pointers** — Trampoline generation for `@func_name` references
- ✅ **Documentation** — specs_list.txt, full reference docs, 234-page PDF manual
- ✅ **Final audit** — 959 tests, 17h+ fuzzing (0 crashes), benchmarks, KNOWN_ISSUES/BUGS cleanup

### v0.14.x — Released

- ✅ **SMT solver expansion** — Contract proofs, range inference, data race analysis, fast-paths, comprehensive documentation

### v0.15.x — Released

- ✅ **Self-hosting foundation** — 5 compiler modules ported to Aria (lexer, parser, type checker, borrow checker, safety checker)
- ✅ **Tier 2 self-hosting** — 4 modules ported (exhaustiveness checker, const evaluator, module resolver, visibility checker)
- ✅ **Tier 3 tool ports** — Doc generator, package manager, project config ported to Aria
- ✅ **Self-hosting census** — 12 total modules ported, final audit and documentation

### v0.16.x — Released

- ✅ **Comprehensive code review** — Full C++ compiler, runtime, and toolchain reviewed (~122,000+ lines, 130+ files)
- ✅ **11 compiler bugs fixed** — Dead code removal, wrong-value bugs, memory/ownership fixes, ABI corrections
- ✅ **72 TODO resolutions** — Backend codegen, type checker, analysis passes, async/runtime stubs, parser, tools
- ✅ **Subsystem reviews** — Frontend, semantic analysis, backend, runtime, tools each reviewed independently
- ✅ **Stdlib audit** — 72 .aria files reviewed, 13 bugs fixed
- ✅ **Test stability** — 1,015 tests (891 positive, 124 expected-failure), AriaString ABI fix, @cast<> codegen, exit-in-lambda enforcement
- ✅ **Error messages** — Contextual hints and improved diagnostics across 9 source files (37+ messages)

### v0.17.x — Released

- ✅ **Install script expansion** — Distro detection, dependency installation,
    build-only/custom-prefix/uninstall modes, and post-install verification
- ✅ **Debian packaging** — Standalone `.deb` builder with runtime dependencies
    and bundled Aria libraries
- ✅ **RPM packaging** — `.rpm` builder with generated spec and staged install
    tree
- ✅ **aria-pkg remote fetch** — `update`, `list --remote`, and remote package
    install from the GitHub package registry
- ✅ **Install documentation** — Source, script, package, and package-manager
    install paths documented

### v0.18.0 — Released

- ✅ **K Framework executable semantics** — First formal semantics seed in
    `k-semantics/aria.k`
- ✅ **Core K test corpus** — 105 programs covering exit, arithmetic, binding,
    fixed values, loops, Result operations, sticky `ERR`, failsafe routing,
    `if`/`else`, helper calls, strings/stdout, structs, `pick`/`fall`, and
    integer `Rules` / `limit<Rules>` checks, plus initial `stack`/`gc`/`wild`
    memory qualifier checks, `defer { ... }` cleanup checks, `@`/`<-` pointer
    read/store-through checks, pointer-member read/store-through checks,
    nested pointer-member path read/store-through checks,
    `#` pin dereference/read-only checks, pin-member store-through rejection,
    pin-derived nested path store-through rejection, pinned-host field mutation rejection,
    direct and nested struct-field borrow path checks, local field-alias
    writeback checks, `wildx` cleanup checks, pinned-host by-value rejection,
    and `$$i`/`$$m` borrow permission checks
- ✅ **CTest integration** — K semantics test passes when K is installed and
    skips cleanly when K is absent
- ✅ **Proof-oriented `kprove` hook** — Haskell-backend proof runner integrated
    with CTest; core arithmetic, field-alias, pin read-only, pinned by-value,
    local pointer, pointer-path, borrow-path, control/rules, arithmetic, and
    Result claim modules are passing
- ✅ **Final audit** — `AUDIT_v0.18.0.md` records validation, proof corpus, gaps,
    and the non-compiler wall for array/index borrow paths
- ⏭️ **Next** — Nitpick rebrand repository/README transition, then future
    compiler/K expansion tracks as needed

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
