# Nitpick Comptime Evaluation Guide

> Added in v0.20.3 — full struct/array support, local variables, recursion, memoization.

## Overview

`comptime(expr)` evaluates an expression at compile time. The result must be
a known constant value (integer, float, bool, string). Comptime evaluation
surfaces bugs earlier and enables zero-cost abstraction without runtime overhead.

## Comptime expressions

```nitpick
int32:n = comptime(2i32 + 3i32);      // n = 5, resolved at compile time
bool:ok = comptime(10i32 > 5i32);     // ok = true
```

All integer literals in comptime expressions must carry explicit type suffixes
(`1i32`, `0i64`, `255u8`, etc.) to avoid unintended type widening.

## Comptime functions

Declare a function as comptime-evaluable with the `comptime func:` prefix:

```nitpick
comptime func:double = int32(int32:x) {
    pass x * 2i32;
};
```

Invoke it inside `comptime(...)`:

```nitpick
int32:eight = comptime(double(4i32));   // 8
```

### Rules

- Must be declared before use (or be mutually recursive — see below).
- Body may contain local variable declarations, `if`/`pass`, and recursive calls.
- All branches must end with `pass value;` (or an earlier `pass`).
- `exit` is **not** allowed inside comptime functions.

## Local variables in comptime bodies

```nitpick
comptime func:hypotenuse_sq = int32(int32:a, int32:b) {
    int32:a2 = a * a;
    int32:b2 = b * b;
    pass a2 + b2;
};
```

Local variables are scoped to the comptime call frame and do not exist at runtime.

## Conditional branches

```nitpick
comptime func:maxval = int32(int32:a, int32:b) {
    if (a > b) {
        pass a;
    }
    pass b;
};

int32:m = comptime(maxval(3i32, 7i32));   // 7
```

## Recursive comptime functions

Recursive comptime functions are supported. Because `fib(n-1)` returns
`Result<int32>`, use `raw` to unwrap the result before arithmetic:

```nitpick
comptime func:fib = int32(int32:n) {
    if (n <= 1i32) {
        pass n;
    }
    int32:a = raw fib(n - 1i32);
    int32:b = raw fib(n - 2i32);
    pass a + b;
};

int32:f10 = comptime(fib(10i32));   // 55
```

> `raw expr` in a comptime context simply unwraps the `Result<T>` — it does
> not suppress runtime errors, because there is no runtime.

## Memoization

The evaluator automatically caches results keyed by `(function_name, args)`.
Repeated calls with the same arguments are free:

```nitpick
// Both calls below resolve the same cached result:
int32:a = comptime(fib(10i32));
int32:b = comptime(fib(10i32));   // cache hit — no re-evaluation
```

Cache keys use structural equality on `ComptimeValue` (integers, floats,
bools, strings, arrays element-wise, structs field-by-field).

## Comparison operators in comptime

All standard comparisons work in comptime: `==`, `!=`, `<`, `<=`, `>`, `>=`.
Composite types (arrays, structs) use element-wise / field-by-field ordering.

## Known limitations (v0.20.3)

- `pick` range arms are not exhaustiveness-checked for `int32`/`int64` domains
  (domain is treated as infinite). Always include a `(*)` wildcard arm when
  using range arms on these types. See `KNOWN_ISSUES.md` for details.
- Comptime cannot call `extern` functions or access global mutable state.
- Infinite recursion in comptime will exhaust the evaluator's call-depth limit
  and produce a compile-time error.
