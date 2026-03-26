# Aria Compiler Test Status

**Last Updated**: March 28, 2026 (v0.2.14 audit)
**Previous**: February 5, 2026 (v0.1.0 — 361/363, stale)

## Current Test Coverage

| Metric | Count | Rate |
|--------|-------|------|
| **Total Test Files** | 862 | — |
| **Positive Tests Passing** | 524 | — |
| **Negative Tests Correctly Rejected** | 116 | — |
| **Total Passing** | 640 | **74.2%** |
| **Actual Failures** | 222 | 25.8% |

> **Note**: "Negative tests" are adversarial tests that expect `COMPILER ERROR`.
> A negative test *passes* when the compiler correctly rejects the code.

---

## Per-Category Breakdown

| Category | Total | Pass | Fail | Neg✓ | Neg✗ | Rate |
|----------|------:|-----:|-----:|-----:|-----:|-----:|
| TOP-LEVEL | 377 | 321 | 7 | 47 | 2 | **98%** |
| adversarial | 60 | 23 | 5 | 31 | 1 | 90% |
| allocator | 4 | 3 | 0 | 0 | 1 | 75% |
| cast | 7 | 0 | 7 | 0 | 0 | 0% |
| coverage | 2 | 0 | 2 | 0 | 0 | 0% |
| determinism | 6 | 4 | 0 | 2 | 0 | 100% |
| feature_validation | 73 | 34 | 26 | 13 | 0 | 64% |
| future | 2 | 0 | 2 | 0 | 0 | 0% |
| fuzz | 100 | 85 | 15 | 0 | 0 | 85% |
| integration | 6 | 4 | 2 | 0 | 0 | 67% |
| io | 3 | 0 | 1 | 2 | 0 | 67% |
| kitchen_sink | 1 | 0 | 0 | 1 | 0 | 100% |
| manual | 8 | 6 | 0 | 2 | 0 | 100% |
| misc | 156 | 6 | 147 | 3 | 0 | 6% |
| module_loading | 1 | 0 | 1 | 0 | 0 | 0% |
| module_test | 3 | 0 | 2 | 1 | 0 | 33% |
| preprocessor | 2 | 0 | 0 | 2 | 0 | 100% |
| primitives | 3 | 3 | 0 | 0 | 0 | 100% |
| regression | 5 | 5 | 0 | 0 | 0 | 100% |
| result | 4 | 4 | 0 | 0 | 0 | 100% |
| runtime | 1 | 0 | 0 | 1 | 0 | 100% |
| safety | 6 | 3 | 0 | 3 | 0 | 100% |
| safety_critical | 10 | 10 | 0 | 0 | 0 | 100% |
| stdlib | 12 | 3 | 1 | 8 | 0 | 92% |
| string | 5 | 5 | 0 | 0 | 0 | 100% |
| syntax | 1 | 1 | 0 | 0 | 0 | 100% |
| wasm | 4 | 4 | 0 | 0 | 0 | 100% |

**Key**: Pass = positive test compiled OK, Fail = positive test failed,
Neg✓ = negative test correctly rejected, Neg✗ = negative test wrongly accepted

### 100% Pass Categories (13)
determinism, kitchen_sink, manual, preprocessor, primitives, regression,
result, runtime, safety, safety_critical, string, syntax, wasm

### Problem Categories
- **misc** (6%): 147 failures — mostly AI-generated tests using `void` instead of `NIL`
- **cast** (0%): 7 tests — cast syntax not yet implemented
- **feature_validation** (64%): 26 failures — tests for partially-implemented features
- **fuzz** (85%): 15 failures — edge-case inputs

---

## Failure Root Causes (222 actual failures)

| Root Cause | Count | Category |
|------------|------:|----------|
| Uses `void` instead of `NIL` | 83 | Test quality |
| Undefined identifier (unimplemented feature) | 47 | Feature gap |
| Missing `failsafe` function | 37 | Test quality |
| Result unwrap issues | 19 | Test quality |
| Parse/syntax errors | 15 | Mixed |
| Trait-related (unimplemented) | 8 | Feature gap |
| Negative test wrongly accepted | 4 | Compiler bug |
| `return` instead of `pass()` | 2 | Test quality |
| Module import failure | 1 | Feature gap |
| Borrow checker false positive | 1 | Compiler bug |
| Other (timeouts, misc) | 5 | Mixed |

### Easily Fixable (122 tests — test quality, not compiler bugs)
- **83**: Change `void` → `NIL` in return types
- **37**: Add required `failsafe` function
- **2**: Change `return` → `pass()`

Fixing these would raise the pass rate to **88.4%** (762/862).

### Compiler Bugs (5 tests)
- **4 negative tests wrongly accepted**: Compiler should reject but doesn't
  - `tests/adversarial/type_system/tbb_sentinel_direct_construct.aria`
  - `tests/allocator/quick_builtin_test.aria`
  - `tests/borrow_lifetime.aria`
  - `tests/borrow_wild.aria`
- **1 borrow checker false positive**

### Feature Gaps (56 tests)
Tests referencing features not yet implemented (traits, cast syntax, full
module system, I/O module). These will pass when the features ship.

---

## Future Features (2 tests — blocked on implementation)

Located in `tests/future/` — **not included in pass rate**.

### 1. batch02_gemini_audit_fixes.aria
**Status**: Specification test for unimplemented trit/nit types
**Blocks**: Nikola substrate implementation

### 2. safety_critical_suite.aria
**Status**: Integration test requiring multiple subsystems
**Blocks**: Extended safety validation

---

## misc/ Directory (156 tests, 6% pass rate)

The `tests/misc/` directory contains 156 tests, mostly AI-generated during
earlier development phases. 147 of these fail due to test quality issues
(not compiler bugs):

- **83**: Use `void` return type (Aria uses `NIL`)
- **31**: Missing required `failsafe` function
- **20**: Reference undefined identifiers
- **12**: Parse/syntax errors in the test itself
- **1**: Timeout

These tests need cleanup before they can be considered reliable. The compiler
itself is not at fault.

---

## Recommended Next Steps

### Quick Wins (raises pass rate to ~88%)
1. Batch-fix 83 `void` → `NIL` tests
2. Add `failsafe` to 37 tests missing it
3. Fix 2 `return` → `pass()` tests

### Compiler Bugs to Investigate
1. 4 negative tests the compiler wrongly accepts
2. 1 borrow checker false positive

### Feature Gaps to Close
1. Cast syntax (7 tests)
2. Trait system (8 tests)
3. Full module system (3 tests)
4. I/O module completion (3 tests)

---

## Running Tests

### Individual Test
```bash
./build/ariac tests/path/to/test.aria
```

### Integration Tests
```bash
cd build && ctest
```

---

## Test Organization

| Directory | Purpose |
|-----------|---------|
| `tests/*.aria` | Core feature tests (top-level) |
| `tests/adversarial/` | Negative tests — expected to fail compilation |
| `tests/fuzz/` | Fuzzer-generated edge cases |
| `tests/feature_validation/` | Tests for specific language features |
| `tests/misc/` | Bulk AI-generated tests (need cleanup) |
| `tests/future/` | Tests for unimplemented features (do not run) |
| `tests/regression/` | Bug-fix regression tests |
| `tests/safety_critical/` | Safety-critical system tests |
| `tests/stdlib/` | Standard library tests |

### Negative Test Convention
Tests expecting compiler errors should include a header comment:
```aria
/**
 * Test: [Description]
 * Expected: COMPILER ERROR - [reason]
 */
```

---

**Current Status**: 640/862 passing (74.2%) — 122 easily fixable test quality issues
**Previous Claim**: 361/363 (v0.1.0, February 2026) — obsolete, did not count new tests
**Audited**: March 28, 2026 by automated per-file compilation audit
