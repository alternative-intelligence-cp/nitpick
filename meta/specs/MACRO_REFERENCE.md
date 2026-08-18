# Macro Reference

`macro:` declarations, invocation, splicing, hygiene, and `comptime` evaluation.

> **This document did not exist until cycle 0.6.0.** D-057 found that the macro
> system was "specified in the wrong artifact" — 32 regression tests under
> `nitpick/tests/bugs/`, carrying the semantics in comments keyed to decision codes
> (`MACRO2-DEC-001…007`, `COMPTIME-001…013`), with no prose that could be read as a
> whole. This is that specification, recovered and written down.
>
> **Where the corpus and a decision disagree, the decision is recorded here and the
> prototype's behaviour is noted as what it replaced.** Three rules below are
> D-057's rather than the corpus's, and each says so.
>
> The corpus is written in the **prototype's dialect** and does not compile against
> this language — see §9.

---

## 1. Declaring a macro

```nitpick
macro:name = (param, …) { body };
```

The body is a block. **What it contains determines where the macro may be
invoked**, and nothing else does: there is no separate declaration of a macro's
kind.

| Body contains | May be invoked | Splices as |
|---|---|---|
| declarations (`func:`, …) | module level | top-level declarations |
| variable declarations | a `struct` body | fields |
| function declarations | an `impl` body | methods |
| a single expression | expression position | that expression |

A macro taking no parameters still declares an empty list: `macro:m = () { … };`.

## 2. Invoking one

```nitpick
name!()          // no arguments
name!(a, b)      // with arguments
```

**Four positions**, and the same spelling in all of them:

```nitpick
make_pair!();                          // module level — emits declarations

struct:Point = { make_xy_fields!(); };  // struct body — splices fields

impl:Box:Pair = { emit_methods!(); };   // impl body — splices methods

string:s = emit_msg!();                 // expression position
```

An invocation whose expansion does not fit where it landed is an error — fields
into something that is not a struct, declarations into an expression.

## 3. Parameter substitution

An argument replaces every occurrence of the parameter name in the body,
**including inside declarations the body emits**:

```nitpick
macro:make_const = (N) {
    func:my_const = int32() { pass N; };
};

make_const!(42i32);      // emits  func:my_const = int32() { pass 42i32; };
```

Substitution traverses the whole emitted subtree. It is not textual: the argument
is an AST node and lands as one.

## 4. Emission

### Multiple declarations, which may reference each other

```nitpick
macro:emit_helpers = () {
    func:helper_a = int32() { pass 11i32; };
    func:helper_b = int32() { pass 31i32; };
    func:helper_sum = int32() { pass (raw helper_a()) + (raw helper_b()); };
};
```

All three become top-level declarations, and `helper_sum` resolves the other two.
**Names emitted by one expansion are visible to each other**, which is why
expansion completes before name resolution begins (§6).

### Splicing into a struct

```nitpick
macro:make_xy_fields = () { int32:x; int32:y; };

struct:Point = { make_xy_fields!(); };
```

`Point` has fields `x` and `y`, and may mix spliced and literal fields freely.

### Splicing into an `impl`

```nitpick
macro:emit_methods = () {
    func:add_one = int32($$i Box:self) { pass (self.n + 1i32); };
};

impl:Box:Pair = { emit_methods!(); };
```

## 5. Hygiene

**An identifier in a macro body resolves in the scope where the macro was
written. Always.**

```nitpick
int32:shared = 100i32;

macro:report = () { `shared = &{shared}`; };

func:main = int32(cstring[]:_~argv) {
    int32:shared = 5i32;
    string:s = report!();     // `shared` is the TOP-LEVEL 100, not the local 5
    exit 0i32;
};
```

If the name does not resolve in the defining scope, that is a **compile error** —
never a silent fall back to the call site.

### `#caller(NAME)` — the sole opt-out

```nitpick
macro:report_opt = () { `shared = &{#caller(shared)}`; };
```

`#caller(NAME)` resolves `NAME` at the **invocation site**. It is the only way to
reach the caller's scope, and naming something absent there is an error like any
other unresolved name.

`#` is the compiler-directive sigil (D-020), so this needs no new syntax shape.

> **This flips the prototype (D-057).** There, an identifier resolving differently
> in the two scopes emitted `NITPICK-061 MACRO_HYGIENE_VIOLATION` and then **kept
> the caller's binding anyway** — `bug603` calls it "the back-compat path". D-057:
>
> > A back-compat path, not a design. And it is precisely the failure the blueprint
> > philosophy exists to prevent: the macro means something different depending on
> > where it is invoked, with a warning as the only guard. A warning is not a
> > mechanism; it is a request that someone be paying attention.
>
> **`NITPICK-061` no longer exists**, because the hazard is structurally absent
> rather than detected.

## 6. Expansion order

**Expansion precedes everything.** It runs before name resolution, before type
checking, before every static analysis — so what those passes see is the expanded
program.

**Expansion iterates to a fixed point.** A macro body may contain invocations:

```nitpick
macro:inner = () { func:f1 = int32() { pass 10i32; }; };
macro:outer = () { inner!(); func:f3 = int32() { pass 30i32; }; };

outer!();     // expands to { inner!(); f3 }, then inner expands on the next round
```

The loop repeats until no invocation remains. This holds for splices too — a
struct-body macro may expand to a body containing another struct-body macro.

**Expansion precedes `comptime` evaluation**, and `comptime` delegates to the
expanded AST (§8).

## 7. Expansion is bounded

```nitpick
macro:m = () { m!(); };      // refused
```

**A depth bound** limits one invocation's nesting; **an iteration bound** limits
the fixed-point loop. Exceeding either is an ordinary compile error naming the
macro and the chain that reached the bound.

The two are separate because they are different mistakes: deeply nested is a
program that is too complicated, mutual recursion is a program that does not
terminate, and one budget would report them alike.

> **New in D-057.** Nothing in the corpus bounds the loop, so the prototype **fails
> to terminate** on the macro above. That is unacceptable in a compiler under
> formal verification, where termination is itself a property to be established.

## 8. `comptime`

### Two forms

```nitpick
comptime func:double = int32(int32:n) { pass (n * 2i32); };   // a callable

int32:v = comptime(double(21i32));                            // a forcing form
```

`comptime(expr)` is a **keyword operator with a parenthesised operand**, not a
call — the same shape as `move(place)` (D-065).

### What the evaluator can do

Recovered from `COMPTIME-001…013`:

| Capability | Evidence |
|---|---|
| integer arithmetic | throughout |
| **mutable locals and assignment** — `x = x + n`, and chains | `COMPTIME-002` |
| **loops** — `loop(lo, hi, step) { … }` | `COMPTIME-001` |
| calls to `comptime func:` declarations, nested | `COMPTIME-001`, `COMPTIME-009` |
| **strings** — concatenation, equality, ordering, length | `COMPTIME-005` |
| size and alignment intrinsics | `COMPTIME-003`, `COMPTIME-004` |
| built-in macros inside `comptime(…)` | `COMPTIME-007` |
| `assert_static comptime(…)`, short-circuiting to the verifier | `COMPTIME-008` |

**This is an interpreter for a subset of the language, not a constant folder.** It
executes loops and mutates locals, which means anything it can express is something
the compiler runs at build time — so evaluation carries a budget for the same
reason expansion carries a bound.

### Macros and `comptime`, both directions

```nitpick
macro:four = () { comptime(2i32 * 2i32); };      // a macro body containing comptime
int64:a = comptime(double_it!(3i32));            // comptime over a macro invocation
int64:b = comptime(add_one!(double_it!(10i32))); // nested arbitrarily
```

One rule covers all of it: **expansion runs to a fixed point first, then evaluation
runs over the result.**

### When evaluation fails

The diagnostic names **the offending expression** and, where the failure is inside
nested `comptime func:` calls, **the call chain** that reached it
(`COMPTIME-009`). A comptime failure is a compile error.

## 9. The corpus is in the prototype's dialect

The 32 recovered tests do not compile against this language. Where they differ,
**this document follows current Nitpick** and the difference is listed here so a
reader of the corpus is not misled:

| Corpus writes | This language | Why |
|---|---|---|
| `impl:Trait:for:Type` | `impl:Type:Trait` | D-030 |
| `@sizeof(T)`, `@alignof(T)` | `#size_of<T>()`, `#align_of<T>()` | `@` is address-of and nothing else (D-020) |
| `expr ? default` | the defaults operator | respelled; see `OP_REFERENCE.md` |
| `0`, `10`, `exit 1` | `0i32`, `10i32`, `exit 1i32` | literals carry their width (D-092) |
| `func:main = int32()` | `func:main = int32(cstring[]:argv)` | |
| `MacroPattern` in a `pick` arm | **removed** | D-057 — no invocation survives to be matched |

## 10. What the corpus does not settle

Recorded as open rather than invented:

- **Are emitted names hygienic?** Hygiene as specified governs how a macro body
  *reads* names. Two invocations of a declaration-emitting macro in one module emit
  the same names, and nothing in the corpus says whether that collides or is
  renamed. It currently collides, because nothing renames.
- **May a macro emit a macro?** Nothing exercises it. The fixed-point loop would
  expand the result, so it likely works by construction — which is not the same as
  being specified.
- **What may a `comptime func:` call?** The corpus shows comptime functions calling
  comptime functions. Whether an ordinary function is callable, and what happens if
  it touches the outside world, is unstated.
- **What are the bounds, numerically?** D-057 settles that they exist.
  `--comptime-budget <N>` is named as the precedent for the shape.
