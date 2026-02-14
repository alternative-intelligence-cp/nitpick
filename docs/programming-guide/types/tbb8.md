# tbb8: 8-bit Twisted Balanced Binary Integer

## Overview

`tbb8` is an 8-bit **Twisted Balanced Binary** integer type with a perfectly symmetric range `[-127, +127]` and a reserved error sentinel value at `-128`. Unlike standard `i8` which has an asymmetric range `[-128, +127]`, `tbb8` sacrifices one value to provide built-in error propagation through the **sticky ERR sentinel mechanism**.

**Key characteristics:**
- **Range:** `-127` to `+127` (inclusive)
- **ERR sentinel:** `-128` (bit pattern `0x80`)
- **Sticky error propagation:** Any operation involving `ERR` produces `ERR`
- **Symmetric range:** Eliminates asymmetry bugs (`abs(-127)` always works, `neg(-127)` always works)
- **Foundation type:** Used as building block for `frac8` (exact rationals) and balanced number systems

```aria
tbb8:temperature = 25;           // Valid range value
tbb8:max_positive = 127;         // Maximum positive value
tbb8:max_negative = -127;        // Maximum negative value (symmetric!)
tbb8:error_state = ERR;          // Error sentinel (-128 / 0x80)
```

## Purpose and Philosophy

**tbb8** exists to solve three fundamental problems:

### 1. Asymmetry Bugs in Standard Integers

Standard two's complement integers have asymmetric ranges: `i8` is `[-128, +127]`. This creates subtle bugs:

```aria
// Standard i8 problems:
i8:val = -128;
i8:negated = -val;        // Overflow! -(-128) = 128, but max is 127 → wraps to -128
i8:absolute = abs(val);   // Same problem! abs(-128) = 128 → wraps to -128

// tbb8 solution:
tbb8:val = -127;          // Symmetric range, -128 reserved for ERR
tbb8:negated = -val;      // 127 ✓ Always works within range!
tbb8:absolute = abs(val); // 127 ✓ No overflow possible!
```

### 2. Automatic Error Propagation

Instead of wrapping silently on overflow or requiring explicit checks after every operation, `tbb8` propagates errors automatically:

```aria
tbb8:a = 100;
tbb8:b = 50;
tbb8:sum = a + b;         // 150 exceeds 127 → sum = ERR (automatic!)
tbb8:product = sum * 2;   // ERR * 2 → ERR (propagates!)
tbb8:final = product + 10; // ERR + 10 → ERR (still propagates!)

// Check error once at the end of a calculation pipeline:
if (final == ERR) {
    io.error("Calculation overflow detected");
}
```

This is called **sticky error propagation** - once `ERR` enters a computation, it "sticks" through all subsequent operations.

### 3. Foundation for Higher-Level Types

`tbb8` serves as the building block for:

- **frac8**: Exact rational numbers (`struct {tbb8:whole; tbb8:num; tbb8:denom}`)
- **Balanced ternary/nonary**: `trit` and `nit` types use tbb8's ERR sentinel
- **Safety-critical systems**: Sensor readings, control systems, robotics where invalid states must be detected

## Syntax and Literals

```aria
// Decimal literals with type suffix
tbb8:byte = 100tbb8;              // Explicit type suffix
tbb8:pos = 127tbb8;                // Maximum positive
tbb8:neg = -127tbb8;               // Maximum negative

// Type inference from context
tbb8:inferred = 42;                // Compiler infers tbb8 from declaration

// ERR sentinel
tbb8:error = ERR;                  // Built-in error constant

// Hexadecimal (useful for bit patterns)
tbb8:pattern = 0x7F;               // 127
tbb8:err_pattern = 0x80;           // ERR (-128)

// Binary (useful for flags/bit manipulation)
tbb8:flags = 0b01111111;           // 127
tbb8:err_binary = 0b10000000;      // ERR (-128)
```

**Type suffix requirement:**
Without context (e.g., standalone constants), use the `tbb8` suffix to distinguish from other integer types:

```aria
func:process = (tbb8:value) -> void {
    // ...
}

process(100tbb8);      // ✓ Explicit type
process(100);          // May infer as i32, type error!
```

## Range and Representation

| Property | Value | Hexadecimal | Binary |
|----------|-------|-------------|--------|
| **Minimum valid** | `-127` | `0x81` | `1000 0001` |
| **Maximum valid** | `+127` | `0x7F` | `0111 1111` |
| **ERR sentinel** | `-128` | `0x80` | `1000 0000` |
| **Size** | 8 bits | 1 byte | 1 byte |

**Memory layout:** Same as standard `i8` - single byte, two's complement representation. The difference is **semantic**: `-128` is reserved for error signaling, not used as a valid numerical value.

## Operations

### Arithmetic Operations

All arithmetic operations on `tbb8` include overflow detection and return `ERR` on overflow:

```aria
// Addition
tbb8:a = 100;
tbb8:b = 50;
tbb8:sum = a + b;              // ERR (150 > 127)

tbb8:c = 60;
tbb8:d = 67;
tbb8:sum2 = c + d;             // 127 ✓

// Subtraction
tbb8:e = -100;
tbb8:f = 50;
tbb8:diff = e - f;             // ERR (-150 < -127)

tbb8:g = 10;
tbb8:h = 137;
tbb8:diff2 = g - h;            // -127 ✓

// Multiplication
tbb8:x = 20;
tbb8:y = 10;
tbb8:product = x * y;          // ERR (200 > 127)

tbb8:m = 10;
tbb8:n = 12;
tbb8:product2 = m * n;         // 120 ✓

// Division
tbb8:p = 100;
tbb8:q = 5;
tbb8:quotient = p / q;         // 20 ✓

tbb8:r = 100;
tbb8:s = 0;
tbb8:div_zero = r / s;         // ERR (division by zero)

// Modulo
tbb8:mod = 127 % 10;           // 7 ✓
tbb8:mod_err = 100 % 0;        // ERR (modulo by zero)

// Negation
tbb8:val = 50;
tbb8:negated = -val;           // -50 ✓

tbb8:max = 127;
tbb8:neg_max = -max;           // -127 ✓ (symmetric range!)

// Standard i8 would fail here:
// i8:broken = -128;
// -broken → 128, but i8 max is 127 → overflow!
```

### Sticky Error Propagation

The defining feature of `tbb8` is that errors **propagate automatically**:

```aria
tbb8:valid = 10;
tbb8:err = ERR;

// ERR is sticky in all operations:
tbb8:r1 = err + 5;             // ERR
tbb8:r2 = 5 + err;             // ERR
tbb8:r3 = err - 10;            // ERR
tbb8:r4 = err * 2;             // ERR
tbb8:r5 = err / 3;             // ERR
tbb8:r6 = -err;                // ERR

// Chained operations:
tbb8:a = 100;
tbb8:b = 50;
tbb8:c = a + b;                // ERR (overflow)
tbb8:d = c * 2;                // ERR (propagates!)
tbb8:e = d + 1000;             // ERR (still propagates!)

// Only check once at the end:
if (e == ERR) {
    io.error("Computation failed somewhere in the chain");
}
```

**Why this matters:**
Traditional error handling requires checking after every operation. Sticky propagation lets you write computational pipelines and check once at the end, improving both readability and performance.

### Comparison Operations

```aria
tbb8:a = 50;
tbb8:b = 75;
tbb8:err = ERR;

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
bool:err_vs_min = (err < -127);   // true
bool:err_vs_max = (err < 127);    // true
bool:err_vs_zero = (err < 0);     // true

// This makes ERR sort to the beginning in sorted collections
```

**Best practice:** Always check for `ERR` explicitly before using comparison results in logic:

```aria
tbb8:temp = read_sensor();
if (temp == ERR) {
    io.error("Sensor failure!");
    return;
}

// Now safe to use temp in comparisons:
if (temp > 100) {
    activate_cooling();
}
```

### Bitwise Operations

Bitwise operations on `tbb8` **do not propagate ERR** - they operate on raw bit patterns:

```aria
tbb8:a = 0b01010101;  // 85
tbb8:b = 0b00110011;  // 51

// Bitwise AND
tbb8:and_result = a & b;       // 0b00010001 = 17

// Bitwise OR
tbb8:or_result = a | b;        // 0b01110111 = 119

// Bitwise XOR
tbb8:xor_result = a ^ b;       // 0b01100110 = 102

// Bitwise NOT
tbb8:not_result = ~a;          // 0b10101010 = -86 (two's complement)

// Left shift
tbb8:left = 0b00001111 << 2;   // 0b00111100 = 60

// Right shift (arithmetic - preserves sign)
tbb8:right = 0b11110000 >> 2;  // 0b11111100 = -4

// WARNING: Bitwise ops on ERR produce ordinary bit results!
tbb8:err = ERR;                // 0x80 = -128
tbb8:err_not = ~err;           // 0x7F = 127 (NOT AN ERROR!)
```

**Critical safety note:** Bitwise operations treat `ERR` as just another bit pattern. If you need error propagation with bitwise ops, check inputs explicitly:

```aria
func:safe_bitwise_and = (tbb8:a, tbb8:b) -> tbb8 {
    if (a == ERR || b == ERR) {
        return ERR;
    }
    return a & b;
}
```

### Absolute Value and Sign Operations

```aria
// Absolute value - always safe with symmetric range!
tbb8:pos = abs(127);           // 127 ✓
tbb8:neg = abs(-127);          // 127 ✓
tbb8:zero = abs(0);            // 0 ✓
tbb8:err_abs = abs(ERR);       // ERR (propagates)

// Compare to standard i8:
// i8:broken = abs(-128);      // Overflow! Would wrap to -128

// Sign extraction
tbb8:a = 75;
tbb8:sign_pos = sign(a);       // 1
tbb8:sign_neg = sign(-50);     // -1
tbb8:sign_zero = sign(0);      // 0
tbb8:sign_err = sign(ERR);     // ERR (propagates)

// Min/Max
tbb8:x = 30;
tbb8:y = 50;
tbb8:minimum = min(x, y);      // 30
tbb8:maximum = max(x, y);      // 50

// ERR handling in min/max:
tbb8:err = ERR;
tbb8:min_err = min(err, 50);   // ERR (propagates)
tbb8:max_err = max(30, err);   // ERR (propagates)
```

## Type Conversions

### To tbb8 (Narrowing or Cross-Type)

Converting to `tbb8` from wider types or different type families requires explicit casting and may produce `ERR`:

```aria
// From i32 (narrowing)
i32:large = 1000;
tbb8:narrow = large as tbb8;   // ERR (1000 > 127)

i32:fits = 100;
tbb8:ok = fits as tbb8;        // 100 ✓

// From f32 (cross-type)
f32:pi = 3.14;
tbb8:rounded = pi as tbb8;     // 3 ✓ (truncates toward zero)

f32:big = 200.5;
tbb8:overflow = big as tbb8;   // ERR (200 > 127)

f32:negative = -200.5;
tbb8:underflow = negative as tbb8; // ERR (-200 < -127)

// From string (parsing)
string:num_str = "42";
tbb8:parsed = parse_tbb8(num_str);  // 42 ✓

string:invalid = "999";
tbb8:parse_err = parse_tbb8(invalid); // ERR (out of range)

string:garbage = "hello";
tbb8:parse_err2 = parse_tbb8(garbage); // ERR (not a number)
```

### From tbb8 (Widening)

**CRITICAL: ARIA-018 SENTINEL DISCONTINUITY CONSTRAINT**

**NEVER implicitly widen `tbb8` to wider types!** This is a compile-time error in Aria.

**The problem:**
Each TBB width has a different ERR sentinel value:
- `tbb8` ERR = `-128` (0x80)
- `tbb16` ERR = `-32768` (0x8000)
- `tbb32` ERR = MIN_INT32
- `tbb64` ERR = MIN_INT64

Standard sign-extension **does not preserve ERR state:**

```aria
tbb8:err = ERR;                // -128 (0x80)
tbb16:broken = err;             // COMPILE ERROR! Implicit widening forbidden!
// If allowed, would produce: 0xFF80 = -128 in tbb16
// But -128 is VALID in tbb16, NOT the ERR sentinel (-32768)!
```

**Example danger:** Sensor failure becomes valid reading!

```aria
// Temperature sensor returns tbb8
tbb8:sensor_temp = read_temp_sensor();  // Returns ERR (sensor failure)

// WRONG! Implicit widening (COMPILE ERROR):
// tbb16:storage = sensor_temp;  // Would "heal" ERR to -128°C!
// System thinks: "Temperature is -128°C, that's cold but valid"
// Reality: Sensor is broken, reading is meaningless!
```

**Solution: Use `tbb_widen<T>()` intrinsic**

The `tbb_widen<T>()` function correctly maps ERR sentinels across widths:

```aria
tbb8:err = ERR;                         // -128 (tbb8 ERR)
tbb16:wide = tbb_widen<tbb16>(err);     // -32768 (tbb16 ERR) ✓
tbb32:wider = tbb_widen<tbb32>(wide);   // MIN_INT32 (tbb32 ERR) ✓

tbb8:valid = 42;
tbb16:valid_wide = tbb_widen<tbb16>(valid);  // 42 ✓
tbb32:valid_wider = tbb_widen<tbb32>(valid); // 42 ✓
```

**Implementation:** `tbb_widen<T>()` compiles to a branchless conditional move (cmov/csel) instruction - zero performance overhead!

```assembly
; Pseudo-assembly for tbb_widen<tbb16>(tbb8_value)
; x86-64 example:
movsx   rax, byte_val      ; Sign-extend -128 → 0xFF80
cmp     byte_val, 0x80     ; Check if input is tbb8 ERR
mov     rcx, 0x8000        ; tbb16 ERR sentinel
cmove   rax, rcx           ; If input was ERR, replace with tbb16 ERR
; Result: ERR → ERR, all other values → sign-extended
```

**Widening to non-TBB types:**

```aria
// To standard integers (ERR maps to specific value)
tbb8:err = ERR;
i32:int_err = tbb_to_i32(err);   // Defined behavior: maps to specific error value or throws

// To floats (ERR maps to NaN or specific value)
tbb8:temp = 25;
f32:float_temp = temp as f32;    // 25.0 ✓

tbb8:err_temp = ERR;
f32:float_err = err_temp as f32; // Implementation-defined (NaN, or -Infinity, check docs)
```

## Use Cases

### 1. Sensor Readings and Control Systems

TBB types excel in safety-critical systems where invalid sensor readings must be detected:

```aria
// Temperature control system
tbb8:read_temperature = () -> tbb8 {
    // Hardware read might fail
    i32:raw = hardware.read_adc(TEMP_CHANNEL);
    
    if (raw < -127 || raw > 127) {
        return ERR;  // Out of valid temperature range
    }
    
    return raw as tbb8;
}

tbb8:current_temp = read_temperature();
tbb8:setpoint = 72;
tbb8:error = current_temp - setpoint;  // If sensor failed, error = ERR

// ERR propagates through control calculations:
tbb8:correction = error * 2;           // ERR if sensor failed
tbb8:output = correction + 50;         // ERR propagates

// Check once before actuating:
if (output == ERR) {
    io.error("Sensor failure detected, entering safe mode");
    shutdown_heater();
    return;
}

// Safe to use output value:
set_heater_power(output);
```

### 2. Financial Calculations (Small Values)

For financial systems with small integer values (e.g., percentage points, small currency denominations):

```aria
// Stock price changes in pennies (range: -$1.27 to +$1.27)
tbb8:calculate_price_change = (tbb8:current, tbb8:previous) -> tbb8 {
    return current - previous;  // Auto-detects overflow
}

tbb8:today = 125;    // $1.25
tbb8:yesterday = 120; // $1.20
tbb8:change = calculate_price_change(today, yesterday);  // 5 ($0.05)

if (change == ERR) {
    io.error("Price change calculation overflow");
}

// Compound calculations:
tbb8:fee_percent = 3;  // 3%
tbb8:adjusted = today - ((today * fee_percent) / 100);  // Check for ERR
```

### 3. Audio/Video Sample Processing

Detect corruption in media pipelines:

```aria
// Audio sample processing (8-bit audio range: -127 to +127)
tbb8:apply_gain = (tbb8:sample, tbb8:gain_db) -> tbb8 {
    // Gain calculation - overflow produces ERR
    return sample + gain_db;
}

tbb8:samples[1024];
// ... fill samples from audio stream ...

for (u32:i = 0; i < 1024; i++) {
    samples[i] = apply_gain(samples[i], 10);
    
    if (samples[i] == ERR) {
        // Sample clipped - handle gracefully
        samples[i] = 127;  // Hard limit instead of overflow
    }
}
```

### 4. State Machines

Use `ERR` to represent invalid state transitions:

```aria
enum:State = {
    IDLE = 0,
    STARTING = 1,
    RUNNING = 2,
    STOPPING = 3,
}

tbb8:transition = (tbb8:current_state, tbb8:event) -> tbb8 {
    if (current_state == State.IDLE && event == Event.START) {
        return State.STARTING;
    }
    if (current_state == State.STARTING && event == Event.READY) {
        return State.RUNNING;
    }
    if (current_state == State.RUNNING && event == Event.STOP) {
        return State.STOPPING;
    }
    if (current_state == State.STOPPING && event == Event.STOPPED) {
        return State.IDLE;
    }
    
    return ERR;  // Invalid transition
}

tbb8:state = State.IDLE;
tbb8:new_state = transition(state, Event.START);

if (new_state == ERR) {
    io.error("Invalid state transition attempted");
} else {
    state = new_state;
}
```

### 5. Branchless Computation Pipelines

Check error once at the end instead of after every operation:

```aria
// Compute: result = ((a + b) * c - d) / e
tbb8:pipeline = (tbb8:a, tbb8:b, tbb8:c, tbb8:d, tbb8:e) -> tbb8 {
    tbb8:sum = a + b;      // May overflow → ERR
    tbb8:prod = sum * c;   // ERR propagates if sum was ERR, or new overflow
    tbb8:diff = prod - d;  // ERR propagates or new underflow
    tbb8:quot = diff / e;  // ERR propagates or new div-by-zero
    
    return quot;  // Check once at the end
}

tbb8:result = pipeline(10, 20, 3, 15, 5);
if (result == ERR) {
    io.error("Pipeline computation failed");
} else {
    io.print("Result: {tbb8}", result);
}

// No branches in the pipeline! CPU can execute speculatively.
```

### 6. Foundation for Exact Rationals

`tbb8` is a component of `frac8` (exact rational numbers):

```aria
// frac8 structure:
// struct:frac8 = {
//     tbb8:whole;   // Integer part
//     tbb8:num;     // Numerator (fractional part)
//     tbb8:denom;   // Denominator
// }

frac8:ratio = {0, 1, 3};  // 1/3 exactly (not 0.333...)
frac8:mixed = {2, 1, 2};  // 2 1/2 exactly (not 2.5 with rounding errors)

// ERR propagation through frac8:
frac8:err_frac = {ERR, 0, 1};  // Whole part is ERR → entire frac is ERR
frac8:div_zero = {1, 1, 0};     // Denominator is zero → ERR

// Arithmetic on frac8 preserves exactness and propagates errors
```

## Common Patterns

### Pattern: Validated Input Processing

```aria
tbb8:process_user_input = (string:input) -> tbb8 {
    i32:parsed = parse_int(input);
    
    // Validate range
    if (parsed < -127 || parsed > 127) {
        return ERR;
    }
    
    return parsed as tbb8;
}

string:user_value = io.read_line();
tbb8:value = process_user_input(user_value);

if (value == ERR) {
    io.error("Invalid input: must be between -127 and 127");
} else {
    // Safe to use value
    process(value);
}
```

### Pattern: Error Accumulation

```aria
tbb8:values[100];
// ... initialize with sensor readings ...

tbb8:sum = 0;
for (u32:i = 0; i < 100; i++) {
    sum = sum + values[i];  // Accumulates ERR if any sensor failed
}

if (sum == ERR) {
    io.error("One or more sensors failed, or sum overflowed");
} else {
    tbb8:average = sum / 100;
    io.print("Average: {tbb8}", average);
}
```

### Pattern: Fallback Chain

```aria
tbb8:primary_sensor = read_sensor_a();
tbb8:backup_sensor = ERR;
tbb8:final_value = ERR;

if (primary_sensor != ERR) {
    final_value = primary_sensor;
} else {
    io.warn("Primary sensor failed, trying backup...");
    backup_sensor = read_sensor_b();
    
    if (backup_sensor != ERR) {
        final_value = backup_sensor;
    } else {
        io.error("Both sensors failed!");
    }
}

if (final_value != ERR) {
    process_measurement(final_value);
}
```

### Pattern: Saturating Arithmetic

Sometimes you want to clamp to range instead of returning ERR:

```aria
tbb8:saturating_add = (tbb8:a, tbb8:b) -> tbb8 {
    if (a == ERR || b == ERR) {
        return ERR;  // Propagate errors
    }
    
    i32:temp = (a as i32) + (b as i32);
    
    if (temp > 127) {
        return 127;   // Saturate at max
    }
    if (temp < -127) {
        return -127;  // Saturate at min
    }
    
    return temp as tbb8;
}

tbb8:x = 100;
tbb8:y = 50;
tbb8:result = saturating_add(x, y);  // 127 (saturated, not ERR)
```

## Common Mistakes and Anti-Patterns

### ❌ Implicit Widening (Compile Error)

```aria
tbb8:small = 42;
tbb16:big = small;  // COMPILE ERROR! Implicit widening forbidden!

// ✓ Correct:
tbb16:big = tbb_widen<tbb16>(small);
```

### ❌ Ignoring ERR in Calculations

```aria
// ❌ WRONG: Using value without checking for ERR
tbb8:sensor = read_sensor();
tbb8:scaled = sensor * 10;  // If sensor is ERR, scaled is also ERR!
set_output(scaled);          // Garbage output if ERR!

// ✓ CORRECT: Check before use
tbb8:sensor = read_sensor();
if (sensor == ERR) {
    io.error("Sensor failure!");
    return;
}
tbb8:scaled = sensor * 10;
set_output(scaled);
```

### ❌ Bitwise Ops Breaking ERR Propagation

```aria
tbb8:err = ERR;              // 0x80
tbb8:masked = err & 0x7F;    // 0x00 (NOT ERR anymore!)

// ✓ CORRECT: Check inputs explicitly for bitwise ops
tbb8:safe_mask = (tbb8:value, tbb8:mask) -> tbb8 {
    if (value == ERR || mask == ERR) {
        return ERR;
    }
    return value & mask;
}
```

### ❌ Comparing ERR Without Explicit Check

```aria
tbb8:temp = read_sensor();

// ❌ WRONG: Numerical comparison with potential ERR
if (temp < 0) {  // If temp is ERR, this is technically true (ERR sorts lowest)
    activate_heater();  // Activating heater for sensor failure!
}

// ✓ CORRECT: Explicit ERR check first
tbb8:temp = read_sensor();
if (temp == ERR) {
    io.error("Sensor failure");
    return;
}
if (temp < 0) {  // Now safe - temp is valid
    activate_heater();
}
```

### ❌ Using ERR as a Magic Number

```aria
// ❌ WRONG: Using -128 instead of ERR
tbb8:value = -128;  // This IS ERR! Don't use as a value!

// ✓ CORRECT: Use ERR constant
tbb8:error_state = ERR;

// ✓ CORRECT: If you really need -128 as a value, use wider type
tbb16:value = -128;  // Valid in tbb16 (ERR is -32768)
```

### ❌ Forgetting Symmetric Range

```aria
// ❌ WRONG: Assuming i8 range
tbb8:value = -128;  // This is ERR, not a valid value!

// ✓ CORRECT: Remember symmetric range
tbb8:min_value = -127;  // Minimum valid value
tbb8:max_value = 127;   // Maximum valid value
```

## Integration with Error Handling

### With Result<T>

```aria
Result<tbb8>:read_config_value = (string:key) -> Result<tbb8> {
    string:value_str = config.get(key)?;  // Propagate config errors
    
    i32:parsed = parse_int(value_str);
    if (parsed < -127 || parsed > 127) {
        return fail("Value out of tbb8 range: " + value_str);
    }
    
    return pass(parsed as tbb8);
}

Result<tbb8>:r = read_config_value("max_retries");
if (r.is_error) {
    io.error("Config error: " + r.err);
} else {
    tbb8:retries = r.val;  // Guaranteed valid tbb8
    // ... use retries ...
}
```

### With unknown

```aria
// Division by zero returns unknown, not ERR
tbb8:a = 100;
tbb8:b = 0;

// Standard division returns unknown for div-by-zero:
// tbb8:result = a / b;  // unknown (not ERR!)

// Custom function can return ERR for div-by-zero:
tbb8:safe_div = (tbb8:numerator, tbb8:denominator) -> tbb8 {
    if (numerator == ERR || denominator == ERR) {
        return ERR;
    }
    if (denominator == 0) {
        return ERR;  // Explicit ERR instead of unknown
    }
    return numerator / denominator;
}

tbb8:result = safe_div(100, 0);  // ERR (explicitly handled)
```

### With failsafe()

```aria
tbb8:critical_sensor = read_critical_sensor();

// Critical systems: ERR triggers failsafe
if (critical_sensor == ERR) {
    failsafe("Critical sensor failure detected");  // Terminates system safely
}

// Alternative: Emphatic unwrap
// Result<tbb8>:sensor_result = read_sensor_with_diagnostics();
// tbb8:value = sensor_result?!;  // Calls failsafe() if error
```

## Performance Considerations

### Memory Overhead

```aria
tbb8:single;        // 1 byte (same as i8)
tbb8:array[1000];   // 1000 bytes (same as i8[1000])
```

**No memory overhead** compared to standard `i8`.

### Computational Overhead

**Arithmetic operations:** Modern CPUs typically add **1-2 cycles** for overflow detection compared to unchecked arithmetic:

```assembly
; Standard i8 addition (1 cycle):
add al, bl

; tbb8 addition with overflow check (2-3 cycles):
add al, bl
jo overflow_handler   ; Jump on overflow flag
jmp done
overflow_handler:
    mov al, 0x80      ; Set to ERR sentinel
done:
```

**Sticky ERR propagation:** After first ERR, propagation is essentially free (single comparison):

```assembly
; Subsequent operations just check and skip:
cmp al, 0x80          ; Is current value ERR?
je skip_operation     ; Yes, skip the op
add al, bl            ; No, do the operation
skip_operation:
```

**Branchless optimization:** Compilers often generate branchless code using conditional moves:

```assembly
; Branchless ERR propagation:
add al, bl            ; Do addition
jo overflow           ; Check overflow
cmp al, 0x80          ; Check if input was ERR
mov cl, 0x80          ; ERR sentinel
cmove al, cl          ; If input was ERR, result is ERR
overflow:
mov al, 0x80          ; If overflow, result is ERR
```

**Typical overhead:** 5-10% compared to unchecked arithmetic. **Safety gain:** Eliminates entire classes of overflow bugs.

### Comparison Performance

```aria
// Checking for ERR: 1 comparison (1 cycle)
if (value == ERR) { ... }

// Standard comparison: Also 1 cycle
if (value > 100) { ... }
```

**No performance difference** for ERR checks vs. normal comparisons.

### When To Use tbb8 vs i8

**Use `tbb8` when:**
- Overflow detection is critical (financial, safety systems)
- Error propagation simplifies logic (sensor chains, pipelines)
- Symmetric range eliminates asymmetry bugs
- Building block for higher types (frac8)

**Use `i8` when:**
- Raw performance is critical, overflow acceptable
- Full range is needed (including -128)
- Bit manipulation without error semantics
- Interfacing with hardware/protocols expecting standard `i8`

## Comparison with Other Types

### vs i8 (Standard Signed 8-bit Integer)

| Feature | tbb8 | i8 |
|---------|------|-----|
| **Range** | `[-127, +127]` | `[-128, +127]` |
| **Overflow behavior** | Returns `ERR` | Wraps silently |
| **Asymmetry bugs** | Eliminated | `abs(-128)` breaks |
| **Error propagation** | Automatic (sticky) | Manual checking required |
| **Negation safety** | `-(−127)` always works | `-(−128)` overflows |
| **Use case** | Safety-critical, exact arithmetic | General purpose, performance |

```aria
// Asymmetry bug in i8:
i8:val = -128;
i8:neg = -val;         // Overflow! Result: -128 (wraps)
i8:abs_val = abs(val); // Overflow! Result: -128 (wraps)

// No asymmetry in tbb8:
tbb8:val = -127;       // -128 reserved for ERR
tbb8:neg = -val;       // 127 ✓ Works perfectly!
tbb8:abs_val = abs(val); // 127 ✓ Always safe!
```

### vs u8 (Unsigned 8-bit Integer)

| Feature | tbb8 | u8 |
|---------|------|-----|
| **Range** | `[-127, +127]` | `[0, 255]` |
| **Signed** | Yes | No |
| **Error sentinel** | Yes (`ERR` = -128) | No built-in |
| **Use case** | Signed values with error detection | Unsigned values, raw bytes |

```aria
// tbb8: Signed temperature with error detection
tbb8:temp = -15;  // -15°C ✓
tbb8:err_temp = ERR;  // Sensor failure

// u8: Unsigned byte, no negatives
u8:byte = 255;    // ✓
u8:neg = -1;      // Type error! u8 is unsigned
```

### vs tbb16/32/64 (Wider TBB Types)

| Type | Range | ERR Sentinel | Size |
|------|-------|--------------|------|
| **tbb8** | `[-127, +127]` | `-128` (0x80) | 1 byte |
| **tbb16** | `[-32767, +32767]` | `-32768` (0x8000) | 2 bytes |
| **tbb32** | `[-2147483647, +2147483647]` | MIN_INT32 | 4 bytes |
| **tbb64** | Large symmetric range | MIN_INT64 | 8 bytes |

**Key consideration:** **Must use `tbb_widen<T>()` to convert between TBB widths!**

```aria
tbb8:small = 42;
tbb16:big = tbb_widen<tbb16>(small);  // ✓ Correct

tbb8:err = ERR;
tbb16:err_big = tbb_widen<tbb16>(err);  // ✓ Maps ERR correctly
```

### vs frac8 (Exact Rational Built on tbb8)

`frac8` is **built on top of** `tbb8`:

```aria
// frac8 structure:
struct:frac8 = {
    tbb8:whole;   // Integer part
    tbb8:num;     // Numerator
    tbb8:denom;   // Denominator
}

// tbb8 provides integer values
tbb8:integer = 42;

// frac8 provides exact rational values
frac8:rational = {0, 1, 3};  // 1/3 exactly
```

**frac8 inherits error propagation from tbb8:** If any component (whole, num, denom) is `ERR`, the entire fraction is `ERR`.

## Platform and Portability

### Bit Representation

`tbb8` uses standard two's complement representation, identical to `i8`:

| Value | Decimal | Hexadecimal | Binary |
|-------|---------|-------------|--------|
| `127` | 127 | `0x7F` | `0111 1111` |
| `1` | 1 | `0x01` | `0000 0001` |
| `0` | 0 | `0x00` | `0000 0000` |
| `-1` | -1 | `0xFF` | `1111 1111` |
| `-127` | -127 | `0x81` | `1000 0001` |
| **`ERR`** | -128 | **`0x80`** | **`1000 0000`** |

**Portability:** Bit pattern `0x80` always represents `ERR` on all platforms (x86, ARM, RISC-V, etc.).

### Endianness

Single-byte type, **endianness irrelevant** (no multi-byte ordering).

### Alignment

**Natural alignment:** 1 byte (can be stored at any address).

```aria
struct:Sensors = {
    tbb8:temp;      // Offset 0, 1 byte
    tbb8:humidity;  // Offset 1, 1 byte
    tbb8:pressure;  // Offset 2, 1 byte
    // Total size: 3 bytes, tightly packed
}
```

### Cross-Platform Consistency

**ERR detection is consistent across all platforms:**

```aria
tbb8:value = get_from_network();  // Received from different architecture

if (value == ERR) {  // ✓ Works on all platforms
    handle_error();
}
```

## Best Practices

### ✅ DO: Check for ERR Explicitly

```aria
tbb8:result = compute();
if (result == ERR) {
    io.error("Computation failed");
    return;
}
// Safe to use result
process(result);
```

### ✅ DO: Use Sticky Propagation for Pipelines

```aria
tbb8:result = step1(input)
    .step2()
    .step3()
    .step4();

// Check once at the end
if (result == ERR) {
    handle_error();
}
```

### ✅ DO: Use tbb_widen<T>() for Widening

```aria
tbb8:small = 42;
tbb16:big = tbb_widen<tbb16>(small);  // ✓ Correct
```

### ✅ DO: Prefer tbb8 for Safety-Critical Code

```aria
// Robotics control:
tbb8:servo_position = calculate_servo_angle();
if (servo_position == ERR) {
    emergency_stop();  // Invalid position detected!
}
```

### ✅ DO: Use ERR for Invalid States

```aria
tbb8:state_machine_transition = (tbb8:state, tbb8:event) -> tbb8 {
    // ... valid transitions ...
    return ERR;  // Invalid transition
}
```

### ❌ DON'T: Use -128 as a Regular Value

```aria
// ❌ WRONG:
tbb8:value = -128;  // This is ERR! Not a regular value!

// ✓ CORRECT: Use tbb16 if you need -128
tbb16:value = -128;  // Valid in tbb16 (ERR is -32768)
```

### ❌ DON'T: Implicitly Widen

```aria
// ❌ COMPILE ERROR:
tbb8:small = ERR;
tbb16:big = small;  // FORBIDDEN!

// ✓ CORRECT:
tbb16:big = tbb_widen<tbb16>(small);
```

### ❌ DON'T: Ignore Overflow in Critical Paths

```aria
// ❌ WRONG:
tbb8:payment = base_amount + fees;  // Might be ERR!
charge_customer(payment);           // Charging garbage!

// ✓ CORRECT:
tbb8:payment = base_amount + fees;
if (payment == ERR) {
    io.error("Payment calculation overflow");
    return;
}
charge_customer(payment);
```

### ❌ DON'T: Use Bitwise Ops Without ERR Checks

```aria
// ❌ WRONG: Bitwise ops don't propagate ERR
tbb8:value = ERR;
tbb8:masked = value & 0x7F;  // 0x00 (NOT ERR!)

// ✓ CORRECT:
if (value == ERR) {
    return ERR;
}
tbb8:masked = value & 0x7F;
```

## Relationship to Language Safety System

`tbb8` is a fundamental component of Aria's layered safety architecture:

**Layer 1: Result<T>** - Hard errors requiring explicit handling
**Layer 2: unknown** - Soft errors for undefined states
**Layer 3: failsafe()** - Last-resort safety net

**TBB types implement sticky error propagation for Layers 1 and 2:**

```aria
// Result<T> uses sticky ERR propagation internally
Result<tbb8>:r = compute();
if (r.is_error) {
    // r.val is ERR, r.err contains error message
}

// unknown integrates with TBB error checking
tbb8:maybe_err = risky_operation();
if (maybe_err == ERR) {
    // Explicit check converts ERR to error handling
}
```

**Priority:** Safety > Stability > Accuracy > Performance > Convenience

`tbb8` sacrifices 1 value from the range (convenience) to gain automatic error propagation (safety).

## Summary

`tbb8` is an 8-bit **Twisted Balanced Binary** integer with:

- **Symmetric range:** `[-127, +127]` (eliminates asymmetry bugs)
- **ERR sentinel:** `-128` (0x80) for automatic error signaling
- **Sticky error propagation:** `ERR + anything = ERR`
- **ARIA-018 protection:** Explicit widening with `tbb_widen<T>()` prevents sentinel discontinuity
- **Foundation role:** Building block for `frac8` exact rationals

**Use `tbb8` when:**
- Safety-critical systems (sensors, robotics, medical, aerospace)
- Automatic error propagation simplifies code
- Overflow detection is critical
- Symmetric range eliminates edge cases

**Avoid `tbb8` when:**
- Full range `[-128, +127]` is required (use `i8`)
- Raw performance is critical and overflow is acceptable
- Bitwise manipulation without error semantics

**Next:** See [tbb16](tbb16.md), [tbb32](tbb32.md), [tbb64](tbb64.md) for wider TBB types, and [frac8](frac8.md) for exact rational arithmetic built on `tbb8`.
