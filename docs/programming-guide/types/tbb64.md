# tbb64: 64-bit Twisted Balanced Binary Integer

## Overview

`tbb64` is a 64-bit **Twisted Balanced Binary** integer type with a perfectly symmetric range and a reserved error sentinel value at the minimum 64-bit signed value (INT64_MIN). `tbb64` provides the **widest range** in the TBB family, making it ideal for high-precision calculations, large-scale systems, and applications requiring maximum numeric capacity with built-in error propagation.

**Key characteristics:**
- **Range:** `[-9223372036854775807, +9223372036854775807]` (±9.22 quintillion, symmetric)
- **ERR sentinel:** `-9223372036854775808` (bit pattern `0x8000000000000000`, INT64_MIN)
- **Sticky error propagation:** Any operation involving `ERR` produces `ERR`
- **Symmetric range:** Eliminates asymmetry bugs (`abs(-9223372036854775807)` always works)
- **Foundation type:** Used for `frac64` (high-precision exact rationals), `tfp64` mantissa, timestamps, large-scale financial systems
- **Maximum capacity:** Widest TBB type - no larger TBB variants exist

```aria
tbb64:timestamp = 1700000000000;       // Unix timestamp in milliseconds
tbb64:max_positive = 9223372036854775807;   // Maximum positive value (INT64_MAX) 
tbb64:max_negative = -9223372036854775807;  // Maximum negative value (symmetric!)
tbb64:error_state = ERR;                    // Error sentinel (INT64_MIN)
```

## Purpose and Philosophy

`tbb64` extends the TBB philosophy to maximum capacity, providing automatic error propagation for applications that require the widest possible integer range.

### Why tbb64?

**Use `tbb64` when:**
- **Timestamps:** Unix time in milliseconds/microseconds (solves Y2038 problem)
- **High-precision frac64:** Exact rational arithmetic with large numerators/denominators
- **Large-scale financial:** Cryptocurrency amounts in smallest units (satoshis, wei)
- **Scientific computing:** High-precision integer calculations
- **Component of tfp64:** 48-bit mantissa for deterministic floating point
- **Maximum range needed:** When tbb32 range is insufficient

**Range comparison:**
- `tbb8`: `[-127, +127]` (256 values)
- `tbb16`: `[-32767, +32767]` (65K values)
- `tbb32`: `[-2147483647, +2147483647]` (~4.3 billion values)
- `tbb64`: `[-9223372036854775807, +9223372036854775807]` (~18.4 quintillion values)

### Asymmetry Elimination (64-bit)

```aria
// Standard i64 problems:
i64:val = -9223372036854775808;  // INT64_MIN
i64:negated = -val;              // Overflow! Wraps to -9223372036854775808
i64:absolute = abs(val);         // Same problem! Wraps to -9223372036854775808

// tbb64 solution:
tbb64:val = -9223372036854775807;  // Symmetric range, INT64_MIN reserved for ERR
tbb64:negated = -val;              // 9223372036854775807 ✓ Always works!
tbb64:absolute = abs(val);         // 9223372036854775807 ✓ No overflow possible!
```

### Sticky Error Propagation

```aria
tbb64:a = 9000000000000000000;
tbb64:b = 500000000000000000;
tbb64:sum = a + b;               // Exceeds max → sum = ERR
tbb64:product = sum * 2;         // ERR * 2 → ERR (propagates!)
tbb64:final = product + 1000;    // ERR + 1000 → ERR (still propagates!)

// Check error once at the end:
if (final == ERR) {
    io.error("Calculation overflow detected");
}
```

## Syntax and Literals

```aria
// Decimal literals with type suffix
tbb64:value = 1500000000000tbb64;       // Explicit type suffix
tbb64:pos = 9223372036854775807tbb64;   // Maximum positive
tbb64:neg = -9223372036854775807tbb64;  // Maximum negative

// Type inference from context
tbb64:inferred = 123456789012345;       // Compiler infers tbb64 from declaration

// ERR sentinel
tbb64:error = ERR;                      // Built-in error constant

// Hexadecimal
tbb64:pattern = 0x7FFFFFFFFFFFFFFF;     // INT64_MAX
tbb64:err_pattern = 0x8000000000000000; // ERR (INT64_MIN)

// Underscores for readability (recommended for large values):
tbb64:trillion = 1_000_000_000_000;     // 1 trillion
tbb64:quintillion = 1_000_000_000_000_000_000; // 1 quintillion
```

## Range and Representation

| Property | Value (Decimal) | Hexadecimal |
|----------|-----------------|-------------|
| **Minimum valid** | `-9223372036854775807` | `0x8000000000000001` |
| **Maximum valid** | `+9223372036854775807` | `0x7FFFFFFFFFFFFFFF` |
| **ERR sentinel** | `-9223372036854775808` | `0x8000000000000000` |
| **Size** | 64 bits | 8 bytes |

**Memory layout:** Same as standard `i64` - eight bytes, two's complement representation, platform-specific endianness. The difference is **semantic**: `INT64_MIN` is reserved for error signaling.

## Operations

### Arithmetic Operations

All arithmetic operations on `tbb64` include overflow detection and return `ERR` on overflow:

```aria
// Addition
tbb64:a = 9000000000000000000;
tbb64:b = 500000000000000000;
tbb64:sum = a + b;              // ERR (exceeds maximum)

tbb64:c = 5000000000000000000;
tbb64:d = 4223372036854775807;
tbb64:sum2 = c + d;             // 9223372036854775807 ✓

// Subtraction
tbb64:e = -9000000000000000000;
tbb64:f = 500000000000000000;
tbb64:diff = e - f;             // ERR (exceeds negative limit)

tbb64:g = 1000000000000;
tbb64:h = 9223373036854775807;
tbb64:diff2 = g - h;            // -9223372036854775807 ✓

// Multiplication
tbb64:x = 10000000000;  // 10 billion
tbb64:y = 1000000000;   // 1 billion
tbb64:product = x * y;  // ERR (10^19 exceeds max)

tbb64:m = 3037000499;   // sqrt(INT64_MAX) ≈ 3037000499
tbb64:n = 3037000499;
tbb64:product2 = m * n; // 9223372030926249001 ✓

// Division
tbb64:p = 9000000000000000000;
tbb64:q = 1000;
tbb64:quotient = p / q;         // 9000000000000000 ✓

tbb64:r = 1000000000;
tbb64:s = 0;
tbb64:div_zero = r / s;         // ERR (division by zero)

// Modulo
tbb64:mod = 9223372036854775807 % 1000000; // 775807 ✓
tbb64:mod_err = 1000000000 % 0;            // ERR (modulo by zero)

// Negation
tbb64:val = 5000000000000000000;
tbb64:negated = -val;           // -5000000000000000000 ✓

tbb64:max = 9223372036854775807;
tbb64:neg_max = -max;           // -9223372036854775807 ✓ (symmetric!)
```

### Sticky Error Propagation

```aria
tbb64:valid = 1000000000000;
tbb64:err = ERR;

// ERR is sticky in all operations:
tbb64:r1 = err + 500000000;    // ERR
tbb64:r2 = 500000000 + err;    // ERR
tbb64:r3 = err - 1000000000;   // ERR
tbb64:r4 = err * 2;            // ERR
tbb64:r5 = err / 3;            // ERR
tbb64:r6 = -err;               // ERR

// Chained operations:
tbb64:a = 9000000000000000000;
tbb64:b = 1000000000000000000;
tbb64:c = a + b;               // ERR (overflow)
tbb64:d = c * 2;               // ERR (propagates!)
tbb64:e = d + 5000000;         // ERR (still propagates!)

// Only check once at the end:
if (e == ERR) {
    io.error("Computation failed somewhere in the chain");
}
```

### Comparison Operations

```aria
tbb64:a = 5000000000000000000;
tbb64:b = 7000000000000000000;
tbb64:err = ERR;

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
bool:err_vs_min = (err < -9223372036854775807);  // true
bool:err_vs_max = (err < 9223372036854775807);   // true
bool:err_vs_zero = (err < 0);                    // true
```

### Bitwise Operations

Bitwise operations on `tbb64` **do not propagate ERR** - they operate on raw bit patterns:

```aria
tbb64:a = 0x5555555555555555;
tbb64:b = 0x3333333333333333;

// Bitwise AND
tbb64:and_result = a & b;      // 0x1111111111111111

// Bitwise OR
tbb64:or_result = a | b;       // 0x7777777777777777

// Bitwise XOR
tbb64:xor_result = a ^ b;      // 0x6666666666666666

// Bitwise NOT
tbb64:not_result = ~a;         // 0xAAAAAAAAAAAAAAAA

// Left shift
tbb64:left = 0x00FF00FF00FF00FF << 8;

// Right shift (arithmetic - preserves sign)
tbb64:right = 0xFF00FF00FF00FF00 >> 8;

// WARNING: Bitwise ops on ERR produce ordinary bit results!
tbb64:err = ERR;               // 0x8000000000000000
tbb64:err_not = ~err;          // 0x7FFFFFFFFFFFFFFF (NOT AN ERROR!)
```

**Critical safety note:** Check inputs explicitly if you need error propagation with bitwise operations.

### Absolute Value and Sign Operations

```aria
// Absolute value - always safe with symmetric range!
tbb64:pos = abs(9223372036854775807);   // 9223372036854775807 ✓
tbb64:neg = abs(-9223372036854775807);  // 9223372036854775807 ✓
tbb64:zero = abs(0);                    // 0 ✓
tbb64:err_abs = abs(ERR);               // ERR (propagates)

// Compare to standard i64:
// i64:broken = abs(-9223372036854775808); // Overflow!

// Sign extraction
tbb64:a = 5000000000000000000;
tbb64:sign_pos = sign(a);      // 1
tbb64:sign_neg = sign(-5000000000000000000); // -1
tbb64:sign_zero = sign(0);     // 0
tbb64:sign_err = sign(ERR);    // ERR (propagates)

// Min/Max
tbb64:x = 3000000000000000000;
tbb64:y = 7000000000000000000;
tbb64:minimum = min(x, y);     // 3000000000000000000
tbb64:maximum = max(x, y);     // 7000000000000000000

// ERR handling in min/max:
tbb64:err = ERR;
tbb64:min_err = min(err, 5000000000000000000);  // ERR (propagates)
tbb64:max_err = max(3000000000000000000, err);  // ERR (propagates)
```

## Type Conversions

### To tbb64 (Narrowing or Cross-Type)

```aria
// From tbb8/tbb16/tbb32 (widening - MUST use tbb_widen!)
tbb8:tiny = 100;
tbb64:widened = tbb_widen<tbb64>(tiny);  // 100 ✓

tbb8:tiny_err = ERR;           // tbb8 ERR (-128)
tbb64:err_widened = tbb_widen<tbb64>(tiny_err);  // tbb64 ERR (INT64_MIN) ✓

tbb16:small = 20000;
tbb64:small_widened = tbb_widen<tbb64>(small);  // 20000 ✓

tbb32:medium = 2000000000;
tbb64:med_widened = tbb_widen<tbb32>(medium);  // 2000000000 ✓

// COMPILE ERROR: Implicit widening forbidden!
// tbb64:broken = tiny_err;    // ERROR! Must use tbb_widen!

// From i128 (if available, narrowing)
// i128:huge = 10000000000000000000000;  // Exceeds tbb64
// tbb64:narrow = huge as tbb64;  // ERR (out of range)

// From f64 (cross-type)
f64:pi = 3.14159265359;
tbb64:rounded = pi as tbb64;   // 3 ✓ (truncates toward zero)

f64:huge = 1.0e19;             // 10^19
tbb64:overflow = huge as tbb64; // ERR (exceeds max)

// From string (parsing)
string:num_str = "1234567890123456789";
tbb64:parsed = parse_tbb64(num_str);  // 1234567890123456789 ✓

string:too_large = "99999999999999999999";
tbb64:parse_err = parse_tbb64(too_large); // ERR (out of range)
```

### From tbb64 (No Larger TBB Type)

**tbb64 is the largest TBB type** - no widening operations to larger TBB variants.

**Conversions to non-TBB types:**

```aria
// To i64 (loses ERR semantics):
tbb64:value = 1234567890123456789;
i64:int_value = value as i64;  // 1234567890123456789 (valid value)

tbb64:err = ERR;
i64:int_err = err as i64;      // -9223372036854775808 (becomes ordinary i64 value!)

// To f64 (may lose precision):
tbb64:large = 9223372036854775807;
f64:float_value = large as f64; // ~9.223372036854776e18 (rounded, loses precision)

// To u64 (if value is non-negative):
tbb64:pos = 5000000000000000000;
u64:unsigned_val = pos as u64;  // 5000000000000000000 ✓

tbb64:neg = -1000000;
u64:unsigned_neg = neg as u64;  // Underflow or error (implementation-defined)
```

### Narrowing to tbb8/tbb16/tbb32

```aria
// Narrowing from tbb64 to tbb32 may overflow:
tbb64:value = 5000000000;      // 5 billion
tbb32:narrow = value as tbb32; // ERR (exceeds tbb32 max)

tbb64:fits = 2000000000;
tbb32:ok = fits as tbb32;      // 2000000000 ✓

// Narrowing to tbb16:
tbb64:large = 50000;
tbb16:tiny = large as tbb16;   // ERR (exceeds tbb16 max)

// Narrowing to tbb8:
tbb64:huge = 500;
tbb8:micro = huge as tbb8;     // ERR (exceeds tbb8 max)

// ERR propagates correctly:
tbb64:err = ERR;
tbb32:err_narrow = err as tbb32;  // ERR (tbb32 ERR = -2147483648)
tbb16:err_tiny = err as tbb16;    // ERR (tbb16 ERR = -32768)
tbb8:err_micro = err as tbb8;     // ERR (tbb8 ERR = -128)
```

## Use Cases

### 1. Timestamps (Solves Y2038 Problem)

Unix time in milliseconds or microseconds:

```aria
// Unix timestamp in MILLISECONDS (not seconds!)
// tbb64 range: ~292 million years (vs tbb32 seconds: until 2038-01-19)
tbb64:get_timestamp_ms = () -> tbb64 {
    // Current time in milliseconds since 1970-01-01 00:00:00 UTC
    return system.current_time_ms();
}

tbb64:now = get_timestamp_ms();  // e.g., 1700000000000 (2023-11-14)

// Time arithmetic with overflow detection:
tbb64:one_day_ms = 86400000;     // 24 * 60 * 60 * 1000
tbb64:tomorrow = now + one_day_ms;

if (tomorrow == ERR) {
    io.error("Time calculation overflow");  // Won't happen for thousands of years!
}

// Duration calculations:
tbb64:start_time = 1700000000000;
tbb64:end_time = 1700086400000;
tbb64:duration_ms = end_time - start_time;  // 86400000 ms (1 day)

if (duration_ms == ERR) {
    io.error("Duration calculation underflow");
}

// Convert to human-readable:
f64:duration_hours = (duration_ms as f64) / 3600000.0;  // 24.0 hours
```

### 2. Large-Scale Financial Systems (Cryptocurrency)

Bitcoin, Ethereum amounts in smallest units:

```aria
// Bitcoin: 1 BTC = 100,000,000 satoshis
// tbb64 range: ±92.23 billion BTC (far exceeds 21 million BTC total supply)
tbb64:btc_to_satoshis = (f64:btc_amount) -> tbb64 {
    f64:satoshis = btc_amount * 100_000_000.0;
    if (satoshis > 9223372036854775807.0 || satoshis < -9223372036854775807.0) {
        return ERR;
    }
    return satoshis as tbb64;
}

tbb64:balance_sat = btc_to_satoshis(10.5);  // 10.5 BTC = 1,050,000,000 satoshis

// Ethereum: 1 ETH = 10^18 wei
// tbb64 range: ±9.2 ETH (careful! Smaller range for wei)
tbb64:eth_to_wei = (f64:eth_amount) -> tbb64 {
    // WARNING: tbb64 can only hold ±9.2 ETH in wei!
    // For larger amounts, use multi-precision arithmetic or different unit
    f64:wei = eth_amount * 1_000_000_000_000_000_000.0;  // 10^18
    if (wei > 9223372036854775807.0 || wei < -9223372036854775807.0) {
        return ERR;
    }
    return wei as tbb64;
}

tbb64:balance_wei = eth_to_wei(5.0);  // 5 ETH = 5*10^18 wei

if (balance_wei == ERR) {
    io.error("ETH amount too large for tbb64 in wei units - use Gwei or multi-precision");
}
```

### 3. High-Precision frac64 Components

Exact rational arithmetic with large numerators/denominators:

```aria
// frac64 structure:
// struct:frac64 = {
//     tbb64:whole;   // Integer part
//     tbb64:num;     // Numerator
//     tbb64:denom;   // Denominator
// }

frac64:pi_approx = {0, 355, 113};  // 355/113 ≈ π (exact)
frac64:large_ratio = {1000000000000, 1, 3};  // 10^12 + 1/3 exactly

// Scientific calculations requiring high precision:
frac64:avogadro_approx = {602214076000000000000000, 0, 1};  // Avogadro's number (approximate)

// ERR propagation through frac64:
frac64:err_whole = {ERR, 0, 1};    // Whole part is ERR → frac is ERR
frac64:div_zero = {1000, 1, 0};    // Denominator zero → ERR
```

### 4. Component of tfp64 (Deterministic Floating Point)

`tbb64` (specifically tbb48) is used as mantissa in `tfp64`:

```aria
// tfp64 (Twisted Floating Point 64-bit):
// struct:tfp64 = {
//     tbb16:exponent;   // 16-bit exponent
//     tbb48:mantissa;   // 48-bit mantissa (tbb family, not standard tbb64)
// }

// ERR in either component → entire tfp64 is ERR
tfp64:value = {100, 12345678901234};  // Valid
tfp64:err_exp = {ERR, 12345678901234};  // ERR (exponent is ERR)
tfp64:err_man = {100, ERR};             // ERR (mantissa is ERR)
```

### 5. Large Counters and Accumulators

Counters that won't overflow for millennia:

```aria
// Global event counter
tbb64:global_event_count = 0;

func:log_event = () -> void {
    global_event_count = global_event_count + 1;
    
    if (global_event_count == ERR) {
        // Would require 9.2 quintillion events!
        io.error("Event counter overflow - this should never happen");
    }
}

// Byte counter for network traffic:
tbb64:total_bytes_sent = 0;

func:send_packet = (u32:packet_size) -> void {
    total_bytes_sent = total_bytes_sent + (packet_size as tbb64);
    
    if (total_bytes_sent == ERR) {
        // Would require 9.2 exabytes!
        io.error("Byte counter overflow");
    }
}
```

### 6. Scientific Computing (High-Precision Integer Calculations)

Large-scale simulations requiring deterministic integer arithmetic:

```aria
// Molecular dynamics simulation (positions in picometers)
// tbb64 range: ±9.2 million light-years with 1 pm precision
tbb64:position_x_pm = 5000000000000000;  // 5 mm in picometers
tbb64:velocity_x_pm_per_fs = 1000000;    // 1 km/s in pm/fs
tbb64:timestep_fs = 1;                    // 1 femtosecond

tbb64:update_position = (tbb64:pos, tbb64:vel, tbb64:dt) -> tbb64 {
    tbb64:displacement = vel * dt;
    if (displacement == ERR) {
        return ERR;  // Velocity too high
    }
    
    return pos + displacement;
}

tbb64:new_x = update_position(position_x_pm, velocity_x_pm_per_fs, timestep_fs);
if (new_x == ERR) {
    io.error("Position calculation overflow");
}
```

## Common Patterns

### Pattern: Timestamp Arithmetic

```aria
// Current time in milliseconds
tbb64:now_ms = () -> tbb64 {
    return system.current_time_ms();
}

// Add duration (in milliseconds)
tbb64:add_duration = (tbb64:timestamp, tbb64:duration_ms) -> tbb64 {
    return timestamp + duration_ms;  // ERR if overflow
}

tbb64:now = now_ms();
tbb64:one_week_ms = 7 * 24 * 60 * 60 * 1000;  // 604,800,000 ms
tbb64:next_week = add_duration(now, one_week_ms);

if (next_week == ERR) {
    io.error("Timestamp overflow");
} else {
    io.print("Next week: {tbb64} ms since epoch", next_week);
}
```

### Pattern: Large Financial Calculation

```aria
// Calculate total portfolio value in satoshis
tbb64:calculate_portfolio_value = (tbb64:btc_holdings[], u32:count, tbb64:btc_price_sat) -> tbb64 {
    tbb64:total = 0;
    
    for (u32:i = 0; i < count; i++) {
        total = total + btc_holdings[i];
        
        if (total == ERR) {
            return ERR;  // Holdings overflow
        }
    }
    
    tbb64:value = total * btc_price_sat;
    return value;  // ERR if multiplication overflows
}

tbb64:holdings[10];
// ... initialize holdings ...

tbb64:btc_price = 5000000000;  // 50,000 USD/BTC in satoshis (assuming 1 USD = 100,000 sat)
tbb64:portfolio = calculate_portfolio_value(holdings, 10, btc_price);

if (portfolio == ERR) {
    io.error("Portfolio calculation overflow");
}
```

### Pattern: High-Precision Accumulator

```aria
tbb64:accumulate_precise = (tbb64:values[], u32:length) -> tbb64 {
    tbb64:sum = 0;
    
    for (u32:i = 0; i < length; i++) {
        if (values[i] == ERR) {
            return ERR;  // Invalid input value
        }
        
        sum = sum + values[i];
        
        if (sum == ERR) {
            return ERR;  // Accumulation overflow
        }
    }
    
    return sum;
}

tbb64:measurements[1000000];
// ... collect measurements ...

tbb64:total = accumulate_precise(measurements, 1000000);
if (total == ERR) {
    io.error("Measurement accumulation failed");
} else {
    tbb64:average = total / 1000000;
    io.print("Average: {tbb64}", average);
}
```

### Pattern: Saturating Arithmetic (64-bit)

```aria
tbb64:saturating_add = (tbb64:a, tbb64:b) -> tbb64 {
    if (a == ERR || b == ERR) {
        return ERR;  // Propagate errors
    }
    
    // Check for overflow using comparison (can't use i128 reliably)
    if (b > 0 && a > 9223372036854775807 - b) {
        return 9223372036854775807;  // Saturate at max
    }
    if (b < 0 && a < -9223372036854775807 - b) {
        return -9223372036854775807;  // Saturate at min
    }
    
    return a + b;
}

tbb64:x = 9000000000000000000;
tbb64:y = 500000000000000000;
tbb64:result = saturating_add(x, y);  // 9223372036854775807 (saturated, not ERR)
```

## Common Mistakes and Anti-Patterns

### ❌ Implicit Widening from Smaller TBB Types

```aria
tbb32:medium = ERR;
tbb64:large = medium;  // COMPILE ERROR! Implicit widening forbidden!

// ✓ Correct:
tbb64:large = tbb_widen<tbb64>(medium);
```

### ❌ Using INT64_MIN as Regular Value

```aria
// ❌ WRONG:
tbb64:value = -9223372036854775808;  // This is ERR!

// ✓ CORRECT: There is no larger TBB type, use i64 if you really need this value
i64:value = -9223372036854775808;  // Valid in i64 (but loses TBB semantics)
```

### ❌ Overflow in Ethereum Wei Calculations

```aria
// ❌ WRONG: 1 ETH = 10^18 wei, tbb64 max ≈ 9.2 * 10^18
tbb64:eth_balance = 100_000_000_000_000_000_000;  // 100 ETH in wei - OVERFLOW!

// ✓ CORRECT: Use Gwei (10^9 wei) for larger amounts
tbb64:gwei_balance = 100_000_000_000;  // 100 ETH in Gwei (10^11 Gwei) ✓
// Or use multi-precision arithmetic library
```

### ❌ Bitwise Ops Without ERR Checks

```aria
tbb64:value = ERR;                       // 0x8000000000000000
tbb64:masked = value & 0x7FFFFFFFFFFFFFFF; // 0x0000000000000000 (NOT ERR!)

// ✓ CORRECT:
if (value == ERR) {
    return ERR;
}
tbb64:masked = value & 0x7FFFFFFFFFFFFFFF;
```

### ❌ Precision Loss When Converting to f64

```aria
tbb64:precise = 9223372036854775807;    // Exact integer
f64:float_val = precise as f64;          // ~9.223372036854776e18 (rounded!)
tbb64:back = float_val as tbb64;         // NOT the same as original! Precision lost!

// ✓ CORRECT: Keep as tbb64 for exact arithmetic, or use frac64
frac64:exact = {9223372036854775807, 0, 1};  // Exact representation
```

## Integration with Error Handling

### With Result<T>

```aria
Result<tbb64>:parse_timestamp = (string:iso8601) -> Result<tbb64> {
    // Parse ISO 8601 timestamp to milliseconds since epoch
    // ... parsing logic ...
    
    i128:parsed_ms = /* ... */;  // May be out of tbb64 range
    
    if (parsed_ms < -9223372036854775807 || parsed_ms > 9223372036854775807) {
        return fail("Timestamp out of tbb64 range: " + iso8601);
    }
    
    return pass(parsed_ms as tbb64);
}

Result<tbb64>:r = parse_timestamp("2023-11-14T12:00:00Z");
if (r.is_error) {
    io.error("Parse error: " + r.err);
} else {
    tbb64:timestamp = r.val;
    // ... use timestamp ...
}
```

### With unknown

```aria
// Division by zero returns unknown, not ERR
tbb64:a = 1000000000000;
tbb64:b = 0;

// Custom function returns ERR for div-by-zero:
tbb64:safe_div = (tbb64:numerator, tbb64:denominator) -> tbb64 {
    if (numerator == ERR || denominator == ERR) {
        return ERR;
    }
    if (denominator == 0) {
        return ERR;  // Explicit ERR instead of unknown
    }
    return numerator / denominator;
}

tbb64:result = safe_div(1000000000000, 0);  // ERR (explicitly handled)
```

## Performance Considerations

### Memory Overhead

```aria
tbb64:single;       // 8 bytes (same as i64)
tbb64:array[1000];  // 8000 bytes (same as i64[1000])
```

**No memory overhead** compared to standard `i64`.

### Computational Overhead

**64-bit operations:** Modern 64-bit CPUs handle these natively. Overhead for overflow detection:

```assembly
; Standard i64 addition (1 cycle on 64-bit CPU):
add rax, rbx

; tbb64 addition with overflow check (2-3 cycles):
add rax, rbx
jo overflow_handler       ; Jump on overflow flag
jmp done
overflow_handler:
    mov rax, 0x8000000000000000   ; Set to ERR sentinel
done:
```

**On 32-bit systems:** tbb64 operations may require multi-instruction sequences, but this is true for i64 as well.

**Typical overhead:** 5-10% compared to unchecked arithmetic on 64-bit systems. **Safety gain:** Eliminates overflow bugs.

### When To Use tbb64 vs i64

**Use `tbb64` when:**
- Timestamps in milliseconds/microseconds (Y2038-proof)
- High-precision financial calculations (cryptocurrency)
- Error propagation simplifies logic
- Building block for frac64 or tfp64
- Maximum integer range with safety guarantees

**Use `i64` when:**
- Raw performance critical, overflow acceptable
- Full range including INT64_MIN needed
- Interfacing with hardware/protocols expecting standard `i64`

## Comparison with Other Types

### vs i64 (Standard Signed 64-bit Integer)

| Feature | tbb64 | i64 |
|---------|-------|-----|
| **Range** | `[-9223372036854775807, +9223372036854775807]` | `[-9223372036854775808, +9223372036854775807]` |
| **Overflow behavior** | Returns `ERR` | Wraps silently |
| **Asymmetry bugs** | Eliminated | `abs(INT64_MIN)` breaks |
| **Error propagation** | Automatic (sticky) | Manual checking required |
| **Use case** | Safety-critical, exact arithmetic, timestamps | General purpose, performance |

### vs tbb8/tbb16/tbb32

| Type | Range (approx) | ERR Sentinel | Size | Use Case |
|------|----------------|--------------|------|----------|
| **tbb8** | ±127 | `-128` | 1 byte | Small values, frac8 |
| **tbb16** | ±32K | `-32768` | 2 bytes | Audio, medium precision |
| **tbb32** | ±2.1 billion | INT32_MIN | 4 bytes | General purpose, frac32 |
| **tbb64** | ±9.2 quintillion | INT64_MIN | 8 bytes | **Timestamps, high precision, frac64** |

**tbb64 is the maximum capacity** TBB type.

### vs frac64 (Exact Rational Built on tbb64)

`frac64` is **built on top of** `tbb64`:

```aria
// frac64 structure:
struct:frac64 = {
    tbb64:whole;   // Integer part
    tbb64:num;     // Numerator
    tbb64:denom;   // Denominator
}

// tbb64 provides integer values
tbb64:integer = 1234567890123456789;

// frac64 provides exact rational values
frac64:rational = {1000000000000, 1, 3};  // 10^12 + 1/3 exactly
```

## Platform and Portability

### Bit Representation

`tbb64` uses standard two's complement representation, identical to `i64`:

| Value | Hexadecimal |
|-------|-------------|
| `9223372036854775807` | `0x7FFFFFFFFFFFFFFF` |
| `1` | `0x0000000000000001` |
| `0` | `0x0000000000000000` |
| `-1` | `0xFFFFFFFFFFFFFFFF` |
| `-9223372036854775807` | `0x8000000000000001` |
| **`ERR`** | **`0x8000000000000000`** |

### Endianness

**Eight-byte value:** Endianness matters for serialization/network transmission.

**Little-endian** (x86-64, ARM64):
- ERR (0x8000000000000000) stored as: `[0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80]`

**Big-endian** (network byte order):
- ERR (0x8000000000000000) stored as: `[0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]`

**Best practice:** Use network byte order conversion functions.

### Alignment

**Natural alignment:** 8 bytes (typically aligned to 8-byte boundaries):

```aria
struct:Data = {
    tbb64:field1;   // Offset 0, 8 bytes
    tbb64:field2;   // Offset 8, 8 bytes
    tbb64:field3;   // Offset 16, 8 bytes
    // Total size: 24 bytes, naturally aligned
}
```

## Best Practices

### ✅ DO: Use for Timestamps (Milliseconds)

```aria
// Unix time in milliseconds - good for ~292 million years
tbb64:timestamp_ms = system.current_time_ms();
```

### ✅ DO: Use tbb_widen for Widening

```aria
tbb32:medium = 2000000000;
tbb64:large = tbb_widen<tbb64>(medium);  // ✓ Correct

tbb32:err = ERR;
tbb64:err_large = tbb_widen<tbb64>(err);  // ✓ Preserves ERR
```

### ✅ DO: Use for High-Precision frac64

```aria
// Exact rational arithmetic with large values:
frac64:precise = {1000000000000, 355, 113};  // 10^12 + 355/113
```

### ✅ DO: Check Range for Ethereum Wei

```aria
// 1 ETH = 10^18 wei, tbb64 max ≈ 9.2 * 10^18
// Safe range: ~9.2 ETH in wei
tbb64:eth_wei = eth_to_wei(amount);
if (eth_wei == ERR) {
    io.error("Amount exceeds tbb64 capacity in wei - use Gwei");
}
```

### ❌ DON'T: Use INT64_MIN as Regular Value

```aria
// ❌ WRONG:
tbb64:value = -9223372036854775808;  // This is ERR!

// ✓ CORRECT: No larger TBB type exists
// If you really need this value, use i64 (loses TBB semantics)
```

### ❌ DON'T: Assume Full i64 Range

```aria
// ❌ WRONG:
for (i64:i = -9223372036854775808; i <= 9223372036854775807; i++) {
    // Includes ERR value!
}

// ✓ CORRECT: Use tbb64 range
for (tbb64:i = -9223372036854775807; i <= 9223372036854775807; i++) {
    // Valid range
}
```

## Relationship to Language Safety System

`tbb64` is a fundamental component of Aria's layered safety architecture:

**Layer 1: Result<T>** - Hard errors requiring explicit handling
**Layer 2: unknown** - Soft errors for undefined states
**Layer 3: failsafe()** - Last-resort safety net

**TBB types implement sticky error propagation for Layers 1 and 2.**

**Priority:** Safety > Stability > Accuracy > Performance > Convenience

`tbb64` sacrifices 1 value from the range (convenience) to gain automatic error propagation (safety).

## Summary

`tbb64` is a 64-bit **Twisted Balanced Binary** integer with:

- **Symmetric range:** `[-9223372036854775807, +9223372036854775807]` (eliminates asymmetry bugs)
- **ERR sentinel:** `-9223372036854775808` (0x8000000000000000) for automatic error signaling
- **Sticky error propagation:** `ERR + anything = ERR`
- **ARIA-018 protection:** Explicit widening with `tbb_widen<T>()`
- **Maximum capacity:** Widest TBB type, no larger variants exist
- **Foundation role:** Building block for `frac64`, `tfp64` mantissa, timestamps

**Use `tbb64` when:**
- **Timestamps** in milliseconds/microseconds (Y2038-proof, good for millennia)
- **Large-scale financial** systems (cryptocurrency satoshis, wei - check ranges!)
- **High-precision frac64** exact rational arithmetic
- **Scientific computing** with deterministic integer math
- **Maximum range** needed with overflow detection

**Avoid `tbb64` when:**
- Full range including INT64_MIN required (use `i64`)
- Raw performance critical and overflow acceptable

**Next:** See [tbb8](tbb8.md), [tbb16](tbb16.md), [tbb32](tbb32.md) for smaller TBB types, and [frac64](frac64.md) for exact rational arithmetic built on `tbb64`.
