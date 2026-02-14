# Aria Programming Guide

Comprehensive type system documentation for the Aria programming language.

## Purpose

This guide provides in-depth documentation for Aria's type system, covering all built-in types with practical examples, best practices, common patterns, and anti-patterns.

## Structure

### types/
Complete type system documentation organized by category.

## Current Status

**Total Files**: 25  
**Total Lines**: ~27,265  
**Sessions Completed**: 32  
**Last Updated**: February 13, 2026

## Type Categories

### Fundamental Types

**Session 15 (February 12, 2026)** - Boolean and character types:
- **bool.md** (784 lines) - Boolean logical type
  - Logical true/false semantics (vs int1 numeric)
  - Operators: AND (&&), OR (||), NOT (!), XOR (^^)
  - Short-circuit evaluation guaranteed
  - Explicit conversions only (no implicit)
  - Common patterns: Flags, guards, predicates, state machines
  - Anti-patterns: Integer as bool, magic parameters, redundant comparisons
  - Best practices: Question-form naming (is_*, has_*, can_*), early returns

- **char.md** (697 lines) - Character type
  - UTF-8 encoded Unicode code points (U+0000 to U+10FFFF)
  - Variable length: 1-4 bytes (ASCII=1, Extended=2, CJK=3, Emoji=4)
  - vs byte: Character vs binary semantics
  - Classification: is_alpha(), is_digit(), is_whitespace(), etc.
  - Case conversion: to_uppercase(), to_lowercase()
  - Common patterns: Filtering, counting, replacement
  - Anti-patterns: Using byte for characters, assuming ASCII
  - Best practices: UTF-8 awareness, built-in classification

**Session 16 (February 13, 2026)** - String type:
- **string.md** (935 lines) - UTF-8 string type
  - Immutable UTF-8 encoded text sequences
  - Byte length vs character count distinction
  - String literals, raw strings (backtick), interpolation (${})
  - Operations: search, split, join, trim, case conversion
  - Performance: concat_n() for bulk operations
  - C interop: to_cstr(), from_cstr()
  - Anti-patterns: Repeated concatenation in loops, byte/char confusion
  - Best practices: Interpolation, raw strings for paths/regex

**Session 17 (February 13, 2026)** - Default integer type:
- **i32.md** (989 lines) - 32-bit signed integer
  - Default integer type (±2.1 billion)
  - Two's complement, asymmetric range
  - Wrapping overflow (defined, not UB)
  - vs i8/i16/i64: Size selection
  - vs u32: Signed vs unsigned semantics
  - vs tbb32: Wrapping vs error detection (Year 2038!)
  - Arithmetic, bitwise, comparisons
  - Anti-patterns: Year 2038 timestamps, overflow bugs, using int as bool
  - Best practices: Use i64 for timestamps, tbb32 for safety-critical

**Session 18 (February 13, 2026)** - Large integer type:
- **i64.md** (991 lines) - 64-bit signed integer
  - Large values beyond ±2.1B (±9.2 quintillion)
  - vs i32: When to use which (decision matrix)
  - vs u64: Signed vs unsigned for large values
  - Nanosecond timestamps (good until year 2262!)
  - Large file sizes (>2GB, up to 8 exabytes)
  - Memory addresses, offsets, big data
  - Performance: Same as i32 on 64-bit CPUs
  - Anti-patterns: Using i64 everywhere (memory waste), not for timestamps
  - Best practices: i64 for timestamps/big files, i32 for small ranges

**Session 19 (February 13, 2026)** - Default floating-point type:
- **f64.md** (915 lines) - 64-bit floating point
  - IEEE 754 double precision (~15-16 decimal digits)
  - vs f32: Precision vs memory trade-off
  - vs i64: Floating vs integer semantics
  - Special values: NaN, Infinity, -Infinity, -0.0
  - Precision limits: Not all large integers fit exactly
  - Math functions: sqrt, trig, exp, log
  - NaN comparisons: NaN != NaN (critical gotcha!)
  - Anti-patterns: Money, equality comparisons, loop counters
  - Best practices: Epsilon comparisons, measurements/science, check special values

**Session 20 (February 13, 2026)** - Single-precision floating-point type:
- **f32.md** (1373 lines) - 32-bit floating point
  - IEEE 754 single precision (~7 decimal digits)
  - vs f64: Performance vs precision (2× SIMD throughput, half memory)
  - vs i32: Floating vs integer (precision limit at ±16,777,216)
  - Special values: NaN, ±Infinity, ±0.0 (same as f64)
  - SIMD vectorization: Process 4/8/16 values in parallel
  - GPU optimization: Native precision for graphics/ML
  - Integer precision limit: Consecutive integers only up to 2²⁴
  - Anti-patterns: Money, large IDs, equality, small+large accumulation
  - Best practices: Graphics/audio/physics, epsilon comparisons, validate NaN/Infinity

**Session 21 (February 13, 2026)** - Unsigned 32-bit integer type:
- **u32.md** (1131 lines) - 32-bit unsigned integer
  - Range: 0 to 4,294,967,295 (0 to ~4.3 billion)
  - vs i32: Signed vs unsigned (2× positive range, no negatives, underflow danger)
  - vs u64: Size selection (4.3B vs 18.4 quintillion)
  - Underflow wrapping: 0 - 1 = MAX (critical gotcha!)
  - Overflow wrapping: MAX + 1 = 0 (modular arithmetic)
  - Use cases: Array indices, file sizes <4GB, counts, hashes, IPv4
  - Decrementing loops: Infinite if not careful (i >= 0 always true)
  - Anti-patterns: Negative values, subtraction without checks, mixing signed/unsigned
  - Best practices: Bounds checking, explicit conversions, u64 for accumulation

**Session 22 (February 13, 2026)** - Unsigned 64-bit integer type:
- **u64.md** (1091 lines) - 64-bit unsigned integer
  - Range: 0 to 18,446,744,073,709,551,615 (0 to ~18.4 quintillion)
  - vs u32: Modern file sizes (>4GB standard), memory addresses, timestamps
  - vs i64: Unsigned vs signed (2× positive range, no negatives, same underflow danger)
  - Modern default for file sizes, memory addresses, high-resolution time
  - Underflow magnified: 0 - 1 = 18.4 quintillion (even more dangerous!)
  - Use cases: Files >4GB, nanosecond timestamps (year 2554), large counts, memory addresses
  - Performance: Native on 64-bit CPUs (same as u32), half SIMD width
  - Anti-patterns: Same as u32 but magnified scale, mixing with smaller types
  - Best practices: u64 for modern files, check underflow, i64 for differences

**Session 23 (February 13, 2026)** - Unsigned 8-bit byte type:
- **u8.md** (1121 lines) - 8-bit unsigned integer (byte)
  - Range: 0 to 255 (the fundamental byte type)
  - vs char: Binary data vs Unicode text (semantic distinction)
  - vs i8: Unsigned vs signed bytes (2× positive range, no negatives)
  - vs u16/u32: Size selection (smallest type, best memory/cache)
  - Universal binary unit: Files, networks, image pixels, protocols
  - Underflow danger at small scale: 0 - 1 = 255
  - Use cases: Raw bytes, RGB pixels (0-255), bit flags, file I/O, checksums
  - SIMD: Best vectorization (16/32/64 values at once)
  - Anti-patterns: Text data (use char!), decrementing loops, underflow bugs
  - Best practices: Binary data only, saturating arithmetic for images, check underflow

**Session 24 (February 13, 2026)** - Unsigned 16-bit medium integer type:
- **u16.md** (1167 lines) - 16-bit unsigned integer
  - Range: 0 to 65,535 (network port standard)
  - vs u8: Medium vs small (256× larger, 2× memory, ports/dimensions >255)
  - vs i16: Unsigned vs signed (2× positive range, full port range 0-65535)
  - vs u32: Medium vs large (save memory for values ≤65K)
  - Standard for network ports (TCP/UDP 0-65535)
  - Unicode BMP: Basic Multilingual Plane (U+0000 to U+FFFF)
  - Use cases: Ports, image dimensions (≤65K), checksums (CRC-16), device IDs
  - Underflow at medium scale: 0 - 1 = 65,535
  - Anti-patterns: RGB values (use u8!), large counts (use u32!), underflow loops
  - Best practices: Network ports, widen before multiplication, check underflow

**Session 25 (February 13, 2026)** - Signed 8-bit byte type:
- **i8.md** (1456 lines) - 8-bit signed integer
  - Range: -128 to +127 (signed, asymmetric)
  - vs u8: Can be negative vs always positive (half the positive range)
  - vs i16: Smaller (1 byte vs 2 bytes), better cache/memory
  - vs i32: Much smaller, use when values guaranteed within -128 to +127
  - Two's complement representation (MSB is sign bit)
  - Wrapping overflow: 127 + 1 = -128 (defined behavior)
  - abs(-128) panics: 128 doesn't fit in i8 (asymmetric range)
  - Use cases: Temperature, offsets, deltas, protocol headers
  - Anti-patterns: Using when unsigned works, ignoring overflow, unchecked abs()
  - Best practices: Prefer u8 when never negative, range checks, widen for abs()

**Session 26 (February 13, 2026)** - Signed 16-bit medium integer type:
- **i16.md** (1517 lines) - 16-bit signed integer
  - Range: -32,768 to +32,767 (audio/coordinate standard)
  - vs i8: Medium vs small (256× more values, 2× memory)
  - vs u16: Signed vs unsigned (can be negative, half positive range)
  - vs i32: Medium vs large (2 bytes vs 4 bytes, save memory)
  - Audio standard: 16-bit PCM (CD quality, -32K to +32K)
  - abs(-32768) panics: 32,768 doesn't fit (asymmetric range)
  - Use cases: Audio samples, screen coordinates, sensors, geography
  - Multiplication gotcha: Widen before multiply (20K × 2 overflows!)
  - Anti-patterns: Audio multiply without widening, large loops, unchecked abs()
  - Best practices: Widen before multiply, clip audio, i32 for accumulation

**Session 29 (February 13, 2026)** - Twisted Balanced Binary (TBB) integer family:
- **tbb8.md** (1217 lines) - 8-bit symmetric integer with ERR sentinel
  - Range: -127 to +127 (symmetric!), -128 = ERR sentinel
  - Sticky error propagation: ERR + anything = ERR (automatic)
  - vs i8: Symmetric vs asymmetric, error detection vs wrapping
  - Eliminates asymmetry bugs: abs(-127) always works, neg(-127) always works
  - ARIA-018 constraint: No implicit widening (ERR "heals" during sign-extension!)
  - Must use tbb_widen<T>() to preserve sentinel mapping across widths
  - Foundation for frac8 (exact rationals): struct {tbb8:whole; tbb8:num; tbb8:denom}
  - Use cases: Sensors, financial calc, audio, state machines, safety-critical
  - Anti-patterns: Using -128 as value, implicit widening, ignoring ERR
  - Best practices: Check ERR explicitly, use for safety-critical, tbb_widen<T>()

- **tbb16.md** (952 lines) - 16-bit symmetric integer with ERR sentinel
  - Range: -32,767 to +32,767 (symmetric!), -32768 = ERR sentinel
  - vs tbb8: Wider range (256× values), 2× memory
  - vs i16: Symmetric vs asymmetric, error detection vs wrapping
  - Component of tfp32: Exponent + mantissa for deterministic floating point
  - Foundation for frac16: Mixed-fraction exact rationals
  - Use cases: 16-bit audio with overflow detection, medium sensors, frac16/tfp32
  - Endianness matters: 2-byte value, network byte order for transmission
  - Anti-patterns: Using -32768 as value, implicit widening, forget endianness
  - Best practices: tbb_widen<T>() for widening, ERR checks, network byte order

- **tbb32.md** (1011 lines) - 32-bit symmetric integer with ERR sentinel
  - Range: -2,147,483,647 to +2,147,483,647 (symmetric!), -2147483648 = ERR sentinel
  - General-purpose TBB type: Default choice for most applications
  - vs tbb16: Wider range (65K× values), 2× memory
  - vs i32: Symmetric vs asymmetric, error detection vs wrapping
  - Foundation for frac32: Most common exact rational type
  - Use cases: Crypto (satoshis), physics (integer mm/μm), counters, financial, frac32
  - Year 2038 warning: Unix timestamps still overflow! (Use tbb64 for timestamps)
  - Anti-patterns: Using -2147483648 as value, timestamps past 2038, implicit widening
  - Best practices: Default TBB choice, check ERR for critical paths, tbb64 for timestamps

- **tbb64.md** (980 lines) - 64-bit symmetric integer with ERR sentinel
  - Range: ±9,223,372,036,854,775,807 (symmetric!), MIN_INT64 = ERR sentinel
  - Maximum capacity TBB type: Widest range with error propagation
  - vs tbb32: Billion× wider range, 2× memory
  - vs i64: Symmetric vs asymmetric, error detection vs wrapping
  - Solves Y2038: Timestamps in milliseconds (good for ~292 million years!)
  - Foundation for frac64, tfp64 mantissa (48-bit)
  - Use cases: Timestamps (ms/μs), large crypto (BTC satoshis), high-precision frac64
  - Ethereum warning: 1 ETH = 10^18 wei, tbb64 max ≈ 9.2×10^18 (only ~9 ETH in wei!)
  - Anti-patterns: Ethereum wei for large amounts, using MIN_INT64 as value
  - Best practices: Timestamps in ms, check Ethereum wei range, use for frac64

**Session 30 (February 13, 2026)** - Exact Rational (frac) integer family:
- **frac32.md** (1064 lines) - 32-bit exact rational numbers (general-purpose)
  - Structure: struct {tbb32:whole; tbb32:num; tbb32:denom} (12 bytes)
  - **Solves "0.1 + 0.2 = 0.3" problem**: Exact rationals (not 0.30000...4!)
  - Range: ±2.1 billion whole, denominators up to 2.1 billion
  - Canonical form: Reduced to lowest terms, proper fraction, sign in whole/num
  - Built on TBB foundation: Sticky ERR propagation from components
  - Use cases: Financial (legal compliance), music (exact harmonics), recipes (exact scaling), science (exact stoichiometry)
  - vs f32: Exact vs approximate, no drift, ~50-100× slower but correct
  - Anti-patterns: Using f64 for money, ignoring ERR, comparing to floats
  - Best practices: Use for exact calculations, check ERR, document precision

- **frac64.md** (711 lines) - 64-bit exact rational numbers (high-precision)
  - Structure: struct {tbb64:whole; tbb64:num; tbb64:denom} (24 bytes)
  - Range: ±9.2 quintillion whole, denominators up to 9.2 quintillion
  - 4 million× larger than frac32 whole range!
  - Use cases: Astronomical (nanosecond timestamps, light-years), large finance (trillions, Bitcoin sub-satoshi), scientific constants
  - Year 2262-proof nanosecond timestamps
  - vs frac32: 4M× range, 2× memory, slower, for massive precision
  - Anti-patterns: Using when frac32 sufficient, graphics/real-time
  - Best practices: Use when frac32 insufficient, document precision limits

- **frac8.md** (637 lines) - 8-bit exact rational numbers (compact)
  - Structure: struct {tbb8:whole; tbb8:num; tbb8:denom} (3 bytes, 4-byte aligned)
  - Range: ±127 whole, denominators up to 127
  - **Cache efficiency**: 16 frac8 values per 64-byte cache line! (vs 5 frac32)
  - Use cases: Recipe fractions, embedded systems (minimal RAM), temperature (-127°C to +127°C), PWM duty cycles
  - Limitations: Small range, easy overflow (50 × 3 = 150 → ERR!)
  - vs frac32: 75% smaller (3 bytes vs 12), 4× cache utilization
  - Anti-patterns: Values > ±127, large calculations (overflow risk)
  - Best practices: Embedded systems, check range before operations, perfect for small exact fractions

- **frac16.md** (648 lines) - 16-bit exact rational numbers (medium-precision)
  - Structure: struct {tbb16:whole; tbb16:num; tbb16:denom} (6 bytes, 8-byte aligned)
  - Range: ±32,767 whole, denominators up to 32,767
  - **Cache efficiency**: 8 frac16 values per 64-byte cache line (2× frac32!)
  - Use cases: Fine percentages (basis points = 0.01%), audio sample timing (44.1-192 kHz), medium finance (up to ±$32K), screen coordinates
  - 256× larger than frac8, half size of frac32
  - vs frac32: Medium vs general (50% memory savings when ±32K sufficient)
  - Anti-patterns: Values > ±32K (use frac32), simple fractions (use frac8)
  - Best practices: Basis point financial, audio timing, check range before operations

**Session 31 (February 13, 2026)** - Twisted Floating Point (tfp) deterministic float family:
- **tfp32.md** (534 lines) - 32-bit deterministic floating point
  - Structure: tbb16 exponent + tbb16 mantissa (4 bytes)
  - **Solves IEEE 754 problems**: No -0 (unique zero), no NaN/Inf (unified ERR), cross-platform bit-exact
  - Precision: ~4-5 decimal digits (vs IEEE float's ~7)
  - Range: 2^(-32768) to 2^(+32767) (16-bit exponent, wider than IEEE!)
  - Software implementation: 10-50× slower but deterministic everywhere
  - Sticky ERR propagation: Division by zero, sqrt(-1), overflow all → ERR
  - Use cases: Physics simulations (replays), distributed consensus, game determinism, AI safety
  - vs frac32: Deterministic approximations vs exact rationals (can represent irrationals)
  - Anti-patterns: Speed-critical code (use flt32), high precision needed (use tfp64)
  - Best practices: Use for cross-platform reproducibility, check ERR, deterministic math

- **tfp64.md** (558 lines) - 64-bit high-precision deterministic floating point
  - Structure: tbb16 exponent + tbb48 mantissa (8 bytes)
  - Precision: ~14-15 decimal digits (vs IEEE double's ~16, lose ~1 digit)
  - Range: 2^(-32768) to 2^(+32767) (same as tfp32, wider than IEEE double!)
  - **Philosophy**: Lose 1 digit precision, gain perfect reproducibility
  - Cross-platform bit-exact: x86, ARM, RISC-V all produce identical results
  - Use cases: GPS (millimeter precision), orbital mechanics, scientific computing, AI training, long-term financial compounding
  - Taylor series transcendentals: sin/cos/sqrt bit-exact everywhere
  - vs frac64: Deterministic approximations vs exact rationals (fixed size, handles irrationals)
  - Anti-patterns: Speed-critical (use flt64), need exact (use frac64)
  - Best practices: High-precision determinism, distributed systems, verify with hashing

**Session 32 (February 13, 2026)** - Q128.128 deterministic fixed-point:
- **fix256.md** (596 lines) - 256-bit ultra-precise deterministic fixed-point
  - Structure: Q128.128 = 128-bit integer + 128-bit fractional (32 bytes, 4× 64-bit limbs)
  - **Precision**: 2^-128 ≈ 2.9×10^-39 (4 orders finer than Planck length!)
  - **Range**: ±2^127 ≈ ±1.7×10^38 (astronomical scale + sub-atomic precision)
  - **Zero drift**: Bit-exact after billions of operations (no accumulation errors)
  - ERR sentinel: Sticky error propagation (overflow, underflow, div-by-zero)
  - **Nightmare scenario prevention**: "Drift in consciousness substrate would destabilize subjective experience"
  - Use cases: AGI consciousness substrate (Nikola's physics engine), robotics (precision motion), safety-critical physics, quantum mechanics
  - GPU support: Bit-identical CPU/GPU results (3.32 GFLOPS on RTX 3090)
  - vs frac64: Ultra-precise approximations vs exact rationals (fixed scale, no denominator growth)
  - vs tfp64: Ultra-precise fixed-scale vs wide-range exponent (uniform precision)
  - Anti-patterns: Speed-critical (use flt64), wide range needed (use tfp64)
  - Best practices: Physics simulations, robotic control, long-running stability, consciousness substrate

### Advanced Types

**Session 27 (February 13, 2026)** - Quantum/speculative types:
- **quantum.md** (1147 lines) - Q3<T> and Q9<T> quantum superposition types
  - Superposition: Track two hypotheses (A and B) simultaneously with confidence
  - Q9<T>: Fine-grained confidence (nit -4 to +4, 9 levels)
  - Q3<T>: Binary confidence (trit -1, 0, +1, 3 levels)
  - Crystallization: Defer decision while accumulating evidence
  - Safety integration: c=0 requires ok() (prevents accidental use)
  - Q-functions: qor, qand, qxor, qnor, qconf, qnconf
  - Use cases: Fuzzing, speculative execution, AI reasoning, uncertainty propagation
  - Cognitive modeling: Evidence accumulation, gradient thinking (not binary)
  - Nikola AI: Native tongue for self-improvement loop (safe AI reasoning)
  - Anti-patterns: Ignoring confidence, using c=0 without ok(), no evidence tracking
  - Best practices: Use Q9 for complex decisions, define thresholds, document evidence

### Built-in Facilities

**Session 28 (February 13, 2026)** - Debug facility:
- **dbug.md** (1039 lines) - Built-in grouped debugging and assertions
  - Group-based debug output with compile/runtime filtering
  - dbug.print(group, msg_fmt, data) - Conditional debug printing
  - dbug.check(group, msg_fmt, data, condition, action?) - Assertions with context
  - Type-safe string interpolation ({i32}, {str}, {CustomType})
  - Compile-time elimination: Zero cost when disabled
  - Hex stream I/O: Separate debug stream (stream 2)
  - Permanent instrumentation: Debug code stays in codebase
  - Use cases: Network debugging, memory tracking, parser development, testing
  - Anti-patterns: printf debugging, removing debug statements, using for errors
  - Best practices: Meaningful groups, hierarchical organization, permanent code

## Usage

Each type guide includes:
- **Purpose and characteristics**
- **When to use / when NOT to use**
- **vs similar types** (semantic distinctions)
- **Literals and syntax**
- **Operations and methods**
- **Common patterns**
- **Anti-patterns**
- **Memory and performance**
- **Best practices**
- **Summary and key principles**

## Navigation

Browse by category in `types/` directory or see:
- [DOCUMENTATION_STATUS.md](DOCUMENTATION_STATUS.md) - **Complete tracking of what's done vs what's left**
- [UPDATE_PROGRESS.md](UPDATE_PROGRESS.md) - Chronological session development log

## Documentation Planning

See [DOCUMENTATION_STATUS.md](DOCUMENTATION_STATUS.md) for:
- Complete feature inventory (documented vs not documented)
- Recommended documentation order
- Progress metrics and estimates
- Next session priorities

## Contributing

This guide is actively developed. See UPDATE_PROGRESS.md for ongoing session work.
