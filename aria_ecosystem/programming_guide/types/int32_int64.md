# Standard Medium/Large Signed Integers (int32, int64)

**Category**: Types → Integers → Standard Signed  
**Sizes**: 32-bit, 64-bit  
**Ranges**: int32 (±2.1 billion), int64 (±9.2 quintillion)  
**Purpose**: Production workhorse integers with wrapping overflow  
**Status**: ✅ FULLY IMPLEMENTED

---

## Table of Contents

1. [Overview](#overview)
2. [int32: 32-bit Signed Integer](#int32-32-bit-signed-integer)
3. [int64: 64-bit Signed Integer](#int64-64-bit-signed-integer)
4. [vs TBB Types (Critical Differences)](#vs-tbb-types-critical-differences)
5. [When to Use Standard Ints vs TBB](#when-to-use-standard-ints-vs-tbb)
6. [Overflow Wrapping Behavior](#overflow-wrapping-behavior)
7. [abs() Undefined Behavior with Minimum Values](#abs-undefined-behavior-with-minimum-values)
8. [Two's Complement Representation](#twos-complement-representation)
9. [Arithmetic Operations](#arithmetic-operations)
10. [The Year 2038 Problem](#the-year-2038-problem)
11. [64-bit Timestamps and Beyond](#64-bit-timestamps-and-beyond)
12. [C Interoperability](#c-interoperability)
13. [Performance Characteristics](#performance-characteristics)
14. [Common Patterns](#common-patterns)
15. [Anti-Patterns](#anti-patterns)
16. [Migration from C/C++](#migration-from-cc)
17. [Related Concepts](#related-concepts)

---

## Overview

Aria's **int32** and **int64** are the production workhorses - traditional two's complement integers with **asymmetric ranges** and **wrapping overflow**.

**Key Properties**:
- **Asymmetric ranges**: One more negative value than positive
- **Wrapping overflow**: Defined behavior (not UB like C!)
- **Industry standard**: Compatible with all modern platforms
- **No error propagation**: No ERR sentinel like TBB
- **Fast**: Native CPU instructions, zero overhead

**⚠️ CRITICAL**: Wrapping overflow can cause subtle bugs (Year 2038 problem!). Use TBB if overflow detection needed.

---

## int32: 32-bit Signed Integer

### Range

**Minimum**: -2,147,483,648 (0x80000000, -2^31)  
**Maximum**: +2,147,483,647 (0x7FFFFFFF, 2^31 - 1)  
**Total values**: 4,294,967,296 (2^32)

**Approximately**: ±2.1 billion

**Asymmetric**: One more negative value (abs(-2,147,483,648) has no int32 representation!)

### Declaration

```aria
int32:count = 1000000;
int32:database_id = -123456;
int32:min_value = -2147483648;
int32:max_value = 2147483647;
```

### Literal Suffix

```aria
int32:large = 1000000i32;  // Explicit int32 literal
```

### Common Use Cases

1. **Default integer**: Most general-purpose integer operations (±2.1B sufficient for 99% of cases)
2. **Database keys**: Primary keys, row IDs (2.1 billion records)
3. **Counters**: Loop indices, event counts, statistics
4. **File sizes**: Files up to 2GB (int32 bytes)
5. **Array indices**: Arrays up to 2.1 billion elements
6. **Unix timestamps**: Seconds since epoch (until Year 2038!)
7. **C interop**: Matching C's `int32_t`, `int` on 32/64-bit platforms

### Example: Database Record IDs

```aria
struct:UserRecord = {
    int32:user_id;       // Primary key (2.1 billion users max)
    string:username;
    string:email;
    int32:created_at;    // Unix timestamp (seconds since 1970)
};

UserRecord:user = {
    user_id: 12345678i32,
    username: "alice",
    email: "alice@example.com",
    created_at: 1739318400i32  // February 12, 2025
};

// Query by ID
func:find_user = (user_id: int32) -> ?UserRecord {
    // Database lookup
    return database_query("SELECT * FROM users WHERE user_id = ?", user_id);
}
```

### Production Scale Examples

```aria
// E-commerce inventory (2.1 billion SKUs max)
int32:sku = 987654321i32;

// Event processing (2.1 billion events)
int32:event_count = 0i32;

till event_stream.has_more() loop
    Event:event = event_stream.read();
    event_count += 1i32;
    
    if (event_count < 0i32) {
        // Wrapped! (overflow detection)
        stderr.write("Event count overflow!\n");
        break;
    }
end

// Geographic coordinates (scaled integers)
int32:latitude = 37774929i32;   // 37.774929° (scaled by 1e6)
int32:longitude = -122419416i32; // -122.419416° (scaled by 1e6)
```

---

## int64: 64-bit Signed Integer

### Range

**Minimum**: -9,223,372,036,854,775,808 (0x8000000000000000, -2^63)  
**Maximum**: +9,223,372,036,854,775,807 (0x7FFFFFFFFFFFFFFF, 2^63 - 1)  
**Total values**: 18,446,744,073,709,551,616 (2^64)

**Approximately**: ±9.2 quintillion (9.2 × 10^18)

**Asymmetric**: One more negative value (abs(INT64_MIN) has no int64 representation!)

### Declaration

```aria
int64:huge = 9000000000000000000;  // 9 quintillion
int64:timestamp_ns = 1739318400000000000;  // Nanoseconds since epoch
int64:min_value = -9223372036854775808;
int64:max_value = 9223372036854775807;
```

### Literal Suffix

```aria
int64:value = 1000000000000i64;  // Explicit int64 literal (1 trillion)
```

### Common Use Cases

1. **High-precision timestamps**: Nanosecond Unix time (good until year 2262)
2. **Large file sizes**: Files up to 8 exabytes (8,192 petabytes)
3. **Memory addresses**: 64-bit pointers, virtual memory offsets
4. **Large datasets**: Genomic data (billions of base pairs)
5. **Financial**: High-value transactions (±$92 quintillion)
6. **Astronomical**: Large counts (stars in galaxy, particles in universe)
7. **Cryptography**: Large integers for key generation
8. **C interop**: Matching C's `int64_t`, `long long`

### Example: Nanosecond Timestamps

```aria
// High-precision event timing
struct:HighResEvent = {
    int64:timestamp_ns;   // Nanoseconds since Unix epoch
    string:event_type;
    string:data;
};

func:get_current_time_ns = () -> int64 {
    // Get nanoseconds since January 1, 1970
    return system_time_nanoseconds();
}

HighResEvent:event = {
    timestamp_ns: get_current_time_ns(),
    event_type: "user_login",
    data: "alice"
};

// Compute elapsed time in nanoseconds
int64:start = get_current_time_ns();
perform_operation();
int64:end = get_current_time_ns();
int64:elapsed_ns = end - start;

log.write("Operation took ");
elapsed_ns.write_to(log);
log.write(" nanoseconds\n");
```

### Extreme Scale Examples

```aria
// Genomic data: 3 billion genomes × 3.2 billion base pairs
int64:total_base_pairs = 9600000000000i64;  // 9.6 trillion

// Financial: Global derivatives market
int64:notional_value_cents = 920000000000000000i64;  // $9.2 quadrillion

// Astronomical: Stars in Milky Way
int64:star_count = 200000000000i64;  // 200 billion stars

// Particle physics: Particles in human body
int64:particle_count = 7000000000000000000000000000i64;  // 7 × 10^27 (octillion)
// ⚠️ Wait, that's too big! int64 max is ~9.2 quintillion (10^18)
// Need larger type for 10^27!
```

---

## vs TBB Types (Critical Differences)

| Feature | Standard int32/int64 | TBB tbb32/tbb64 |
|---------|----------------------|-----------------|
| **Range** | Asymmetric (int32: -2.15B to +2.15B) | Symmetric (tbb32: -2.15B to +2.15B) |
| **Overflow** | Wraps around (MAX + 1 = MIN) | Returns ERR sentinel |
| **Underflow** | Wraps around (MIN - 1 = MAX) | Returns ERR sentinel |
| **Division by zero** | Panics (undefined behavior) | Returns ERR sentinel |
| **abs(MIN)** | Panics (undefined behavior) | Returns ERR sentinel |
| **Error checking** | Manual (check before/after) | Automatic (sticky ERR) |
| **Performance** | Zero overhead | ~0-3% overhead |
| **C interop** | Direct (same representation) | Requires conversion |
| **Year 2038** | Silent wraparound to 1901! | **ERR detection!** |

### Year 2038 Problem Example

```aria
// Standard int32 - WRAPS (disaster!)
int32:timestamp = 2147483647i32;  // January 19, 2038, 03:14:07 UTC (MAX)
timestamp += 1i32;
// Result: -2147483648 (December 13, 1901!) ⚠️ SILENT WRAPAROUND

// TBB tbb32 - DETECTS ERROR
tbb32:safe_timestamp = 2147483647;
safe_timestamp += 1;
// Result: ERR (overflow detected, program can HALT)

if (safe_timestamp == ERR) {
    stderr.write("YEAR 2038 OVERFLOW DETECTED!\n");
    stderr.write("SYSTEM MUST MIGRATE TO 64-BIT TIMESTAMPS!\n");
    panic("Cannot continue with overflowed timestamp");
}
```

**Key insight**: TBB types **save your production system** from the Year 2038 bug by detecting the overflow instead of silently wrapping to 1901!

---

## When to Use Standard Ints vs TBB

### Use Standard Ints (int32/int64) When:

1. **Performance critical**: Every cycle counts, zero overhead required
2. **C interoperability**: FFI with C libraries, binary protocols
3. **Bit manipulation**: Hashing, cryptography, compression (wrapping intentional)
4. **Large arrays**: Billions of elements, memory efficiency critical
5. **Overflow impossible**: Range constraints mathematically guarantee no overflow
6. **Legacy compatibility**: Matching existing systems using int32/int64
7. **64-bit timestamps**: int64 nanoseconds good until year 2262

### Use TBB Types (tbb32/tbb64) When:

1. **Safety-critical**: Healthcare, finance, aviation (overflow = catastrophe)
2. **Year 2038 concerns**: 32-bit timestamps that might overflow
3. **Error propagation**: Sticky errors through computation chains
4. **Accumulation**: Long-running sums, counters (overflow risk)
5. **Database transactions**: Financial calculations (overflow detection required)
6. **Symmetric ranges**: Need abs() to always work
7. **Nikola consciousness**: Simulations where overflow = state corruption

### Quick Decision Matrix

```aria
// Web server request counter - USE TBB (detect overflow!)
tbb64:request_count = 0;
request_count += 1;
if (request_count == ERR) {
    log_error("Request counter overflow!");
}

// Memory address offset - USE STANDARD (uint64 better, but int64 for signed offsets)
int64:offset = calculate_offset(base_address);

// Hash function - USE STANDARD (wrapping intentional)
int32:hash = compute_hash(data) % table_size;

// Financial transaction - USE TBB (safety-critical!)
tbb64:transaction_cents = account_balance + transaction_amount;
if (transaction_cents == ERR) {
    rollback_transaction("Overflow!");
}

// Genomic position - USE STANDARD (3.2B base pairs, int64 sufficient, no arithmetic)
int64:genome_position = 1234567890i64;

// Unix timestamp (32-bit) - **USE TBB** (Year 2038!)
tbb32:timestamp = get_current_unix_timestamp();
```

---

## Overflow Wrapping Behavior

### Defined in Aria (NOT Undefined Behavior!)

Unlike C/C++ where signed overflow is undefined, Aria **defines** overflow as two's complement wrapping.

```aria
// int32 overflow
int32:max = 2147483647i32;
int32:wrapped = max + 1i32;
// wrapped = -2147483648 (wraps to minimum)

// int64 overflow
int64:max64 = 9223372036854775807i64;
int64:wrapped64 = max64 + 1i64;
// wrapped64 = -9223372036854775808 (wraps to minimum)
```

**Why defined?**: Predictable behavior for modular arithmetic, cryptography, hashing.

### Silent Wraparound Dangers

```aria
// DANGEROUS: Accumulation without overflow check
int32:total_bytes = 0i32;

till files.length loop:i
    int32:file_size = files[i].get_size();
    total_bytes += file_size;  // ⚠️ Can wrap silently!
end

// If total > 2.1GB, total_bytes wraps negative!
// Causes: disk space calculation errors, buffer allocation failures
```

**Safe version with TBB**:

```aria
tbb64:total_bytes = 0;

till files.length loop:i
    tbb64:file_size = files[i].get_size();
    total_bytes += file_size;
    
    if (total_bytes == ERR) {
        stderr.write("Total file size overflow!\n");
        break;
    }
end
```

---

## abs() Undefined Behavior with Minimum Values

### The abs(INT_MIN) Problem Revisited

```aria
// int32
int32:min32 = -2147483648i32;
int32:result32 = abs(min32);  // ⚠️ PANICS! (abs = 2147483648, doesn't fit)

// int64
int64:min64 = -9223372036854775808i64;
int64:result64 = abs(min64);  // ⚠️ PANICS! (abs = 9223372036854775808, doesn't fit)
```

**Why panic?**: Minimum has no positive representation in asymmetric range.

### Safe Alternatives

**Option 1**: Use TBB (symmetric ranges)

```aria
// tbb32 has symmetric range: -2,147,483,647 to +2,147,483,647
tbb32:value = -2147483647;
tbb32:result = abs(value);  // result = 2147483647 (fits!)

// Note: tbb32 minimum is -2,147,483,647, not -2,147,483,648!
```

**Option 2**: Check before calling abs()

```aria
int32:value = get_signed_value();

if (value == -2147483648i32) {
    stderr.write("Cannot take abs of INT32_MIN\n");
    // Handle special case
} else {
    int32:result = abs(value);  // Safe
}
```

**Option 3**: Widen to larger type

```aria
int32:small = -2147483648i32;
int64:wide = int64(small);  // -2147483648 fits in int64
int64:result = abs(wide);   // result = 2147483648 (fits in int64!)
```

---

## Two's Complement Representation

### Bit Representation (int32)

```
+2,147,483,647 = 0x7FFFFFFF = 0b01111111111111111111111111111111
+1             = 0x00000001 = 0b00000000000000000000000000000001
 0             = 0x00000000 = 0b00000000000000000000000000000000
-1             = 0xFFFFFFFF = 0b11111111111111111111111111111111
-2,147,483,648 = 0x80000000 = 0b10000000000000000000000000000000
```

### Bit Representation (int64)

```
+9,223,372,036,854,775,807 = 0x7FFFFFFFFFFFFFFF
 0                          = 0x0000000000000000
-1                          = 0xFFFFFFFFFFFFFFFF
-9,223,372,036,854,775,808 = 0x8000000000000000
```

### Sign Bit

**Most significant bit (MSB)** determines sign:
- `0` = positive or zero
- `1` = negative

For int32, bit 31 is the sign bit.  
For int64, bit 63 is the sign bit.

---

## Arithmetic Operations

### Safe Addition with Overflow Detection

```aria
func:safe_add_i32 = (a: int32, b: int32) -> Result<int32, string> {
    // Positive overflow: both positive, sum negative
    if (a > 0i32 && b > 0i32 && (a + b) < 0i32) {
        fail("Positive overflow");
    }
    
    // Negative underflow: both negative, sum positive
    if (a < 0i32 && b < 0i32 && (a + b) > 0i32) {
        fail("Negative underflow");
    }
    
    pass(a + b);
}

// Or just use TBB (automatic)
tbb32:a = 2000000000;
tbb32:b = 200000000;
tbb32:sum = a + b;  // ERR (overflow detected)
```

### Multiplication Overflow

```aria
// int32 multiplication can overflow easily
int32:a = 100000i32;
int32:b = 30000i32;
int32:product = a * b;  // 3,000,000,000... overflow! wraps to negative

// Safe multiplication: widen, check, narrow
int64:wide_a = int64(a);
int64:wide_b = int64(b);
int64:wide_product = wide_a * wide_b;  // 3,000,000,000 (fits in int64)

if (wide_product > 2147483647i64 || wide_product < -2147483648i64) {
    stderr.write("Product doesn't fit in int32!\n");
} else {
    int32:result = int32(wide_product);  // Safe narrow
}
```

---

## The Year 2038 Problem

### What is it?

Unix timestamps stored as **int32 seconds** since January 1, 1970 00:00:00 UTC will overflow on:

**January 19, 2038, 03:14:07 UTC**

At this moment, 2,147,483,647 seconds will have elapsed, hitting **INT32_MAX**.

One second later: **INT32_MIN** (-2,147,483,648), interpreted as **December 13, 1901**!

### The Disaster Scenario

```aria
// System using int32 timestamps (legacy code)
int32:current_time = get_unix_timestamp();  // Returns seconds since 1970

// January 19, 2038, 03:14:07 UTC
current_time = 2147483647i32;  // One second before overflow

// One second later...
current_time = get_unix_timestamp();
// current_time = -2147483648 (wrapped to minimum!)

// System interprets as December 13, 1901
// Result:
// - All "future" events appear to be in the past
// - Certificates expire (current time < expiration time check fails)
// - File modification times corrupted
// - Database queries break (WHERE timestamp > ...)
// - Financial transactions rejected
// - Authentication systems fail
```

### TBB to the Rescue!

```aria
// Use TBB tbb32 to DETECT overflow instead of silent wraparound
tbb32:timestamp = 2147483647;  // January 19, 2038, 03:14:07 UTC

timestamp += 1;  // One second later

if (timestamp == ERR) {
    // ERROR DETECTED! System can HALT instead of corrupting data!
    stderr.write("⚠️ YEAR 2038 OVERFLOW DETECTED!\n");
    stderr.write("CRITICAL: Timestamp overflow imminent!\n");
    stderr.write("ACTION REQUIRED: Migrate to 64-bit timestamps!\n");
    
    // Trigger emergency shutdown, alert administrators
    panic("Cannot continue with overflowed timestamp");
}
```

**TBB benefit**: System **stops** instead of running with corrupted time data!

### Proper Fix: Migrate to int64

```aria
// Use int64 for timestamps (nanoseconds or microseconds)
int64:timestamp_ns = get_unix_timestamp_nanoseconds();

// Nanoseconds since 1970, int64 good until year 2262
// 9,223,372,036,854,775,807 nanoseconds = 292 years
```

---

## 64-bit Timestamps and Beyond

### Nanosecond Timestamps (Recommended)

```aria
// int64 nanoseconds since Unix epoch
int64:now_ns = 1739318400000000000i64;  // Feb 12, 2025 (nanoseconds)

// Good until: ~2262 (292 years from 1970)
// Range: 1970 to 2262 (sufficient for all practical purposes)
```

### Microsecond Timestamps

```aria
// int64 microseconds since Unix epoch
int64:now_us = 1739318400000000i64;  // Feb 12, 2025 (microseconds)

// Good until: ~292,277 AD (290,000 years!)
```

### Millisecond Timestamps (JavaScript-style)

```aria
// int64 milliseconds since Unix epoch
int64:now_ms = 1739318400000i64;  // Feb 12, 2025 (milliseconds)

// Good until: ~292,277,026 AD (290 million years!)
```

**Recommendation**: Use **int64 nanoseconds** for high-precision timestamps (good until 2262).

---

## C Interoperability

### Direct ABI Compatibility

| Aria Type | C Type | Size | Range |
|-----------|--------|------|-------|
| int32 | `int32_t` | 4 bytes | ±2.1 billion |
| int64 | `int64_t` | 8 bytes | ±9.2 quintillion |

### FFI Example

```aria
// C library functions
extern "C" {
    func:c_file_size = int64(pathname: int8@);
    func:c_timestamp = int32();
    func:c_process_id = int32();
}

// Aria usage
int64:file_size = c_file_size("/var/log/system.log");

log.write("File size: ");
file_size.write_to(log);
log.write(" bytes\n");

int32:current_time = c_timestamp();
int32:pid = c_process_id();
```

**No conversion needed**: Aria int32/int64 are ABI-compatible with C.

---

## Performance Characteristics

### Zero Overhead

```aria
int32:a = 1000000i32;
int32:b = 2000000i32;
int32:sum = a + b;
```

**Generated assembly** (x86-64):
```asm
mov eax, 1000000    ; Load 1000000 into EAX
add eax, 2000000    ; Add 2000000 to EAX
; Result in EAX (one instruction!)
```

### int32 vs int64 Performance

**32-bit arithmetic**: Single instruction on all CPUs  
**64-bit arithmetic**: Single instruction on 64-bit CPUs, two instructions on 32-bit CPUs

**Modern systems (64-bit)**: int64 performance identical to int32.

### Memory Efficiency

```aria
// 1 billion int32 values
int32[]:medium = aria.alloc<int32>(1000000000);
// Memory: 4 GB

// 1 billion int64 values
int64[]:large = aria.alloc<int64>(1000000000);
// Memory: 8 GB (2× larger)
```

**Trade-off**: int64 uses 2× memory but avoids overflow for large values.

---

## Common Patterns

### Range-Checked Narrowing (int64 → int32)

```aria
func:narrow_to_i32 = (value: int64) -> ?int32 {
    if (value < -2147483648i64 || value > 2147483647i64) {
        return NIL;  // Out of range
    }
    
    return int32(value);  // Safe cast
}

// Usage
int64:large = 5000000000i64;  // 5 billion
?int32:small = narrow_to_i32(large);

if (small == NIL) {
    stderr.write("Value too large for int32\n");
}
```

### Overflow-Safe Accumulation

```aria
// Unsafe: int32 can overflow
int32:sum = 0i32;

till 1000000 loop:i
    sum += 10000i32;  // Total: 10 billion (overflow!)
end

// Safe: Use int64 for accumulation
int64:safe_sum = 0i64;

till 1000000 loop:i
    safe_sum += 10000i64;  // Total: 10 billion (fits!)
end
```

### Timestamp Conversion

```aria
// Convert int32 seconds to int64 nanoseconds
int32:timestamp_s = 1739318400i32;  // Feb 12, 2025
int64:timestamp_ns = int64(timestamp_s) * 1000000000i64;

// Convert int64 nanoseconds to int32 seconds
int64:ns = 1739318400000000000i64;
int32:seconds = int32(ns / 1000000000i64);
```

---

## Anti-Patterns

### ❌ Using int32 for Modern Timestamps

```aria
// WRONG: int32 timestamp (Year 2038 problem!)
int32:timestamp = get_unix_timestamp();  // ❌ Overflows in 2038!

// RIGHT: int64 timestamp (good until 2262)
int64:timestamp_ns = get_unix_timestamp_nanoseconds();  // ✅ Safe
```

### ❌ Ignoring Overflow in Financial Calculations

```aria
// WRONG: int32 for money (can overflow!)
int32:account_balance_cents = 1000000000i32;  // $10 million
account_balance_cents += 1500000000i32;  // Add $15 million
// Result: -1794967296 (overflow! balance is now negative!)

// RIGHT: Use TBB or int64
tbb64:safe_balance_cents = 1000000000;
safe_balance_cents += 1500000000;
if (safe_balance_cents == ERR) {
    stderr.write("Transaction overflow!\n");
    rollback();
}
```

### ❌ Using abs(INT_MIN)

```aria
// WRONG: abs() on minimum value
int32:value = -2147483648i32;
int32:absolute = abs(value);  // ❌ PANICS!

// RIGHT: Check first or use TBB
if (value == -2147483648i32) {
    stderr.write("Cannot take abs of INT32_MIN\n");
} else {
    int32:absolute = abs(value);
}
```

---

## Migration from C/C++

### Undefined Behavior → Defined

**C**: Signed overflow is undefined
```c
int32_t a = INT32_MAX;
a += 1;  // UNDEFINED BEHAVIOR
```

**Aria**: Signed overflow wraps (defined)
```aria
int32:a = 2147483647i32;
a += 1i32;  // a = -2147483648 (defined wrapping)
```

### Explicit Type Suffixes

**C**: Implicit conversions
```c
int32_t x = 1000000;  // Literal is int, implicitly converted
```

**Aria**: Explicit suffixes required
```aria
int32:x = 1000000i32;  // Must specify i32
```

### Year 2038 Migration

**C legacy code**:
```c
time_t timestamp = time(NULL);  // time_t is int32 on many systems
```

**Aria modern code**:
```aria
int64:timestamp_ns = get_unix_timestamp_nanoseconds();  // int64, safe until 2262
```

---

## Related Concepts

### Other Integer Types

- **int8, int16**: Smaller standard signed integers
- **uint32, uint64**: Unsigned integers (0 to 4.3B / 18.4 quintillion)
- **tbb32, tbb64**: Symmetric-range error-propagating integers
- **int1024, int2048, int4096**: Large cryptographic integers

### Special Values

- **NIL**: Optional types (not applicable to int32/int64)
- **ERR**: TBB error sentinel (not applicable to standard ints)
- **NULL**: Pointer sentinel (not applicable to value types)

### Related Operations

- **Bit manipulation**: Shifts, masks, flags
- **Overflow detection**: Manual checking or TBB automatic
- **Widening**: int32 → int64 (always safe)
- **Narrowing**: int64 → int32 (check range!)

---

## Summary

**int32** and **int64** are Aria's production workhorse integers:

✅ **Production scale**: int32 (±2.1B), int64 (±9.2 quintillion)  
✅ **Wrapping overflow**: Defined behavior (not UB like C!)  
✅ **Zero overhead**: Native CPU instructions  
✅ **C compatible**: Direct FFI, same ABI  
✅ **Industry standard**: Two's complement on all platforms  

⚠️ **Year 2038 problem**: int32 timestamps overflow in 2038! Use int64 or TBB  
⚠️ **Overflow wraps silently**: Use TBB if overflow is an error  
⚠️ **abs(INT_MIN) panics**: Use TBB for symmetric ranges  

**Use int32 for**:
- General-purpose integers (±2.1B sufficient)
- Database keys, counters, array indices
- C interoperability
- Performance-critical code

**Use int64 for**:
- Modern timestamps (nanoseconds, good until 2262)
- Large file sizes (exabytes)
- Memory addresses, offsets
- Genomic data, astronomical counts

**Use tbb32/tbb64 for**:
- Safety-critical systems
- **Year 2038 overflow detection**
- Financial calculations
- Error propagation
- Symmetric ranges

---

**Next**: [uint8, uint16 (Unsigned Small Integers)](uint8_uint16.md)  
**See Also**: [int8/int16](int8_int16.md), [tbb32](tbb32.md), [tbb64](tbb64.md)

---

*Aria Language Project - Production Integers with Year 2038 Awareness*  
*February 12, 2026 - Establishing timestamped prior art on int32/int64 semantics*
