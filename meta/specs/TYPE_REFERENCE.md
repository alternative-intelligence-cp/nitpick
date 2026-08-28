# Nitpick Type Reference — Memory Layouts & Behaviors

> **⚠️ FORMAL VERIFICATION RULE**: No C/C++ in any form. All types are defined either in
> handwritten LLVM IR (Tier 0) or in Nitpick source (Tier 1+). No libc, no C structs,
> no C calling conventions except where end-users explicitly opt into FFI.

This document defines every Nitpick type's:
1. **LLVM IR representation** — the exact `%struct`, `iN`, or `<N x T>` used
2. **Memory layout** — size, alignment, field offsets
3. **Tier** — 0 (handwritten LLVM IR) or 1+ (Nitpick source)
4. **Behavior summary** — what operations are valid and how they lower

---

## 1. Fundamental Scalars (Tier 0)

These map directly to LLVM primitive types. The compiler emits them as raw LLVM instructions.

### 1.1 Booleans

| Nitpick Type | LLVM IR Type | Size | Alignment | Notes |
|---|---|---|---|---|
| `bool` | `i1` (stored as `i8`) | 1 byte | 1 | `true` = 1, `false` = 0 |

**Behaviors:**
- Logical operators: `&&` (short-circuit AND), `||` (short-circuit OR), `!` (NOT)
- Comparison: `==`, `!=`
- No arithmetic operations
- Cast: `bool => int32` yields 0 or 1

**LLVM IR pattern:**
```llvm
; bool:x = true;
%x = alloca i8, align 1
store i8 1, ptr %x

; if (x) { ... }
%val = load i8, ptr %x
%cond = trunc i8 %val to i1
br i1 %cond, label %then, label %else
```

### 1.2 Signed Integers

| Nitpick Type | LLVM IR Type | Size | Alignment |
|---|---|---|---|
| `int8` | `i8` | 1 byte | 1 |
| `int16` | `i16` | 2 bytes | 2 |
| `int32` | `i32` | 4 bytes | 4 |
| `int64` | `i64` | 8 bytes | 8 |

**Behaviors:**
- Arithmetic: `+`, `-`, `*`, `/`, `%` — **overflow and underflow wrap**, per standard two's complement. They are *defined* behaviour, not undefined, and are routinely what is wanted (hashing, checksums, PRNGs, modular arithmetic).
  - Lowers to plain `add`/`sub`/`mul iN` — no overflow check, no trap.
  - **If overflow should be an error, use `tbb` instead** (D-037). The two families complement each other and the choice is made at the declaration, visibly.
  - **Divide/modulo by zero still traps to `failsafe`** — unlike overflow, it has no defined result, and Nitpick does not invent one (D-007). On `tbb` it yields ERR.

> A previous revision specified checked arithmetic here — `llvm.sadd.with.overflow.iN`
> with a failsafe trap on overflow. **Struck** (D-037): it would make ordinary
> correct code unrunnable and would leave no way to express wrapping at all.
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=` → `icmp eq/ne/slt/sgt/sle/sge`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>` → `and`, `or`, `xor`, `shl`, `ashr`
- Casting: explicit only (`x => int64`, `y =>! int32`)
- Literal suffixes: `42i32`, `-1i8`, `0FFhexi64`

**LLVM IR pattern:**
```llvm
; int32:result = a + b;      — wraps on overflow, no check
%val = add i32 %a, %b

; int32:q = a / b;           — divide-by-zero still traps
%is_zero = icmp eq i32 %b, 0
br i1 %is_zero, label %failsafe_trap, label %do_div
do_div:
  %q = sdiv i32 %a, %b
```

*(The overflow-checked form using `llvm.sadd.with.overflow.i32` belongs to `tbb`,
not to `int` — see §6.)*

### 1.3 Unsigned Integers

| Nitpick Type | LLVM IR Type | Size | Alignment |
|---|---|---|---|
| `uint8` | `i8` | 1 byte | 1 |
| `uint16` | `i16` | 2 bytes | 2 |
| `uint32` | `i32` | 4 bytes | 4 |
| `uint64` | `i64` | 8 bytes | 8 |

**Behaviors:**
- Same operations as signed integers, but:
  - Division/modulo use `udiv`/`urem` instead of `sdiv`/`srem`
  - Comparisons use `ult`/`ugt`/`ule`/`uge` instead of `slt`/`sgt`/`sle`/`sge`
  - Right shift uses `lshr` (logical) instead of `ashr` (arithmetic)
  - Overflow wraps, as with signed types — no check, no trap (D-037). Use `tbb` where overflow should be an error.
- Literal suffixes: `42u32`, `0FFhexu8`

> **Note:** `uint8` and `char8` share the same LLVM IR type (`i8`) but are **semantically distinct**. The type checker enforces different operation sets. See §2 for char types.

### 1.4 IEEE Floating Point (final form: D-143, 0.9.4)

| Nitpick Type | LLVM IR Type | Size | Alignment | Provides |
|---|---|---|---|---|
| `flt32` | `float` | 4 bytes | 4 | everything |
| `flt64` | `double` | 8 bytes | 8 | everything |
| `flt128` | `fp128` | 16 bytes | 16 | **storage only** — holds, moves, crosses FFI; no literals, arithmetic, or comparison (each is a soft-float libcall with no verified provider) |

`flt256`/`flt512` are **reserved words, not types** (D-143): LLVM has no
fp256/fp512, and a soft-float that wide has no consumer. The resolver refuses
them; the `f256`/`f512` literal suffixes are gone.

**Behaviors (flt32/flt64):**
- Arithmetic: `+`, `-`, `*`, `/`, `%` → `fadd`, `fsub`, `fmul`, `fdiv`, `frem`
  (`frem` lowers to the runtime floor's hand-written, exact `fmod`/`fmodf`)
- **Total, no traps**: division by zero yields ±inf/nan per IEEE — defined
  behavior, the reason infinities exist — unlike the integer trap row (D-007's
  float row was corrected by D-143)
- Negation is `fneg` (sign-bit exact: `-(0.0)` is `-0.0`)
- Comparison: `fcmp` ordered predicates, except `!=` which is `une` so that
  NaN ≠ NaN is true and `!=` stays the negation of `==`
- Literal suffixes: `3.14f32`, `2.718f64` (this section once said `3.14flt32`,
  which never lexed). A `flt32` literal carries at most 15 significant digits
  (D-143): it lowers through a correctly-rounded double, exact to 15 digits by
  the double-rounding theorem, and unbounded digits would make the value
  implementation-defined.
- Math functions (sin, cos, sqrt, …) arrive with the library tier, wrapping
  LLVM intrinsics

---

## 2. Character Types (Tier 0 layout, Tier 1 operations) — NEW

> **Design rationale:** Characters are semantically distinct from unsigned integers.
> `char8:c = 65char8;` is the letter 'A', not the number 65. Math operations on
> characters are nonsensical and are rejected at compile time. This eliminates the
> C-inherited confusion that caused extensive workarounds in the current compiler.

### 2.1 Character Widths

| Nitpick Type | LLVM IR Type | Size | Alignment | Encoding |
|---|---|---|---|---|
| `char8` | `i8` | 1 byte | 1 | UTF-8 code unit / ASCII byte |
| `char16` | `i16` | 2 bytes | 2 | UTF-16 code unit |
| `char32` | `i32` | 4 bytes | 4 | Unicode scalar value (full codepoint) |

**Behaviors — FORBIDDEN operations (compile-time error):**
- Arithmetic: `+`, `-`, `*`, `/`, `%` — **ERROR**: "cannot perform arithmetic on char type"
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>` — **ERROR**: "cannot perform bitwise operations on char type"

**Behaviors — PERMITTED operations:**
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=` → `icmp eq/ne/ult/ugt/ule/uge` (unsigned comparison for Unicode ordering)
- Assignment: `char8:c = 'A';` or `char8:c = 65char8;`
- Indexing into char arrays: `char8[]:arr; char8:c = arr[0];`

**Behaviors — CONVERSION functions (Tier 1, written in Nitpick):**

| Function | Signature | Description |
|---|---|---|
| `toUpper` | `char8 → char8` | Convert to uppercase (ASCII range only for char8) |
| `toLower` | `char8 → char8` | Convert to lowercase (ASCII range only for char8) |
| `isAlpha` | `char8 → bool` | Is alphabetic character |
| `isDigit` | `char8 → bool` | Is decimal digit |
| `isAlphaNumeric` | `char8 → bool` | Is alpha or digit |
| `isWhitespace` | `char8 → bool` | Is space, tab, CR, LF |
| `isUpper` | `char8 → bool` | Is uppercase letter |
| `isLower` | `char8 → bool` | Is lowercase letter |
| `toUint` | `char8 → uint8` | Reinterpret as unsigned integer |
| `fromUint` | `uint8 → char8` | Reinterpret unsigned integer as char |
| `toChar16` | `char8 → char16` | Widen (zero-extend) |
| `toChar32` | `char8 → char32` | Widen (zero-extend) |

> **Implementation priority:** `char8` and all its functions first. `char16` and `char32`
> functions are structurally identical (same pattern, wider types) and can be filled in
> later without any architectural changes.

**LLVM IR pattern:**
```llvm
; char8:c = 'A';
; Under the hood this is just i8, but the type checker prevents arithmetic
%c = alloca i8, align 1
store i8 65, ptr %c

; Comparison is permitted (unsigned)
%val = load i8, ptr %c
%is_upper = icmp uge i8 %val, 65    ; >= 'A'
%is_upper2 = icmp ule i8 %val, 90   ; <= 'Z'
%result = and i1 %is_upper, %is_upper2
```

### 2.2 Character Literals

```nitpick
char8:a = 'A';           // Single character literal
char8:newline = '\n';    // Escape sequences
char8:tab = '\t';
char8:null = '\0';
char8:backslash = '\\';
char8:quote = '\'';
char8:hex = '\x41';      // Hex escape (= 'A')
char32:emoji = '\u{1F600}';  // Unicode escape (char32 only)
```

### 2.3 Character Arrays

```nitpick
// char arrays do NOT implicitly add a null byte
char8[5]:hello = ['H', 'e', 'l', 'l', 'o'];

// To create a C-compatible string, use the cstring type (D-049):
cstring:cs = "Hello";                        // literal — checked at compile time
Result<cstring>:r = to_cstring(some_string);  // runtime — fails on an interior NUL
// NOT char8[] — an ordinary char array carries no termination guarantee

// char arrays are NOT strings. This is a compile error:
// string:s = hello;  // ERROR: cannot assign char8[] to string
```

### 2.4 Advanced Native Primitives (Hardware/Optimization Targets)

Nitpick includes several domain-specific native primitives designed for aggressive LLVM IR optimization, specifically targeting nonary logic simulation and high-performance tensor/matrix operations without the overhead of library abstractions.

| Type | Description | Optimization Target |
|---|---|---|
| `trit` | A base-3 unit of information (values: -1, 0, 1 or 0, 1, 2). | Bit-packed arrays, ternary logic gates. |
| `tryte` | A block of 10 trits (GRAMMAR_ADOPTION_CONFLICTS Part Q corrected the earlier 6). | Registers and nonary byte equivalents. |
| `nit` | Base-9 primitive (values: 0-8). | Nonary wave processing emulation. |
| `nyte` | A block of 2 nits (values: 0-80). | Compact nonary data streams. |
| `tensor` | N-dimensional array primitive. | Emits LLVM vector/SIMD intrinsics. |
| `matrix` | 2D data primitive. | Hardware-accelerated dot products / SGEMM. |

---

## 3. String Types (Tier 1 — Written in Nitpick)

### 3.1 String Layout

| Nitpick Type | Alias For | LLVM IR Struct | Size | Alignment |
|---|---|---|---|---|
| `string` | `string<char8>` | `{ptr, i64, i64}` | 24 bytes | 8 |
| `string<char16>` | — | `{ptr, i64, i64}` | 24 bytes | 8 |
| `string<char32>` | — | `{ptr, i64, i64}` | 24 bytes | 8 |
| `cstring` | — | `{ptr, i64}` | 16 bytes | 8 |

> **Superseded claim.** `FULL_specs.txt` §15.1.3 states that *"the `string` type
> guarantees internal null-termination for zero-cost abstraction when interfacing
> with C."* **That is prototype-era text and does not hold here.** `string` is
> `{ptr, len, cap}` as above and is not NUL-terminated; `to_cstring` (D-049)
> exists precisely because it is not. The same passage calls `int8->` a *"Fat
> Pointer containing bounds metadata"*, which D-038 settled the other way —
> pointers are thin. Both claims are superseded; see `PROTOTYPE_DELTA.md` §4.
>
> Note that D-049 would survive either way: a `string` may carry an interior NUL
> regardless of whether it also carries a trailing one, so the poison-NUL check
> is needed under both readings. Only the conversion cost differs — a copy rather
> than a scan.

**Struct fields:**
```llvm
; %String = type { ptr, i64, i64 }
;   Field 0: ptr   — pointer to char8[] (heap-allocated data buffer)
;   Field 1: i64   — length (number of char units, NOT bytes for char16/32)
;   Field 2: i64   — capacity (allocated char units)
```

**Offset map:**
| Offset | Size | Field | Type |
|--------|------|-------|------|
| 0 | 8 | data | `ptr` (to `char8[]` / `char16[]` / `char32[]`) |
| 8 | 8 | length | `i64` |
| 16 | 8 | capacity | `i64` |

### 3.2 String Behaviors

**FORBIDDEN operations:**
- Arithmetic: `+` is **concatenation**, NOT addition. No `-`, `*`, `/`, `%`.

**PERMITTED operations:**
- Concatenation: `string:c = a + b;` → allocates new buffer, copies both
- Comparison: `==`, `!=` → byte-by-byte comparison
- Ordering: `<`, `>`, `<=`, `>=` → lexicographic comparison
- Indexing: `char8:c = s[0];` → returns the char at that index (bounds-checked)
- Length: `int64:len = s.length;` → field access

**Standard functions (Tier 1, written in Nitpick):**

| Function | Signature | Description |
|---|---|---|
| `charAt` | `(string, int64) → char8` | Get character at index (bounds-checked) |
| `substring` | `(string, int64, int64) → string` | Extract substring (start, length) |
| `split` | `(string, char8) → string[]` | Split by delimiter |
| `trim` | `(string) → string` | Remove leading/trailing whitespace |
| `trimLeft` | `(string) → string` | Remove leading whitespace |
| `trimRight` | `(string) → string` | Remove trailing whitespace |
| `contains` | `(string, string) → bool` | Substring search |
| `startsWith` | `(string, string) → bool` | Prefix check |
| `endsWith` | `(string, string) → bool` | Suffix check |
| `indexOf` | `(string, char8) → int64` | First occurrence (-1 if not found) |
| `toUpper` | `(string) → string` | Uppercase (ASCII) |
| `toLower` | `(string) → string` | Lowercase (ASCII) |
| `toCharArray` | `(string, char8[]:dest) → Result<int64>` | Copy into a caller-owned destination; returns elements written. *(Previously `(string) → char8[]`, which is ill-formed under D-070 — the copy needs an owner and a `T[]` view cannot be one.)* |
| `fromCharArray` | `(char8[]) → string` | Convert char array to string (copy) |
| `to_cstring` | `(string) → Result<cstring>` | **Builtin**: NUL-terminated `cstring` (D-049). **Fails on an interior NUL** |
| `to_string` | `(cstring) → string` | **Builtin**: copies out of a `cstring` |

### 3.2.1 `cstring` — the kernel-bound string type (D-049)

`string` is `{ptr, len, cap}` and is **not NUL-terminated**, so it cannot be
handed to a syscall. `cstring` is the type that can.

```
cstring   { ptr: wild char8->, len: int64 }
```

The buffer is `len + 1` bytes with `buf[len] == 0u8`. **The length is retained**,
so `nlibc` never calls `strlen` — the unbounded "scan until NUL" read is absent
from every path and name in the library, which is what makes these calls
tractable for Astrée.

**`to_cstring` fails on an interior NUL.** A `string` may contain `0u8` anywhere;
a NUL-terminated form silently truncates there, so a validator inspecting the
`string` and a kernel reading the bytes would be examining *different strings* —
the poison-NUL bypass (`"avatar.png\0.sh"`). Rejecting it once at the boundary
replaces a check every validator would otherwise have to repeat.

Two ways to obtain one:

| Source | Checked | Cost |
|---|---|---|
| string literal in `cstring` position | compile time — interior NUL is a compile error | zero |
| `to_cstring(s)` on a runtime `string` | runtime — interior NUL is `Result.err` | one scan |

This literal-checked-at-compile-time mechanism was shared with `fmt` (D-045)
until D-053 removed that type; `cstring` is now its only consumer.

`cstring` is immutable: mutation could break the terminator invariant, and
construction is `string`'s job. `cstring` → `string` is an explicit `to_string`,
since it copies.

### 3.3 String Literals & LLVM IR Emission

```nitpick
string:greeting = "Hello, world!";
```

```llvm
; String literal "Hello, world!" (13 chars)
@.str.0 = private unnamed_addr constant [13 x i8] c"Hello, world!", align 1

; string:greeting = "Hello, world!";
; Emitted as a String struct with pointer to constant data
%greeting = alloca {ptr, i64, i64}, align 8
%data_ptr = getelementptr [13 x i8], ptr @.str.0, i64 0, i64 0
store ptr %data_ptr, ptr %greeting, align 8                    ; .data
%len_ptr = getelementptr {ptr, i64, i64}, ptr %greeting, i32 0, i32 1
store i64 13, ptr %len_ptr, align 8                            ; .length
%cap_ptr = getelementptr {ptr, i64, i64}, ptr %greeting, i32 0, i32 2
store i64 13, ptr %cap_ptr, align 8                            ; .capacity
```

---

## 4. Large Integers — native `iN` (D-011; corrected 0.9.3)

> **The LBIM section this replaces was the dead reading.** An earlier draft
> represented wide integers as limb-structs "working around LLVM backend bugs";
> D-011 settled the opposite with measurements, and cycle 0.9.3 confirmed them
> on the shipping toolchain (LLVM 20.1.2): `add i128` is five instructions with
> no libcall, i128 division emits the four `__divti3`-family libcalls — which
> the runtime floor provides as hand-written IR — and division at every wider
> width, through an executed `udiv i4096` probe, expands inline. Wide integers
> ARE the LLVM types; nothing is limbed.

| Nitpick Type | LLVM IR Type | Size | Alignment |
|---|---|---|---|
| `int128` / `uint128` / `tbb128` | `i128` | 16 bytes | **16** |
| `int256` / `uint256` / `tbb256` | `i256` | 32 bytes | **16** |
| `int512` / `uint512` | `i512` | 64 bytes | **16** |
| `int1024` / `uint1024` | `i1024` | 128 bytes | **16** |
| `int2048` / `uint2048` | `i2048` | 256 bytes | **16** |
| `int4096` / `uint4096` | `i4096` | 512 bytes | **16** (cryptographic modular arithmetic) |

**The alignment column is measured, not assumed** (0.9.3): the x86-64
datalayout aligns `i128` at 16 and every wider integer inherits that cap — an
executed probe put `{i8, i128}` at 32 bytes and `{i8, i256}` at 48. Three
sources previously disagreed (this document said 8; the frontend computed
bits/8, i.e. 32 for `i256`); the frontend now stores exactly this column
(`tt_int`/`tt_tbb` cap at 16), because a frontend struct offset that is not
LLVM's is memory corruption wearing a type annotation.

**Behaviors:** ordinary integer semantics at every width — D-037 wrapping,
D-092 explicit widening, the D-142 division guards (zero divisor and the
structural INT_MIN/−1 check, which is width-independent by construction).

---

## 5. Twisted Fixed-Point Types (Tier 0 layout, Tier 1 operations)

> **Deterministic arithmetic** — no floating-point non-determinism. Ideal for
> safety-critical applications (aerospace, medical, financial, navigation).
> **Twisted Fixed Point (`tfp`)** reserves a specific value (the most negative value)
> as a sticky error state. Operations resulting in undefined behavior or overflow
> resolve to this error state rather than crashing.

| Nitpick Type | LLVM IR Type | Size | Format | Alignment | Notes |
|---|---|---|---|---|---|
| `tfp32` | `i32` | 4 bytes | Q16.16 | 4 | 16-bit integer + 16-bit fraction |
| `tfp64` | `i64` | 8 bytes | Q32.32 | 8 | 32-bit integer + 32-bit fraction |
| `tfp128` | `i128` | 16 bytes | Q64.64 | 16 | native carrier (D-195; the word-struct rows were pre-D-011) |
| `tfp256` | `i256` | 32 bytes | Q128.128 | 16 | native carrier (D-195) |

**Literal syntax:** Suffix the numeric literal with the type name:
```nitpick
tfp256:pi = 3.14159265358979323846tfp256;
tfp64:half = 0.5tfp64;
tfp32:ratio = 1.5tfp32;
```

**Operations (all `tfp*` types):**
- Add/sub: same as integer add/sub on the raw representation (no shift needed)
- Mul: `(a * b) >> FRAC_BITS` (multiply raws, shift right by fractional bit count)
- Div: `(a << FRAC_BITS) / b` (shift left by fractional bits, then divide)
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`, `<=>` (spaceship)
  - **ERR-aware semantics (corrected at 1.3.2, D-195):** operations that
    overflow produce the `ERR` sentinel (most negative value) and it is
    STICKY; a comparison on an ERR operand TRAPS to `failsafe` (D-008 §5 —
    the NaN-style "always false/true" this row once described was rejected
    there for breaking trichotomy). `is_err(x)` is the test that looks
    without trapping.
- Remainder: `%` — the same-scale remainder, under the ERR discipline
- Unary negation: `-val` (total: the balance excludes the one overflowing value)
- ~~Shift / bitwise on the raw representation~~ — STRUCK at 1.3.2 (D-195):
  `ERR << 1` is zero, a one-instruction ERR laundry; the raw bits are not
  the value's API. Scaling is multiplication by a power-of-two constant.

**`floor` and `trunc` are METHODS on every width** (D-195 struck the
`tfp256_*` free-function family — one mechanism):
```nitpick
tfp256:x = 3.7tfp256;
tfp256:floored = x.floor();   // -> 3.0tfp256 (toward -inf)
tfp256:trunced = x.trunc();   // -> 3.0tfp256 (toward zero)
```

**Cast support:**
```nitpick
tfp32:g = 42.5tfp32;
flt64:fl  = g => flt64;         // OK — 32 raw bits fit a 53-bit mantissa exactly
tfp256:f = 42.5tfp256;
flt64:f2  = f =>! flt64;        // Q128.128 into 52 mantissa bits LOSES (corrected at 1.3.2)
int64:i   = f => int64;         // COMPILE ERROR — drops the fractional part
int64:i2  = f =>! int64;        // OK — explicit opt-in, truncates toward zero, yields 42
tfp64:f64 = f =>! tfp64;        // narrowing: precision loss, so =>! is required
tfp128:w  = 1.5tfp64 => tfp128; // widening keeps every value; ERR maps to ERR
```

> A cast OUT of the family TRAPS on an ERR operand under BOTH spellings
> (D-195): `=>!` acknowledges precision loss, and ERR is not a value —
> no acknowledgment converts a taint.

`=>` is a **compile-time error** wherever data loss is possible — not a runtime
trap and not a warning. `=>!` is the sole opt-out, and it is deliberately
greppable so an auditor can find every place a conversion was allowed to lose
information.

---

### 5a. Dimensional Analysis Type (`dim256<Unit>`)

> **Dimensional analysis is a `dim256`-exclusive feature.** Only `dim256` supports
> the `<Unit>` annotation. Narrower fixed-point types do NOT support dimensional annotations.

**A unit is an exponent vector** (D-196, 1.3.3) over the seven SI base
dimensions — mass, length, time, current, temperature, amount, luminous
intensity. A unit NAME is a name for a vector; the algebra runs on the
vectors themselves and is TOTAL, so `force * dist` IS the Joules vector
whether or not anything names it. Two `dim256` types are the same type
exactly when their vectors are equal. The DIMENSIONLESS vector is not a
`dim256` at all: it IS `tfp256`, which is why `dist / dist` is a bare
`tfp256` and why bare `dim256` (as an annotation or a literal suffix) is
refused — the dimensionless type already has a name.

**Syntax:**
```nitpick
dim256<Joules>:energy = 1000.0dim256<Joules>;
dim256<Meters>:dist   = 5.0dim256<Meters>;
dim256<Seconds>:time  = 2.0dim256<Seconds>;
```

**Named units.** The seven SI base units are compiler-declared
(`Kilograms`, `Meters`, `Seconds`, `Amperes`, `Kelvin`, `Moles`,
`Candela`); the standard derived names (`Newtons`, `Joules`, `Hertz`,
`Pascals`, `Watts`, `MetersPerSecond`, …) are PRELUDE declarations of the
same form any program can write (D-196, user-ratified — the grammar's
`unit:` production):

```nitpick
unit:Hertz = 1 / Seconds;
unit:Newtons = Kilograms * Meters / (Seconds * Seconds);
pub unit:Furlongs = Meters;   // a name is a name for a VECTOR — this one
                              // equals Meters' vector; declare it only to
                              // write it in annotations
```

The right-hand side is unit algebra only — unit names, `1`, `*`, `/`,
parentheses — evaluated at compile time. An annotation position takes a
single NAME, never an inline expression: compose in the declaration, name
the result, annotate with the name.

> **Runtime representation:** `dim256<Joules>` is IDENTICAL to bare `tfp256` at the
> LLVM IR level. The unit annotation is purely compile-time metadata — there is NO
> runtime overhead, and every D-195 `tfp256` rule (ERR discipline, saturation,
> exact literals, `.floor()`/`.trunc()`) applies unchanged.

**Dimensional algebra enforcement (compile time):**
```nitpick
// Units are tracked through arithmetic:
dim256<Meters>:dist  = 10.0dim256<Meters>;
dim256<Seconds>:time = 5.0dim256<Seconds>;
dim256<Meters>:speed = dist / time;      // ERROR: Meters/Seconds != Meters
                                          // Compiler: "dimensional mismatch: expected
                                          //   Meters, got Meters*Seconds^-1"

// Multiply creates compound units — the algebra is total, no registration:
dim256<Joules>:work = force * dist;      // OK: Newtons * Meters IS the Joules vector
```

**Rules:**
- Adding/subtracting/`%` same unit: ✅ result has same unit
- Adding/subtracting different units: ❌ compile-time error, both units shown
- Multiplying/dividing: ✅ vectors add/subtract; a bare `tfp256` operand is the
  zero vector, so scaling needs no special case; a canceled result IS `tfp256`
- Comparing different units: ❌ compile-time error (same vector: full ordering —
  a `dim256` is a number)
- `dim256<U> => tfp256`: ✅ drops the unit — always safe, a no-op at IR; ERR
  RIDES (the crossing never leaves the twisted family, so the D-144 leaving
  trap does not apply)
- `tfp256 =>! dim256<U>`: the acknowledged unit ASSERTION; without `=>!` it
  refuses — a silent unit-gain is how unit bugs are born (this replaced the
  pre-D-196 "warning" row: Nitpick has no warnings)
- `dim256<U> => dim256<V>`, `dim256<U>` ⇄ anything else: ❌ CAST_IMPOSSIBLE —
  a relabel is spelled as its two honest halves, `=> tfp256` then
  `=>! dim256<V>`
- Rendering: a dimensioned value has no `ToString` BY DESIGN — rendering drops
  the unit and drops are explicit: `&{x => tfp256}`

**In functions:**
```nitpick
func:velocity = dim256<Meters>(dim256<Meters>:d, dim256<Seconds>:t) {
    pass(d / t);    // Type error: d/t is Meters*Seconds^-1, not Meters
                    // Must declare return as dim256<MetersPerSecond> or bare tfp256
}
```

**In structs:**
```nitpick
pub struct:PhysicsBody = {
    dim256<Meters>:position;
    dim256<MetersPerSecond>:velocity;
    dim256<Kilograms>:mass;
};
```

**Type system:**
```llvm
; dim256<Joules> at LLVM IR level — identical to tfp256:
%tfp256 = type { i64, i64, i64, i64 }
; No IR difference — unit is purely a compile-time annotation on the type node
```

**Codegen note:** The dimensional annotation is attached to the AST type node.
Type checker verifies dimensional algebra. Codegen ignores it entirely.

---



## 6. TBB Types — Twisted Balanced Binary (Tier 0 layout, Tier 1 operations)

The most negative two's-complement value is **reserved as the ERR sentinel** and
excluded from the numeric range. What remains is symmetric about zero — the
"balanced" in Twisted Balanced Binary.

| Nitpick Type | LLVM IR Type | Size | Alignment | ERR sentinel | Valid numeric range |
|---|---|---|---|---|---|
| `tbb8` | `i8` | 1 byte | 1 | `-128` | `-127 .. 127` |
| `tbb16` | `i16` | 2 bytes | 2 | `-32768` | `-32767 .. 32767` |
| `tbb32` | `i32` | 4 bytes | 4 | `INT32_MIN` | `-(2^31-1) .. 2^31-1` |
| `tbb64` | `i64` | 8 bytes | 8 | `INT64_MIN` | `-(2^63-1) .. 2^63-1` |
| `tbb128` | `i128` | 16 bytes | 8 | `INT128_MIN` | `-(2^127-1) .. 2^127-1` |
| `tbb256` | `i256` | 32 bytes | 8 | `INT256_MIN` | `-(2^255-1) .. 2^255-1` |

*(A previous revision listed `tbb8` as `-128..127 (balanced)`. That is the
asymmetric two's-complement range and contradicted the sentinel rule stated
immediately below it. Corrected per D-008.)*

**Why balanced.** Excluding the most negative value makes negation and absolute
value **total**: `abs(x)` and `x * -1` are representable for every valid `x`, and
`INT_MIN / -1` — which faults in hardware on x86 — cannot arise, because
`INT_MIN` is ERR and rejected by the operand pre-check. An entire family of
asymmetry bugs is eliminated structurally.

**Behaviors:**
- **Any operation on an ERR value yields ERR.** This overrides mathematical
  identities: `ERR * 0` is `ERR`, not `0`; so is `ERR - ERR`. If an identity
  could erase ERR, sticky propagation would be defeated by ordinary algebra.
- Overflow **saturates to ERR** rather than wrapping. Out-of-range results land
  on the sentinel bit pattern naturally (`-127 + -1 = -128`).
- Division or modulo by zero yields ERR (D-007).
- **Comparison or branching on ERR traps to `failsafe`** — `bool` has exactly two
  values and cannot represent ERR, so the total rule cannot apply there. ERR
  flows through data and stops at control flow. Use `is_err(x)` to test without
  trapping, or a `pick` with an explicit `ERR:` arm.
- **Bitwise operators are rejected** on `tbb` — they can fabricate the sentinel
  (`~127i8` is `-128`) or destroy it (`ERR & 0` is `0`).
- **Casts are never straight bit operations.** The sentinel differs at every
  width, so sign-extending `tbb8` ERR yields a *valid* `tbb32` value. See
  D-008 §6. `FORMAL_DRAFT` 2.3.1 names a `tbb_widen<T>()` intrinsic for this.
- **A cast OUT of the family traps on an ERR operand under BOTH spellings**
  (D-144 as amended at 1.3.2, one rule with `tfp`): `=>!` acknowledges the
  VALUE's loss, and ERR is not a value — no acknowledgment converts a taint.
  The value itself is range-classified like any numeric pair, so `tbb64 =>
  int8` and `tbb32 => uint32` are compile errors and take the bang, exactly
  as the plain matrix spells `int64 => int8` and `int32 => uint32`. Entering,
  `=>` traps on a value with no image (the sentinel bit pattern, or out of
  range) and `=>!` saturates it to ERR — the family's own discipline, the
  reason entering is a runtime question while leaving is a compile-time one.
- **No implicit default value.** Definite-assignment analysis rejects
  read-before-write at compile time, which is stronger than any default (D-010).
  Assignment *replaces* a value, so an ERR taint is cleared by `x = 5i32`;
  stickiness governs computation, not storage.
- Used for function error codes, the `failsafe` signature, and any arithmetic
  that must degrade rather than trap.


---

## 7. Exotic Types — Ternary/Nonary (Tier 0 layout, Tier 1 operations)

**What they are**, which is the part that does not depend on the machine:

| Nitpick Type | Base | States |
|---|---|---|
| `trit` | base-3 | −1, 0, 1 |
| `tryte` | base-3 | 10 trits |
| `nit` | base-9 | −4 … 4 |
| `nyte` | base-9 | 5 nits |

> `tryte` and `nyte` hold the SAME 59049 states (3^10 = 9^5), which is why
> both lower to `i16` below. An earlier revision said `nyte` = "2 nits" — a
> carry-over defect fixed at 1.3.0, along with a "(Part Q)" annotation on
> `tryte` that had no referent.

**How the BINARY rung lowers them**, which is a fact about a target and not about
the type:

| Nitpick Type | LLVM IR Type | Size | Alignment |
|---|---|---|---|
| `trit` | `i8` | 1 byte | 1 |
| `tryte` | `i16` | 2 bytes | 2 |
| `nit` | `i8` | 1 byte | 1 |
| `nyte` | `i16` | 2 bytes | 2 |

> **The two tables are separate on purpose, and the separation is load-bearing.**
>
> A `trit` is a three-state value. That it currently arrives as an `i8` is how the
> binary rung emulates one — **not what a `trit` is** — and a compiler that treated
> the two as the same thing would have baked the emulation into the type.
>
> That matters because **ternary/nonary hardware is a direction this project has**,
> not a hypothetical: a native backend would lower `trit` to a ternary machine word
> and never see an `i8`. If the representation were the identity, that backend would
> be a retrofit rather than a second lowering — which is the "bolt everything on
> afterwards" this separation exists to prevent.
>
> Practically: ternary arithmetic is checked as **ternary** in the frontend, and the
> binary emulation lives in the rung that lowers it. Nothing above the backend may
> assume `i8`.
>
> It is the same argument the escape analysis makes for writing rules ahead of the
> constructs they govern — groundwork laid before the single Astrée run is far
> cheaper than groundwork added after it.

---


## 9. Composite Types (Tier 0 layout)

### 9.1 Structs

```nitpick
struct MyStruct = { int32:x; int64:y; bool:flag; };
```

```llvm
%MyStruct = type { i32, i64, i8 }
; Offset 0:  i32 x      (4 bytes)
; Offset 4:  padding     (4 bytes for alignment)
; Offset 8:  i64 y      (8 bytes)
; Offset 16: i8  flag   (1 byte)
; Offset 17: padding    (7 bytes for alignment)
; Total: 24 bytes, alignment 8
```

**Field access:**
```llvm
; val = obj.y
%field_ptr = getelementptr %MyStruct, ptr %obj, i32 0, i32 1
%val = load i64, ptr %field_ptr
```

### 9.2 Arrays (Fixed)

> **Design Note:** Fixed arrays are **Value Types**, not references. Passing `int32[4]` to a function copies all 16 bytes. They do NOT implicitly decay to pointers like in C. If you want to mutate an array inside a function or avoid copying, you must explicitly pass a pointer to it (`int32[4]->`).

```nitpick
int32[4]:arr = [1i32, 2i32, 3i32, 4i32];
```

```llvm
%arr = alloca [4 x i32], align 4
; Element access (bounds-checked):
; val = arr[idx]
; First: bounds check idx < 4
%in_bounds = icmp ult i64 %idx, 4
br i1 %in_bounds, label %access, label %failsafe_oob
access:
  %elem_ptr = getelementptr [4 x i32], ptr %arr, i64 0, i64 %idx
  %val = load i32, ptr %elem_ptr
```

### 9.2.1 Slices (Unsized Arrays)

`T[]` is a **slice** — a non-owning view of contiguous elements (D-070).

```llvm
%slice = type { ptr, i64 }    ; data, element count — 16 bytes, align 8
```

- **Indexing is bounds-checked against the runtime `len`**, trapping to
  `failsafe` exactly as a fixed array's static check does. This is where
  out-of-bounds detection comes from, and it is why pointers carry no bounds
  metadata (D-038).
- **`.len`** is available on every slice.
- **A slice is a second-class borrow** (D-004): it passes down the call stack and
  never up, cannot outlive the storage it views, and cannot cross a thread spawn
  or an `await`.

Constructed by ranging a fixed array or another slice — `arr[0...n]` — or, in
`wild` context only, from a raw pointer and a length with
**`#wild_slice<T>(ptr, len)`**. That form is deliberately parallel to
`#wild_ptr<T>(addr)` (D-019): an unverifiable extent is as privileged as an
unverifiable address, and as greppable.

`T[]` **never owns**. Growable owning sequences are a library concern, per D-041.

A slice does not cross an `extern` boundary as a view — nothing
address-shaped crosses a driver interface at all (D-149). A byte-slice
parameter in an `extern` block means a SIZED PAYLOAD the Bridge copies into
the ring outbound and copies-then-validates inbound.

### 9.3 Enums (Tagged)

```nitpick
pub enum:Color = { Red = 0i32; Green = 1i32; Blue = 2i32; };
```

```llvm
; Enum values are plain i32 constants
; Color.Red = 0, Color.Green = 1, Color.Blue = 2

; Tagged enums with payloads:
; enum Shape = { Circle(flt64); Rect(flt64, flt64); };
%Shape = type { i32, [2 x i64] }  ; tag + payload slot: [N x iK], K = the WIDEST
; payload's alignment in bits, N covering the widest size (0.9.2) — making the
; element BE the alignment is what keeps an 8-byte payload off odd addresses;
; the earlier [4 x i8]+[16 x i8] example had struct align 4 and was misaligned.
; `enum =>! intN` reads the TAG (slot 0) at every shape; `intN => enum` is
; impossible in both spellings (D-140).
; Note: The compiler explicitly inserts padding arrays to ensure that 
; payload extraction does not cause unaligned reads/segfaults on strict architectures.
```

---

## 10. Pointer & Reference Types (Tier 0)

| Nitpick Syntax | LLVM IR | Description |
|---|---|---|
| `T->` | `ptr` | Pointer to T |
| `any->` | `ptr` | Type-erased pointer |

> **Pointer Syntax Rule:** Nitpick exclusively uses the `->` operator for pointer types (`int32->`). The C-style `*` pointer syntax (e.g. `void*`, `char*`) is **forbidden everywhere** — the former `extern`-block allowance died with in-process FFI (D-149): an `extern` block is a driver interface written entirely in Nitpick types, and there is no C ABI to maintain.
> - `@var` = address of
> - `<-ptr` = dereference pointer
> - `ptr.field` = unified member access (automatically dereferences if pointer)

All pointers are **thin** — a single machine word, LLVM opaque `ptr` at the IR
level, carrying **no bounds metadata** (D-038). The distinction between wild and
borrow pointers is enforced entirely by the type checker; the LLVM IR is
identical.

> `FORMAL_DRAFT` 15 §15.1.3 claims `int8->` is a *fat* pointer carrying bounds
> metadata. **Struck.** Second-class borrows (D-004) and generation-counted
> `Handle<T>` already close the dangling and use-after-free classes statically, so
> runtime metadata would pay twice — at the cost of C ABI compatibility, two to
> three words per pointer on the numeric hot path, and turning part of
> `--verify-memory` into a runtime guarantee rather than a static one.
> `--guard-pages` remains available for overrun detection around `wild`
> allocations without changing the representation.

---

## 11. Optional & Result Types (Tier 1 — Written in Nitpick)

### 11.1 Optional\<T\> (T?)

```llvm
; T? = { i1 has_value, T value }
; Example: int32?
%Optional_i32 = type { i8, i32 }   ; i8 for has_value (padded from i1)
; .has_value at offset 0 (1 byte)
; padding: 3 bytes
; .value at offset 4 (4 bytes)
; Total: 8 bytes

; int32?:a = NIL;    = { i8 0, i32 0 }      ; zeroinitializer -- the value half is ZEROED, never undef
; int32?:b = 42i32;  = { i8 1, i32 42 }
```

> **Lowered at 1.0.7, compiler-known exactly as `Result<T>` is.** `ll_type`
> answers `{ i8, T }`; `NIL` where an `Optional` is expected lowers to
> `zeroinitializer` (the earlier comment's `undef` is struck — a defined byte is
> what the `fail` desugar already chooses for a `Result`'s value half, and what
> a verifier can reason about); the wrap from `T` is built at every slot by the
> emitter's one slot helper (`emit_fit`, 1.0.6e); `== NIL`/`!= NIL` is a tag
> test in either operand order; `??` evaluates its default only on the empty
> path; `?.` yields `zeroinitializer` of the result type when empty and wraps
> the field when present — or stores it as it is when the field is itself an
> `Optional`, the flattening rule below. No library struct declares it: D-099
> strikes its readable members, and a struct whose fields are not members would
> be the kind of special case the blueprint rule forbids.

**There is no constructor, and none is needed (D-099).** An `Optional<T>` is built
by writing the value; it is emptied by writing `NIL`. That is the pair the
language has always had — **`NULL` is no pointer, `NIL` is no value** — and §27 is
the authority on both.

| Written | Is |
|---|---|
| `int32?:a = NIL;` | empty |
| `int32?:b = 42i32;` | holding `42i32` |
| `a == NIL`, `a != NIL` | the test |
| `a ?? d` | the value, or `d` |
| `a?.f` | the field, still wrapped |

`T?` and `Optional<T>` are **one type with two spellings**, so every rule below
applies to both — a rule written at one of them could be stepped around by using
the other.

The wrap from `T` to `Optional<T>` is the **one implicit conversion in the
language**. It applies wherever a value meets a slot — a declaration's
initialiser, a call argument, `pass`, a `Result{…}` field, an unwrap's default —
and nothing else is coerced: no implicit widening (D-092), no numeric conversion,
no pointer decay. A `NIL`-**typed value**, which is what `drop f()` yields, is
deliberately *not* accepted; `drop` produces `NIL` precisely so that using a
discarded outcome as a value is an error.

> ⚠️ **`Some(42)` was struck (D-099).** It appeared in this IR comment and
> **nowhere else** — no grammar production, no keyword, no AST node, and no trace
> in the prototype. A replacement literal form `Optional{…}` was then drafted and
> **also struck**, for the same reason: it was an invention arrived at by symmetry
> with `Result{…}` rather than by looking for what already built an `Optional`.
> `tests/special/test_nil_optional.npk` in the prototype had the answer.

**An `Optional` has no readable members.** `.has_value` and `.value` are IR field
names, not source-level members — neither exists in the prototype's source
surface. `.has_value` duplicates `== NIL`, and `.value` is the unchecked read the
wrapper exists to prevent; `??` and `?.` are the accessors that cannot be wrong.

**Two inner types are refused**, both naming a state nobody can write down:

- **`NIL?`** — `NIL?:x = NIL;` is ambiguous between empty and holding `NIL`'s one
  value.
- **`Optional<Optional<T>>`** — the inner absence is unreachable, since `x = NIL`
  sets the outer tag. The postfix form cannot even be lexed: `??` is one token, so
  `int32??` reads as `int32` followed by the null-coalesce operator. **`?.`
  flattens** rather than manufacturing the type behind the rule's back.

### 11.2 Result\<T\>

> **EVERY function in Nitpick returns `Result<T>`** except `pub func:main` and
> `pub func:failsafe` (which return raw `int32` and use `exit()`).
> This is NOT optional — the compiler enforces it. `<T>` is implied from the
> function's declared return type.

**Struct definition (canonical field names):**
```nitpick
struct<T>:Result = {
    T:value;        // Success value (zero-initialized if error — see fail desugar)
    Error:err;      // 0 = no error. The DOMAIN is typed now (D-179): the prelude declares system, users declare their own, the sign is an encoding detail.
};
```

**LLVM IR layout:**
```llvm
; Result<T> = { T value, i32 error }
; Example: Result<int32>
%Result_i32 = type { i32, i32 }
; .value at offset 0
; .err at offset 4 (Error, 4 bytes)
; Total: 8 bytes, align 4
```

**Creating Results — `pass` / `fail` / explicit `return`:**

| Syntax | Desugars to | Notes |
|---|---|---|
| `pass(retVal);` | `return Result{value: retVal};` | Success path (`err` omitted = the zero) |
| `fail(errCode);` | `return Result{err: errCode, value: zero};` | Error path — `errCode` must be non-zero and non-ERR |
| `return Result{err: errCode, value: retVal};` | (literal, no desugar) | Special: return both value AND error |

> **The suffixes here read `0i32` in an earlier revision**, from before D-069
> settled the field as a `tbb32`. The struct definition directly above already
> said `tbb32:error`, so the table was contradicting it two lines later — and
> `0i32` is not assignable to a `tbb32`, there being no implicit conversion
> between them (D-092, corrected). Corrected to `0tbb32`.

**Either field may be omitted**, and each defaults to the zero for its side. That
is not a convenience: it is what makes the two desugars above expressible as
literals, since `pass` writes only `value` and `fail` writes only `error`. **Field
order is free** — the names are matched, so `Result{value: v, err: e}` and
`Result{err: e, value: v}` are the same node.

**There is no `is_error` field to write** (D-069). It is derived on every read.

> ### The error state is stored once (D-069)
>
> `is_error` was previously a **stored** `bool` alongside `error`, encoding the
> same fact twice with no invariant relating them — `{err: 0, is_error: true}`
> and `{err: 5, is_error: false}` were both constructible and neither had a
> defined meaning. The field is removed.
>
> **`r.is_error` remains valid source** as a derived accessor for
> `r.err != 0i32`, so existing `pick(r.is_error)` code is unaffected. The fact
> is now computed rather than stored, and so cannot contradict the field it
> summarises.
>
> The error field's value space is total: `0` is success, positive codes are user
> errors, negative codes are system errors, and `INT32_MIN` — `tbb32`'s ERR
> sentinel — is **unconstructible**. Building a `Result` whose code is ERR, or
> whose code is `0` on a failure path, traps to `failsafe` where it would be
> built: an error the caller cannot identify defeats the discipline that forces
> the caller to handle it.

**Unwrapping / accessing Result values:**

The compiler WILL NOT allow accessing `.value` without first checking `.is_error`
(or using one of the bypass operators). Available strategies:

| Operator | Syntax | Shorthand | Behavior |
|---|---|---|---|
| Safe unwrap | `expr ? defaultVal` | — | If error, use default value |
| Null coalesce | `expr ?? defaultVal` | — | **On an `Optional`, not a `Result`** — if it is `NIL`, use the default (D-099) |
| Emphatic unwrap | `expr ?! errCode` | — | If error, triggers failsafe (Layer 3) |
| Raw unwrap | `raw(expr)` or `raw expr` | `_!` | Extract `.value` WITHOUT checking error (unsafe, auditable "TOS keyword") |
| Drop | `drop(expr)` or `drop expr` | `_?` | Discard the entire Result, don't care about value or error |
| Discard | `discard(param)` | `_~` | For unused variables/params — semantically different from `drop` |

> **`raw` vs `drop` vs `discard`:** These are three DISTINCT concepts:
> - `raw` = unwrap a `Result<T>`'s value. **D-163 (settled; the licence is ON — `NITPICK-TYPE-042`)**: licensed only
>   on a call whose callee is `never fails` — a checked, zero-cost unwrap, not a
>   bypass. A may-fail call uses `relay` / `?!` / `? d` / `is_err`.
> - `drop` = the "void call": run a `never fails` function whose success type is
>   `NIL` (D-163). It no longer discards an error — a may-fail call is `relay`ed,
>   `?!`-trapped, or `? NIL`-swallowed; a never-failing VALUE is `discard(raw f())`.
> - `discard` = "I have this value/param and will not use it" — takes a VALUE,
>   never a `Result` (D-089/D-163).
>
> Everything above is CURRENT: the contract, the statement closed list
> (`TYPE-039`), the `defer` rule (`TYPE-040`), and the licence itself —
> an unlicensed `raw`/`drop` is refused with `TYPE-042` since 1.1.2.

**Driver-interface (`extern`) behavior (D-149):**
```nitpick
extern:"storage_driver" = {
    opaque struct:DbHandle;
    func:open = DbHandle(int8[]:path);
};
```
> **ALL functions (including `extern` driver methods), except `main` and
> `failsafe`, return `Result<T>`** — and for driver methods no per-function
> contract is written, because the WIRE has a universal failure convention:
> every dispatch returns status plus payload, and timeouts, driver death,
> and protocol violations arrive as uniform negative codes in the D-141
> error space. In-process C FFI does not exist; the history below explains
> the shape this section used to have.

> ### The D-002 era, kept for the record
>
> When in-process C linkage was still the plan, a C call's failure could not
> be inferred from its type (C has no universal failure convention), so
> D-002 required every `extern` declaration to state its own contract —
> `fails on result < 0i32 with errno`, `never fails` — and made omitting it
> a compile error, so silence never became a silent `Ok`. D-149 removed
> in-process FFI entirely; the contracts went with it, their PRINCIPLE (a
> foreign failure always arrives as an errored `Result`) now delivered by
> the wire protocol itself. The contract grammar remains parsed and is
> refused by the checker with D-149 named.

---

## 12. Handle & Arena Types (Tier 0 layout, Tier 1 operations)

### 12.1 Handle\<T\>

```llvm
; Handle<T> = { u64 index, u32 generation }
%Handle = type { i64, i32 }
; .index at offset 0 (8 bytes)
; .generation at offset 8 (4 bytes)
; padding: 4 bytes
; Total: 16 bytes, alignment 8
```

### 12.2 arena\<T\>->

```llvm
; arena<T>-> is a pointer to a slab: { T* data, i64 count, i64 capacity }
; Allocated via alloc() and cast: alloc(N) => arena<T>->
%Arena = type { ptr, i64, i64 }
```

---

## 13. Atomic Types (Tier 0 — Native LLVM IR)

`atomic<T>` emits native LLVM atomic IR — no C shims, per the zero-dependency constraint. All high-level methods enforce **SeqCst** ordering (D-016); weaker orderings are reachable only through low-level compiler intrinsics.

| Operation | LLVM IR |
|---|---|
| `a.load()` | `load atomic i32, ptr %a seq_cst` |
| `a.store(v)` | `store atomic i32 %v, ptr %a seq_cst` |
| `a.swap(v)` | `atomicrmw xchg ptr %a, i32 %v seq_cst` |
| `a.fetch_add(v)` | `atomicrmw add ptr %a, i32 %v seq_cst` |
| `a.fetch_sub(v)` | `atomicrmw sub ptr %a, i32 %v seq_cst` |
| `a.compare_exchange(exp, des)` | `cmpxchg ptr %a, i32 %exp, i32 %des seq_cst seq_cst` |

---

## 14. SIMD Types (Tier 0 — LLVM Vector Types)

| Nitpick Type | LLVM IR Type | Size | Alignment |
|---|---|---|---|
| `simd<flt32, 4>` | `<4 x float>` | 16 bytes | 16 |
| `simd<flt64, 2>` | `<2 x double>` | 16 bytes | 16 |
| `simd<int32, 8>` | `<8 x i32>` | 32 bytes | 32 |

> **Implemented at 1.3.1 (D-194).** Elements: the integer widths 8..64,
> `flt32`/`flt64`, and `bool` (comparison results must be bindable); lanes
> 2..64, total ≤ 64 bytes; alignment = next power of two ≥ size, capped 64.
> `simd(…)` constructs annotation-directed — N components, or ONE that
> splats. Operations are elementwise on identical types (`+ - * / %`; and
> `& | ^ << >>` on integer lanes); comparisons yield `simd<bool, N>`;
> `v[i]` is a bounds-checked lane place; `.len` is the lane count. Integer
> vector division carries D-007 as ANY-LANE checks (zero → DivByZero,
> INT_MIN/−1 → DivOverflow). Reductions are methods — `.sum()/.min()/.max()`
> on numeric lanes, `.all()/.any()` on `bool` lanes — lowered as ordered
> extract-and-fold chains: float `.sum()` is deterministic BY CONSTRUCTION,
> and nothing lowers to a libcall. Casts are elementwise under the scalar
> rules and never change N. Shuffles are OUT by decision (D-194): no
> consumer in evidence.

---

## 15. Vector / Matrix / Tensor Types (Tier 1 — **library**, not keywords)

| Nitpick Type | Backing | Layout |
|---|---|---|
| `vec2` | `simd<flt64, 2>` | 16 bytes |
| `vec3` | `simd<flt64, 3>` | 24 bytes |
| `vec4` | `simd<flt64, 4>` | 32 bytes |
| `matrix<T>` | `{ptr, i32, i32}` (data, rows, cols) | 24 bytes |
| `tensor<T>` | `{ptr, ptr, i32}` (data, dims_array, ndims) | 24 bytes |

> **These are LIBRARY types and are not keywords** (D-135). They were keywords, and
> `vec2` was backed by a **struct** `{flt64, flt64}` — which is the shape that does
> *not* reach an LLVM vector register, so the keyword form was the slower one. §14's
> `simd<T, N>` is the primitive and these are built on it.
>
> A library cannot declare a type whose name is a keyword, so the removal is what
> makes them writable at all rather than a tidying-up.
>
> `matrix<T>` and `tensor<T>` are heap-backed containers — a pointer, a shape — with
> nothing SIMD about them, so they were library types by construction whatever the
> spelling said.

---

## 16. Function Types (Tier 0)

```llvm
; func:add = int32(int32:a, int32:b) { ... };
define { i32, i32 } @add(i32 %a, i32 %b) {   ; D-069: {value, error} — the stored is_error flag was removed; .is_error derives as error != 0
  ; Returns Result<int32> by default
  ; {value, error} — error is tbb32 (i32), NOT a pointer payload.
  ; The stored is_error flag was removed by D-069; r.is_error is derived.
  ; See §11.2; the {T, void*, i8} form is rejected (D-005).
}

; When Result elision proves function is infallible:
define i32 @add_elided(i32 %a, i32 %b) {
  ; Returns raw i32
}
```

---

## 17. Future\<T\> — **internal lowering artifact, not a user type (D-058)**

> **Not surface syntax.** Nothing in the language produces a `Future<T>`:
> `await f()` yields `T` directly, and spawning via `drop work()` discards the
> result. It cannot be named in a signature or held in a variable. The IR shape
> below is documented in `CONCURRENCY_REFERENCE.md` §2.4, where the `@llvm.coro`
> lowering is described; it is retained here only so the layout has one
> authoritative statement.
>
> Making it user-visible would require committing to composition, cancellation,
> polling, and a lifetime story for a suspended coroutine frame — and would force
> an answer to `Future<Result<T>>` versus `Result<Future<T>>`, which staying
> internal avoids entirely.


```llvm
; future<T> is a coroutine handle + result slot
%Future = type { ptr, ptr }  ; {coroutine_handle, result_slot}
```

---

## 18. dyn Trait — Dynamic Dispatch (Tier 1)

**One data word and ONE VTABLE WORD PER TRAIT** — `(N+1) x 8` bytes (D-159).
The 16-byte shape below is the single-bound case, not the general one; a
multi-bound `dyn` built at 16 bytes leaves every vtable pointer after the first
with nowhere to live.

```llvm
; `dyn A` — the fat pointer: {data_ptr, vtable_ptr}
%DynTrait = type { ptr, ptr }

; `dyn A & B & C` — one vtable word per bound, 32 bytes
%DynABC = type { ptr, ptr, ptr, ptr }

; Vtable: function pointers in TRAIT DECLARATION ORDER (D-158), one per
; declared method, each an adapter thunk compiled against the concrete type
%Describable_vtable = type { ptr }  ; one method → one fn ptr
```

The bounds are **canonically ordered by trait name at type interning**, so
`dyn A & B` and `dyn B & A` are one type with one layout and every vtable
word's slot is a compile-time constant. Widening (`dyn A & B` → `dyn A`) is a
value rebuild copying the data word plus the retained bounds' words; there are
no runtime tables and no prefix/subview scheme. `TRAITS_REFERENCE.md` §5.3 is
the fuller statement.

---

## 19. Dimensional Types (Tier 1)

Dimensional analysis types carry unit metadata at compile time only.
At runtime they are the same as their base numeric type.

```nitpick
// (An earlier draft showed `int32<Meters>` unit-annotations here; D-036
// rejected value-generic units on plain integers — this section awaits the
// dimensional types' own design.)
// At LLVM IR level: just i32
```

Supported unit dimensions (compile-time annotations only):
- `Joules`, `Meters`, `Seconds`, `Newtons`, `Kelvin`
- Arithmetic across dimensions is validated at compile time (e.g., `Meters/Seconds` = velocity)

---

## 20. frac<N> — Rational Fraction Types (Tier 1)

> **Missing from earlier docs, recovered from codebase.** Fraction types with
> whole number, numerator, and denominator components. Useful for exact arithmetic
> without floating-point rounding.

| Type | Whole | Numer | Denom | Total Size |
|---|---|---|---|---|
| `frac8` | int8 | int8 | uint8 | 3 bytes |
| `frac16` | int16 | int16 | uint16 | 6 bytes |
| `frac32` | int32 | int32 | uint32 | 12 bytes |
| `frac64` | int64 | int64 | uint64 | 24 bytes |

```llvm
; frac32 LLVM IR layout: struct { i32 whole, i32 numer, i32 denom }
%frac32 = type { i32, i32, i32 }
```

Operations:
- `frac_add(a, b)`, `frac_sub(a, b)`, `frac_mul(a, b)`, `frac_div(a, b)`
- `frac_simplify(a)` — reduce to lowest terms (GCD reduction)
- `frac_to_flt64(a)` — convert to floating point
- `frac_from_int(whole, numer, denom)` — construct from parts

---

## 21. complex<T> — Generic Complex Number (Tier 1)

Generic complex number type with real and imaginary components.

```nitpick
complex<flt64>:z = complex(3.0flt64, 4.0flt64);  // 3 + 4i
```

```llvm
; complex<flt64> layout: struct { double real, double imag }
%complex_flt64 = type { double, double }
```

Operations:
- `complex_add(a, b)`, `complex_sub(a, b)`, `complex_mul(a, b)`, `complex_div(a, b)`
- `complex_abs(a)` — magnitude (√(real² + imag²))
- `complex_conj(a)` — complex conjugate
- `complex_real(a)`, `complex_imag(a)` — extract components
- Supports all `T` that support arithmetic: `flt32`, `flt64`, `tfp32`, `tfp64` (the `fix*` names are obsolete — D-036)

---

> **Memory Model Note (string, buffer):**
> Nitpick supports multiple memory spaces: managed/`stack` (default, scope-determined), `wild` (unmanaged C-like memory), and `wildx` (JIT executable memory). There is no `gc` space and no collector (D-003).
> By default, `string` and `uint8[]`-backed storage are scope-managed (stack-allocated where escape analysis permits, otherwise arena- or `wild`-backed with a determined owner). `buffer` is often allocated in `wild` memory, hence the existence of manual `buffer_free()`. No pin operator is needed: nothing relocates memory implicitly, so a pointer taken for FFI stays valid for the lifetime its owner guarantees (D-020).

## 22. binary — **REMOVED (D-074)**

`binary` was `{ ptr, i64 length }` — an immutable, zero-copy-sliceable blob.

**D-070 defines a slice as `{ ptr, i64 len }`.** Identical layout, identical
non-owning behaviour, identical sub-ranging. The remaining difference was
immutability, and that is a **binding** property in Nitpick rather than a type
property, so an immutable byte view is `fixed uint8[]`.

Redundant twice over. `binary` and its seven `binary_*` operations are removed;
use `uint8[]`. `buffer` (§23) is retained, because a slice cannot own and the
owning byte container is what a read fills and a write drains.

> §24 is absent. It is where `stream` presumably belonged — a `BuiltinType`
> keyword that was never defined. D-074 returns it to userland along with
> `process`, `pipe`, `debug`, and `log`; the I/O model is `IO_REFERENCE.md`.

---

## 23. buffer — Mutable Raw Memory Buffer (Tier 0/1)

The **owning** byte container — what a read fills and a write drains. A slice
(`uint8[]`) cannot own, which is why this type survives D-074's removal of
`binary`.

```llvm
; buffer layout: struct { ptr data, i64 length, i64 capacity }
; Same layout as string — but typed differently (no encoding assumption)
%buffer = type { ptr, i64, i64 }
```

```nitpick
buffer:buf = buffer_new(1024i64);  // 1024 byte buffer
buffer_write_i32(buf, 0i64, 42i32);  // write int32 at offset 0
```

Operations:
- `buffer_new(cap)` — allocate
- `buffer_free(buf)` — deallocate
- `buffer_write_i8/i16/i32/i64(buf, offset, val)` — typed writes
- `buffer_read_i8/i16/i32/i64(buf, offset)` — typed reads
- `buffer_bytes(buf)` — borrow the contents as `uint8[]`; `fixed` for a read-only view *(was `buffer_to_binary`; D-074)*
- `buffer_resize(buf, new_cap)` — grow/shrink (ralloc)

---

## 25. vec9 / tmatrix / ttensor — Extended Vector Types (Tier 1)

### vec9 — 3×3 Matrix as Flat Vector

```llvm
; vec9 layout: 9 floats contiguous
%vec9_flt32 = type { float, float, float,
                     float, float, float,
                     float, float, float }
```

Used for 3×3 rotation matrices, 2D homogeneous transforms.
Access: `v.m00`, `v.m01`, ... `v.m22` (9 fields named mRC = row R, col C)

### tmatrix — Ternary-Element Matrix

Like `matrix<T>` but element type is `trit`/`tryte` (ternary valued).
Used for quantum state simulation.

### ttensor — Ternary-Element Tensor

N-dimensional generalization of `tmatrix`. Element type is `trit`/`tryte`.

Priority: **P4** — post-bootstrap.

---

## 26. `const` vs `fixed` — Immutability Keywords

> **These are frequently confused — this section is the authoritative reference.**

### `fixed` — Nitpick Immutability Keyword

`fixed` is Nitpick's native immutability keyword. It means "this value CANNOT change after initialization."

**Valid uses:**
```nitpick
// Variable declaration
fixed int32:MAX_SIZE = 1024i32;           // module-level constant
pub fixed string:VERSION = "1.0.0";       // public module-level constant

// Struct field
pub struct:Config = {
    fixed string:name;       // field can never be reassigned after construction
    int32:value;             // normal mutable field
};

// Local variable
func:f = NIL() {
    fixed int32:limit = 100i32;   // cannot be reassigned in this scope
    // limit = 200i32;             // ERROR: ARIA-056 cannot assign to fixed
    pass(NIL);
};
```

**Diagnostic:** Attempting to assign to a `fixed` variable or field emits:
```
ARIA-056: cannot assign to fixed field <f> of struct <S>
ARIA-056: cannot assign to fixed variable <name>
```

**IR representation:** `fixed` variables are still emitted as `alloca` + `store` at IR
level — they are NOT LLVM `constant` globals unless also at module scope.
At module scope, `pub fixed T:name = val;` → `@name = global T val` (initialized once).
The immutability is enforced entirely by the type checker; no LLVM `readonly`/`const`
attribute is needed since the type checker prevents further writes.

**`fixed` in function parameters:**
```nitpick
func:greet = NIL(fixed string:name) {
    // name = "other";   // ERROR: cannot reassign fixed parameter
    pass(NIL);
};
```

**`fixed` with arrays** (static arrays):
```nitpick
fixed int32[4]:arr = [1i32, 2i32, 3i32, 4i32];  // fixed-size, immutable array
```

---

### `const` — C Interoperability Only

`const` is **ONLY valid inside `extern { }` blocks** for C/C++ FFI compatibility.
It is NOT a general Nitpick keyword.

```nitpick
// VALID: const in extern block (C ABI compatibility)
extern {
    const int32:EINVAL = 22i32;          // C-compatible const int
    func:strlen = int64(const int8->:s); // const char* parameter
}

// INVALID: const outside extern block — type checker error:
// const int32:x = 42i32;    ERROR: 'const' is only valid in extern blocks
//                            Use 'fixed' for Nitpick constants
```

**Diagnostic for invalid `const` use:**
```
ARIA-XXX: 'const' is reserved for extern blocks only.
          Use 'fixed' to declare an immutable Nitpick value.
```

**At IR level:** `const` in extern maps to C's `const` qualifier — typically a `nocapture readonly` pointer attribute in LLVM IR for pointer parameters, or a `constant` global for values.

---

## 27. Special Values: void / any / NULL / NIL / unknown


> **These are frequently confused. This section is the authoritative reference.**

### `NIL` — No Value

**`NIL` and `NULL` are a designed pair: `NULL` is no pointer, `NIL` is no value.**
Everything below follows from that one sentence, and nothing beyond it is part of
`NIL`'s meaning.

- Represents "nothing" at the type level (like `void` in C, but as a value)
- Return type annotation for functions that produce no meaningful value: `func:f = NIL(...)`
- "void functions" DO NOT EXIST in Nitpick — they return `Result<NIL>` instead
- `pass(NIL)` desugars to `return Result{ value: NIL, err: 0i32 }`
- To call a NIL-returning function without checking: `drop(myFunc());`
- **`NIL` is zero-sized** (D-084). Its only value carries no information, so it
  occupies nothing: a `NIL` struct field takes no space and `pass(NIL)` moves
  nothing.
- **`NIL` is also the empty `Optional<T>`** (D-099): `int32?:a = NIL;`, and
  `a == NIL` is how an `Optional` is tested. That is not a second meaning — "no
  value" is what it says in both positions, which is what lets one spelling do
  both without breaking the rule that a construct means the same thing everywhere.
- Consequently `NIL` **reads its context**, exactly as `NULL` does: where an
  `Optional<T>` is expected it is the empty one, and anywhere else it is the unit
  value. The two differ in one way, and it is forced — **`NULL` with no context is
  an error and `NIL` is not**, because `NULL` has no type of its own to fall back
  to and `NIL` does.
- `NIL?` is refused: it would be empty in two ways with no way to tell them apart.
- IR: `Result<NIL>` is therefore `{ i32 }` — **4 bytes, align 4, returned in a
  single register.** Since void functions do not exist, this is the most common
  return type in the language.
  *(Two previous revisions were wrong here. One gave `{ ptr undef, ptr null, i8 0 }`,
  typing the error field as a pointer against the canonical layout in §11
  (corrected per D-069); the replacement gave `{ i8 undef, i32 0 }`, which is
  8 bytes after padding to carry no information, because `NIL`'s size had never
  been stated (corrected per D-084).)*

### `NULL` — No Pointer

- Represents address zero — the null pointer
- ONLY valid in pointer contexts: `int32->:p = NULL;`
- NOT valid as a general "no value" — that's `NIL`
- IR: `ptr null`

### `void` — C Interop Only

- ONLY valid inside `extern { }` blocks, for functions that return C `void`
- **Forbidden everywhere else** — type checker error with diagnostic:
  `"'void' is reserved for extern blocks; use 'NIL' for Nitpick functions returning nothing"`
- IR: maps to LLVM `void` return type in the extern function's `declare`

### `any` — Type-Erased Pointer

- Nitpick's equivalent of C's `void*`
- MUST be used with the pointer suffix: `any->`  (NOT bare `any`)
- Bare `any` without `->` is a type error:
  `"'any' must be used as a pointer type: 'any->'. Bare 'any' is not a valid type."`
- IR: `ptr` (opaque pointer — same as all other pointers in LLVM opaque pointer mode)
- Cast to concrete type via **`p =>! T`** before dereferencing

  > ⚠️ **Corrected (D-095).** This read `p => T`. `=>` means *nothing can be lost
  > and the compiler proved it*, and nothing is proven here: an `any->` is
  > type-erased, so giving it a type is an assertion about memory the checker
  > cannot see. Spelling it `=>` misreports which of the two forms is doing the
  > work, and hides the most consequential unchecked operation in the language
  > from the audit that exists to find it.

### Auto-dereference is one level (D-098)

`.` dereferences a pointer once. `p.x` where `p` is `T->` reaches the field;
`pp.x` where `pp` is `T->->` is an error telling the reader to write `(<-pp).x`.

Peeling until a struct appears would make the number of indirections invisible at
the use site — `pp.x` and `p.x` would read identically while doing different
amounts of work, and widening a declaration from `T->` to `T->->` would leave
every use compiling. The same rule applies to UFCS: `q.method()` peels exactly one
level, because `q.x` does.

`any->` has no members at any level.

### `unknown` — Layer 2 Safety Taint

- Not a type the user can write directly — it's a compiler-assigned taint
- Assigned to the `value` field when `fail(errCode)` is used
- Propagates through operations: `unknown + 1` → result is also `unknown`
- Must be cleared by checking `Result.is_error` first

  > ⚠️ **Corrected (D-097).** This read "cleared via `ok(val)` or by checking
  > `Result.is_error` first". `ok` tested the **user-writable** `unknown` that
  > D-007 removed, so it was an operator whose subject no longer existed, and it
  > has been removed from the language. Checking `is_error` is the remaining
  > route, and it was always the one that composed with the `Result` discipline.
- IR: uses `undef` value with taint metadata in debug builds

---

## 28. Operator Reference

> Complete listing of all Nitpick operators and their lowering.

### Arithmetic
| Operator | Meaning | IR | Notes |
|---|---|---|---|
| `+` | add | `add`/`fadd` | |
| `-` | subtract | `sub`/`fsub` | |
| `*` | multiply | `mul`/`fmul` | |
| `/` | divide | `sdiv`/`fdiv` | div-by-zero → failsafe |
| `%` | remainder | `srem`/`frem` | |
| `**` | power | library call | Tier 1 |
| `<=>` | spaceship | `icmp`+select | Returns -1/0/1 |

### Bitwise
| Operator | Meaning | IR | Notes |
|---|---|---|---|
| `&` | bitwise AND | `and` | |
| `\|` | bitwise OR | `or` | |
| `^` | bitwise XOR | `xor` | |
| `~` | bitwise NOT | `xor %v, -1` | |
| `<<` | left shift | `shl` | |
| `>>` | right shift (signed) | `ashr` | |
| `>>>` | right shift (unsigned) | `lshr` | |

### Comparison
| Operator | Meaning | IR | Notes |
|---|---|---|---|
| `==` | equal | `icmp eq`/`fcmp oeq` | Scalars, pointers, and an `Optional` against `NIL`. **A struct, array, `Result`, `string` or `dyn` does not compare with `==`** (D-169; refused at the checker from 1.0.9c, a named rung until then) — implement `Eq` and call `a.eq(b)`; `string_eq` for strings. |
| `!=` | not equal | `icmp ne`/`fcmp one` | |
| `<` | less than | `icmp slt`/`fcmp olt` | |
| `<=` | less or equal | `icmp sle`/`fcmp ole` | |
| `>` | greater than | `icmp sgt`/`fcmp ogt` | |
| `>=` | greater or equal | `icmp sge`/`fcmp oge` | |

### Logical
| Operator | Meaning | IR | Notes |
|---|---|---|---|
| `&&` | logical AND | `and i1` (short-circuit) | Both sides bool |
| `\|\|` | logical OR | `or i1` (short-circuit) | Both sides bool |
| `!` | logical NOT | `xor i1 %v, true` | |

### Pointer / Address / Dereference
| Operator | Meaning | IR | Notes |
|---|---|---|---|
| `@val` | address-of | returns alloca ptr | val must be lvalue |
| `<-ptr` | dereference | `load T, ptr %ptr` | ptr must be pointer type |
| `ptr->field` | member via ptr | `getelementptr` + `load` | ptr must be struct ptr |
| `val.field` | direct member | `getelementptr` + `load` | val must be struct value |

### Result / Safety
| Operator | Meaning | IR | Notes |
|---|---|---|---|
| `?` | safe unwrap with default | branch + select | `res ? default` |
| `?!` | emphatic unwrap | branch → failsafe | No default |
| `??` | null coalesce | branch + select | `opt ?? default` |
| `?.\|` | safe navigation | branch + select | `opt?.field` |
| `\|>` | pipe forward | `call f(%v)` | `v \|> f` = `f(v)` |
| `<\|` | pipe backward | `call f(%v)` | `f <\| v` = `f(v)` |

### Cast / Conversion
| Operator | Meaning | IR | Notes |
|---|---|---|---|
| `expr => T` | checked cast | `sext`/`zext`/`trunc`/`sitofp`/... | Bounds checked |
| `expr =>! T` | unchecked cast | same but no bounds check | TOS auditable |

### Range
| Operator | Meaning | Notes |
|---|---|---|
| `a..b` | inclusive range [a, b] | Used in `for`, `pick` patterns |
| `a...b` | exclusive range [a, b) | Used in `for`, `pick` patterns |

### Ternary
| Operator | Meaning | IR | Notes |
|---|---|---|---|
| `is (cond) : then : else` | ternary/conditional | `select i1 %cond, %then, %else` | NOT `? :` syntax |

### String / Template
| Operator | Meaning | Notes |
|---|---|---|
| `` `text &{expr}` `` | template literal | Interpolation via `&{ }` |
| `r"raw"` | raw string | No escape processing |
| `"""triple"""` | triple-quoted string | Multiline, preserves indentation |

---

## Type Implementation Priority

| Priority | Types | Cycle |
|---|---|---|
| **P0 — Bootstrap** | bool, int8/16/32/64, uint8/16/32/64, char8, string, ptr | 0-1 |
| **P1 — Core** | flt32/flt64, arrays, structs, enums, Result\<T\>, Optional | 2-3 |
| **P2 — Advanced** | Handle\<T\>, arena\<T\>->, tbb8/16/32/64, tfp32/64 | 4-5 |
| **P3 — Specialized** | int128+, tfp128/256, dim256, char16/32, string\<char16/32\>, simd, atomic | 6 |
| **P4 — Extended** | vec/matrix/tensor/vec9/tmatrix/ttensor, trit/tryte/nit/nyte, dyn Trait, Future | 6-7 |
| **P5 — Scientific** | frac8/16/32/64, complex\<T\>, buffer | 7+ |

