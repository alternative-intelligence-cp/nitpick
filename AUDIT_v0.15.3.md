# AUDIT — Aria v0.15.3 (Final Self-Hosting Census)

**Date**: 2026-04-05
**Branch**: dev-0.15.x
**Previous**: v0.15.2 (Tier 3 Tool Ports)

## Summary

v0.15.3 closes out the v0.15.x self-hosting stretch with a full census
of what is now written in Aria vs what remains in C++, and documents
the rationale for every C++ component that was NOT ported.

No code changes — this is an audit-only release.

---

## Self-Hosting Census: Aria Code

### v0.15.x Ports (12 modules, 3 tiers)

| # | Module                | C++ Original | Shim (C++) | Aria Module | Tests  | Release |
|---|----------------------|-------------|------------|-------------|--------|---------|
| 1 | Visibility Checker   | 150         | 236        | 201         | 245    | v0.15.0 |
| 2 | Async Analyzer       | 204         | 84         | 72          | 244    | v0.15.0 |
| 3 | Diagnostics Engine   | 206         | 233        | 123         | 266    | v0.15.0 |
| 4 | Module Table         | 364         | 267        | 168         | 285    | v0.15.0 |
| 5 | Definite Assignment  | 465         | 225        | 85          | 331    | v0.15.0 |
| 6 | Closure Analyzer     | 263         | 210        | 70          | 271    | v0.15.1 |
| 7 | Warnings System      | 510         | 233        | 62          | 260    | v0.15.1 |
| 8 | Module Resolver      | 341         | 248        | 55          | 235    | v0.15.1 |
| 9 | Usage Stats          | 286         | 247        | 57          | 277    | v0.15.1 |
| 10| Doc Generator        | 1,309       | 755        | 109         | 267    | v0.15.2 |
| 11| Package Manager      | 1,078       | 524        | 80          | 257    | v0.15.2 |
| 12| Project Config       | 368         | 284        | 92          | 255    | v0.15.2 |
|   | **Totals**           | **5,544**   | **3,546**  | **1,174**   | **3,193** |      |

### Pre-v0.15.x Self-Hosted Modules (Phase 5.x)

| Module             | Aria Lines | C++ Helper    | Helper Lines | Phase  |
|--------------------|-----------|---------------|-------------|--------|
| lexer.aria         | 1,210     | (lex_helpers) | —           | v0.5.1 |
| parser.aria        | 1,910     | (ast_helpers) | —           | v0.5.2 |
| type_checker.aria  | 1,809     | sema_helpers  | 1,306       | v0.5.3 |
| const_evaluator    | 68        | pass_helpers  | 951         | v0.5.5 |
| borrow_checker     | 49        | pass_helpers  | (shared)    | v0.5.5 |
| safety_checker     | 44        | pass_helpers  | (shared)    | v0.5.5 |
| exhaustiveness     | 42        | pass_helpers  | (shared)    | v0.5.5 |
| **Totals**         | **5,132** |               | **2,257**   |        |

### Other Aria stdlib (non-ported, general purpose)

53 stdlib .aria files comprising ~8,708 lines of standard library code
(arena, atomic, channel, condvar, io, json, math, mutex, net, pipe,
process, signal, sys, toml, etc.)

### Grand Totals — Aria Code

| Category                      | Lines  |
|-------------------------------|--------|
| Self-hosted ports (v0.15.x)   | 1,174  |
| Self-hosted tests (v0.15.x)   | 3,193  |
| Pre-5.x frontend ports        | 5,132  |
| General stdlib                 | 8,708  |
| Other test .aria files         | 1,492  |
| **Total Aria**                 | **19,699** |

---

## Self-Hosting Census: C++ Code

### Total C++ in src/

| Directory                 | Lines   | Description                                    |
|--------------------------|---------|------------------------------------------------|
| src/frontend/sema/       | 24,123  | Type checker, borrow checker, symbol table, etc |
| src/frontend/parser/     | 5,969   | Parser                                          |
| src/frontend/lexer/      | 1,982   | Lexer                                           |
| src/frontend/preprocessor/| 1,546  | Preprocessor                                    |
| src/frontend/ (other)    | 716     | diagnostics.cpp, warnings.cpp                   |
| src/backend/ir/          | 29,884  | IR generator + codegen                          |
| src/backend/runtime/     | 3,855   | TFP ops, ternary ops, tensor ops                |
| src/analysis/            | 6,082   | Z3 verifier (5,231) + range analysis (851)      |
| src/runtime/sema/        | 5,803   | Shims (3,546) + helpers (2,257)                 |
| src/runtime/gc/          | 1,583   | Garbage collector                               |
| src/runtime/allocators/  | 2,508   | Wild/WildX/arena/pool/slab allocators            |
| src/runtime/assembler/   | 3,105   | JIT assembler (x86-64)                          |
| src/runtime/async/       | 2,933   | io_uring, coroutines                            |
| src/runtime/thread/      | 2,291   | OS threading primitives                         |
| src/runtime/telemetry/   | 1,021   | Telemetry writer                                |
| src/runtime/result/      | 342     | Result type implementation                      |
| src/runtime/ (other)     | ~24,422 | Other runtime support                           |
| src/tools/               | 8,121   | LSP, DAP, debugger, doc-gen, pkg-mgr, config    |
| src/main.cpp             | 6,496   | Compiler driver/orchestrator                    |
| **Total src/ C++**       | **~133,698** |                                             |
| include/ headers         | 36,876  | Header files                                    |
| **Grand Total C++**      | **~170,574** |                                             |

### Self-Hosting Ratio

| Metric                              | Value        |
|-------------------------------------|-------------|
| Aria code (all .aria in stdlib+tests) | 19,699 lines |
| C++ code (src/ + include/)           | ~170,574 lines |
| Ratio (Aria / total)                | ~10.4%       |
| Self-hosted compiler modules (Aria)  | 6,306 lines  |
| Self-hosted compiler modules (C++ originals) | 5,544 lines |
| C++ shims backing Aria ports         | 5,803 lines  |
| Self-hosting test coverage           | 144 tests (12 suites × 12 tests) |

---

## What Remains in C++ and Why

### MUST STAY — Deep LLVM/Z3/Codegen Coupling

These modules have intimate knowledge of LLVM IR types, Z3 SMT solver
APIs, or direct machine code emission. Porting them would require Aria
bindings for LLVM and Z3, which don't exist and aren't planned.

| Module                    | Lines  | Why It Stays                                   |
|--------------------------|--------|------------------------------------------------|
| ir_generator.cpp         | 12,250 | Heart of codegen — LLVM IR emission throughout  |
| codegen_expr_call.cpp    | 7,943  | Function call ABI + LLVM intrinsics             |
| codegen_stmt.cpp         | 3,159  | Statement codegen → LLVM BasicBlock/IRBuilder   |
| codegen_expr.cpp         | 2,543  | Expression codegen → LLVM Value/Type            |
| codegen_expr_compound.cpp| 1,865  | Compound expr codegen → LLVM struct/array       |
| codegen_expr_binary.cpp  | 1,059  | Binary ops → LLVM arithmetic intrinsics         |
| z3_verifier.cpp          | 5,231  | Z3 C++ API throughout (expr, solver, model)     |
| range_analysis.cpp       | 851    | Feeds directly into Z3 expressions              |
| **Subtotal**             | **34,901** |                                             |

### MUST STAY — Core Type System & Semantic Analysis

These modules are deeply interconnected with each other and with LLVM
type representations. Bug risk from porting would be catastrophic.

| Module                    | Lines  | Why It Stays                                   |
|--------------------------|--------|------------------------------------------------|
| type_checker_stmts.cpp   | 5,479  | Statement type checking — core correctness      |
| type_checker_call.cpp    | 5,190  | Overload resolution + generic instantiation     |
| borrow_checker.cpp       | 3,644  | Ownership analysis — deep AST/scope coupling    |
| type_checker.cpp         | 1,622  | Type checker driver — coordinates everything    |
| const_evaluator.cpp      | 1,513  | Compile-time eval — LLVM APFloat/APInt          |
| type.cpp                 | 1,317  | Type representation — used by every sema pass   |
| generic_resolver.cpp     | 1,224  | Monomorphization — creates LLVM type instances   |
| exhaustiveness.cpp       | 479    | Pattern matching completeness — feeds codegen    |
| safety_checker.cpp       | 345    | Safety analysis — feeds Z3 verifier             |
| symbol_table.cpp         | 247    | Core data structure for all semantic passes     |
| **Subtotal**             | **21,060** |                                             |

### MUST STAY — Frontend Pipeline

| Module                    | Lines  | Why It Stays                                   |
|--------------------------|--------|------------------------------------------------|
| parser.cpp               | 5,969  | 6K lines, deep AST construction coupling        |
| lexer.cpp                | 1,982  | Token stream consumed by parser (tight coupling) |
| preprocessor (all)       | 1,546  | Macro expansion before parsing                  |
| module_loader.cpp        | 270    | Orchestrates lex→parse for imports (glue)       |
| main.cpp                 | 6,496  | Compiler driver — orchestrates entire pipeline  |
| **Subtotal**             | **16,263** |                                             |

### MUST STAY — Performance-Critical Runtime

These are in C/C++ for raw performance. They manage memory at the
register/cache-line level, emit machine code, or use OS-specific APIs.

| Module                    | Lines  | Why It Stays                                   |
|--------------------------|--------|------------------------------------------------|
| GC (gc/)                 | 1,583  | Garbage collector — needs predictable perf      |
| Allocators (allocators/) | 2,508  | Wild/WildX/arena/pool/slab — pointer math       |
| JIT assembler            | 3,105  | Direct x86-64 machine code emission             |
| Async (async/)           | 2,933  | io_uring + coroutines — OS-specific C APIs      |
| Threading (thread/)      | 2,291  | pthread/futex primitives                        |
| TFP/ternary/tensor ops   | 3,855  | Ternary arithmetic — bit-level operations       |
| **Subtotal**             | **16,275** |                                             |

### COULD PORT LATER — Deferred (Tier 4+)

| Module                    | Lines  | Why Deferred                                   |
|--------------------------|--------|------------------------------------------------|
| Result type (result.cpp) | 342    | Fundamental type — bug would cascade everywhere |
| Telemetry (telemetry.cpp)| 735    | Threading + async buffering needed in Aria first|
| LSP server (lsp/)        | 1,744  | Valuable but large; needs Aria JSON-RPC + async |
| DAP/Debugger (dap+dbg)   | 3,467  | Complex async protocol; needs Aria DAP bindings |
| **Subtotal**             | **6,288** |                                              |

### ALREADY PORTED (v0.15.x) — C++ Originals Still Compiled

The original C++ files for ported modules remain in the build (not deleted).
Both the shim and original exist — the shim is what Aria stdlib calls.
These can be removed in a future cleanup release (v0.16.x or later).

| Module                    | Lines  | Status                                         |
|--------------------------|--------|------------------------------------------------|
| visibility_checker.cpp   | 150    | Ported — shim active, original still compiled   |
| async_analyzer.cpp       | 204    | Ported — shim active, original still compiled   |
| diagnostics.cpp          | 206    | Ported — shim active, original still compiled   |
| module_table.cpp         | 364    | Ported — shim active, original still compiled   |
| definite_assignment.cpp  | 465    | Ported — shim active, original still compiled   |
| closure_analyzer.cpp     | 263    | Ported — shim active, original still compiled   |
| warnings.cpp             | 510    | Ported — shim active, original still compiled   |
| module_resolver.cpp      | 341    | Ported — shim active, original still compiled   |
| usage_stats.cpp          | 286    | Ported — shim active, original still compiled   |
| doc/ (parser+generator)  | 1,309  | Ported — shim active, originals still compiled  |
| pkg/ (installer)         | 1,078  | Ported — shim active, original still compiled   |
| project_config.cpp       | 368    | Ported — shim active, original still compiled   |
| **Subtotal**             | **5,544** |                                              |

---

## Regression Status

| Suite               | Result          |
|---------------------|-----------------|
| CTest               | 4/4 pass        |
| Self-hosting Tier 1 | 60/60 pass (5 suites × 12 tests) |
| Self-hosting Tier 2 | 48/48 pass (4 suites × 12 tests) |
| Self-hosting Tier 3 | 36/36 pass (3 suites × 12 tests) |
| **Total**           | **144/144 self-hosting + 4/4 CTest** |

## Fuzzer Status

- Ongoing fuzzer campaign: v0.14.5 binary, 800+ tests, 0 crashes, 0 runtime errors
- 19 frozen compiler snapshots (v0.2.46 through v0.14.5)
- No new crash signatures from v0.15.x changes (shims are additive, not replacing)

---

## v0.15.x Series Summary

| Release  | Theme                      | Ports | New Tests | Cumulative Tests |
|----------|---------------------------|-------|-----------|-----------------|
| v0.15.0  | Tier 1 Safe Ports          | 5     | 60        | 60              |
| v0.15.1  | Tier 2 Medium Ports        | 4     | 48        | 108             |
| v0.15.2  | Tier 3 Tool Ports          | 3     | 36        | 144             |
| v0.15.3  | Final Audit & Census       | 0     | 0         | 144             |

**Total v0.15.x Accomplishments:**
- 12 C++ modules ported to Aria via C-bridge shim pattern
- 5,544 lines of C++ functionality covered by Aria + shim layer
- 144 new self-hosting tests across 12 suites
- Zero regressions throughout entire series
- Self-hosting ratio: ~10.4% (Aria / total codebase)
