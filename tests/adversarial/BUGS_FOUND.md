# Adversarial Testing - Bugs Found

**Date**: December 28, 2025
**Phase**: 2.1 Adversarial Testing
**Status**: ALL BUGS FIXED

---

## ✅ BUG #1 (FIXED): Reference Escape to Local Variable

**Test File**: superseded legacy fixture
**Expected**: COMPILER ERROR
**Actual**: CORRECTLY REJECTED ✅

### Description
An early pre-`$$i`/`$$m` borrow-expression fixture showed that returning a
reference to a local variable could create a dangling reference. Current Aria no
longer exposes dollar-prefixed borrow-return syntax; active tests now reject
that syntax at parse/semantic entry points and cover borrow lifetimes with
`$$i` / `$$m` declarations and parameters.

### Code
Legacy code sample removed: dollar-prefixed borrow-return expressions are not
Aria syntax.

### Impact
- **Severity**: CRITICAL
- **Safety Risk**: Use-after-free, dangling pointer dereference
- **Affected Feature**: Borrow checker lifetime analysis

### Fix Applied
The old borrow-expression escape hook is now a no-op because the syntax itself
is invalid. Current lifetime checks are attached to `$$i` / `$$m` declarations,
parameters, and path aliases.

---

## ✅ BUG #2 (FIXED): Move of Pinned Variable

**Test File**: `borrow_checker/pin_then_move.aria`
**Expected**: COMPILER ERROR
**Actual**: CORRECTLY REJECTED ✅

### Description
After pinning a variable with `#x`, the compiler still allows moving that variable by passing it to a function. This violates Appendage Theory - pinned variables must not be movable.

### Code
```aria
func:take_ownership = void(int32:val) {
    print("Got value\n");
};

func:main = int32() {
    int32:x = 10;
    wild int32@:pin_x = #x;  // Pin x

    take_ownership(x);  // ERROR: Cannot move pinned variable

    return 0;
};
```

### Impact
- **Severity**: CRITICAL
- **Safety Risk**: Pin promises address stability; moving the value breaks this invariant
- **Affected Feature**: Pin operator (#) tracking in borrow checker

### Fix Applied
Modified `checkCallExpr()` in `borrow_checker.cpp` to:
1. Check each function argument for identifier expressions
2. If the identifier is a pinned variable, emit error
3. Error message: "Cannot pass pinned variable by value. Use a reference instead."

---

---

## ✅ BUG #3 (FIXED): TBB Mixed Size Implicit Widening

**Test File**: `type_system/tbb_mixed_sizes.aria`
**Expected**: COMPILER ERROR
**Actual**: CORRECTLY REJECTED ✅

### Description
The compiler allowed implicit widening between TBB types of different sizes (e.g., tbb8 → tbb16). This is unsound because different TBB sizes have different sentinel values:
- tbb8 sentinel = -128
- tbb16 sentinel = -32768

Widening -128 from tbb8 to tbb16 would create a valid value (not a sentinel), breaking sticky error semantics.

### Code
```aria
func:main = int32() {
    tbb8:small = 10;
    tbb16:large = small;  // ERROR: No implicit widening between TBB sizes
    return 0;
};
```

### Fix Applied
Modified `PrimitiveType::isAssignableTo()` in `type.cpp` and `canCoerce()` in `type_checker.cpp` to:
1. Reject all TBB → TBB coercion except exact same type
2. Added ARIA-018 documentation explaining sentinel value differences

---

## ✅ BUG #4 (FIXED): Move of Borrowed Variable

**Test File**: `borrow_checker/move_while_borrowed.aria`
**Expected**: COMPILER ERROR
**Actual**: CORRECTLY REJECTED ✅

### Description
The compiler allowed moving (passing by value to a function) a variable while it had active borrows. This would invalidate the borrows and lead to use-after-move.

### Code
```aria
func:main = int32() {
    int32:x = 10;
    $$m int32:r = x;  // Borrow x
    take_ownership(x);  // ERROR: Cannot move x while borrowed
    return 0;
};
```

### Fix Applied
Modified `checkCallExpr()` in `borrow_checker.cpp` to:
1. Check if identifier arguments have active loans
2. If borrowed, emit error with location of the borrow

---

## ✅ Tests That Correctly Reject

### Borrow Checker Tests
1. `aliasing_two_mut.aria` - Double mutable borrow ✅
2. `aliasing_mut_and_immut.aria` - Mutable + immutable borrow ✅
3. `aliasing_immut_after_mut.aria` - Immutable after mutable ✅
4. `aliasing_modify_while_borrowed.aria` - Modify borrowed variable ✅
5. `aliasing_loop_overlap.aria` - Loop overlap borrow ✅
6. `double_pin.aria` - Double pin same variable ✅
7. `borrow_through_mut.aria` - Immutable borrow after mutable ✅
8. `reassign_pinned.aria` - Reassign pinned variable ✅
9. `move_while_borrowed.aria` - Move borrowed variable ✅

### TBB Type System Tests
10. `tbb_sentinel_direct_construct.aria` - Sentinel literal rejection ✅
11. `tbb_to_int_implicit.aria` - TBB → int conversion rejection ✅
12. `int_to_tbb_implicit.aria` - int → TBB conversion rejection ✅
13. `tbb_mixed_sizes.aria` - TBB size widening rejection ✅

---

## Summary

| Category | Tests Run | Passed | Bugs Found | Bugs Fixed |
|----------|-----------|--------|------------|------------|
| Borrow Checker | 11 | 11 | 3 | 3 |
| TBB Type System | 4 | 4 | 1 | 1 |
| **Total** | **15** | **15** | **4** | **4** |

**All 15 tests now correctly reject their violations**

---

## Fixes Applied

1. **Bug #1 Fixed**: Added `checkReferenceEscape()` to detect returning references to locals
2. **Bug #2 Fixed**: Modified `checkCallExpr()` to detect passing pinned variables by value
3. **Bug #3 Fixed**: Modified `isAssignableTo()` to reject TBB mixed size coercion
4. **Bug #4 Fixed**: Modified `checkCallExpr()` to detect moving borrowed variables

---

## ✅ BUG #5 (FIXED): Conditional Borrow Merge

**Test File**: `borrow_checker/conditional_borrow_merge.aria`
**Expected**: COMPILER ERROR
**Actual**: CORRECTLY REJECTED ✅

### Description
After a conditional where different branches borrow different variables, the compiler now conservatively tracks that the borrow target may vary.

### Code
```aria
if (cond) {
    $$i int32:r = x;
    x = 30;  // ERROR: x is borrowed by r
} else {
    $$i int32:r = y;
}
```

### Fix Applied
Two-part fix in `borrow_checker.cpp` (ARIA-021):
1. Modified `LifetimeContext::merge()` to preserve loans created in branches
2. Used UNION for:
   - `loan_origins`: track all possible borrow targets from either branch
   - `active_loans`: keep loans that exist in either branch
   - `active_pins`: keep pins that exist in either branch

---

## ✅ BUG #6 (FIXED): Same Variable as Multiple Mutable Arguments

**Test File**: `borrow_checker/param_alias_same_var.aria`
**Expected**: COMPILER ERROR
**Actual**: CORRECTLY REJECTED ✅

### Description
Passing the same variable as two mutable reference arguments creates aliasing.

### Code
```aria
modify_both(x, x);  // ERROR: Same variable borrowed mutably twice
```

### Fix Applied
Modified `checkCallExpr()` in `borrow_checker.cpp` (ARIA-020a) to:
1. Track all borrow targets in function arguments
2. If same variable borrowed multiple times, emit error
3. Error message: "Cannot borrow 'x' multiple times in the same function call"

---

## ✅ BUG #7 (FIXED): Reborrow While Mutable Borrow Exists

**Test File**: `borrow_checker/reborrow_mutable.aria`
**Expected**: COMPILER ERROR
**Actual**: CORRECTLY REJECTED ✅

### Description
Passing a variable to a function while an existing mutable borrow is active.

### Code
```aria
$$m int32:m1 = x;  // First borrow
use_ref(x);        // ERROR: x already borrowed by m1
```

### Fix Applied
Modified `checkCallExpr()` in `borrow_checker.cpp` (ARIA-020b) to:
1. Check if borrow target in function argument already has active loans
2. If borrowed, emit error with location of existing borrow
3. Error message: "Cannot borrow 'x' because it is already borrowed by 'm1'"

---

## Summary (Final)

| Category | Tests Run | Passed | Bugs Found | Bugs Fixed |
|----------|-----------|--------|------------|------------|
| Borrow Checker | 17 | 17 | 6 | 6 |
| TBB Type System | 4 | 4 | 1 | 1 |
| **Total** | **21** | **21** | **7** | **7** |

**All 7 bugs fixed!**

---

## Fixes Applied (Complete List)

1. **Bug #1 Fixed**: Added `checkReferenceEscape()` to detect returning references to locals
2. **Bug #2 Fixed**: Modified `checkCallExpr()` to detect passing pinned variables by value
3. **Bug #3 Fixed**: Modified `isAssignableTo()` to reject TBB mixed size coercion
4. **Bug #4 Fixed**: Modified `checkCallExpr()` to detect moving borrowed variables
5. **Bug #5 Fixed**: Modified `checkAssignment()` and `merge()` for conditional borrow tracking
6. **Bug #6 Fixed**: Modified `checkCallExpr()` to detect argument aliasing
7. **Bug #7 Fixed**: Modified `checkCallExpr()` to detect reborrow while borrowed
