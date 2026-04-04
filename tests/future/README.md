# Future Feature Tests

**Status: EMPTY as of v0.13.3**

All tests have been either:
- ✅ Fixed and promoted to main test suite
- ✅ Discarded (tested unplanned features: GPU, Vec<T> generics, UFCS, LBIM methods)

## Promoted Tests (v0.13.3)
- `numeric_types_parser_test.aria` → `tests/` (fixed i32→i64 type mismatches)
- `test_turbofish_module.aria` → `tests/misc/` (expected-error test, works as-is)

## Discarded Tests (v0.13.3)
- `test_gpu_intrinsics.aria` — GPU not in v0.13.x scope
- `test_gpu_vector_add.aria` — GPU not in v0.13.x scope
- `test_fix256_div_gpu.aria` — GPU not in v0.13.x scope
- `test_fix256_gpu.aria` — GPU not in v0.13.x scope
- `test_vec_methods.aria` — Vec<T> generics not implemented
- `test_vec_phase3.aria` — Vec<T> generics not implemented
- `test_atomic_ufcs.aria` — UFCS not implemented, no failsafe
- `safety_critical_suite.aria` — Requires int1024.method() syntax not supported

