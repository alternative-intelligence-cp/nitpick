# Optional\<T\> & Safe Navigation Guide — Nitpick v0.20.4

Nitpick has a first-class `optional<T>` type for values that may be `NIL`.
This guide covers declaration, safe navigation (`?.`), nil coalescing (`??`),
and how to work with optional struct fields.

---

## Declaring Optionals

Add `?` after the type name to make it optional:

```nitpick
int32?:age = NIL;            // starts as NIL
string?:name = "Alice";      // starts with a value

Point?:origin = Point{x: 0, y: 0};
Point?:nothing = NIL;
```

---

## Safe Navigation (`?.`)

Use `?.` to access a field on an optional struct. If the optional is `NIL`,
the entire expression short-circuits to `NIL` (an `optional<FieldType>`).
If it has a value, the field is returned as `optional<FieldType>`.

```nitpick
struct:Point = { int32:x; int32:y; };

Point?:p = Point{x: 3, y: 7};
int32?:x_val = p?.x;   // type: int32? = Some(3)

Point?:q = NIL;
int32?:q_val = q?.x;   // type: int32? = NIL
```

**Key type rule** (v0.20.4 fix): `obj?.field` where `field` has type `T`
returns `optional<T>`, not bare `T`. This ensures type correctness flows
through subsequent operations.

---

## Nil Coalescing (`??`)

Use `??` to unwrap an optional with a fallback default. The result type is
the unwrapped `T` (not `optional<T>`):

```nitpick
Point?:p = NIL;
int32:x = p?.x ?? 0i32;    // 0 (NIL coalesced)

Point?:q = Point{x: 5, y: 9};
int32:y = q?.y ?? -1i32;   // 9 (has value, returned directly)
```

The `??` operator:
- Left side: `optional<T>`
- Right side: `T` (the default)
- Result: `T`

---

## Chaining

Safe navigation chains are supported. Each `?.` propagates `NIL` if any
step is `NIL`:

```nitpick
struct:Address = { string?:city; };
struct:User = { Address?:addr; };

User?:u = NIL;
string:city = u?.addr?.city ?? "unknown";
// u is NIL → entire chain is NIL → "unknown"
```

---

## Function Pointer Results (`?` unwrap)

Calling a function pointer that returns `Result<T>` uses `?` for unwrapping:

```nitpick
(int32)(int32):fp = int32(int32:x) { pass x * 2i32; };
int32:result = fp(5i32) ? -1i32;   // call + unwrap, -1 on error
```

This is distinct from the nil-coalescing `??` used with `optional<T>`.

---

## NIL Literal

`NIL` is the zero value for any optional type:

```nitpick
string?:s = NIL;
int32?:n = NIL;
Point?:p = NIL;
```

There is no automatic coercion — you cannot pass `NIL` where a non-optional
type is expected.

---

## Optional vs. Non-Optional

Non-optional types cannot be `NIL`. The type checker enforces this:

```nitpick
int32:x = NIL;   // ERROR: int32 is not optional
int32?:y = NIL;  // OK
```

To convert an optional to a non-optional value, use `??`:

```nitpick
int32?:maybe = compute_maybe();
int32:definitely = maybe ?? 0i32;
```

---

## See Also

- [CLOSURE_GUIDE.md](CLOSURE_GUIDE.md) — closure lifetime and capture semantics
- [COMPTIME_GUIDE.md](COMPTIME_GUIDE.md) — compile-time evaluation
