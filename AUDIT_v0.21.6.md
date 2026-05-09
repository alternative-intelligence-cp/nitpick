# AUDIT_v0.21.6.md — K Semantics & Verification Closure

**Date:** 2026-05-30  
**Branch:** `dev-0.21.x`  
**Base tag:** `v0.21.5` (commit `769feda`)  
**Release:** v0.21.6 — K Semantics & Verification (final 0.21.x release)

---

## Baseline Validation

| Suite | Count | Result |
|-------|-------|--------|
| CTest | 24/24 | ✅ PASS |
| K core semantics | 139/139 | ✅ PASS |
| K proofs | 10/10 | ✅ PASS |
| Bug regression (v0.21.6) | 2/2 | ✅ PASS |
| Open bugs (META/ARIA/BUGS) | 0 | ✅ clean |

All 24 CTest tests pass with no regressions from v0.21.5.

---

## Work Summary — v0.21.6

This release addresses the two AUDIT_v0.21.0 items deferred to v0.21.6:

- **A-009** — K type lattice extended with `tbb8`, `tbb16`, `tbb64`
- **A-010** — K semantics extended with `async func:` and `await` (synchronous model)

It also delivers the `SEMANTIC_GAPS.md` documentation and associated compiler-level
regression tests (bug105, bug106).

---

## A-009 — tbb8 / tbb16 / tbb64 Type Lattice (Resolved)

**Finding (from AUDIT_v0.21.0):** K semantics only formalized `tbb32`. The other
three bounded-integer variants (`tbb8`, `tbb16`, `tbb64`) were absent from aria.k.

**Resolution:** Added to `k-semantics/aria.k`:

| Item | Details |
|------|---------|
| Syntax | `"tbb8"`, `"tbb16"`, `"tbb64"` added to `Type` |
| Val constructors | `TBB8(Int)`, `TBB16(Int)`, `TBB64(Int)` |
| Normalization functions | `normalizeTBB8`, `normalizeTBB16`, `normalizeTBB64` |
| ERR sentinels | tbb8 → -127, tbb16 → -32767, tbb64 → -9223372036854775807 |
| Arithmetic | All four ops (+, -, *, ÷) for each type |
| normalizeValue / isNumericValue dispatch | Updated |

**K tests added:** 128–136 (9 tests)

| Test | Description |
|------|-------------|
| 128_tbb8_basic_pass | Basic tbb8 addition |
| 129_tbb8_min_is_err | ERR sentinel recognition |
| 130_tbb8_overflow_err | Overflow produces ERR |
| 131_tbb16_basic_pass | Basic tbb16 multiplication |
| 132_tbb16_min_is_err | tbb16 ERR sentinel |
| 133_tbb16_overflow_err | tbb16 overflow |
| 134_tbb64_basic_pass | Basic tbb64 subtraction |
| 135_tbb64_min_is_err | tbb64 ERR sentinel |
| 136_tbb8_err_propagation | ERR + tbb8 → ERR |

**Compiler-level regression test:** `tests/bugs/bug105_tbb_variants_pass.npk`
- Exercises tbb8/tbb16/tbb64 arithmetic and ERR sentinel at runtime
- Exit 0 ✅

---

## A-010 — async func: / await K Semantics (Resolved — Synchronous Model)

**Finding (from AUDIT_v0.21.0):** K semantics had no rules for `async func:`
declarations or `await` expressions.

**Resolution:** Added to `k-semantics/aria.k`:

| Item | Details |
|------|---------|
| Syntax | `"async" "func:" Id "=" Type ...` (0, 1, 2 param variants) |
| Exp syntax | `"await" Exp [strict]` |
| loadFuncs rules | 5 rules mapping `async func:` → same storage as sync `func:` |
| await rules | `await Result(_, V, _) => V` and `await V => V [owise]` |

**Model note:** The K model uses the **synchronous model** — `await` completes
immediately by extracting the success value from a `Result`. True coroutine
suspension/resume is not modeled. See `k-semantics/SEMANTIC_GAPS.md` for what
remains unformalized.

**K tests added:** 137–139 (3 tests)

| Test | Description |
|------|-------------|
| 137_async_basic_pass | 0-param async func, await extracts value (exit 42) |
| 138_async_one_param_pass | 1-param async func, await propagates result (exit 7) |
| 139_async_chained_await_pass | Two chained await calls (exit 9) |

**Compiler-level regression test:** `tests/bugs/bug106_async_await_pass.npk`
- Declares async functions and calls via `drop` from main
- Exit 0 ✅

---

## SEMANTIC_GAPS.md

Created `k-semantics/SEMANTIC_GAPS.md` documenting:
- Remaining type lattice gaps (int128, flt32/64 arithmetic, etc.)
- Remaining feature gaps (true coroutine frames, generics, traits, macros)
- Verification tool gaps (KLEE suite coverage, Alive2 integration)
- Prioritization for v0.22.x+

---

## Files Changed

| File | Change |
|------|--------|
| `k-semantics/aria.k` | +97 lines: tbb8/16/64 types + async/await rules |
| `k-semantics/SEMANTIC_GAPS.md` | New: semantic gap documentation |
| `k-semantics/tests/core/128–139_*.aria` | New: 12 K test files |
| `tests/bugs/bug105_tbb_variants_pass.npk` | New: A-009 compiler regression |
| `tests/bugs/bug106_async_await_pass.npk` | New: A-010 compiler regression |
| `tests/bugs/run_bug_tests_0216.sh` | New: v0.21.6 regression runner |
| `tests/CMakeLists.txt` | Added `bug_tests_v0216` CTest entry |

---

## Sign-off Checklist

- [x] A-009 formalized in K (tbb8/16/64 arithmetic + ERR sentinels)
- [x] A-010 formalized in K (async/await, synchronous model)
- [x] K core: 139/139 PASS
- [x] K proofs: 10/10 PASS
- [x] CTest: 24/24 PASS
- [x] bug105: exit 0 ✅
- [x] bug106: exit 0 ✅
- [x] SEMANTIC_GAPS.md written
- [x] No regressions vs v0.21.5

---

## Remaining Open Items

All A-009 and A-010 items are addressed at the K-semantics level. Deeper items
(true coroutine frames, int128 arithmetic, generics/traits in K) are documented in
`k-semantics/SEMANTIC_GAPS.md` and deferred to v0.22.x+.

**v0.21.x is now complete. Next milestone: v0.22.x.**
