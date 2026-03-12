# Aria Language Quick Reference

**Purpose:** One-line lookup for every type, operator, construct, and stdlib
module in Aria. No elaboration — just the facts.

**Full spec:** `.internal/aria_specs.txt`  
**Full guide:** `aria_ecosystem/programming_guide/`

---

## Table of Contents

1. [Primitive Numeric Types](#1-primitive-numeric-types)
2. [Specialized Numeric Types](#2-specialized-numeric-types)
3. [Non-Numeric Primitives](#3-non-numeric-primitives)
4. [Compound & Generic Types](#4-compound--generic-types)
5. [Sentinel & Special Values](#5-sentinel--special-values)
6. [Pointer System (Blueprint Style)](#6-pointer-system-blueprint-style)
7. [Memory & Allocation](#7-memory--allocation)
8. [Declarations](#8-declarations)
9. [Functions & Closures](#9-functions--closures)
10. [Generics & Contracts](#10-generics--contracts)
11. [Control Flow](#11-control-flow)
12. [Operators](#12-operators)
13. [Literal Syntax & Type Suffixes](#13-literal-syntax--type-suffixes)
14. [Error Handling — Three Safety Layers](#14-error-handling--three-safety-layers)
15. [Result\<T\> API](#15-resultt-api)
16. [String Interpolation](#16-string-interpolation)
17. [I/O Streams](#17-io-streams)
18. [Modules & FFI](#18-modules--ffi)
19. [Async / Concurrency](#19-async--concurrency)
20. [Stdlib Modules](#20-stdlib-modules)
21. [Builtins & Allocator Functions](#21-builtins--allocator-functions)

---

## 1. Primitive Numeric Types

### Signed Integers
| Type | Width | Notes |
|------|-------|-------|
| `int1` | 1-bit | |
| `int2` | 2-bit | |
| `int4` | 4-bit | |
| `int8` | 8-bit | |
| `int16` | 16-bit | |
| `int32` | 32-bit | common default |
| `int64` | 64-bit | |
| `int128` | 128-bit | |
| `int256` | 256-bit | |
| `int512` | 512-bit | |
| `int1024` | 1024-bit | LBIM — 16×64-bit limbs; post-quantum crypto (RSA-4096) |
| `int2048` | 2048-bit | LBIM — 32×64-bit limbs; quantum-resistant (RSA-8192) |
| `int4096` | 4096-bit | LBIM — 64×64-bit limbs; max quantum resistance (limit) |

### Unsigned Integers
Same widths as signed: `uint1` through `uint4096` (same LBIM rules for ≥1024).

### IEEE Floating Point (platform-dependent rounding — use tfp for determinism)
| Type | Width |
|------|-------|
| `flt32` | 32-bit IEEE float |
| `flt64` | 64-bit IEEE double |
| `flt128` | 128-bit extended |
| `flt256` | 256-bit |
| `flt512` | 512-bit |

---

## 2. Specialized Numeric Types

### TBB — Twisted Balanced Binary (safety integers)
Symmetric range; min value is ERR sentinel; sticky error propagation.

| Type | Range | ERR |
|------|-------|-----|
| `tbb8` | −127 to +127 | −128 (0x80) |
| `tbb16` | −32767 to +32767 | −32768 (0x8000) |
| `tbb32` | ±2 147 483 647 | INT32_MIN |
| `tbb64` | symmetric 64-bit | INT64_MIN |

> TBB widening **must** use `tbb_widen<T>(val)` — sign-extension corrupts the ERR sentinel.

### TFP — Twisted Floating Point (deterministic IEEE replacement)
Software-implemented; bit-exact across all platforms; no `−0`, no NaN; unified ERR.

| Type | Notes |
|------|-------|
| `tfp32` | 16-bit exponent + 16-bit mantissa (tbb16+tbb16) |
| `tfp64` | 16-bit exponent + 48-bit mantissa (tbb16+tbb48) |

### fix256 — Deterministic Fixed-Point
Q128.128 format; 256-bit (128 integer + 128 fractional); no floating-point drift; bit-identical cross-platform; precision ≈ 2⁻¹²⁸; ERR sentinel = high limb set.  
Supports optional dimensional unit parameter: `fix256<Joules>`, `fix256<Meters>`, etc. — compiler enforces dimensional equations.

### frac — Exact Rational Arithmetic
Mixed-number struct: `{ tbb_N:whole; tbb_N:num; tbb_N:denom }`. Inherits TBB sticky errors.

| Type | Component |
|------|-----------|
| `frac8` | built on `tbb8` |
| `frac16` | built on `tbb16` |
| `frac32` | built on `tbb32` |
| `frac64` | built on `tbb64` |

### Balanced Non-Binary
| Type | Description |
|------|-------------|
| `trit` | Balanced ternary digit (−1, 0, 1); stored in i8; ERR = −128 |
| `tryte` | 10 trits (3¹⁰ values); stored in uint16 |
| `nit` | Balanced nonary digit (−4..+4); stored in i8; ERR = −128 |
| `nyte` | 5 nits (9⁵ values); stored in uint16 |

Literal prefixes: `0t` for ternary (T = −1), `0n` for nonary (A–D = −1..−4).

---

## 3. Non-Numeric Primitives

| Type | Description |
|------|-------------|
| `bool` | Boolean; `true` / `false` |
| `string` | Managed text string |
| `tensor` | N-dimensional numeric tensor |
| `matrix` | 2D matrix |
| `vec2` | 2-component SIMD vector |
| `vec3` | 3-component SIMD vector |
| `vec9` | 9-component SIMD vector (for 9D physics grids) |
| `dyn` | Dynamic/runtime type |
| `obj` | Generic object (any struct) |
| `binary` | Raw binary data |
| `buffer` | Byte buffer |
| `stream` | Data stream |
| `process` | OS process handle |
| `pipe` | IPC pipe |

---

## 4. Compound & Generic Types

| Type | Description |
|------|-------------|
| `struct:Name = { type:field; }` | Value struct type |
| `struct<T>:Name = { *T:field; }` | Generic struct; `*T` references the type param |
| `type[N]:arr` | Fixed-size array with N elements |
| `type[]:arr` | Dynamic/unsized array |
| `func` | Function type / function-pointer variable |
| `(RetT)(ParamT)` | Anonymous function pointer type |
| `Result<T>` | Error-or-value wrapper; fields: `value`, `err`, `is_error` |
| `T?` | Optional type; may hold `T` or `NIL` |
| `complex<T>` | Complex number `{ *T:real; *T:imag }` with arithmetic, conjugate, magnitude |
| `atomic<T>` | Lock-free concurrent wrapper; default ordering: SeqCst |
| `simd<T, N>` | N-lane SIMD vector (N = 2/4/8/16/32/64); maps to AVX-512/AVX2/NEON |
| `Handle<T>` | Generational arena handle `{ uint64:index; uint32:generation }` — detects use-after-free on realloc |
| `arena<T>` | Arena allocator; `alloc()`, `get(Handle)`, `free(Handle)`, `grow()` |

---

## 5. Sentinel & Special Values

| Value | Type | Description |
|-------|------|-------------|
| `NIL` | Unit/optional | Aria's "no value" — for `T?` optional types only |
| `NULL` | Pointer | C-style null pointer (address 0x0) — for `type->` pointer types only |
| `ERR` | Type-specific | Type-specific error sentinel; sticky over arithmetic |
| `unknown` | Any | Soft error state returned instead of crashing (e.g., division by zero → `unknown`) |
| `void` | FFI only | C "no return value" marker — **forbidden in Aria-native code** |

> `NIL` → optional types only. `NULL` → pointer types only. Mixing is a compile error.

---

## 6. Pointer System (Blueprint Style)

| Syntax | Meaning |
|--------|---------|
| `int64->:ptr` | Declare a pointer-to-int64 variable (arrow points **to** the type) |
| `@var` | Address-of operator — get pointer to `var` |
| `<-ptr` | Dereference — pull value **from** pointer |
| `ptr->member` | Member access through pointer (arrow points **to** the field) |
| `#var` | Pin — prevents GC from moving `var` in memory; restricts mutable borrows |
| `$var` | Mutable borrow reference |
| `!$var` | Immutable borrow reference |

> C-style `*` pointer syntax is **only** used inside `extern { }` blocks for FFI compatibility.

---

## 7. Memory & Allocation

### Allocation Modes

| Keyword | Behavior | Use when |
|---------|----------|----------|
| `stack` | Stack-allocated (default for locals) | local vars, small structs, perf-critical |
| `gc` | GC heap — auto-freed when no refs remain | complex/shared ownership |
| `wild` | Manual heap — must `aria.free()` | real-time, deterministic, embedded |
| `wildx` | Executable memory heap | JIT code generation only |

### Resource Management

| Construct | Description |
|-----------|-------------|
| `defer stmt;` | Run `stmt` when scope exits — all paths including error returns |
| `defer { ... }` | Deferred block |
| Multiple defers | Execute in LIFO order (reverse declaration order) |

---

## 8. Declarations

```aria
type:name = value;              // variable
const type:NAME = value;        // compile-time constant
```

Type annotation is `type:name` — the colon separates type from name, always.

---

## 9. Functions & Closures

### Named Function
```aria
func:name = ReturnType(type:param, type:param) {
    pass(value);   // success return
    fail(error);   // error return
};
```

All functions implicitly return `Result<ReturnType>`.

### Special Functions
```aria
// Entry point — auto-unwraps Result to raw int32 for OS
func:main = int32(type:arg...) { pass(0i32); };

// Mandatory safety net — must be defined; compilation fails without it
func:failsafe = void(int32:err_code) { /* cleanup + terminate */ };
```

### Anonymous Function (Closure/Lambda)
```aria
ReturnType(type:param) { body }               // closure literal
ReturnType(type:param) { body }(arg, arg)      // immediate execution
```

No `=>` for closures — `=>` is the **cast operator**.

### Function Pointer Type
```aria
(ReturnType)(ParamType1, ParamType2):name      // type annotation for func pointer var
```

### Capture Semantics
- Value capture (copy): safe, no lifetime concern
- Reference capture (pointer): `int32->:ref = @var;` captured manually

### Async Function
```aria
async func:name = ReturnType(type:param) { await expr; };
```

---

## 10. Generics & Contracts

### Generic Functions
```aria
func<T>:name = *T(*T:val) { pass(val); };     // T = type param; *T = use of T
ReturnType:r = name::<int32>(42);             // turbofish explicit type
ReturnType:r = name(42);                      // type inferred
```

### Generic Structs
```aria
struct<T>:Box = { *T:value; int64:timestamp; };
Box<int32>:b = { value: 42, timestamp: 0i64 };
```

### Contracts (Design by Contract)
```aria
func:divide = int32(int32:a, int32:b)
    requires b != 0              // precondition — checked before body
    ensures result != ERR        // postcondition — checked on return
{
    pass(a / b);
};
```

| Keyword | Description |
|---------|-------------|
| `requires` | Function precondition |
| `ensures` | Function postcondition |
| `invariant` | Loop invariant (syntax reserved, implementation deferred) |
| `result` | Special binding in `ensures` clauses — holds return value |

---

## 11. Control Flow

### Conditionals
```aria
if (cond) { } else if (cond) { } else { }

is cond : true_val : false_val              // ternary — all three parts required
```

### Loops
```aria
while (cond) { }                            // standard while

for (int32:i = 0i32; i < 100i32; i++) { }  // C-style for

// 'when' loop — structured conditional loop with completion/failure branches
when (cond) { body } then { /* ran to completion */ } end { /* exited early */ }

// 'loop' — Aria counted loop; $ is always int64 iteration variable
loop (start, limit, step) { int64:i = $; }
// step is magnitude only; direction determined by start vs limit

// 'till' — Aria zero-based counted loop; $ is always int64
till (limit, step) { int64:i = $; }
// positive step: 0 → limit; negative step: limit → 0
```

### Pick (Pattern Matching Switch)
```aria
pick (value) {
    (< 9)    { fall(low); },        // value < 9
    (> 9)    { fall(high); },       // value > 9
    (9)      { fall(exact); },      // value == 9
    (*)      { fall(other); },      // catch-all
    low: (!) { /* handle */; },     // labeled target
    high:(!) { /* handle */; },
    exact:(!)  { /* handle */; },
    other:(!)  { /* handle */; },
}
```

| Keyword | Description |
|---------|-------------|
| `fall(label)` | Explicit fallthrough to named label in `pick` |
| `(*)` | Catch-all arm |
| `(!)` | Label target arm (unreachable by direct match — only via `fall`) |

### Other
```aria
break                               // exit loop
continue                            // next iteration
async { await expr; }               // async block
```

---

## 12. Operators

### Arithmetic
| Op | Meaning |
|----|---------|
| `+` `-` `*` `/` `%` | standard arithmetic |
| `++` `--` | increment / decrement |
| `+=` `-=` `*=` `/=` `%=` | compound assignment |
| `&=` `\|=` `^=` | compound bitwise assignment |

### Comparison
| Op | Meaning |
|----|---------|
| `==` `!=` `<` `>` `<=` `>=` | standard comparison — returns `bool` |
| `<=>` | spaceship — returns −1/0/+1 (`tbb` type) |

### Logical & Bitwise
| Op | Meaning |
|----|---------|
| `&&` `\|\|` `!` | logical and/or/not |
| `&` `\|` `^` `~` | bitwise and/or/xor/not |
| `<<` `>>` | bit shift |

### Pointer & Memory
| Op | Meaning |
|----|---------|
| `->` | pointer type declaration AND member access |
| `@` | address-of |
| `<-` | pointer dereference |
| `#` | memory pin |
| `$` | iteration variable / mutable borrow ref |
| `!$` | immutable borrow ref |

### Result / Error
| Op | Meaning |
|----|---------|
| `?` | safe unwrap: `expr ? default` |
| `?!` | emphatic unwrap: `expr?!` — calls `failsafe(err)` on failure |
| `??` | null coalescing: `optional ?? fallback` |
| `?.` | safe navigation: `obj?.field` (⚠️ token recognized, codegen pending) |

### Other
| Op | Meaning |
|----|---------|
| `=>` | cast/coerce: `val => int8` — "val becomes int8" |
| `\|>` | pipeline forward: `v \|> f \|> g` ≡ `g(f(v))` (⚠️ type-check done, codegen WIP) |
| `<\|` | pipeline backward: `f <\| v` ≡ `f(v)` (⚠️ same status) |
| `..` | inclusive range: `0..10` |
| `...` | exclusive range: `0...10` |
| `is` | ternary condition keyword: `is cond : a : b` |
| `!!!` | direct failsafe call: `!!! ERR_CODE` — emergency termination |
| `:` | type annotation separator (not an operator — binds in declarations) |
| `&{}` | template interpolation inside backtick strings |

### Operator Precedence (1 = highest)
1. Primary: `() [] . -> <-`
2. Unary: `! - + @ # !!!`
3. Dereference: `<-`
4. Multiplicative: `* / %`
5. Additive: `+ -`
5.5. Cast: `=>`
6. Bitshift: `<< >>`
7. Relational: `< <= > >= <=>`
8. Equality: `== !=`
9. Bitwise AND: `&`
10. Bitwise XOR: `^`
11. Bitwise OR: `|`
12. Logical AND: `&&`
13. Logical OR: `||`
14. Pipeline: `|> <|`
15. Result unwrap: `? ?? ?! ?.`
16. Range: `.. ...`
17. Ternary: `is : :`
18. Assignment: `= += -= ...`
19. Comma: `,`

---

## 13. Literal Syntax & Type Suffixes

### Number Suffixes (required when literal would overflow declared type)
| Suffix | Type | Example |
|--------|------|---------|
| `u8` `u16` `u32` `u64` `u128` `u256` `u512` | unsigned int | `255u8` |
| `u1024` `u2048` `u4096` | LBIM unsigned | `12345u1024` |
| `i8` `i16` `i32` `i64` `i128` `i256` `i512` | signed int | `-40i8` |
| `i1024` `i2048` `i4096` | LBIM signed | `12345i1024` |
| `f32` `f64` `f128` `f256` `f512` | IEEE float | `3.14f64` |
| `fix256` | fixed-point | `10.5fix256` |
| `tbb8` `tbb16` `tbb32` `tbb64` | TBB | `100tbb8` |
| `tf` | twisted float (tfp64 literal) | `3.14tf` |

> 2048/4096-bit literals use string parsing: `parse_int2048("1.5e308")`.

### Other Literal Forms
| Literal | Description |
|---------|-------------|
| `0xFF` `0b1010` `0o755` | hex / binary / octal integer |
| `"text"` `'text'` | regular string (no interpolation) |
| `` `text &{expr}` `` | template literal — supports interpolation |
| `true` `false` | boolean |
| `NIL` | optional no-value |
| `NULL` | null pointer |
| `ERR` | type-specific error sentinel |
| `'a'` | char literal |
| `{field: val}` | struct literal |
| `[a, b, c]` | array literal |
| `0t1T0` | balanced ternary literal (T = −1) |
| `0n2A` | balanced nonary literal (A = −1) |

### Zero Implicit Conversion Policy
No implicit numeric widening or coercion between types. Use `=>` for explicit casts.  
Exception: suffix may be omitted when literal fits natively in the declared type.

---

## 14. Error Handling — Three Safety Layers

### Layer 1: `Result<T>` — Hard errors (must be handled)
Functions return `Result<T>`; caller must unwrap or propagate. See §15.

### Layer 2: `unknown` — Soft errors (crash-inducing ops → recoverable state)
Operations that would crash in other languages return `unknown` instead:
- Division by zero → `unknown`
- Array out of bounds → `unknown`
- Integer overflow (checked mode) → `unknown`
- Null pointer dereference → `unknown`
- `sqrt(-1)` → `unknown`

Check: `ok(val)` → `1` if valid, `0` if unknown  
Pass upward: `pass(ok(val))` strips `unknown` taint and explicitly acknowledges the state

### Layer 3: `failsafe()` — Final safety net (mandatory)
Every program **must** define `failsafe` or compilation fails:
```aria
func:failsafe = void(int32:err_code) {
    // emergency cleanup + controlled shutdown
};
```

Triggered by:
- `expr?!` — emphatic unwrap on error
- `!!! ERR_CODE` — direct failsafe call (operator form)
- Errors propagating to top level (future)

---

## 15. Result\<T\> API

| Syntax | Description |
|--------|-------------|
| `result<T>:r = func()` | typed result variable |
| `r.is_error` | `bool` — true if error |
| `r.err` | error info (valid when `is_error` is true) |
| `raw(r)` | extract value — **no** `is_error` check; caller takes responsibility |
| `drop(r)` | explicitly discard result without checking (TOS explicit bypass) |
| `pass(value)` | success return from a function |
| `fail(error)` | error return from a function |
| `ok(val)` | check/strip `unknown` state: 1=valid, 0=unknown |
| `expr ? default` | unwrap with fallback default |
| `expr?!` | emphatic unwrap — calls `failsafe(err)` on error |
| `optional ?? fallback` | null coalesce for `T?` optional types |
| `expr?` | propagate error upward (only valid inside `Result`-returning function) |

---

## 16. String Interpolation

Template literals use backticks; embed expressions with `&{expr}`:

```aria
`The value is &{x}`
`Sum: &{a + b}`
`Status: &{is ok : "good" : "bad"}`
`User: &{user?.name ?? "Guest"}`
```

Escape sequences: `\n` `\t` `\"` `\'` `` \` `` `\\`  
Multiline: content between backticks may span lines.

---

## 17. I/O Streams

| Stream | Direction | Content |
|--------|-----------|---------|
| `stdin` | in | text input |
| `stdout` | out | normal text output |
| `stderr` | out | error messages |
| `stddbg` | out | debug output |
| `stddati` | in | binary data input |
| `stddato` | out | binary data output |

---

## 18. Modules & FFI

### Importing
```aria
use std.io;                         // module
use std.io.{println, readFile};     // selective
use math.*;                         // wildcard
use "./utils.aria" as utils;        // local with alias
use cfg(target_os = "linux") std.os.linux;  // conditional
use cfg(feature = "net") std.net;
```

### Defining
```aria
mod name { pub func:api; func:internal; }   // inline module
mod network;                                 // file-based (network.aria)
pub func:name = ...                          // public visibility
```

### Extern (C FFI)
```aria
extern "libc" {
    func:malloc = void*(uint64:size);   // C-style * for FFI only
    func:free   = void(void*:ptr);
    func:printf = int32(string:fmt, ...);
}
```

> Pointer types inside `extern` blocks use C-style `*`. All Aria-native code uses `->`.

---

## 19. Async / Concurrency

| Feature | Description |
|---------|-------------|
| `async func:name = T(...) { }` | async function |
| `await expr` | await a future/promise |
| `catch` | error handling in async context |
| `atomic<T>` | lock-free atomic variable |
| `atomic<T>:v.load_acquire()` | acquire-ordered load |
| `atomic<T>:v.load_relaxed()` | relaxed load (explicit low-ordering) |
| `atomic<T>:v.store_release(val)` | release-ordered store |
| `atomic<T>:v.store_relaxed(val)` | relaxed store |
| `atomic<T>:v.swap_acqrel(val)` | atomic swap |
| `compare_exchange_weak_acqrel(...)` | CAS with spurious-fail possibility |
| `fetch_add_relaxed(val)` | atomic fetch-and-add |
| `atomic_fence_acquire()` | acquire fence |
| `atomic_fence_release()` | release fence |
| `atomic_fence_seqcst()` | sequential consistency fence |
| `sync` | synchronization primitive marker keyword |

Default ordering for `atomic<T>`: **SeqCst** (sequential consistency).

---

## 20. Stdlib Modules

### lib/std/ (Standard Library)

| Module | Description |
|--------|-------------|
| `lib/std/algorithms` | Quicksort, search, GCD, LCM, comparison utilities |
| `lib/std/arrays` | Array fill, min/max, search helpers |
| `lib/std/async` | Async runtime: `Future<T>`, work-stealing scheduler, M:N threading, RAMP optimization |
| `lib/std/bits` | Bit manipulation: popcount, bit-set, bitwise ops on integers |
| `lib/std/bits_int32` / `bits_uint32` / `bits_int_only` / `bits_uint_only` | Type-specific bit utilities |
| `lib/std/class` | OOP macro system — expands to zero-overhead struct + func definitions |
| `lib/std/collections` | `Vec`, `HashMap`, `Set` and other data structures (generic, opt-out GC) |
| `lib/std/compare` | Comparison and ordering utilities |
| `lib/std/convert` | Type conversion utilities |
| `lib/std/cstring` | C null-terminated string utilities (for FFI) |
| `lib/std/file` | File I/O operations (`open`, `read`, `write`, `close`) |
| `lib/std/float` | Float utilities: `lerp`, `approx_equal`, `min`, `max`, `clamp` |
| `lib/std/fs` | File system: stat, mkdir, rm, path ops with Result-based error handling |
| `lib/std/hash` | Hash functions: multiplicative, DJB2, combine |
| `lib/std/int` | Integer utilities: `abs`, digit extraction, digit count |
| `lib/std/io` | I/O: print, println, read, write (wraps C stdio) |
| `lib/std/logic` | Boolean utilities: `and`, `or`, `xor`, `not` |
| `lib/std/math` | Extended math: transcendentals, interpolation, rounding, modular arithmetic |
| `lib/std/mem` | Memory utilities: `memcpy`, `memmove`, `memset` (C wrappers with auto-conversion) |
| `lib/std/numeric` | Extended numeric utilities |
| `lib/std/path` | Cross-platform path manipulation: `basename`, `dirname`, `join`, `normalize` |
| `lib/std/process` | Process management with six-stream I/O topology |
| `lib/std/random` | Random number generation |
| `lib/std/result` | Standard error codes and result helpers |
| `lib/std/string` | String formatting, parsing, number-from-string, advanced string ops |
| `lib/std/sys` | System calls: `exit`, `getenv`, platform info |
| `lib/std/time` | Timestamp, duration, date formatting |

### lib/std/core/ (Core Allocators)

| Module | Description |
|--------|-------------|
| `lib/std/core/arena` | Arena allocator implementation |
| `lib/std/core/memory` | Core memory management primitives |
| `lib/std/core/pool` | Object pool allocator |
| `lib/std/core/result` | Core Result<T> type definitions |
| `lib/std/core/slab` | Slab allocator |

### stdlib/ (Extended / Domain Modules)

| Module | Description |
|--------|-------------|
| `stdlib/atomic` | Lock-free atomic ops using Type-Oriented Programming (UFCS style) |
| `stdlib/atomic/int32` | Atomic int32 specialization |
| `stdlib/complex` | Complex number arithmetic (add, mul, conjugate, magnitude) |
| `stdlib/core` | Core language builtins |
| `stdlib/dbug` | Debug logging with groups, levels, and structured output |
| `stdlib/number` | Number utilities (formatting, conversion) |
| `stdlib/quantum` | Quantum physics operations (UFIE wave equation support) |
| `stdlib/string_builder` | Efficient string building with method chaining (UFCS) |
| `stdlib/string_convert` | String↔type conversion: `int_to_str`, `str_to_int`, etc. |
| `stdlib/traits/numeric` | Numeric trait definitions |
| `stdlib/traits/numeric_impls` | Numeric trait implementations |
| `stdlib/wave` | Wave function operations |
| `stdlib/wavemech` | Advanced wave mechanics (UFIE, phase, amplitude) |

### Hashmap & Vec Specializations (stdlib/)
Type-specific concrete implementations while generics mature:
- `stdlib/lib_hashmap_int8_int8`
- `stdlib/lib_hashmap_int32_int64`
- `stdlib/lib_hashmap_int64_int64`
- `stdlib/lib_vec_int32`

---

## 21. Builtins & Allocator Functions

### Safety & Error
| Builtin | Description |
|---------|-------------|
| `ok(val)` | Test/strip `unknown`: returns 1 if valid, 0 if unknown |
| `raw(expr)` | Extract `Result<T>` value without `is_error` check (TOS bypass — caller's responsibility) |
| `drop(expr)` | Discard `Result` without checking (TOS explicit bypass — you own the consequences) |
| `pass(value)` | Return success from current function |
| `fail(error)` | Return error from current function |
| `failsafe(err_code)` | Directly invoke failsafe handler |

### Memory Allocation
| Function | Description |
|----------|-------------|
| `aria.alloc<T>(count)` | Allocate `count` elements of `T` on wild (manual) heap |
| `aria.free(ptr)` | Free wild-allocated memory |
| `aria.gc_alloc<T>(count)` | Allocate on GC heap |
| `aria.alloc_buffer(size)` | Allocate raw byte buffer |
| `aria.alloc_string(size)` | Allocate string buffer |
| `aria.alloc_array<T>(count)` | Allocate typed array |
| `aria.alloc_executable(size)` | Allocate executable memory (wildx / JIT) |
| `aria.free_executable(ptr)` | Free executable memory |

### I/O
| Function | Description |
|----------|-------------|
| `print(str)` | Write to stdout (no newline) |
| `println(str)` | Write to stdout with newline |
| `readFile(path)` | Returns `Result<string>` |
| `writeFile(path, content)` | Returns `Result<NIL>` |
| `readJSON(path)` | Returns `Result<obj>` |
| `readCSV(path)` | Returns `Result<array>` |
| `openFile(path, mode)` | Returns `Result<stream>` |

### Process
| Function | Description |
|----------|-------------|
| `spawn(cmd, args)` | Returns `Result<process>` |
| `fork()` | Returns `Result<obj>` with `is_child` / `child_pid` |
| `exec(cmd, args)` | Replace current process |
| `createPipe()` | Returns `pipe` for IPC |
| `wait(pid)` | Wait for process, returns exit code |

### Functional / Array
| Function | Description |
|----------|-------------|
| `filter(arr, len, pred)` | Filter array with predicate function |
| `transform(arr, len, fn)` | Map function over array |
| `reduce(arr, len, init, fn)` | Fold/accumulate |
| `map(arr, len, fn)` | Alias for transform |
| `sort(arr)` | Sort array |
| `reverse(arr)` | Reverse array |
| `unique(arr)` | Deduplicate |

### Networking
| Function | Description |
|----------|-------------|
| `httpGet(url)` | HTTP GET; returns `Result<obj>` |

### System
| Function | Description |
|----------|-------------|
| `getMemoryUsage()` | Returns memory usage |
| `getActiveConnections()` | Returns connection count |
| `createLogger(name)` | Returns structured `log` instance |
| `computeOptimalSize()` | Compile-time size computation |

---

## Key Design Principles

| Principle | Expression |
|-----------|------------|
| Safety first | Three-layer error system prevents undefined behavior |
| No implicit conversion | Every type mismatch is a compile error |
| Explicit allocation | No hidden heap allocations |
| Blueprint pointers | Arrows show data flow direction semantically |
| Errors are data | `Result<T>` flows through the type system |
| Compiler-enforced accountability | `failsafe()` required; `raw()`/`drop()` require deliberate choice |
| Zero-cost abstractions | Generics monomorphized; `Result<T>` single-branch overhead |

---

*Last updated: March 2026. Source of truth: `.internal/aria_specs.txt`*
