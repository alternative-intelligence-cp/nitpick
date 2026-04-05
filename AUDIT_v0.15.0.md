# AUDIT — Aria v0.15.0 (Self-Hosting Foundation)

**Date**: 2026-04-05
**Branch**: dev-0.15.x
**Commit**: 26050ec

## Summary

v0.15.0 ports 5 isolated C++ compiler modules to Aria via the C-bridge
shim pattern (extern "C" bridge + Aria stdlib module + test file).

## Ports Completed

| # | Module                | C++ Lines | Shim Lines | Aria Lines | Test Cases | Status |
|---|----------------------|-----------|------------|------------|------------|--------|
| 1 | Visibility Checker   | 150       | 236        | 201        | 12/12 PASS | ✅     |
| 2 | Async Analyzer       | 204       | 84         | 72         | 12/12 PASS | ✅     |
| 3 | Diagnostics Engine   | 206       | 233        | 123        | 12/12 PASS | ✅     |
| 4 | Module Table         | 364       | 267        | 168        | 12/12 PASS | ✅     |
| 5 | Definite Assignment  | 465       | 225        | 85         | 12/12 PASS | ✅     |

**Totals**: 1,389 C++ lines covered → 1,045 shim + 649 Aria + 60 tests

## Files Added (16 files, 3,070 lines)

### C Bridge Shims (src/runtime/sema/)
- visibility_shim.cpp (236 lines)
- async_shim.cpp (84 lines)
- diagnostics_shim.cpp (233 lines)
- module_table_shim.cpp (267 lines)
- definite_assignment_shim.cpp (225 lines)

### Aria Stdlib Modules (stdlib/)
- visibility_checker.aria (201 lines)
- async_analyzer.aria (72 lines)
- diagnostics.aria (123 lines)
- module_table.aria (168 lines)
- definite_assignment.aria (85 lines)

### Test Files (tests/stdlib/)
- test_visibility_checker.aria (245 lines) — 12 tests
- test_async_analyzer.aria (244 lines) — 12 tests
- test_diagnostics.aria (266 lines) — 12 tests
- test_module_table.aria (285 lines) — 12 tests
- test_definite_assignment.aria (331 lines) — 12 tests

### Modified
- CMakeLists.txt (+5 shim source entries)

## Regression

- CTest: 4/4 pass (test_runner_minimal, integration, gpu, gpu_ptx)
- Self-hosting tests: 60/60 pass
- Stdlib pre-existing: test_atomic_simple PASS; test_atomic_int32, test_wavemech pre-existing failures unchanged
- No regressions introduced

## Architecture Pattern

Each port follows the same pattern:
1. **C Bridge Shim** (extern "C"): Opaque state management, data storage,
   string returns via thread_local buffer (`const char*` for FFI ABI)
2. **Aria Stdlib Module**: Higher-level logic (formatting, validation,
   orchestration) calling into shim externs
3. **Test File**: 12 self-contained test cases, compiled standalone with
   `ariac`, exits 0 on success

### Key ABI Rules Confirmed
- String params: compiler extracts `.data` → C receives `const char*`
- String returns: C must return `const char*`; compiler auto-wraps via
  strlen + gc_alloc + memcpy into AriaString
- Wild pointers: borrow checker tracks via `_free`/`_close`/`_destroy`/`_release` suffix
- `pub func` returns wrapped in Result<T>; callers use `raw` to unwrap
- Reserved words: `mod`, `ok`, `is`, `raw`, `stream`, `end`, etc.

## Self-Hosting Status (cumulative)

| Phase   | What                                        | Lines |
|---------|---------------------------------------------|-------|
| v0.5.1  | lexer.aria                                  | 1,210 |
| v0.5.2  | parser.aria                                 | 1,910 |
| v0.5.3  | type_checker.aria                           | 1,809 |
| v0.5.5  | const_evaluator + borrow + safety + exhaust | 203   |
| v0.15.0 | 5 sema modules (this release)               | 649   |
| **Total** | **Aria self-hosted code**                 | **5,781** |
