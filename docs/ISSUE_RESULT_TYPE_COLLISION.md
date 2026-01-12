# Issue: 'result' Type Name Collision

## Problem

The `result` type causes variable name collision, preventing developers from using the natural variable name `result` in their code.

### Manifestation
When a variable named `result` is used in functions with while loops, calling the function multiple times causes infinite hang:

```aria
pub func:factorial_i32 = int32(int32:n) {
    int32:result = 1;  // ❌ Collides with internal 'result' type
    int32:i = 2;
    
    while (i <= n) {
        result = (result * i);
        i = (i + 1);
    }
    
    pass(result);
};

// First call: Works
// Second call: Infinite hang
```

### Root Cause
- `result` is a core type in Aria (like `int32`, `string`)
- Codegen appears to use "result" internally as temporary variable name
- When user code also uses `result`, namespace collision occurs
- Collision particularly affects functions with loops (codegen issue)

## Solution

**Rename the type from `result` to `Result` (capitalize)**

### Benefits
1. **Frees up natural variable name**: Developers can use `result` without collision
2. **Follows convention**: Capitalized types match Go, Rust, Swift conventions
3. **Clear distinction**: `Result` (type) vs `result` (variable) visually distinct
4. **Minimal change**: ~15 lines across codebase

### Implementation

#### 1. Lexer (src/frontend/lexer/lexer.cpp)
```cpp
// Change line 122:
{"result", TokenType::TOKEN_KW_RESULT},
// To:
{"Result", TokenType::TOKEN_KW_RESULT},
```

#### 2. Type System (src/frontend/sema/type_checker.cpp)
```cpp
// Update string comparisons (3 locations):
"result" → "Result"
```

#### 3. Codegen (src/backend/ir/codegen_expr.cpp)
```cpp
// Line 263:
if (typeName == "result") return llvm::PointerType::get(context, 0);
// To:
if (typeName == "Result") return llvm::PointerType::get(context, 0);
```

#### 4. Documentation (docs/info/aria_specs.txt)
```
Line 60: result, → Result,
Line 98: use result type → use Result type
Line 106: result() → Result()
Line 113: result type → Result type
etc.
```

#### 5. Standard Library
Search for any uses of `result` as a type and update to `Result`

### Before/After

**Before:**
```aria
result:output = someFunc();  // ❌ 'result' is type, can't use as variable
int32:result = 5;             // ❌ Collision!
```

**After:**
```aria
Result:output = someFunc();  // ✅ Result is the type
int32:result = 5;             // ✅ result is just a variable, no collision!
```

## Testing

After implementation:
1. Verify all existing Result type usage compiles
2. Test variable named `result` in functions with loops
3. Verify multiple function calls work correctly
4. Run full stdlib test suite

## Impact

**Breaking Change:** Yes (minor)
- Code using `result` as type must change to `Result`
- Only affects code using error handling (not widely used yet)
- Easy migration: find/replace `result:` → `Result:`

**Compatibility:**
- Could support both `result` and `Result` temporarily with deprecation warning
- Phase out lowercase version in next major release

## Files to Modify

1. `src/frontend/lexer/lexer.cpp` - Keyword definition
2. `src/frontend/lexer/token.cpp` - Token name display  
3. `src/frontend/preprocessor/preprocessor.cpp` - Reserved words list
4. `src/frontend/sema/type_checker.cpp` - Type name checks (3 locations)
5. `src/backend/ir/codegen_expr.cpp` - Type mapping
6. `src/backend/ir/ir_generator.cpp` - Already uses TypeKind::RESULT (no change needed)
7. `src/frontend/sema/type.cpp` - TypeKind comparison (no change needed)
8. `docs/info/aria_specs.txt` - Documentation updates (~20 occurrences)
9. Any stdlib files using `result` type (search and update)

## Estimated Effort

- Code changes: 30 minutes
- Testing: 1 hour
- Documentation: 30 minutes
- **Total: ~2 hours**

## Priority

**HIGH** - Blocks natural variable naming, causes hard-to-debug runtime hangs

## Workaround (Temporary)

Until fix is implemented, avoid using `result` as a variable name in any function with loops:
```aria
// Instead of:
int32:result = something;

// Use:
int32:ret_val = something;
int32:output = something;
int32:value = something;
```

## Status

- [x] Issue identified (Jan 6, 2026)
- [x] Solution designed (Jan 6, 2026)
- [ ] Implementation
- [ ] Testing
- [ ] Documentation update
- [ ] Release

## Notes

This is a simple, elegant solution that improves developer experience significantly. The capitalized type name `Result` better matches industry conventions and provides clear visual distinction from the variable name `result`.
