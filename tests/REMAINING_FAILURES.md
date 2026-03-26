# Remaining Test Failures — v0.2.14 Audit

**Last Updated**: March 28, 2026
**Previous**: February 1, 2026 (v0.1.0 — 7/146, stale)

## Summary: 222 Failures out of 862 Tests (74.2% pass rate)

This count properly handles **negative tests** (adversarial tests that
*expect* compiler errors). A negative test passes when the compiler
correctly rejects the code.

---

## Failure Breakdown by Root Cause

### Test Quality Issues (122 — easily fixable, not compiler bugs)

| Issue | Count | Fix |
|-------|------:|-----|
| Uses `void` instead of `NIL` | 83 | Change return type to `NIL` |
| Missing `failsafe` function | 37 | Add `failsafe` func to test |
| Uses `return` instead of `pass()` | 2 | Replace with `pass()` |

Fixing these 122 tests would raise the pass rate from 74.2% to **88.4%**.

### Undefined Identifiers / Unimplemented Features (47)

Tests reference functions, types, or syntax not yet in the compiler:
- Trait methods and implementations
- Cast expressions
- Extended module system features
- I/O module functions
- Various stdlib functions not yet exposed

### Result Unwrap Issues (19)

Tests use `Result<T>` without properly handling the error case, or call
`.unwrap()` when it doesn't exist as a method.

### Parse / Syntax Errors (15)

Tests contain syntax the parser doesn't yet accept:
- Array type syntax `Type[size]:name`
- Generic struct syntax `struct<T, E>`
- Extern func outside blocks
- Static member access `Type.CONSTANT`

### Trait-Related (8)

Tests exercise the trait system which is partially implemented.
These will pass as the trait system matures.

### Compiler Bugs (5)

**4 negative tests wrongly accepted** (compiler fails to reject bad code):
- `tests/adversarial/type_system/tbb_sentinel_direct_construct.aria`
- `tests/allocator/quick_builtin_test.aria`
- `tests/borrow_lifetime.aria`
- `tests/borrow_wild.aria`

**1 borrow checker false positive** (compiler wrongly rejects valid code)

### Other (6)

- 2 timeouts
- 1 module import failure
- 1 `print()` type argument error
- 2 miscellaneous

---

## By Test Directory

| Directory | Failures | Total | Notes |
|-----------|:--------:|:-----:|-------|
| misc/ | 147 | 156 | Mostly `void`→`NIL` + missing `failsafe` |
| feature_validation/ | 26 | 73 | Partially-implemented features |
| fuzz/ | 15 | 100 | Edge cases |
| TOP-LEVEL | 7 | 377 | 98% pass rate |
| cast/ | 7 | 7 | Cast syntax not implemented |
| adversarial/ | 5 | 60 | 90% (negative-test-aware) |
| coverage/ | 2 | 2 | Test infrastructure |
| future/ | 2 | 2 | By design — blocked on features |
| integration/ | 2 | 6 | Module/import issues |
| module_test/ | 2 | 3 | Module system gaps |
| io/ | 1 | 3 | I/O features not yet available |
| module_loading/ | 1 | 1 | Module resolution |
| stdlib/ | 1 | 12 | 92% pass with negative tests |
| allocator/ | 1 | 4 | One wrongly-accepted negative test |

---

## Priority Recommendations

### P0 — Quick Wins (batch-fixable)
Fix the 122 test quality issues. These are mechanical fixes:
```bash
# void → NIL (83 tests)
# Add failsafe func (37 tests)
# return → pass() (2 tests)
```

### P1 — Compiler Bugs
Investigate the 5 tests where the compiler produces wrong behavior.

### P2 — Feature Completion
The 56 feature gap failures will resolve as features ship.

---

Generated: March 28, 2026 — v0.2.14 automated audit
Previous: February 1, 2026 — v0.1.0 (7/146 failures, commit 3730f5b)
