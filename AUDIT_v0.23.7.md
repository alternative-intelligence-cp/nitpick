# Nitpick v0.23.7 — Cycle Audit

**Date:** June 11, 2026  
**Branch:** `dev-0.23.x` → merged to `main`  
**Tag:** `v0.23.7`  
**Auditor:** Copilot / randy

---

## Cycle Summary

The v0.23.x cycle delivered the complete macro system from initial triage
through built-in macros, debug tooling, and formal specification.

**8 releases:** v0.23.0 (triage) through v0.23.7 (K semantics + debug flag + docs)

---

## Per-Slice Summary

| Version | Commit | Description | CTest |
|---------|--------|-------------|-------|
| v0.23.0 | `7313933` | Macro triage — MACRO-001 smoke test in CTest | 32/32 |
| v0.23.1 | `41080bb` | Basic macro pipeline, regression tests (MACRO-001, MACRO-002) | 33/33 |
| v0.23.2 | `9282964` | Macro hygiene fix — gensym/cloneAST (MACRO-003) | 34/34 |
| v0.23.3 | `7686fa7` | Recursive macros + depth guard (MACRO-004) | 35/35 |
| v0.23.4 | `e09b65f` | Variadic macros `..?param` + arity checks (MACRO-005) | 36/36 |
| v0.23.5 | `d31e0fa` | Statement-position macros (MACRO-006) | 37/37 |
| v0.23.6 | `affd927` | Built-in macros: assert!, todo!, unreachable!, cfg! (MACRO-008..011) | 38/38 |
| v0.23.6+ | `ba66d8a` | **Bug fix:** restore missing `case RETURN:` in `checkStatement()` | 38/38 |
| v0.23.7 | (this) | `--expand-macros` flag (MACRO-012), K semantics (MACRO-013), docs, tests | 39/39 |

---

## Bug Fixed During Cycle (v0.23.6)

**Bug:** `case ASTNode::NodeType::RETURN:` was accidentally dropped from
`checkStatement()` when the `MACRO_INVOCATION` case was inserted in v0.23.5.
The `checkReturnStmt()` function was dead code; `return` statements at
statement position hit the ICE `default:` branch instead.

**Root cause:** The case label sat directly above the insertion point of the
`MACRO_INVOCATION` block; the diff removed it without detection because
Nitpick primarily uses `pass` rather than `return`, so no existing test
exercised a bare `return` statement.

**Fix:** Restored in commit `ba66d8a` (between v0.23.6 and v0.23.7).

---

## MACROS.md Statistics

| Item | Status |
|------|--------|
| MACRO-001 | ✅ Macro smoke test + basic registration |
| MACRO-002 | ✅ Regression test suite (basic pipeline) |
| MACRO-003 | ✅ Hygiene (gensym/cloneAST) |
| MACRO-004 | ✅ Recursive expansion + depth guard |
| MACRO-005 | ✅ Variadic macros (`..?params`) |
| MACRO-006 | ✅ Statement-position macro invocations |
| MACRO-007 | ⏭ Deferred — code-gen macro (complex body shim) |
| MACRO-008 | ✅ `assert!` built-in |
| MACRO-009 | ✅ `todo!` built-in |
| MACRO-010 | ✅ `unreachable!` built-in |
| MACRO-011 | ✅ `cfg!` platform conditional |
| MACRO-012 | ✅ `--expand-macros` debug flag |
| MACRO-013 | ✅ K semantics for macros (loadMacros, macroExpand) |

---

## Test Counts

| Suite | Count |
|-------|-------|
| CTest (fast, excludes K core) | 37/37 |
| K core tests | 139/139 |
| K proofs | (pass via ctest) |
| v0.23.7 regression tests (bug142–bug145) | 5/5 |

---

## New Files Added This Cycle

### Compiler
- `tests/bugs/run_bug_tests_0237.sh` — v0.23.7 regression tests (MACRO-012)
- `tests/CMakeLists.txt` — `bug_tests_v0237` CTest entry added

### K Semantics
- `k-semantics/aria.k` — `<macros>` cell, `MacroDecl` syntax, `loadMacros` +
  `macroExpand` rules added; `Pgm` updated to include `MacroDecls`;
  `loadMacros` called from the main program envelope rule

### aria-docs
- `guide/macros/README.md` — macro overview and comparison table
- `guide/macros/basic.md` — declaration and invocation syntax
- `guide/macros/hygiene.md` — gensym and variable isolation
- `guide/macros/recursive.md` — recursive macros and depth guard
- `guide/macros/variadic.md` — rest parameters (`..?name`)
- `guide/macros/code_gen.md` — statement-position and code-generating macros
- `guide/macros/builtins.md` — assert!, todo!, unreachable!, cfg!
- `guide/macros/debug.md` — `--expand-macros` usage and output format

---

## Known Issues Updated

See `KNOWN_ISSUES.md` for MACRO-007 deferral rationale.

---

## Next Cycle

v0.24.x planning is pending. See `META/NITPICK/ROADMAP/MASTER_ROADMAP.md`.
