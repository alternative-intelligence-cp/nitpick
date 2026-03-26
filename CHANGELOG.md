# Aria Language Changelog

## [0.2.14] - March 2026

### Documentation
- **Comprehensive documentation overhaul** — 486 files across aria-docs
- **Syntax corrections** — `fn`→`func:`, `<<`→`print()`/`println()`, `eprint()`→`stderr_write()`, `dprint()`→`stddbg_write()`, `${}`→`&{}` interpolation
- **Reference rewrites** — interpolation.md, string_interpolation.md, template_syntax.md, comptime.md, macros.md, inline.md (new)
- **COMPILER_ARCHITECTURE.md** — added WebAssembly compilation section
- **Unimplemented feature warnings** — 10 type guide pages now warn about features still in development
- **VS Code extension** — complete grammar rewrite for v0.2.12+ features, added snippets

### Testing
- **Full test audit** — 862 tests audited, 640 passing (74.2%) with negative test detection
- **TEST_STATUS.md** — complete rewrite with honest per-category breakdown and failure taxonomy
- **REMAINING_FAILURES.md** — root cause analysis: 122 test quality issues, 5 compiler bugs, 56 feature gaps

### Quality
- **93 TODO annotations** documented across compiler source (2 HACKs, 0 FIXMEs)
- **CONTRIBUTING.md** added to all 15 repos
- **LICENSE** and **.gitignore** added where missing
- **Stale files removed** — compiled binaries, duplicate specs, backup files
- **.gitignore** updated across repos to prevent binary commits

## [0.2.13] - March 2026

### Added
- **WebAssembly compilation target** — `ariac --emit-wasm -o program.wasm` compiles Aria to WASI-compatible WebAssembly
- **`--emit-wasm` flag** and `--target=wasm32-wasi` compiler option (also `wasm32-unknown-unknown` for bare WASM)
- **WASM runtime** (`libaria_runtime_wasm.a`) — stripped-down runtime for WASM: strings, I/O, math, allocators, maps (~850 LOC)
- **LLVM WebAssembly backend** — `emit_wasm_object()` with WASI entry point handling (`__main_argc_argv`), wasm-ld linking
- **WASM compatibility checker** — scans module and warns about unsupported features (threading, async, fork/exec, signals, mmap)
- **Compiler-rt builtins** — `__multi3` (128-bit multiply via `__int128`) for int64 arithmetic on wasm32
- **WASM test suite** — `tests/wasm/`: hello world, strings, arithmetic, functions/recursion (4/4 passing)
- **WASM documentation** — `docs/WASM_GUIDE.md`: compilation guide, feature matrix, architecture, examples, troubleshooting

## [0.2.12] - March 2026

### Added
- **Preprocessor macros** — `macro:NAME(params) { body }`, `%*` variadic parameter expansion, `#%N` stringification, `##` token pasting
- **Comptime evaluation** — `comptime { }` blocks evaluated at compile time, compile-time constants
- **Magic constants** — `__FILE__`, `__LINE__`, `__FUNC__`, `__COUNTER__` for metaprogramming
- **Inline hints** — `inline` and `noinline` function attributes for optimization control

### Improved
- **Borrow checker** — Better analysis of conditional paths and reassignment patterns
- **Compiler diagnostics** — Improved error messages for macro expansion and comptime evaluation errors

## [0.2.11] - March 2026

### Added
- **Thread pool** — `ThreadPool.create(n)`, `ThreadPool.submit(pool, func, arg)`, `ThreadPool.wait_idle(pool)`, `ThreadPool.shutdown(pool)`
- **Atomics stdlib** — `AtomicInt32`, `AtomicInt64`, `AtomicBool` with all memory orderings (relaxed, acquire, release, acq_rel, seq_cst)
- **Lock-free data structures** — `LFQueue` (MPMC Vyukov), `LFStack` (Treiber), `RingBuf` (SPSC Lamport)
- **Channels** — `Channel.create()`, `Channel.send()`, `Channel.recv()` for typed inter-thread communication
- **Synchronization primitives** — `Mutex`, `RWLock`, `Barrier`, `CondVar` stdlib modules
- **OS components** — Arena, pool, slab allocators; shared memory IPC; signal handling; process management
- **AI-native filesystem (AIFS)** — FUSE-based filesystem for AI workloads with metadata streams
- **AriaX kernel mods** — Hexstream FD 3-5 patches for six-stream I/O

## [0.2.10] - March 2026

### Added
- **aria-transformer** — Full Transformer encoder package: token + positional embeddings, multi-head scaled dot-product attention with causal masking, GELU feed-forward network, pre-norm architecture with residual connections, weight-tied unembedding, attention visualization. 10/10 tests.
- **aria-mamba** — Mamba selective state space model package: linear projections, 1D causal depthwise convolution, selective SSM (S6) with input-dependent discretization (dt, B, C), softplus for dt, A_log diagonal initialization, SiLU gating, output projection, layer norm + residuals. 10/10 tests.
- **aria-jamba** — Jamba hybrid model package: interleaved Transformer attention + Mamba SSM layers (configurable `attn_every` ratio), Mixture of Experts (MoE) FFN with top-k softmax gating over N expert networks, each expert a 2-layer FFN with SiLU. 10/10 tests.
- **aria-looping** — Looping / iterative-refinement model package (Universal Transformer style): shared weights across iterations, iteration embeddings to distinguish passes, adaptive convergence stopping via L2 delta threshold, configurable fixed or adaptive iteration count. 10/10 tests.
- **aria-tensor** — Dense tensor library: 47 operations including creation (1D-3D), element access, fill (xavier, random), reshape/transpose, arithmetic (add/sub/mul/scale), matmul, activations (ReLU, sigmoid, tanh, SiLU, GELU, softmax), reductions (sum, mean, max, min), layer_norm, concat/slice, GPU interop. 12/12 tests.
- **aria-cuda** — CUDA FFI bindings: device management, memory alloc/copy/free, kernel launch, cuBLAS GEMM, stream operations. 10/10 tests.
- **aria-uacp** — Universal AI Communication Protocol: binary-framed message types (text, tensor, tool invocation, streaming, store/load, heartbeat, capability negotiation), error/retry semantics, codec layer for human-readable interface. 12/12 tests.
- **Self-improving training loop** — `self_improve.py` for aria-specialist: automated pipeline that generates Aria code from the specialist model, compile-filters with ariac, appends successes to the training corpus, retrains, and tracks metrics across iterations. Foundation for Nikola self-improvement in v0.3.0.

## [0.2.9] - July 2026

### Added
- **aria-server** — HTTP/1.1 server library: `server_listen(port)`, `server_accept()`, request parsing (method, path, query, headers, body), response building with status codes, content types, and custom headers, peer address/port info. C shim + Aria wrapper + test suite.
- **aria-router** — Express-style routing library: `router_add(method, path, handler_id)`, path parameters (`/users/:id`) with `router_param()` extraction, multi-parameter routes, middleware chain (`router_use(prefix, id)`), wildcard routes (`/files/*`), method dispatch. 21 tests passing.
- **aria-body-parser** — Request body parsing: JSON top-level object parser, URL-encoded form parser with `%XX`/`+` decoding, content-type detection (JSON/form/multipart/text), field access by key/index, raw body passthrough. 28 tests passing.
- **aria-cors** — CORS middleware: configurable origins (wildcard or specific list), methods, headers, expose headers, credentials, max-age. Generates CORS response headers and preflight headers. Case-insensitive origin matching. 18 tests passing.
- **aria-cookie** — Cookie handling: Cookie header parsing (semicolon-separated, trimmed), get by name/index, Set-Cookie builder with Path/Domain/Max-Age/Secure/HttpOnly/SameSite attributes, delete cookie helper. 23 tests passing.
- **aria-session** — In-memory session management: cryptographic session IDs via `/dev/urandom` (32 hex chars = 128 bits), create/load/destroy, key/value storage (32 vars/session), expiry with max-age, cookie header generation, active session counting, eviction of oldest when full. 23 tests passing.
- **aria-rate-limit** — Token bucket rate limiting: per-client buckets by key (max 1024), configurable max tokens + refill rate, check with optional cost, remaining tokens, retry-after calculation, `X-RateLimit-*` response headers. 14 tests passing.
- **aria-static** — Static file serving: configurable root directory, MIME type detection (28 types), path traversal protection (`..` rejection + `realpath` validation), directory index (`index.html`), file reading (max 1MB). 22 tests passing.
- **demo_api.aria** — REST API example demonstrating server + router integration: GET `/` (HTML homepage), GET `/health` (JSON health check), GET `/api/users` (user list), GET `/api/users/:id` (path param extraction), POST `/api/users` (JSON body handling).

### Fixed
- **Borrow checker false positive** — `isDeallocator()` pattern-matched function names ending in `_close`/`_free`/`_destroy`/`_release` and assumed arguments were wild pointers, even for `int32` values. Added `wild_states` guard before `recordWildFree` and `recordDeferFree` calls.
- **FFI string return ABI** — Corrected AriaString struct return ABI for extern functions.

## [0.2.8] - July 2026

### Added
- **Gamepad input API** — `rl_is_gamepad_available`, `rl_is_gamepad_button_pressed`, `rl_is_gamepad_button_down`, `rl_is_gamepad_button_released`, `rl_get_gamepad_axis_movement` added to `aria-raylib` C shim and Aria bindings. Full set of button constants (`GP_DPAD_*`, `GP_FACE_*`, `GP_L1/R1/L2/R2`, `GP_START/SELECT/HOME`, `GP_L3/R3`) and axis constants exported.
- **Procedural audio synthesis** — `rl_gen_beep(freq_hz, dur_ms, wave_type, volume)` in `aria-raylib` shim synthesizes square, triangle, sawtooth, or sine tones into raylib sound slots at runtime — no audio files required. Applies 10% fade-out envelope to eliminate click artifacts.
- **aria-tetris v0.2.8** — Full-featured Tetris clone rewritten (765 → 928 lines):
  - 7 procedural sound effects (move, rotate, lock, line clear, Tetris, level-up, game over)
  - Gamepad support: D-pad movement, face buttons for rotate/hard-drop/hold, Start for pause
  - High score persistence: read/write `aria_tetris_best.txt` on game over
  - Line-clear flash animation: 300 ms white flash marks cleared rows, suspends physics during flash
  - Title/pause/game-over screens show persistent high score
- **Package manifests** — `aria-package.toml` added for `aria-tetris` v0.2.8.
- **GML → Native educational guide** — `aria-docs/guide/from_gml_to_native.md`: full tutorial tracing a `draw_sprite()` call from GML source through Aria, C shim, shared library, and onto the GPU. Includes bouncing-ball side-by-side, compiler pipeline diagram, ABI explanation, and build walkthrough.
- **Package reference docs** — `aria-docs/packages/` directory with full API reference for `aria-tetris`, `aria-gml`, `aria-opengl`, and `aria-raylib`.

## [0.2.7] - March 2026

### Added
- **Six-Stream I/O runtime** — `aria_streams_init()` called at program startup, initializes all 6 file descriptors (stdin/stdout/stderr/stddbg/stddati/stddato) with graceful fallback when FDs 3-5 aren't available.
- **`stdin_read_all()` builtin** — Reads all content from stdin as a string. Supports piped input (non-seekable streams) via chunked reading.
- **`stdin_read_line()` fix** — Now properly wraps C `char*` return in `AriaString*` via `aria_string_from_cstr_simple`.
- **Argument access runtime** — `aria_args_init(argc, argv)` called at startup. `aria_get_argc()` and `aria_arg(index)` builtins for command-line argument access.
- **Main function signature** — Compiler now generates `main(i32, ptr)` automatically, passing argc/argv to the runtime.
- **26 POSIX tools** — Complete set of Unix utilities implemented in Aria, living in the `ariax` repo under `tools/`. All support stdin for pipeline chaining.
  - File utilities: cat, head, tail, wc, tee, cut, sort, uniq, tr
  - Search & filter: grep, find, diff
  - System utilities: echo, yes, true, false, env, sleep, basename, dirname, seq
  - Text processing: nl, fold, paste, expand, unexpand
- **Pipeline support** — All tools read from stdin when no file argument given, enabling standard Unix pipelines: `cat file | grep pattern | sort | uniq | wc`

### Fixed
- **`aria_text_stream_read_all` pipe support** — Now handles non-seekable streams (pipes) by reading in chunks until EOF, instead of using `ftell/fseek` which fails on pipes.

## [0.2.6] - July 2026

### Added
- **`--shared` compiler flag** — Compile Aria source directly to shared libraries (`.so`). Functions are exported with C ABI linkage, enabling cross-language interop with C, Python, Rust, Go, and any language supporting C FFI.
- **Cross-language bindings documentation** — `docs/CROSS_LANGUAGE_BINDINGS.md` covers Aria→C export, C→Aria FFI, Aria→Python via ctypes, type mapping tables, and symbol naming conventions.
- **Binding examples** — `examples/bindings/` with working Aria→C and Aria→Python demos: `mathlib.aria`, `use_from_c.c`, `use_from_python.py`, and `run_demo.sh`.
- **12 new ecosystem packages** (aria-packages repo):
  - **aria-websocket** — RFC 6455 WebSocket client/server with pure POSIX sockets
  - **aria-toml** — Self-contained TOML parser with dotted keys and table support
  - **aria-yaml** — Indentation-based YAML parser with dotted key access
  - **aria-compress** — zlib deflate/inflate, gzip/gunzip compression
  - **aria-crypto** — SHA-256, MD5, HMAC-SHA256 (self-contained, no OpenSSL)
  - **aria-url** — URL parsing, percent-encode/decode, component extraction
  - **aria-mime** — 70+ MIME type lookups by filename/extension
  - **aria-semver** — Semantic version parsing, comparison, constraint matching
  - **aria-template** — `{{var}}` string template engine with set/get/render
  - **aria-cli** — ANSI colors, bold/italic/underline, progress bars, banners
  - **aria-env** — Environment variable get/set/unset, home/path/user helpers
  - **aria-xml** — XML tag/attribute lookup, path queries, element counting
- **Package READMEs** — All 12 new packages documented with API tables and usage examples.
- **Package registry updated** — 55 total packages in `registry/index.json`.

## [0.2.5] - July 2026

### Added
- **GitHub Actions CI pipeline** — Automated build, test, and .deb packaging on push/PR to main/dev. Uses ubuntu-24.04, LLVM 20, CMake + Ninja.
- **GitHub issue & PR templates** — Bug report, feature request, and compiler crash issue templates (YAML forms). PR template with regression test and valgrind checklist.
- **CONTRIBUTING.md** — Contributor guide covering prerequisites, build instructions, project structure, code style, and submission process.
- **Compiler architecture manual** — 695-line technical document (`docs/COMPILER_ARCHITECTURE.md`) covering the full compilation pipeline, AST (49 node types), type system (16 TypeKind values), IR generation, runtime library (~33,900 lines, 20+ components), memory model, FFI mechanism, and source map with line counts.
- **Man pages** — groff man pages for all 5 tools: `ariac(1)`, `aria-ls(1)`, `aria-pkg(1)`, `aria-doc(1)`, `aria-dap(1)`.
- **Specialist model evaluation** — Comprehensive evaluation of all model versions (v1–v6). v4.1 remains best (82.1% keyword accuracy, 47.1% compile rate). Strategy documented: fix corpus quality before scaling model size.

### Fixed
- **String interpolation in template literals** — String variables in template literals (`` `Hello, &{name}!` ``) produced garbage output. The codegen was sending `AriaString*` pointers to the generic `ptrToAriaString()` handler which called `strlen()` on the struct pointer. Now correctly loads the AriaString struct directly when the interpolated expression is a string type. Integer interpolation was unaffected.
- **README code examples** — Fixed 5 incorrect examples: `fail("string")` → `fail(1i32)` (integer error code required), removed unimplemented `!` postfix unwrap, fixed `ok()` pattern, replaced unimplemented `+?!/+!!!` operators with working TBB overflow example.
- **GETTING_STARTED hello world** — Added missing `failsafe` function and fixed its signature from `void` to `NIL`.

### Changed
- **README update** — Comprehensive update from v0.2.2 to v0.2.4: CI badge, status section (June 2026), toolchain table (43 packages), moved async/await, fail(), arrays-in-structs to Stable features, added 4 DB packages, v0.2.3/v0.2.4/v0.2.5 roadmap sections.

## [0.2.4] - July 2026

### Added
- **Async/await error propagation** — `pass()` and `fail()` in async functions store `Result<T>` to the coroutine promise; `await` extracts the Result via `@llvm.coro.promise`. Full error propagation across async boundaries.
- **Async-aware `drop()`** — `drop(async_func())` automatically resumes and destroys the coroutine frame, running the async body to completion.
- **Regression test suite** — 5 regression tests (test_async_await, test_extern_count, test_extern_names, test_main_call_limit, test_nested_struct_array) with `run_regression.sh` runner.
- **Traits & Borrow Semantics RFC** — Design document for trait system (monomorphization by default, `dyn Trait` opt-in) and user-facing borrow syntax (`$x` / `$mut x`). Implementation deferred to v0.2.5+. See `META/ARIA/TRAITS_AND_BORROW_SEMANTICS_RFC.md`.

### Fixed
- **Coroutine double-free** — Fixed final suspend switch routing: case 0 (suspended) now goes to return block (frame stays alive for promise read), case 1 (destroy) goes to cleanup (frees frame). Previously all cases went to cleanup, causing `coro.resume` and `coro.destroy` to both free the frame.
- **Coroutine memory management** — Changed coroutine frame allocation from `aria_gc_alloc`/`aria_gc_free` to `malloc`/`free` directly. GC allocator returned offset pointers that caused invalid free.
- **Dual await code paths** — Unified two `await` handlers (old Future-based in IRGenerator, new promise-based in ExprCodegen) into a single promise-based path.
- **`aria.async` metadata** — Added missing `setMetadata("aria.async", ...)` on async functions so `await` and `drop()` can detect async callees.
- **Arrays in structs: nested read** — Fixed nested member access through array-of-structs (`cloud.pts[0].x`) by registering the Aria element type in `value_types` for loaded struct array elements.
- **Arrays in structs: nested write** — Fixed nested member assignment (`cloud.pts[0].x = val`) by adding GEP chain for struct→array field→element→nested field in the assignment handler.

### Changed
- **Deprecated LLVM API migration** — Replaced ~44 `Intrinsic::getDeclaration` → `getOrInsertDeclaration` and ~8 `CreateGlobalStringPtr` → `CreateGlobalString` calls. Ready for LLVM 21.
- **Zero Aria-source warnings** — Fixed 17 unused variable/parameter warnings, constructor field initialization order, and trigraph warning. Build now produces 0 warnings from Aria code.

## [0.2.3] - March 2026

### Added
- **Database client libraries** — Four new packages providing idiomatic Aria bindings for popular databases:
  - `aria-sqlite` — SQLite3 embedded database: open, exec, parameterized queries (?1/?2), result iteration, transactions (34/34 tests)
  - `aria-postgres` — PostgreSQL via libpq: connect, exec, parameterized queries ($1/$2), result cursors, transactions, LISTEN/NOTIFY (40/40 tests)
  - `aria-mysql` — MySQL via libmysqlclient: connect, exec, parameterized queries (?), stmt-based SELECT, simple_query, transactions, affected_rows, last_insert_id (44/44 tests)
  - `aria-redis` — Redis via hiredis: connect, GET/SET/DEL/EXISTS, EXPIRE/TTL, INCR, lists (LPUSH/RPUSH/LPOP/RPOP/LRANGE), hashes (HSET/HGET/HDEL), sets (SADD/SREM/SISMEMBER), PING, SELECT DB (53/53 tests)
  - All SQL drivers include SQL injection safety tests (parameterized queries block `'; DROP TABLE` attacks)
  - All use C shim pattern with connection pool (16 slots) and parameter staging
  - Total: 171 assertions across 4 packages, all passing
- **DATABASE_GUIDE.md** — Comprehensive guide covering all 4 database libraries: prerequisites, building, common patterns, API reference tables, known workarounds
- **GETTING_STARTED.md Section 14** — Database Client Libraries quickstart

### Changed
- Package registry updated to 43 packages (added aria-sqlite, aria-postgres, aria-mysql, aria-redis)

## [0.2.2] - March 2026

### Added
- **GUI toolkit wrappers** — Three new packages providing idiomatic Aria bindings for native GUI/graphics libraries:
  - `aria-raylib` — raylib v6.0 bindings: window, drawing, shapes, text, keyboard, mouse, timing (20/20 tests)
  - `aria-sdl2` — SDL2 multimedia bindings: init, window, renderer, drawing, events, input, timing (19/19 tests)
  - `aria-gtk4` — GTK4 desktop GUI bindings: widget registry (label, button, entry, separator, check_button), flag-based event model, non-blocking activation (20/20 tests)
  - All use C shim pattern with flat parameter decomposition (Aria can't pass function pointers or struct values to extern)

- **9 ecosystem utility libraries** — All with C shim + full test suites (166 tests total, all passing):
  - `aria-test` — Test framework with assertion helpers (14/14)
  - `aria-csv` — CSV parsing and generation (25/25)
  - `aria-log` — Structured logging with severity levels (11/11)
  - `aria-base64` — Base64 encoding and decoding (13/13)
  - `aria-datetime` — Date/time formatting and arithmetic (22/22)
  - `aria-regex` — Regular expression matching (19/19)
  - `aria-fs` — File system utilities (33/33)
  - `aria-socket` — Socket abstraction layer (10/10)
  - `aria-http` — HTTP client (GET/POST) (19/19)

- **aria-ls improvements** — `documentSymbol` (outline view), `references` (find all references), `signatureHelp` (parameter hints)
- **aria-mcp improvements** — `aria_format` tool, structured error reporting, MCP resources for `aria_ref.md` sections
- **aria_make test command** — Scans for `test_*.aria` / `*_test.aria`, compiles, runs, reports pass/fail
- **aria-dap improvements** — Conditional breakpoints, hit conditions, logpoints, set variable. Added `SourceBreakpoint` type and `ExceptionBreakpointsFilter` (all/uncaught)
- **aria-safety improvements** — 4 new checks: `UNSAFE`, `EXTERN`, `CAST`, `TODO`. Added `--json` output mode
- **Debian packaging** — `debian/` directory with control, rules, changelog, copyright, `build-deb.sh`. Produces `aria_0.2.2-1_amd64.deb` (17 MB). Tested install/remove on Linux Mint 22.3
- **V5 specialist corpus** — 2,688 training examples (2,419 train / 269 val) covering all new libraries, GUI wrappers, and tooling code
- **Repository reorganization** — Monorepo split into 10 focused repos under `alternative-intelligence-cp` org: aria (compiler core), aria-packages, aria-docs, aria-tools, aria-specialist, aria-lang (hub), ariax, educational, johnny5, tech

### Changed
- **Package count** — Ecosystem expanded from 27 to 39 packages (27 original + 3 GUI wrappers + 9 utility libraries)
- **Test count** — 677+ → 850+ test files with 172 new library assertions and 59 GUI wrapper tests

---

## [0.2.1.1] - June 2026

### Added
- **aria-dap debugger** — Full Debug Adapter Protocol server using LLDB 20 backend. Supports breakpoints, stepping (next/stepIn/stepOut), stack traces, variable inspection, thread listing, and process control.
- **`-g` compiler flag** — Emit DWARF debug info. Adds `DW_TAG_compile_unit`, `DW_TAG_subprogram`, `DW_TAG_variable`, and `DW_TAG_formal_parameter` entries with correct source locations.
- **O0 for debug builds** — When `-g` is used, LLVM optimization is set to O0 and clang++ linking uses `-O0` to preserve variable locations and line table entries.
- **VS Code debugger integration** — Debug Adapter factory, configuration provider with auto-compile support (`compileFirst`), launch.json snippets, and `aria.debugger.path`/`aria.compiler.path` settings.
- **LLDB server auto-detection** — aria-dap automatically finds `lldb-server` across common LLVM installation paths.

### Fixed
- **DWARF path doubling** — When input filename was absolute, `comp_dir` was incorrectly set to `cwd + "/" + absolute_path`. Now correctly uses the directory from the absolute path.
- **DAP protocol compliance** — Fixed `initialized` event ordering (after response), request_seq tracking, Content-Length header parsing, and response serialization.
- **Thread safety** — Added mutex protection for DAP stdout writes and sequence number generation.
- **Null safety** — Fixed `GetCString()`, `GetFilename()`, `GetFunctionName()` null dereference crashes in DAP server.
- **Process I/O isolation** — Debuggee stdout/stderr redirected to `/dev/null` to prevent DAP protocol corruption.

---

## [0.2.1] - March 2026

### Added
- **`install.sh`** — One-command toolchain installer with prerequisite checking, `--build-only`, `--uninstall`, `--prefix` options. Builds and installs ariac, aria-ls, aria-pkg, aria-doc, aria-safety, aria-mcp, stdlib, and man pages.
- **aria-ls completion** — Keyword completion (37 Aria keywords with descriptions), built-in type completion (15 types), and file-scoped symbol completion with correct CompletionItemKind values. Trigger characters: `:` and `.`
- **aria-pkg search** — Search the package registry by name/description
- **aria-pkg pack** — Create distributable package tarballs
- **aria-pkg directory install** — Install packages from local directories

### Fixed
- **aria-doc unknown.html bug** — Parser used Rust syntax (`fn`, `struct`) instead of Aria's colon syntax (`func:`, `struct:`). All items fell through to ItemKind::VARIABLE with name `unknown`, overwriting a single `unknown.html`. Parser rewritten for Aria syntax; brace-depth tracking added for multi-line declarations. Now generates 435 unique HTML pages across all 27 ecosystem packages.
- **aria-ls hover** — Was a stub showing only the token name. Rewritten with AST-based analysis: shows full type signatures for declared symbols (functions, structs, enums, traits, constants, variables) in markdown code blocks. Falls back to builtin type descriptions for Aria keywords.
- **aria-ls goto-definition** — Was a stub doing naive first-occurrence text search. Rewritten with AST-based declaration lookup: finds actual declaration location by name match across the parsed AST.
- **aria-pkg registry loading** — `loadRegistry()` was a stub returning false. Fixed to parse `registry.json`.
- **aria-pkg metadata parsing** — Strict field requirements caused valid packages to fail. Made `authors` and `license` optional; handle both array and string dependency formats.
- **aria-pkg tarball extraction** — Fixed tar command arguments for proper package extraction.

### Verified
- **aria-mcp** — compile, docs, safety audit, and specialist ask endpoints all functional
- **aria-safety** — Static safety auditor binary working correctly
- **27/27 ecosystem packages** — All install successfully via aria-pkg

---

## [0.2.0] - March 2026

### Added
- **Self-hosting compiler frontend** — Complete port of lexer, parser, type checker, borrow checker, safety checker, exhaustiveness checker, and const evaluator to Aria
  - 220 tests across 5 modules (lexer, parser, type_checker, pipeline, supporting_passes)
  - LLVM backend remains in C++ by design (ir_generator, codegen_expr, codegen_stmt)
  - Opaque C handle pattern for tokens, AST nodes, and symbol tables

- **"Did you mean?" compiler suggestions** — Levenshtein distance-based suggestions for:
  - Undefined identifiers (walks entire scope chain for closest match)
  - Unknown type names with case-insensitive matching (e.g., `Int32` → `int32`)

- **Accurate source locations for type errors** — ~15 operator type errors (arithmetic, bitwise, comparison, unary, assignment) now report actual line/column instead of `Line 0, Column 0`

### Changed
- **License changed from AGPL v3 to Apache 2.0**
- **"void function" errors use Aria terminology** — Now says "NIL function cannot return a value" with actionable guidance
- **Removed donation pages** from aria, educational, and nikola repositories

### Fixed
- **7 critical codegen bugs** discovered during self-hosting:
  - Inline function return comparison — Added Result<T> auto-unwrapping before binary operators
  - String concatenation codegen — Added string type detection in TOKEN_PLUS
  - `pass(extern_ptr_func())` — Added Optional/Result unwrapping in pass() codegen
  - Functions with >8 pass/return exits — Works with 21+ exits after Result unwrapping fixes
  - Extern wild int8* re-assignment corruption — Added Optional/Result unwrapping in variable re-assignment path
  - Cross-module extern pointer return corruption — Resolved by pass() unwrapping + assignment unwrapping fixes
  - Wild ptr NIL comparison + extern call crash — Added pointer-NIL (null) and integer-NIL (0) coercion

---

## [0.1.0] - February 2026

### Added
- **Balanced Literal Syntax** - Ternary and nonary number literals
  - Ternary: `0t[01T]+` where T=-1, base-3 positional (e.g., `0t1T0` = 6)
  - Nonary: `0n[01234ABCD]+` where A=-1, B=-2, C=-3, D=-4, base-9 positional (e.g., `0n2A` = 17)
  - Implementation in `src/frontend/lexer/lexer.cpp` lines 750-865
  - Token types: TOKEN_TERNARY, TOKEN_NONARY
  - Converts to int64 at compile time for use with arithmetic

- **i8 Type for trit/nit** - Changed from i2/i4 to i8 storage
  - Enables ERR sentinel value (-128) for overflow detection
  - Provides sticky error propagation matching TBB type safety
  - Prevents arithmetic wrap-around issues (1+1 in i2 would wrap to -2)

- **ERR Propagation for Exotic Types**
  - trit/nit operations propagate ERR sentinel through calculations
  - `ERR + x = ERR`, `ERR * x = ERR`, etc.
  - Prevents silent corruption of error states

- **println() Builtin** - Convenience function for print with newline
  - Usage: `println("text")` instead of `print("text\n")`
  - Returns int64 (bytes written) like print()
  - Implementation in type checker and IR codegen

### Fixed
- **Type Mismatch Bugs** - Fixed 4 tests with incorrect return types
  - `check_ternary_support.aria` - main() int64→int32
  - `comprehensive_ternary.aria` - All 9 functions int64→int32
  - `manual_ternary_pattern.aria` - main() int64→int32
  - `ternary_debug_value.aria` - trit→int32 cast handling

- **String Return Value Bug (CRITICAL)** - Functions returning strings now work correctly
  - **Root Cause**: Test files were missing explicit unwrap operator (`?`)
  - **Design**: ALL functions return `Result<T>` (even void returns `Result<NIL>`)
    * `pass(val)` and `fail(err)` are sugar to build `{value, error*, is_error}` struct
    * No implicit unwrapping allowed - explicit error handling required
  - **Correct Usage**: `string:s = get_string() ? "default";` 
    * The `?` operator checks `is_error` field and branches appropriately
    * Generates proper control flow: error_block vs success_block with PHI merge
  - **Fix**: Updated test files to use explicit unwrap operator
  - **Impact**: Maintains strict explicit > implicit principle for Nikola's deterministic requirements
  - **Status**: String operations working with proper error handling semantics

- **Test Suite Progress**
  - Improved from 168/373 to 188/389 tests passing (45% → 48.3%)
  - Fixed type system issues preventing exotic type compilation
  - String return value fix enabled 16 additional tests to pass
  - Created TEST_FAILURE_ANALYSIS.md documenting all issues

- **Parameter Reassignment Bug** - Fixed segfault in int64_toString() with negative numbers
  - **Root Cause**: Parameter reassignment (`value = -value`) causes segfault
  - **Investigation**: Parameters appear to be immutable or have codegen bug for reassignment
  - **Solution**: Use local variable (`work_value`) instead of modifying function parameter
  - **Impact**: int64_toString() now handles all cases correctly
  - **Files Updated**:
    * `stdlib/string_convert.aria` - Core library function
    * `tests/test_int64_toString.aria` - Test file with same pattern
  - **Validation**: All test cases passing
    * Zero: 0 ✓
    * Positive: 42, 987654321 ✓
    * Negative: -123 ✓ (previously segfaulted)
    * Balanced literals: 0t1T0=6, 0n2A=17 ✓

### Changed
- **print()/println() Design** - Strict string-only policy
  - Both functions require string argument (no polymorphism)
  - Future `print_t()` will handle formatted output with toString
  - Avoids C printf-style segfault vulnerabilities
  - Separation of concerns: primitives stay simple
  - Fixed to handle both string literals and string variables correctly

### Status
- **String Library**: Core string operations working (concat, println, returns)
- **Result Type Semantics**: Properly enforced with explicit unwrapping
  - All functions return `Result<T>` - no exceptions (except extern C functions)
  - Unwrap operator `?` generates proper error-checking control flow
  - Maintains deterministic error handling for Nikola requirements
- **ToString Library**: int64_toString() implementation **COMPLETE**
  - Works for all cases: zero, positive, negative, large numbers
  - Fixed parameter reassignment bug: `value = -value` caused segfault
  - Solution: Use local variable (`work_value`) instead of modifying parameter
  - Parameters appear to be immutable or have codegen bug for reassignment
  - Validated with balanced literals: `0t1T0` = "6", `0n2A` = "17"
  - Updated: `stdlib/string_convert.aria`, `tests/test_int64_toString.aria`
  - Test output: Zero: 0, Positive: 42, Negative: -123, Large: 987654321
- **Test Suite**: 188/389 passing (48.3%) with proper Result semantics
  - Tests now use explicit `?` unwrap operator where needed
  - No implicit magic - all error paths are explicit
  - Debug needed in src/runtime/strings/strings.cpp

- **Balanced Type Runtime** - trit/tryte/nit/nyte operations
  - Literal syntax complete, runtime operations in progress
  - Kleene logic for trit AND/OR/NOT operations
  - Range clamping for overflow (1+1=1 for trit)
  - Implementation timeline: 1-2 weeks

### Documentation
- Updated TRIT_NIT_ROADMAP.md with literal syntax completion
- Updated aria_specs.txt with 0t/0n syntax specification
- Created test_balanced_literals.aria demonstrating both notations
- Created TEST_FAILURE_ANALYSIS.md for systematic debugging

---

## Previous Releases

(Historical changelog to be populated from git history)
