#!/usr/bin/env python3
"""
aria_patch_corpus.py — Generate targeted training examples to fix known weaknesses:
  1. nit type — balanced nonary, range [-4, 4], NOT int64
  2. nyte type — 5-nit packed value, Wave9 compression
  3. for loop syntax — (i in lo..hi) and (i in lo...hi)
  4. Wave9 field types — nit, not int32
  5. wave_lerp and related wave functions
  6. Type annotation syntax correction (colon-style: type:name = value)
  7. Aria vs other language confusion (no ->, no fn keyword, no backtick interpolation)

Run:  python3 aria_patch_corpus.py > aria_patch_data.jsonl
"""
import json
import sys

def ex(instruction, output, weight=3.0, category="patch"):
    return {
        "instruction": instruction,
        "input": "",
        "output": output,
        "metadata": {"source": "patch_v2", "category": category, "confidence": "high"},
        "weight": weight,
    }

examples = []

# ============================================================
# SECTION 1: nit type — the most badly hallucinated area
# ============================================================

examples.append(ex(
    "What is the nit type in Aria? What are its valid values and internal representation?",
    """\
The `nit` type is Aria's **balanced nonary digit** — the fundamental unit of the ternary/nonary
type system. It is NOT an integer type like int64 or int32.

**Key facts:**
- Range: **-4 to +4** (nine possible values: -4, -3, -2, -1, 0, 1, 2, 3, 4)
- Total distinct states: **9** (not 2^N like binary types)
- Storage: stored as `i8` (one byte) in the compiler's IR, but semantically constrained to [-4, 4]
- Special sentinel: `-128` (i8 MIN) represents an error/overflow state
- Represents a single "digit" in base-9 balanced numeral system

**Declaring nit variables:**
```aria
nit:a = 3;       // valid: 3 is in [-4, 4]
nit:b = -4;      // valid: minimum value
nit:c = 4;       // valid: maximum value
nit:zero = 0;    // valid: neutral element
// nit:d = 5;    // ERROR: 5 is out of range
```

**Arithmetic is saturating — result stays in [-4, 4]:**
```aria
nit:x = 3;
nit:y = 4;
nit:sum = x + y;   // = 4 (saturated, not 7)
```

**nit is used as the field type in Wave9 structs:**
```aria
struct:Wave9 = {
    nit:r;  nit:s;  nit:t;   // neuromodulation axes
    nit:u;  nit:v;  nit:w;   // quantum axes
    nit:x;  nit:y;  nit:z;   // spatial axes
};
```

**nit is NOT int64, int32, or any standard binary integer type.**
""",
    weight=5.0,
))

examples.append(ex(
    "Is nit an integer type like int64 in Aria?",
    """\
No. `nit` is completely different from integer types like `int64`, `int32`, etc.

- `int64` = 64-bit signed binary integer, range -9,223,372,036,854,775,808 to +9,223,372,036,854,775,807
- `nit` = balanced nonary digit, range **-4 to +4** (nine values only)

The confusion may arise because `nit` is stored as an `i8` (byte) internally for efficiency,
but it is logically constrained to nine values. It is not a "universal size" type or any
general-purpose integer.

```aria
// CORRECT: nit is a nonary digit
nit:coord = 2;      // spatial coordinate value
nit:signal = -3;    // neuromodulation state

// WRONG: do not use nit for large numbers
// nit:big = 100;  // would overflow/error
```

Use `int32` or `int64` for general arithmetic. Use `nit` for Wave9 components,
quantum coordinates, and other balanced nonary computations.
""",
    weight=5.0,
))

examples.append(ex(
    "Show me how to declare and use nit variables in Aria.",
    """\
```aria
func:main = int32() {
    // Basic nit declarations — range is [-4, 4]
    nit:a = 2;
    nit:b = -3;
    nit:c = 0;
    nit:max_val = 4;
    nit:min_val = -4;

    // Cast to int32 for printing or arithmetic with ints
    int32:a_int = @cast<int32>(a);

    // Nit arithmetic (saturating: result clamped to [-4, 4])
    nit:sum = a + b;      // 2 + (-3) = -1
    nit:neg = -a;         // -2

    // Comparison
    bool:eq = (a == b);
    bool:gt = (a > b);

    pass(0);
};
```

The nine valid values for `nit` are: **-4, -3, -2, -1, 0, 1, 2, 3, 4**
""",
    weight=4.0,
))

examples.append(ex(
    "What are the ternary/nonary types in Aria and how do they relate?",
    """\
Aria has a hierarchy of balanced nonary (base-9) types:

| Type    | Description                              | Range / Size                  |
|---------|------------------------------------------|-------------------------------|
| `nit`   | Single balanced nonary digit             | [-4, 4] — 9 values            |
| `trit`  | Single balanced ternary digit            | [-1, 0, 1] — 3 values         |
| `nyte`  | Packed 5-nit value (like a "byte")       | 9^5 = 59,049 states           |
| `Wave9` | 9-dimensional nit vector                 | 9 nit fields (r,s,t,u,v,w,x,y,z) |

These are **not** standard binary types. They live in a separate type universe
designed for Nikola's quantum/neural computation model.

```aria
nit:coord = 3;          // one balanced nonary digit: -4..4
trit:flag = -1;         // one balanced ternary digit: -1, 0, or 1
nyte:packed = some_nyte_function();   // 5-nit packed value

Wave9:position = Wave9 {
    r: 0, s: 0, t: 0,
    u: 0, v: 0, w: 0,
    x: coord, y: 2, z: -1
};
```
""",
    weight=4.0,
))

examples.append(ex(
    "How do I cast a nit to int32 in Aria?",
    """\
Use `@cast<int32>()` to convert a `nit` value to `int32`:

```aria
func:main = int32() {
    nit:n = 3;
    int32:n_int = @cast<int32>(n);   // n_int = 3

    nit:neg = -4;
    int32:neg_int = @cast<int32>(neg);  // neg_int = -4

    // Common use: pass nit to a function expecting int32
    int32:r_val = @cast<int32>(some_wave.r);
    pass(0);
};
```

And to go the other way, cast int32 to nit — but the value must be in [-4, 4]:
```aria
int32:small = 2;
nit:as_nit = @cast<nit>(small);   // fine — 2 is in range
```
""",
    weight=3.5,
))

# ============================================================
# SECTION 2: nyte type
# ============================================================

examples.append(ex(
    "What is the nyte type in Aria?",
    """\
`nyte` is Aria's **packed 5-nit value** — analogous to a byte in binary systems,
but in balanced nonary arithmetic.

**Key facts:**
- Encodes 5 `nit` values in a single `uint16_t` (16-bit unsigned integer) internally
- Total states: 9^5 = **59,049** distinct values
- Used to compress Wave9 data (9D → 5D projection, lossy)
- Stored with a bias offset so all values are positive as a uint16

**Packing and unpacking:**
```aria
use "stdlib/wave.aria".*;

func:main = int32() {
    // Pack 5 nit values into a nyte
    nit:n0 = 1;  nit:n1 = 2;  nit:n2 = -1;  nit:n3 = 3;  nit:n4 = -2;
    nyte:packed = aria_pack_nyte(n0, n1, n2, n3, n4);

    // Unpack: get individual nit by index
    nit:first = aria_nyte_get_nit(packed, 0);   // = 1
    nit:second = aria_nyte_get_nit(packed, 1);  // = 2

    // Compress a Wave9 to nyte
    Wave9:w = wave_from_spatial(2, 3, -1);
    nyte:compressed = nyte_from_wave(w);

    // Decompress back (lossy — some dimensions discarded)
    Wave9:approx = wave_from_nyte(compressed);
    pass(0);
};
```
""",
    weight=4.0,
))

examples.append(ex(
    "How does nyte_from_wave work in Aria? What information is kept or lost?",
    """\
`nyte_from_wave(w)` compresses a 9-dimensional `Wave9` into a 5-nit `nyte` by
keeping the 5 highest-priority dimensions and discarding the rest.

**Encoding priority (what is kept):**
| nit index | Wave9 field | Meaning            |
|-----------|-------------|-------------------|
| 0         | x           | spatial X          |
| 1         | y           | spatial Y          |
| 2         | z           | spatial Z          |
| 3         | u           | quantum U          |
| 4         | r           | state R            |

**Discarded (set to 0 on reconstruction):** s, t, v, w

```aria
use "stdlib/wave.aria".*;

func:main = int32() {
    Wave9:w = wave_from_full(-2, 0, 0, 3, 0, 0, 1, 2, -1);
    //                        r  s  t  u  v  w  x  y   z
    nyte:packed = nyte_from_wave(w);

    // Reconstruct — s, t, v, w will be 0
    Wave9:approx = wave_from_nyte(packed);
    // approx: r=-2 (from n4), x=1 (n0), y=2 (n1), z=-1 (n2), u=3 (n3)
    // s=0, t=0, v=0, w=0 (lost)
    pass(0);
};
```
""",
    weight=3.5,
))

# ============================================================
# SECTION 3: Wave9 type and wave functions
# ============================================================

examples.append(ex(
    "Write the Wave9 struct definition and a function that creates a zero wave in Aria.",
    """\
```aria
// Wave9 struct — all fields are nit (balanced nonary digit, range [-4, 4])
struct:Wave9 = {
    nit:r;  // Neuromodulation axis 1
    nit:s;  // Neuromodulation axis 2
    nit:t;  // Temporal dimension
    nit:u;  // Quantum dimension 1
    nit:v;  // Quantum dimension 2
    nit:w;  // Quantum dimension 3
    nit:x;  // Spatial X
    nit:y;  // Spatial Y
    nit:z;  // Spatial Z
};

// Create a Wave9 with all components set to zero
pub func:wave_zero = Wave9() {
    pass(Wave9 {
        r: 0, s: 0, t: 0,
        u: 0, v: 0, w: 0,
        x: 0, y: 0, z: 0
    });
};
```

**Important:** Wave9 fields are `nit`, not `int32` or any other type.
""",
    weight=5.0,
))

examples.append(ex(
    "How do I create a Wave9 from spatial coordinates in Aria?",
    """\
Use `wave_from_spatial(x, y, z)` from the wave stdlib, or construct directly:

```aria
use "stdlib/wave.aria".*;

func:main = int32() {
    // Using stdlib function (takes nit arguments)
    nit:x = 3;
    nit:y = -2;
    nit:z = 1;
    Wave9:pos = wave_from_spatial(x, y, z);
    // pos.x=3, pos.y=-2, pos.z=1, all others=0

    // Direct struct literal
    Wave9:manual = Wave9 {
        r: 0, s: 0, t: 0,
        u: 0, v: 0, w: 0,
        x: 3, y: -2, z: 1
    };

    // Access fields
    nit:out_x = pos.x;   // = 3
    int32:x_int = @cast<int32>(out_x);

    pass(0);
};
```
""",
    weight=4.0,
))

examples.append(ex(
    "How do I interpolate between two Wave9 values in Aria?",
    """\
Use `wave_lerp(a, b, t)` from the wave stdlib. `t` is an `int32` percentage from 0 to 100
(0 = pure `a`, 100 = pure `b`, 50 = midpoint).

```aria
use "stdlib/wave.aria".*;

func:main = int32() {
    Wave9:start = wave_from_spatial(0, 0, 0);
    Wave9:finish = wave_from_spatial(4, -4, 2);

    // Midpoint (t=50)
    int32:half = 50;
    Wave9:mid = wave_lerp(start, finish, half);
    // mid.x = 2, mid.y = -2, mid.z = 1

    // 75% toward finish (t=75)
    int32:t75 = 75;
    Wave9:near_end = wave_lerp(start, finish, t75);

    // t=0 → returns start exactly
    int32:t0 = 0;
    Wave9:at_start = wave_lerp(start, finish, t0);

    // t=100 → returns finish exactly
    int32:t100 = 100;
    Wave9:at_end = wave_lerp(start, finish, t100);

    pass(0);
};
```

Note: `t` is `int32` (0–100), NOT `flt64`. The result fields are `nit` values.
""",
    weight=5.0,
))

examples.append(ex(
    "What is the correct type for Wave9 fields in Aria?",
    """\
Wave9 fields are **`nit`** — balanced nonary digits with range **[-4, 4]**.

```aria
struct:Wave9 = {
    nit:r;  nit:s;  nit:t;   // NOT int32, NOT int8, NOT flt64
    nit:u;  nit:v;  nit:w;
    nit:x;  nit:y;  nit:z;
};
```

When you access a field, you get a `nit`:
```aria
Wave9:w = wave_from_spatial(2, -1, 3);
nit:x_coord = w.x;             // type is nit
int32:x_int = @cast<int32>(w.x);  // cast to int32 for arithmetic
```

**Common mistake:** Using `int32` for Wave9 fields produces a type error.
The correct types are always `nit` for all nine dimensions.
""",
    weight=5.0,
))

# ============================================================
# SECTION 4: For loop syntax
# ============================================================

examples.append(ex(
    "How do I write a for loop in Aria? Show inclusive and exclusive range examples.",
    """\
Aria uses `for (var in range)` syntax with two range operators:
- `a..b` — **inclusive** range: iterates a, a+1, ..., b
- `a...b` — **exclusive** range: iterates a, a+1, ..., b-1

```aria
func:main = int32() {
    // Inclusive range: prints 1 2 3 4 5
    for (i in 1..5) {
        string:s = int64_toString(i);
        drop(println(s));
    }

    // Exclusive range: prints 1 2 3 4
    for (i in 1...5) {
        string:s = int64_toString(i);
        drop(println(s));
    }

    // With variables
    int64:lo = 0;
    int64:hi = 3;
    for (i in lo..hi) {   // prints 0 1 2 3
        string:s = int64_toString(i);
        drop(println(s));
    }

    pass(0);
};
```

**With type annotation on the loop variable:**
```aria
for (int32:j in 0..9) {
    // j has type int32
}

for (int64:i in 1..100) {
    // i has type int64
}
```

**Note:** The range operator `..` is inclusive. `...` is exclusive. This is the
*opposite* of Rust's `..` (exclusive) and `..=` (inclusive). Do not confuse them.
""",
    weight=5.0,
))

examples.append(ex(
    "What is the difference between .. and ... in Aria for loops?",
    """\
In Aria's `for` loops:

- `a..b` — **inclusive**: iterates from `a` up to and including `b`
- `a...b` — **exclusive**: iterates from `a` up to but NOT including `b`

```aria
// 1..5 → 1, 2, 3, 4, 5  (includes 5)
for (i in 1..5) { /* runs 5 times */ }

// 1...5 → 1, 2, 3, 4    (excludes 5)
for (i in 1...5) { /* runs 4 times */ }
```

This is **unlike Rust** where `..` is exclusive and `..=` is inclusive.
In Aria, `..` is INCLUSIVE.
""",
    weight=4.0,
))

examples.append(ex(
    "Show me a for loop that sums numbers in Aria.",
    """\
```aria
func:main = int32() {
    int32:sum = 0;

    // Sum 1 through 10 (inclusive range)
    for (i in 1..10) {
        int32:val = @cast<int32>(i);
        sum = sum + val;
    }
    // sum = 55

    pass(sum);
};

func:failsafe = NIL(int32:err_code) { pass(NIL); };
```

Or with an explicit typed loop variable:
```aria
func:sum_range = int32(int32:n) {
    int32:total = 0;
    for (int32:i in 1..n) {
        total = total + i;
    }
    pass(total);
};
```
""",
    weight=4.0,
))

examples.append(ex(
    "Does Aria have Rust-style fn functions or -> return types?",
    """\
No. Aria uses a completely different function syntax — it does NOT use `fn`, `->`,
or any Rust-like notation.

**Correct Aria function syntax:**
```aria
func:function_name = return_type(param_type:param_name) {
    // body
    pass(return_value);
};
```

**Wrong (Rust-style — do NOT use in Aria):**
```aria
// fn add(a: int32, b: int32) -> int32  ← WRONG, this is Rust
// fn wave_lerp(a: Wave9, b: Wave9, t: f64) -> Wave9  ← WRONG
```

**Correct examples:**
```aria
func:add = int32(int32:a, int32:b) {
    pass(a + b);
};

func:greet = string(string:name) {
    pass(string_concat("Hello, ", name));
};

func:wave_scale = Wave9(Wave9:w, nit:factor) {
    // ...
    pass(result);
};
```

The pattern is always: `func:name = ReturnType(ParamType:paramName, ...) { ... };`
""",
    weight=5.0,
))

examples.append(ex(
    "Does Aria use backtick string interpolation like `&{var}`?",
    """\
No. Aria does not use backtick string interpolation or `&{var}` syntax.

**To build formatted strings in Aria, use `string_concat`:**
```aria
func:main = int32() {
    int32:x = 42;
    string:x_str = int32_toString(x);
    string:msg = string_concat("Value is: ", x_str);
    drop(println(msg));
    pass(0);
};
```

**Or concatenate step by step:**
```aria
string:result = "N5(";
result = string_concat(result, int32_toString(n0));
result = string_concat(result, ",");
result = string_concat(result, int32_toString(n1));
result = string_concat(result, ")");
```

There is no template literal / interpolation syntax in Aria. Always use
`string_concat` and explicit conversion functions like `int32_toString`.
""",
    weight=4.0,
))

# ============================================================
# SECTION 5: Variable declaration and type annotation
# ============================================================

examples.append(ex(
    "What is the correct variable declaration syntax in Aria?",
    """\
Aria uses **`type:name = value`** (colon between type and name):

```aria
int32:x = 10;
string:msg = "hello";
bool:flag = true;
nit:coord = 3;
Wave9:pos = wave_zero();
flt64:pi = 3.14159;
```

**The pattern is: `Type:variableName = initializer;`**

Common mistakes from other languages:
```aria
// WRONG (Python-style):  x: int = 10
// WRONG (C-style):       int x = 10;
// WRONG (Rust-style):    let x: i32 = 10;
// CORRECT (Aria):        int32:x = 10;
```

Inside function bodies, every variable needs a type annotation:
```aria
func:example = int32() {
    int32:count = 0;
    string:name = "aria";
    bool:done = false;
    nit:a = 2;
    nit:b = -1;
    pass(count);
};
```
""",
    weight=4.0,
))

examples.append(ex(
    "How do I declare a struct type in Aria?",
    """\
Use `struct:Name = { field_type:field_name; ... };`:

```aria
struct:Point2D = {
    flt64:x;
    flt64:y;
};

struct:Color = {
    uint8:r;
    uint8:g;
    uint8:b;
};

// Wave9 uses nit fields (balanced nonary digits):
struct:Wave9 = {
    nit:r;  nit:s;  nit:t;
    nit:u;  nit:v;  nit:w;
    nit:x;  nit:y;  nit:z;
};
```

**Creating struct instances with a struct literal:**
```aria
Point2D:origin = Point2D { x: 0.0, y: 0.0 };
Wave9:zero = Wave9 { r: 0, s: 0, t: 0, u: 0, v: 0, w: 0, x: 0, y: 0, z: 0 };
```

**Accessing fields:**
```aria
flt64:px = origin.x;
nit:wx = zero.x;
```
""",
    weight=4.0,
))

# ============================================================
# SECTION 6: Result type / pass / fail / failsafe
# ============================================================

examples.append(ex(
    "How does error handling work in Aria? What is the Result type?",
    """\
Aria functions implicitly return a `Result<T>` wrapping every return value.
You use `pass(value)` to return success and `fail(error_code)` to return failure.
Every program must define a `failsafe` function to handle uncaught errors.

```aria
func:divide = int32(int32:a, int32:b) {
    if (b == 0) {
        fail(1);   // error code 1 = division by zero
    }
    pass(a / b);
};

func:main = int32() {
    int32:result = divide(10, 2);   // = 5
    pass(0);
};

// Required: handles unhandled errors at the program level
func:failsafe = NIL(int32:err_code) {
    pass(NIL);
};
```

`pass(NIL)` is used when the return type is `NIL` (void-like).
`fail(N)` propagates an error with integer code N.
""",
    weight=3.5,
))

# ============================================================
# SECTION 7: Complete worked examples combining the above
# ============================================================

examples.append(ex(
    "Write a complete Aria program that creates two Wave9 values, lerps between them, and prints the result.",
    """\
```aria
use "stdlib/wave.aria".*;

func:failsafe = NIL(int32:err_code) { pass(NIL); };

func:main = int32() {
    // Create wave at spatial position (4, 4, 4)
    nit:n4 = 4;
    Wave9:a = wave_from_spatial(n4, n4, n4);

    // Create a zero wave
    Wave9:b = wave_zero();

    // Lerp halfway between b (zero) and a (4,4,4): expect (2,2,2)
    int32:t = 50;
    Wave9:mid = wave_lerp(b, a, t);

    // Print result
    string:s = wave9_toString(mid);
    drop(println(s));    // prints: W9(0,0,0,0,0,0,2,2,2)

    pass(0);
};
```

Key points:
- Wave9 fields are `nit` (range [-4, 4]), so 4 is the maximum spatial coordinate
- `wave_lerp` takes `int32` percentage (0–100), not a float
- `wave_zero()` creates a Wave9 with all fields = 0
- `wave9_toString` formats as `W9(r,s,t,u,v,w,x,y,z)`
""",
    weight=5.0,
))

examples.append(ex(
    "Write an Aria function that counts how many nit values in a Wave9 are positive.",
    """\
```aria
func:count_positive_dims = int32(Wave9:w) {
    int32:count = 0;
    nit:zero = 0;

    if (w.r > zero) { count = count + 1; }
    if (w.s > zero) { count = count + 1; }
    if (w.t > zero) { count = count + 1; }
    if (w.u > zero) { count = count + 1; }
    if (w.v > zero) { count = count + 1; }
    if (w.w > zero) { count = count + 1; }
    if (w.x > zero) { count = count + 1; }
    if (w.y > zero) { count = count + 1; }
    if (w.z > zero) { count = count + 1; }

    pass(count);
};
```

Or using a for loop doesn't directly work over struct fields — Aria structs don't
have built-in iteration. You access each field by name directly.
""",
    weight=3.5,
))

examples.append(ex(
    "Write an Aria for loop that builds a string of comma-separated numbers from 1 to 5.",
    """\
```aria
use "stdlib/string_utils.aria".*;

func:main = int32() {
    string:result = "";
    bool:first = true;

    for (i in 1..5) {
        int32:val = @cast<int32>(i);
        string:num = int32_toString(val);
        if (first) {
            result = num;
            first = false;
        } else {
            result = string_concat(result, ",");
            result = string_concat(result, num);
        }
    }

    drop(println(result));   // prints: 1,2,3,4,5
    pass(0);
};

func:failsafe = NIL(int32:err_code) { pass(NIL); };
```

Note: `i` in `for (i in 1..5)` has type `int64` by default.
Use `@cast<int32>(i)` when you need `int32`.
""",
    weight=4.0,
))

examples.append(ex(
    "In Aria, what is the difference between pass() and return? Which one should I use?",
    """\
Use `pass()` — Aria does NOT have a `return` keyword.

```aria
func:add = int32(int32:a, int32:b) {
    pass(a + b);   // CORRECT: use pass()
    // return a + b;   ← WRONG: Aria has no return keyword
};

func:void_func = NIL() {
    // do some work...
    pass(NIL);   // CORRECT: NIL is Aria's void
    // return;  ← WRONG
};
```

`pass(value)` signals successful completion and returns the value.
`fail(code)` signals an error with an integer error code.

Every non-NIL function path must end with `pass(value)`.
""",
    weight=4.0,
))

examples.append(ex(
    "Show a complete Aria program with a while loop and a for loop.",
    """\
```aria
func:failsafe = NIL(int32:err_code) { pass(NIL); };

func:main = int32() {
    // While loop: count down from 5
    int32:n = 5;
    while (n > 0) {
        n = n - 1;
    }
    // n = 0 now

    // For loop with inclusive range: sum 1..10
    int32:sum = 0;
    for (i in 1..10) {
        int32:val = @cast<int32>(i);
        sum = sum + val;
    }
    // sum = 55

    // For loop with exclusive range: 0, 1, 2, 3, 4
    int32:product = 1;
    for (int32:j in 1...6) {
        product = product * j;
    }
    // product = 120 (= 5!)

    pass(sum);
};
```
""",
    weight=4.0,
))

# ============================================================
# SECTION 8: Common anti-patterns / corrections
# ============================================================

examples.append(ex(
    "What NOT to do in Aria — common mistakes from developers coming from other languages",
    """\
**1. Don't use `fn` or `->` (Rust-style):**
```aria
// WRONG:  fn lerp(a: Wave9) -> Wave9 { ... }
// RIGHT:  func:lerp = Wave9(Wave9:a) { ... };
```

**2. Don't use `nit` for large numbers:**
```aria
// WRONG:  nit:count = 1000;   // out of range, nit is [-4, 4]
// RIGHT:  int32:count = 1000;
```

**3. Don't confuse .. (inclusive) with Rust's .. (exclusive):**
```aria
// In Aria: 1..5 = 1,2,3,4,5  (INCLUSIVE)
// In Rust: 1..5 = 1,2,3,4    (exclusive)
// Aria exclusive is: 1...5
```

**4. Don't use backtick string interpolation:**
```aria
// WRONG:  print(`value is &{x}`)
// RIGHT:  print(string_concat("value is ", int32_toString(x)))
```

**5. Don't use `return` — use `pass`:**
```aria
// WRONG:  return 42;
// RIGHT:  pass(42);
```

**6. Wave9 fields are nit, not int32:**
```aria
// WRONG:  struct:Wave9 = { int32:x; int32:y; ... }
// RIGHT:  struct:Wave9 = { nit:x; nit:y; ... }
```

**7. Don't omit the `func:failsafe` — it's required for error handling:**
```aria
func:failsafe = NIL(int32:err_code) { pass(NIL); };
```
""",
    weight=5.0,
))

examples.append(ex(
    "Write a Wave9 distance comparison function in Aria without using sqrt.",
    """\
```aria
// Returns true if wave |a - b|² < threshold²
// Avoids sqrt by comparing squared distances
func:wave_within_distance = bool(Wave9:a, Wave9:b, int32:threshold_sq) {
    int32:dr = @cast<int32>(a.r - b.r);
    int32:ds = @cast<int32>(a.s - b.s);
    int32:dt = @cast<int32>(a.t - b.t);
    int32:du = @cast<int32>(a.u - b.u);
    int32:dv = @cast<int32>(a.v - b.v);
    int32:dw = @cast<int32>(a.w - b.w);
    int32:dx = @cast<int32>(a.x - b.x);
    int32:dy = @cast<int32>(a.y - b.y);
    int32:dz = @cast<int32>(a.z - b.z);

    int32:dist_sq = dr*dr + ds*ds + dt*dt + du*du + dv*dv + dw*dw
                  + dx*dx + dy*dy + dz*dz;

    pass(dist_sq < threshold_sq);
};
```

Usage:
```aria
Wave9:a = wave_from_spatial(1, 2, 3);
Wave9:b = wave_from_spatial(1, 2, 3);
bool:same = wave_within_distance(a, b, 1);  // threshold_sq = 1 means dist < 1
```
""",
    weight=3.5,
))

# Write output
for e in examples:
    print(json.dumps(e))

print(f"# Generated {len(examples)} patch examples", file=sys.stderr)
