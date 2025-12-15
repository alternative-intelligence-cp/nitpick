# Phase 2.3: LLVM IR Lifetime Intrinsics - Implementation Plan

## Current State (v0.0.17)

### Completed in Phase 2.2:
- ✅ Borrow checker integrated with type checker
- ✅ Memory region detection (is_wild, is_stack, requires_drop)
- ✅ AST annotations ready (scope_depth, pinned_target, etc.)
- ✅ Integration tests passing

### Current Codegen Behavior:
- Variables declared in global scope become LLVM `@globalvariables`
- No lifetime intrinsics emitted
- No explicit scope tracking in IR generation
- Borrow checker annotations not yet consumed by codegen

## Phase 2.3 Goals

### Primary Objectives:
1. Emit `llvm.lifetime.start` when variables enter scope
2. Emit `llvm.lifetime.end` when variables exit scope (based on scope_depth)
3. Generate shadow stack for pinned objects (#operator)
4. Use borrow analysis to optimize redundant checks

### LLVM Lifetime Intrinsics Overview:

```llvm
; Declare lifetime intrinsics
declare void @llvm.lifetime.start.p0(i64, ptr nocapture)
declare void @llvm.lifetime.end.p0(i64, ptr nocapture)

; Usage example:
define void @example() {
entry:
  %x = alloca i8, align 1
  call void @llvm.lifetime.start.p0(i64 1, ptr %x)  ; x begins life
  store i8 42, ptr %x
  ; ... use x ...
  call void @llvm.lifetime.end.p0(i64 1, ptr %x)    ; x ends life
  ret void
}
```

### Benefits:
- Stack coloring optimization (reuse stack slots)
- Better alias analysis
- Improved memory usage
- Foundation for stack-to-register promotion
- Explicit lifetime boundaries for debuggers

## Implementation Strategy

### Task 2: Add Lifetime Intrinsics

**File:** `src/backend/codegen.cpp`

**Changes Needed:**

1. **Declare Lifetime Intrinsics** (add to codegen initialization):
```cpp
Function* getOrInsertLifetimeStart() {
    if (Function* existing = ctx.module->getFunction("llvm.lifetime.start.p0")) {
        return existing;
    }
    
    FunctionType* lifetimeType = FunctionType::get(
        Type::getVoidTy(ctx.llvmContext),
        {Type::getInt64Ty(ctx.llvmContext), PointerType::getUnqual(ctx.llvmContext)},
        false
    );
    
    return Function::Create(lifetimeType, Function::ExternalLinkage, 
                          "llvm.lifetime.start.p0", ctx.module.get());
}

Function* getOrInsertLifetimeEnd() {
    // Similar to above but for llvm.lifetime.end.p0
}
```

2. **Emit lifetime.start on Variable Declaration**:
```cpp
// When processing VarDecl node:
void handleVarDecl(VarDecl* node) {
    // Check if we have scope information from borrow checker
    if (node->scope_depth >= 0) {
        // Allocate variable (if not global)
        Value* varPtr = ...;
        
        // Get size of variable type
        Type* varType = ctx.getLLVMType(node->type);
        uint64_t size = ctx.module->getDataLayout().getTypeAllocSize(varType);
        
        // Emit lifetime.start
        Function* lifetimeStart = getOrInsertLifetimeStart();
        ctx.builder->CreateCall(lifetimeStart, {
            ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), size),
            varPtr
        });
        
        // Track variable for lifetime.end emission
        ctx.scopedVariables[node->scope_depth].push_back({varPtr, size, node->name});
    }
}
```

3. **Emit lifetime.end on Scope Exit**:
```cpp
// When exiting a scope (block end, return, etc.):
void emitScopeCleanup(int scopeDepth) {
    auto it = ctx.scopedVariables.find(scopeDepth);
    if (it != ctx.scopedVariables.end()) {
        Function* lifetimeEnd = getOrInsertLifetimeEnd();
        
        // Emit lifetime.end for all variables in this scope (LIFO order)
        for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
            ctx.builder->CreateCall(lifetimeEnd, {
                ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), rit->size),
                rit->ptr
            });
        }
        
        ctx.scopedVariables.erase(it);
    }
}
```

### Task 3: Integrate Borrow Checker scope_depth

**File:** `src/backend/codegen_context.h`

**Add to CodeGenContext:**
```cpp
struct ScopedVariable {
    Value* ptr;
    uint64_t size;
    std::string name;
    int scope_depth;
};

// Map: scope_depth -> list of variables in that scope
std::map<int, std::vector<ScopedVariable>> scopedVariables;

// Current scope depth (incremented on block entry, decremented on exit)
int currentScopeDepth = 0;
```

### Task 4: Shadow Stack for Pinned Objects

**Purpose:** Track pinned objects for GC integration

**Implementation:**
```cpp
// Global shadow stack (array of pointers)
GlobalVariable* shadowStack = new GlobalVariable(
    *ctx.module,
    ArrayType::get(PointerType::getUnqual(ctx.llvmContext), SHADOW_STACK_SIZE),
    false,
    GlobalValue::InternalLinkage,
    ConstantAggregateZero::get(ArrayType::get(...)),
    "aria_shadow_stack"
);

// When # operator is used (pinning):
void handlePinning(VarDecl* pinned, VarDecl* target) {
    // Add target to shadow stack
    Value* stackPtr = getShadowStackTop();
    ctx.builder->CreateStore(target->llvmValue, stackPtr);
    incrementShadowStackTop();
    
    // Mark for removal on scope exit
    ctx.pinnedVariables[pinned->scope_depth].push_back(target);
}
```

### Task 5: Codegen Tests

**Tests to Create:**

1. `test_lifetime_simple.aria` - Basic variable lifetime
2. `test_lifetime_nested_scopes.aria` - Nested block scoping
3. `test_lifetime_loop.aria` - Loop variable lifetimes
4. `test_lifetime_conditional.aria` - If/else branch lifetimes
5. `test_shadow_stack_pinning.aria` - Pinned object tracking

**Validation:**
- Check generated IR contains `llvm.lifetime.start` calls
- Check generated IR contains `llvm.lifetime.end` calls
- Verify lifetimes match scope boundaries
- Verify shadow stack operations for pinned objects

### Task 6: Optimization Based on Borrow Analysis

**Opportunities:**

1. **Eliminate Redundant Checks:**
   - If borrow checker proves no aliasing, skip runtime checks
   - Use `noalias` attribute on function parameters

2. **Stack Slot Reuse:**
   - Lifetime intrinsics enable LLVM to reuse stack space
   - Variables with non-overlapping lifetimes share stack slots

3. **Register Promotion:**
   - Short-lived variables can be kept in registers
   - `llvm.lifetime.*` helps identify candidates

## Dependencies

### Required from Phase 2.1 & 2.2:
- ✅ `VarDecl::scope_depth` (set by borrow checker)
- ✅ `VarDecl::requires_drop` (wild memory flag)
- ✅ `VarDecl::is_pinned_shadow` (pinning flag)
- ✅ `VarDecl::pinned_target` (pinned variable name)

### Codegen Infrastructure Needed:
- Scope depth tracking in CodeGenContext
- Variable lifetime map (scope -> variables)
- Shadow stack global variable
- Lifetime intrinsic declarations

## Testing Strategy

### Unit Tests:
- Test lifetime intrinsic emission
- Test scope depth tracking
- Test shadow stack operations

### Integration Tests:
- Compile test programs and inspect IR
- Verify `llvm.lifetime.start` before first use
- Verify `llvm.lifetime.end` at scope exit
- Check shadow stack for pinned objects

### Validation:
```bash
# Check for lifetime intrinsics in generated IR
./build/ariac test.aria -o test.ll --emit-llvm
grep "llvm.lifetime.start" test.ll
grep "llvm.lifetime.end" test.ll

# Verify IR with LLVM tools
opt -verify test.ll -o /dev/null
```

## Estimated Effort

- **Task 2:** 2-3 days (add lifetime intrinsics)
- **Task 3:** 1-2 days (integrate scope_depth)
- **Task 4:** 2-3 days (shadow stack for pinning)
- **Task 5:** 1-2 days (tests)
- **Task 6:** 2-3 days (optimizations)

**Total:** 8-13 days for complete Phase 2.3

## Next Steps

1. Add lifetime intrinsic declarations to codegen
2. Implement scope tracking in CodeGenContext
3. Emit lifetime.start on VarDecl
4. Emit lifetime.end on scope exit
5. Add shadow stack for pinned objects
6. Create comprehensive tests
7. Document optimizations enabled

## Notes

- Phase 2.3 is a major codegen enhancement (7426-line file)
- Requires careful integration with existing code
- Foundation for future optimizations
- Critical for memory safety guarantees
- Enables stack coloring and other LLVM opts

**Status:** Phase 2.2 Complete ✅ | Phase 2.3 Ready to Begin 🚀
