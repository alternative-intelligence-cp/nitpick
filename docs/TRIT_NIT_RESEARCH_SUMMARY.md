# trit/nit Implementation: Research Summary

**Created**: February 5, 2026  
**Sources**: Converted research library (670 .md files)  
**Key Documents**:
- `aria-lang/Balanced Ternary and Nonary Intrinsics.md`
- `aria-lang/Aria Exotic Arithmetic Core Implementation.md`
- `general/Exotic Balanced Base Arithmetic Implementation.md`

---
't violat
## Executive Summary

Found **complete implementation specifications** for trit/nit types with production-ready algorithms, packing strategies, and C++ intrinsic signatures. Ready to implement.

---

## 1. Core Type Specifications

### 1.1 trit (Balanced Ternary Digit)

```cpp
// Storage
type: int8_t  // 8 bits (allows overflow detection)

// Valid values
{-1, 0, +1}

// ERR sentinel
-128 (0x80)  // Sticky error propagation

// Why 8 bits for 3 values?
// - log2(3) ≈ 1.58 bits minimum
// - int2 would wrap: 1 + 1 = 2 → -2 (breaks logic)
// - int8 allows: 1 + 1 = 2 → ERR (proper error)
```

### 1.2 nit (Balanced Nonary Digit)

```cpp
// Storage
type: int8_t  // 8 bits

// Valid values
{-4, -3, -2, -1, 0, 1, 2, 3, 4}

// ERR sentinel
-128 (0x80)

// Relationship to trit
// 1 nit = 2 trits (3² = 9 states)
// Compression/acceleration layer for ternary
```

### 1.3 tryte (10 trits packed)

```cpp
// Storage
type: uint16_t  // 16 bits

// Capacity
10 trits = 3^10 = 59,049 states

// Range
[-29,524, +29,524]

// Efficiency
59,049 / 65,536 ≈ 90.1% utilization

// ERR sentinel
0xFFFF (TRYTE_ERR)

// Gap states
65,536 - 59,049 = 6,487 illegal states
```

### 1.4 nyte (5 nits packed)

```cpp
// Storage
type: uint16_t  // 16 bits

// Capacity
5 nits = 9^5 = 59,049 states (same as tryte!)

// Range
[-29,524, +29,524] (identical to tryte)

// Isomorphism
9^5 = (3²)^5 = 3^10
// nyte and tryte have same information content

// ERR sentinel
0xFFFF (NYTE_ERR)
```

---

## 2. Packing Strategies

### 2.1 Split-Byte Packing (tryte)

**Strategy**: Split 10 trits into two "trybbles" (5 trits each)

```
10 trits: [t9 t8 t7 t6 t5 | t4 t3 t2 t1 t0]
          └─────────────┘   └─────────────┘
           High Trybble      Low Trybble
           (uint8)           (uint8)
```

**Trybble encoding**:
```cpp
// Each trybble: 5 trits → 3^5 = 243 states
// Range: [-121, +121]

// Pack with bias
trybble_stored = (Σ(ti × 3^i) for i=0..4) + 121

// Unpack via LUT
Trybble LUT_UNPACK[256];  // 256 × 5 bytes = 1.28 KB
```

**Benefits**:
- Fast unpacking (byte mask + shift, no division)
- Individual trit access via LUT
- Optimized for logic operations

### 2.2 Biased Radix Packing (nyte)

**Strategy**: Encode as single biased integer

```cpp
// Bias = 29,524 (magnitude of minimum value)
nyte_stored = (Σ(ni × 9^i) for i=0..4) + 29,524

// Maps [-29524, +29524] → [0, 59048]
```

**Benefits**:
- Direct arithmetic possible (unbias → add → rebias)
- Simpler than Split-Byte for magnitude operations
- Optimized for arithmetic throughput

---

## 3. Arithmetic Algorithms

### 3.1 Balanced Ternary Addition (Truth Table)

```
A + B + Cin = 3×Cout + Result

Truth table for trit addition:
 A  |  B  | Cin | Sum | Cout | Result | Logic
----|-----|-----|-----|------|--------|---------------
 0  |  0  |  0  |  0  |  0   |   0    | Identity
 1  |  0  |  0  |  1  |  0   |   1    | Identity
-1  |  0  |  0  | -1  |  0   |  -1    | Identity
 1  |  1  |  0  |  2  |  1   |  -1    | 2 = 3(1) - 1
-1  | -1  |  0  | -2  | -1   |   1    | -2 = 3(-1) + 1
 1  |  1  |  1  |  3  |  1   |   0    | 3 = 3(1) + 0
-1  | -1  | -1  | -3  | -1   |   0    | -3 = 3(-1) + 0
```

**Carry logic**:
```cpp
if (sum >= 2) {
    carry = 1;
    sum -= 3;
} else if (sum <= -2) {
    carry = -1;
    sum += 3;
} else {
    carry = 0;
}
```

### 3.2 Tryte Addition Algorithm

```cpp
uint16_t aria_tryte_add(uint16_t a, uint16_t b) {
    // 1. Sticky ERR check
    if (a == TRYTE_ERR || b == TRYTE_ERR) return TRYTE_ERR;
    
    // 2. Unpack (Split-Byte)
    uint8_t a_lo = a & 0xFF;
    uint8_t a_hi = (a >> 8) & 0xFF;
    uint8_t b_lo = b & 0xFF;
    uint8_t b_hi = (b >> 8) & 0xFF;
    
    // 3. LUT lookup
    const Trybble& ta_lo = LUT_UNPACK[a_lo];
    const Trybble& ta_hi = LUT_UNPACK[a_hi];
    const Trybble& tb_lo = LUT_UNPACK[b_lo];
    const Trybble& tb_hi = LUT_UNPACK[b_hi];
    
    // 4. Ripple carry addition
    int8_t result[10];
    int8_t carry = 0;
    
    for (int i = 0; i < 10; i++) {
        int sum = ta[i] + tb[i] + carry;
        
        if (sum >= 2) {
            carry = 1;
            sum -= 3;
        } else if (sum <= -2) {
            carry = -1;
            sum += 3;
        } else {
            carry = 0;
        }
        result[i] = sum;
    }
    
    // 5. Overflow check
    if (carry != 0) return TRYTE_ERR;
    
    // 6. Repack
    uint8_t lo = pack_trybble(result[0..4]);
    uint8_t hi = pack_trybble(result[5..9]);
    
    return (hi << 8) | lo;
}
```

### 3.3 Kleene Ternary Logic

**AND operation** (min):
```
-1 = false, 0 = unknown, 1 = true

     -1   0   1
-1 | -1  -1  -1
 0 | -1   0   0
 1 | -1   0   1

AND(a, b) = min(a, b)
```

**OR operation** (max):
```
     -1   0   1
-1 | -1   0   1
 0 |  0   0   1
 1 |  1   1   1

OR(a, b) = max(a, b)
```

### 3.4 Nonary Addition (Optimized)

```cpp
uint16_t aria_nyte_add(uint16_t a, uint16_t b) {
    // 1. Sticky ERR check
    if (a == NYTE_ERR || b == NYTE_ERR) return NYTE_ERR;
    
    // 2. Unbias
    int32_t val_a = (int32_t)a - 29524;
    int32_t val_b = (int32_t)b - 29524;
    
    // 3. Add
    int32_t sum = val_a + val_b;
    
    // 4. Range check  
    if (sum < -29524 || sum > 29524) return NYTE_ERR;
    
    // 5. Rebias
    return (uint16_t)(sum + 29524);
}
```

**Why simpler than tryte?**
- Nyte uses biased integer encoding
- CPU ALU handles addition directly
- Only need unbias/rebias + range check

---

## 4. C++ Runtime Intrinsics

### 4.1 Atomic Trit Operations

```cpp
extern "C" {
    // Arithmetic
    int8_t aria_trit_add(int8_t a, int8_t b);
    int8_t aria_trit_sub(int8_t a, int8_t b);
    int8_t aria_trit_mul(int8_t a, int8_t b);
    int8_t aria_trit_neg(int8_t a);
    
    // Kleene logic
    int8_t aria_trit_and(int8_t a, int8_t b);  // min(a, b)
    int8_t aria_trit_or(int8_t a, int8_t b);   // max(a, b)
    
    // Full adder (with carry)
    int16_t aria_trit_add_carry(int8_t a, int8_t b, int8_t cin);
    // Returns: (carry << 8) | (sum & 0xFF)
}
```

### 4.2 Composite Tryte Operations

```cpp
extern "C" {
    // Arithmetic
    uint16_t aria_tryte_add(uint16_t a, uint16_t b);
    uint16_t aria_tryte_sub(uint16_t a, uint16_t b);
    uint16_t aria_tryte_mul(uint16_t a, uint16_t b);
    uint16_t aria_tryte_div(uint16_t a, uint16_t b);
    uint16_t aria_tryte_mod(uint16_t a, uint16_t b);
    uint16_t aria_tryte_neg(uint16_t a);  // Flip all trits
    
    // Comparison
    int32_t aria_tryte_cmp(uint16_t a, uint16_t b);
    // Returns: -1 (less), 0 (equal), 1 (greater), -2 (ERR)
    
    // Conversions
    uint16_t aria_bin_to_tryte(int32_t value);
    int32_t aria_tryte_to_bin(uint16_t tryte);
    
    // Trit extraction
    int8_t aria_tryte_get_trit(uint16_t tryte, uint8_t index);
}
```

### 4.3 Atomic Nit Operations

```cpp
extern "C" {
    // Arithmetic
    int8_t aria_nit_add(int8_t a, int8_t b);
    int8_t aria_nit_sub(int8_t a, int8_t b);
    int8_t aria_nit_mul(int8_t a, int8_t b);
    int8_t aria_nit_neg(int8_t a);
    
    // Kleene logic (9-valued)
    int8_t aria_nit_and(int8_t a, int8_t b);  // min(a, b)
    int8_t aria_nit_or(int8_t a, int8_t b);   // max(a, b)
}
```

### 4.4 Composite Nyte Operations

```cpp
extern "C" {
    // Arithmetic (similar to tryte)
    uint16_t aria_nyte_add(uint16_t a, uint16_t b);
    uint16_t aria_nyte_sub(uint16_t a, uint16_t b);
    uint16_t aria_nyte_mul(uint16_t a, uint16_t b);
    uint16_t aria_nyte_div(uint16_t a, uint16_t b);
    uint16_t aria_nyte_neg(uint16_t a);
    
    // Comparison
    int32_t aria_nyte_cmp(uint16_t a, uint16_t b);
    
    // Conversions
    uint16_t aria_bin_to_nyte(int32_t value);
    int32_t aria_nyte_to_bin(uint16_t nyte);
}
```

---

## 5. Lookup Table Initialization

### 5.1 LUT Structure

```cpp
namespace aria::backend {
    // Unpack LUT: uint8 → 5 trits
    struct Trybble {
        int8_t trits[5];
    };
    
    static Trybble LUT_UNPACK[256];
    
    // Power of 3 table
    static const int32_t POW3[10] = {
        1, 3, 9, 27, 81,
        243, 729, 2187, 6561, 19683
    };
}
```

### 5.2 LUT Initialization Algorithm

```cpp
void TernaryOps::initialize() {
    for (int i = 0; i < 256; i++) {
        // Bias is 121 for trybble
        int32_t value = i - 121;
        
        // Mark invalid range
        if (value < -121 || value > 121) {
            for (int k = 0; k < 5; k++) {
                LUT_UNPACK[i].trits[k] = -2;  // Sentinel
            }
            continue;
        }
        
        // Unpack balanced ternary
        int32_t v = value;
        for (int k = 0; k < 5; k++) {
            int rem = v % 3;
            v /= 3;
            
            // Adjust for balanced system
            if (rem == 2) {
                rem = -1;
                v += 1;  // Compensate carry
            } else if (rem == -2) {
                rem = 1;
                v -= 1;
            }
            
            LUT_UNPACK[i].trits[k] = static_cast<int8_t>(rem);
        }
    }
}
```

---

## 6. LLVM Integration

### 6.1 Type Mapping (CodeGenContext)

```cpp
llvm::Type* CodeGenContext::getLLVMType(AstType* type) {
    switch (type->kind) {
        case TypeKind::Trit:
            return llvm::Type::getInt8Ty(context);
        
        case TypeKind::Nit:
            return llvm::Type::getInt8Ty(context);
        
        case TypeKind::Tryte:
            return llvm::Type::getInt16Ty(context);
        
        case TypeKind::Nyte:
            return llvm::Type::getInt16Ty(context);
        
        // ...
    }
}
```

### 6.2 Arithmetic Lowering

```cpp
// In BinaryOpVisitor::visit()
if (left_type == Type::Trit && right_type == Type::Trit) {
    if (op == BinaryOp::Add) {
        // Generate call to runtime instead of LLVM add
        llvm::Function* trit_add = module->getFunction("aria_trit_add");
        return builder.CreateCall(trit_add, {left_val, right_val});
    }
}

if (left_type == Type::Tryte && right_type == Type::Tryte) {
    if (op == BinaryOp::Add) {
        llvm::Function* tryte_add = module->getFunction("aria_tryte_add");
        return builder.CreateCall(tryte_add, {left_val, right_val});
    }
}

// Similar for nit/nyte operations
```

---

## 7. Safety Properties

### 7.1 Sticky ERR Propagation

**Rule**: `ERR ⊕ x = ERR` for all operations ⊕

```cpp
uint16_t aria_tryte_add(uint16_t a, uint16_t b) {
    // FIRST: Check for ERR
    if (a == TRYTE_ERR || b == TRYTE_ERR) {
        return TRYTE_ERR;  // Sticky propagation
    }
    
    // ... rest of operation ...
}
```

### 7.2 Overflow Detection

**Never wrap** - always return ERR on overflow:

```cpp
// After ripple carry addition
if (carry != 0) {
    return TRYTE_ERR;  // Overflow detected
}
```

### 7.3 No Silent Failures

**No implicit conversions**:
```aria
trit:x = 2;  // COMPILE ERROR (out of range)
trit:y = trit_from_int(2);  // Returns ERR at runtime
```

---

## 8. Performance Optimizations

### 8.1 LUT vs Computation

**Without LUT** (slow):
```cpp
// Extract 5th trit: (value / 3^5) % 3
// Requires division (expensive)
```

**With LUT** (fast):
```cpp
// Extract 5th trit: LUT_UNPACK[byte].trits[i]
// Single memory lookup (cheap)
```

### 8.2 Split-Byte vs Monolithic

**Split-Byte** (tryte):
- Optimized for trit access
- Better for logic operations
- Fast unpacking (mask + shift)

**Biased integer** (nyte):
- Optimized for arithmetic
- Direct ALU usage
- Fast add/sub (unbias/rebias)

---

## 9. Implementation Checklist

### Phase 1: Type System (2 days)
- [ ] Add Trit, Nit, Tryte, Nyte to Type enum
- [ ] Update getLLVMType() mapping
- [ ] Add type checking in semantic analyzer
- [ ] Add literal parsing (e.g., `1T` for trit -1)

### Phase 2: Runtime Library (3 days)
- [ ] Implement LUT initialization
- [ ] Implement trit arithmetic functions
- [ ] Implement tryte arithmetic (Split-Byte)
- [ ] Implement nit arithmetic functions
- [ ] Implement nyte arithmetic (biased)
- [ ] Implement Kleene logic operations

### Phase 3: Compiler Integration (3 days)
- [ ] Add BinaryOp lowering for trit/nit types
- [ ] Add conversion functions
- [ ] Add comparison operators
- [ ] Link runtime library to compiler

### Phase 4: Testing (2 days)
- [ ] Move `tests/future/batch02_gemini_audit_fixes.aria` to `tests/`
- [ ] Verify all 31 test cases pass
- [ ] Add fuzzer corpus for trit/nit
- [ ] Stress test overflow detection

### Phase 5: Documentation (1 day)
- [ ] Update aria_specs.txt with runtime function signatures
- [ ] Add examples to programming guide
- [ ] Update CHANGELOG

---

## 10. Success Criteria

✅ **All 31 batch02 tests pass**  
✅ **ERR propagation verified** (never silently heals)  
✅ **No wrapping** (overflow always detected)  
✅ **Kleene logic correct** (AND=min, OR=max)  
✅ **Fuzzer passes** (no crashes on random input)  
✅ **363/363 tests passing** (100% full coverage)

---

## 11. References

**Key Research Documents**:
1. `aria-lang/Balanced Ternary and Nonary Intrinsics.md`
   - Complete C++ intrinsic signatures
   - LUT architecture and initialization
   - Split-Byte packing strategy
   - Ripple-carry addition algorithm

2. `aria-lang/Aria Exotic Arithmetic Core Implementation.md`
   - Truth tables for ternary operations
   - Balanced carry logic
   - Kleene logic semantics

3. `general/Exotic Balanced Base Arithmetic Implementation.md`
   - Theoretical framework (radix economy)
   - Symmetry advantages
   - Biased radix encoding

4. `aria-lang/Balanced Ternary Arithmetic Research.md`
   - Additional algorithms
   - Edge case handling

**Total converted research**: 670 markdown files searchable in VS Code

---

**Status**: Ready to implement. All algorithms specified. Construction can begin.
