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
decided before its first item is read. **A body is declarations or statements, not
both** (DEF-150, 1.6.1e step 2): a statement in a body that declares is
`NITPICK-MACRO-010` at the statement, once, and the body then expands to nothing —
no site holds both kinds, and every reader of the body's window reads each item by
the body's one kind. A declaration may begin with its modifiers (`comptime func:`,
`async func:`): the lookahead reads past them. A body holding a parse error expands
to nothing; the errors are the report. It has to be: `#name(...);` standing alone
is a splice among declarations and an expression statement among statements, and
the sigil does not say which. Deciding per item made every body beginning with `#`
a declaration body, so `macro:opt = () { #caller(x) + 1i32; };` was refused as "not
a single expression" and a statement macro could not invoke another one.

A body that is **nothing but a single invocation** — `macro:alias = () { #b(); };`
— is whatever `b` is, which the parse cannot know. It is read as an expression
body, and at statement position it becomes a block holding `#b();` that the next
round expands. Both work.

**At a declaration site the alias is passed through** (DEF-171, 1.6.1e step 3): at
module level, in a `struct` body and in an `impl` or `trait` body, the alias's one
invocation — its arguments substituted — stands as a splice where the alias stood,
and the next round expands it as what its target is. An alias of an alias takes one
round each. A target that does not fit the site is refused by the target's own
check, once, at the alias's body, with the note naming the alias's invocation.
(Until that step all three sites refused the alias with `NITPICK-MACRO-005` for
being an expression body, whatever `b` was — the library listener's `mc0046b`.)
`#caller(name)` and a compiler builtin (`#size_of<T>()`) are values, not aliases: a
body that is one of those is an expression body, and a declaration site refuses it.

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
same text — so splicing one into the other is a conversion. **A qualifier and a
`limit` travel to the field**: `fixed int32:a;` spliced into a struct body is a
`fixed` field — a later `s.a = 5i32;` is `NITPICK-ASSIGN-002`, exactly as on a field
written by hand — and `limit<r> int64:n;` is a limited field (D-308). **An
initialiser is refused** (`NITPICK-MACRO-005`): a field has none, and dropping a
value quietly is worse than not accepting the program.

> *[2026-09-30, 1.6.1e step 3 — a dated note (the library listener's `mc0085b`).]*
> This paragraph said "a variable declaration carrying an initialiser or a qualifier
> is refused rather than stripped: a field has neither" until this step. It described
> the splice before 1.0.8, which wrote no qualifier onto the field, and fields before
> D-287 and D-308 gave them `fixed` and `limit`. The qualifier has travelled since
> 1.0.8 and the rule since 1.5.8b step 6; the compiler was right and the sentence
> stale, so the step withdrew the refusal its plan had scheduled.
> `tests/analysis/rejection/spliced_fixed_field.npk` pins the travelling qualifier
> beside its hand-written twin, `tests/expansion/rejection/field_initialiser.npk` the
> initialiser's refusal with and without `fixed`.

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

**Wherever an expression stands in the body** — in a statement, in the body of a
declaration the macro emits, and inside what those carry: a **type** (an array's
size, a `comptime` argument), a **pattern** (its value, a range pattern's two
bounds), an **attribute**'s arguments, a variant's value, and every type of an
emitted declaration — a parameter's, a return type, a field's, a cast's target, a
generic argument, a generic parameter's bound:

```nitpick
macro:sized = (N) { wild int32[N]->:p = (#caller(mem) =>! wild int32[N]->); … #size_of<int32[N]>() … };
macro:in_range = (LO, HI) { pick (#caller(v)) { (LO..HI) { … }, (*) { … } } };
macro:arr_field = (N) { int32[N]:arr; };                 // struct:S3 = { #arr_field(3i32); };
```

Each instantiation gets its own copy of every such type and pattern (§6), so two
invocations with different arguments are two different types.
`tests/backend/programs/macro_param_positions.npk` observes each position through a
value, twice.

**What a parameter cannot be** — `NITPICK-MACRO-011`, refused at the macro's
DECLARATION, whether or not anything invokes it, once per parameter at its first
such spelling:

- **the name of a type.** `T:x`, `T{ … }`, `x =>! T`, `(T.Variant(b))`, `(T{ a, b })`:
  an argument is an expression and cannot name a type, so nothing could be
  substituted there and the body would mean whatever `T` names where the macro is
  written.
- **a name the body declares AND writes as an expression.** A local, a `for`
  binding, a pattern's binding, a function's parameter, a generic parameter, or a
  declaration the body emits: the declaration's name is never substituted and
  every USE of the name is, so each use would be the argument and not that
  declaration.

A name the body declares and never writes as an expression is NOT refused: that is
§10's open question (may a parameter name an emitted declaration? — `func:N` is
literally called `N`), left open. A FIELD and a VARIANT of a parameter's spelling
are neither — they are reached through `.`, are not identifiers, and are left
alone (`Box{ v: v }` substitutes the value and keeps the field). And the name in
`#caller(N)` is copied by name; it is no use of the parameter.

**A compile-time VALUE in an argument list is written in parentheses.** A bare
identifier there is read as a type's name (D-064 §2) — for a macro parameter as for
any other name — so `simd<int32, N>` is the first refusal above and
`simd<int32, (N)>` is the argument landing as the lane count.

**A range passed where a whole value pattern stands is refused**
(`NITPICK-MACRO-005`, at the pattern): a pattern's kind is the parser's (§6), and a
range substituted for `(K)` would leave a range value in a value pattern. The body
takes the bounds — `(LO..HI)`.

> *[2026-10-01, 1.6.1e step 3a — a dated note (DEF-189, FIXED).]* Until this step
> two positions were NOT reached by substitution: a **type** and a **pattern**
> written in the body. The clone copied both through, so a parameter written there
> stayed the bare name and resolved by the body's own rule (§5): refused where the
> module had no such name (`NITPICK-TYPE-004`, `NITPICK-RESOLVE-002`), and **read as
> the module's binding where it had one** — beside a module-level
> `fixed int32:N = 2i32;`, `#mk(3i32)` over `macro:mk = (N) { int32[N]:a = …; }`
> built a two-element array, in silence, at both legs, on every compiler to that
> day. The two things a parameter cannot be were accepted the same way: `T:x` beside
> a module `struct:T` built the module's type whatever was passed, and a body's
> `int32:N = 1i32;` declared a local that every later `N` then ignored for the
> argument. The paragraphs above are the rule as it now holds. (The step's first
> form refused EVERY declaration of a parameter's name; the sweep found the library
> listener's `mc0388` — §10's own example, expected to run — refused by it, and the
> rule was narrowed to the half that answers wrongly before it landed.)

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

**What a spliced member may name is what the body's own scope holds (§5).** A field
`V:item;` spliced into a generic `struct:Box<V>`, or a method spliced into
`impl:<T>:Stack<T>` whose signature says `T`, names a type parameter of the
declaration it LANDS in — the invocation site's — and is `NITPICK-TYPE-001` since
DEF-192 (it compiled, by capture, before). `Self` is a keyword, not a name, and
means the landing declaration's type as it does anywhere. Whether a body should
have a way to name the landing declaration's type parameter is **S-124** (§10).

### Splicing into an `impl`

```nitpick
macro:emit_methods = () {
    func:add_one = int32(Box:self) { pass (self.n + 1i32); };
};

impl:Box:Pair = { #emit_methods(); };
```

(The example read `$$i Box:self` until 1.6.1e step 2 — never a parameter form:
a lent receiver is `Box->:self`, a by-value one `Box:self`. The library
listener's copy of it trapped the compiler, DEF-150's c1.)

## 5. Hygiene

**An identifier in a macro body resolves in the scope where the macro was
written. Always.**

> *[2026-09-26, 1.6.1e step 1 — a dated note (DEF-144).]* The resolver kept this rule and the
> emitter did not: a body's free name was emitted as the CALLER's local of the same name whenever
> the caller had one (a lone identifier, a comparison's operand — arithmetic read right by the
> constant folder's accident). The emitter reads the resolver's symbol now; the rule holds in the
> emitted program as it held in the checked one.

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

**A type's name is a name** (DEF-192, 1.6.1e). `T` written in a body — a local's
annotation, a cast's target, `#size_of<T>()`, a field or a signature the body emits
— is the type the macro's own scope holds, and where that scope holds none it is
`NITPICK-TYPE-001`, the sentence saying which scope was asked. It binds a generic
parameter, or a trait's associated type, only where **the body itself declares it**
(the `T` of an emitted `func:same<T>`, the `A` of an emitted `struct:Pair<A>`); the
parameters of the function, struct or `impl` the macro is INVOKED in belong to the
invocation site, and a body cannot see them. An ARGUMENT's type names stand at the
invocation and bind there — `#plus_one(#size_of<T>())` inside `func:g<T>` measures
the caller's `T`, as the caller wrote.

> *[2026-10-01, 1.6.1e — why this paragraph is dated.]* The resolver has bound a
> body's identifiers this way since 0.6, and it never enters a type: a type's name
> is resolved later, by the type checker, which asked the generic parameters of
> whatever declaration it was standing in FIRST — and a body lands inside the
> declaration that invoked it. So `macro:sz = () { #size_of<T>(); };` beside a
> module `struct:T`, invoked inside `func:g<T>` or a method of `impl:<T>:Box<T>`,
> measured the caller's type in silence (8 for 4, both legs). The rule is asked of
> the node's SPAN now (`ast_macro_at`: a clone keeps its template's span, so the
> body's own nodes and every instantiation's copies answer alike).

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

**It stands wherever an expression stands in a body** — inside a type's size or a
pattern's value or bounds as well as in a statement (since DEF-189; it was
`NITPICK-MACRO-008` there while a body's types and patterns were shared by every
instantiation, §6).

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

**Expansion reaches every position an expression is written in** (DEF-170, 1.6.1e
step 3). Until that step the walk followed a statement's condition and body and a
declaration's body and initialiser, and an invocation anywhere else was
`NITPICK-MACRO-006` — the compiler's own defect code — in a correct program (the
library listener's `mc0278`, a `while`'s `decreases`). It follows what the clone
and the depth pass enumerate:

- a loop's `invariant` and `decreases`, in every loop form;
- a function's contracts — `requires`, `ensures`, `decreases`, `acquires`, `joins`;
- a `Rules` block's clauses;
- the expressions inside a **type**, wherever the type stands — an array size and
  a `comptime` argument (`int32[#n()]`, `Mutex<Config, #lvl()>`) in a binding, a
  parameter, a field, a return type, a cast's target, a generic argument, a
  pointee, an impl's target or trait, an `assoc` default, a generic bound;
- a `pick` pattern's value and a range pattern's bounds;
- a variant's value, a `unit:` right-hand side, an `error:` code, an attribute's
  arguments, an extern function's `fails on` predicate.

`tests/backend/programs/macro_walk_positions.npk` observes each through a value.

**Every instantiation is its own copy of the body** (DEF-189, 1.6.1e step 3a). An
expression, a statement, a declaration, a pattern, an attribute, a generic
parameter and a verification clause are cloned, each one; a **type** is cloned
exactly when it holds an expression — an array's size, a `comptime` argument — and
is otherwise the template's own node, because a type with no expression inside
cannot differ between two instantiations and nothing later writes onto a type. So
an invocation written inside a body's type or pattern is expanded once per
instantiation, in that instantiation's copy, and a parameter there is that
instantiation's argument (§3).

> *[2026-10-01 — what this replaced.]* From 1.6.1e step 3 until step 3a this
> paragraph read "a type, a pattern, an attribute and a generic parameter written in
> a macro body are the template's own nodes … every instantiation of the body points
> at ONE node, and an invocation inside it is expanded in place the first time an
> instantiation leads the walk to it — once, for all of them". That was sound only
> while no parameter was substituted into one, which was the defect (DEF-189); and a
> declaration shared by two instantiations (a `for` binding, a generic parameter, a
> field, an attribute) was one node under two symbols.

**A pattern's kind is the parser's.** `(1i32..5i32)` is a range pattern because the
parser saw the `..`; `(#m())` is a value pattern, read before anybody knows what `m`
is. A macro whose body is a range therefore cannot stand as a WHOLE pattern —
`NITPICK-MACRO-005` at the pattern. Write the range in the pattern and expand its
bounds: `(#lo()..#hi())`.

**Expansion precedes `comptime` evaluation**, and `comptime` delegates to the
expanded AST (§8).

## 7. Expansion is bounded

```nitpick
macro:m = () { #m(); };      // refused
```

**A macro that reaches itself never settles, and that is decided at the
declarations** (DEF-172, 1.6.1e step 3). After the macros are collected and before
the first round, the compiler reads the invocation graph of each module's macros —
a macro's edges are the `#name(...)` written anywhere between its body's braces
that name a macro of the same module (D-124: another module's macro is no edge, so
two modules' macros form no cycle) — and refuses every cycle once:
`NITPICK-MACRO-004` at the declaration of the cycle's first macro in source order,
the shortest chain in the sentence — "macro `ping_m` never settles: `ping_m` invokes
`pong_m`, which invokes `ping_m`". A body has no conditional, so a cycle in this
graph is non-termination the moment any member is invoked, and it is refused
UNINVOKED: the example above is refused where it stands. A program refused here is
not expanded.

**The edge is what the body writes**, not what an instantiation happens to keep.
`macro:a = () { #ign(#a()); };` is refused although `ign` discards its argument: the
rule is one syntactic reading of the declarations, and a rule that depended on what
every other macro does with its parameters would change a declaration's verdict
when an unrelated body was edited.

> *[2026-09-30, 1.6.1e step 3 — what this replaced.]* The declaration above was
> ACCEPTED while nothing invoked it (the library listener's `mc0246`), and an invoked
> cycle ran the rounds to their bound and was reported at `prelude.npk:17:1` — module
> 0's root, a file the reader did not write — in a sentence naming neither macro
> (`mc0251`).

**A depth bound** limits one invocation's nesting; **an iteration bound** — 32
rounds — limits the fixed-point loop. With the cycles refused at the declarations,
what reaches the iteration bound is a chain that is finite and too long: each round
expands one level, so a chain of exactly 32 macros settles on the last round and is
accepted, and a 33rd link is `NITPICK-MACRO-004` at the first invocation still
standing in the program's own text after the last round, naming its macro.

**A `macro:` declared inside a macro body is refused where it is written**
(`NITPICK-MACRO-005`): macros are collected once, before expansion begins, so one
that an expansion emitted could never be invoked. (It was `NITPICK-MACRO-006`, from
the clone, and only when the outer macro was invoked.)

**A diagnostic inside an expansion is annotated once.** `NITPICK-MACRO-009` is the
note "the X at line N lies in a macro body that this invocation expanded"; it
annotates a diagnostic and never another note (DEF-173: the driver called the
annotation twice at four of its returns since 0.7.8, and the second pass annotated
the first pass's notes).

**And no tree nests deeper than 256 levels**
(`AST_DEPTH_MAX`, DEF-150, 1.6.1e step 2): no node sits more than 256 levels below
its root — a function's body block is the first level, a module-level initialiser's
expression its own first — so no walk of the compiler recurses further. The parser
MEASURES each declaration's tree after parsing it (a node's height is the depth of
the subtree it roots; the AST logs construction order, children first, and one
pass computes every height) and refuses the first subtree past the bound with
`NITPICK-PARSE-012`, once per declaration; its own descent is held to the same
number, so a parenthesised nesting is refused before the 257th level is parsed.
The expander refuses a splice whose clone would land past the bound — the site's
depth plus the clone's height — with `NITPICK-MACRO-003` at the invocation, since a
body and a site the parser admitted apart can still sum past it. Every whole-body
analysis's depth reads the same constant. The shapes it closed: a 500-deep
parenthesised expression, and a 600-term chain `1 + 1 + … + 1`, which the parser
builds in a loop while the tree is 600 deep on its left spine — both ran the
constant folder (whose fuel counts work, 4,096, not depth) off the compiler's stack.
A program that did not parse is not expanded (the rule the pipeline applies after
expansion, one stage earlier), so a macro whose body the parser refused reports
that refusal alone.

The two bounds are separate because they are different mistakes: deeply nested is
one invocation that is too complicated, a chain past the round bound is too many
layers of macros each invoking the next, and one budget would report them alike. A
cycle is neither — it does not terminate, and it is refused at its declaration.

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
in one are consumed when the macro is cloned, not where they appear. What an
instantiation holds is the CLONE, which is program text and is audited like any
other: `(#no_such())` as a pattern in an instantiated body is `MACRO-001` at the
span the body wrote, with the note that names the instantiation (it used to pass
the whole front half unexpanded and die in the emitter, until 1.6.1e step 3).
(Between that step and step 3a a body's types and patterns were shared by every
instantiation, the audit made an exception for a template expression the walk had
reached, and `#caller(name)` in one was `MACRO-008`; each instantiation has its own
copy now, §6, and `#caller` there is consumed like any other.)

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
| an enum's payload-less variant as a value; `==` and `!=` on two | D-338 |
| `pick`, statement and expression form, over a constant selector — inside a `comptime func:` | D-338 |
| `?!` and `?|` over a call the evaluator completed, as `raw` | D-338 |
| size and alignment intrinsics | `COMPTIME-003`, `COMPTIME-004` |
| built-in macros inside `comptime(…)` | `COMPTIME-007` |
| `assert_static comptime(…)`, short-circuiting to the verifier | `COMPTIME-008` |

> *[2026-10-01, 1.6.1e step 3b — D-338 as it landed (landing 90).]* String **ordering** is
> evaluated: `a.cmp(b)` on two constant strings answers the prelude's `Ordering`, by the order
> the run time's `impl:string:Ord` computes (unsigned bytes, then length) — the two held equal
> by `tests/backend/programs/comptime_string_order.npk`, which asks both. Until this landing
> the row's "ordering" was `NITPICK-TYPE-004` (the library listener's `mc0309b`): the evaluator
> held no enum value and ran no `pick`. Concatenation, equality and length were always there.

**This is an interpreter for a subset of the language, not a constant folder.** It
executes loops and mutates locals, which means anything it can express is something
the compiler runs at build time — so evaluation carries a budget for the same
reason expansion carries a bound.

### An enum value and a `pick`, at compile time (D-338)

The evaluator holds **a payload-less enum variant** as a value — `Ordering.Less`, any
non-generic enum's variant that carries no payload — in a local, as a `comptime func:`'s
argument or its result. Two values of one enum compare with `==` and `!=` where the run
time's `==` exists (an enum none of whose variants carries a payload). `a.cmp(b)` on two
constant strings is the ONE method call it folds. `f(x) ?! E` and `f(x) ?| d` fold through a
call it completed, as `raw f(x)` does: an evaluation that completed did not fail, and `?|`'s
fallback is lazy. `&&` and `||` decide on their left operand where it decides, as at run
time (DEF-211: until 1.6.1e step 3b both operands were evaluated, and a division its guard
protects was refused).

It runs **a `pick`, in both forms, inside a `comptime func:`**: the selector a plain integer,
a `bool`, a `char`, a `string` or an enum value; the arms tested in source order, a `where`
guard asked after its pattern; a wildcard, a value, a range (by the selector's signedness — a
`char` and a `uint64` compare unsigned) or a bind-free variant; `give` and `fall` as at run
time. An arm that binds a payload is passed over when its variant is not the selector's.

```nitpick
comptime func:order = int32(string:a, string:b) never fails {
    pick (raw a.cmp(b)) {
        (Ordering.Less)    { pass -1i32; },
        (Ordering.Equal)   { pass 0i32; },
        (Ordering.Greater) { pass 1i32; }
    }
};
fixed int32:AB_VS_B = comptime(order("ab", "b"));     // -1
```

Four things it does not do — each is `NITPICK-TYPE-004` at the site that forced the
evaluation, never a different value:

- **A `pick` is evaluated only inside a `comptime func:` call.** `comptime(pick (K) { … })`
  written directly in an ordinary body, or as a module binding's initialiser, is not a
  constant: an arm is a block of statements, and the evaluator runs statements in a call's
  frame and nowhere else. (Folded where it stands, an arm's assignment to a local of the
  surrounding function would be evaluated and never run.) Call a `comptime func:`.
- **The arm of an expression `pick` ends in `give`.** A `pass` there leaves the function
  through an expression; the statement form returns from its arms.
- **No binding pattern is taken** (a struct pattern; a variant that binds a payload, since the
  values the evaluator holds carry none), there is no `ERR:` arm, and a `tbb`, ternary,
  fixed-point or float selector is not selected on.
- **An enum value does not leave the evaluator**: `comptime(…)` whose value is one, and a
  module binding initialised with one (`NITPICK-TYPE-035`, as before D-338), are refused where
  they are written. Whether they should be constants is **S-127**, the user's.

A `comptime func:` that recurses THROUGH A TYPE — `#size_of<int8[comptime(deep())]>()` inside
`deep` — meets the depth bound like any other recursion (DEF-213; until 1.6.1e step 3b it
stopped the compiler with no message).

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

> *[2026-10-01 — a dated note.]* D-222 (2026-08-29) retired `const`: the binding
> that folds is a MODULE-LEVEL `fixed` binding, and the sentence above about
> `fixed` describes a local one.

Likewise a call folds when the function is declared `comptime`, and not because it
happens to be foldable — whether the compiler runs your code is not something to
discover by accident.

**Inside a `comptime func:` call, a binding is the one the resolver bound** (DEF-196,
1.6.1e). The evaluator keeps a call's parameters and locals by the DECLARATION that
introduced each, so:

- a local declared in a nested block is a new binding, and the outer one of its
  spelling is untouched when the block ends — as at run time;
- a macro body's free name is the module's (§5), whatever the function it is
  invoked in calls its locals;
- a name written inside a TYPE (`int32[LEN]`) is the module's `fixed` binding and
  never a local: a local is not an array size (D-222), and the checker never read
  one there.

> *[2026-10-01, 1.6.1e — why this is dated.]* The evaluator kept them by NAME, in one
> list per call, on a premise its own comment stated ("there is no nested scope in a
> body this evaluator accepts") that stopped being true when it learned `if`,
> `while` and the counted loops. Each of the three evaluated WRONGLY at compile time
> while the same body answered rightly at run time: `int32:x = 1i32; if (x == 1i32)
> { int32:x = 2i32; } pass x + 10i32;` was 12 for 11; `{ X + N; }` beside a module
> `N` of 100, invoked where a local `N` is 1, was 6 for 105; `#size_of<int32[LEN]>()`
> with a local `LEN` was 8 where the checker's type is 12 bytes.

### When evaluation fails

The diagnostic names **the offending expression** and, where the failure is inside
nested `comptime func:` calls, **the call chain** that reached it
(`COMPTIME-009`). A comptime failure is a compile error.

The chain is in the sentence (1.6.1e step 3; the library listener's `mc0344` — it
was promised here and written nowhere): the report keeps the offending expression's
span and ends "; reached through the compile-time calls `outer_call` -> `inner_div`",
outermost first, a name entered several times in a row written once with its count
(`deep` (x64) at the depth bound). One expression's failure is still one report,
whichever path reaches it first — the typer folding it where it stands, or a call
evaluating it.

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
  *[2026-10-01, 1.6.1e step 3a.]* STILL OPEN — **S-123**, the user's: refuse it,
  keep the literal name, or substitute a name. What is refused since that step
  (`NITPICK-MACRO-011`, §3) is the half of it that answered WRONGLY: a body that
  declares a parameter's name and also writes it as an expression, whose every
  such use was the argument instead.

> **Settled since: are emitted names hygienic?** No — **a macro never renames what
> it emits** (D-128), and a collision is an error like any other name declared
> twice. Renaming makes the feature useless in every position: a renamed field
> cannot be named by the caller, a renamed method cannot satisfy the trait it
> implements, and `#make_pair()`'s `greet1` could not be called. §4's splicing works
> entirely by naming what was emitted.
- **May a body name the type parameter of the declaration it is invoked in?** Not
  since DEF-192 (§5): a type's name in a body is the defining scope's, so a field
  spliced into `struct:Box<V>` cannot say `V`, and a method spliced into
  `impl:<T>:Stack<T>` cannot say `T` — each compiled before, by the capture the
  hygiene rule exists to prevent. `#caller(NAME)` is an expression form and has no
  type form. **S-124**, the user's: leave it so (generics and `#[derive]` are the
  tools for code over a type parameter), or accept `#caller(T)` where a type's name
  stands.
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
  *[2026-10-01.]* And where a `comptime func:` may BE called: it is emitted nowhere, so a
  call of one outside a constant context compiles and is refused by `llc` as an undefined
  symbol (DEF-214, open). **S-128**, the user's: recommended, a `comptime func:` exists at
  compile time only and the checker refuses such a call by name.
- **What are the bounds, numerically?** D-057 settles that they exist.
  `--comptime-budget <N>` is named as the precedent for the shape.
