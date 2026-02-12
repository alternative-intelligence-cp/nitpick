# fix256 GPU Implementation Design

**Type:** Deterministic 256-bit fixed-point arithmetic  
**Representation:** Q128.128 (128 integer bits + 128 fractional bits)  
**Storage:** 4x uint64 limbs [limb0, limb1, limb2, limb3]

## Limb Layout (Little-Endian)

```
Bit Position:  255..192  191..128  127..64   63..0
Limb Index:    limb3     limb2     limb1     limb0
Purpose:       High Int   Low Int   High Frac Low Frac
```

**Binary Point:** Between limb1 and limb2  
**Integer Part:** limbs[2..3] (128 bits)  
**Fractional Part:** limbs[0..1] (128 bits)

## Operations to Implement

### Phase 5A: Basic Arithmetic (4-6 hours)
1. **Addition** - Add with carry propagation
2. **Subtraction** - Subtract with borrow propagation  
3. **Comparison** - Less than, greater than, equal

### Phase 5B: Multiplication (3-4 hours)
4. **Karatsuba Multiplication** - Divide-and-conquer 256-bit multiply

### Phase 5C: Division (2-3 hours)
5. **Long Division** - 256-bit ÷ 256-bit quotient + remainder

### Phase 5D: Conversions (1-2 hours)
6. **From float32** - Convert IEEE 754 to Q128.128
7. **To float32** - Convert Q128.128 to IEEE 754
8. **From int32** - Integer to fixed-point
9. **To int32** - Fixed-point to integer (truncate)

## Implementation Strategy

### Approach 1: Runtime Library (CHOSEN)
- Implement operations as C runtime functions
- Link with Aria runtime  
- GPU kernels call runtime functions
- **Pros:** Easier to implement, debug, test
- **Cons:** Function call overhead (minimal on GPU)

### Approach 2: LLVM IR Intrinsics
- Generate inline LLVM IR for each operation
- No function call overhead
- **Pros:** Maximum performance
- **Cons:** Complex IR generation, harder to debug

**Decision:** Start with Approach 1 (runtime library), optimize to Approach 2 later if needed.

## File Organization

```
include/runtime/fix256.h         - Public API declarations
src/runtime/fix256/
    fix256_arithmetic.cpp        - Add, sub, compare
    fix256_multiply.cpp          - Karatsuba multiplication  
    fix256_divide.cpp            - Long division
    fix256_convert.cpp           - Type conversions
```

## C Runtime API

```c
// Arithmetic (returns overflow flag in carry-out)
uint64_t aria_fix256_add(uint64_t* result, const uint64_t* a, const uint64_t* b);
uint64_t aria_fix256_sub(uint64_t* result, const uint64_t* a, const uint64_t* b);

// Multiplication (Karatsuba algorithm) 
void aria_fix256_mul(uint64_t* result, const uint64_t* a, const uint64_t* b);

// Division (quotient + remainder)
void aria_fix256_div(uint64_t* quotient, uint64_t* remainder, 
                      const uint64_t* dividend, const uint64_t* divisor);

// Comparison
int32_t aria_fix256_cmp(const uint64_t* a, const uint64_t* b);  // -1, 0, +1

// Conversions
void aria_fix256_from_f32(uint64_t* result, float value);
float aria_fix256_to_f32(const uint64_t* value);
void aria_fix256_from_i32(uint64_t* result, int32_t value);
int32_t aria_fix256_to_i32(const uint64_t* value);
```

## Aria Language API

```aria
// Constructor from literal
fix256:value = 3.14159265358979323846fix256;

// Arithmetic operators
fix256:sum = a + b;      // Maps to aria_fix256_add()
fix256:diff = a - b;     // Maps to aria_fix256_sub()
fix256:prod = a * b;     // Maps to aria_fix256_mul()
fix256:quot = a / b;     // Maps to aria_fix256_div()

// Comparison
bool:less = a < b;       // Maps to aria_fix256_cmp()
bool:equal = a == b;

// Conversions
fix256:from_float = fix256.from_float(3.14f32);
float32:to_float = value.to_float();
```

## GPU Compatibility Requirements

**CRITICAL for Determinism:**
- ✅ No floating-point operations (uses integer math only)
- ✅ No architecture-specific instructions (portable PTX)
- ✅ Bit-exact results across all GPUs (RTX 3090, A100, H100, etc.)
- ✅ No undefined behavior
- ✅ Overflow detection for safety

**Performance Targets:**
- Addition/Subtraction: 10-20 cycles
- Multiplication (Karatsuba): 100-200 cycles
- Division: 200-400 cycles
- **Physics timestep budget:** <100,000 cycles for 1000 operations = <100 cycles/op average

## Testing Strategy

### Unit Tests
```aria
// Test addition
func:test_add = void() {
    fix256:a = fix256.from_float(1.5f32);
    fix256:b = fix256.from_float(2.5f32);
    fix256:result = a + b;
    assert(result.to_float() == 4.0f32);
};

// Test multiplication  
func:test_mul = void() {
    fix256:a = fix256.from_float(3.0f32);
    fix256:b = fix256.from_float(4.0f32);
    fix256:result = a * b;
    assert(result.to_float() == 12.0f32);
};

// Test determinism (same input → same output across GPUs)
func:test_determinism = void() {
    fix256:a = fix256.from_float(0.1f32);
    fix256:sum = fix256.from_float(0.0f32);
    
    till (i:int32 = 0i32; i < 10i32) {
        sum = sum + a;
        i = i + 1i32;
    }
    
    // Result should be EXACTLY 1.0 (not 0.9999999... like float)
    assert(sum.to_float() == 1.0f32);
};
```

### Integration Tests
- Test in actual GPU kernel
- Verify PTX codegen
- Test on real RTX 3090 hardware
- Compare results across different CUDA compute capabilities

## Next Steps

1. ✅ Design complete
2. Create runtime library skeleton
3. Implement addition/subtraction
4. Implement Karatsuba multiplication
5. Implement division
6. Add conversions
7. Test on GPU
8. Document Phase 5 completion

**Estimated Time:** 8-12 hours total
