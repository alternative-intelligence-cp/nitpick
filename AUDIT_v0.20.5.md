# AUDIT — Nitpick v0.20.5

**Release series:** v0.20.0 – v0.20.5  
**Branch:** dev-0.20.x → main  
**Audit date:** June 2026  
**Auditor:** automated (GitHub Copilot / dev session)

---

## 1. v0.20.x Cycle Summary

### v0.20.0 — Diagnostics & Warning System Polish
The two warning types that had been declared in `WarningConfig::enableAll()` but
never emitted (`UNUSED_FUNCTION`, `EMPTY_BLOCK`) were given real analysis
implementations in `WarningAnalyzer`. Silent `default: return nullptr` /
`default: break` catch-alls in the IR generator and type checker that silently
dropped unhandled AST nodes were replaced with actionable ICE messages. Template
literal error quality was improved with guidance text. The `IMPLICIT_CONVERSION`
no-op stub was cleaned up with an explanatory comment. Commit `a730230`.

### v0.20.1 — Preprocessor Hardening
Two missing preprocessor directives were added to
`src/frontend/preprocessor/preprocessor.cpp`: `%error` (compile-time fatal
diagnostic) and `%warning` (compile-time warning). The `%str()` stringify
operator and `##`-style token concatenation were implemented for `%define` macro
bodies. Preprocessor test coverage was expanded from 2 files to a 16-test suite
covering all directives including `%if`/`%elif`, `%rep`, `%assign`,
`%push`/`%pop`, nesting, and edge cases. Commit `214d930`, tag `v0.20.1`.

### v0.20.2 — Template Literals & Display Trait
The `Display` trait was added to the standard library and wired into the template
literal type checker (`src/frontend/sema/type_checker.cpp:inferTemplateLiteral`).
User-defined structs and enums can now opt into template literal interpolation
(`\`value is &{x}\``) by implementing `Display`. The codegen fallback `throw` for
non-primitive types in template literals was replaced with a proper `Display`
vtable call path. A 23-test suite (`tests/template_literals/`) covers structs,
enums, nested types, and the `flt32` edge case. Commit `16f44a7`, tag `v0.20.2`.

### v0.20.3 — Const Evaluator Step 7
Five documented stubs in `src/frontend/sema/const_evaluator.cpp` were
implemented:
- `compare()` for array operands (was `return false`)
- `compare()` for struct operands (was `return false`)
- `evalDeref()` for struct/pointer const dereference (was stub)
- `evalStore()` for non-integer const stores (was stub with error)
- Memoization cache activation for recursive `comptime` functions

`pick`/`pass`/`VAR_DECL` nodes in comptime bodies and raw unwrap in comptime
expressions were also completed. A `comptime_evaluator_tests` CTest was added.
Commit `8443b5c`, tag `v0.20.3`.

### v0.20.4 — Closure Lifetimes & optional\<T\>
Two type-system completeness items were resolved:

1. **Closure lifetime validation**: `closure_analyzer.cpp:validateLifetimes()`
   was implemented (had been `return true` stub since introduction). Closures
   that capture local variables by reference and escape their scope now produce
   a compile-time error with actionable diagnostics.

2. **`optional<T>` type propagation**: The type checker's safe navigation (`?.`)
   path was corrected to return `optional<field.type>` instead of bare
   `field.type`. Null coalescing (`?|`) and chained `?.` chains were covered.
   `tests/optional/run_optional_tests.sh` added as CTest `optional_safe_nav_tests`.

Commit `74f754c`, tag `v0.20.4`, CTest 17/17.

### v0.20.5 — vec9 Dynamic Indexing Fix & Cycle Closeout (this release)
**Root cause:** `ir_generator.cpp` vec9 dynamic indexing (both read and write
paths) used `builder.CreateICmpEQ(index_value, builder.getInt32(i))` where
`index_value` was i64 (loop integer literals without explicit suffix default to
i64 in the IR generator) and `builder.getInt32(i)` is i32. This triggered
`ICmpInst::AssertOK()` with: *"Both operands to ICmp instruction are not of the
same type!"*

**Fix:** Added an `IntCast` to i32 before the `ICmpEQ` select-loop in both the
read path (line ~12325) and write path (line ~8197) of `ir_generator.cpp`. A
supplementary alloca-spill fallback was also added in
`codegen_expr_compound.cpp` (used if the struct path is ever taken directly).

**New test:** `tests/vec/test_vec9_dynamic_index.npk` — constructs an `ivec9`
with 9 known values, verifies constant indexing and dynamic indexing via a
`loop(0, 9, 1)` counter, and accumulates the sum of all 9 elements. Runtime
exit 0. Added as CTest `vec9_dynamic_index_tests` (#18).

**KNOWN_ISSUES.md** updated to v0.20.5: all 7 v0.20.x resolved items moved to
the Resolved section, test counts updated.

---

## 2. Final Test Counts

| Suite | Result |
|-------|--------|
| CTest total | **18/18** |
| K core (k_semantics_core) | ✅ pass |
| K proofs (k_semantics_proofs) | ✅ pass |
| vec9 dynamic index | 1/1 |
| Closure lifetime tests | 1/1 |
| Optional safe navigation | 1/1 |
| Genuine failures | **0** |

---

## 3. Features Added in v0.20.x

| Feature | First Available |
|---------|----------------|
| `UNUSED_FUNCTION` warning with real analysis | v0.20.0 |
| `EMPTY_BLOCK` warning with real analysis | v0.20.0 |
| ICE messages for unhandled AST nodes (IR + TC) | v0.20.0 |
| `%error` preprocessor directive | v0.20.1 |
| `%warning` preprocessor directive | v0.20.1 |
| `%str()` stringify in `%define` macros | v0.20.1 |
| Token concatenation (`##`) in `%define` macros | v0.20.1 |
| `Display` trait for struct/enum interpolation | v0.20.2 |
| Template literal `&{struct}` via Display vtable | v0.20.2 |
| `comptime` struct/array comparison | v0.20.3 |
| `comptime` non-integer store/deref | v0.20.3 |
| `comptime` memoization cache (recursive functions) | v0.20.3 |
| Closure lifetime escape detection (compile-time error) | v0.20.4 |
| `optional<T>` correct type propagation through `?.` | v0.20.4 |
| Null coalescing `?|` operator | v0.20.4 |
| `vec9` / `ivec9` dynamic indexing with runtime variable | v0.20.5 |

---

## 4. Sign-Off

- ✅ All 18 CTest suites pass (18/18, 0 failures)
- ✅ K proofs still valid (k_semantics_proofs passes)
- ✅ No regressions vs v0.19.5 baseline (tests #1–#17 all pass)
- ✅ No new ICEs introduced
- ✅ `KNOWN_ISSUES.md` updated to v0.20.5
- ✅ vec9 dynamic indexing confirmed correct at IR level (`--emit-llvm` verified
  the select-chain uses matching i32 types throughout)
