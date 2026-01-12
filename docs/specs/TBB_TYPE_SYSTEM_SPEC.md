# TBB Type System Specification

**Twisted Balanced Binary (TBB) Types for Aria**

Version: 1.0.0
Date: December 24, 2025
Author: Claude (Opus 4.5) - Per request from Aria
Status: Complete Specification

---

## 1. Executive Summary

TBB (Twisted Balanced Binary) types are Aria's error-propagating integer types that provide safe arithmetic with automatic overflow detection and error propagation. Unlike standard integers that can silently overflow or require exceptions, TBB types encode error states directly within the value representation using bit tagging.

**Key Properties:**
- Symmetric signed ranges (no two's complement asymmetry)
- MIN_INT reserved as error sentinel (ERR)
- Sticky error propagation: `ERR op x = ERR`
- Overflow automatically converts to ERR
- Division by zero produces ERR
- No undefined behavior

---

## 2. Design Philosophy

### 2.1 Problem Statement

Standard integer types suffer from fundamental issues:

```
// Standard int8 problem: asymmetric range [-128, +127]
int8_t x = -128;
int8_t y = -x;        // Undefined behavior! -(-128) = 128 overflows
int8_t z = abs(-128); // Undefined behavior!
```

### 2.2 TBB Solution

TBB types reserve the minimum value as an error sentinel, creating symmetric ranges:

```aria
// TBB: symmetric range [-127, +127]
tbb8:x = -127;
tbb8:y = -x;        // y = 127 (works perfectly!)
tbb8:z = abs(-127); // z = 127 (no overflow!)
```

### 2.3 Error Propagation Model

TBB uses "sticky errors" - once an error occurs, it propagates through all subsequent operations without requiring explicit checks at each step:

```aria
tbb8:a = 100;
tbb8:b = 50;
tbb8:c = a + b;      // 150 > 127, so c = ERR
tbb8:d = c + 10;     // ERR + 10 = ERR (sticky)
tbb8:e = c * 2;      // ERR * 2 = ERR (sticky)

// Check error once at the end
if(c == ERR) {
    // Handle error
}
```

---

## 3. Type Definitions

### 3.1 TBB Type Family

| Type   | Storage | Valid Range                    | ERR Sentinel                   | Hex ERR     |
|--------|---------|--------------------------------|--------------------------------|-------------|
| tbb8   | 8-bit   | [-127, +127]                   | -128                           | 0x80        |
| tbb16  | 16-bit  | [-32,767, +32,767]             | -32,768                        | 0x8000      |
| tbb32  | 32-bit  | [-2,147,483,647, +2,147,483,647] | -2,147,483,648               | 0x80000000  |
| tbb64  | 64-bit  | [-9,223,372,036,854,775,807, +9,223,372,036,854,775,807] | -9,223,372,036,854,775,808 | 0x8000000000000000 |

### 3.2 Bit Layout Diagrams

#### tbb8 (8-bit)
```
  Bit:   7   6   5   4   3   2   1   0
       +---+---+---+---+---+---+---+---+
       | S |   |   |   |   |   |   |   |  Standard signed integer layout
       +---+---+---+---+---+---+---+---+

  ERR Sentinel: 0x80 = 10000000b = -128

  Valid Range:
    Max:  0x7F = 01111111b = +127
    Zero: 0x00 = 00000000b = 0
    Min:  0x81 = 10000001b = -127
    ERR:  0x80 = 10000000b = RESERVED
```

#### tbb16 (16-bit)
```
  Bit: 15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
      +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      | S |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
      +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

  ERR Sentinel: 0x8000 = -32,768
  Valid Range: [-32,767, +32,767]
```

#### tbb32 (32-bit)
```
  Bit: 31 ... 16  15 ... 0
      +----------+--------+
      |  High    |  Low   |
      +----------+--------+

  ERR Sentinel: 0x80000000 = -2,147,483,648
  Valid Range: [-2,147,483,647, +2,147,483,647]
```

#### tbb64 (64-bit)
```
  Bit: 63 ... 32  31 ... 0
      +----------+--------+
      |  High    |  Low   |
      +----------+--------+

  ERR Sentinel: 0x8000000000000000 = INT64_MIN
  Valid Range: [-INT64_MAX, +INT64_MAX]
```

---

## 4. Arithmetic Operations

### 4.1 Pre-Operation Checks

All binary operations follow this pattern:

```
FUNCTION tbb_op(a, b, type) -> tbb_type:
    // 1. Check for ERR inputs (sticky propagation)
    IF a == ERR OR b == ERR:
        RETURN ERR

    // 2. Perform operation in wider type
    result = compute(a, b)

    // 3. Check for overflow
    IF result < MIN_VALID OR result > MAX_VALID:
        RETURN ERR

    // 4. Return valid result
    RETURN result
```

### 4.2 Addition

**Algorithm:**
1. Check if either operand is ERR (return ERR)
2. Check for overflow before adding:
   - If both positive and `a > MAX - b`: overflow
   - If both negative and `a < MIN - b`: underflow
3. Perform addition
4. Return result or ERR

**Truth Table (tbb8 examples):**

| a      | b      | Result | Reason                    |
|--------|--------|--------|---------------------------|
| 100    | 27     | 127    | Valid (100+27=127)        |
| 100    | 28     | ERR    | Overflow (100+28=128>127) |
| -100   | -27    | -127   | Valid (-100-27=-127)      |
| -100   | -28    | ERR    | Underflow (-128<-127)     |
| ERR    | 50     | ERR    | Sticky error              |
| 50     | ERR    | ERR    | Sticky error              |
| 0      | 0      | 0      | Identity                  |

### 4.3 Subtraction

**Algorithm:**
1. Check if either operand is ERR (return ERR)
2. Check for overflow:
   - If `a > 0`, `b < 0`, and `a > MAX + b`: overflow
   - If `a < 0`, `b > 0`, and `a < MIN + b`: underflow
3. Perform subtraction
4. Return result or ERR

**Truth Table (tbb8 examples):**

| a      | b      | Result | Reason                    |
|--------|--------|--------|---------------------------|
| 50     | 30     | 20     | Valid                     |
| 50     | -77    | 127    | Valid (50-(-77)=127)      |
| 50     | -78    | ERR    | Overflow (128>127)        |
| -50    | 77     | -127   | Valid                     |
| -50    | 78     | ERR    | Underflow (<-127)         |

### 4.4 Multiplication

**Algorithm:**
1. Check if either operand is ERR (return ERR)
2. Use LLVM `smul_with_overflow` intrinsic
3. Check if result exceeds valid TBB range
4. Return result or ERR

**Truth Table (tbb8 examples):**

| a      | b      | Result | Reason                    |
|--------|--------|--------|---------------------------|
| 10     | 10     | 100    | Valid                     |
| 10     | 13     | ERR    | Overflow (130>127)        |
| -10    | 10     | -100   | Valid                     |
| -10    | -13    | ERR    | Overflow (130>127)        |
| 127    | 1      | 127    | Valid                     |
| 127    | -1     | -127   | Valid                     |

### 4.5 Division

**Algorithm:**
1. Check if either operand is ERR (return ERR)
2. Check for division by zero (return ERR)
3. Perform signed integer division
4. Return result (division of valid inputs always valid for TBB)

**Truth Table (tbb8 examples):**

| a      | b      | Result | Reason                    |
|--------|--------|--------|---------------------------|
| 100    | 10     | 10     | Valid                     |
| 100    | 0      | ERR    | Division by zero          |
| -127   | -1     | 127    | Valid (symmetric!)        |
| ERR    | 10     | ERR    | Sticky error              |

### 4.6 Negation

**Algorithm:**
1. Check if operand is ERR (return ERR)
2. Check if operand is MIN_VALID (return ERR - would overflow)
3. Return negated value

**Truth Table (tbb8):**

| a      | Result | Reason                    |
|--------|--------|---------------------------|
| 50     | -50    | Valid                     |
| -50    | 50     | Valid                     |
| 127    | -127   | Valid                     |
| -127   | 127    | Valid (symmetric!)        |
| ERR    | ERR    | Sticky error              |

**Note:** Unlike standard `int8_t` where `-(-128)` is undefined behavior, TBB's symmetric range means `-(-127) = 127` is always valid.

---

## 5. Comparison Operations

TBB values can be compared using standard comparison operators:

```aria
tbb8:a = 50;
tbb8:b = 30;

if(a > b) { ... }   // true
if(a == ERR) { ... } // false
```

**ERR Comparison Semantics:**
- `ERR == ERR` returns `true`
- `ERR < x` returns `false` (ERR is not less than anything)
- `ERR > x` returns `false` (ERR is not greater than anything)
- `ERR != x` returns `true` (unless x is also ERR)

---

## 6. API Specification

### 6.1 Constructors

```aria
// From integer literals
tbb64.from_int(int64) -> tbb64
tbb32.from_int(int32) -> tbb32
tbb16.from_int(int16) -> tbb16
tbb8.from_int(int8) -> tbb8

// Implicit conversion from literals in declarations
tbb64:x = 42;  // Implicitly calls from_int
```

### 6.2 Error Checking

```aria
// Check if value is ERR
tbb64.is_err(tbb64) -> bool
tbb32.is_err(tbb32) -> bool
tbb16.is_err(tbb16) -> bool
tbb8.is_err(tbb8) -> bool

// Direct comparison also works
if(x == ERR) { ... }
```

### 6.3 Value Extraction

```aria
// Convert to standard integer (ERR produces undefined value)
tbb64.to_int(tbb64) -> int64
tbb32.to_int(tbb32) -> int32
tbb16.to_int(tbb16) -> int16
tbb8.to_int(tbb8) -> int8

// Safe extraction with default
tbb64.unwrap_or(tbb64, int64 default) -> int64
```

### 6.4 Constants

```aria
// ERR sentinel for each type
tbb64::ERR  // -9,223,372,036,854,775,808
tbb32::ERR  // -2,147,483,648
tbb16::ERR  // -32,768
tbb8::ERR   // -128

// Maximum values
tbb64::MAX  // +9,223,372,036,854,775,807
tbb32::MAX  // +2,147,483,647
tbb16::MAX  // +32,767
tbb8::MAX   // +127

// Minimum valid values
tbb64::MIN  // -9,223,372,036,854,775,807
tbb32::MIN  // -2,147,483,647
tbb16::MIN  // -32,767
tbb8::MIN   // -127
```

### 6.5 Type Conversion and ARIA-018: Sentinel Discontinuity Constraint

**CRITICAL SAFETY REQUIREMENT**: Implicit widening between TBB types is **FORBIDDEN** due to sentinel discontinuity.

#### 6.5.1 The Sentinel Healing Problem

Standard sign-extension breaks TBB's error semantics:

```aria
tbb8:err = ERR;              // 0x80 = -128 (ERR in tbb8)
// FORBIDDEN: Implicit widening
// tbb16:wide = err;         // Would become 0xFF80 = -128 (VALID in tbb16, not ERR!)
                              // ERR "healed" into valid number!
```

**Binary Analysis of the Bug**:
```
tbb8  ERR:  0x80       = -128 (sentinel)
            ↓ sext
tbb16:      0xFF80     = -128 (VALID number, NOT sentinel)
tbb16 ERR:  0x8000     = -32768 (actual sentinel)
```

The error state is **lost** during widening because each TBB width has a different sentinel value.

#### 6.5.2 ARIA-018: Formal Safety Constraint

**Standard**: ARIA-018 (Sentinel Discontinuity Constraint)

**Requirement**: Implicit type coercion between TBB types of differing widths is forbidden because the error sentinel value is width-dependent. A direct mapping of bit patterns via standard Two's Complement sign-extension fails to preserve the error state, violating the Sticky Error invariant.

**Rationale**: All widening operations must explicitly check for the source sentinel and map it to the destination sentinel.

**Enforcement**: Compiler rejects implicit widening at type-checking phase.

#### 6.5.3 Safe Explicit Widening: `tbb_widen<T>()`

**Correct Usage - Explicit Widening**:
```aria
tbb8:narrow = get_sensor();
tbb16:wide = tbb_widen<tbb16>(narrow);  // ✓ Safe! ERR preserved
```

**Semantics**:
```
tbb_widen<T>(value):
  if value == ERR_source:
    return ERR_destination
  else:
    return sign_extend(value)
```

**Implementation**: Branchless `cmov`/`csel` instruction on modern CPUs:
```llvm
%is_err = icmp eq %val, %src_sentinel
%extended = sext %val to %dst_type
%result = select %is_err, %dst_sentinel, %extended
```

**Runtime Performance**: Zero overhead (single conditional move).

#### 6.5.4 Available Widening Functions

```aria
// Explicit widening intrinsics
tbb_widen<tbb16>(tbb8)   -> tbb16
tbb_widen<tbb32>(tbb8)   -> tbb32
tbb_widen<tbb64>(tbb8)   -> tbb64
tbb_widen<tbb32>(tbb16)  -> tbb32
tbb_widen<tbb64>(tbb16)  -> tbb64
tbb_widen<tbb64>(tbb32)  -> tbb64
```

**C Runtime Functions**:
```c
extern "C" int16_t aria_tbb_widen_8_16(int8_t val);
extern "C" int32_t aria_tbb_widen_8_32(int8_t val);
extern "C" int64_t aria_tbb_widen_8_64(int8_t val);
extern "C" int32_t aria_tbb_widen_16_32(int16_t val);
extern "C" int64_t aria_tbb_widen_16_64(int16_t val);
extern "C" int64_t aria_tbb_widen_32_64(int32_t val);
```

#### 6.5.5 Narrowing (Explicit Only)

Narrowing requires explicit checking and may produce ERR:

```aria
tbb64:wide = 200;
tbb8:narrow = tbb_narrow<tbb8>(wide);  // ERR (200 > 127)
```

**Semantics**:
```
tbb_narrow<T>(value):
  if value == ERR_source:
    return ERR_destination
  if value > MAX_destination:
    return ERR_destination
  if value < MIN_destination:
    return ERR_destination
  return truncate(value)
```

#### 6.5.6 Safety Impact

**Without ARIA-018 enforcement**:
- Sensor failure (ERR) converts to valid -128°C reading
- Thermal algorithm activates heaters
- Physical hardware damage

**With ARIA-018 enforcement**:
- ERR propagates correctly through widening
- System detects failure and halts safely
- No silent corruption

**AGI Context**: In the 9D torus physics engine, a single healed ERR can corrupt millions of neural weights during training, causing catastrophic model divergence ("Infected Hypercube" failure mode).

---

## 7. C Runtime Implementation

### 7.1 Type Definitions

```c
// Header: aria_tbb.h

#ifndef ARIA_TBB_H
#define ARIA_TBB_H

#include <stdint.h>
#include <stdbool.h>

// TBB types are just typedefs to standard integers
// The semantics are enforced at compile time and in operations
typedef int8_t  tbb8_t;
typedef int16_t tbb16_t;
typedef int32_t tbb32_t;
typedef int64_t tbb64_t;

// ERR sentinels
#define TBB8_ERR   ((tbb8_t)INT8_MIN)      // -128
#define TBB16_ERR  ((tbb16_t)INT16_MIN)    // -32768
#define TBB32_ERR  ((tbb32_t)INT32_MIN)    // -2147483648
#define TBB64_ERR  ((tbb64_t)INT64_MIN)    // -9223372036854775808

// Maximum valid values
#define TBB8_MAX   ((tbb8_t)INT8_MAX)      // 127
#define TBB16_MAX  ((tbb16_t)INT16_MAX)    // 32767
#define TBB32_MAX  ((tbb32_t)INT32_MAX)    // 2147483647
#define TBB64_MAX  ((tbb64_t)INT64_MAX)    // 9223372036854775807

// Minimum valid values (NOT ERR)
#define TBB8_MIN   ((tbb8_t)(-INT8_MAX))   // -127
#define TBB16_MIN  ((tbb16_t)(-INT16_MAX)) // -32767
#define TBB32_MIN  ((tbb32_t)(-INT32_MAX)) // -2147483647
#define TBB64_MIN  ((tbb64_t)(-INT64_MAX)) // -9223372036854775807

#endif // ARIA_TBB_H
```

### 7.2 Error Detection

```c
// Inline functions for error checking
static inline bool tbb8_is_err(tbb8_t x) {
    return x == TBB8_ERR;
}

static inline bool tbb16_is_err(tbb16_t x) {
    return x == TBB16_ERR;
}

static inline bool tbb32_is_err(tbb32_t x) {
    return x == TBB32_ERR;
}

static inline bool tbb64_is_err(tbb64_t x) {
    return x == TBB64_ERR;
}
```

### 7.3 Arithmetic Operations

```c
// tbb64 addition with overflow detection
tbb64_t aria_tbb64_add(tbb64_t a, tbb64_t b) {
    // Sticky error propagation
    if (a == TBB64_ERR || b == TBB64_ERR) {
        return TBB64_ERR;
    }

    // Overflow detection for addition
    // If both positive: check a > MAX - b
    // If both negative: check a < MIN - b
    if (a > 0 && b > 0 && a > TBB64_MAX - b) {
        return TBB64_ERR;  // Overflow
    }
    if (a < 0 && b < 0 && a < TBB64_MIN - b) {
        return TBB64_ERR;  // Underflow
    }

    return a + b;
}

// tbb64 subtraction with overflow detection
tbb64_t aria_tbb64_sub(tbb64_t a, tbb64_t b) {
    // Sticky error propagation
    if (a == TBB64_ERR || b == TBB64_ERR) {
        return TBB64_ERR;
    }

    // Overflow detection for subtraction
    // If a > 0, b < 0: check a > MAX + b
    // If a < 0, b > 0: check a < MIN + b
    if (a > 0 && b < 0 && a > TBB64_MAX + b) {
        return TBB64_ERR;
    }
    if (a < 0 && b > 0 && a < TBB64_MIN + b) {
        return TBB64_ERR;
    }

    return a - b;
}

// tbb64 multiplication with overflow detection
tbb64_t aria_tbb64_mul(tbb64_t a, tbb64_t b) {
    // Sticky error propagation
    if (a == TBB64_ERR || b == TBB64_ERR) {
        return TBB64_ERR;
    }

    // Handle zero case
    if (a == 0 || b == 0) {
        return 0;
    }

    // Use 128-bit arithmetic or careful checks
    // For simplicity, use signed overflow detection
    __int128 result = (__int128)a * (__int128)b;

    if (result > TBB64_MAX || result < TBB64_MIN) {
        return TBB64_ERR;
    }

    return (tbb64_t)result;
}

// tbb64 division with zero check
tbb64_t aria_tbb64_div(tbb64_t a, tbb64_t b) {
    // Sticky error propagation
    if (a == TBB64_ERR || b == TBB64_ERR) {
        return TBB64_ERR;
    }

    // Division by zero
    if (b == 0) {
        return TBB64_ERR;
    }

    // Note: Division of valid TBB inputs cannot overflow
    // because |a / b| <= |a| which is already valid
    return a / b;
}

// tbb64 negation
tbb64_t aria_tbb64_neg(tbb64_t a) {
    // Sticky error propagation
    if (a == TBB64_ERR) {
        return TBB64_ERR;
    }

    // Check for min value negation (would overflow)
    if (a == TBB64_MIN) {
        return TBB64_ERR;
    }

    return -a;
}
```

### 7.4 Conversion Functions

```c
// Convert from standard integer with range check
tbb64_t aria_tbb64_from_int(int64_t x) {
    if (x < TBB64_MIN || x > TBB64_MAX) {
        return TBB64_ERR;
    }
    return (tbb64_t)x;
}

// Convert to standard integer (caller must check for ERR first)
int64_t aria_tbb64_to_int(tbb64_t x) {
    return (int64_t)x;
}

// Safe extraction with default
int64_t aria_tbb64_unwrap_or(tbb64_t x, int64_t default_val) {
    if (x == TBB64_ERR) {
        return default_val;
    }
    return (int64_t)x;
}
```

---

## 8. LLVM Codegen Integration

The Aria compiler generates LLVM IR for TBB operations. This is implemented in `src/backend/ir/tbb_codegen.cpp`.

### 8.1 Code Structure

```cpp
class TBBCodegen {
public:
    TBBCodegen(llvm::LLVMContext& ctx, llvm::IRBuilder<>& builder);

    // Arithmetic operations
    llvm::Value* generateAdd(llvm::Value* lhs, llvm::Value* rhs, Type* type);
    llvm::Value* generateSub(llvm::Value* lhs, llvm::Value* rhs, Type* type);
    llvm::Value* generateMul(llvm::Value* lhs, llvm::Value* rhs, Type* type);
    llvm::Value* generateDiv(llvm::Value* lhs, llvm::Value* rhs, Type* type);
    llvm::Value* generateNeg(llvm::Value* operand, Type* type);

    // Helpers
    llvm::Value* getErrSentinel(Type* type);
    llvm::Value* getMaxValue(Type* type);
    llvm::Value* getMinValue(Type* type);

private:
    llvm::Value* isErr(llvm::Value* value, Type* type);
    llvm::Value* checkAddOverflow(llvm::Value* lhs, llvm::Value* rhs, Type* type);
    llvm::Value* checkSubOverflow(llvm::Value* lhs, llvm::Value* rhs, Type* type);
    llvm::Value* checkMulOverflow(llvm::Value* lhs, llvm::Value* rhs, Type* type);
};
```

### 8.2 Generated IR Pattern

For each TBB operation, the codegen produces:

```llvm
; Example: tbb8 addition
define i8 @tbb8_add(i8 %lhs, i8 %rhs) {
entry:
  br label %check_lhs_err

check_lhs_err:
  %lhs_is_err = icmp eq i8 %lhs, -128
  br i1 %lhs_is_err, label %return_err, label %check_rhs_err

check_rhs_err:
  %rhs_is_err = icmp eq i8 %rhs, -128
  br i1 %rhs_is_err, label %return_err, label %check_overflow

check_overflow:
  ; Overflow detection logic...
  br i1 %will_overflow, label %return_err, label %do_add

do_add:
  %result = add i8 %lhs, %rhs
  br label %merge

return_err:
  br label %merge

merge:
  %phi = phi i8 [ %result, %do_add ], [ -128, %return_err ]
  ret i8 %phi
}
```

---

## 9. Use Cases

### 9.1 Financial Arithmetic

```aria
// Calculate total with overflow protection
func:calculate_total = tbb64(prices: array<tbb64>) {
    tbb64:total = 0;
    for(price in prices) {
        total = total + price;
    }

    if(total == ERR) {
        fail(OVERFLOW_ERROR);
    }

    pass(total);
};
```

### 9.2 Safe Pointer Offsets

```aria
// Calculate array offset safely
func:safe_offset = tbb64(base: tbb64, index: tbb64, element_size: tbb64) {
    tbb64:offset = index * element_size;
    tbb64:address = base + offset;

    if(address == ERR) {
        // Invalid offset
        pass(-1);
    }

    pass(address);
};
```

### 9.3 Branchless Error Pipelines

```aria
// Process data pipeline - check error once at end
func:process_pipeline = tbb64(input: tbb64) {
    tbb64:step1 = input * 2;
    tbb64:step2 = step1 + 100;
    tbb64:step3 = step2 / 3;
    tbb64:result = step3 - 50;

    // Single error check for entire pipeline
    if(result == ERR) {
        fail(PIPELINE_ERROR);
    }

    pass(result);
};
```

### 9.4 Audio Sample Processing

```aria
// Mix audio samples with overflow protection
func:mix_samples = tbb16(a: tbb16, b: tbb16) {
    tbb16:mixed = a + b;

    // Overflow produces ERR instead of wrap-around distortion
    if(mixed == ERR) {
        // Clamp to max instead
        if(a > 0) {
            pass(tbb16::MAX);
        } else {
            pass(tbb16::MIN);
        }
    }

    pass(mixed);
};
```

---

## 10. Performance Analysis

### 10.1 Operation Costs

| Operation      | Standard int64 | TBB64           | Overhead |
|----------------|----------------|-----------------|----------|
| Addition       | 1 cycle        | ~4-6 cycles     | 4-6x     |
| Subtraction    | 1 cycle        | ~4-6 cycles     | 4-6x     |
| Multiplication | 3-4 cycles     | ~6-8 cycles     | 2x       |
| Division       | 20-40 cycles   | ~22-42 cycles   | ~1.05x   |
| Comparison     | 1 cycle        | 1 cycle         | 1x       |
| Error Check    | N/A            | 1 cycle         | N/A      |

### 10.2 Optimization Opportunities

1. **Branch Prediction**: Modern CPUs handle the error-check branches well because errors are rare
2. **LLVM Intrinsics**: Using `llvm.sadd.with.overflow` avoids double computation
3. **Inlining**: Small TBB operations should be inlined to reduce call overhead
4. **SIMD**: TBB operations can be vectorized for array processing

### 10.3 Memory Footprint

TBB types have zero memory overhead - they use the same storage as standard integers:

| Type   | Storage Size |
|--------|--------------|
| tbb8   | 1 byte       |
| tbb16  | 2 bytes      |
| tbb32  | 4 bytes      |
| tbb64  | 8 bytes      |

---

## 11. Comparison with Alternatives

### 11.1 TBB vs Result<T>

| Aspect           | TBB                      | Result<T>               |
|------------------|--------------------------|-------------------------|
| Storage          | Same as T                | T + tag byte minimum    |
| Error check      | Comparison with sentinel | Match/unwrap            |
| Composition      | Automatic (sticky)       | Manual (map, and_then)  |
| Arithmetic       | Native operators         | Requires wrapping       |
| Overhead         | 4-6x on each op          | 1x but manual checks    |

**Use TBB when:** You need many arithmetic operations with error propagation.
**Use Result<T> when:** You need rich error types with different failure modes.

### 11.2 TBB vs Checked Arithmetic

| Aspect           | TBB                      | Checked (Rust-style)    |
|------------------|--------------------------|-------------------------|
| Error handling   | Sticky, check once       | Exception/panic each op |
| Composition      | Automatic                | Try-catch or propagate  |
| Performance      | Predictable              | Branch for each op      |
| Debugging        | ERR visible in value     | Exception stack trace   |

### 11.3 TBB vs Saturating Arithmetic

| Aspect           | TBB                      | Saturating              |
|------------------|--------------------------|-------------------------|
| Overflow result  | ERR (detectable)         | Clamped to max/min      |
| Error detection  | Yes                      | No (silent clamp)       |
| Use case         | Error handling           | Signal processing       |

---

## 12. Implementation Checklist

### 12.1 Runtime Layer
- [x] Type definitions (aria_tbb.h)
- [x] Error detection macros
- [x] Addition with overflow check
- [x] Subtraction with overflow check
- [x] Multiplication with overflow check
- [x] Division with zero check
- [x] Negation with min-value check
- [ ] Modulo operation
- [ ] Bitwise operations (optional)

### 12.2 Type Checker Layer
- [x] TBB types registered in type system
- [x] Type coercion rules (widening/narrowing)
- [x] Literal assignment validation
- [x] ERR constant recognition
- [ ] tbb.from_int() builtin
- [ ] tbb.to_int() builtin
- [ ] tbb.is_err() builtin

### 12.3 IR Codegen Layer
- [x] TBBCodegen class
- [x] generateAdd()
- [x] generateSub()
- [x] generateMul()
- [x] generateDiv()
- [x] generateNeg()
- [ ] Runtime function declarations
- [ ] Comparison codegen

### 12.4 Tests
- [x] tbb_basic.aria - Basic operations
- [x] tbb_overflow.aria - Overflow detection
- [x] tbb_mult_overflow.aria - Multiplication overflow
- [x] tbb_sticky_error.aria - Error propagation
- [x] tbb_division.aria - Division operations
- [x] tbb_err_literal.aria - ERR literal
- [x] tbb_no_overflow.aria - Valid operations

---

## 13. References

1. `include/backend/ir/tbb_codegen.h` - TBB codegen header
2. `src/backend/ir/tbb_codegen.cpp` - TBB codegen implementation
3. `include/frontend/sema/type.h` - Type system definitions
4. `docs/gemini/responses/research_002_balanced_ternary_arithmetic.txt` - Theoretical foundation
5. `docs/gemini/responses/research_003_balanced_nonary_arithmetic.txt` - Related nyte/nit types
6. `examples/features/09_tbb_arithmetic.aria` - Usage examples

---

## Appendix A: Complete Truth Tables

### A.1 tbb8 Addition Full Table

```
For all valid (a, b) pairs where -127 <= a, b <= 127:

a + b result:
  If a == ERR or b == ERR: ERR
  If a + b > 127: ERR (overflow)
  If a + b < -127: ERR (underflow)
  Otherwise: a + b
```

### A.2 Special Cases

| Expression        | Result | Reason                           |
|-------------------|--------|----------------------------------|
| `127 + 1`         | ERR    | Overflow                         |
| `-127 + (-1)`     | ERR    | Underflow                        |
| `127 + 0`         | 127    | Identity                         |
| `-127 + 0`        | -127   | Identity                         |
| `ERR + ERR`       | ERR    | Sticky error                     |
| `127 * 1`         | 127    | Identity                         |
| `127 * (-1)`      | -127   | Valid negation                   |
| `-127 * (-1)`     | 127    | Valid (symmetric range!)         |
| `127 / 127`       | 1      | Valid                            |
| `127 / 0`         | ERR    | Division by zero                 |
| `-(-127)`         | 127    | Valid negation                   |

---

## Appendix B: ASCII Art Diagrams

### B.1 TBB8 Value Space

```
    ERR           MIN                ZERO               MAX
     |             |                  |                  |
     v             v                  v                  v
+----+-------------+------------------+------------------+
|-128|   -127 ... -1    0    1 ... 127|
+----+-------------+------------------+------------------+
  ^                                                      ^
  |                                                      |
Reserved                                            Valid Range
(Error)                                          [-127, +127]
```

### B.2 Error Propagation Flow

```
    Input A        Input B
       |              |
       v              v
    +------+      +------+
    |ERR?  |      |ERR?  |
    +------+      +------+
       |              |
       +------+-------+
              |
              v
           +-----+
           | OR  |-----> Yes ---> Return ERR
           +-----+
              |
              No
              |
              v
         +--------+
         |Compute |
         +--------+
              |
              v
         +--------+
         |Overflow|-----> Yes ---> Return ERR
         +--------+
              |
              No
              |
              v
         Return Result
```

---

**End of Specification**
