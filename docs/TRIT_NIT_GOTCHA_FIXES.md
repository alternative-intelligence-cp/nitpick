# trit/nit GOTCHA Validation & Fixes

**Date**: February 5, 2026  
**Status**: 🚨 CRITICAL BUGS FOUND - MUST FIX BEFORE PRODUCTION  
**Files Affected**: `src/backend/ir/ternary_codegen.cpp`, `src/backend/runtime/ternary_ops.cpp`

---

## 🔍 Validation Results

### ✅ GOOD NEWS: Infrastructure Exists
- Type system registration complete (PrimitiveType for trit/nit/tryte/nyte)
- Runtime library implemented (666 lines in ternary_ops.cpp)
- Header complete (ternary_ops.h with 40+ intrinsics)
- LUT initialization exists
- Split-Byte and Biased-Radix packing implemented
- Codegen framework exists (ternary_codegen.cpp)

### 🚨 CRITICAL ISSUES FOUND

#### GOTCHA #1: Backend Bypass Bug (**CONFIRMED** - Line 131)
**File**: `src/backend/ir/ternary_codegen.cpp`  
**Issue**: Atomic types (trit/nit) use direct LLVM arithmetic instead of runtime calls

**Evidence**:
```cpp
// Line 131 - generateAdd() for atomic types
// Atomic type (trit/nit): inline LLVM operation with clamping
llvm::Value* result = builder.CreateAdd(lhs, rhs, "add_tmp");
return clampToRange(result, type);
```

**Why This Breaks**:
1. `CreateAdd` generates LLVM `add i8` instruction
2. Does NOT call `aria_trit_add()` runtime function
3. No sticky ERR propagation
4. SENTINEL HEALING: `ERR + 1 = -127` (garbage), not `ERR`

**Similar Issues**:
- Line 149: `CreateSub` for subtraction
- Line 177: `CreateMul` for multiplication  
- Line 205: `CreateSDiv` for division
- Line 257: `CreateSRem` for modulo
- Line 337: `CreateNeg` for negation

---

#### GOTCHA #5: Kleene Logic MISSING (**CONFIRMED** - NOT IN FILE)
**File**: `src/backend/ir/ternary_codegen.cpp`  
**Issue**: No logic operations implemented AT ALL

**Evidence**: Searched entire file for `AND`, `OR`, `NOT` - not found!

**What's Missing**:
```cpp
// THESE FUNCTIONS DON'T EXIST:
llvm::Value* generateAnd(llvm::Value* lhs, llvm::Value* rhs, Type* type);
llvm::Value* generateOr(llvm::Value* lhs, llvm::Value* rhs, Type* type);
llvm::Value* generateNot(llvm::Value* operand, Type* type);
```

**Why This Breaks**:
1. No calls to `aria_trit_and()`, `aria_trit_or()`, `aria_trit_not()`
2. If codegen falls back to bitwise ops: **LOGIC INVERSION**
3. `True AND False = True` (should be `False`)

**Research Finding** (from Safety Audit):
> *"The audit identified a critical flaw... The function ExprCodegen::generateExoticBinaryOp is responsible for emitting the IR for these operations... The compiler does **not** call runtime functions for atomic ternary logic."*

---

#### GOTCHA #3: Error Swallowing (STATUS UNKNOWN)
**File**: `src/backend/runtime/ternary_ops.cpp`  
**Issue**: Need to validate runtime doesn't return 0 on invalid input

**Research Warning** (from Safety Audit):
```cpp
// WRONG - From broken aria_nit_add
if (a < NIT_MIN || a > NIT_MAX || b < NIT_MIN || b > NIT_MAX) {
    return 0;  // ❌ Silently masks error as "Unknown"
}
```

**Required Fix**:
```cpp
// CORRECT - Return ERR sentinel
if (a < NIT_MIN || a > NIT_MAX || b < NIT_MIN || b > NIT_MAX) {
    return NIT_ERR;  // -128
}
```

**Validation Needed**: Check ternary_ops.cpp lines for all bounds checks

---

## 🔧 Required Fixes

### Fix 1: Replace LLVM Arithmetic with Runtime Calls

**File**: `src/backend/ir/ternary_codegen.cpp`

**Current (BROKEN)**:
```cpp
llvm::Value* TernaryCodegen::generateAdd(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    if (isCompositeType(type)) {
        // ... call runtime for tryte/nyte ...
    }
    
    // ❌ BROKEN: Uses CreateAdd for atomic types
    llvm::Value* result = builder.CreateAdd(lhs, rhs, "add_tmp");
    return clampToRange(result, type);
}
```

**Fixed (CORRECT)**:
```cpp
llvm::Value* TernaryCodegen::generateAdd(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    std::string typeName = type->toString();
    
    // ALL exotic types call runtime (trit, nit, tryte, nyte)
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
    } else if (typeName == "tryte") {
        if (!fn_tryte_add) {
            fn_tryte_add = getOrDeclareIntrinsic("aria_tryte_add", true);
        }
        addFn = fn_tryte_add;
    } else if (typeName == "nyte") {
        if (!fn_nyte_add) {
            fn_nyte_add = getOrDeclareIntrinsic("aria_nyte_add", true);
        }
        addFn = fn_nyte_add;
    }
    
    if (addFn) {
        // Get correct LLVM type (i8 for atomic, i16 for composite)
        llvm::Type* targetType = getTernaryLLVMType(type);
        if (lhs->getType() != targetType) {
            lhs = builder.CreateIntCast(lhs, targetType, true, "trit_cast_lhs");
        }
        if (rhs->getType() != targetType) {
            rhs = builder.CreateIntCast(rhs, targetType, true, "trit_cast_rhs");
        }
        return builder.CreateCall(addFn, {lhs, rhs}, typeName + "_add");
    }
    
    // Should never reach here for exotic types
    llvm::errs() << "ERROR: No runtime function for exotic type: " << typeName << "\n";
    return nullptr;
}
```

**Apply to**: `generateAdd`, `generateSub`, `generateMul`, `generateDiv`, `generateMod`, `generateNeg`

---

### Fix 2: Implement Kleene Logic Operations

**File**: `src/backend/ir/ternary_codegen.cpp`

**Add to header** (`include/backend/ir/ternary_codegen.h`):
```cpp
// Logic operations
llvm::Value* generateAnd(llvm::Value* lhs, llvm::Value* rhs, Type* type);
llvm::Value* generateOr(llvm::Value* lhs, llvm::Value* rhs, Type* type);
llvm::Value* generateNot(llvm::Value* operand, Type* type);
```

**Implementation**:
```cpp
llvm::Value* TernaryCodegen::generateAnd(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    std::string typeName = type->toString();
    
    // Kleene AND = min(a, b) - ALWAYS use runtime for exotic types
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
    std::string typeName = type->toString();
    
    // Kleene OR = max(a, b) - ALWAYS use runtime
    llvm::Function* orFn = nullptr;
    
    if (typeName == "trit") {
        if (!fn_trit_or) {
            fn_trit_or = getOrDeclareIntrinsic("aria_trit_or", true);
        }
        orFn = fn_trit_or;
    } else if (typeName == "nit") {
        if (!fn_nit_or) {
            fn_nit_or = getOrDeclareIntrinsic("aria_nit_or", true);
        }
        orFn = fn_nit_or;
    }
    
    if (orFn) {
        llvm::Type* i8 = builder.getInt8Ty();
        if (lhs->getType() != i8) {
            lhs = builder.CreateIntCast(lhs, i8, true, "trit_cast_lhs");
        }
        if (rhs->getType() != i8) {
            rhs = builder.CreateIntCast(rhs, i8, true, "trit_cast_rhs");
        }
        return builder.CreateCall(orFn, {lhs, rhs}, typeName + "_or");
    }
    
    llvm::errs() << "ERROR: No OR operation for type: " << typeName << "\n";
    return nullptr;
}

llvm::Value* TernaryCodegen::generateNot(llvm::Value* operand, Type* type) {
    std::string typeName = type->toString();
    
    // Balanced ternary NOT = -value
    llvm::Function* notFn = nullptr;
    
    if (typeName == "trit") {
        if (!fn_trit_not) {
            fn_trit_not = getOrDeclareIntrinsic("aria_trit_not", false);
        }
        notFn = fn_trit_not;
    } else if (typeName == "nit") {
        // nin NOT = negation (same as NOT in Kleene logic)
        if (!fn_nit_neg) {  // Reuse negation for NOT
            fn_nit_neg = getOrDeclareIntrinsic("aria_nit_neg", false);
        }
        notFn = fn_nit_neg;
    }
    
    if (notFn) {
        llvm::Type* i8 = builder.getInt8Ty();
        if (operand->getType() != i8) {
            operand = builder.CreateIntCast(operand, i8, true, "trit_cast");
        }
        return builder.CreateCall(notFn, {operand}, typeName + "_not");
    }
    
    llvm::errs() << "ERROR: No NOT operation for type: " << typeName << "\n";
    return nullptr;
}
```

**Add member variables to header**:
```cpp
class TernaryCodegen {
private:
    // ... existing members ...
    llvm::Function* fn_trit_add = nullptr;
    llvm::Function* fn_trit_sub = nullptr;
    llvm::Function* fn_trit_mul = nullptr;
    llvm::Function* fn_trit_and = nullptr;  // NEW
    llvm::Function* fn_trit_or = nullptr;   // NEW
    llvm::Function* fn_trit_not = nullptr;  // NEW
    llvm::Function* fn_nit_add = nullptr;
    llvm::Function* fn_nit_and = nullptr;   // NEW
    llvm::Function* fn_nit_or = nullptr;    // NEW
    llvm::Function* fn_nit_neg = nullptr;   // NEW (or reuse for NOT)
    // ... rest ...
};
```

---

### Fix 3: Validate Error Swallowing in Runtime

**File**: `src/backend/runtime/ternary_ops.cpp`

**Task**: Search for all bounds checks and verify they return ERR, not 0

**Pattern to Find**:
```cpp
if (a < MIN || a > MAX) {
    return 0;  // ❌ WRONG
}
```

**Required Pattern**:
```cpp
if (a < MIN || a > MAX) {
    return ERR_SENTINEL;  // ✅ CORRECT
}
```

**Files to Check**:
- `aria_nit_add()`
- `aria_nit_sub()`
- `aria_nit_mul()`
- `aria_nit_and()`
- `aria_nit_or()`
- All nit operations

---

## 📋 Fix Checklist

### Priority 1: Critical Fixes (MUST DO)
- [ ] **Fix 1a**: Replace `generate Add()` with runtime calls for trit/nit
- [ ] **Fix 1b**: Replace `generateSub()` with runtime calls for trit/nit
- [ ] **Fix 1c**: Replace `generateMul()` with runtime calls for trit/nit
- [ ] **Fix 1d**: Replace `generateDiv()` with runtime calls for trit/nit
- [ ] **Fix 1e**: Replace `generateMod()` with runtime calls for trit/nit
- [ ] **Fix 1f**: Replace `generateNeg()` with runtime calls for trit/nit

- [ ] **Fix 2a**: Implement `generateAnd()` with runtime calls
- [ ] **Fix 2b**: Implement `generateOr()` with runtime calls
- [ ] **Fix 2c**: Implement `generateNot()` with runtime calls

- [ ] **Fix 3**: Validate all bounds checks return ERR, not 0

### Priority 2: Integration (AFTER FIXES)
- [ ] Wire logic ops into ExprCodegen::visitBinaryOp()
- [ ] Wire logic ops into ExprCodegen::visitUnaryOp()
- [ ] Update getOrDeclareIntrinsic() signature for i8 types
- [ ] Test batch02 compilation (should compile without errors)

### Priority 3: Validation (VERIFY FIXES WORK)
- [ ] Test: `1T + 1T = ERR` (overflow detection)
- [ ] Test: `ERR + 1T = ERR` (sticky ERR, NO healing)
- [ ] Test: `1T && (-1T) = -1T` (Kleene AND, NO inversion)
- [ ] Test: `(-1T) || 1T = 1T` (Kleene OR)
- [ ] Test: `!0T = 0T` (NOT unknown = unknown)
- [ ] Run full batch02 test suite
- [ ] Verify 363/363 tests passing

---

## 🎯 Success Criteria

### Before Moving batch02 from tests/future/:
✅ No LLVM arithmetic ops used for trit/nit (grep CreateAdd/CreateSub/CreateMul)  
✅ All logic operations call runtime (grep CreateAnd/CreateOr/CreateXor → should be ZERO)  
✅ All bounds checks return ERR sentinels (grep "return 0" in nit functions)  
✅ batch02 compiles without errors  
✅ All 31 batch02 tests pass  

### After All Fixes:
✅ 363/363 tests passing  
✅ Zero sentinel healing detected  
✅ Zero logic inversions detected  
✅ Fuzzer finds no crashes  
✅ Property-based tests verify Kleene axioms  

---

**Blueprint Philosophy**: "We will get there and when we get there we will be certain we have done the best we could. That's all that matters."

**Next Step**: Begin implementing fixes systematically, one at a time, testing after each change.
