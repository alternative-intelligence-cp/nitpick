# The `tbb8` Type (Twisted Balanced Binary 8-bit)

**Category**: Types → TBB (Twisted Balanced Binary)  
**Syntax**: `tbb8`  
**Purpose**: Symmetric 8-bit integer with ERR sentinel for sticky error propagation

---

## Overview

`tbb8` is a **Twisted Balanced Binary 8-bit** type with a symmetric range and built-in error handling through a special sentinel value. Unlike standard signed integers that use asymmetric two's complement, TBB types center their range around zero and reserve the minimum value for error signaling.

**⚠️ CRITICAL: This is NOT NEGOTIABLE and fundamental to Aria's design.**

---

## Range and ERR Sentinel

| Aspect | Value | Details |
|--------|-------|---------|
| **Valid Range** | `-127` to `+127` | Symmetric around zero (255 total values) |
| **ERR Sentinel** | `-128` (`0x80`) | Reserved for error state |
| **Total Values** | 256 | 255 normal values + 1 ERR |
| **Storage** | 8 bits | Standard byte size |

### Why -128 is ERR

The minimum value (`-128` / `0x80`) is **permanently reserved** as the ERR sentinel. Any `tbb8` variable with this value is considered to be in an error state. This design choice creates a "sticky error" system where errors automatically propagate through calculations.

---

## Declaration and Initialization

```aria
// Basic declaration
tbb8:value = 42;
tbb8:negative = -100;
tbb8:max = 127;
tbb8:min = -127;  // Minimum VALID value

// Error state
tbb8:error = ERR;  // ERR literal resolves to -128 for tbb8
```

---

## Sticky Error Propagation

The defining feature of TBB types is **sticky error propagation**: once a value becomes ERR, all subsequent arithmetic operations with it produce ERR.

### Arithmetic with ERR

```aria
tbb8:a = 10;
tbb8:b = ERR;

// All operations with ERR produce ERR
tbb8:sum = a + b;      // ERR (10 + ERR = ERR)
tbb8:product = a * b;  // ERR (10 * ERR = ERR)
tbb8:diff = a - b;     // ERR (10 - ERR = ERR)

// ERR is contagious
tbb8:x = 5;
tbb8:y = x + ERR;      // y = ERR
tbb8:z = y + 100;      // z = ERR (once ERR, stays ERR)
```

### Overflow/Underflow Creates ERR

Operations that would exceed the valid range automatically produce ERR:

```aria
tbb8:a = 100;
tbb8:b = 50;

tbb8:sum = a + b;      // ERR (150 exceeds +127 maximum)
tbb8:diff = -100 - 50; // ERR (-150 below -127 minimum)
tbb8:product = 20 * 10; // ERR (200 exceeds +127)
```

---

## Error Detection

### Check for ERR State

```aria
tbb8:value = computeValue();

if (value == ERR) {
    stderr.write("Computation produced an error");
    fail(1);
}

// Or use comparison
tbb8:sentinel = -128;
if (value == sentinel) {
    // Error handling
}
```

### Safe Arithmetic with Validation

```aria
func:safeAdd = Result<tbb8>(tbb8:a, tbb8:b) {
    tbb8:sum = a + b;
    
    if (sum == ERR) {
        fail(1);  // Return error through Result type
    }
    
    pass(sum);
};

// Usage
tbb8:result = safeAdd(100, 50) ? 0;  // result = 0 (overflow detected)
```

---

## Valid Operations

### Supported Operations

| Operation | Behavior | Example |
|-----------|----------|---------|
| Addition (`+`) | Produces ERR on overflow | `a + b` |
| Subtraction (`-`) | Produces ERR on underflow | `a - b` |
| Multiplication (`*`) | Produces ERR on overflow | `a * b` |
| Division (`/`) | Produces ERR on div-by-zero | `a / b` |
| Modulo (`%`) | Produces ERR on div-by-zero | `a % b` |
| Comparison (`<`, `>`, `==`, `!=`) | ERR compares equal only to ERR | `a == ERR` |
| Assignment (`=`) | Direct assignment | `a = ERR` |

### Division by Zero

```aria
tbb8:a = 10;
tbb8:b = 0;

tbb8:quotient = a / b;  // ERR (division by zero)
```

---

## Comparison Behavior

```aria
tbb8:err1 = ERR;
tbb8:err2 = ERR;
tbb8:valid = 42;

// ERR equals ERR
bool:same = (err1 == err2);  // true

// ERR not equal to valid values
bool:diff = (err1 == valid);  // false

// Ordering comparisons with ERR are implementation-defined
// Best practice: always check for ERR explicitly before comparisons
```

---

## Use Cases

### 1. Overflow Detection in Calculations

```aria
func:calculateScore = tbb8(tbb8:base, tbb8:bonus) {
    tbb8:total = base + bonus;
    
    if (total == ERR) {
        stderr.write("Score overflow!");
        pass(127);  // Cap at maximum
    }
    
    pass(total);
};
```

### 2. Error Propagation in Pipelines

```aria
tbb8:value = 10;

// Error propagates through entire pipeline
tbb8:result = value
    |> ($ + 50)     // 60
    |> ($ * 3)      // 180 -> ERR (overflow)
    |> ($ - 10);    // ERR (propagates)

if (result == ERR) {
    stderr.write("Pipeline failed due to overflow");
}
```

### 3. Bounded Integer Arithmetic

```aria
// Counters that must stay in valid range
tbb8:lives = 3;
tbb8:damage = -1;

lives = lives + damage;  // 2

// If lives become ERR, player state is invalid
if (lives == ERR) {
    gameOver();
}
```

---

## Conversion and Casting

### To/From Standard Integers

```aria
// From int8 (may produce ERR)
int8:largeValue = -128;
tbb8:converted = largeValue;  // ERR (out of range)

int8:validValue = 100;
tbb8:ok = validValue;  // 100 (in range)

// To int8
tbb8:tbbVal = 42;
int8:intVal = tbbVal;  // 42
```

### Validation on Conversion

```aria
func:toTBB8 = Result<tbb8>(int8:value) {
    if (value < -127 || value > 127) {
        fail(1);  // Out of range
    }
    pass(value);
};

tbb8:safe = toTBB8(-128) ? 0;  // 0 (conversion failed)
```

---

## Common Patterns

### Pattern 1: Check-Then-Use

```aria
tbb8:value = computeValue();

if (value == ERR) {
    // Handle error state
    fail(1);
}

// Safe to use value here
processValue(value);
```

### Pattern 2: Default on Error

```aria
tbb8:value = riskyOperation();
tbb8:safe = (value == ERR) is 0 : value;  // Use 0 if ERR
```

### Pattern 3: Error Propagation

```aria
func:compute = Result<tbb8>(tbb8:input) {
    tbb8:step1 = input * 2;
    if (step1 == ERR) { fail(1); }
    
    tbb8:step2 = step1 + 50;
    if (step2 == ERR) { fail(2); }
    
    pass(step2);
};
```

---

## Gotchas and Edge Cases

### ❌ Don't Use -128 as a Valid Value

```aria
// WRONG: -128 is ERR, not a valid value
tbb8:wrong = -128;  // This is ERR!

// CORRECT: Use -127 as minimum
tbb8:minimum = -127;
```

### ❌ Don't Ignore ERR in Comparisons

```aria
tbb8:value = computeValue();

// WRONG: May compare against ERR without checking
if (value > 100) {  // What if value is ERR?
    // ...
}

// CORRECT: Check for ERR first
if (value != ERR && value > 100) {
    // ...
}
```

### ✅ ERR is Sticky - Use It!

```aria
// Error handling made simple through stickiness
tbb8:result = input
    |> validateInput
    |> processStep1
    |> processStep2
    |> finalizeOutput;

// Single check at the end catches any error in the chain
if (result == ERR) {
    handleError();
}
```

---

## Relationship to Other TBB Types

- **`tbb16`**: 16-bit version, range [-32767, +32767], ERR = -32768
- **`tbb32`**: 32-bit version, range [-2147483647, +2147483647], ERR = min value
- **`tbb64`**: 64-bit version, symmetric range, ERR = min value

All TBB types share the **sticky error propagation** behavior, making them interchangeable in error-handling patterns.

---

## Related Topics

- [tbb16](tbb16.md) - 16-bit TBB type
- [tbb32](tbb32.md) - 32-bit TBB type  
- [tbb64](tbb64.md) - 64-bit TBB type
- [ERR Sentinel](../advanced_features/err_sentinel.md) - ERR literal and semantics
- [result Type](result.md) - Aria's primary error handling mechanism
- [Overflow Detection](../advanced_features/overflow_detection.md)

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Lines 18-21  
**Non-Negotiable**: ERR sentinel and sticky error propagation are core language features
