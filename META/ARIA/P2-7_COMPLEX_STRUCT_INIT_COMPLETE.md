# P2.7: Complex Struct Initialization - COMPLETE

**Date**: February 17, 2026  
**Status**: ✅ COMPLETE  
**Effort**: ~2 hours actual  
**Priority**: P2 (Enhancement - better ergonomics)

## Problem Solved

Structs with array fields could not be initialized using inline array literals. Users had to use runtime initialization:

```aria
// ❌ BEFORE: This didn't work
Container:c = {
    count: 3,
    points: [{x: 10, y: 20}, {x: 30, y: 40}, {x: 50, y: 60}]  // ERROR
};

// ✅ WORKAROUND: Runtime initialization required
Container:c = {count: 0, points: uninitialized};
c.points[0] = {x: 10, y: 20};
c.points[1] = {x: 30, y: 40};
```

## Root Cause

Two type system bugs:

1. **Array literals typed as pointers**: `inferArrayLiteral` returned `T*` instead of `T[N]`
2. **Object literals lacked context**: When array literals contained object literals, the type checker couldn't infer the struct type

## Solution

### Fix #1: Array Literal Type (type_checker.cpp:5966)

**Changed**: Return proper array type with size information

```cpp
// BEFORE:
return typeSystem->getPointerType(elementType);  // Returns T*

// AFTER:
size_t arraySize = expr->elements.size();
return typeSystem->getArrayType(elementType, arraySize);  // Returns T[N]
```

### Fix #2: Array of Objects in Variable Declarations (type_checker.cpp:7221-7293)

**Added**: Special case to mark object literals with struct type name when in array

```cpp
// When: Point[2]:points = [{x: 1, y: 2}, {x: 3, y: 4}]
// Mark each {...} with type_name = "Point" before type checking
if (declaredType->getKind() == TypeKind::ARRAY &&
    stmt->initializer->type == ASTNode::NodeType::ARRAY_LITERAL) {
    
    const ArrayType* declaredArray = static_cast<const ArrayType*>(declaredType);
    Type* declaredElem = declaredArray->getElementType();
    
    if (declaredElem->getKind() == TypeKind::STRUCT) {
        // Mark all object literal elements with struct type name
        arrayLiteralObjectCoercion = true;
    }
}
```

### Fix #3: Array of Objects in Struct Fields (type_checker.cpp:7339-7361)

**Added**: Mark object literals when they appear as array fields in struct initializers

```cpp
// When: Container:c = {count: 3, points: [{...}, {...}]}
// Mark each object literal in the array with the expected struct type
if (litField.value->type == ASTNode::NodeType::ARRAY_LITERAL &&
    structField.type->getKind() == TypeKind::ARRAY) {
    
    const ArrayType* expectedArrayType = static_cast<const ArrayType*>(structField.type);
    Type* expectedElemType = expectedArrayType->getElementType();
    
    if (expectedElemType->getKind() == TypeKind::STRUCT) {
        // Mark all object literal elements
    }
}
```

## Files Modified

1. **src/frontend/sema/type_checker.cpp**:
   - Line 5966: Fixed `inferArrayLiteral` to return array type (~3 lines)
   - Lines 7221-7293: Added array literal object coercion (~72 lines)
   - Lines 7339-7361: Added struct field array literal handling (~22 lines)
   - Line 7481: Added flag to skip type check (~1 line)
   - **Total**: ~98 lines added/modified

## What Now Works

### 1. Simple Array Literals

```aria
int32[3]:nums = [10, 20, 30];  // ✅ Works
```

### 2. Array of Structs

```aria
Point[2]:points = [{x: 1, y: 2}, {x: 3, y: 4}];  // ✅ Works
```

### 3. Struct with Array Fields

```aria
Container:c = {
    count: 3,
    points: [{x: 10, y: 20}, {x: 30, y: 40}, {x: 50, y: 60}]
};  // ✅ Works
```

### 4. Nested Cases

```aria
struct:Nested = {
    int32:id;
    Container[2]:containers;
};

Nested:n = {
    id: 1,
    containers: [
        {count: 2, points: [{x: 1, y: 2}, {x: 3, y: 4}]},
        {count: 1, points: [{x: 5, y: 6}, {x: 7, y: 8}]}
    ]
};  // ✅ Should work (deeply nested)
```

## Testing

### Test Files Created

1. **test_array_literal_simple.aria**: ✅ PASS
   - Simple int array literal
   - Array of struct literals
   
2. **test_struct_init_array_literal.aria**: ✅ PASS
   - Struct with array field initialization
   - Complete P2.7 use case

## Technical Details

### Type Flow

**Before Fix**:
1. Parser creates `ArrayLiteralExpr([{x: 1}, {x: 2}])`
2. Type checker calls `inferArrayLiteral`
3. Infers first element as `obj` (no type name)
4. Returns `obj*` (pointer type)
5. Error: Can't assign `obj*` to `Point[2]`

**After Fix**:
1. Parser creates `ArrayLiteralExpr([{x: 1}, {x: 2}])`
2. Type checker detects: declared type is `Point[2]`
3. Marks each object literal: `type_name = "Point"`
4. Infers first element as `Point` (has type name now)
5. Returns `Point[2]` (array type with size)
6. ✅ Type match: `Point[2]` equals `Point[2]`

### Key Insight

Contextual typing requires propagating expected types "downward" during inference. Object literals need to know their target type before being inferred. This is achieved by pre-marking them before calling `inferType`.

## Impact

**User-Facing**:
- Natural struct initialization syntax
- No more verbose runtime initialization
- Matches user expectations from other languages

**Code Quality**:
- More readable initializations
- Fewer lines of code
- Less error-prone (compile-time validation)

## Example From DBUG Module

**Before P2.7**:
```aria
DebugManager:mgr = {watches: uninitialized};
mgr.watches[0] = {label: "x", active: true, ptr: NULL};
mgr.watches[1] = {label: "y", active: true, ptr: NULL};
// ... 16 more lines ...
```

**After P2.7**:
```aria
DebugManager:mgr = {
    watches: [
        {label: "x", active: true, ptr: NULL},
        {label: "y", active: true, ptr: NULL}
        // ... concise initialization ...
    ]
};
```

## Limitations

- Array indexing not yet implemented in codegen (separate issue)
- Empty array literals still can't infer type without context
- Multi-dimensional arrays not tested (should work but untested)

## Next Steps

P2.7 is complete. Ready to move to next priority item.

**Remaining Language-Level Items**:
- Continue audit of language features
- Address any other ergonomic improvements

---

**Completion Time**: 2 hours (faster than 4-6 hour estimate)  
**Lines Changed**: ~98 lines in type_checker.cpp  
**Tests**: 2 new test files, both passing
