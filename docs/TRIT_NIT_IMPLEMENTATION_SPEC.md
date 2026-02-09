# trit/nit Type Implementation Specification

**Status**: HIGH PRIORITY - Core Nikola substrate requirement  
**Based on**: aria_specs.txt, batch02 tests, Aria Ecosystem Feature Audit

---

## Overview

`trit` and `nit` are **balanced numeral types** essential for Nikola's 9D wave interference substrate. Unlike binary digits, they provide **symmetric signed ranges** that naturally express wave states.

---

## 1. trit (Balanced Ternary Digit)

### 1.1 Specification

| Property | Value |
|----------|-------|
| **Name** | trit (ternary digit) |
| **Values** | `-1`, `0`, `+1` |
| **ERR Sentinel** | `-128` (0x80 in i8 storage) |
| **Storage** | i8 (8 bits) - allows overflow detection |
| **Range Check** | Valid: `{-1, 0, 1}`, Invalid: all other values → ERR |

**Why 8 bits for 3 values?**
- Requires extra bits beyond minimum (log2(3) ≈ 1.58) for overflow detection
- i2 would wrap: `1 + 1 = 2 → -2` (breaks ternary logic)
- i8 allows: `1 + 1 = 2 → ERR` (proper error signaling)

### 1.2 Arithmetic Operations

#### Addition
```
trit + trit → trit (with ERR on overflow)

-1 + -1 = ERR  (underflow)
-1 +  0 = -1
-1 +  1 =  0
 0 +  0 =  0
 0 +  1 =  1
 1 +  1 = ERR  (overflow)
ERR + x = ERR  (sticky propagation)
```

#### Multiplication
```
trit * trit → trit (with ERR on overflow)

-1 * -1 =  1
-1 *  0 =  0
-1 *  1 = -1
 0 * x  =  0
 1 *  1 =  1
ERR * x = ERR  (sticky propagation)
```

### 1.3 Logic Operations (Kleene Ternary Logic)

**trit_and(a, b)** - AND operation using min():
```
-1 = false
 0 = unknown  
 1 = true

AND table (min of operands):
     -1   0   1
-1 | -1  -1  -1
 0 | -1   0   0
 1 | -1   0   1
```

**trit_or(a, b)** - OR operation using max():
```
OR table (max of operands):
     -1   0   1
-1 | -1   0   1
 0 |  0   0   1
 1 |  1   1   1
```

### 1.4 Validation Functions

```aria
// Check if value is ERR sentinel
func:trit_is_err = bool(trit:value) {
    pass(value == -128);
};

// Convert from int with range checking
func:trit_from_int = trit(int32:value) {
    if (value < -1 || value > 1) {
        pass(-128);  // Return ERR
    }
    pass(value as trit);
};

// Convert to int (returns -128 for ERR)
func:trit_to_int = int32(trit:value) {
    pass(value as int32);
};
```

---

## 2. nit (Balanced Nonary Digit)

### 2.1 Specification

| Property | Value |
|----------|-------|
| **Name** | nit (nonary digit, base-9) |
| **Values** | `-4`, `-3`, `-2`, `-1`, `0`, `+1`, `+2`, `+3`, `+4` |
| **ERR Sentinel** | `-128` (0x80 in i8 storage) |
| **Storage** | i8 (8 bits) - allows overflow detection |
| **Range Check** | Valid: `{-4..4}`, Invalid: all other values → ERR |

**Why balanced nonary?**
- 9 wave interference states in Nikola's 9D-TWI substrate
- Symmetric around zero (like trit but finer gradation)
- Natural mapping to phase relationships

### 2.2 Arithmetic Operations

#### Addition
```
nit + nit → nit (with ERR on overflow)

-4 + -1 = ERR  (underflow: -5 < -4)
-4 +  0 = -4
-3 +  2 = -1
 0 +  0 =  0
 2 +  3 = ERR  (overflow: 5 > 4)
 4 +  1 = ERR  (overflow: 5 > 4)
ERR + x = ERR  (sticky propagation)
```

#### Multiplication
```
nit * nit → nit (with ERR on overflow)

-4 * -1 =  4
-4 *  2 = ERR  (overflow: -8 out of range)
-2 *  2 = -4
 0 * x  =  0
 2 *  2 =  4
 2 *  3 = ERR  (overflow: 6 > 4)
ERR * x = ERR  (sticky propagation)
```

### 2.3 Logic Operations (Nine-Valued Kleene Logic)

**nit_and(a, b)** - AND operation using min():
```
More negative = more false
More positive = more true

AND(a, b) = min(a, b)

Examples:
AND(3, 2) = 2 (more conservative estimate)
AND(-1, 2) = -1 (false wins)
AND(0, 4) = 0 (unknown limits)
```

**nit_or(a, b)** - OR operation using max():
```
OR(a, b) = max(a, b)

Examples:
OR(2, 3) = 3 (more optimistic estimate)
OR(-3, 1) = 1 (true wins)
OR(0, -2) = 0 (unknown more true than false)
```

### 2.4 Validation Functions

```aria
// Check if value is ERR sentinel
func:nit_is_err = bool(nit:value) {
    pass(value == -128);
};

// Convert from int with range checking
func:nit_from_int = nit(int32:value) {
    if (value < -4 || value > 4) {
        pass(-128);  // Return ERR
    }
    pass(value as nit);
};

// Convert to int (returns -128 for ERR)
func:nit_to_int = int32(nit:value) {
    pass(value as int32);
};
```

---

## 3. Implementation Requirements

### 3.1 Type System Changes

**Add to Type enum** (include/ast/type.h):
```cpp
enum class Type {
    // ... existing types ...
    Trit,        // Balanced ternary digit
    Tryte,       // 10 trits packed in u16
    Nit,         // Balanced nonary digit
    Nyte,        // 5 nits packed in u16
    // ...
};
```

**Storage mapping** (backend/codegen_context.h):
```cpp
case Type::Trit:
    return llvm::Type::getInt8Ty(context);  // i8 for overflow detection
case Type::Nit:
    return llvm::Type::getInt8Ty(context);  // i8 for overflow detection
```

### 3.2 Runtime Library Functions

**Required runtime implementations** (lib/runtime/balanced_logic.cpp):

```cpp
// Trit operations
extern "C" int8_t trit_add(int8_t a, int8_t b);
extern "C" int8_t trit_mul(int8_t a, int8_t b);
extern "C" int8_t trit_and(int8_t a, int8_t b);  // Kleene AND (min)
extern "C" int8_t trit_or(int8_t a, int8_t b);   // Kleene OR (max)
extern "C" bool trit_is_err(int8_t value);
extern "C" int8_t trit_from_int(int32_t value);
extern "C" int32_t trit_to_int(int8_t value);

// Nit operations
extern "C" int8_t nit_add(int8_t a, int8_t b);
extern "C" int8_t nit_mul(int8_t a, int8_t b);
extern "C" int8_t nit_and(int8_t a, int8_t b);    // Kleene AND (min)
extern "C" int8_t nit_or(int8_t a, int8_t b);     // Kleene OR (max)
extern "C" bool nit_is_err(int8_t value);
extern "C" int8_t nit_from_int(int32_t value);
extern "C" int32_t nit_to_int(int8_t value);
```

### 3.3 Arithmetic Lowering

**Compiler must intercept trit/nit operations** and generate runtime calls instead of LLVM add/mul:

```cpp
// In BinaryOpVisitor::visit()
if (left_type == Type::Trit && right_type == Type::Trit) {
    if (op == BinaryOp::Add) {
        // Generate call to trit_add() instead of LLVM add
        return builder.CreateCall(trit_add_func, {left_val, right_val});
    }
    else if (op == BinaryOp::Mul) {
        return builder.CreateCall(trit_mul_func, {left_val, right_val});
    }
}
```

### 3.4 ERR Propagation

**All operations must check for ERR and propagate**:

```cpp
int8_t trit_add(int8_t a, int8_t b) {
    const int8_t ERR = -128;
    
    // Sticky ERR propagation
    if (a == ERR || b == ERR) return ERR;
    
    // Range validation (defensive)
    if (a < -1 || a > 1 || b < -1 || b > 1) return ERR;
    
    // Arithmetic with overflow check
    int16_t result = (int16_t)a + (int16_t)b;
    if (result < -1 || result > 1) return ERR;
    
    return (int8_t)result;
}
```

---

## 4. Testing Requirements

### 4.1 Unit Tests (move from tests/future/ when implemented)

- `tests/future/batch02_gemini_audit_fixes.aria` - 31 test cases covering:
  - Arithmetic with overflow detection
  - ERR sentinel propagation (sticky behavior)
  - Kleene logic operations (AND/OR)
  - Conversion functions with validation

### 4.2 Safety Validation

- ✅ ERR never silently heals to valid value
- ✅ Overflow always detected (never wraps)
- ✅ Logic operations follow Kleene semantics
- ✅ Invalid values impossible to construct

---

## 5. Use Case: Nikola Wave Arithmetic

**Why these types matter**:

```aria
// Nikola 9D wave interference calculation
nit:phase_a = 3;    // Wave state in dimension A
nit:phase_b = -2;   // Wave state in dimension B

// Interference (min for destructive, max for constructive)
nit:destructive = nit_and(phase_a, phase_b);  // = -2 (weaker wins)
nit:constructive = nit_or(phase_a, phase_b);   // = 3 (stronger wins)

// Arithmetic must detect overflow
nit:sum = phase_a + phase_b;  // = 1 (valid)
nit:overflow = 4 + 2;          // = ERR (out of range)

// ERR propagates through calculation chain
nit:calculation = overflow * destructive;  // = ERR (sticky)
```

**This is not exotic** - this is **fundamental wave arithmetic** for the AGI substrate.

---

## 6. Implementation Timeline

| Task | Effort | Priority |
|------|--------|----------|
| Type system integration | 2 days | HIGH |
| Runtime library (C++) | 3 days | HIGH |
| Arithmetic lowering | 3 days | HIGH |
| Kleene logic ops | 2 days | HIGH |
| Testing & validation | 2 days | HIGH |
| Documentation | 1 day | MEDIUM |
| **Total** | **~2 weeks** | **CRITICAL** |

---

## 7. Success Criteria

Move `tests/future/batch02_gemini_audit_fixes.aria` back to `tests/` when:
- ✅ All 31 test cases pass
- ✅ ERR propagation verified
- ✅ No implicit conversions (type safety)
- ✅ Runtime library complete
- ✅ Fuzzer passes on trit/nit operations

**Then**: 363/363 tests passing (100% full coverage)

---

**Remember**: trit/nit are to Nikola what `int` is to C. Not optional. Foundation.
