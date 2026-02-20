# String Interpolation Segfault Fix - Complete

**Date**: February 12, 2026  
**Status**: ✅ RESOLVED  
**Priority**: P2 (Language-level feature)  

## Problem

Template literals would segfault when preceded by a plain `println` call:

```aria
func:main = int32() {
    int32:x = 42;
    drop(println("Test 1: Plain text"));       // OK
    drop(println(`Test 2: x = &{x}`));          // SEGFAULT
    pass(0);
};
```

## Root Cause

**Type Mismatch in Return Value**

Template literal codegen was returning a `char*` (raw pointer), but `println` expected either:
1. `GlobalVariable*` pointing to an AriaString struct (for string literals)
2. `AriaString*` pointer (struct pointer)

The mismatch caused `println` to incorrectly interpret the `char*` as an `AriaString*` pointer, leading to:
- Loading from an invalid address
- Extracting garbage values
- Segmentation fault

### Code Flow Analysis

**String Literals** (working):
```cpp
// codegen_expr.cpp:1341-1351
llvm::GlobalVariable* string_gv = new llvm::GlobalVariable(...);
return string_gv;  // Returns GlobalVariable* to AriaString struct
```

**Template Literals** (broken):
```cpp
// Original code (WRONG):
llvm::Value* resultPtr = builder.CreateCall(concatNFn, ...);  // Returns AriaString*
llvm::Value* resultStruct = builder.CreateLoad(ariaStringType, resultPtr);
return builder.CreateExtractValue(resultStruct, 0);  // Returns char* (field 0)
```

**println Processing** (codegen_expr.cpp:3663-3672):
```cpp
else if (arg->getType()->isPointerTy()) {
    llvm::StructType* ariaStringType = llvm::StructType::getTypeByName(context, "struct.AriaString");
    if (ariaStringType) {
        // Expects AriaString* - loads struct and extracts data field
        llvm::Value* str_struct = builder.CreateLoad(ariaStringType, arg, "str_struct");
        str_ptr = builder.CreateExtractValue(str_struct, 0, "str_data");
    }
}
```

**When arg is char\* instead of AriaString\***:
- `CreateLoad(ariaStringType, char*)` tries to load 16 bytes from a string pointer
- The string pointer itself is often in low memory (GC-allocated)
- Loading 16 bytes from arbitrary string data as a struct = garbage
- Extracting field 0 of garbage = invalid pointer
- Using invalid pointer = segfault

## Solution

Return `AriaString*` pointers from template literal codegen to match string literal behavior.

### Fix #1: Non-Empty Template Literals

**Location**: codegen_expr.cpp:2015-2024  
**Change**: Return the pointer directly instead of loading and extracting

```cpp
// BEFORE (WRONG):
llvm::Value* resultPtr = builder.CreateCall(concatNFn, { arrayPtr, count });
llvm::Value* resultStruct = builder.CreateLoad(ariaStringType, resultPtr);
return builder.CreateExtractValue(resultStruct, 0);  // char* - WRONG!

// AFTER (CORRECT):
llvm::Value* resultPtr = builder.CreateCall(concatNFn, { arrayPtr, count });
return resultPtr;  // AriaString* - matches string literal behavior
```

### Fix #2: Empty Template Literals

**Location**: codegen_expr.cpp:1758-1760  
**Change**: Allocate on GC heap and return pointer

```cpp
// BEFORE (WRONG):
llvm::Value* emptyStr = createAriaString("");
return builder.CreateExtractValue(emptyStr, 0);  // char* - WRONG!

// AFTER (CORRECT):
llvm::Value* emptyStr = createAriaString("");

// Allocate AriaString struct on GC heap
llvm::FunctionCallee gcAllocCallee = module->getOrInsertFunction("aria_gc_alloc", ...);
llvm::Value* structSize = llvm::ConstantInt::get(..., 16); // sizeof(AriaString)
llvm::Value* heapPtr = builder.CreateCall(gcAlloc, { structSize });

// Cast and store
llvm::Value* strPtr = builder.CreateBitCast(heapPtr, llvm::PointerType::get(ariaStringType, 0));
builder.CreateStore(emptyStr, strPtr);

return strPtr;  // AriaString* - matches string literal behavior
```

## Files Modified

1. **src/backend/ir/codegen_expr.cpp**:
   - Line 1758-1777: Empty template literal return (18 lines changed)
   - Line 2015-2024: Non-empty template literal return (10 lines removed)
   - Total: ~28 line delta

## Testing

### Test Results

**test_string_interp_bug.aria**: ✅ PASS
```
Test 1: Plain text
Test 2: x = 42
```

**test_string_interp_comprehensive.aria**: ✅ PASS
```
Test 1: Plain text
Test 2: x = 42
Test 3: x = 42, y = 100
Test 4: x = 42
Test 5: Plain text again
Test 6: x = 42
Test 7: y = 100
(empty line for empty template)
Test 9: No interpolation
Test 10: x + y = 142
```

### Edge Cases Verified

- ✅ Plain text followed by template literal
- ✅ Template literal followed by plain text
- ✅ Multiple interpolations in one template
- ✅ Multiple template literals in sequence
- ✅ Empty template literals
- ✅ Template literals with no interpolation
- ✅ Complex expressions in interpolation

## Technical Notes

### Why GC Allocation for Empty Templates?

Empty template literals need heap allocation because:
1. Stack allocation would be freed when function returns (use-after-return)
2. Global variables would require unique names (complexity)
3. GC heap matches the pattern used by `aria_string_concat_n_simple`

Consistency: All template literal results are now GC-allocated AriaString structs.

### Size: sizeof(AriaString)

```c
struct AriaString {
    const char* data;  // 8 bytes (64-bit)
    int64_t length;    // 8 bytes
};
// Total: 16 bytes
```

### Memory Safety

Both fixes use GC allocation:
- Empty templates: Explicit `aria_gc_alloc` call
- Non-empty templates: `aria_string_concat_n_simple` uses GC internally

GC lifetime management ensures strings remain valid until no longer referenced.

## Impact

**User-Facing**:
- "Hello World" examples now work reliably
- Template literals can be used anywhere string literals work
- No more mysterious segfaults in basic I/O

**Technical**:
- Consistent type handling across string types
- Enables future optimizations (string interning, etc.)
- Foundation for more complex string features

## Related Issues

This fix enables:
- P2.7: Complex struct initialization (can now test with string fields)
- Future formatter system (consistent string return types)
- FFI string marshaling (clear pointer semantics)

## Lessons Learned

1. **Type consistency is critical**: Different code paths must return the same type
2. **Match existing patterns**: Template literals should work like string literals
3. **Test edge cases early**: Empty template literal exposed the same bug
4. **Root causes aren't always obvious**: Initial hypothesis (stack corruption) was wrong

The actual issue was a type protocol mismatch, not stack corruption. The fix was to follow the established pattern (return pointers, not extract values).
