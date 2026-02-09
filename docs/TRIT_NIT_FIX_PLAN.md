# CRITICAL FIX PLAN - trit/nit Codegen

**Status**: Ready to implement  
**Files to Modify**: 1 file only - `src/backend/ir/ternary_codegen.cpp`  
**Estimated Time**: 2-3 hours  
**Risk Level**: MEDIUM (well-defined changes, clear test validation)

---

## ✅ GOOD NEWS

**Runtime Library: 100% CORRECT**
- ✅ GOTCHA #3: Verified - Returns ERR on overflow (line 467 aria_nit_add)
- ✅ Sticky ERR propagation in all functions
- ✅ LUT initialization correct
- ✅ Split-Byte and Biased-Radix packing correct
- ✅ All 40+ intrinsics implemented
- ✅ No error swallowing

**Only 1 File Needs Fixes**: `src/backend/ir/ternary_codegen.cpp`

---

## 🔧 FIXES REQUIRED

### Fix 1: Replace LLVM Arithmetic with Runtime Calls (6 functions)

**Pattern**: Change from `CreateAdd/Sub/Mul` → `CreateCall(aria_trit_*/aria_nit_*)`

**Functions to Fix**:
1. `generateAdd()` - Line 131
2. `generateSub()` - Line 149  
3. `generateMul()` - Line 177
4. `generateDiv()` - Line 205
5. `generateMod()` - Line 257
6. `generateNeg()` - Line 337

**Change Pattern Example** (generateAdd):

**BEFORE**:
```cpp
// Atomic type (trit/nit): inline LLVM operation with clamping
llvm::Value* result = builder.CreateAdd(lhs, rhs, "add_tmp");
return clampToRange(result, type);
```

**AFTER**:
```cpp
// Atomic type (trit/nit): call runtime for sticky ERR propagation
std::string typeName = type->toString();
llvm::Function* addFn = nullptr;

if (typeName == "trit") {
    if (!fn_trit_add) {
        fn_trit_add = getOrDeclareIntrinsic("aria_trit_add", true);
    }
    addFn = fn_trit_add;
} else if (typeName == "nit") {
    if (!fn_nit_add) {
        fn_nit_add = getOrDeclareIntrinsic("aria_nit_add", true);
    }
    addFn = fn_nit_add;
}

if (addFn) {
    llvm::Type* i8 = builder.getInt8Ty();
    if (lhs->getType() != i8) {
        lhs = builder.CreateIntCast(lhs, i8, true, "atomic_cast_lhs");
    }
    if (rhs->getType() != i8) {
        rhs = builder.CreateIntCast(rhs, i8, true, "atomic_cast_rhs");
    }
    return builder.CreateCall(addFn, {lhs, rhs}, typeName + "_add");
}

// Should not reach here for exotic types
llvm::errs() << "ERROR: No runtime function for type: " << typeName << "\n";
return nullptr;
```

---

### Fix 2: Implement Kleene Logic Operations (3 NEW functions)

**Add to Class** (in `include/backend/ir/ternary_codegen.h`):
```cpp
public:
    llvm::Value* generateAnd(llvm::Value* lhs, llvm::Value* rhs, Type* type);
    llvm::Value* generateOr(llvm::Value* lhs, llvm::Value* rhs, Type* type);
    llvm::Value* generateNot(llvm::Value* operand, Type* type);

private:
    llvm::Function* fn_trit_add = nullptr;
    llvm::Function* fn_trit_sub = nullptr;
    llvm::Function* fn_trit_mul = nullptr;
    llvm::Function* fn_trit_div = nullptr;
    llvm::Function* fn_trit_and = nullptr;  // NEW
    llvm::Function* fn_trit_or = nullptr;   // NEW
    llvm::Function* fn_trit_not = nullptr;  // NEW
    
    llvm::Function* fn_nit_add = nullptr;
    llvm::Function* fn_nit_sub = nullptr;
    llvm::Function* fn_nit_mul = nullptr;
    llvm::Function* fn_nit_div = nullptr;
    llvm::Function* fn_nit_and = nullptr;   // NEW
    llvm::Function* fn_nit_or = nullptr;    // NEW
    
    // ... existing members ...
};
```

**Implement in ternary_codegen.cpp**:
```cpp
llvm::Value* TernaryCodegen::generateAnd(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    std::string typeName = type->toString();
    llvm::Function* andFn = nullptr;
    
    if (typeName == "trit") {
        if (!fn_trit_and) {
            fn_trit_and = getOrDeclareIntrinsic("aria_trit_and", true);
        }
        andFn = fn_trit_and;
    } else if (typeName == "nit") {
        if (!fn_nit_and) {
            fn_nit_and = getOrDeclareIntrinsic("aria_nit_and", true);
        }
        andFn = fn_nit_and;
    }
    
    if (andFn) {
        llvm::Type* i8 = builder.getInt8Ty();
        if (lhs->getType() != i8) {
            lhs = builder.CreateIntCast(lhs, i8, true, "trit_cast_lhs");
        }
        if (rhs->getType() != i8) {
            rhs = builder.CreateIntCast(rhs, i8, true, "trit_cast_rhs");
        }
        return builder.CreateCall(andFn, {lhs, rhs}, typeName + "_and");
    }
    
    llvm::errs() << "ERROR: No AND operation for type: " << typeName << "\n";
    return nullptr;
}

llvm::Value* TernaryCodegen::generateOr(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    // Same pattern as AND, but call aria_trit_or / aria_nit_or
    // ... (full implementation in Fix 2b)
}

llvm::Value* TernaryCodegen::generateNot(llvm::Value* operand, Type* type) {
    // Call aria_trit_not for trit, aria_nit_neg for nit (neg == NOT in Kleene)
    // ... (full implementation in Fix 2c)
}
```

---

### Fix 3: Wire Logic Ops into ExprCodegen

**File**: `src/backend/ir/codegen_expr.cpp`

**Find**: `ExprCodegen::visitBinaryOp()` and add logic op handling

**Add handling for**:
- `TokenType::And` → call `ternaryCodegen->generateAnd()`
- `TokenType::Or` → call `ternaryCodegen->generateOr()`

**Find**: ` ExprCodegen::visitUnaryOp()` and add:
- `TokenType::Not` → call `ternaryCodegen->generateNot()`

---

## 📋 Implementation Sequence

### Step 1: Update Header (5 min)
- [ ] Add generateAnd/Or/Not declarations
- [ ] Add fn_trit_and/or/not, fn_nit_and/or member variables
- [ ] Save and verify no syntax errors

### Step 2: Fix Arithmetic Operations (30 min)
- [ ] Fix generateAdd() - replace CreateAdd with runtime call
- [ ] Fix generateSub() - replace CreateSub with runtime call
- [ ] Fix generateMul() - replace CreateMul with runtime call
- [ ] Fix generateDiv() - replace CreateSDiv with runtime call
- [ ] Fix generateMod() - replace CreateSRem with runtime call
- [ ] Fix generateNeg() - replace CreateNeg with runtime call
- [ ] **Test**: Compile aria, verify no build errors

### Step 3: Implement Logic Operations (45 min)
- [ ] Implement generateAnd() - Kleene AND (min)
- [ ] Implement generateOr() - Kleene OR (max)
- [ ] Implement generateNot() - Balanced ternary NOT (neg)
- [ ] **Test**: Compile aria, verify no build errors

### Step 4: Wire into ExprCodegen (30 min)
- [ ] Add logic op routing in visitBinaryOp()
- [ ] Add NOT routing in visitUnaryOp()
- [ ] **Test**: Compile aria, verify no build errors

### Step 5: Compilation Test (15 min)
- [ ] Try compiling batch02_gemini_audit_fixes.aria
- [ ] Check for "undefined reference" errors
- [ ] Verify IR contains calls to aria_trit_*, NOT CreateAdd/CreateAnd

### Step 6: Runtime Test (30 min)
- [ ] Test: `1T + 1T` → should return ERR
- [ ] Test: `ERR + 1T` → should return ERR (sticky)
- [ ] Test: `1T && (-1T)` → should return -1T (false)
- [ ] Test: `!(0T)` → should return 0T (unknown)
- [ ] Move batch02 to tests/ and run full suite

---

## ⚠️ CRITICAL CHECKS

**Before committing fixes**:
```bash
# 1. Verify NO LLVM arithmetic for exotic types
cd /home/randy/Workspace/REPOS/aria
grep -n "CreateAdd\|CreateSub\|CreateMul\|CreateSDiv\|CreateSRem" src/backend/ir/ternary_codegen.cpp
# Expected: Only in comments or composite type sections, NEVER for atomic types

# 2. Verify NO bitwise logic for exotic types
grep -n "CreateAnd\|CreateOr\|CreateXor" src/backend/ir/ternary_codegen.cpp
# Expected: ZERO matches

# 3. Verify all runtime calls present
grep -n "aria_trit_add\|aria_trit_and\|aria_nit_add\|aria_nit_and" src/backend/ir/ternary_codegen.cpp
# Expected: Multiple matches, all in CreateCall statements

# 4. Build and test
./build.sh && build/ariac tests/batch02_gemini_audit_fixes.aria 2>&1 | grep -i error
# Expected: No "undefined reference" or compilation errors
```

---

## 🎯 Success Metrics

After all fixes:
- ✅ Build completes with 0 errors
- ✅ `grep CreateAdd src/backend/ir/ternary_codegen.cpp` → only composite types or comments
- ✅ `grep CreateAnd src/backend/ir/ternary_codegen.cpp` → 0 matches
- ✅ batch02 compiles successfully
- ✅ Test: Sentinel healing → PASS (ERR + x = ERR)
- ✅ Test: Logic inversion → PASS (True AND False = False)
- ✅ Move batch02 from tests/future/ → tests/
- ✅ All 363 tests passing

---

**Ready to begin?** Start with Step 1 (Update Header).
