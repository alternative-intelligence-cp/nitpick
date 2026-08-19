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

**A body is a declaration body if it CONTAINS a declaration** (D-125), and that is
decided before its first item is read. It has to be: `#name(...);` standing alone
is a splice among declarations and an expression statement among statements, and
the sigil does not say which. Deciding per item made every body beginning with `#`
a declaration body, so `macro:opt = () { #caller(x) + 1i32; };` was refused as "not
a single expression" and a statement macro could not invoke another one.

A body that is **nothing but a single invocation** — `macro:alias = () { #b(); };`
— is whatever `b` is, which the parse cannot know. It is read as an expression
body, and at statement position it becomes a block holding `#b();` that the next
round expands. Both work.

## 2. Invoking one

```nitpick
#name()          // no arguments
#name(a, b)      // with arguments
```

**Four positions**, and the same spelling in all of them:

```nitpick
#make_pair();                          // module level — emits declarations

struct:Point = { #make_xy_fields(); };  // struct body — splices fields

impl:Box:Pair = { #emit_methods(); };   // impl body — splices methods

string:s = #emit_msg();                 // expression position
```

An invocation whose expansion does not fit where it landed is an error — fields
into something that is not a struct, declarations into an expression. What each site
holds, and how a body reaches it:

| site | body | how it arrives |
|---|---|---|
| module level | declarations | cloned |
| `struct` body | variable declarations | **converted to fields** |
| `impl` or `trait` body | declarations | cloned |
| expression position | one expression | substituted in place |
| `enum` body | — | refused |

The struct case is the one that is not a copy. `int32:x;` parses as a STATEMENT
inside a macro body and as a FIELD inside a struct — different grammars reading the
same text — so splicing one into the other is a conversion. A variable declaration
carrying an initialiser or a qualifier is refused rather than stripped: a field has
neither, and losing a `fixed` quietly is worse than not accepting the program.

**An enum body is refused**, and that is the absence of a spelling rather than a
restriction: a variant is a name with an optional payload, no macro body can contain
one, and mapping some other body shape onto variants would be inventing a rule.

## 3. Parameter substitution

An argument replaces every occurrence of the parameter name in the body,
**including inside declarations the body emits**:

```nitpick
macro:make_const = (N) {
    func:my_const = int32() { pass N; };
};

#make_const(42i32);      // emits  func:my_const = int32() { pass 42i32; };
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

struct:Point = { #make_xy_fields(); };
```

`Point` has fields `x` and `y`, and may mix spliced and literal fields freely.

### Splicing into an `impl`

```nitpick
macro:emit_methods = () {
    func:add_one = int32($$i Box:self) { pass (self.n + 1i32); };
};

impl:Box:Pair = { #emit_methods(); };
```

## 5. Hygiene

**An identifier in a macro body resolves in the scope where the macro was
written. Always.**

**A macro is invocable only in the module that declares it** (D-124). It is not
exported, `use` does not bind it, `pub` on it changes nothing, and a module nested
inside the declaring one cannot reach it. That is what makes the sentence above
implementable: the scope the macro was written in and the scope its expansion lands
in are the same scope, so nothing downstream has to carry a second one. Invoking a
macro from another module is `NITPICK-MACRO-007`.

`#[derive]` (D-123) is the mechanism for code generation that crosses a module
boundary; `macro:` is a local shorthand.

```nitpick
int32:shared = 100i32;

macro:report = () { `shared = &{shared}`; };

func:main = int32(cstring[]:_~argv) {
    int32:shared = 5i32;
    string:s = #report();     // `shared` is the TOP-LEVEL 100, not the local 5
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

**It resolves the way any name at that point resolves** — reaching the caller's
locals and, past them, the module's own names. `#caller(NAME)` means "whatever
`NAME` means here", not "the caller's locals only". It differs from writing the
bare name exactly when the invocation site has a local binding of it, which is the
case it exists for.

**It is checked like any other name.** Naming something absent from the invocation
site is `NITPICK-RESOLVE-002`, and writing `#caller` outside a macro body — where
there is no invocation to reach — is `NITPICK-MACRO-008`. An escape hatch with no
rule would be the one path in the language worse than having no escape hatch.

### How each position gets it

| Invoked as | The expansion becomes | Free names resolve by |
|---|---|---|
| a declaration | the declarations, in this module | landing where the macro was written |
| a statement | **a block** holding the statements | the block's parent being the module scope |
| an expression | the expression, substituted in place | one mark on the substituted node |

The **block** is worth stating rather than treating as an implementation detail. A
`int32:tmp = …` in a statement body lives in the block's own scope: it cannot
collide with a caller's `tmp`, it cannot be read after the invocation, and a free
name in the body walks up past the caller's locals to the module. One node carries
the whole rule, which is why statement-position hygiene needs no check anywhere.

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
macro:outer = () { #inner(); func:f3 = int32() { pass 30i32; }; };

#outer();     // expands to { #inner(); f3 }, then inner expands on the next round
```

The loop repeats until no invocation remains. This holds for splices too — a
struct-body macro may expand to a body containing another struct-body macro.

**Expansion precedes `comptime` evaluation**, and `comptime` delegates to the
expanded AST (§8).

## 7. Expansion is bounded

```nitpick
macro:m = () { #m(); };      // refused
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

### Nothing is left standing

**Every `#name(...)` still in the program when expansion finishes is refused**
(D-126) — as `MACRO-001` if the name is unknown, `MACRO-007` if the macro is
declared in another module, `MACRO-008` if it is `#caller`. Only the three
compiler builtins survive.

This closes a hole that produced no diagnostic at all rather than a bad one:
`#totally_not_a_macro(3i32)` used to compile clean, because an unrecognised `#name`
was never resolved, never typed, and reached the end of the frontend as the
**invalid** type — the encoding the checker uses to suppress cascades, and
therefore silent. A mistyped macro name expanded to nothing and said nothing.

It is also how a hole in the expansion **walk** is caught. Invocations are found by
walking each module's declarations, because an invocation's meaning depends on
which module it is in (D-124) and a whole-array scan cannot say. A walk can miss a
statement kind; the scan afterwards cannot, so a miss arrives as a refusal naming
the invocation rather than as a body that quietly never expanded.

A macro body is exempt, because a body is a **template** — the invocations written
in one are consumed when the macro is cloned, not where they appear.

---

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
int64:a = comptime(#double_it(3i32));            // comptime over a macro invocation
int64:b = comptime(#add_one(#double_it(10i32))); // nested arbitrarily
```

One rule covers all of it: **expansion runs to a fixed point first, then evaluation
runs over the result.**

### What a name means inside one

**A `const` global folds; nothing else that is a name does** (D-130). `const` is
the marker that says a binding has one value for the whole program (D-010), so it
is the marker that says a name may stand in a constant expression. A `fixed`
binding is assigned once at **run** time and is correctly not one, and neither is a
local or a parameter of an ordinary function.

Likewise a call folds when the function is declared `comptime`, and not because it
happens to be foldable — whether the compiler runs your code is not something to
discover by accident.

### When evaluation fails

The diagnostic names **the offending expression** and, where the failure is inside
nested `comptime func:` calls, **the call chain** that reached it
(`COMPTIME-009`). A comptime failure is a compile error.

### And when it does not finish

**Two bounds, not one** (D-130), for the reason §7 gives for expansion having two:
they fail differently. A budget bounds the total work; a **depth** bound bounds
recursion, because a `comptime func:` that calls itself exhausts the compiler's own
stack long before a budget measured in steps runs out — measured, and it segfaulted
the checker before the second bound existed.

Exceeding either is `NITPICK-TYPE-025`, which is its own code and not "this is not
a constant": one means the expression cannot be evaluated, the other means it can
and never stops.

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
| `name!(args)` — the invocation | **`#name(args)`** | D-046: `#` is the compiler-directive sigil, and "a caller does not need to know whether `#foo(x)` is compiler-provided or user-defined" |
| `MacroPattern` in a `pick` arm | **removed** | D-057 — no invocation survives to be matched |

> **The invocation spelling is the one this document got wrong first.** Its
> examples were transcribed from the corpus with `name!(…)` intact — which is
> precisely the mistake this section exists to prevent, made while writing the
> section. `#name(…)` is the form; there is no postfix `!` in the grammar, and `!`
> is prefix logical-not and nothing else.

## 10. What the corpus does not settle

Recorded as open rather than invented:

- **May a parameter name an emitted declaration?** `macro:m = (N) { func:N = …; };`
  does not work: substitution reaches EXPRESSIONS, and a declaration's name is a
  payload rather than an expression, so the emitted function is literally called
  `N`. The corpus never writes it — `bug593` substitutes into a body and every
  emitted name is fixed — so this is unimplemented rather than refused. It is a
  question about **parameters**, not about hygiene.

> **Settled since: are emitted names hygienic?** No — **a macro never renames what
> it emits** (D-128), and a collision is an error like any other name declared
> twice. Renaming makes the feature useless in every position: a renamed field
> cannot be named by the caller, a renamed method cannot satisfy the trait it
> implements, and `#make_pair()`'s `greet1` could not be called. §4's splicing works
> entirely by naming what was emitted.
- **May a macro emit a macro?** Nothing exercises it. The fixed-point loop would
  expand the result, so it likely works by construction — which is not the same as
  being specified. Note what D-124 adds: an emitted macro would belong to the
  module it landed in, which is where its own invocations would have to be.
- **What does a diagnostic inside an expansion point at?** A node written in the
  macro and instantiated at the call site has two places it came from, and it
  currently carries the macro's. So "cannot find `only_local`" reports the line of
  the macro BODY, not of the invocation that made it wrong. Both are true and the
  reader needs both; 0.6.6 is where a diagnostic learns to say the second.
- **What may a `comptime func:` call?** The corpus shows comptime functions calling
  comptime functions. Whether an ordinary function is callable, and what happens if
  it touches the outside world, is unstated.
- **What are the bounds, numerically?** D-057 settles that they exist.
  `--comptime-budget <N>` is named as the precedent for the shape.
