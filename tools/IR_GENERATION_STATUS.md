# Aria IR Generation Status & Implementation Plan

**Date:** 2026-02-02  
**Current State:** 39/142 tests passing (27.5%)  
**Phase:** 3 - IR Generation

---

## Executive Summary

IR generation infrastructure exists and is functional for basic types and operations. 27.5% of tests successfully compile to executables. The foundation is solid - we have working code generation for:

- Integer arithmetic (int8→int64, uint*)
- Floating point (flt16→flt512)  
- TBB types (basic storage and arithmetic)
- Vector types (vec2)
- Control flow (if/else, while)
- Functions and returns

**Critical gaps** preventing the remaining 72% from compiling:
1. **Arrays** - No IR for array types (int32@)
2. **Templates** - Generic instantiation not generating IR
3. **Large integers** - LBIM structs (int128→int4096) failing
4. **TBB safety** - Missing overflow→ERR conversion
5. **Strings** - String operations not implemented

---

## ✅ Working Features (27.5% - High Quality)

### Primitive Types
```
✅ int8, int16, int32, int64
✅ uint*, flt16, flt32, flt64, flt128, flt256, flt512
✅ tbb8, tbb16, tbb32, tbb64 (basic)
✅ vec2<T> (all operations)
```

### Operations
```
✅ Arithmetic: +, -, *, /, %
✅ Comparison: ==, !=, <, >, <=, >=
✅ Logical: and, or, not
✅ Unary: -, !
```

### Control Flow
```
✅ if/else statements
✅ while loops
✅ Function calls
✅ return statements
```

### Passing Tests (39)
- blueprint_pointer_demo.aria
- const_*.aria (6 tests)
- err_literal.aria
- flt*_*.aria (12 tests)
- literal_*.aria (3 tests)
- math_types_basic.aria
- move_basic_test.aria
- null_coalesce_test.aria
- phase4_typed_literals.aria
- pow2_coverage.aria
- simple_*.aria (3 tests)
- small_int_types.aria
- tbb_basic.aria
- uint_token_test.aria
- unwrap_operator_test.aria
- vec2_*.aria (5 tests)

---

## ❌ Missing Features (72% - Prioritized)

### Priority 1: Core Type System Gaps

#### 1.1 Array Types (Blocking ~20 tests)
**Status:** NOT IMPLEMENTED  
**Impact:** HIGH - Many tests use arrays

**Failing tests:**
- array_slice.aria, array_slice_simple.aria
- All large_int tests (use arrays)
- numeric_types tests

**Required IR:**
```llvm
; Array allocation
%arr = alloca [10 x i32], align 4

; Array indexing
%elem_ptr = getelementptr [10 x i32], ptr %arr, i64 0, i64 %index
%elem = load i32, ptr %elem_ptr
```

**Implementation plan:**
1. Add array type recognition in IR generator
2. Implement alloca for array types
3. Add GEP (getelementptr) for indexing
4. Handle array literals and initialization

---

#### 1.2 Large Integer Types - LBIM Structs (Blocking ~30 tests)
**Status:** PARTIALLY IMPLEMENTED  
**Impact:** HIGH - Exotic balanced types failing

**Failing tests:**
- int128_basic.aria
- int256_basic.aria (COMPILATION_ERROR)
- int512_basic.aria (COMPILATION_ERROR)  
- int1024_basic.aria, int2048_basic.aria, int4096_basic.aria
- All int4096_*.aria tests (~15 tests)

**Current state:**
- IR generator has `promoteToLBIMStruct` function
- LBIM types recognized (int128→int4096, uint128→uint4096)
- **BUT:** Arithmetic operations not generating correct IR

**Required IR:**
```llvm
; LBIM struct: { i64 limbs[N], i8 sign }
%int256 = type { [4 x i64], i8 }

; Addition call
%result = call %int256 @aria_int256_add(%int256 %a, %int256 %b)
```

**Implementation plan:**
1. Debug why int256/int512 show COMPILATION_ERROR
2. Implement runtime functions: aria_intN_add/sub/mul/div
3. Generate calls to runtime for LBIM arithmetic
4. Handle LBIM comparisons

---

#### 1.3 Template Instantiation (Blocking ~6 tests)
**Status:** NOT IMPLEMENTED IN IR  
**Impact:** MEDIUM - Affects generic code

**Failing tests:**
- template_basic.aria, template_bools.aria
- template_complex.aria, template_floats.aria
- template_integers.aria, template_mixed.aria

**Required:**
- Semantic analyzer does template instantiation
- IR generator needs to codegen instantiated versions
- Each instantiation (e.g., Vec<int32>, Vec<flt64>) needs separate functions

**Implementation plan:**
1. Check if generic_resolver creates instantiated AST nodes
2. Generate IR for each template specialization
3. Mangle names: `Vec_push_int32`, `Vec_push_flt64`

---

### Priority 2: Safety Features (Critical for Children)

#### 2.1 TBB Overflow Detection (Blocking ~6 tests)
**Status:** PARTIAL - TBB types work, but NO overflow checking  
**Impact:** CRITICAL - Children's safety feature

**Failing tests:**
- tbb_overflow.aria, tbb_mult_overflow.aria
- tbb_division.aria
- tbb_err_literal.aria
- tbb_sticky_error.aria
- tbb_auto_lowering.aria

**Current state:**
- TBB arithmetic uses normal LLVM add/sub/mul
- **No overflow detection**
- **No ERR conversion**

**Required IR:**
```llvm
; TBB add with overflow check
%{result, overflow} = call {i8, i1} @llvm.sadd.with.overflow.i8(i8 %a, i8 %b)
%is_overflow = extractvalue {i8, i1} %{result, overflow}, 1
br i1 %is_overflow, label %set_err, label %normal

set_err:
  store i8 -128, ptr %result_ptr  ; ERR sentinel
  br label %continue

normal:
  %val = extractvalue {i8, i1} %{result, overflow}, 0
  store i8 %val, ptr %result_ptr
  br label %continue
```

**Implementation plan:**
1. Use LLVM intrinsics: `llvm.sadd.with.overflow.i*`
2. Check overflow flag after each TBB operation
3. Convert to ERR (-128/-32768/etc) on overflow
4. Implement sticky ERR: check if either operand is ERR

---

#### 2.2 ERR Propagation - Sticky Semantics (Blocking ~3 tests)
**Status:** NOT IMPLEMENTED  
**Impact:** CRITICAL - Core safety feature

**Required IR:**
```llvm
; Before operation: check if either operand is ERR
%a_val = load i8, ptr %a
%b_val = load i8, ptr %b
%a_is_err = icmp eq i8 %a_val, -128
%b_is_err = icmp eq i8 %b_val, -128
%either_err = or i1 %a_is_err, %b_is_err
br i1 %either_err, label %propagate_err, label %do_operation

propagate_err:
  store i8 -128, ptr %result
  br label %continue

do_operation:
  ; ... actual TBB arithmetic with overflow check ...
```

**Implementation plan:**
1. Add ERR checks before every TBB operation
2. Short-circuit to ERR if either operand is ERR
3. Combine with overflow detection (Priority 2.1)

---

### Priority 3: Language Features

#### 3.1 String Operations (Blocking ~3 tests)
**Status:** NOT IMPLEMENTED  
**Impact:** LOW - Nice to have

**Failing tests:**
- string_comprehensive.aria
- string_executable.aria
- string_ultra_simple.aria

---

#### 3.2 For Loops / Ranges (Blocking ~2 tests)
**Status:** NOT IMPLEMENTED  
**Impact:** LOW

**Failing tests:**
- for_range.aria, for_range_typed.aria
- range_basic.aria, range_expressions.aria

---

#### 3.3 Borrow Checking IR (Blocking ~5 tests)
**Status:** Semantic analysis works, IR not implemented  
**Impact:** MEDIUM - Memory safety

**Failing tests:**
- borrow_basic.aria, borrow_lifetime.aria
- borrow_integration_test.aria
- borrow_pinning.aria, borrow_wild.aria
- move_semantics_test1.aria

**Note:** move_comprehensive_test and move_use_after_move_test intentionally fail (validation tests).

---

#### 3.4 Nil/Optional Types (Blocking ~5 tests)
**Status:** NOT IMPLEMENTED  
**Impact:** MEDIUM

**Failing tests:**
- nil_basic.aria, nil_test.aria
- nil_coalesce_debug.aria
- nil_optional_comprehensive.aria
- nil_vs_null.aria

---

#### 3.5 Balanced Ternary / Nonary (Blocking ~2 tests)
**Status:** PARTIAL - ternary_codegen.cpp exists  
**Impact:** LOW - Exotic feature

**Failing tests:**
- balanced_ternary_basic.aria
- nonary_basic.aria
- ternary_*.aria (6 tests)

---

#### 3.6 Miscellaneous
- arrow_operator_*.aria (3 tests) - Method call syntax
- gap001/002_test.aria (2 tests) - Unknown
- pointer_*.aria (3 tests) - Pointer operations beyond basic
- print_*.aria (3 tests) - I/O operations
- safety_critical_*.aria (3 tests) - Advanced safety features
- generic_struct_basic.aria - Generic structs
- map_test.aria - Map data structure
- ffi_safety_test.aria - FFI
- file_io_*.aria (2 tests) - File I/O
- fall_statement_test.aria - Fall through

---

## Implementation Strategy

### Phase 3.1: Core Type System (2-3 days)
**Goal:** Get to 50% test pass rate

**Tasks:**
1. **Arrays** (1 day)
   - Implement array allocation in IR
   - Add GEP for indexing
   - Handle array literals
   - **Expected:** +15-20 tests passing

2. **LBIM Debug** (1 day)
   - Fix int256/int512 compilation errors
   - Implement runtime library stubs for LBIM arithmetic
   - **Expected:** +5-10 tests passing

3. **Templates** (1 day)
   - Generate IR for template specializations
   - Name mangling for instantiated versions
   - **Expected:** +6 tests passing

**Total expected:** ~55-70% pass rate (78-99 tests)

---

### Phase 3.2: TBB Safety (2-3 days)
**Goal:** Children's safety features operational

**Tasks:**
1. **Overflow Detection** (1-2 days)
   - Use llvm.sadd.with.overflow intrinsics
   - Convert overflow → ERR
   - Test with tbb_overflow.aria
   - **Expected:** +3-4 tests passing

2. **ERR Propagation** (1 day)
   - Add ERR checks before operations
   - Sticky semantics: once ERR, always ERR
   - **Expected:** +2-3 tests passing

**Total expected:** ~75-80% pass rate (106-113 tests)

---

### Phase 3.3: Remaining Features (3-5 days)
**Goal:** 90%+ pass rate

**Tasks:**
1. **Strings** (1 day)
2. **For loops** (1 day)
3. **Borrow checking IR** (1 day)
4. **Nil/Optional** (1 day)
5. **Misc fixes** (1-2 days)

**Total expected:** 90%+ pass rate (127+ tests)

---

## Metrics & Goals

| Phase | Pass Rate | Tests Passing | Timeline |
|-------|-----------|---------------|----------|
| Current | 27.5% | 39/142 | ✅ Done |
| 3.1 (Types) | 55-70% | 78-99 | 2-3 days |
| 3.2 (Safety) | 75-80% | 106-113 | +2-3 days |
| 3.3 (Features) | 90%+ | 127+ | +3-5 days |
| **Goal** | **95%+** | **135+** | **7-11 days** |

---

## Next Actions

**IMMEDIATE (Today):**
1. ✅ Create this status document
2. 🔄 Fix array IR generation
   - Start with simple array allocation
   - Test with array_slice_simple.aria
3. 🔄 Debug int256/int512 compilation errors
   - Understand why they show COMPILATION_ERROR vs others
   - Check IR generator LBIM handling

**TOMORROW:**
1. Complete arrays
2. Fix LBIM arithmetic generation
3. Begin template IR generation

**THIS WEEK:**
1. Phase 3.1 complete (50-70% pass rate)
2. Begin Phase 3.2 (TBB safety)

---

## Technical Notes

### LLVM Intrinsics for TBB
```llvm
declare {i8, i1} @llvm.sadd.with.overflow.i8(i8, i8)
declare {i16, i1} @llvm.sadd.with.overflow.i16(i16, i16)  
declare {i32, i1} @llvm.sadd.with.overflow.i32(i32, i32)
declare {i64, i1} @llvm.sadd.with.overflow.i64(i64, i64)
```

### TBB ERR Sentinels
```
tbb8:  -128 (0x80)
tbb16: -32768 (0x8000)
tbb32: -2147483648 (0x80000000)
tbb64: -9223372036854775808 (0x8000000000000000)
```

### LBIM Structure
```cpp
struct int256 {
    uint64_t limbs[4];  // Little-endian: limbs[0] = low bits
    int8_t sign;        // 1 = positive, -1 = negative, 0 = zero
};
```

---

## For Nikola & Children

**What this means:**
- Basic Aria code compiles and runs! ✅
- TBB types exist but overflow safety not implemented yet ⚠️
- Arrays are the biggest gap - fixing this unlocks many tests
- Safety features (TBB overflow, ERR) are Priority 2 - critical for your safety

**Timeline:**
- **This week:** Arrays and large numbers working
- **Next week:** TBB overflow protection operational
- **Two weeks:** 90%+ tests passing, production-ready foundation

---

*Foundation is solid. 27.5% passing proves IR generation works. Now we systematically fill the gaps.*
