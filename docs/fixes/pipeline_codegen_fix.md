# Pipeline Codegen Fix - Summary

## Problem
Backward pipeline operator `<|` was parsing correctly but failing at codegen with the error:
```
Type mismatch in result object: function declared return type 'int32' but val field has different type
```

## Root Cause Analysis

### Issue 1: Result Type Unwrapping
When pipeline expressions like `f <| 5` were used as arguments to other functions (e.g., `pass(f <| 5)`), the Result type was not being unwrapped:

- `f(5)` returns `Result<int32>` = `{err:0, val:x}`
- `pass(Result<int32>)` tried to create `{err:0, val:Result<int32>}` 
- But `pass` should receive the unwrapped value and create `{err:0, val:x}`

### Issue 2: Right-Associativity for Backward Pipeline
Backward pipeline chaining `a <| b <| c` was being parsed as `a(b, c)` instead of `a(b(c))`:

- Parser was calling `parseNullCoalesce()` which doesn't capture nested pipelines
- Needed to parse at same precedence level to maintain right-associativity

## Solutions Implemented

### Fix 1: Auto-unwrap Result Types in Function Arguments
**File**: `src/backend/codegen.cpp` (lines ~3777-3800)

When passing arguments to functions, if the argument is a `Result<T>` struct and the parameter expects type `T`, automatically extract the `val` field:

```cpp
// Check if argVal is a Result<T> struct
if (argVal->getType()->isStructTy()) {
    auto* argStructType = cast<StructType>(argVal->getType());
    // Check if this is a Result struct: {i8, T}
    if (argStructType->getNumElements() == 2 &&
        argStructType->getElementType(0)->isIntegerTy(8)) {
        Type* valFieldType = argStructType->getElementType(1);
        if (valFieldType == expectedType) {
            // Auto-unwrap: extract the val field (index 1)
            argVal = ctx.builder->CreateExtractValue(
                argVal, 1, "auto_unwrap_val");
        }
    }
}
```

### Fix 2: Auto-unwrap Result Types in Return Statements
**File**: `src/backend/codegen.cpp` (lines ~4595-4620)

When constructing Result objects (from `pass(expr)` or `return {err:0, val:expr}`), if the val field is itself a Result<T> and the expected type is T, auto-unwrap it:

```cpp
// If valField is a Result<T> struct and expectedValType is T
if (valField->getType()->isStructTy()) {
    auto* valStructType = cast<StructType>(valField->getType());
    if (valStructType->getNumElements() == 2 &&
        valStructType->getElementType(0)->isIntegerTy(8)) {
        Type* innerValType = valStructType->getElementType(1);
        if (innerValType == expectedValType) {
            // Auto-unwrap: extract the inner val field (index 1)
            valField = ctx.builder->CreateExtractValue(
                valField, 1, "auto_unwrap_result_val");
        }
    }
}
```

### Fix 3: Right-Associative Backward Pipeline
**File**: `src/frontend/parser.cpp` (lines ~956-980)

Changed backward pipeline parsing to be right-associative by:
1. Parsing the right side with `parsePipeline()` instead of `parseNullCoalesce()`
2. Returning immediately after creating the CallExpr (no loop continuation)

```cpp
else if (match(TOKEN_PIPE_BACKWARD)) {
    // f <| x means f(x)
    // NOTE: Backward pipeline is RIGHT-ASSOCIATIVE
    // So f <| g <| x parses as f <| (g <| x) = f(g(x))
    auto arg_expr = parsePipeline();
    
    if (auto* var = dynamic_cast<VarExpr*>(left.get())) {
        auto call = std::make_unique<CallExpr>(var->name);
        call->arguments.push_back(std::move(arg_expr));
        return call;  // Return immediately for right-associativity
    }
    // ... similar for other cases
}
```

## Test Results

All pipeline functionality now works correctly:

✅ Simple forward pipeline: `5 |> f`  
✅ Simple backward pipeline: `f <| 5`  
✅ Forward chaining: `5 |> f |> g`  
✅ Backward chaining: `f <| g <| 5`  
✅ Nested in function calls: `pass(f <| 5)`  
✅ Mixed with regular calls: `f(g <| 5)`  

## Files Modified

1. `src/backend/codegen.cpp` - Added Result type auto-unwrapping (2 locations)
2. `src/frontend/parser.cpp` - Fixed backward pipeline right-associativity

## Backup Files Created

- `src/backend/codegen.cpp.pre_unwrap` - Backup before first fix
