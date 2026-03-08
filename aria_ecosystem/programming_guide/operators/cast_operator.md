# The `=>` Operator (Type Cast / Coerce)

**Category**: Operators → Type System  
**Syntax**: `expr => TypeName`  
**Purpose**: Explicit type cast (coercion) — convert a value to a different type  
**Decided**: March 6, 2026

---

## Overview

The **cast operator** (`=>`) converts a value from one type to another.  It is a
true infix operator: the source value is on the left, the target type is on the
right.

```aria
a => int8      // "a becomes int8"
```

**Operator family** (all infix, no parentheses):

| Operator | Meaning |
|----------|---------|
| `->` | Pointer type (int64->:ptr) / member access (ptr->field) |
| `<-` | Pointer dereference (`value <- ptr`) |
| `@` | Address-of (`@variable`) |
| `=>` | Type cast / coerce (`a => int8`) |

`=>` is **NOT** a lambda operator. Lambda syntax is just `returnType(params) { body }`.

---

## Basic Syntax

```aria
// Narrow: wider type → narrower type
int32:a = 300i32;
int8:b  = a => int8;    // 300 wraps to 44 (truncated)

// Widen: narrower type → wider type
int8:x  = 42i8;
int64:y = x => int64;   // 42 (sign-extended)

// Float conversions
float64:pi = 3.14159f64;
float32:pf = pi => float32;

// Integer ↔ float
int32:i = 7i32;
float64:f = i => float64;   // 7.0
int32:back = f => int32;    // 7 (truncated towards zero)
```

---

## Precedence

`=>` binds **tighter than additive** operators (`+`, `-`) but **looser than unary**:

```aria
// Cast binds to its immediate left operand:
int8:c = b + a => int8;   // equivalent to: b + (a => int8)
                           //  casts a, then adds to b

// Use parens if you want to cast the whole expression:
int8:d = (b + a) => int8; // adds first, then casts
```

---

## Examples

### Example 1: Mixing int32 and int64 in arithmetic

```aria
int32:count = 100i32;
int64:total = 1000000i64;

// Must cast before mixing types
int64:result = count => int64 + total;   // ✅ explicit
```

### Example 2: Loop index to array subscript

```aria
// $ in loop/till is always int64 — capture it
loop(0, 16, 1) {
    int64:r = $;
    loop(0, 32, 1) {
        int64:c = $;
        int64:idx = r * 32i64 + c;
        // ...
    }
}
```

### Example 3: Narrowing for FFI / bitfield packing

```aria
int32:flags = build_flags();
int8:lo = flags => int8;         // low byte only
int8:hi = (flags >> 8i32) => int8;
```

### Example 4: Float → int truncation

```aria
float64:seconds = 3.75f64;
int64:whole = seconds => int64;  // 3 (truncated toward zero)
int64:rounded = (seconds + 0.5f64) => int64;  // 4
```

### Example 5: Chained casts

```aria
int8:tiny = 127i8;
int64:big = tiny => int32 => int64;  // chain: int8 → int32 → int64
```

---

## Type-Safety Rules

`=>` performs an explicit cast — Aria will not do this implicitly.

```aria
int32:a = 5i32;
int8:b  = a;          // ❌ ERROR: implicit narrowing forbidden
int8:b  = a => int8;  // ✅ OK: explicit narrowing allowed
```

Overflow/underflow during narrowing is **defined behaviour** (wrap, not UB):

| Source | Target | Behaviour |
|--------|--------|-----------|
| `300i32 => int8` | `44` | truncated (mod 256) |
| `-1i32 => uint8` | `255` | reinterpreted |
| `3.9f64 => int64` | `3` | truncated toward zero |
| `NaN => int64` | implementation-defined | use with care |

---

## Operator System Context

Aria's pointer and type system uses consistent infix operator notation:

```aria
int64->:ptr = @myVar;        // pointer type + address-of
int64:val   = <- ptr;        // dereference
int8:small  = myInt32 => int8; // cast
```

No function-call syntax (`cast<int8>(x)` or `int8(x)`) — operators are part of
the expression grammar.

---

## Common Pitfalls

### ❌ Using int32 to capture `$` in loops

```aria
loop(0, 10, 1) {
    int32:i = $;   // ❌ ERROR: $ is int64, not int32
    int64:i = $;   // ✅ correct
}
```

### ❌ Forgetting cast when mixing widths in arithmetic

```aria
int32:a = 5i32;
int64:b = 10i64;
int64:c = a + b;          // ❌ ERROR: type mismatch
int64:c = a => int64 + b; // ✅ cast first
```

---

## Related Topics

- [Type Annotation (`:`)](type_annotation.md) — declare variable type
- [Pointer Operator (`->`)](arrow.md) — pointer types
- [Dereference (`<-`)](../memory_model/dereference.md) — dereference pointer
- [Dollar Variable (`$`)](../control_flow/dollar_variable.md) — loop iterator (always `int64`)
- [Zero Implicit Conversion](../types/zero_implicit_conversion.md) — why explicit cast is required

---

**Status**: Decided March 6, 2026 — implementation pending  
**Specification**: aria_specs.txt — Operator List (`=>`) and Precedence Table (level 5.5)  
**Operator family**: `->` pointer, `<-` dereference, `@` address-of, `=>` cast
