# Work Package 002: Closure Support Implementation Plan

## Status Discovery (December 8, 2025)

### What Already Works ✅
1. **Function types as first-class values**: Can assign functions to variables
2. **Calling through function variables**: Optimized to direct calls at compile-time
3. **Module-level variable capture**: Lambdas can reference globals
4. **Lambda expressions**: Basic syntax and code generation works
5. **Type checker**: Fixed to properly scope lambda parameters

### What Doesn't Work ❌
**Local Variable Capture** - Critical Missing Feature

Current behavior:
```llvm
define internal %result_func @makeAdder(i32 %x) {
  %x1 = alloca i32, align 4    ; x stored in makeAdder's stack
  store i32 %x, ptr %x1, align 4
  ...
  store ptr @adder, ptr %val_ptr  ; Return function pointer
}

define internal %result_int32 @adder(i32 %y) {
  %0 = load i32, ptr %x1, align 4  ; ❌ ILLEGAL: %x1 is in different function!
}
```

**LLVM Error**: "Referring to an instruction in another function"

## Implementation Requirements

### Phase 1: Capture Analysis (Semantic Analysis)
Location: `src/frontend/sema/type_checker.cpp`

Need to add:
```cpp
struct CaptureInfo {
    std::set<std::string> captured_vars;  // Variables from parent scopes
    bool needs_heap_env;                   // True if captures non-globals
};

// In LambdaExpr visitor:
// 1. Walk lambda body AST
// 2. For each VarExpr, check if it's:
//    - Local parameter: OK, no capture
//    - Global: OK, load from global
//    - Parent scope local: CAPTURE - add to captured_vars
// 3. Store CaptureInfo in lambda node
```

### Phase 2: Environment Generation (CodeGen)
Location: `src/backend/codegen.cpp`

Need to implement:

**2.1 Environment Struct Type**
```cpp
// For lambda capturing {x: int32, y: int64}:
%env_adder = type { i32, i64 }
```

**2.2 Environment Allocation**
```llvm
; In makeAdder:
%env = call ptr @malloc(i64 16)  ; sizeof(env_adder)
%x_ptr = getelementptr %env_adder, ptr %env, i32 0, i32 0
store i32 %x, ptr %x_ptr
```

**2.3 Fat Pointer Creation**
```cpp
// Instead of returning just function pointer:
struct FatPointer {
    void* func_ptr;
    void* env_ptr;
};

%result_func = type { i8, {ptr, ptr} }  ; {err, {func, env}}
```

**2.4 Lambda Signature Change**
```llvm
; Old: define %result_int32 @adder(i32 %y)
; New: define %result_int32 @adder(ptr %env, i32 %y)
;                                  ^^^^ hidden first parameter
```

**2.5 Environment Access in Lambda**
```llvm
define %result_int32 @adder(ptr %env, i32 %y) {
  %env_typed = bitcast ptr %env to ptr %env_adder
  %x_ptr = getelementptr %env_adder, ptr %env_typed, i32 0, i32 0
  %x = load i32, ptr %x_ptr  ; ✅ LEGAL: loads from heap
  ...
}
```

**2.6 Call Site Updates**
```llvm
; When calling through fat pointer:
%fat_ptr = ...  ; {func_ptr, env_ptr}
%func = extractvalue {ptr, ptr} %fat_ptr, 0
%env = extractvalue {ptr, ptr} %fat_ptr, 1
%result = call %result_int32 %func(ptr %env, i32 5)
;                                   ^^^^ pass environment
```

### Phase 3: Parser Support (Optional)
Currently function variables work through type inference. Explicit syntax not needed yet:
```aria
// This already works:
func:add = int32(int32:a, int32:b) { ... };
func:myAdd = add;  // Type inferred

// This could be added later if needed:
func<int32(int32, int32)>:myAdd = add;
```

## Implementation Order

1. **Capture Analysis in Type Checker** (2-3 hours)
   - Add CaptureInfo struct to AST nodes
   - Implement variable scope analysis in lambda visitor
   - Mark which variables need heap allocation

2. **Environment Codegen** (4-6 hours)
   - Generate environment struct types
   - Malloc environment in parent function
   - Store captured variables into environment
   - Return fat pointer {func, env}

3. **Lambda Codegen Updates** (2-3 hours)
   - Add hidden env parameter to lambda functions
   - Load captured variables from env instead of parent scope
   - Update VarExpr visitor to handle captured variables

4. **Call Site Updates** (2-3 hours)
   - Detect calls through fat pointers vs direct calls
   - Extract and pass environment pointer
   - Update CallExpr visitor

5. **Testing & Validation** (2-3 hours)
   - test_closure_local_capture.aria should compile
   - Test nested closures
   - Test multiple captured variables
   - Test returning closures

**Total Estimate**: 12-18 hours of focused work

## Testing Strategy

### Test 1: Basic Local Capture
```aria
func:makeAdder = func(int32:x) {
    func:adder = int32(int32:y) {
        pass(x + y);  // Captures x
    };
    pass(adder);
};
```

### Test 2: Multiple Captures
```aria
func:makeMultiplier = func(int32:a, int32:b) {
    func:multiply = int32(int32:x) {
        pass(a * x + b);  // Captures both a and b
    };
    pass(multiply);
};
```

### Test 3: Nested Closures
```aria
func:outer = func(int32:x) {
    func:middle = func(int32:y) {
        func:inner = int32(int32:z) {
            pass(x + y + z);  // Captures x and y
        };
        pass(inner);
    };
    pass(middle);
};
```

## Current Blockers

❌ **Capture analysis not implemented** - Can't detect which variables need heap allocation
❌ **Environment codegen not implemented** - Can't generate heap-allocated closure environments  
❌ **Fat pointer support incomplete** - Return type exists but call sites don't use it

## Next Steps

Ready to implement Phase 1 (Capture Analysis). This is the foundation - without knowing what variables are captured, we can't generate proper environments.
