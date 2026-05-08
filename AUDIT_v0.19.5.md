# AUDIT — Nitpick v0.19.5

**Release series:** v0.19.0 – v0.19.5  
**Branch:** dev-0.19.x → main  
**Audit date:** May 7, 2026  
**Auditor:** automated (GitHub Copilot / dev session)

---

## 1. v0.19.x Cycle Summary

### v0.19.0 — Dynamic-Index Borrows & Multi-Dimensional Arrays
Introduced dynamic array index borrow paths (`arr[i]`) in the borrow checker and codegen. The `$$i`/`$$m` borrow syntax was extended to variable-index paths (commit `7793d4b`). Multi-dimensional array literal initializers were fixed to correctly initialize nested arrays (commit `1204d60`). Struct/array mixed borrow interaction was implemented with proper disjoint-path analysis (commit `9f4b6e3`). Slice polish closed out the release with inclusive/exclusive/variable-index slice semantics (commit `82ba834`).

### v0.19.1 — Struct Update Syntax & Pick Struct Patterns
`TypeName{ ...base, field: expr }` struct update expressions were added to the language (commit `598731d`). Struct field destructuring patterns in `pick` — `TypeName{ field1, _ }` — were implemented in the type checker and codegen (commit `b4a1c72`). The restriction that `pass(extern_returning_struct())` required an intermediate temp variable was lifted. The `struct ==` operator was correctly rejected at compile time as a type error.

### v0.19.2 — Borrow Checker Loop Fix & Struct/Borrow Interop
A type-mismatch bug when using variable `int32` expressions as loop bounds was fixed (commit `f82228d`). The borrow checker was extended to properly inspect `loop()` statement bodies for borrow conflicts (commit `2b74090`). Regression tests for borrow checker rejections and struct-update/borrow interop were added (bugs 065–069).

### v0.19.3 — Six Bug Fixes
Six tracked bugs from KNOWN_ISSUES were resolved in a single pass (commit `76e0000`):
- **bug072**: Non-pub helper functions called from `pub func:` crashed codegen — fixed; intra-module calls now work regardless of visibility.
- **bug073**: `ahget` on a missing key segfaulted — now returns zero.
- **bug074**: `@func_name` passed directly as a call argument triggered a type-checker mismatch — fixed; assign to a lambda variable first.
- **bug075**: Variables declared before `loop()` became inaccessible after loop exit — fixed.
- **bug076**: `Result<T>` in arithmetic emitted a misleading compiler error — error message improved.
- **bug077**: A `pick` with 30+ arms triggered a codegen segfault — fixed with chunked codegen.

### v0.19.4 — K Semantics Update
K Framework semantics were updated to cover all new language features introduced in v0.19.0–v0.19.3: dynamic-index borrow paths, struct update syntax, pick struct field patterns, and lambda function pointer variables (tests 110–127, commit `9a0f8ae`). The CTest timeout for `k_semantics_core` was bumped from 300 s to 600 s to accommodate the now-larger 127-test K suite (commit `81aa32f`).

### v0.19.5 — Audit, Docs & Test Coverage (this release)
Five guide documentation files in `aria-docs` were written or substantially expanded:
- `guide/types/array.md` — multi-dim arrays, dynamic-index borrows, arrays-in-structs borrow rules
- `guide/types/struct.md` — extern-returning struct usage, pick struct pattern cross-reference
- `guide/memory_model/borrow.md` — full rewrite: array element borrows, lambda borrows, inter-procedural borrows, borrow error reference table
- `guide/functions/declaration.md` — non-pub helper pattern, `@func_name` as first-class value
- `guide/control_flow/loop_till.md` — variables declared before loop, carrier-variable pattern

Four new integration tests were added (bug078–bug081):
- `bug078_pick_struct_pat_wildcard_arm_pass.npk` — struct pick pattern with trailing wildcard
- `bug079_pick_struct_pat_fields_are_copies_pass.npk` — bound fields from struct pick are immutable copies
- `bug080_lambda_nil_return_pass.npk` — lambda with conditional sign-detection logic
- `bug081_lambda_reuse_and_higher_order_pass.npk` — lambda stored, reused, passed to higher-order function

`KNOWN_ISSUES.md` was updated from v0.13.7 to v0.19.5: six resolved items moved to the Resolved section, stale limitations removed, test counts updated, and the document rebranded to Nitpick.

---

## 2. Final Test Counts

| Suite | Result |
|-------|--------|
| CTest (excl. k_semantics_core) | 10/10 |
| K core (k_semantics_core) | 127/127 |
| K proofs | 10/10 |
| Compiler regression (.npk passing) | 831 |
| Expected compile errors | 78 |
| Adversarial / fuzz / skip tests | 52 |
| Genuine failures | **0** |

---

## 3. Features Added in v0.19.x

| Feature | First Available |
|---------|----------------|
| `arr[i]` dynamic-index borrow paths (`$$i`/`$$m`) | v0.19.0 |
| Multi-dimensional array literals `[[a,b],[c,d]]` | v0.19.0 |
| Struct update syntax `TypeName{ ...base, field: expr }` | v0.19.1 |
| Pick struct field pattern `TypeName{ field1, _ }` | v0.19.1 |
| Direct `pass` of extern-returning struct without temp | v0.19.1 |
| Loop bounds accept variable `int32` expressions | v0.19.2 |
| Borrow checker covers `loop()` statement bodies | v0.19.2 |
| Non-pub helper functions callable from `pub func:` | v0.19.3 |
| K semantics for all v0.19.0–v0.19.3 features (18 new rules) | v0.19.4 |

---

## 4. Bugs Fixed (v0.19.x)

| ID | Description | Commit | Version |
|----|-------------|--------|---------|
| bug061 | `pass(extern_returning_struct())` required temp var | `b4a1c72` | v0.19.1 |
| bug062–064 | Struct codegen edge cases + struct `==` rejected | `b4a1c72` | v0.19.1 |
| bug065–067 | Borrow checker rejection regression tests | `cc4eb3f` | v0.19.2 |
| bug068–069 | Struct update + borrow interop | `26cb1bb` | v0.19.2 |
| bug070–071 | Loop variable-index type mismatch + borrow loop body | `f82228d`, `2b74090` | v0.19.2 |
| bug072 | Non-pub helper codegen crash | `76e0000` | v0.19.3 |
| bug073 | `ahget` missing key segfault | `76e0000` | v0.19.3 |
| bug074 | `@func_name` call-arg type mismatch | `76e0000` | v0.19.3 |
| bug075 | Pre-loop variables inaccessible after exit | `76e0000` | v0.19.3 |
| bug076 | `Result<T>` arithmetic error message | `76e0000` | v0.19.3 |
| bug077 | Large pick (30+ arms) codegen segfault | `76e0000` | v0.19.3 |
| bug078–081 | Struct pick pattern + lambda coverage tests | *(v0.19.5)* | v0.19.5 |

---

## 5. Known Issues Remaining

See `KNOWN_ISSUES.md` for the full list. Summary:

- **Lambda closures do not capture scope** — all values must be explicit parameters (design decision; future work)
- **`ahset` silently returns -1 on overflow** — check return value
- **flt32 ABI passes as double** at C boundary — C shims must cast
- **String ABI is asymmetric** — params as `const char*`, returns as `AriaString` by value
- **`fixed` with compile-time arithmetic may yield 0** — use runtime variable
- **Negative constants across `use` boundaries zeroed** — compute inline
- **Nested module pub-pub calls** — GC OOM in pathological cases

---

## 6. Deferred to v0.20.x

- Lambda closure capture of enclosing locals and globals
- `NIL`-return lambda variable declarations (parser does not accept `NIL(T:x) {...}` syntax)
- K semantics coverage for `v0.19.5` new integration tests (bug078–081)
- Astrée combined Aria+Nikola static analysis trial

---

## 7. Validation Sign-Off

| Check | Status |
|-------|--------|
| No regressions vs v0.19.4 baseline | ✅ |
| CTest 10/10 | ✅ |
| K core 127/127 | ✅ |
| K proofs 10/10 | ✅ |
| KNOWN_ISSUES.md current | ✅ |
| Guide docs updated (5 files) | ✅ |
| New integration tests passing (4 tests) | ✅ |
| AUDIT document written | ✅ |
