# Aria v0.0.6 - Implementation Status Check
**Date:** December 4, 2025  
**Purpose:** Systematic check of what's implemented vs spec

## Summary

**✅ WORKING (10 features):**
- While loops
- For loops  
- Till loops
- When loops
- Defer statements
- Lambda expressions
- Function declarations
- Template strings (string interpolation)
- Result types
- Basic variable declarations

**❌ BROKEN (4 features):**
1. **Pick statements** - Return type mismatch (returns i64 instead of result_int8)
2. **Member access** - Return type mismatch (returns i64 instead of result_int8)
3. **Unwrap operator** - Return type mismatch (returns raw types instead of result)
4. **Arrays** - Parse error (token mismatch)

## ROOT CAUSE ANALYSIS

### Issue #1: Auto-Wrap Not Working for Pick/Member/Unwrap
**Problem:** Functions declared to return `result<int8>` are returning raw values (i64, i8) instead of wrapped result structs.

**Error Pattern:**
```
Function return type does not match operand type of return inst!
  ret i64 0
 %result_int8 = type { i8, i8 }
```

**Root Cause:** The `auto_wrap` feature that automatically wraps return values in result structs is not being applied in:
- Pick statement return paths
- Member access expressions  
- Unwrap operator results

**Fix Needed:** Ensure all return paths through these constructs check `ctx.currentFunctionAutoWrap` and wrap the value in a result struct before returning.

### Issue #2: Array Literal Parsing
**Problem:** Parser fails on array literal syntax `int8[]:arr3 = [100, 300, 550];`

**Error:** `Parse Error: Expected token type 9 but got 88 at line 13, col 14`

**Root Cause:** Array literal parsing (TOKEN_LBRACKET followed by comma-separated values) may not be implemented or has syntax mismatch.

**Fix Needed:** Implement or fix `parseArrayLiteral()` in parser.

## Detailed Test Results

### ✅ WORKING FEATURES

#### Loops (All 4 types work!)
```bash
Testing While Loop... ✅ WORKS
Testing For Loop... ✅ WORKS  
Testing Till Loop... ✅ WORKS
Testing When Loop... ✅ WORKS
```

#### Functions & Control Flow
```bash
Testing Defer... ✅ WORKS
Testing Lambda... ✅ WORKS
Testing Function Decl... ✅ WORKS
Testing Template String... ✅ WORKS
Testing Result Type... ✅ WORKS
```

### ❌ BROKEN FEATURES

#### 1. Pick (Pattern Matching)
```bash
Testing Pick (Pattern Match)... ❌ BROKEN
```
**Error:**
```
Function return type does not match operand type of return inst!
  ret i64 0
 %result_int8 = type { i8, i8 }
```
**File:** `tests/test_pick_simple.aria`

#### 2. Member Access  
```bash
Testing Member Access... ❌ BROKEN
```
**Error:**
```
Function return type does not match operand type of return inst!
  ret i64 %addtmp
 %result_int8 = type { i8, i8 }
```
**File:** `tests/test_member_access.aria`

#### 3. Unwrap Operator
```bash
Testing Unwrap Operator... ❌ BROKEN
```
**Error:**
```
Function return type does not match operand type of return inst!
  ret i64 42
 %result_int8 = type { i8, i8 }
Function return type does not match operand type of return inst!
  ret i8 %0
 %result_int8 = type { i8, i8 }
Function return type does not match operand type of return inst!
  ret i8 %unwrap_result
 %result_int8 = type { i8, i8 }
```
**File:** `tests/test_unwrap_minimal.aria`

#### 4. Arrays
```bash
Testing Arrays... ❌ BROKEN
```
**Error:**
```
Parse Error: Expected token type 9 but got 88 at line 13, col 14
```
**File:** `tests/test_arrays.aria`

## Implementation Gaps vs Spec

### From Aria v0.0.6 Specs.txt

**✅ IMPLEMENTED:**
- [x] Basic types (int8, int16, int32, int64, uint*, bool)
- [x] Result wrapper type
- [x] Variable declarations (type:name = value)
- [x] Functions (fn name(params) -> type { })
- [x] Lambda expressions
- [x] While loops
- [x] For loops  
- [x] Till loops (with $ iterator variable)
- [x] When/Then/End loops
- [x] Defer statements
- [x] Template strings with &{expr} interpolation
- [x] Basic operators (+, -, *, /, ==, !=, <, >, etc.)

**❌ NOT IMPLEMENTED / BROKEN:**
- [ ] Pick statement auto-wrap (partially working, breaks on return)
- [ ] Member access operator (.) on result types
- [ ] Unwrap operator (?) for result types
- [ ] Array literals [val1, val2, val3]
- [ ] Array indexing arr[index]
- [ ] Object literals { key: value }
- [ ] Destructuring patterns in pick
- [ ] Break/Continue statements (parsed but not codegen)
- [ ] Exotic types (trit, tryte, nit, nyte)
- [ ] Memory allocation keywords (wild, gc, stack - partially done)
- [ ] Pinning operator (#)
- [ ] Safe reference operator ($)
- [ ] Pipeline operators (|>, <|)
- [ ] Spaceship operator (<=>)
- [ ] Null coalescing (??)
- [ ] Safe navigation (?.)
- [ ] Async/await
- [ ] Module system (use, mod, pub, extern)
- [ ] Comptime evaluation
- [ ] NASM-style macros (preprocessor done, not integrated with comptime)
- [ ] Standard library functions

## Recommended Fix Priority

### HIGH PRIORITY (Blocks basic programs)
1. **Fix auto-wrap in Pick statements** - Pattern matching is core feature
2. **Fix auto-wrap in Return paths** - All functions use result types
3. **Implement array literals** - Arrays are fundamental

### MEDIUM PRIORITY (Important but workarounds exist)
4. **Fix member access on results** - Can use destructuring instead
5. **Fix unwrap operator** - Can manually check err field
6. **Implement break/continue codegen** - Loops work without them
7. **Implement array indexing** - Can use helper functions

### LOW PRIORITY (Advanced features)
8. Object literals
9. Destructuring in pick
10. Exotic types (trit/nyte)
11. Advanced operators (pipeline, spaceship, safe nav)
12. Module system
13. Async/await
14. Comptime integration

## Next Steps

### Immediate (Fix the 4 broken tests)

**Step 1: Fix Auto-Wrap in Pick Statements**
- File: `src/backend/codegen.cpp`
- Function: `visit(PickStmt*)` 
- Add auto-wrap check before all return instructions in pick cases

**Step 2: Fix Auto-Wrap in Return Statements**
- File: `src/backend/codegen.cpp`
- Function: `visit(ReturnStmt*)`
- Ensure auto-wrap is applied to all return values

**Step 3: Implement Array Literal Parsing**
- File: `src/frontend/parser.cpp`
- Add: `parseArrayLiteral()` method
- Handle: `[expr1, expr2, ..., exprN]` syntax

**Step 4: Implement Array Literal Codegen**
- File: `src/backend/codegen.cpp`
- Add: `visit(ArrayLiteral*)` method
- Generate: LLVM array allocation and initialization

### Short Term (Complete basic language features)

- Implement array indexing (both parse + codegen)
- Implement object literals (parse + codegen)
- Add break/continue codegen
- Fix member access operator
- Fix unwrap operator

### Long Term (Advanced features)

- Exotic types (trit/tryte/nit/nyte)
- Complete memory management (wild/gc/stack with borrow checker)
- Module system (use/mod/pub/extern)
- Comptime execution
- Async/await
- Standard library
- Self-hosting (compiler written in Aria)
