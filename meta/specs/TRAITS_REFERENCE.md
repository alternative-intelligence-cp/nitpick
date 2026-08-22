# Nitpick Traits, Generics, and Dispatch

Adopted from `FORMAL_DRAFT/13_traits_and_generics.md` with corrections applied for
D-001…D-031. Chapter 06's overlapping trait and generic material is superseded —
see `GRAMMAR_ADOPTION_CONFLICTS.md` Part F for why chapter 13's forms won.

---

## 1. Philosophy

Nitpick uses strict **composition over inheritance**. Classical class-based
inheritance is rejected outright in favour of interface definitions (traits) and
isolated data structures (structs). This avoids the brittle base-class problem
and keeps memory layouts predictable.

Because there is no inheritance, an AST-style class hierarchy is expressed as
**tagged enums over composable structs** rather than base/derived nodes.

## 2. Traits and Implementations

```nitpick
trait:Serializable = {
    func:to_bytes = buffer(Self:self);
};

struct:Message = {
    int32:id;
};

impl:Message:Serializable = {
    func:to_bytes = buffer(Message:self) {
        pass result;
    };
};
```

`Self` denotes the implementing type inside a `trait` or `impl` body, and is
invalid anywhere else.

> Chapter 06 §6.5–6.6 spells these `trait:Reader { … };` (no `=`) and
> `impl Reader for FileStream { … }` (space-separated). Both are **struck** — they
> are the only declarations in the language that would not follow the
> `keyword:name = value;` house form (D-030).
>
> Chapter 13's own `impl:Trait:for:Type` is also superseded. **`impl` takes no
> connector, and the type comes first** (D-031):
>
> ```ebnf
> ImplDeclaration ::= "impl" ":" TypeOrParam (":" TraitName)? "=" "{" ImplBody "}" ";"
> ```
>
> Slot 1 is always the type being implemented on, in every form. `for` was
> dropped because it already means "iterate over"; no replacement keyword was
> added because a connector carries no information that position does not already
> carry.

### 2.1 Default Methods

A trait may supply a default body. Impls that omit the method inherit it; any impl
may override.

```nitpick
trait:Describable = {
    func:name     = string(Self:self);            // required
    func:describe = string(Self:self) {           // default
        pass("an object");
    };
};
```

### 2.2 Supertraits

A trait may require that implementing types also satisfy other traits. Multiple
requirements combine with `&` (D-029).

```nitpick
trait:Ordered = Equatable & {
    func:compare = int32(Self:a, Self:b);
};
```

Requirements are enforced **transitively**: if `C` requires `B` and `B` requires
`A`, implementing `C` requires implementations of both.

### 2.3 Associated Types (`assoc`)

A trait may declare an associated type; an impl binds it to a concrete type.

```nitpick
trait:Iterator = {
    assoc:Item;
    func:next = Item(Self:self);
};

impl:Range:Iterator = {
    assoc:Item = int32;
    func:next  = int32(Range:self) { pass(self.current); };
};
```

Associated types may carry defaults — `assoc:Error = string;` — inherited by any
impl that omits the binding.

> The keyword is **`assoc`**, not `Type` (D-028). `Type` used to declare a namespace
> (`FORMAL_DRAFT` 02 §2.7.4) and cannot also mean "associated type" inside a trait
> body — that is a construct changing meaning by context, and it made
> `Type:Foo = { … };` genuinely ambiguous between an associated type bound to an
> anonymous struct and a nested namespace. **D-088 has since removed the namespace
> construct outright** — `mod` already did that job and can additionally name a
> file — so `Type` is not a keyword at all any more.

### 2.4 Inherent Impls

Methods may attach to a type with no trait involved:

```nitpick
impl:Point = {
    func:magnitude = flt64(Point:self) {
        pass(flt64_sqrt(flt64(self.x * self.x + self.y * self.y)));
    };
};
```

Inherent methods dispatch **statically via UFCS** — `p.magnitude()` resolves to
`Point_magnitude(p)`. This is an independent confirmation that UFCS is part of the
language (D-006).

### 2.5 Derive Attributes

```nitpick
#[derive(PartialOrd, ToString, Eq, Hash, Clone, Debug, Ord)]
struct:Config = {
    int32:priority;
    string:name;
};
```

Supported, and there are **seven** (D-123): `PartialOrd`, `ToString`, `Eq`,
`Hash`, `Clone`, `Debug`, `Ord`.

`Ord` compares **in declaration order**, so reordering a struct's fields is a
semantic change. `Hash` combines with **FNV-1a** — specified rather than left to
the implementation, because a derived hash that varies between builds is not
something a verified compiler can have. Real cryptography is `ncrypto`, a
separately audited artifact; this is a hash for a map.

A refusal **names the field that blocks it**, not the type.

### What each one is

The seven are declared in the **prelude** (D-132) — `src/prelude/prelude.npk`,
ordinary Nitpick that every module has bound into it the way `use "prelude.npk".*`
would bind it. They are not magic; they are an import nobody has to write, and a
program may not declare a name the prelude declares.

| Trait | Method |
|---|---|
| `Eq` | `func:eq = bool(Self:self, Self:other);` |
| `Ord` | `func:cmp = Ordering(Self:self, Self:other);` |
| `PartialOrd` | `func:partial_cmp = Ordering?(Self:self, Self:other);` |
| `Clone` | `func:clone = Self(Self:self);` |
| `Hash` | `func:hash = uint64(Self:self);` |
| `ToString` | `func:to_string = string(Self:self);` |
| `Debug` | `func:debug = string(Self:self);` |

`Ordering` is a prelude enum — `Less`, `Equal`, `Greater`. An ordering is **not an
integer**: the prototype returned `int32` meaning less, equal or greater by sign,
which is three meanings in one number and the shape D-036 rejects for `bool` and
`char`.

**`PartialOrd` is not a second spelling of `Ord`.** It answers a different question,
and floats make it a real one: they are IEEE 754 with `nan` and no trap
(`OP_REFERENCE` §4), so two `flt64`s genuinely may not compare and a total `cmp`
over one would have to lie.

### What is generated today, and what is refused

`Eq`, `Ord`, `PartialOrd`, `Clone`, `ToString` and `Debug` generate for a struct;
`Eq`, `Clone`, `ToString` and `Debug` generate for an enum. Two are refused with the
reason rather than guessed at (D-133):

- **`Ord`/`PartialOrd` on an enum** — ordering a variant means comparing its tag,
  and `<` on an enum is refused by the type checker.
- **`Hash`** — FNV-1a folds bytes, and nothing exposes a `string`'s bytes to Nitpick
  source. A derived hash that skipped string fields would vary with data it ignored.

> **`Default` and `Display` were listed here and are removed (D-123).**
>
> **`Default`** would have the compiler choose values that carry meaning, and
> several obvious choices are false in this language: `fd` zero is *stdin*, not "no
> descriptor" (D-042); `tbb32` zero claims "no error"; a pointer zero is `NULL`. It
> reintroduces through an explicit door exactly what D-010 removed — a value
> appearing that nobody chose. If a type wants a default, someone writes one.
>
> **`Display`** was a second name for `ToString`'s job. Both would generate a
> function returning `string`, since D-053 moved formatting to `&{ }`
> interpolation, and two spellings for one thing is the cost the blueprint
> philosophy exists to avoid.

> Chapter 13 writes this `@derive(…)`. **Wrong** — `@` is address-of and nothing
> else, so `@derive` reads as "the address of derive". `derive` annotates a
> declaration, so it takes the attribute form `#[…]` (D-020).

### 2.6 Blanket Impls

A blanket impl implements a trait for every type satisfying a bound:

```nitpick
impl:<T: Printable>:Loggable = {
    func:log_str = string(T:self) { pass("[LOG]"); };
};
```

`T` is substituted with each concrete type implementing `Printable`. **Concrete
impls take priority** over blanket-generated ones — expressed as the *order* the
method lookup asks its question, so there is never a moment at which two
candidates exist and something has to choose (D-111).

Three rules complete the form, all D-111:

- **At most one blanket impl per trait.** The language has no negative bounds, so
  nothing stops a type satisfying two different bounds — `impl:<T: A>:Loggable`
  and `impl:<T: B>:Loggable` genuinely overlap, and with no specialization
  (D-064 §7) there is no rule to choose by. Both are named in the diagnostic. A
  blanket impl beside a *concrete* one is not a conflict: overlapping is the point.
- **A blanket impl must name a trait.** `impl:<T: Bound> = { … };` parses, and
  would add methods to every type satisfying a bound — methods on types the writer
  does not own.
- **A blanket impl does not apply to itself.** Otherwise
  `impl:<T: Loggable>:Loggable` would be true of everything by circular reasoning.

`Self` inside the block is the parameter, and the block is checked for
completeness, supertraits and duplicate names exactly as a concrete impl is.

> Chapter 13 spells this `impl:Loggable:for:T:where:Printable = { … };`, making
> `where` a colon-separated path segment — a second, unrelated syntactic role for
> a keyword that otherwise guards `pick` arms as a parenthesized expression. The
> bound form above applies the same `<T: Bound>` rule used everywhere else
> (D-030), and the type-first ordering of D-031.

### 2.7 Data Hiding and Opaque Types

Struct fields follow module visibility — private by default, exported with `pub`.
That is the mechanism for hiding a representation written in Nitpick.

`opaque` is a **different thing and is not a second way to do it**: it declares a
type whose representation belongs to a DRIVER, and it is legal **only inside an
`extern` block** (D-066, as narrowed by D-149) — a typed wire handle the Bridge
round-trips by value, minted and honoured on the driver's side of the process
boundary.

```nitpick
extern:"storage_driver" = {
    opaque struct:DbHandle;
    func:db_open = DbHandle(int8[]:path);
    func:db_rows = int64(DbHandle:h);
}
```

**Opaque values have no value semantics** — they cannot be copied, because a copy
would have to know the size, which is exactly what the type withholds:

```nitpick
Handle:h  = handle_create();     // initialization from a call — fine
Handle:h2 = h;                   // rejected — OPAQUE-COPY-001
```

Passing borrows (D-004); transferring ownership is `move(h)` (D-065), for which an
opaque handle is the canonical case.

> The standalone `opaque:DatabaseHandle;` form previously shown here is **struck**
> (conflict 49, D-066). It has zero occurrences in the prototype, while
> `opaque struct:` has its own test directory, a negative test, and a K-semantics
> `loadStructs` rule.

---

## 3. Generics

Zero-cost generic programming via **monomorphization**. Type parameters are
declared in angle brackets **after** the name.

```nitpick
struct:Container<T> = {
    T:value;
};

func:extract_value<T> = T(Container<T>:c) {
    pass c.value;
};
```

### 3.1 Bounds

Bounds attach with `:` inside the brackets; multiple bounds combine with `&`
(D-029).

```nitpick
func:process<T: Renderable & Serializable> = NIL(T:item) {
    item.render();
};
```

> Chapter 06 §6.3 places parameters **before** the name — `func<T: …>:process` —
> which no other declaration in the language does. Struck (D-030). Chapter 13
> never shows a bounded generic at all, so the form above is written rather than
> adopted.

### 3.1.1 Value parameters

Parameters may carry a compile-time **value** as well as a type, marked
`comptime` (D-064). D-056's lock levels are the motivating case.

```nitpick
struct:Mutex<T, comptime int32:LEVEL> = { … };

Mutex<Config, 2>:cfg_lock;          // use site supplies type and value
```

> `comptime` is required rather than a bare `int32:LEVEL` because `<T: Renderable>`
> and `<int32:LEVEL>` would otherwise put the newly introduced name on opposite
> sides of the same colon — unreadable without already knowing whether the other
> identifier is a trait or a type.

Four rules govern the argument, all D-109:

- **A value argument stops below the binary operators.** Inside `<…>` a `>`
  *closes the list*, so a comparison must be parenthesized: `Mutex<Config, (A > B)>`.
  A literal, a name, or a unary expression needs no parentheses.
- **An identifier is parsed as a type.** `Mutex<Config, LEVEL>` cannot be told
  apart at the parser, so which kind it means is decided where the parameter list
  is known — and supplying the wrong kind says *which kind was wanted*.
- **Only an integer literal is constant at this rung.** The same rule and the same
  sentence `int32[N]` uses; general constant folding is `comptime`'s, in cycle 0.6.
- **An unsuffixed literal takes the parameter's declared type; a suffixed one must
  already be it.** `Mutex<Config, 2i64>` against `comptime int32:LEVEL` is a
  mismatch somebody wrote, not a width to be adjusted — there is no implicit
  widening anywhere else either.

Two values are two arguments and therefore two types: `Mutex<T, 2>` and
`Mutex<T, 3>` are different lock levels, which is what D-056 exists to keep apart.

### 3.2 Checking happens at the definition

**A generic body is type-checked once**, treating each parameter as an opaque type
satisfying exactly its declared bounds and nothing else. Instantiation checks only
that the concrete arguments satisfy those bounds; the body is not re-checked
(D-064).

The consequence to know: **a body may not use any capability its bounds do not
declare.** There is no duck typing. That restriction is what makes the body
checkable once rather than once per instantiation — which matters directly,
because Astrée analyses monomorphized output and a body correct for the
instantiations that happen to exist may still be wrong in general.

Three rules follow, all D-107:

- **A bound set is transitively closed.** `T: Ordered` where `Ordered = Equatable
  & { … }` guarantees `Equatable`, because §2.2 makes a supertrait a requirement
  on the implementing type — so the body may use `Equatable`'s methods.
- **UFCS does not reach a free function through a parameter.** `p.magnitude()`
  and `magnitude(p)` are the same call for a concrete receiver (D-006), but a
  free function taking a `T` is not a capability `T` declares. Reaching it would
  be duck typing arrived at by omission, and it would break at an instantiation
  rather than at the definition — reported against a caller who supplied a
  perfectly reasonable type.
- **A `comptime` value parameter is not a type.** `LEVEL:x` is as wrong as
  `MAX:x`; the marker is what makes the two kinds of parameter readable apart.

A parameter **shadows** a module-level type of the same name, being the inner
binding. The diagnostic for an undeclared capability **names the bounds the
parameter does declare**, because the fix is either the call or the bound list and
the compiler already knows which are possible.

### 3.3 Instantiation

Type arguments are **inferred** at the overwhelming majority of call sites, and
nothing is written:

```nitpick
int32:val = extract_value(my_container);
```

Where inference cannot decide, explicit type arguments in **expression position
are always the turbofish** (D-064):

```nitpick
int32:val = extract_value::<int32>(my_container);
```

| Position | Form |
|---|---|
| Type | bare brackets — `Handle<Node<int64>>:h;`, `struct:Container<T>` |
| Expression | turbofish, always — `extract_value::<int32>(c)` |
| `#`-builtin | bare brackets — `#size_of<int32>()`, `#wild_ptr<T>(addr)` (D-020) |

> The earlier form — implicit `f<int32>(x)` with the turbofish as a *"fallback
> where ambiguous"*, resolved by unspecified *"lookahead"* — is **struck**. A
> fallback spelling requires the author to know whether this particular call site
> is ambiguous before knowing which of two forms to write, and "lookahead" names
> a technique rather than a rule. `#`-builtins keep bare brackets because the `#`
> sigil is itself the disambiguator, visible in the token — the same principle
> D-046 settled for `!`.

### 3.4 Nested Generics

Closing brackets need no separating space — the lexer splits `>>`.

```nitpick
Handle<Node<int64>>:my_handle;
```

> `>>` is also the right-shift operator. A type-argument list is opened by a type
> position or by `::<`, and nowhere else, so `>>` splits **only** inside one and is
> a right-shift everywhere outside. No lookahead, no speculative parse, no third
> case (D-064, narrowing the D-030 note).

### 3.5 Monomorphization

Instantiation depth is capped at **64**; exceeding it is a compile error with the
instantiation stack printed, never a silent truncation. Instantiations are
deduplicated by mangled name, a generic's body is exported with its module so
using modules can instantiate it, and **mangled names are readable and
reversible** — no hash — because an auditor reading a verification report has to
map a symbol back to its declaration by inspection (D-064).

There is **no specialization**: one instantiation cannot be given a different body
from another.

### 3.4 Arenas Embedded in Generic Structs

Generic structs may embed `arena<T>` fields, with chained UFCS access. The
compiler resolves the arena field offset from the struct's type information at
code-generation time.

```nitpick
struct:Header<T> = {
    arena<Node<T>>:node_arena;
};

Header<int64>:hdr;
Handle<Node<int64>>:h1 = hdr.node_arena.alloc(my_node);
```

> Chapter 13 §13.3.3 additionally shows pointer access as
> `ptr->node_arena.alloc(…)`. **Struck** — `.` handles all member access and
> auto-dereferences when the operand is a pointer; `->` is type-position only
> (D-006). The pointer case is written identically to the value case.

---

## 4. Coherence and Object Safety

### 4.1 Coherence

There is **at most one implementation** of a given trait for a given type.
Overlapping implementations are a compile error.

### 4.2 Object Safety

A trait may be used as `dyn Trait` only if (D-157, restated from three
earlier contradictory phrasings):

1. every method takes a `self` parameter — no static methods;
2. **`Self` appears nowhere but the receiver** — not in a later parameter,
   not in the return, not NESTED anywhere in a type tree (`Optional<Self>`,
   `Self[]`, `Handle<Self>`, `Self->` all disqualify): behind a vtable the
   erased value would be read at the wrong layout, and the rule admits no
   size-based exceptions;
3. **no generic methods** — type parameters and comptime parameters alike: a
   vtable slot holds one address, and a generic method is a family of them.

A method whose signature mentions an **associated type** also disqualifies
the trait (D-160) — the projection's layout is unknowable behind erasure,
the same argument as rule 2.

Non-object-safe traits cannot be used with `dyn`.

---

## 5. Dispatch

### 5.1 Static (Monomorphization)

The default. Trait implementations resolve at compile time, which keeps the
abstraction zero-cost and lets LLVM inline aggressively.

### 5.2 Dynamic (`dyn`)

Explicit opt-in to runtime polymorphism, constructing a fat pointer
(`{ data_ptr, vtable_ptr }`, 16 bytes on 64-bit). Dispatch (D-158): a call
through a `dyn` reaches the methods DECLARED BY THE NAMED TRAITS — own
lists only; supertrait methods are not reachable (construct a `dyn Super`
from the concrete value instead), and two named traits declaring one method
name is a compile-time ambiguity. The vtable is per (impl, trait), indexed
by trait declaration order, and its entries are per-impl adapter thunks —
the caller passes the data pointer, the thunk loads the concrete receiver:

```nitpick
Message:msg = Message{ id: 1i32 };
dyn Serializable:obj = msg;
```

### 5.3 Multi-bound `dyn`

```nitpick
dyn Drawable & Serializable:obj = msg;
```

`dyn A & B` is assignable to `dyn A` — widening by dropping bounds. Each trait
must be object-safe. The ABI (D-159): `dyn` over N traits is
`{ data, vt_1 … vt_N }` — (N+1)×8 bytes — with the bounds CANONICALLY
ORDERED (sorted by trait name) at type interning, so every vtable word has a
statically known slot and the widening is a value rebuild copying the data
word plus the retained traits' words. No runtime tables.

> Chapter 13 uses `+` here while using `&` for supertraits and bounds. Unified on
> `&` (D-029).

> **`dyn` obscures the control-flow graph**, so it raises warnings under strict
> safety auditing profiles. Prefer static dispatch on any path where verification
> matters — which, for robotics-facing code, is most of them.

---

## 6. Relationship to other documents

- **Casting** was documented in chapter 13 §13.6 and is **not** reproduced here.
  `=>` and `=>!` are the only cast forms (D-021); `@cast<T>` and
  `@cast_unchecked<T>` are removed. `=>` is a **compile-time error** where data
  loss is possible, not a warning. Integer-to-pointer casting is illegal outside
  `#wild_ptr<T>(addr)` in `wild` context (D-019). See `OP_REFERENCE.md` §7.
- **Closures** (chapter 06 §6.4) are removed (D-018). Lambdas without capture
  remain as function values.
- `TYPE_REFERENCE.md` §18 documents the `dyn Trait` fat-pointer layout.
