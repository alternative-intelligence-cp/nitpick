# Aria Compiler Test Suite v0.0.6

## Overview
Comprehensive test suite for Aria language features implemented in v0.0.6.

## Test Files

### 1. Assignment Operators (`test_assignments.aria`)
**Status**: ✅ Passing  
**Features Tested**:
- Basic assignment (`=`)
- Plus-assign (`+=`)
- Minus-assign (`-=`)
- Multiply-assign (`*=`)
- Divide-assign (`/=`)
- Modulo-assign (`%=`)

**Expected Results**:
- `test_basic_assign()` → 10
- `test_plus_assign()` → 8
- `test_minus_assign()` → 6
- `test_mul_assign()` → 10
- `test_div_assign()` → 5
- `test_mod_assign()` → 2

### 2. Dollar Variable (`test_dollar_variable.aria`)
**Status**: ✅ Passing  
**Features Tested**:
- `$` variable in till loops (iterator)
- Positive step till loops
- Negative step till loops
- Variable step sizes

**Expected Results**:
- `test_till_basic()` → 10 (sum of 0+1+2+3+4)
- `test_till_negative_step()` → 15 (sum of 5+4+3+2+1)
- `test_till_larger_step()` → 20 (sum of 0+2+4+6+8)

### 3. When Loops (`test_when_loops.aria`)
**Status**: ✅ Passing  
**Features Tested**:
- When loops with condition
- `then` block execution (normal completion)
- `end` block execution (break exit)
- When loops without then/end blocks
- Break statement in when loops

**Expected Results**:
- `test_when_success()` → 100 (then block executes)
- `test_when_break()` → -1 (end block executes)
- `test_when_no_blocks()` → 10 (sum of 0+1+2+3+4)

### 4. Pick Ranges (`test_pick_ranges.aria`)
**Status**: ✅ Passing  
**Features Tested**:
- Inclusive range syntax (`..`)
- Exclusive range syntax (`...`)
- Greater-than-or-equal comparison (`>=`)
- Less-than comparison (`<`)
- Pattern matching with ranges

**Expected Results**:
- `classify_number(3)` → 1 (matches 0..5)
- `classify_number(7)` → 2 (matches 6...10)
- `classify_number(15)` → 3 (matches >=10)
- `classify_number(-5)` → -1 (matches <0)
- `test_ranges()` → 5 (sum of all)

## Running Tests

### Compile All Tests
```bash
cd build
for test in ../tests/test_*.aria; do
    ./ariac "$test" -o "$(basename $test .aria).ll"
done
```

### Individual Test Compilation
```bash
./ariac ../tests/test_assignments.aria -o test_assignments.ll
./ariac ../tests/test_dollar_variable.aria -o test_dollar_variable.ll
./ariac ../tests/test_when_loops.aria -o test_when_loops.ll
./ariac ../tests/test_pick_ranges.aria -o test_pick_ranges.ll
```

## Test Coverage

| Feature | Parser | Codegen | Test File | Status |
|---------|--------|---------|-----------|--------|
| `$` variable | ✅ | ✅ | test_dollar_variable.aria | ✅ |
| `?` unwrap operator | ✅ | ✅ | (pending) | 🟡 |
| Assignment `=` | ✅ | ✅ | test_assignments.aria | ✅ |
| Compound assigns | ✅ | ✅ | test_assignments.aria | ✅ |
| Pick ranges `..` | ✅ | ✅ | test_pick_ranges.aria | ✅ |
| Pick ranges `...` | ✅ | ✅ | test_pick_ranges.aria | ✅ |
| Pick comparisons | ✅ | ✅ | test_pick_ranges.aria | ✅ |
| When loops | ✅ | ✅ | test_when_loops.aria | ✅ |
| When `then` block | ✅ | ✅ | test_when_loops.aria | ✅ |
| When `end` block | ✅ | ✅ | test_when_loops.aria | ✅ |

## Known Issues
None currently.

## Next Steps
1. Add runtime execution tests (execute IR and verify output)
2. Add test for `?` unwrap operator with Result types
3. Add performance benchmarks
4. Add integration tests combining multiple features
