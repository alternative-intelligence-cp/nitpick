# Closure Lifetime Guide — Nitpick v0.20.4

Nitpick supports first-class lambda functions that can capture variables from
their enclosing scope. This guide documents the capture semantics, lifetime
rules, and the ARIA-025 borrow error.

---

## Syntax

```nitpick
// Lambda type: (ReturnType)(ParamTypes...)
(int32)(int32):add_one = int32(int32:x) { pass x + 1i32; };
```

---

## Capture Semantics

When a lambda body references a variable from the enclosing scope, the
compiler runs `ClosureAnalyzer` during type-checking to determine the
capture mode:

| Mode           | When used                                             |
|----------------|-------------------------------------------------------|
| `BY_VALUE`     | Variable is read-only inside the lambda               |
| `BY_REFERENCE` | Variable is mutated inside the lambda                 |

Module-level `fixed` constants are **not** captured — they are globally
visible and do not require a capture slot.

```nitpick
fixed int32:LIMIT = 100i32;   // NOT captured — globally visible

func:main = int32() {
    int32:base = 10i32;       // captured BY_VALUE (read-only inside lambda)

    (int32)(int32):add = int32(int32:x) { pass x + base; };
    // base is captured; LIMIT is not captured
    exit 0;
};
```

---

## Lifetime Rules & ARIA-025

Captures by reference obey strict lifetime rules enforced by the borrow
checker. Violations emit error code **ARIA-025**.

### Rule 1: Lambda must not escape its capture scope

A lambda that captures a local variable BY_REFERENCE cannot be returned
from the function that owns that variable — the variable's stack frame will
be gone when the lambda is called.

```nitpick
// ERROR — ARIA-025: dangling reference capture
func:make_adder = (int32)(int32)() {
    int32:base = 10i32;
    (int32)(int32):add = int32(int32:x) { pass x + base; };
    pass add;   // ERROR: 'add' captures 'base' by reference and escapes
};
```

Fix: pass parameters explicitly (no capture needed for pure lambdas):

```nitpick
func:make_adder = (int32)(int32, int32)() {
    // Return a lambda that takes base as an explicit parameter
    pass int32(int32:x, int32:b) { pass x + b; };
};
```

### Rule 2: Lambda passed to function — potential escape warning

When a lambda capturing BY_REFERENCE is passed to another function, the
borrow checker emits a **warning** (ARIA-025, severity WARNING) because the
callee might store the lambda beyond the call's lifetime.

```nitpick
// WARNING — ARIA-025: potential dangling reference
func:apply = void((int32)(int32):f, int32:x) { ... };

func:main = int32() {
    int32:base = 5i32;
    (int32)(int32):add = int32(int32:x) { pass x + base; };
    apply(add, 3i32);  // WARNING: 'add' captures 'base' by reference
    exit 0;
};
```

### Rule 3: IIFE pattern is safe

Immediately-invoked function expressions (IIFEs) are safe — the lambda
never escapes:

```nitpick
func:main = int32() {
    int32:factor = 3i32;
    int32:result = (int32(int32:x) { pass x * factor; })(5i32) ? -1i32;
    // Safe: lambda is called immediately, never stored or returned
    exit 0;
};
```

---

## Pure Lambdas (No Captures)

Lambdas with no references to outer scope have no captured variables and
are always safe:

```nitpick
(int32)(int32, int32):multiply = int32(int32:x, int32:y) { pass x * y; };
int32:r = multiply(4i32, 5i32) ? -1i32;
```

---

## ARIA-025 Error Reference

| Code      | Severity | Message                                                |
|-----------|----------|--------------------------------------------------------|
| ARIA-025  | ERROR    | Closure captures local variable by reference and escapes function scope (dangling reference) |
| ARIA-025  | WARNING  | Closure captures local variable by reference and is passed to another function (potential dangling reference if callee stores closure) |

---

## See Also

- [OPTIONALS_GUIDE.md](OPTIONALS_GUIDE.md) — `optional<T>`, `?.`, `??`
- [MACRO_AUTHORING_GUIDE.md](MACRO_AUTHORING_GUIDE.md) — compile-time features
