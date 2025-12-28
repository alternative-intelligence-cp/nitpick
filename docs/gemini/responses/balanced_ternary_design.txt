# Balanced Ternary Implementation Design

**Target**: Aria v0.0.11  
**Research Source**: `docs/gemini/responses/research_002_balanced_ternary_arithmetic.txt`  
**Types**: `trit` (single digit), `tryte` (10 trits in uint16)

---

## 1. Mathematical Foundation

### Digit Set
- **Values**: {-1, 0, 1} represented as {T, 0, 1}
- **T** = -1 (Trit-down or "Throw")
- **Radix**: Base-3
- **Value**: $V = \sum_{i=0}^{n-1} d_i \cdot 3^i$ where $d_i \in \{T, 0, 1\}$

### Advantages
1. **Inherently signed** - No sign bit needed
2. **Symmetric negation** - Simply invert trits: 1→T, T→1, 0→0
3. **Natural rounding** - Truncation rounds to nearest

---

## 2. Storage Architecture

### trit (Single Digit)
- **Storage**: `int8` (-1, 0, 1)
- **TypeKind**: `TRIT`
- **LLVM Type**: `i8`

### tryte (10 Trits)
- **Storage**: `uint16` (59,049 valid states out of 65,536)
- **TypeKind**: `TRYTE`
- **LLVM Type**: `i16`
- **Range**: [-29,524, +29,524]
- **Error Sentinel**: 0xFFFF (65,535)

### Packing Strategy: Split-Byte Encoding
```
uint16 layout:
┌─────────────────┬─────────────────┐
│   High Byte     │    Low Byte     │
│  Trits 5-9      │   Trits 0-4     │
│   (5 trits)     │    (5 trits)    │
└─────────────────┴─────────────────┘
   Bits 8-15         Bits 0-7
```

**Per Byte (Trybble)**:
- **Capacity**: 5 trits = $3^5 = 243$ states
- **Storage**: uint8 (256 capacity) = 94.9% utilization
- **Range**: [-121, +121]
- **Bias**: +121
- **Encoding**: $StoredByte = \sum_{i=0}^4 d_i \cdot 3^i + 121$

**Benefits**:
- Fast unpacking via 256-entry lookup tables (LUTs)
- No division operations needed
- Cache-friendly byte access

---

## 3. Implementation Plan

### Phase 1: Core Data Types (Week 1, Days 1-3)
- [ ] Add `TRIT` and `TRYTE` to `TypeKind` enum
- [ ] Update `isNumeric()` and `isInteger()` predicates
- [ ] Add type parsing: "trit", "tryte"
- [ ] Add `toString()` cases
- [ ] Update `getLLVMType()` mapping
- [ ] Add to type checker validation

### Phase 2: Packing/Unpacking (Week 1, Days 4-5)
- [ ] Create `src/backend/ternary_ops.h`
- [ ] Create `src/backend/ternary_ops.cpp`
- [ ] Implement packing functions:
  - `packTrybble(trit[5]) -> uint8`
  - `packTryte(trit[10]) -> uint16`
- [ ] Implement unpacking functions:
  - `unpackTrybble(uint8) -> trit[5]`
  - `unpackTryte(uint16) -> trit[10]`
- [ ] Create lookup tables (LUTs):
  - 256-entry decode table
  - Precomputed powers of 3

### Phase 3: Binary ↔ Ternary Conversion (Week 2, Days 1-2)
- [ ] Implement `binaryToTernary(int64) -> tryte`
- [ ] Implement `ternaryToBinary(tryte) -> int64`
- [ ] Handle overflow → ERR sentinel
- [ ] Create conversion test suite

### Phase 4: Arithmetic Operations (Week 2, Days 3-7)
- [ ] **Addition**:
  - Trit-by-trit with carry propagation
  - Carry can be {-1, 0, 1}
  - Truth table implementation
- [ ] **Subtraction**:
  - Negate and add
  - Negation is $O(n)$ inversion
- [ ] **Multiplication**:
  - Shift-and-add algorithm
  - Handle partial products
- [ ] **Division**:
  - Long division adapted for ternary
  - Remainder handling
- [ ] **Negation**:
  - Simple trit inversion (no carry)

### Phase 5: Code Generation (Week 3, Days 1-3)
- [ ] Create `TernaryLowerer` class (similar to `TBBLowerer`)
- [ ] Integrate with `visitBinaryOp` in codegen
- [ ] Integrate with `visitUnaryOp` for negation
- [ ] Add literal handling in parser
- [ ] Generate LLVM IR for operations

### Phase 6: Testing & Validation (Week 3, Days 4-5)
- [ ] Unit tests for packing/unpacking
- [ ] Arithmetic operation tests
- [ ] Conversion tests
- [ ] Edge cases: overflow, ERR propagation
- [ ] Comprehensive test program
- [ ] Performance benchmarks

---

## 4. Key Algorithms

### Addition Truth Table (Sample)
```
Ai   Bi   Cin  Sraw  Cout  Ri
-1   -1   -1   -3    -1    0     (-1-1-1 = -3 = 3×(-1) + 0)
-1   -1    0   -2    -1    1     (-1-1+0 = -2 = 3×(-1) + 1)
-1    0    0   -1     0   -1     (-1+0+0 = -1)
 0    0    0    0     0    0
 1    1    1    3     1    0     (1+1+1 = 3 = 3×1 + 0)
```

### Negation (Simplest)
```cpp
trit negate(trit t) {
    if (t == 1) return -1;  // T
    if (t == -1) return 1;
    return 0;
}
```

### Packing (Conceptual)
```cpp
uint16 packTryte(trit t[10]) {
    uint8 low = packTrybble(t[0..4]);   // Trits 0-4
    uint8 high = packTrybble(t[5..9]);  // Trits 5-9
    return (high << 8) | low;
}

uint8 packTrybble(trit t[5]) {
    int value = 0;
    for (int i = 0; i < 5; i++) {
        value += t[i] * pow3[i];
    }
    return (uint8)(value + 121);  // Bias
}
```

---

## 5. File Structure

### New Files
```
src/backend/ternary_ops.h       # Ternary operations interface
src/backend/ternary_ops.cpp     # Packing, unpacking, arithmetic
tests/ternary_test.aria         # Comprehensive test suite
docs/research/balanced_ternary_design.md  # This file
```

### Modified Files
```
src/frontend/sema/types.h       # Add TRIT, TRYTE to TypeKind
src/frontend/sema/type_checker.cpp  # Parse "trit", "tryte"
src/backend/codegen.cpp         # Integrate ternary operations
src/backend/codegen_context.h   # Map trit/tryte to LLVM types
CMakeLists.txt                  # Add ternary_ops.cpp to build
```

---

## 6. Testing Strategy

### Unit Tests
- Packing: All 243 trybble states
- Unpacking: Verify inverse
- Arithmetic: Known-answer tests
- Conversion: Round-trip binary→ternary→binary

### Integration Tests
- Mixed arithmetic with other types
- Type promotion/demotion
- Overflow behavior
- ERR propagation

### Example Programs
- Ternary counter
- Base-3 calculator
- Demonstrate symmetric negation advantage
- Compare with binary for same logic

---

## 7. Performance Targets

### Lookup Tables (Critical)
- **Decode LUT**: 256 × 5 bytes = 1.28 KB (L1 cache friendly)
- **Powers of 3**: [1, 3, 9, 27, 81] precomputed

### Operation Costs (Estimated)
- **Unpack trybble**: ~5 cycles (LUT lookup)
- **Pack trybble**: ~20 cycles (5 multiplies + sum)
- **Addition**: ~50 cycles (unpack + add + pack)
- **Negation**: ~30 cycles (invert + pack)

---

## 8. Error Handling

### Sentinel Value
- **TRYTE_ERR**: 0xFFFF (65,535)
- **Trigger conditions**:
  - Overflow: result > 29,524 or < -29,524
  - Invalid input: uint16 > 59,048
  - Error propagation: operation on ERR

### Sticky Errors
Like TBB, ternary errors should be sticky:
- `ERR + x → ERR`
- `ERR * x → ERR`
- `-ERR → ERR`

---

## 9. Success Criteria

### Functional
- [ ] trit and tryte types compile
- [ ] Ternary literals parse correctly
- [ ] All arithmetic operations work
- [ ] Binary↔ternary conversions accurate
- [ ] ERR sentinel propagates correctly

### Performance
- [ ] Packing/unpacking uses LUTs
- [ ] No division operations in hot path
- [ ] Operations complete in < 100 cycles

### Quality
- [ ] 100% test coverage
- [ ] Documentation complete
- [ ] Example programs included
- [ ] Clean LLVM IR generation

---

## 10. Timeline

**Total**: 2-3 weeks

| Phase | Duration | Outcome |
|-------|----------|---------|
| 1. Types | 3 days | Type system integration |
| 2. Pack/Unpack | 2 days | Storage layer working |
| 3. Conversion | 2 days | Binary↔ternary working |
| 4. Arithmetic | 5 days | All operations implemented |
| 5. Codegen | 3 days | LLVM IR generation |
| 6. Testing | 2 days | Validation complete |

**Target completion**: v0.0.11 by early January 2025
