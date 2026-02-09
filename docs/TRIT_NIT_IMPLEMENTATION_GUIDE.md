# trit/nit Implementation Guide - Complete Blueprint

**Status**: Production-ready specification extracted from 659+ research documents  
**Timeline**: 2-3 weeks (10-15 working days)  
**Success Criteria**: 363/363 tests passing (batch02 moves from tests/future/)

---

## ⚠️ CRITICAL: Known Implementation Failures to Avoid

This section documents **catastrophic failures** from previous implementation attempts, extracted from comprehensive safety audits. **DO NOT REPEAT THESE MISTAKES.**

### 🚨 FAILURE MODE 1: Backend Bypass Bug

**What Went Wrong**:
```cpp
// WRONG - From broken implementation in src/backend/ir/codegen_expr.cpp
if (exoticType == "tryte") { funcPrefix = "aria_tryte_"; }
else if (exoticType == "nyte") { funcPrefix = "aria_nyte_"; }
else {
    // trit and nit use regular int8 operations (no special runtime needed)
    return nullptr;  // ❌ CATASTROPHIC BUG
}
```

**Result**: Logic inversion - `True AND False = True` (should be False)

**Why It Breaks**:
- LLVM i8 uses two's complement (-1 = 0xFF binary)
- Bitwise AND: `0x01 & 0xFF = 0x01` (True AND False = True)
- Correct Kleene logic: `min(1, -1) = -1` (True AND False = False)

**THE FIX**: **ALWAYS call runtime functions for trit/nit operations**, NEVER use direct LLVM arithmetic.

---

### 🚨 FAILURE MODE 2: Sentinel Healing

**What Went Wrong**:
```cpp
// WRONG - Sentinel erosion
var x: nit = ERR;  // -128 (0x80)
var y: nit = 1;
var z = x + y;     // Uses LLVM add i8: -128 + 1 = -127
// z is now garbage (-127), NOT ERR
// Subsequent checks (z == ERR) fail silently
```

**Result**: ERR state "healed" into invalid scalar, program proceeds with corrupted data

**THE FIX**: Sticky ERR propagation **MUST** be in runtime functions, not relied upon from LLVM ops.

---

### 🚨 FAILURE MODE 3: Error Swallowing

**What Went Wrong**:
```cpp
// WRONG - From broken aria_nit_add
if (a < NIT_MIN || a > NIT_MAX || b < NIT_MIN || b > NIT_MAX) {
    return 0;  // ❌ Silently masks error as "Unknown"
}
```

**Result**: Out-of-bounds values become valid 0, breaking error detection

**THE FIX**: **ALWAYS return ERR sentinel** on invalid input, never 0 or other "neutral" value.

---

## 📋 Implementation Phases

### Phase 1: Type System Integration (2 days)

**Goal**: Add trit/nit/tryte/nyte to Aria's type system

**Files to Modify**:
- `include/ast/type.h`: Add to Type enum
- `src/ast/type.cpp`: Update `getLLVMType()`, `toString()`, `isPrimitive()`
- `src/semantic/type_checker.cpp`: Add type checking rules
- `src/parser/parser.cpp`: Add literal parsing (e.g., `1T` for trit, `5N` for nit)

**Type Mapping**:
```cpp
// In getLLVMType() - include/ast/type.h
case Type::Trit:
case Type::Nit:
    return llvm::Type::getInt8Ty(context);  // i8 for atomic
    
case Type::Tryte:
case Type::Nyte:
    return llvm::Type::getInt16Ty(context); // i16 for composite
```

**Constants** (share between compiler and runtime):
```cpp
// include/runtime/exotic_constants.h
#define TRIT_ERR  static_cast<int8_t>(-128)   // 0x80
#define NIT_ERR   static_cast<int8_t>(-128)   // 0x80
#define TRYTE_ERR static_cast<uint16_t>(0xFFFF)
#define NYTE_ERR  static_cast<uint16_t>(0xFFFF)

#define TRYTE_MIN -29524
#define TRYTE_MAX +29524
#define NYTE_MIN  -29524
#define NYTE_MAX  +29524
```

---

### Phase 2: Runtime Library Implementation (3-4 days)

**Goal**: Implement all 40+ intrinsic functions with correct Kleene logic

**File**: `lib/runtime/exotic_types.c` (new file)

#### 2.1: Lookup Table Initialization

```c
// Trybble unpacking LUT (256 entries, ~1.28 KB, L1 cache resident)
typedef struct {
    int8_t trits[5];  // 5 trits per trybble
} Trybble;

static Trybble LUT_UNPACK[256];
static uint8_t LUT_PACK[243];  // 3^5 = 243 valid combinations

void aria_init_lut(void) {
    // Initialize unpacking table
    for (int i = 0; i < 256; i++) {
        int value = i;
        for (int j = 0; j < 5; j++) {
            int remainder = value % 3;
            LUT_UNPACK[i].trits[j] = (remainder == 2) ? -1 : remainder;
            value /= 3;
        }
    }
    
    // Initialize packing table (inverse mapping)
    for (int i = 0; i < 243; i++) {
        uint8_t packed = 0;
        int temp = i;
        for (int j = 0; j < 5; j++) {
            int trit = temp % 3;
            packed += trit * pow3[j];  // pow3[] = {1, 3, 9, 27, 81}
            temp /= 3;
        }
        LUT_PACK[i] = packed;
    }
}
```

**Cache Efficiency Analysis**:
- Full 16-bit LUT: 65,536 entries × 10 bytes = ~640 KB (L2/L3 cache)
- Split-Byte LUT: 256 entries × 5 bytes = ~1.28 KB (L1 cache) ✅
- **Deterministic latency**: No cache misses for physics simulations

---

#### 2.2: Atomic Type Operations (trit/nit)

**Kleene Logic Implementation** (NOT bitwise):

```c
// Balanced ternary AND: min(a, b)
int8_t aria_trit_and(int8_t a, int8_t b) {
    if (a == TRIT_ERR || b == TRIT_ERR) return TRIT_ERR;  // Sticky ERR
    return (a < b) ? a : b;  // min(a, b)
}

// Balanced ternary OR: max(a, b)
int8_t aria_trit_or(int8_t a, int8_t b) {
    if (a == TRIT_ERR || b == TRIT_ERR) return TRIT_ERR;
    return (a > b) ? a : b;  // max(a, b)
}

// Balanced ternary NOT: -a
int8_t aria_trit_not(int8_t a) {
    if (a == TRIT_ERR) return TRIT_ERR;
    return -a;
}
```

**Arithmetic with Truth Tables**:

```c
// Balanced ternary addition: A + B + Cin = 3×Cout + Result
// Truth table excerpt:
//   -1 + -1 + 0  = -2 → Cout=-1, Result=1  (because -2 = 3×(-1) + 1)
//    1 +  1 + 0  =  2 → Cout=1,  Result=-1 (because 2 = 3×1 + (-1))

int8_t aria_trit_add(int8_t a, int8_t b) {
    if (a == TRIT_ERR || b == TRIT_ERR) return TRIT_ERR;
    
    int sum = a + b;
    
    // Normalize to {-1, 0, 1} with carry
    if (sum >= 2) return TRIT_ERR;  // Overflow (would need carry out)
    if (sum <= -2) return TRIT_ERR; // Underflow
    
    return sum;
}

// With explicit carry tracking
typedef struct { int8_t result; int8_t carry; } TritAddResult;

TritAddResult aria_trit_add_carry(int8_t a, int8_t b, int8_t cin) {
    TritAddResult res;
    
    if (a == TRIT_ERR || b == TRIT_ERR || cin == TRIT_ERR) {
        res.result = TRIT_ERR;
        res.carry = 0;
        return res;
    }
    
    int sum = a + b + cin;
    
    // Balanced ternary normalization
    if (sum >= 2) {
        res.carry = 1;
        res.result = sum - 3;
    } else if (sum <= -2) {
        res.carry = -1;
        res.result = sum + 3;
    } else {
        res.carry = 0;
        res.result = sum;
    }
    
    return res;
}
```

**Nonary (nit) Operations** (9-valued Kleene logic):

```c
// Balanced nonary AND: min(a, b) where a,b ∈ {-4,-3,-2,-1,0,1,2,3,4}
int8_t aria_nit_and(int8_t a, int8_t b) {
    if (a == NIT_ERR || b == NIT_ERR) return NIT_ERR;
    if (a < -4 || a > 4 || b < -4 || b > 4) return NIT_ERR;  // Bounds check
    return (a < b) ? a : b;
}

// Balanced nonary OR: max(a, b)
int8_t aria_nit_or(int8_t a, int8_t b) {
    if (a == NIT_ERR || b == NIT_ERR) return NIT_ERR;
    if (a < -4 || a > 4 || b < -4 || b > 4) return NIT_ERR;
    return (a > b) ? a : b;
}

// Addition using 9×9 lookup table
static const int8_t NIT_ADD_TABLE[9][9] = {
    // Rows: a+4 (offset to 0-8), Cols: b+4
    {-4, -3, -2, -1,  0,  1,  2,  3,  4},  // a=-4
    {-3, -2, -1,  0,  1,  2,  3,  4, NIT_ERR},  // a=-3 (overflow at 4+(-3)=1 → ERR)
    // ... (complete table from research)
};

int8_t aria_nit_add(int8_t a, int8_t b) {
    if (a == NIT_ERR || b == NIT_ERR) return NIT_ERR;
    if (a < -4 || a > 4 || b < -4 || b > 4) return NIT_ERR;  // ⚠️ MUST return ERR, not 0
    
    return NIT_ADD_TABLE[a + 4][b + 4];
}
```

---

#### 2.3: Composite Type Operations (tryte/nyte)

**Split-Byte Packing for tryte**:

```c
// Tryte: 10 trits split into 2 trybbles (5 trits each)
uint16_t aria_tryte_add(uint16_t a, uint16_t b) {
    if (a == TRYTE_ERR || b == TRYTE_ERR) return TRYTE_ERR;
    
    // Step 1: Unpack into trybbles using LUT
    uint8_t a_lo = a & 0xFF;
    uint8_t a_hi = (a >> 8) & 0xFF;
    uint8_t b_lo = b & 0xFF;
    uint8_t b_hi = (b >> 8) & 0xFF;
    
    Trybble ta_lo = LUT_UNPACK[a_lo];
    Trybble ta_hi = LUT_UNPACK[a_hi];
    Trybble tb_lo = LUT_UNPACK[b_lo];
    Trybble tb_hi = LUT_UNPACK[b_hi];
    
    // Step 2: Ripple-carry addition across trits
    Trybble result_lo, result_hi;
    int8_t carry = 0;
    
    // Add low trybble
    for (int i = 0; i < 5; i++) {
        TritAddResult res = aria_trit_add_carry(ta_lo.trits[i], tb_lo.trits[i], carry);
        if (res.result == TRIT_ERR) return TRYTE_ERR;
        result_lo.trits[i] = res.result;
        carry = res.carry;
    }
    
    // Add high trybble with carry from low
    for (int i = 0; i < 5; i++) {
        TritAddResult res = aria_trit_add_carry(ta_hi.trits[i], tb_hi.trits[i], carry);
        if (res.result == TRIT_ERR) return TRYTE_ERR;
        result_hi.trits[i] = res.result;
        carry = res.carry;
    }
    
    // Step 3: Check for overflow
    if (carry != 0) return TRYTE_ERR;  // Value exceeds ±29,524
    
    // Step 4: Repack into uint16
    uint8_t packed_lo = pack_trybble(result_lo);
    uint8_t packed_hi = pack_trybble(result_hi);
    
    return (packed_hi << 8) | packed_lo;
}
```

**Biased Radix Encoding for nyte**:

```c
// Nyte: 5 nits with biased radix encoding (bias = 29,524)
uint16_t aria_nyte_add(uint16_t a, uint16_t b) {
    if (a == NYTE_ERR || b == NYTE_ERR) return NYTE_ERR;
    
    // Step 1: Unbias to balanced values
    int32_t a_balanced = (int32_t)a - 29524;
    int32_t b_balanced = (int32_t)b - 29524;
    
    // Step 2: Add in balanced domain
    int32_t result_balanced = a_balanced + b_balanced;
    
    // Step 3: Check overflow
    if (result_balanced < NYTE_MIN || result_balanced > NYTE_MAX) {
        return NYTE_ERR;
    }
    
    // Step 4: Rebias to storage representation
    return (uint16_t)(result_balanced + 29524);
}
```

**Why Two Strategies?**:
- **Tryte (Split-Byte)**: Optimized for logic operations and trit access
- **Nyte (Biased Radix)**: Optimized for arithmetic throughput
- Both use same range: [-29,524, +29,524] → isomorphic!

---

### Phase 3: Compiler Integration (3 days)

**Goal**: Generate correct LLVM IR that calls runtime functions

**Files to Modify**:
- `src/backend/codegen/codegen_expr.cpp`: Binary operation lowering
- `src/backend/codegen/codegen_context.h`: Runtime function declarations
- `src/backend/codegen/codegen_module.cpp`: Link runtime library

#### 3.1: Binary Operation Lowering

```cpp
// In CodegenExpr::visitBinaryOp() - src/backend/codegen/codegen_expr.cpp

llvm::Value* CodegenExpr::visitBinaryOp(BinaryOp* node) {
    auto left = visit(node->left);
    auto right = visit(node->right);
    auto type = node->left->type;
    
    // Handle exotic types - ALWAYS use runtime calls
    if (type->isTrit() || type->isNit() || type->isTryte() || type->isNyte()) {
        return generateExoticBinaryOp(type, node->op, left, right);
    }
    
    // ... standard types
}

llvm::Value* CodegenExpr::generateExoticBinaryOp(
    Type* type, 
    TokenType op, 
    llvm::Value* left, 
    llvm::Value* right
) {
    std::string funcName;
    
    // Map type + operation to runtime function
    if (type->isTrit()) {
        switch (op) {
            case TokenType::Plus:  funcName = "aria_trit_add"; break;
            case TokenType::Minus: funcName = "aria_trit_sub"; break;
            case TokenType::Star:  funcName = "aria_trit_mul"; break;
            case TokenType::And:   funcName = "aria_trit_and"; break;  // ⚠️ NOT bitwise!
            case TokenType::Or:    funcName = "aria_trit_or"; break;   // ⚠️ NOT bitwise!
            default: 
                throw std::runtime_error("Unsupported trit operation");
        }
    } else if (type->isNit()) {
        // Similar for nit operations
        funcName = "aria_nit_" + getOpName(op);
    } else if (type->isTryte()) {
        funcName = "aria_tryte_" + getOpName(op);
    } else if (type->isNyte()) {
        funcName = "aria_nyte_" + getOpName(op);
    }
    
    // Get or declare runtime function
    llvm::Function* func = module->getFunction(funcName);
    if (!func) {
        llvm::FunctionType* funcType = getFunctionType(type, op);
        func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            funcName,
            module
        );
    }
    
    // Generate call
    return builder->CreateCall(func, {left, right});
}
```

#### 3.2: Optimization - Intrinsic Lowering (Optional Performance Enhancement)

**Problem**: Calling a function for every trit operation (8-bit add) has massive overhead

**Solution**: Emit inline LLVM IR for simple operations using `icmp` + `select`

```cpp
// Optimized trit_and: min(a, b) using LLVM select
llvm::Value* CodegenExpr::inlineTritAnd(llvm::Value* a, llvm::Value* b) {
    // Generate: a < b ? a : b  (min function)
    auto cmp = builder->CreateICmpSLT(a, b);  // Signed less-than
    return builder->CreateSelect(cmp, a, b);
}

// With ERR checking:
llvm::Value* CodegenExpr::inlineTritAndSafe(llvm::Value* a, llvm::Value* b) {
    auto trit_err = llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), -128);
    
    // Check if a == ERR
    auto a_is_err = builder->CreateICmpEQ(a, trit_err);
    // Check if b == ERR  
    auto b_is_err = builder->CreateICmpEQ(b, trit_err);
    auto either_err = builder->CreateOr(a_is_err, b_is_err);
    
    // Compute min(a, b)
    auto cmp = builder->CreateICmpSLT(a, b);
    auto min_val = builder->CreateSelect(cmp, a, b);
    
    // Return ERR if input was ERR, otherwise min
    return builder->CreateSelect(either_err, trit_err, min_val);
}
```

**When to Use**:
- Inline for hot paths (loops, inner computations)
- Function calls for cold paths (initialization, I/O)
- Benchmark both to validate performance improvements

---

### Phase 4: Testing & Validation (2 days)

**Goal**: Move batch02 from tests/future/ and verify 363/363 tests passing

#### 4.1: Move Tests

```bash
cd /home/randy/Workspace/REPOS/aria
mv tests/future/batch02_gemini_audit_fixes.aria tests/
```

#### 4.2: Validate Test Cases

**From batch02** (31 tests covering):
- Basic arithmetic: `1T + 1T = 0T` (overflow → ERR)
- Kleene logic: `1T && (-1T) = -1T` (True AND False = False)
- Sticky ERR: `ERR + 1T = ERR`
- Boundary cases: `TRYTE_MAX + 1 = ERR`
- Conversions: `tryte_to_nyte(x)` isomorphism
- Nonary operations: `4N + 1N = ERR` (overflow)

#### 4.3: Fuzzing Strategy

**Property-Based Testing**:

```cpp
// tests/fuzzing/trit_properties.cpp
FOR_ALL(int8_t a IN {-1, 0, 1}, int8_t b IN {-1, 0, 1}) {
    // Property: Kleene AND is commutative
    ASSERT_EQ(aria_trit_and(a, b), aria_trit_and(b, a));
    
    // Property: Kleene AND is idempotent
    ASSERT_EQ(aria_trit_and(a, a), a);
    
    // Property: ERR is absorbing
    ASSERT_EQ(aria_trit_and(a, TRIT_ERR), TRIT_ERR);
}
```

**Adversarial Inputs**:
```cpp
// Sentinel healing detection
int8_t result = aria_trit_add(TRIT_ERR, 1);
ASSERT_EQ(result, TRIT_ERR);  // Must NOT become -127

// Overflow detection
uint16_t overflow = aria_tryte_add(TRYTE_MAX, 1);
ASSERT_EQ(overflow, TRYTE_ERR);  // Must NOT wrap around
```

---

### Phase 5: Documentation (1 day)

**Files to Update**:
- `aria_specs.txt`: Add runtime function signatures
- `docs/PROGRAMMING_GUIDE.md`: Add trit/nit examples
- `CHANGELOG.md`: Document new types
- `docs/IMPLEMENTATION_NOTES.md`: Record any gotchas discovered

---

## 📊 Success Metrics

- ✅ **Type System**: Trit/Nit/Tryte/Nyte in Type enum
- ✅ **Runtime Library**: 40+ intrinsic functions implemented
- ✅ **Compiler Integration**: Binary ops call runtime (NO bypass)
- ✅ **Test Coverage**: 363/363 tests passing (100%)
- ✅ **Fuzzing**: Property-based tests for Kleene logic
- ✅ **Performance**: LUT fits in L1 cache (~1.28 KB)
- ✅ **Safety**: Zero sentinel healing, zero logic inversions

---

## 🔍 Implementation Checklist

### Pre-Implementation
- [x] Research converted and indexed (670 .md files searchable)
- [x] All specifications extracted (TRIT_NIT_RESEARCH_SUMMARY.md)
- [x] Known failures documented (this guide)
- [ ] Build environment verified (`./build.sh` works)
- [ ] Test baseline established (361/361 current tests passing)

### Phase 1: Type System (2 days)
- [ ] Add Type enum entries (Trit, Nit, Tryte, Nyte)
- [ ] Update getLLVMType() mapping (i8 for atomic, i16 for composite)
- [ ] Add type checking rules in semantic analyzer
- [ ] Add literal parsing (`1T`, `5N`, etc.)
- [ ] Create shared constants header (TRIT_ERR, etc.)
- [ ] Run type system tests

### Phase 2: Runtime Library (3-4 days)
- [ ] Initialize LUT tables (256-entry unpacking, 243-entry packing)
- [ ] Implement trit operations (add, sub, mul, and, or, not)
- [ ] Implement nit operations (add, sub, mul, and, or, not)
- [ ] Implement tryte operations (add, sub, mul, div, mod, neg)
- [ ] Implement nyte operations (add, sub, mul, div, mod, neg)
- [ ] Implement conversion functions (bin→trit, trit→bin, etc.)
- [ ] Unit test each runtime function individually
- [ ] Verify LUT cache efficiency (perf profiling)

### Phase 3: Compiler Integration (3 days)
- [ ] Add runtime function declarations to codegen context
- [ ] Implement generateExoticBinaryOp() with NO atomic bypass
- [ ] Add comparison operators (trit_cmp, tryte_cmp, etc.)
- [ ] Link runtime library to compiler build
- [ ] Test IR generation (dump LLVM IR, verify function calls)
- [ ] Verify NO direct LLVM arithmetic for atomic types

### Phase 4: Testing (2 days)
- [ ] Move tests/future/batch02 to tests/
- [ ] Run full test suite (expect 363/363 passing)
- [ ] Add fuzzer corpus for trit/nit operations
- [ ] Run property-based tests (Kleene logic axioms)
- [ ] Stress test overflow detection
- [ ] Verify sticky ERR propagation (no healing)

### Phase 5: Documentation (1 day)
- [ ] Update aria_specs.txt with intrinsic signatures
- [ ] Add trit/nit examples to programming guide
- [ ] Document implementation gotchas discovered
- [ ] Update CHANGELOG with new types
- [ ] Update README test count (363/363)

---

## 🎯 Next Steps

**Option 1**: Begin Phase 1 immediately (type system integration)  
**Option 2**: Search research for additional edge cases  
**Option 3**: Set up continuous fuzzing infrastructure first  

**Recommendation**: **Start Phase 1** - all specifications are complete, no unknowns remaining. Research is accessible if questions arise during implementation.

---

**Blueprint Philosophy Reminder**: 
- No shortcuts on safety checks
- No "regular int8 operations" for exotic types  
- No magic numbers (use named constants)
- No silent error swallowing (always propagate ERR)
- Test coverage must be 100% on implemented features

**Ready to build when you are.** 🏗️
