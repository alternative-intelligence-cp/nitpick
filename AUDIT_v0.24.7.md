# Nitpick v0.24.7 — Cycle Audit

**Date:** April 21, 2026
**Branch:** `dev-0.24.x` → merged to `main`
**Tag:** `v0.24.7`
**Auditor:** Copilot / randy

---

## Cycle Summary

The v0.24.x cycle delivered the complete **comptime / CTFE** subsystem,
from initial triage through type reflection, comptime functions, generics,
limit-rule integration, K semantics, and the `guide/comptime/` docs.

**8 releases:** v0.24.0 (triage) through v0.24.7 (reflection + K + docs + closeout).

---

## Per-Slice Summary

| Version | Description | CTest |
|---------|-------------|-------|
| v0.24.0 | Comptime triage; COMPTIME-001/002 smoke tests + basic `comptime(expr)` const folding | 39/39 |
| v0.24.1 | `comptime { ... }` blocks + mutable locals (COMPTIME-003) | 40/40 |
| v0.24.2 | `comptime func:` declarations + memoization (COMPTIME-004) | 41/41 |
| v0.24.3 | Comptime intrinsics: `@sizeof`, `@alignof`, `@offsetof`, `@len` (COMPTIME-005) | 42/42 |
| v0.24.4 | Macro/comptime cooperation + recursion budget (COMPTIME-006/007) | 43/43 |
| v0.24.5 | `T: type` parameters + type-level dispatch (COMPTIME-008/009/010) | 44/44 |
| v0.24.6 | `limit<Rules>` short-circuit + `assert_static` (COMPTIME-011/012) | 45/45 |
| v0.24.7 | Struct field reflection (COMPTIME-013) + K semantics (COMPTIME-014) + docs + audit | 46/46 |

---

## v0.24.7 Slice Details

### Phase 1 — COMPTIME-013: Struct Field Reflection

- `getTypeInfo()` in `src/frontend/sema/const_evaluator.cpp` now emits a
  per-field `fields` sub-map for struct types: each field name maps to a
  comptime struct `{name, type_name, bit_width, alignment, offset}`.
- `bit_width` is set for primitive field types (int8/16/32/64, uint8/16/32/64,
  flt32/64, bool, string).
- New `@fieldType(T, "name")` intrinsic shorthand returns a comptime string
  (the field's type name) — wired in parser, type-checker (returns `string`),
  and const evaluator (drills into `.fields.<name>.type_name`).
- `inferComptimeExpr()` refactored to **evaluate-first**: comptime intrinsic
  chains like `@typeInfo(T).fields.x.type_name` no longer trip the
  inference path that bombed on them previously.

### Phase 1 — String Comptime Codegen Fix

- **Bug:** `string:tx = comptime("flt64")` segfaulted at runtime because the
  `COMPTIME_EXPR` codegen branch in `ir_generator.cpp` emitted a raw `i8*`
  global string instead of an `AriaString` struct `{i8*, i64}`.
- **Fix:** mirrored the string-literal codegen pattern from
  `codegen_expr.cpp` — allocate the `struct.NpkString` global with both
  the data pointer and the length. Comptime string values now interoperate
  cleanly with `println()`, string equality, and string parameters.

### Phase 2 — COMPTIME-014: K Semantics

- `k-semantics/aria.k`:
  - Syntax: added `comptime(Exp)`, `comptime Block`, and three
    `comptime func:` `FuncDecl` variants (0, 1, 2 params).
  - Configuration: added `<comptime-env>` cell.
  - Rules:
    - `comptime(V:Val) => V` — value passthrough
    - `comptime B:Block => B` — block passthrough
    - `loadFuncs(comptime func: F = T(...) {...};)` — registers `F` in
      both `<functions>` (callable) and `<comptime-env>` (with arity tag).
- 3 new K core tests:
  - `140_comptime_value_passthrough.aria` — `comptime(3 + 4)` → 7
  - `141_comptime_block_passthrough.aria` — `comptime { ... exit 5; }`
  - `142_comptime_func_decl.aria` — `comptime func:dbl` callable from `main`

### Phase 3 — `aria-docs/guide/comptime/` (9 chapters)

- `README.md` — overview, three forms, comptime-vs-macros table
- `basic.md` — `comptime(expr)`, `comptime { ... }`, `comptime func:`
- `ctfe.md` — pipeline, budget, recursion, memoization, mutable locals
- `intrinsics.md` — `@sizeof`, `@alignof`, `@offsetof`, `@len`, `@typeInfo`,
  `@fieldType` reference + struct-field reflection table
- `generics.md` — `T: type` parameters and type-level dispatch
- `macros.md` — comptime ↔ macro interaction patterns
- `limits.md` — `limit<Rules>` short-circuit + `assert_static`
- `limitations.md` — full disallowed-operations table (I/O, GC, extern, ...)
- `debug.md` — comptime diagnostics anatomy + common errors

### Phase 4 — Regression Sweep

| Suite | Count |
|-------|-------|
| CTest | **46/46** |
| K core | **142/142** |
| K proofs | **10/10** |
| v0.24.7 regression tests (bug171, bug172) | **2/2** |

No regressions. All previously-passing comptime, macro, borrow, pin,
async, and SSE tests continue to pass.

---

## COMPTIME.md Statistics

All 14 COMPTIME items resolved across the v0.24.x cycle.

| Item | Status | Slice |
|------|--------|-------|
| COMPTIME-001 | ✅ Triage / smoke test | v0.24.0 |
| COMPTIME-002 | ✅ Basic `comptime(expr)` const folding | v0.24.0 |
| COMPTIME-003 | ✅ `comptime { ... }` blocks + locals | v0.24.1 |
| COMPTIME-004 | ✅ `comptime func:` + memoization | v0.24.2 |
| COMPTIME-005 | ✅ `@sizeof`/`@alignof`/`@offsetof`/`@len` | v0.24.3 |
| COMPTIME-006 | ✅ Macro ↔ comptime cooperation | v0.24.4 |
| COMPTIME-007 | ✅ Recursion budget enforcement | v0.24.4 |
| COMPTIME-008 | ✅ `T: type` parameters | v0.24.5 |
| COMPTIME-009 | ✅ Type-level dispatch (`@typeInfo.kind`/`.name`) | v0.24.5 |
| COMPTIME-010 | ✅ Comptime regression suite (bug146–170) | v0.24.5 |
| COMPTIME-011 | ✅ `limit<Rules>` short-circuit at comptime | v0.24.6 |
| COMPTIME-012 | ✅ `assert_static(cond, msg)` | v0.24.6 |
| COMPTIME-013 | ✅ Struct field reflection (`@typeInfo.fields`, `@fieldType`) | v0.24.7 |
| COMPTIME-014 | ✅ K semantics for comptime | v0.24.7 |

---

## Known Limitations Carried Forward

- **No runtime polymorphism via comptime.** `T: type` parameters are
  comptime-only and erased before codegen.
- **No I/O or GC at comptime.** Determinism guarantee — see
  `guide/comptime/limitations.md`.
- **CTFE budget is a fixed compile-time constant.** A `--comptime-budget`
  flag is planned but not yet exposed.
- **`--trace-comptime` flag** is documented in `debug.md` as planned but
  not yet implemented.
- **Comptime arrays of struct values** evaluate correctly but cannot yet
  be embedded directly into IR globals — large tables should be built
  inside a `comptime { ... }` returning the final scalar / small array.

---

## Test Counts (Final)

| Suite | Count | Notes |
|-------|-------|-------|
| CTest (fast, excludes K core) | 46/46 | includes `bug_tests_v0247` |
| K core | 142/142 | 3 new comptime tests (140–142) |
| K proofs | 10/10 | unchanged |
| Total regression bug tests | bug001 – bug172 | all green |

---

## Closeout Actions

- [x] AUDIT_v0.24.7.md (this file)
- [x] KNOWN_ISSUES.md updated → v0.24.7
- [x] META/NITPICK_COMPTIME/COMPTIME.md → all 14 items ✅
- [x] META/ARIA/ROADMAP/0.24/RELEASE_0.24.7.md → COMPLETE
- [x] META/ARIA/ROADMAP/MASTER_ROADMAP.md → v0.24.x ✅, v0.25.x NEXT
- [x] aria-docs/guide/comptime/ committed (9 chapters)
- [x] Tag v0.24.7 on dev-0.24.x, merged to main
