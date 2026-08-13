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
- Arithmetic: `+`, `-`, `*`, `/`, `%` — all use **checked (safe) variants** by default
  - Safe add: `call {iN, i1} @llvm.sadd.with.overflow.iN(iN %a, iN %b)` → check overflow bit → failsafe on overflow
  - When Z3/Rules prove no overflow: plain `add iN %a, %b`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=` → `icmp eq/ne/slt/sgt/sle/sge`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>` → `and`, `or`, `xor`, `shl`, `ashr`
- Casting: explicit only (`x => int64`, `@cast_unchecked<int32>(y)`)
- Literal suffixes: `42i32`, `-1i8`, `0xFF_i64`

**LLVM IR pattern (safe add):**
```llvm
; int32:result = a + b;  (checked)
%pair = call {i32, i1} @llvm.sadd.with.overflow.i32(i32 %a, i32 %b)
%val = extractvalue {i32, i1} %pair, 0
%overflow = extractvalue {i32, i1} %pair, 1
br i1 %overflow, label %failsafe_trap, label %continue
```

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
  - Overflow checking uses `@llvm.uadd.with.overflow.iN` etc.
- Literal suffixes: `42u32`, `0xFFu8`

> **Note:** `uint8` and `char8` share the same LLVM IR type (`i8`) but are **semantically distinct**. The type checker enforces different operation sets. See §2 for char types.

### 1.4 IEEE Floating Point

| Nitpick Type | LLVM IR Type | Size | Alignment |
|---|---|---|---|
| `flt32` | `float` | 4 bytes | 4 |
| `flt64` | `double` | 8 bytes | 8 |
| `flt128` | `fp128` | 16 bytes | 16 |

**Behaviors:**
- Arithmetic: `+`, `-`, `*`, `/`, `%` → `fadd`, `fsub`, `fmul`, `fdiv`, `frem`
- No overflow checking (IEEE 754 handles inf/nan)
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=` → `fcmp oeq/one/olt/ogt/ole/oge`
- Math functions (sin, cos, sqrt, etc.) via Nitpick stdlib wrapping LLVM intrinsics
- Literal suffixes: `3.14flt32`, `2.718flt64`, bare `3.14` defaults to `flt64`

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

// To create a C-compatible string (null-terminated char array):
char8[]:cstr = as_cstring("Hello");  // Produces ['H','e','l','l','o','\0']
// as_cstring works with string literals, char arrays, or Nitpick strings

// char arrays are NOT strings. This is a compile error:
// string:s = hello;  // ERROR: cannot assign char8[] to string
```

### 2.4 Advanced Native Primitives (Hardware/Optimization Targets)

Nitpick includes several domain-specific native primitives designed for aggressive LLVM IR optimization, specifically targeting nonary logic simulation and high-performance tensor/matrix operations without the overhead of library abstractions.

| Type | Description | Optimization Target |
|---|---|---|
| `trit` | A base-3 unit of information (values: -1, 0, 1 or 0, 1, 2). | Bit-packed arrays, ternary logic gates. |
| `tryte` | A block of 6 trits (values: 0 to 728). | Registers and nonary byte equivalents. |
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
| `toCharArray` | `(string) → char8[]` | Convert to character array (copy) |
| `fromCharArray` | `(char8[]) → string` | Convert char array to string (copy) |
| `as_cstring` | `(string) → char8[]` | **Builtin**: Produces null-terminated char8 array |

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

## 4. Large Integers — LBIM (Tier 0 layout, Tier 1 operations)

The Limb-Based Integral Model represents integers > 64 bits as arrays of `i64` limbs.
This works around LLVM backend bugs with native `i128`/`i256` types.

| Nitpick Type | LLVM IR Type | Size | Limbs | Alignment |
|---|---|---|---|---|
| `int128` | `{i64, i64}` | 16 bytes | 2 | 8 |
| `uint128` | same as signed | 16 bytes | 2 | 8 |
| `uint4096` | `{i64 x 64}` | 512 bytes | 64 | 8 | (standard for cryptographic modular arithmetic)

**Behaviors:**
- Arithmetic: ripple-carry add, borrow-chain sub, schoolbook mul, runtime div/mod
- Comparison: limb-by-limb from MSL (most significant limb) to LSL
- Signed vs unsigned distinguished at operation level (sdiv vs udiv, slt vs ult)

**LLVM IR pattern (LBIM struct):**
```llvm
; int128 = 2-limb struct
%LBIM_128 = type { i64, i64 }   ; limb[0] = low, limb[1] = high

; int128:x = 42i128;
; → promote literal to 2-limb: {42, 0}
%x = alloca %LBIM_128, align 8
%low_ptr = getelementptr %LBIM_128, ptr %x, i32 0, i32 0
store i64 42, ptr %low_ptr
%high_ptr = getelementptr %LBIM_128, ptr %x, i32 0, i32 1
store i64 0, ptr %high_ptr
```

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
| `tfp128` | `{i64, i64}` | 16 bytes | Q64.64 | 8 | 64-bit integer + 64-bit fraction |
| `tfp256` | `{i64, i64, i64, i64}` | 32 bytes | Q128.128 | 8 | 128-bit integer + 128-bit fraction |

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
  - **ERR-aware semantics:** Operations that overflow produce the `ERR` sentinel (most negative value); comparisons propagate ERR — `ERR == anything` is always false, `ERR != anything` is always true.
- Shift: `<<`, `>>` (on raw bits — useful for scaling)
- Remainder: `%` (on raw bits)
- Unary negation: `-val`
- Bitwise: `&`, `|`, `^`, `~` (on raw representation)

**`tfp256`-specific intrinsics:**
```nitpick
tfp256:x = 3.7tfp256;
tfp256:floored = tfp256_floor(x);   // -> 3.0tfp256
tfp256:trunced = tfp256_trunc(x);   // -> 3.0tfp256 (toward zero)
```

**Cast support:**
```nitpick
tfp256:f = 42.5tfp256;
int64:i   = @cast<int64>(f);    // truncates to 42
flt64:fl  = @cast<flt64>(f);    // 42.5 (nearest representable)
tfp64:f64 = f => tfp64;         // narrow cast (precision loss)
```

---

### 5a. Dimensional Analysis Type (`dim256<Unit>`)

> **Dimensional analysis is a `dim256`-exclusive feature.** Only `dim256` supports
> the `<Unit>` annotation. Narrower fixed-point types do NOT support dimensional annotations.

**Syntax:**
```nitpick
dim256<Joules>:energy = 1000.0dim256<Joules>;
dim256<Meters>:dist   = 5.0dim256<Meters>;
dim256<Seconds>:time  = 2.0dim256<Seconds>;
```

**Supported unit dimensions** (compile-time annotations only — erased at IR level):
| Keyword | Physical Quantity |
|---|---|
| `Joules` | Energy |
| `Meters` | Length / Distance |
| `Seconds` | Time |
| `Newtons` | Force |
| `Kelvin` | Temperature |

> **Runtime representation:** `dim256<Joules>` is IDENTICAL to bare `tfp256` at the
> LLVM IR level. The unit annotation is purely compile-time metadata — there is NO
> runtime overhead.

**Dimensional algebra enforcement (compile time):**
```nitpick
// Units are tracked through arithmetic:
dim256<Meters>:dist  = 10.0dim256<Meters>;
dim256<Seconds>:time = 5.0dim256<Seconds>;
dim256<Meters>:speed = dist / time;      // ERROR: Meters/Seconds != Meters
                                          // Compiler: "dimensional mismatch: expected
                                          //   Meters, got Meters/Seconds"

// Multiply creates compound units (checked against expected type):
dim256<Joules>:work = force * dist;      // OK: Newtons * Meters = Joules (if registered)
```

**Rules:**
- Adding/subtracting same unit: ✅ result has same unit
- Adding/subtracting different units: ❌ compile-time error
- Multiplying/dividing units: ✅ compiler tracks compound unit algebra
- Comparing different units: ❌ compile-time error
- Assigning bare `tfp256` to dimensional: ⚠️ warning (unit loss)
- Assigning dimensional to bare `tfp256`: ✅ allowed (drops unit annotation)

**In functions:**
```nitpick
func:velocity = dim256<Meters>(dim256<Meters>:d, dim256<Seconds>:t) {
    pass(d / t);    // Type error: d/t is Meters/Seconds, not Meters
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

| Nitpick Type | LLVM IR Type | Size | Alignment | Range |
|---|---|---|---|---|
| `tbb8` | `i8` | 1 byte | 1 | -128..127 (balanced) |
| `tbb16` | `i16` | 2 bytes | 2 | -32768..32767 |
| `tbb32` | `i32` | 4 bytes | 4 | ~±2.1B |
| `tbb64` | `i64` | 8 bytes | 8 | ~±9.2E18 |
| `tbb128` | `{i64, i64}` | 16 bytes | 8 | ~±1.7E38 |
| `tbb256` | `{i64 x 4}` | 32 bytes | 8 | ~±5.8E76 |

**Behaviors:**
- Arithmetic uses **safe variants** with error sentinel detection
- Error sentinel: minimum value of the type (e.g., `INT32_MIN` for tbb32)
- On overflow/error: result is set to sentinel, no crash
- Used primarily for function error codes and the `failsafe` signature


---

## 7. Exotic Types — Ternary/Nonary (Tier 0 layout, Tier 1 operations)

| Nitpick Type | LLVM IR Type | Size | Alignment | Base |
|---|---|---|---|---|
| `trit` | `i8` | 1 byte | 1 | Base-3 (-1, 0, 1) |
| `tryte` | `i16` | 2 bytes | 2 | 6 trits packed |
| `nit` | `i8` | 1 byte | 1 | Base-9 (-4..4) |
| `nyte` | `i16` | 2 bytes | 2 | Packed nits |

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

### 9.3 Enums (Tagged)

```nitpick
pub enum:Color = { Red = 0i32; Green = 1i32; Blue = 2i32; };
```

```llvm
; Enum values are plain i32 constants
; Color.Red = 0, Color.Green = 1, Color.Blue = 2

; Tagged enums with payloads:
; enum Shape = { Circle(flt64); Rect(flt64, flt64); };
%Shape = type { i32, [4 x i8], [16 x i8] }  ; tag + padding (for alignment) + max(payload sizes)
; Note: The compiler explicitly inserts padding arrays to ensure that 
; payload extraction does not cause unaligned reads/segfaults on strict architectures.
```

---

## 10. Pointer & Reference Types (Tier 0)

| Nitpick Syntax | LLVM IR | Description |
|---|---|---|
| `T->` | `ptr` | Pointer to T |
| `any->` | `ptr` | Type-erased pointer (void*) |

> **Pointer Syntax Rule:** Nitpick exclusively uses the `->` operator for pointer types (`int32->`). The C-style `*` pointer syntax (e.g. `void*`, `char*`) is **strictly forbidden** inside Nitpick code and is only permitted inside `extern { }` blocks to maintain C ABI definitions.
> - `@var` = address of
> - `<-ptr` = dereference pointer
> - `ptr.field` = unified member access (automatically dereferences if pointer)

All pointers are LLVM opaque `ptr` at the IR level. The distinction between
wild, borrow, and GC pointers is enforced entirely by the type checker — the LLVM IR is identical.

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

; NIL = { i8 0, i32 undef }
; Some(42) = { i8 1, i32 42 }
```

### 11.2 Result\<T\>

> **EVERY function in Nitpick returns `Result<T>`** except `pub func:main` and
> `pub func:failsafe` (which return raw `int32` and use `exit()`).
> This is NOT optional — the compiler enforces it. `<T>` is implied from the
> function's declared return type.

**Struct definition (canonical field names):**
```nitpick
struct<T>:Result = {
    T:value;        // Success value (zero-initialized if error — see fail desugar)
    tbb32:error;    // Error code (NIL if no error). Convention: < 0 system, > 0 user.
    bool:is_error;  // Error flag (false = success, true = error)
};
```

**LLVM IR layout:**
```llvm
; Result<T> = { T value, i32 error, i8 is_error }
; Example: Result<int32>
%Result_i32 = type { i32, i32, i8 }
; .value at offset 0
; .error at offset 4 (tbb32, 4 bytes)
; .is_error at offset 8 (bool, 1 byte)
; padding: 3 bytes
; Total: 12 bytes
```

**Creating Results — `pass` / `fail` / explicit `return`:**

| Syntax | Desugars to | Notes |
|---|---|---|
| `pass(retVal);` | `return Result{error: NIL, value: retVal, is_error: false};` | Success path |
| `fail(errCode);` | `return Result{error: errCode, value: zero, is_error: true};` | Error path — value is zero-initialized |
| `return Result{error: errCode, value: retVal, is_error: true};` | (literal, no desugar) | Special: return both value AND error |

**Unwrapping / accessing Result values:**

The compiler WILL NOT allow accessing `.value` without first checking `.is_error`
(or using one of the bypass operators). Available strategies:

| Operator | Syntax | Shorthand | Behavior |
|---|---|---|---|
| Safe unwrap | `expr ? defaultVal` | — | If error, use default value |
| Null coalesce | `expr ?? defaultVal` | — | If NIL (optional), use default |
| Emphatic unwrap | `expr ?! errCode` | — | If error, triggers failsafe (Layer 3) |
| Raw unwrap | `raw(expr)` or `raw expr` | `_!` | Extract `.value` WITHOUT checking error (unsafe, auditable "TOS keyword") |
| Drop | `drop(expr)` or `drop expr` | `_?` | Discard the entire Result, don't care about value or error |
| Discard | `discard(param)` | `_~` | For unused variables/params — semantically different from `drop` |

> **`raw` vs `drop` vs `discard`:** These are three DISTINCT concepts:
> - `raw` = "give me the value, I don't care about the error" (bypasses safety)
> - `drop` = "I don't need the value OR the error" (throws away the Result entirely)
> - `discard` = "I have this parameter/variable but I'm not going to use it" (silences unused warnings)

**FFI (extern) behavior:**
```nitpick
extern {
    func:sqlite3_open = int32(int8->:filename, any->:db_out);
};
```
> **ALL functions (including `extern`), except `main` and `failsafe`, return `Result<T>`**.
> Even when calling C FFI functions via `extern`, the Nitpick compiler automatically wraps
> the C return value in a `Result<T>`. If the C function does not provide error information, 
> the result defaults to `Ok(val)`. This ensures consistency across the language: you never 
> have to guess whether a function call needs error handling or `raw`. If you do not care 
> about the error from an `extern` function, simply append `raw` to unwrap the value directly.
> This check can be optimized out at compile-time if `raw` is used, ensuring zero-overhead FFI. 

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

Per AGENTS.md rule: `atomic<T>` emits native LLVM atomic IR (no C shims).

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

---

## 15. Vector / Matrix / Tensor Types (Tier 1 — Nitpick structs)

| Nitpick Type | Backing | Layout |
|---|---|---|
| `vec2` | `{flt64, flt64}` | 16 bytes |
| `vec3` | `{flt64, flt64, flt64}` | 24 bytes |
| `vec4` | `{flt64, flt64, flt64, flt64}` | 32 bytes |
| `matrix<T>` | `{ptr, i32, i32}` (data, rows, cols) | 24 bytes |
| `tensor<T>` | `{ptr, ptr, i32}` (data, dims_array, ndims) | 24 bytes |

---

## 16. Function Types (Tier 0)

```llvm
; func:add = int32(int32:a, int32:b) { ... };
define {i32, ptr, i8} @add(i32 %a, i32 %b) {
  ; Returns Result<int32> by default
  ; {value, error, is_error}
}

; When Result elision proves function is infallible:
define i32 @add_elided(i32 %a, i32 %b) {
  ; Returns raw i32
}
```

---

## 17. Future\<T\> — Async Types (Tier 1)

```llvm
; future<T> is a coroutine handle + result slot
%Future = type { ptr, ptr }  ; {coroutine_handle, result_slot}
```

---

## 18. dyn Trait — Dynamic Dispatch (Tier 1)

```llvm
; Fat pointer: {data_ptr, vtable_ptr}
%DynTrait = type { ptr, ptr }

; Vtable: array of function pointers
%Describable_vtable = type { ptr }  ; one method → one fn ptr
```

---

## 19. Dimensional Types (Tier 1)

Dimensional analysis types carry unit metadata at compile time only.
At runtime they are the same as their base numeric type.

```nitpick
int32<Meters>:distance = 100i32<Meters>;
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
- Supports all `T` that support arithmetic: `flt32`, `flt64`, `fix32`, `fix64`

---

> **Memory Model Note (string, binary, buffer):**
> Nitpick supports multiple memory spaces: `stack` (default), `gc` (garbage collected), `wild` (unmanaged C-like memory), and `wildx` (JIT executable memory). 
> By default, `string` and `binary` are `gc`-managed (or stack allocated via escape analysis). `buffer` is often allocated in `wild` memory, hence the existence of manual `buffer_free()`. To safely interop between `gc` and `wild` memory, the pin operator `#` prevents the garbage collector from moving a pinned object.

## 22. binary — Raw Binary Data (Tier 0/1)

Immutable blob of binary data. Like `string` but without encoding semantics.

```llvm
; binary layout: struct { ptr data, i64 length }
; Same as string but WITHOUT capacity (always exact-sized)
%binary = type { ptr, i64 }
```

```nitpick
binary:b = load_file("data.bin");
int32:magic = binary_read_i32(b, 0i64);  // read first 4 bytes as int32
```

Operations:
- `binary_length(b)` — byte count
- `binary_byte_at(b, idx)` — single byte
- `binary_slice(b, start, len)` — sub-range (zero-copy, shares pointer)
- `binary_read_i32(b, offset)`, `binary_read_i64(b, offset)` — typed reads
- `binary_from_string(s)` — reinterpret string bytes as binary
- `binary_to_string(b)` — reinterpret binary as UTF-8 string (no copy)

---

## 23. buffer — Mutable Raw Memory Buffer (Tier 0/1)

Like `binary` but mutable. Used for I/O buffers, serialization, etc.

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
- `buffer_to_binary(buf)` — snapshot as immutable binary
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

- Represents "nothing" at the type level (like `void` in C, but as a value)
- Return type annotation for functions that produce no meaningful value: `func:f = NIL(...)`
- "void functions" DO NOT EXIST in Nitpick — they return `Result<NIL>` instead
- `pass(NIL)` desugars to `return Result{ value: NIL, error: NIL, is_error: false }`
- To call a NIL-returning function without checking: `drop(myFunc());`
- IR: `Result<NIL>` is `{ ptr undef, ptr null, i8 0 }` at the struct level

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
- Cast to concrete type via `@cast<T>(p)` before dereferencing

### `unknown` — Layer 2 Safety Taint

- Not a type the user can write directly — it's a compiler-assigned taint
- Assigned to the `value` field when `fail(errCode)` is used
- Propagates through operations: `unknown + 1` → result is also `unknown`
- Must be cleared via `ok(val)` or by checking `Result.is_error` first
- IR: uses `undef` value with taint metadata in debug builds

---

## 27. Operator Reference

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
| `==` | equal | `icmp eq`/`fcmp oeq` | |
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
| `@cast<T>(val)` | checked cast (verbose) | same as `=>` | |
| `@cast_unchecked<T>(val)` | unchecked cast (verbose) | same as `=>!` | TOS auditable |

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
| **P5 — Scientific** | frac8/16/32/64, complex\<T\>, binary, buffer | 7+ |

