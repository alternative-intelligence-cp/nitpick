# K Semantics Status — v0.19.4

**Date:** 2026-05-07
**Branch:** dev-0.19.x
**Tag:** v0.19.4

## Test Results

| Suite | Passed | Failed | Total |
|-------|--------|--------|-------|
| K core tests | 127 | 0 | 127 |
| K proofs | 10 | 0 | 10 |

## New Tests (v0.19.4)

### Phase 1 — Dynamic-index borrows (110–114)
| # | Test | Expected |
|---|------|----------|
| 110 | array_dyn_index_imm_borrow_pass | 20 |
| 111 | array_dyn_index_mut_borrow_writeback_pass | 99 |
| 112 | array_two_dyn_borrows_same_array_failsafe | 7 |
| 113 | array_dyn_borrow_and_literal_diff_arrays_pass | 0 |
| 114 | array_dyn_index_same_slot_as_literal_failsafe | 7 |

### Phase 2 — Struct update expressions (115–117)
| # | Test | Expected |
|---|------|----------|
| 115 | struct_update_single_field_pass | 5 |
| 116 | struct_update_two_fields_pass | 30 |
| 117 | struct_update_unmodified_field_preserved_pass | 1 |

### Phase 3 — Struct field patterns in pick (118–120)
| # | Test | Expected |
|---|------|----------|
| 118 | pick_struct_pat_2field_pass | 30 |
| 119 | pick_struct_pat_2field_two_cases_pass | 10 |
| 120 | pick_struct_pat_type_mismatch_fallthrough_pass | 99 |

### Phase 4 — Lambda variables (121–125)
| # | Test | Expected |
|---|------|----------|
| 121 | lambda_1param_declare_call_pass | 42 |
| 122 | lambda_1param_double_call_pass | 42 |
| 123 | lambda_2param_pass | 42 |
| 124 | lambda_chained_calls_pass | 10 |
| 125 | lambda_identity_then_arithmetic_pass | 42 |

### Phase 5 — Non-pub helpers and loop scope (126–127)
| # | Test | Expected |
|---|------|----------|
| 126 | nonpub_helper_intramodule_call_pass | 42 |
| 127 | loop_outer_var_accessible_after_loop_pass | 10 |

## New Semantic Rules in aria.k

- `DynIndexPath(Id, Int)` / `DynIndexAlias(Id, Int)` borrow path types
- `Lambda1` / `Lambda2` Val constructors
- Struct update expressions: single-field and two-field variants
- Dynamic-index borrow rules (`$$i`/`$$m T:X = H[Idx]`) with conservative conflict model
- DynIndexAlias writeback rule
- `hasDynBorrowOnArray` predicate (DynIndexPath only; literal IndexPath excluded)
- Struct field pattern pick rules (2-field and 3-field, match/no-match/non-struct)
- Lambda declaration rules (1-param and 2-param)
- Lambda dispatch rules via env+store cell pattern matching
- `invokeLambda` / `invokeLambda2` execution with full call-frame setup
- `storeGet(Map, Int)` and `extractInt(Val)` helpers

## Bug Fixed
- `hasDynBorrowOnArray` incorrectly included `IndexPath(H, _)` match, which caused
  literal split borrows on different array slots to trigger false conflicts (test 107 regression).
  Fixed to only match `DynIndexPath(H, _)`.
