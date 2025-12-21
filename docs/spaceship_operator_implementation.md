# Spaceship Operator (<=>)  Implementation

**Status**: ✅ Complete (Session 29)  
**Date**: December 2024  
**Feature**: Three-way comparison operator

## Overview

The spaceship operator (`<=>`) performs three-way comparison, returning:
- `-1` if left < right
- `0` if left == right  
- `1` if left > right

## Syntax

```aria
int64:a = 5;
int64:b = 3;
int64:result = a <=> b;  // result = 1 (since 5 > 3)
```

## Implementation Details

### Token Definition
- **Token**: `TOKEN_SPACESHIP`
- **Location**: `include/frontend/token.h` line 177
- **Lexeme**: `<=>`

### Lexer Support  
- **File**: `src/frontend/lexer/lexer.cpp`
- **Lines**: 359-361
- **Logic**: Checks for `<` followed by `=` followed by `>`

### Parser Support
- **File**: `src/frontend/parser/parser.cpp`  
- **Line**: 48
- **Precedence**: Level 9 (same as other comparison operators)
- **Associativity**: Left-to-right
- **AST Node**: `BinaryExpr` with `TOKEN_SPACESHIP` operator

### Type Checking
- **Handled by**: Standard binary operator type checking
- **Operand Types**: Must be compatible (both integers, both floats, etc.)
- **Result Type**: `int64`

### IR Generation  
- **File**: `src/backend/ir/ir_generator.cpp`
- **Lines**: ~2371-2395
- **Implementation**: Uses LLVM select instructions

```cpp
// Generate: result = select(lt, -1, select(gt, 1, 0))
llvm::Value* ltCmp = builder.CreateICmpSLT(L, R, "spaceship.lt");
llvm::Value* gtCmp = builder.CreateICmpSGT(L, R, "spaceship.gt");

llvm::Value* negOne = llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), -1);
llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0);
llvm::Value* one = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1);

llvm::Value* gtResult = builder.CreateSelect(gtCmp, one, zero, "spaceship.gt_sel");
llvm::Value* result = builder.CreateSelect(ltCmp, negOne, gtResult, "spaceship.result");
```

### Generated LLVM IR

For `int64:result = a <=> b;` where `a=5, b=3`:

```llvm
%a1 = load i64, ptr %a, align 4
%b2 = load i64, ptr %b, align 4
%spaceship.lt = icmp slt i64 %a1, %b2
%spaceship.gt = icmp sgt i64 %a1, %b2
%spaceship.gt_sel = select i1 %spaceship.gt, i64 1, i64 0
%spaceship.result = select i1 %spaceship.lt, i64 -1, i64 %spaceship.gt_sel
store i64 %spaceship.result, ptr %result, align 4
```

## Bug Fix History

**Initial Issue**: Code was added to `codegen_expr.cpp` but never executed.  
**Root Cause**: `IRGenerator::codegenExpression()` has its own binary operator switch statement that handles all binary operators inline. The `ExprCodegen::codegenBinary()` is only used for certain advanced expression types, not basic binary operators.  
**Solution**: Added `TOKEN_SPACESHIP` case to the binary operator switch in `ir_generator.cpp` at line ~2371.

## Test Coverage

### Test Files
1. **tests/spaceship_minimal.aria** - Basic functionality
   ```aria
   func:test = int64() {
       int64:a = 5;
       int64:b = 3;
       int64:result = a <=> b;  // result = 1
       pass(result);
   };
   ```

2. **tests/spaceship_basic.aria** - Multiple comparisons
   ```aria
   func:test_spaceship = int64() {
       int64:x = 5;
       int64:y = 3;
       int64:z = 5;
       
       int64:result1 = x <=> y;  // 1 (x > y)
       int64:result2 = y <=> x;  // -1 (y < x)
       int64:result3 = x <=> z;  // 0 (x == z)
       
       pass(result1, result2, result3);
   };
   ```

### Verification
All tests compile successfully and generate correct LLVM IR with:
- Proper comparison instructions (`icmp slt`, `icmp sgt`)
- Correct select logic
- Appropriate value naming for debugging

## Use Cases

1. **Sorting Algorithms**: Custom comparators can return spaceship result
2. **Database Queries**: Three-way comparison for range queries
3. **Binary Search**: Simplified comparison logic
4. **Data Structure Ordering**: Balanced trees, heaps, etc.

## Future Enhancements

- [ ] Float spaceship with NaN handling
- [ ] String spaceship for lexicographic comparison
- [ ] Struct spaceship with member-wise comparison
- [ ] Generic spaceship for custom types

## References

- C++20 Spaceship Operator (`operator<=>`)
- Rust `Ordering` enum (`Less`, `Equal`, `Greater`)
- Aria Token Specification: `include/frontend/token.h`
