# tbb16: 16-bit Twisted Balanced Binary Integer

## Overview

`tbb16` is a 16-bit **Twisted Balanced Binary** integer type with a perfectly symmetric range `[-32767, +32767]` and a reserved error sentinel value at `-32768`. Like its smaller sibling `tbb8`, `tbb16` sacrifices one value to provide built-in error propagation through the **sticky ERR sentinel mechanism**.

**Key characteristics:**
- **Range:** `-32767` to `+32767` (inclusive)
- **ERR sentinel:** `-32768` (bit pattern `0x8000`)
- **Sticky error propagation:** Any operation involving `ERR` produces `ERR`
- **Symmetric range:** Eliminates asymmetry bugs (`abs(-32767)` always works)
- **Foundation type:** Used as building block for `frac16` (exact rationals), `tfp32` exponent, and balanced number systems

```aria
tbb16:distance = 15000;          // Valid range value
tbb16:max_positive = 32767;      // Maximum positive value
tbb16:max_negative = -32767;     // Maximum negative value (symmetric!)
tbb16:error_state = ERR;         // Error sentinel (-32768 / 0x8000)
```

## Purpose and Philosophy

`tbb16` extends the TBB philosophy to 16-bit applications, providing wider range while maintaining automatic error propagation and symmetric range benefits.

### Why tbb16 vs tbb8?

**Use `tbb16` when:**
- Range `[-127, +127]` is insufficient
- Building `frac16` for exact rationals with larger numerators/denominators
- Component of `tfp32` (deterministic floating point)
- Medium-precision measurements (pressure sensors, acceleration, etc.)
- Audio processing (16-bit audio samples with overflow detection)

**Range comparison:**
- `tbb8`: `[-127, +127]` (256 values, 1 reserved for ERR)
- `tbb16`: `[-32767, +32767]` (65536 values, 1 reserved for ERR)

### Asymmetry Elimination (16-bit)

```aria
// Standard i16 problems:
i16:val = -32768;
i16:negated = -val;        // Overflow! -(-32768) = 32768, but max is 32767 → wraps to -32768
i16:absolute = abs(val);   // Same problem! abs(-32768) = 32768 → wraps to -32768

// tbb16 solution:
tbb16:val = -32767;        // Symmetric range, -32768 reserved for ERR
tbb16:negated = -val;      // 32767 ✓ Always works within range!
tbb16:absolute = abs(val); // 32767 ✓ No overflow possible!
```

### Sticky Error Propagation

```aria
tbb16:a = 30000;
tbb16:b = 5000;
tbb16:sum = a + b;         // 35000 exceeds 32767 → sum = ERR (automatic!)
tbb16:product = sum * 2;   // ERR * 2 → ERR (propagates!)
tbb16:final = product + 100; // ERR + 100 → ERR (still propagates!)

// Check error once at the end:
if (final == ERR) {
    io.error("Calculation overflow detected");
}
```

## Syntax and Literals

```aria
// Decimal literals with type suffix
tbb16:value = 15000tbb16;          // Explicit type suffix
tbb16:pos = 32767tbb16;            // Maximum positive
tbb16:neg = -32767tbb16;           // Maximum negative

// Type inference from context
tbb16:inferred = 12345;            // Compiler infers tbb16 from declaration

// ERR sentinel
tbb16:error = ERR;                 // Built-in error constant

// Hexadecimal (useful for bit patterns)
tbb16:pattern = 0x7FFF;            // 32767
tbb16:err_pattern = 0x8000;        // ERR (-32768)

// Binary (less common for 16-bit, but valid)
tbb16:flags = 0b0111111111111111;  // 32767
tbb16:err_binary = 0b1000000000000000; // ERR (-32768)
```

## Range and Representation

| Property | Value | Hexadecimal | Binary |
|----------|-------|-------------|--------|
| **Minimum valid** | `-32767` | `0x8001` | `1000 0000 0000 0001` |
| **Maximum valid** | `+32767` | `0x7FFF` | `0111 1111 1111 1111` |
| **ERR sentinel** | `-32768` | `0x8000` | `1000 0000 0000 0000` |
| **Size** | 16 bits | 2 bytes | 2 bytes |

**Memory layout:** Same as standard `i16` - two bytes, two's complement representation, platform-specific endianness for multi-byte storage. The difference is **semantic**: `-32768` is reserved for error signaling.

## Operations

### Arithmetic Operations

All arithmetic operations on `tbb16` include overflow detection and return `ERR` on overflow:

```aria
// Addition
tbb16:a = 30000;
tbb16:b = 5000;
tbb16:sum = a + b;              // ERR (35000 > 32767)

tbb16:c = 20000;
tbb16:d = 12767;
tbb16:sum2 = c + d;             // 32767 ✓

// Subtraction
tbb16:e = -30000;
tbb16:f = 5000;
tbb16:diff = e - f;             // ERR (-35000 < -32767)

tbb16:g = 5000;
tbb16:h = 37767;
tbb16:diff2 = g - h;            // -32767 ✓

// Multiplication
tbb16:x = 300;
tbb16:y = 150;
tbb16:product = x * y;          // ERR (45000 > 32767)

tbb16:m = 180;
tbb16:n = 182;
tbb16:product2 = m * n;         // 32760 ✓

// Division
tbb16:p = 30000;
tbb16:q = 5;
tbb16:quotient = p / q;         // 6000 ✓

tbb16:r = 10000;
tbb16:s = 0;
tbb16:div_zero = r / s;         // ERR (division by zero)

// Modulo
tbb16:mod = 32767 % 100;        // 67 ✓
tbb16:mod_err = 10000 % 0;      // ERR (modulo by zero)

// Negation
tbb16:val = 15000;
tbb16:negated = -val;           // -15000 ✓

tbb16:max = 32767;
tbb16:neg_max = -max;           // -32767 ✓ (symmetric range!)

// Standard i16 would fail here:
// i16:broken = -32768;
// -broken → 32768, but i16 max is 32767 → overflow!
```

### Sticky Error Propagation

```aria
tbb16:valid = 1000;
tbb16:err = ERR;

// ERR is sticky in all operations:
tbb16:r1 = err + 500;          // ERR
tbb16:r2 = 500 + err;          // ERR
tbb16:r3 = err - 1000;         // ERR
tbb16:r4 = err * 2;            // ERR
tbb16:r5 = err / 3;            // ERR
tbb16:r6 = -err;               // ERR

// Chained operations:
tbb16:a = 25000;
tbb16:b = 10000;
tbb16:c = a + b;               // ERR (overflow)
tbb16:d = c * 2;               // ERR (propagates!)
tbb16:e = d + 5000;            // ERR (still propagates!)

// Only check once at the end:
if (e == ERR) {
    io.error("Computation failed somewhere in the chain");
}
```

### Comparison Operations

```aria
tbb16:a = 15000;
tbb16:b = 20000;
tbb16:err = ERR;

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
bool:err_vs_min = (err < -32767);   // true
bool:err_vs_max = (err < 32767);    // true
bool:err_vs_zero = (err < 0);       // true
```

### Bitwise Operations

Bitwise operations on `tbb16` **do not propagate ERR** - they operate on raw bit patterns:

```aria
tbb16:a = 0b0101010101010101;  // 21845
tbb16:b = 0b0011001100110011;  // 13107

// Bitwise AND
tbb16:and_result = a & b;      // 0b0001000100010001 = 4369

// Bitwise OR
tbb16:or_result = a | b;       // 0b0111011101110111 = 30583

// Bitwise XOR
tbb16:xor_result = a ^ b;      // 0b0110011001100110 = 26214

// Bitwise NOT
tbb16:not_result = ~a;         // 0b1010101010101010 = -21846 (two's complement)

// Left shift
tbb16:left = 0b0000111100001111 << 4;  // 0b1111000011110000

// Right shift (arithmetic - preserves sign)
tbb16:right = 0b1111000011110000 >> 4; // 0b1111111100001111

// WARNING: Bitwise ops on ERR produce ordinary bit results!
tbb16:err = ERR;               // 0x8000 = -32768
tbb16:err_not = ~err;          // 0x7FFF = 32767 (NOT AN ERROR!)
```

**Critical safety note:** Bitwise operations treat `ERR` as just another bit pattern. Check inputs explicitly if you need error propagation.

### Absolute Value and Sign Operations

```aria
// Absolute value - always safe with symmetric range!
tbb16:pos = abs(32767);        // 32767 ✓
tbb16:neg = abs(-32767);       // 32767 ✓
tbb16:zero = abs(0);           // 0 ✓
tbb16:err_abs = abs(ERR);      // ERR (propagates)

// Compare to standard i16:
// i16:broken = abs(-32768);   // Overflow! Would wrap to -32768

// Sign extraction
tbb16:a = 15000;
tbb16:sign_pos = sign(a);      // 1
tbb16:sign_neg = sign(-5000);  // -1
tbb16:sign_zero = sign(0);     // 0
tbb16:sign_err = sign(ERR);    // ERR (propagates)

// Min/Max
tbb16:x = 10000;
tbb16:y = 20000;
tbb16:minimum = min(x, y);     // 10000
tbb16:maximum = max(x, y);     // 20000

// ERR handling in min/max:
tbb16:err = ERR;
tbb16:min_err = min(err, 5000);  // ERR (propagates)
tbb16:max_err = max(3000, err);  // ERR (propagates)
```

## Type Conversions

### To tbb16 (Narrowing or Cross-Type)

```aria
// From i32 (narrowing)
i32:large = 100000;
tbb16:narrow = large as tbb16; // ERR (100000 > 32767)

i32:fits = 20000;
tbb16:ok = fits as tbb16;      // 20000 ✓

// From tbb8 (widening - MUST use tbb_widen!)
tbb8:small = 100;
tbb16:widened = tbb_widen<tbb16>(small);  // 100 ✓

tbb8:small_err = ERR;          // tbb8 ERR (-128)
tbb16:err_widened = tbb_widen<tbb16>(small_err);  // tbb16 ERR (-32768) ✓

// COMPILE ERROR: Implicit widening forbidden!
// tbb16:broken = small_err;   // ERROR! Must use tbb_widen!

// From f32 (cross-type)
f32:pi = 3.14159;
tbb16:rounded = pi as tbb16;   // 3 ✓ (truncates toward zero)

f32:big = 50000.5;
tbb16:overflow = big as tbb16; // ERR (50000 > 32767)

f32:negative = -50000.5;
tbb16:underflow = negative as tbb16; // ERR (-50000 < -32767)

// From string (parsing)
string:num_str = "12345";
tbb16:parsed = parse_tbb16(num_str);  // 12345 ✓

string:invalid = "99999";
tbb16:parse_err = parse_tbb16(invalid); // ERR (out of range)
```

### From tbb16 (Widening)

**CRITICAL: ARIA-018 SENTINEL DISCONTINUITY CONSTRAINT**

**Must use `tbb_widen<T>()` to widen to larger TBB types!**

```aria
tbb16:value = 12345;
tbb32:wide = tbb_widen<tbb32>(value);  // 12345 ✓

tbb16:err = ERR;               // -32768 (tbb16 ERR)
tbb32:err_wide = tbb_widen<tbb32>(err);  // MIN_INT32 (tbb32 ERR) ✓

// COMPILE ERROR: Implicit widening forbidden!
// tbb32:broken = err;         // ERROR! ERR would "heal" to -32768 (valid in tbb32)
```

**Why this matters:**
```aria
// Pressure sensor returns tbb16 (range: -32767 to +32767 Pascals)
tbb16:pressure = read_pressure_sensor();  // Returns ERR (sensor failure)

// WRONG! Implicit widening (COMPILE ERROR):
// tbb32:storage = pressure;  // Would "heal" ERR to -32768 Pa!
// System thinks: "Pressure is -32768 Pa, that's vacuum but valid"
// Reality: Sensor is broken, reading is meaningless!

// ✓ CORRECT: Explicit widening preserves ERR state
tbb32:storage = tbb_widen<tbb32>(pressure);  // ERR remains ERR ✓
```

### Narrowing to tbb8

```aria
// Narrowing from tbb16 to tbb8 may overflow:
tbb16:value = 200;
tbb8:narrow = value as tbb8;   // ERR (200 > 127)

tbb16:fits = 100;
tbb8:ok = fits as tbb8;        // 100 ✓

// ERR propagates correctly:
tbb16:err = ERR;
tbb8:err_narrow = err as tbb8; // ERR (tbb8 ERR = -128)
```

## Use Cases

### 1. Audio Processing (16-bit Audio)

16-bit audio is the CD standard - `tbb16` provides overflow detection:

```aria
// Process 16-bit audio samples
tbb16:apply_gain = (tbb16:sample, i32:gain_db) -> tbb16 {
    // gain_db in decibels, convert to linear multiplier
    f64:multiplier = 10.0 ^ (gain_db as f64 / 20.0);
    i32:amplified = ((sample as f64) * multiplier) as i32;
    
    // Check range
    if (amplified > 32767 || amplified < -32767) {
        return ERR;  // Clipping detected
    }
    
    return amplified as tbb16;
}

tbb16:samples[44100];  // 1 second @ 44.1 kHz
// ... read samples from audio stream ...

for (u32:i = 0; i < 44100; i++) {
    samples[i] = apply_gain(samples[i], 6);  // +6 dB
    
    if (samples[i] == ERR) {
        // Clipping detected - apply limiting
        samples[i] = (samples[i] > 0) ? 32767 : -32767;
    }
}
```

### 2. Sensor Measurements (Medium Precision)

Temperature, pressure, acceleration sensors often use 16-bit precision:

```aria
// Temperature in centidegrees Celsius (range: -327.67°C to +327.67°C)
tbb16:read_temperature_centiC = () -> tbb16 {
    i32:raw = hardware.read_adc(TEMP_CHANNEL);
    
    if (raw < -32767 || raw > 32767) {
        return ERR;  // Sensor out of range or failed
    }
    
    return raw as tbb16;
}

tbb16:temp = read_temperature_centiC();
if (temp == ERR) {
    io.error("Temperature sensor failure!");
    shutdown_heater();
} else {
    f64:celsius = (temp as f64) / 100.0;
    io.print("Temperature: {f64}°C", celsius);
}
```

### 3. Financial Calculations (Larger Values)

For financial systems tracking values in smallest currency units (e.g., cents):

```aria
// Account balance in cents (range: -$327.67 to +$327.67)
tbb16:deposit_cents = (tbb16:balance, tbb16:amount) -> tbb16 {
    return balance + amount;  // Overflow detection automatic
}

tbb16:account = 15000;  // $150.00
tbb16:deposit = 5000;   // $50.00
tbb16:new_balance = deposit_cents(account, deposit);  // 20000 ($200.00) ✓

if (new_balance == ERR) {
    io.error("Transaction amount exceeds account limits");
}
```

### 4. Component of tfp32 (Deterministic Floating Point)

`tbb16` is used in `tfp32` structure:

```aria
// tfp32 (Twisted Floating Point 32-bit):
// struct:tfp32 = {
//     tbb16:exponent;   // 16-bit exponent
//     tbb16:mantissa;   // 16-bit mantissa
// }

// ERR in either component → entire tfp32 is ERR
tfp32:value = {100, 12345};      // Valid
tfp32:err_exp = {ERR, 12345};    // ERR (exponent is ERR)
tfp32:err_man = {100, ERR};      // ERR (mantissa is ERR)
```

### 5. Component of frac16 (Exact Rationals)

`tbb16` is the building block for `frac16`:

```aria
// frac16 structure:
// struct:frac16 = {
//     tbb16:whole;   // Integer part
//     tbb16:num;     // Numerator
//     tbb16:denom;   // Denominator
// }

frac16:ratio = {0, 1, 3};        // 1/3 exactly
frac16:mixed = {100, 1, 2};      // 100 1/2 exactly

// ERR propagation through frac16:
frac16:err_whole = {ERR, 0, 1};  // Whole part is ERR → frac is ERR
frac16:div_zero = {10, 1, 0};    // Denominator zero → ERR
```

### 6. Network Protocol Fields

Protocol fields using 16-bit signed values:

```aria
// TCP sequence number offset (signed 16-bit)
tbb16:calculate_offset = (tbb16:expected, tbb16:received) -> tbb16 {
    return received - expected;  // Overflow detection automatic
}

tbb16:expected_seq = 30000;
tbb16:received_seq = 31000;
tbb16:offset = calculate_offset(expected_seq, received_seq);  // 1000 ✓

if (offset == ERR) {
    io.error("Sequence number calculation overflow - possible attack or corruption");
}
```

## Common Patterns

### Pattern: Validated Range Conversion

```aria
tbb16:convert_from_i32 = (i32:value) -> tbb16 {
    if (value < -32767 || value > 32767) {
        return ERR;
    }
    return value as tbb16;
}

i32:user_input = get_user_input();
tbb16:validated = convert_from_i32(user_input);

if (validated == ERR) {
    io.error("Value out of tbb16 range [-32767, +32767]");
} else {
    process(validated);
}
```

### Pattern: Saturating Arithmetic

```aria
tbb16:saturating_add = (tbb16:a, tbb16:b) -> tbb16 {
    if (a == ERR || b == ERR) {
        return ERR;  // Propagate errors
    }
    
    i32:temp = (a as i32) + (b as i32);
    
    if (temp > 32767) {
        return 32767;   // Saturate at max
    }
    if (temp < -32767) {
        return -32767;  // Saturate at min
    }
    
    return temp as tbb16;
}

tbb16:x = 30000;
tbb16:y = 5000;
tbb16:result = saturating_add(x, y);  // 32767 (saturated, not ERR)
```

### Pattern: Multi-Sensor Averaging

```aria
tbb16:sensors[4];  // 4 redundant sensors

// Read all sensors
for (u32:i = 0; i < 4; i++) {
    sensors[i] = read_sensor(i);
}

// Calculate average, handling errors
tbb16:sum = 0;
u32:valid_count = 0;

for (u32:i = 0; i < 4; i++) {
    if (sensors[i] != ERR) {
        sum = sum + sensors[i];
        valid_count++;
    }
}

if (sum == ERR) {
    io.error("Sensor sum overflow");
} else if (valid_count == 0) {
    io.error("All sensors failed!");
} else {
    tbb16:average = sum / (valid_count as tbb16);
    io.print("Average reading: {tbb16}", average);
}
```

### Pattern: Protocol Checksum with Error Detection

```aria
tbb16:calculate_checksum = (u8:data[], u32:length) -> tbb16 {
    tbb16:sum = 0;
    
    for (u32:i = 0; i < length; i++) {
        sum = sum + (data[i] as tbb16);
        
        if (sum == ERR) {
            return ERR;  // Checksum overflow
        }
    }
    
    return ~sum;  // Bitwise NOT for checksum
}

u8:packet[256];
// ... receive packet ...

tbb16:checksum = calculate_checksum(packet, 256);
if (checksum == ERR) {
    io.error("Checksum calculation failed (packet too large)");
}
```

## Common Mistakes and Anti-Patterns

### ❌ Implicit Widening from tbb8

```aria
tbb8:small = ERR;
tbb16:big = small;  // COMPILE ERROR! Implicit widening forbidden!

// ✓ Correct:
tbb16:big = tbb_widen<tbb16>(small);
```

### ❌ Narrowing Without Validation

```aria
i32:large = 100000;
tbb16:narrow = large as tbb16;  // ERR silently!

// ✓ Better: Check explicitly
if (large < -32767 || large > 32767) {
    io.error("Value out of tbb16 range");
    return;
}
tbb16:narrow = large as tbb16;
```

### ❌ Using -32768 as Regular Value

```aria
// ❌ WRONG:
tbb16:value = -32768;  // This is ERR! Not a regular value!

// ✓ CORRECT: Use tbb32 if you need -32768
tbb32:value = -32768;  // Valid in tbb32 (ERR is MIN_INT32)
```

### ❌ Bitwise Ops Without ERR Checks

```aria
tbb16:value = ERR;             // 0x8000
tbb16:masked = value & 0x7FFF; // 0x0000 (NOT ERR!)

// ✓ CORRECT:
if (value == ERR) {
    return ERR;
}
tbb16:masked = value & 0x7FFF;
```

### ❌ Assuming i16 Range

```aria
// ❌ WRONG: Assuming full i16 range
tbb16:values[100];
for (i32:i = -32768; i <= 32767; i++) {  // Includes -32768!
    // ... processing ...
}

// ✓ CORRECT: Use tbb16 range
for (i32:i = -32767; i <= 32767; i++) {  // Valid tbb16 range
    // ... processing ...
}
```

## Integration with Error Handling

### With Result<T>

```aria
Result<tbb16>:read_config_pressure = (string:key) -> Result<tbb16> {
    string:value_str = config.get(key)?;  // Propagate config errors
    
    i32:parsed = parse_int(value_str);
    if (parsed < -32767 || parsed > 32767) {
        return fail("Pressure value out of tbb16 range: " + value_str);
    }
    
    return pass(parsed as tbb16);
}

Result<tbb16>:r = read_config_pressure("max_pressure");
if (r.is_error) {
    io.error("Config error: " + r.err);
} else {
    tbb16:max_pressure = r.val;
    // ... use max_pressure ...
}
```

### With unknown

```aria
// Division by zero returns unknown, not ERR
tbb16:a = 10000;
tbb16:b = 0;

// Custom function returns ERR for div-by-zero:
tbb16:safe_div = (tbb16:numerator, tbb16:denominator) -> tbb16 {
    if (numerator == ERR || denominator == ERR) {
        return ERR;
    }
    if (denominator == 0) {
        return ERR;  // Explicit ERR instead of unknown
    }
    return numerator / denominator;
}

tbb16:result = safe_div(10000, 0);  // ERR (explicitly handled)
```

## Performance Considerations

### Memory Overhead

```aria
tbb16:single;       // 2 bytes (same as i16)
tbb16:array[1000];  // 2000 bytes (same as i16[1000])
```

**No memory overhead** compared to standard `i16`.

### Computational Overhead

**Arithmetic operations:** Typically **1-3 cycles** overhead for overflow detection:

```assembly
; Standard i16 addition (1 cycle):
add ax, bx

; tbb16 addition with overflow check (2-3 cycles):
add ax, bx
jo overflow_handler   ; Jump on overflow flag
jmp done
overflow_handler:
    mov ax, 0x8000    ; Set to ERR sentinel
done:
```

**Comparison:** Same performance as `i16` - single comparison instruction.

**Typical overhead:** 5-10% compared to unchecked arithmetic. **Safety gain:** Eliminates overflow bugs.

### When To Use tbb16 vs i16

**Use `tbb16` when:**
- Overflow detection critical (financial, safety systems)
- Error propagation simplifies logic (sensor chains, pipelines)
- Building block for frac16 or tfp32
- Symmetric range needed

**Use `i16` when:**
- Raw performance critical, overflow acceptable
- Full range `[-32768, +32767]` needed
- Interfacing with hardware/protocols expecting standard `i16`

## Comparison with Other Types

### vs i16 (Standard Signed 16-bit Integer)

| Feature | tbb16 | i16 |
|---------|-------|-----|
| **Range** | `[-32767, +32767]` | `[-32768, +32767]` |
| **Overflow behavior** | Returns `ERR` | Wraps silently |
| **Asymmetry bugs** | Eliminated | `abs(-32768)` breaks |
| **Error propagation** | Automatic (sticky) | Manual checking required |
| **Use case** | Safety-critical, exact arithmetic | General purpose, performance |

### vs tbb8/tbb32/tbb64

| Type | Range | ERR Sentinel | Size | Use Case |
|------|-------|--------------|------|----------|
| **tbb8** | `[-127, +127]` | `-128` | 1 byte | Small values, frac8 |
| **tbb16** | `[-32767, +32767]` | `-32768` | 2 bytes | Medium precision, audio, frac16, tfp32 |
| **tbb32** | Large symmetric | MIN_INT32 | 4 bytes | General purpose, frac32 |
| **tbb64** | Large symmetric | MIN_INT64 | 8 bytes | High precision, frac64 |

**Remember:** Must use `tbb_widen<T>()` to convert between TBB widths!

### vs frac16 (Exact Rational Built on tbb16)

`frac16` is **built on top of** `tbb16`:

```aria
// frac16 structure:
struct:frac16 = {
    tbb16:whole;   // Integer part
    tbb16:num;     // Numerator
    tbb16:denom;   // Denominator
}

// tbb16 provides integer values
tbb16:integer = 12345;

// frac16 provides exact rational values
frac16:rational = {10, 1, 3};  // 10 1/3 exactly
```

## Platform and Portability

### Bit Representation

`tbb16` uses standard two's complement representation, identical to `i16`:

| Value | Decimal | Hexadecimal | Binary |
|-------|---------|-------------|--------|
| `32767` | 32767 | `0x7FFF` | `0111 1111 1111 1111` |
| `1` | 1 | `0x0001` | `0000 0000 0000 0001` |
| `0` | 0 | `0x0000` | `0000 0000 0000 0000` |
| `-1` | -1 | `0xFFFF` | `1111 1111 1111 1111` |
| `-32767` | -32767 | `0x8001` | `1000 0000 0000 0001` |
| **`ERR`** | -32768 | **`0x8000`** | **`1000 0000 0000 0000`** |

### Endianness

**Two-byte value:** Endianness matters for serialization/network transmission.

**Little-endian** (x86, x86-64, ARM64 typical):
- `ERR` (0x8000) stored as: `[0x00, 0x80]` in memory

**Big-endian** (network byte order, some embedded systems):
- `ERR` (0x8000) stored as: `[0x80, 0x00]` in memory

**Best practice:** Use network byte order conversion functions when transmitting:

```aria
tbb16:value = 12345;
u16:network_order = htons(value as u16);  // Host to network short
// ... send network_order over wire ...

u16:received = // ... receive from wire ...
tbb16:host_value = ntohs(received) as tbb16;  // Network to host short
```

### Alignment

**Natural alignment:** 2 bytes (typically aligned to even addresses):

```aria
struct:Data = {
    tbb16:field1;   // Offset 0, 2 bytes
    tbb16:field2;   // Offset 2, 2 bytes
    tbb16:field3;   // Offset 4, 2 bytes
    // Total size: 6 bytes, naturally aligned
}
```

## Best Practices

### ✅ DO: Use tbb_widen for Type Widening

```aria
tbb8:small = 42;
tbb16:big = tbb_widen<tbb16>(small);  // ✓ Correct

tbb8:err = ERR;
tbb16:err_big = tbb_widen<tbb16>(err);  // ✓ Preserves ERR
```

### ✅ DO: Check ERR Before Use

```aria
tbb16:sensor = read_sensor();
if (sensor == ERR) {
    io.error("Sensor failure!");
    return;
}
// Safe to use sensor
process(sensor);
```

### ✅ DO: Use for Audio Processing

```aria
// 16-bit audio with overflow detection
tbb16:samples[44100];
for (u32:i = 0; i < 44100; i++) {
    samples[i] = apply_effect(samples[i]);
    if (samples[i] == ERR) {
        // Clipping detected
        samples[i] = limit(samples[i]);
    }
}
```

### ✅ DO: Component of Higher Types

```aria
// frac16 for exact rationals
frac16:precise = {100, 1, 3};  // 100 1/3 exactly

// tfp32 exponent
tfp32:float_value = {10, 12345};  // tbb16 exponent + mantissa
```

### ❌ DON'T: Use -32768 as Regular Value

```aria
// ❌ WRONG:
tbb16:value = -32768;  // This is ERR!

// ✓ CORRECT: Use tbb32 if needed
tbb32:value = -32768;  // Valid in tbb32
```

### ❌ DON'T: Forget Endianness in Network Code

```aria
// ❌ WRONG: Direct transmission
tbb16:value = 12345;
send_bytes(&value, 2);  // Endianness problem!

// ✓ CORRECT: Convert to network byte order
u16:network = htons(value as u16);
send_bytes(&network, 2);
```

## Relationship to Language Safety System

`tbb16` is a fundamental component of Aria's layered safety architecture:

**Layer 1: Result<T>** - Hard errors requiring explicit handling
**Layer 2: unknown** - Soft errors for undefined states
**Layer 3: failsafe()** - Last-resort safety net

**TBB types implement sticky error propagation for Layers 1 and 2.**

**Priority:** Safety > Stability > Accuracy > Performance > Convenience

`tbb16` sacrifices 1 value from the range (convenience) to gain automatic error propagation (safety).

## Summary

`tbb16` is a 16-bit **Twisted Balanced Binary** integer with:

- **Symmetric range:** `[-32767, +32767]` (eliminates asymmetry bugs)
- **ERR sentinel:** `-32768` (0x8000) for automatic error signaling
- **Sticky error propagation:** `ERR + anything = ERR`
- **ARIA-018 protection:** Explicit widening with `tbb_widen<T>()`
- **Foundation role:** Building block for `frac16`, `tfp32` exponent, balanced number systems

**Use `tbb16` when:**
- 16-bit audio processing with overflow detection
- Medium-precision sensor measurements
- Error propagation simplifies code
- Component of frac16 or tfp32

**Avoid `tbb16` when:**
- Full range `[-32768, +32767]` required (use `i16`)
- Raw performance critical and overflow acceptable

**Next:** See [tbb8](tbb8.md), [tbb32](tbb32.md), [tbb64](tbb64.md) for other TBB types, and [frac16](frac16.md) for exact rational arithmetic built on `tbb16`.
