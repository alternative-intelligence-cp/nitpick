# Test Status - December 20, 2025

## Current Status
- ✅ **9 tests passing** (14%)
- ❌ **16 tests failing at compilation**
- ❌ **40 tests failing at runtime** (mostly segfaults)
- **Total**: 65 tests

## Changes Made
1. Bulk updated 28 tests: `int32()` → `int64()` return types
2. Fixed `array_minimal.aria`: `int32[]` → `int64[]` for literals
3. Fixed `check_defer_support.aria`: Removed semicolon after defer block

## Compilation Failures (16 tests)

### Category 1: Empty Expression Syntax (control flow blocks)
Files with `if () { };` pattern (should be `if () { }` without semicolon):
- `control_flow_test.aria` - 13 instances
- `check_parser_support.aria` - Multiple instances  
- Others TBD

**Fix**: Remove `;` after control flow block endings

### Category 2: Design Incompatibility - Truthiness
- `comprehensive_short_circuit.aria` - Uses C-style truthiness (int treated as bool)
- `operators_test.aria` - Same issue + bitwise requires unsigned types

**Decision Needed**: These tests assume C-style truthiness which Aria rejects by design.
Either:
1. Update tests to use explicit comparisons (`x != 0`)
2. Change language design to allow truthiness

### Category 3: Generic Function Syntax
- `generics_basic.aria` - Uses `ret` instead of `pass`  
- `generics_simple_test.aria` - Wrong generic syntax `func<T>:f = T(...)` should be `= *T(...)`
- `generics_void_test.aria` - Uses `void` return type (not supported?)
- `generics_concrete_test.aria` - Type inference issues
- `generics_main.aria` - Cannot compare `*T` and `*U`

**Fix**: Update to correct syntax and return statements

### Category 4: Miscellaneous
- `arrays_simple.aria` - Missing semicolon at EOF
- `check_struct_instantiation.aria` - References undefined functions
- Others need investigation

## Runtime Failures (40 tests - Segfaults)

Most tests compile but crash at runtime. Categories likely include:
1. Array operations (indexing, slicing)
2. String operations  
3. Defer operations
4. Module/generic instantiation
5. Complex control flow

**Investigation Needed**: Run tests with debugger or add print statements

## Passing Tests (9)
1. `check_struct_simple.aria`
2. `generics_multiple_types.aria`
3. `generics_parse_only.aria`
4. `generic_struct_basic.aria`
5. `module_errors.aria`
6. `result_simple.aria`
7. `test_main.aria`
8. `test_regular_call.aria`
9. `unwrap_simple.aria`

## Recommended Approach

### Phase 1: Quick Wins (Compilation Fixes)
1. Remove semicolons from control flow blocks (bulk sed)
2. Fix generic syntax errors (manual, 5-6 files)
3. Update truthiness tests to use explicit comparisons

**Expected Impact**: +8-10 tests compiling

### Phase 2: Runtime Debugging
1. Start with simplest failing tests
2. Use gdb or add debug prints
3. Fix array/string/defer bugs systematically

**Expected Impact**: +10-20 tests passing

### Phase 3: Design Decisions
1. Document which tests are incompatible with design
2. Either update tests or language
3. Create new tests for actual features

## Long-Term Goal
Target: 90%+ pass rate before v0.1.0 release
Current: 14% → Need to fix ~50 tests
