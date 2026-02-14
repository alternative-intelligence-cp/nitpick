# tbb32: 32-bit Twisted Balanced Binary Integer

## Overview

`tbb32` is a 32-bit **Twisted Balanced Binary** integer type with a perfectly symmetric range `[-2147483647, +2147483647]` and a reserved error sentinel value at `-2147483648` (INT32_MIN). `tbb32` is the **general-purpose** TBB type for most applications, offering a wide range while maintaining automatic error propagation through the **sticky ERR sentinel mechanism**.

**Key characteristics:**
- **Range:** `-2147483647` to `+2147483647` (inclusive)
- **ERR sentinel:** `-2147483648` (bit pattern `0x80000000`, INT32_MIN)
- **Sticky error propagation:** Any operation involving `ERR` produces `ERR`
- **Symmetric range:** Eliminates asymmetry bugs (`abs(-2147483647)` always works)
- **Foundation type:** Used as building block for `frac32` (exact rationals), safety-critical calculations, general arithmetic
- **General purpose:** Wide enough for most applications, narrow enough to be efficient

```aria
tbb32:distance = 1500000;              // Valid range value
tbb32:max_positive = 2147483647;       // Maximum positive value (INT32_MAX)
tbb32:max_negative = -2147483647;      // Maximum negative value (symmetric!)
tbb32:error_state = ERR;               // Error sentinel (-2147483648 / 0x80000000)
```

## Purpose and Philosophy

`tbb32` is the **default choice** for TBB arithmetic when range requirements are uncertain. It provides sufficient range for most applications while maintaining the safety guarantees of the TBB family.

### Why tbb32?

**Use `tbb32` when:**
- General-purpose arithmetic with error detection
- Building `frac32` for exact rationals (common use case)
- Financial calculations (amounts in smallest units, e.g., microcents)
- Physics simulations with integer millimeter/micrometer precision
- Counters, indices, identifiers that need overflow detection
- Default choice when narrower types are insufficient

**Range comparison:**
- `tbb8`: `[-127, +127]` (256 values)
- `tbb16`: `[-32767, +32767]` (65K values)
- `tbb32`: `[-2147483647, +2147483647]` (~4.3 billion values)
- `tbb64`: Large 64-bit symmetric range

### Asymmetry Elimination (32-bit)

```aria
// Standard i32 problems:
i32:val = -2147483648;     // INT32_MIN
i32:negated = -val;        // Overflow! -(-2147483648) = 2147483648, but max is 2147483647
i32:absolute = abs(val);   // Same problem! abs(-2147483648) wraps to -2147483648

// tbb32 solution:
tbb32:val = -2147483647;   // Symmetric range, -2147483648 reserved for ERR
tbb32:negated = -val;      // 2147483647 ✓ Always works within range!
tbb32:absolute = abs(val); // 2147483647 ✓ No overflow possible!
```

### Sticky Error Propagation

```aria
tbb32:a = 2000000000;
tbb32:b = 500000000;
tbb32:sum = a + b;         // 2500000000 exceeds 2147483647 → sum = ERR
tbb32:product = sum * 2;   // ERR * 2 → ERR (propagates!)
tbb32:final = product + 1000; // ERR + 1000 → ERR (still propagates!)

// Check error once at the end:
if (final == ERR) {
    io.error("Calculation overflow detected");
}
```

## Syntax and Literals

```aria
// Decimal literals with type suffix
tbb32:value = 1500000tbb32;        // Explicit type suffix
tbb32:pos = 2147483647tbb32;       // Maximum positive
tbb32:neg = -2147483647tbb32;      // Maximum negative

// Type inference from context
tbb32:inferred = 123456789;        // Compiler infers tbb32 from declaration

// ERR sentinel
tbb32:error = ERR;                 // Built-in error constant

// Hexadecimal
tbb32:pattern = 0x7FFFFFFF;        // 2147483647 (INT32_MAX)
tbb32:err_pattern = 0x80000000;    // ERR (-2147483648 / INT32_MIN)

// Underscores for readability:
tbb32:million = 1_000_000;         // 1 million
tbb32:billion = 1_000_000_000;     // 1 billion
```

## Range and Representation

| Property | Value | Hexadecimal | Decimal |
|----------|-------|-------------|---------|
| **Minimum valid** | `-2147483647` | `0x80000001` | -2,147,483,647 |
| **Maximum valid** | `+2147483647` | `0x7FFFFFFF` | +2,147,483,647 |
| **ERR sentinel** | `-2147483648` | `0x80000000` | -2,147,483,648 (INT32_MIN) |
| **Size** | 32 bits | 4 bytes | 4 bytes |

**Memory layout:** Same as standard `i32` - four bytes, two's complement representation, platform-specific endianness. The difference is **semantic**: `-2147483648` is reserved for error signaling.

## Operations

### Arithmetic Operations

All arithmetic operations on `tbb32` include overflow detection and return `ERR` on overflow:

```aria
// Addition
tbb32:a = 2000000000;
tbb32:b = 500000000;
tbb32:sum = a + b;              // ERR (2500000000 > 2147483647)

tbb32:c = 1000000000;
tbb32:d = 1147483647;
tbb32:sum2 = c + d;             // 2147483647 ✓

// Subtraction
tbb32:e = -2000000000;
tbb32:f = 500000000;
tbb32:diff = e - f;             // ERR (-2500000000 < -2147483647)

tbb32:g = 100000000;
tbb32:h = 2247483647;
tbb32:diff2 = g - h;            // -2147483647 ✓

// Multiplication
tbb32:x = 100000;
tbb32:y = 50000;
tbb32:product = x * y;          // ERR (5000000000 > 2147483647)

tbb32:m = 46340;
tbb32:n = 46340;
tbb32:product2 = m * n;         // 2147395600 ✓

// Division
tbb32:p = 2000000000;
tbb32:q = 50;
tbb32:quotient = p / q;         // 40000000 ✓

tbb32:r = 1000000;
tbb32:s = 0;
tbb32:div_zero = r / s;         // ERR (division by zero)

// Modulo
tbb32:mod = 2147483647 % 1000;  // 647 ✓
tbb32:mod_err = 1000000 % 0;    // ERR (modulo by zero)

// Negation
tbb32:val = 1500000000;
tbb32:negated = -val;           // -1500000000 ✓

tbb32:max = 2147483647;
tbb32:neg_max = -max;           // -2147483647 ✓ (symmetric range!)

// Standard i32 would fail here:
// i32:broken = -2147483648;
// -broken → 2147483648, but i32 max is 2147483647 → overflow!
```

### Sticky Error Propagation

```aria
tbb32:valid = 1000000;
tbb32:err = ERR;

// ERR is sticky in all operations:
tbb32:r1 = err + 500000;       // ERR
tbb32:r2 = 500000 + err;       // ERR
tbb32:r3 = err - 1000000;      // ERR
tbb32:r4 = err * 2;            // ERR
tbb32:r5 = err / 3;            // ERR
tbb32:r6 = -err;               // ERR

// Chained operations:
tbb32:a = 2000000000;
tbb32:b = 1000000000;
tbb32:c = a + b;               // ERR (overflow)
tbb32:d = c * 2;               // ERR (propagates!)
tbb32:e = d + 5000000;         // ERR (still propagates!)

// Only check once at the end:
if (e == ERR) {
    io.error("Computation failed somewhere in the chain");
}
```

### Comparison Operations

```aria
tbb32:a = 1500000000;
tbb32:b = 2000000000;
tbb32:err = ERR;

// Standard comparisons
bool:eq = (a == b);            // false
bool:ne = (a != b);            // true
bool:lt = (a < b);             // true
bool:le = (a <= b);            // true
bool:gt = (a > b);             // false
bool:ge = (a >= b);            // false

// ERR comparisons
bool:is_err = (err == ERR);    // true ✓ Primary error check
bool:not_err = (a != ERR);     // true ✓ Validity check

// ERR comparison behavior (IMPORTANT!)
// ERR compares less than all valid values:
bool:err_vs_min = (err < -2147483647);  // true
bool:err_vs_max = (err < 2147483647);   // true
bool:err_vs_zero = (err < 0);           // true
```

### Bitwise Operations

Bitwise operations on `tbb32` **do not propagate ERR** - they operate on raw bit patterns:

```aria
tbb32:a = 0x55555555;  // 1431655765
tbb32:b = 0x33333333;  // 858993459

// Bitwise AND
tbb32:and_result = a & b;      // 0x11111111 = 286331153

// Bitwise OR
tbb32:or_result = a | b;       // 0x77777777 = 2004318071

// Bitwise XOR
tbb32:xor_result = a ^ b;      // 0x66666666 = 1717986918

// Bitwise NOT
tbb32:not_result = ~a;         // 0xAAAAAAAA = -1431655766

// Left shift
tbb32:left = 0x00FF00FF << 8;  // 0xFF00FF00

// Right shift (arithmetic - preserves sign)
tbb32:right = 0xFF00FF00 >> 8; // 0xFFFF00FF

// WARNING: Bitwise ops on ERR produce ordinary bit results!
tbb32:err = ERR;               // 0x80000000
tbb32:err_not = ~err;          // 0x7FFFFFFF = 2147483647 (NOT AN ERROR!)
```

**Critical safety note:** Bitwise operations treat `ERR` as just another bit pattern. Check inputs explicitly if you need error propagation.

### Absolute Value and Sign Operations

```aria
// Absolute value - always safe with symmetric range!
tbb32:pos = abs(2147483647);   // 2147483647 ✓
tbb32:neg = abs(-2147483647);  // 2147483647 ✓
tbb32:zero = abs(0);           // 0 ✓
tbb32:err_abs = abs(ERR);      // ERR (propagates)

// Compare to standard i32:
// i32:broken = abs(-2147483648); // Overflow! Would wrap to -2147483648

// Sign extraction
tbb32:a = 1500000000;
tbb32:sign_pos = sign(a);      // 1
tbb32:sign_neg = sign(-500000000); // -1
tbb32:sign_zero = sign(0);     // 0
tbb32:sign_err = sign(ERR);    // ERR (propagates)

// Min/Max
tbb32:x = 1000000000;
tbb32:y = 2000000000;
tbb32:minimum = min(x, y);     // 1000000000
tbb32:maximum = max(x, y);     // 2000000000

// ERR handling in min/max:
tbb32:err = ERR;
tbb32:min_err = min(err, 500000000);  // ERR (propagates)
tbb32:max_err = max(300000000, err);  // ERR (propagates)
```

## Type Conversions

### To tbb32 (Narrowing or Cross-Type)

```aria
// From i64 (narrowing)
i64:large = 10000000000;
tbb32:narrow = large as tbb32; // ERR (10000000000 > 2147483647)

i64:fits = 2000000000;
tbb32:ok = fits as tbb32;      // 2000000000 ✓

// From tbb8/tbb16 (widening - MUST use tbb_widen!)
tbb8:small = 100;
tbb32:widened = tbb_widen<tbb32>(small);  // 100 ✓

tbb8:small_err = ERR;          // tbb8 ERR (-128)
tbb32:err_widened = tbb_widen<tbb32>(small_err);  // tbb32 ERR (-2147483648) ✓

tbb16:medium = 20000;
tbb32:med_widened = tbb_widen<tbb32>(medium);  // 20000 ✓

// COMPILE ERROR: Implicit widening forbidden!
// tbb32:broken = small_err;   // ERROR! Must use tbb_widen!

// From f32/f64 (cross-type)
f64:pi = 3.14159265359;
tbb32:rounded = pi as tbb32;   // 3 ✓ (truncates toward zero)

f64:big = 3000000000.0;
tbb32:overflow = big as tbb32; // ERR (3000000000 > 2147483647)

f64:negative = -3000000000.0;
tbb32:underflow = negative as tbb32; // ERR (-3000000000 < -2147483647)

// From string (parsing)
string:num_str = "1234567890";
tbb32:parsed = parse_tbb32(num_str);  // 1234567890 ✓

string:invalid = "99999999999";
tbb32:parse_err = parse_tbb32(invalid); // ERR (out of range)
```

### From tbb32 (Widening)

**CRITICAL: ARIA-018 SENTINEL DISCONTINUITY CONSTRAINT**

**Must use `tbb_widen<T>()` to widen to tbb64!**

```aria
tbb32:value = 1234567890;
tbb64:wide = tbb_widen<tbb64>(value);  // 1234567890 ✓

tbb32:err = ERR;               // -2147483648 (tbb32 ERR)
tbb64:err_wide = tbb_widen<tbb64>(err);  // MIN_INT64 (tbb64 ERR) ✓

// COMPILE ERROR: Implicit widening forbidden!
// tbb64:broken = err;         // ERROR! ERR would "heal" to -2147483648 (valid in tbb64)
```

### Narrowing to tbb8/tbb16

```aria
// Narrowing from tbb32 to tbb16 may overflow:
tbb32:value = 50000;
tbb16:narrow = value as tbb16; // ERR (50000 > 32767)

tbb32:fits = 20000;
tbb16:ok = fits as tbb16;      // 20000 ✓

// Narrowing to tbb8:
tbb32:large = 500;
tbb8:tiny = large as tbb8;     // ERR (500 > 127)

tbb32:valid = 100;
tbb8:ok_tiny = valid as tbb8;  // 100 ✓

// ERR propagates correctly:
tbb32:err = ERR;
tbb16:err_narrow = err as tbb16; // ERR (tbb16 ERR = -32768)
tbb8:err_tiny = err as tbb8;     // ERR (tbb8 ERR = -128)
```

## Use Cases

### 1. Financial Calculations (Microcents, Satoshis)

Financial systems tracking values in smallest units with large ranges:

```aria
// Bitcoin amounts in satoshis (1 BTC = 100,000,000 satoshis)
// tbb32 range: -21.47 BTC to +21.47 BTC (sufficient for most transactions)
tbb32:calculate_fee = (tbb32:amount_satoshis, tbb32:fee_rate_sat_per_byte, tbb32:tx_bytes) -> tbb32 {
    tbb32:fee = (fee_rate_sat_per_byte * tx_bytes);  // Overflow detection automatic
    return fee;
}

tbb32:amount = 100_000_000;  // 1 BTC
tbb32:fee_rate = 50;         // 50 sat/byte
tbb32:tx_size = 250;         // 250 bytes
tbb32:fee = calculate_fee(amount, fee_rate, tx_size);  // 12,500 satoshis

if (fee == ERR) {
    io.error("Fee calculation overflow");
}

// Checking sufficient balance for transaction:
tbb32:balance = 150_000_000;  // 1.5 BTC
tbb32:total_cost = amount + fee;

if (total_cost == ERR) {
    io.error("Transaction amount calculation overflow");
} else if (total_cost > balance) {
    io.error("Insufficient balance");
} else {
    execute_transaction(amount, fee);
}
```

### 2. Physics Simulations (Integer Millimeter Precision)

Physics engines using integer arithmetic for determinism:

```aria
// Positions in micrometers (range: ±2147 meters with 1 μm precision)
tbb32:position_x = 1500000000;  // 1.5 km in micrometers
tbb32:velocity_x = 50000;       // 50 mm/s in μm/s
tbb32:acceleration_x = 1000;    // 1 mm/s² in μm/s²
tbb32:time_step = 16;           // 16 ms timestep

// Physics update with overflow detection:
tbb32:update_position = (tbb32:pos, tbb32:vel, tbb32:accel, tbb32:dt_ms) -> tbb32 {
    // s = s₀ + v₀t + ½at²
    tbb32:velocity_term = vel * dt_ms;  // May overflow
    if (velocity_term == ERR) {
        return ERR;
    }
    
    tbb32:accel_half = accel / 2;
    tbb32:accel_term = accel_half * dt_ms * dt_ms;  // May overflow
    if (accel_term == ERR) {
        return ERR;
    }
    
    tbb32:new_pos = pos + velocity_term + accel_term;  // May overflow
    return new_pos;
}

tbb32:new_x = update_position(position_x, velocity_x, acceleration_x, time_step);
if (new_x == ERR) {
    io.error("Physics simulation overflow - object left simulation bounds");
    clamp_or_remove_object();
}
```

### 3. General-Purpose Counting and Indexing

Counters, indices, and identifiers that need overflow detection:

```aria
// Request counter with overflow detection
tbb32:request_count = 0;

func:handle_request = () -> void {
    request_count = request_count + 1;
    
    if (request_count == ERR) {
        io.error("Request counter overflow! System has processed >2.1 billion requests.");
        request_count = 0;  // Reset or handle gracefully
    }
}

// File size tracking
tbb32:file_size_bytes = 0;
tbb32:chunk_size = 1024;

func:append_chunk = () -> void {
    file_size_bytes = file_size_bytes + chunk_size;
    
    if (file_size_bytes == ERR) {
        io.error("File size exceeds tbb32 limit (~2.1 GB)");
        // Switch to tbb64 or split file
    }
}
```

### 4. Component of frac32 (Exact Rationals)

`tbb32` is the building block for `frac32`:

```aria
// frac32 structure:
// struct:frac32 = {
//     tbb32:whole;   // Integer part
//     tbb32:num;     // Numerator
//     tbb32:denom;   // Denominator
// }

frac32:ratio = {0, 1, 3};           // 1/3 exactly
frac32:mixed = {1000000, 1, 2};     // 1000000 1/2 exactly
frac32:precise = {0, 355, 113};     // 355/113 (pi approximation, exact)

// ERR propagation through frac32:
frac32:err_whole = {ERR, 0, 1};     // Whole part is ERR → frac is ERR
frac32:div_zero = {100, 1, 0};      // Denominator zero → ERR

// Financial exact rationals:
frac32:interest_rate = {0, 5, 100}; // 5/100 = 5% exactly (not 0.05 with rounding)
```

### 5. Time and Date Calculations

Unix timestamps (seconds since 1970-01-01):

```aria
// Unix timestamp in seconds
// tbb32 range: 1901-12-13 to 2038-01-19 (Y2038 problem still exists!)
tbb32:current_time = get_unix_timestamp();  // e.g., 1700000000 (2023-11-14)

// Time arithmetic with overflow detection:
tbb32:one_day_seconds = 86400;  // 24 * 60 * 60
tbb32:tomorrow = current_time + one_day_seconds;

if (tomorrow == ERR) {
    io.error("Time calculation overflow - approaching Y2038 limit!");
    // Migrate to tbb64 or use millisecond precision
}

// Duration calculations:
tbb32:start_time = 1700000000;
tbb32:end_time = 1700086400;
tbb32:duration = end_time - start_time;  // 86400 seconds (1 day)

if (duration == ERR) {
    io.error("Duration calculation underflow");
}
```

**Note:** For systems expected to run past January 19, 2038, use `tbb64` for timestamps!

### 6. Image Processing (Pixel Coordinates)

Image dimensions and coordinates:

```aria
// Image up to 46340 x 46340 pixels (sqrt(INT32_MAX))
tbb32:width = 4096;
tbb32:height = 2160;
tbb32:total_pixels = width * height;  // 8,847,360 ✓

if (total_pixels == ERR) {
    io.error("Image too large for tbb32 pixel count");
}

// Pixel offset calculation:
tbb32:calculate_offset = (tbb32:x, tbb32:y, tbb32:stride) -> tbb32 {
    return y * stride + x;  // Overflow detection automatic
}

tbb32:offset = calculate_offset(1920, 1080, 4096);
if (offset == ERR) {
    io.error("Pixel offset calculation overflow");
}
```

## Common Patterns

### Pattern: Safe Integer Arithmetic Pipeline

```aria
tbb32:pipeline_calculation = (tbb32:a, tbb32:b, tbb32:c, tbb32:d) -> tbb32 {
    // Complex calculation: ((a + b) * c - d) / 2
    tbb32:sum = a + b;
    if (sum == ERR) { return ERR; }
    
    tbb32:product = sum * c;
    if (product == ERR) { return ERR; }
    
    tbb32:diff = product - d;
    if (diff == ERR) { return ERR; }
    
    tbb32:result = diff / 2;
    return result;  // Final result or ERR
}

// Alternative: Let errors propagate, check once:
tbb32:pipeline_auto = (tbb32:a, tbb32:b, tbb32:c, tbb32:d) -> tbb32 {
    return ((a + b) * c - d) / 2;  // ERR propagates automatically
}

tbb32:result = pipeline_auto(1000000, 2000000, 500, 100000);
if (result == ERR) {
    io.error("Calculation pipeline failed");
}
```

### Pattern: Validated Input Conversion

```aria
Result<tbb32>:parse_and_validate = (string:input) -> Result<tbb32> {
    // Try parsing as i64 first (wider range)
    i64:parsed = parse_int(input);
    
    if (parsed < -2147483647 || parsed > 2147483647) {
        return fail("Value out of tbb32 range: " + input);
    }
    
    return pass(parsed as tbb32);
}

Result<tbb32>:r = parse_and_validate("1234567890");
if (r.is_error) {
    io.error("Parse error: " + r.err);
} else {
    tbb32:value = r.val;  // Guaranteed valid tbb32
    process(value);
}
```

### Pattern: Accumulator with Overflow Detection

```aria
tbb32:safe_sum = (tbb32:array[], u32:length) -> tbb32 {
    tbb32:sum = 0;
    
    for (u32:i = 0; i < length; i++) {
        sum = sum + array[i];
        
        if (sum == ERR) {
            return ERR;  // Accumulation overflow detected
        }
    }
    
    return sum;
}

tbb32:values[1000];
// ... initialize values ...

tbb32:total = safe_sum(values, 1000);
if (total == ERR) {
    io.error("Sum overflow - values too large");
} else {
    tbb32:average = total / 1000;
    io.print("Average: {tbb32}", average);
}
```

### Pattern: Saturating Arithmetic

```aria
tbb32:saturating_add = (tbb32:a, tbb32:b) -> tbb32 {
    if (a == ERR || b == ERR) {
        return ERR;  // Propagate errors
    }
    
    i64:temp = (a as i64) + (b as i64);
    
    if (temp > 2147483647) {
        return 2147483647;   // Saturate at max
    }
    if (temp < -2147483647) {
        return -2147483647;  // Saturate at min
    }
    
    return temp as tbb32;
}

tbb32:x = 2000000000;
tbb32:y = 500000000;
tbb32:result = saturating_add(x, y);  // 2147483647 (saturated, not ERR)
```

## Common Mistakes and Anti-Patterns

### ❌ Implicit Widening from tbb8/tbb16

```aria
tbb8:small = ERR;
tbb32:big = small;  // COMPILE ERROR! Implicit widening forbidden!

// ✓ Correct:
tbb32:big = tbb_widen<tbb32>(small);
```

### ❌ Using -2147483648 as Regular Value

```aria
// ❌ WRONG:
tbb32:value = -2147483648;  // This is ERR! Not a regular value!

// ✓ CORRECT: Use tbb64 if you need -2147483648
tbb64:value = -2147483648;  // Valid in tbb64 (ERR is MIN_INT64)
```

### ❌ Ignoring Y2038 Problem

```aria
// ❌ WRONG: Using tbb32 for timestamps in long-running systems
tbb32:timestamp = get_unix_timestamp();  // Will fail after 2038-01-19!

// ✓ CORRECT: Use tbb64 for timestamps
tbb64:timestamp = get_unix_timestamp_64bit();  // Good until year 292 billion
```

### ❌ Overflow in Intermediate Calculations

```aria
// ❌ WRONG: Potential overflow in multiplication
tbb32:a = 100000;
tbb32:b = 50000;
tbb32:c = 1000;
tbb32:result = (a * b) / c;  // a * b overflows! result = ERR / c = ERR

// ✓ CORRECT: Reorder to avoid overflow
tbb32:result = (a / c) * b;  // (100000 / 1000) * 50000 = 100 * 50000 = 5000000 ✓
// Or check intermediate:
tbb32:temp = a * b;
if (temp == ERR) {
    io.error("Intermediate overflow");
}
```

### ❌ Bitwise Ops Without ERR Checks

```aria
tbb32:value = ERR;             // 0x80000000
tbb32:masked = value & 0x7FFFFFFF; // 0x00000000 (NOT ERR!)

// ✓ CORRECT:
if (value == ERR) {
    return ERR;
}
tbb32:masked = value & 0x7FFFFFFF;
```

## Integration with Error Handling

### With Result<T>

```aria
Result<tbb32>:read_config_value = (string:key) -> Result<tbb32> {
    string:value_str = config.get(key)?;  // Propagate config errors
    
    i64:parsed = parse_int(value_str);
    if (parsed < -2147483647 || parsed > 2147483647) {
        return fail("Value out of tbb32 range: " + value_str);
    }
    
    return pass(parsed as tbb32);
}

Result<tbb32>:r = read_config_value("max_connections");
if (r.is_error) {
    io.error("Config error: " + r.err);
} else {
    tbb32:max_conns = r.val;
    // ... use max_conns ...
}
```

### With unknown

```aria
// Division by zero returns unknown, not ERR
tbb32:a = 1000000;
tbb32:b = 0;

// Custom function returns ERR for div-by-zero:
tbb32:safe_div = (tbb32:numerator, tbb32:denominator) -> tbb32 {
    if (numerator == ERR || denominator == ERR) {
        return ERR;
    }
    if (denominator == 0) {
        return ERR;  // Explicit ERR instead of unknown
    }
    return numerator / denominator;
}

tbb32:result = safe_div(1000000, 0);  // ERR (explicitly handled)
```

## Performance Considerations

### Memory Overhead

```aria
tbb32:single;       // 4 bytes (same as i32)
tbb32:array[1000];  // 4000 bytes (same as i32[1000])
```

**No memory overhead** compared to standard `i32`.

### Computational Overhead

**Arithmetic operations:** Typically **1-3 cycles** overhead for overflow detection:

```assembly
; Standard i32 addition (1 cycle):
add eax, ebx

; tbb32 addition with overflow check (2-3 cycles):
add eax, ebx
jo overflow_handler   ; Jump on overflow flag
jmp done
overflow_handler:
    mov eax, 0x80000000   ; Set to ERR sentinel
done:
```

**Comparison:** Same performance as `i32` - single comparison instruction.

**Typical overhead:** 5-10% compared to unchecked arithmetic. **Safety gain:** Eliminates overflow bugs.

### When To Use tbb32 vs i32

**Use `tbb32` when:**
- Overflow detection critical (financial, safety systems)
- Error propagation simplifies logic (complex calculations)
- Building block for frac32 (exact rationals)
- General-purpose arithmetic with safety guarantees

**Use `i32` when:**
- Raw performance critical, overflow acceptable
- Full range `[-2147483648, +2147483647]` needed (timestamps pre-Y2038)
- Interfacing with hardware/protocols expecting standard `i32`

## Comparison with Other Types

### vs i32 (Standard Signed 32-bit Integer)

| Feature | tbb32 | i32 |
|---------|-------|-----|
| **Range** | `[-2147483647, +2147483647]` | `[-2147483648, +2147483647]` |
| **Overflow behavior** | Returns `ERR` | Wraps silently |
| **Asymmetry bugs** | Eliminated | `abs(-2147483648)` breaks |
| **Error propagation** | Automatic (sticky) | Manual checking required |
| **Use case** | Safety-critical, exact arithmetic | General purpose, performance |

### vs tbb8/tbb16/tbb64

| Type | Range | ERR Sentinel | Size | Use Case |
|------|-------|--------------|------|----------|
| **tbb8** | `[-127, +127]` | `-128` | 1 byte | Small values, frac8 |
| **tbb16** | `[-32767, +32767]` | `-32768` | 2 bytes | Audio, medium precision |
| **tbb32** | `[-2147483647, +2147483647]` | `-2147483648` | 4 bytes | **General purpose**, frac32 |
| **tbb64** | Large symmetric | MIN_INT64 | 8 bytes | High precision, timestamps, frac64 |

**tbb32 is the default choice** for most TBB applications.

### vs frac32 (Exact Rational Built on tbb32)

`frac32` is **built on top of** `tbb32`:

```aria
// frac32 structure:
struct:frac32 = {
    tbb32:whole;   // Integer part
    tbb32:num;     // Numerator
    tbb32:denom;   // Denominator
}

// tbb32 provides integer values
tbb32:integer = 1234567890;

// frac32 provides exact rational values
frac32:rational = {1000, 1, 3};  // 1000 1/3 exactly
```

## Platform and Portability

### Bit Representation

`tbb32` uses standard two's complement representation, identical to `i32`:

| Value | Decimal | Hexadecimal |
|-------|---------|-------------|
| `2147483647` | 2,147,483,647 | `0x7FFFFFFF` |
| `1` | 1 | `0x00000001` |
| `0` | 0 | `0x00000000` |
| `-1` | -1 | `0xFFFFFFFF` |
| `-2147483647` | -2,147,483,647 | `0x80000001` |
| **`ERR`** | -2,147,483,648 | **`0x80000000`** |

### Endianness

**Four-byte value:** Endianness matters for serialization/network transmission.

**Little-endian** (x86, x86-64, ARM64 typical):
- ERR (0x80000000) stored as: `[0x00, 0x00, 0x00, 0x80]` in memory

**Big-endian** (network byte order):
- ERR (0x80000000) stored as: `[0x80, 0x00, 0x00, 0x00]` in memory

**Best practice:** Use network byte order conversion:

```aria
tbb32:value = 1234567890;
u32:network_order = htonl(value as u32);  // Host to network long
```

### Alignment

**Natural alignment:** 4 bytes (typically aligned to 4-byte boundaries):

```aria
struct:Data = {
    tbb32:field1;   // Offset 0, 4 bytes
    tbb32:field2;   // Offset 4, 4 bytes
    tbb32:field3;   // Offset 8, 4 bytes
    // Total size: 12 bytes, naturally aligned
}
```

## Best Practices

### ✅ DO: Use tbb32 as Default TBB Type

```aria
// For general-purpose arithmetic with overflow detection:
tbb32:count = 0;
tbb32:sum = 0;
tbb32:product = 1;
```

### ✅ DO: Use tbb_widen for Widening

```aria
tbb16:medium = 20000;
tbb32:large = tbb_widen<tbb32>(medium);  // ✓ Correct

tbb16:err = ERR;
tbb32:err_large = tbb_widen<tbb32>(err);  // ✓ Preserves ERR
```

### ✅ DO: Check ERR in Critical Paths

```aria
tbb32:balance = get_balance();
if (balance == ERR) {
    io.error("Balance retrieval failed");
    return;
}
tbb32:new_balance = balance - withdrawal;
if (new_balance == ERR) {
    io.error("Insufficient funds or underflow");
    return;
}
commit_withdrawal(withdrawal);
```

### ✅ DO: Use for frac32 Components

```aria
// Exact rational arithmetic:
frac32:pi_approx = {0, 355, 113};  // 355/113 ≈ π (exact)
frac32:interest = {0, 5, 100};     // 5% exactly
```

### ❌ DON'T: Use -2147483648 as Regular Value

```aria
// ❌ WRONG:
tbb32:value = -2147483648;  // This is ERR!

// ✓ CORRECT: Use tbb64
tbb64:value = -2147483648;  // Valid in tbb64
```

### ❌ DON'T: Use for Timestamps Past 2038

```aria
// ❌ WRONG: Unix timestamp in tbb32
tbb32:timestamp = get_unix_time();  // Fails 2038-01-19!

// ✓ CORRECT: Use tbb64 for timestamps
tbb64:timestamp = get_unix_time_64();  // Good for millennia
```

### ❌ DON'T: Assume Full i32 Range

```aria
// ❌ WRONG:
for (i32:i = -2147483648; i <= 2147483647; i++) {  // Includes ERR value!
    // ...
}

// ✓ CORRECT: Use tbb32 range
for (tbb32:i = -2147483647; i <= 2147483647; i++) {  // Valid range
    // ...
}
```

## Relationship to Language Safety System

`tbb32` is a fundamental component of Aria's layered safety architecture:

**Layer 1: Result<T>** - Hard errors requiring explicit handling
**Layer 2: unknown** - Soft errors for undefined states
**Layer 3: failsafe()** - Last-resort safety net

**TBB types implement sticky error propagation for Layers 1 and 2.**

**Priority:** Safety > Stability > Accuracy > Performance > Convenience

`tbb32` sacrifices 1 value from the range (convenience) to gain automatic error propagation (safety).

## Summary

`tbb32` is a 32-bit **Twisted Balanced Binary** integer with:

- **Symmetric range:** `[-2147483647, +2147483647]` (eliminates asymmetry bugs)
- **ERR sentinel:** `-2147483648` (0x80000000) for automatic error signaling
- **Sticky error propagation:** `ERR + anything = ERR`
- **ARIA-018 protection:** Explicit widening with `tbb_widen<T>()`
- **General-purpose:** Default choice for most TBB applications
- **Foundation role:** Building block for `frac32` exact rationals

**Use `tbb32` when:**
- General-purpose arithmetic with overflow detection
- Financial calculations (satoshis, microcents)
- Physics simulations (integer precision)
- Counters, indices with safety guarantees
- Building frac32 exact rationals

**Avoid `tbb32` when:**
- Full range `[-2147483648, +2147483647]` required (use `i32`)
- Timestamps exceeding 2038 (use `tbb64`)
- Raw performance critical and overflow acceptable

**Next:** See [tbb8](tbb8.md), [tbb16](tbb16.md), [tbb64](tbb64.md) for other TBB types, and [frac32](frac32.md) for exact rational arithmetic built on `tbb32`.
