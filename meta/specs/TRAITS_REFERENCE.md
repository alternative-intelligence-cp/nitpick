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

> The keyword is **`assoc`**, not `Type` (D-028). `Type` declares a namespace
> (`FORMAL_DRAFT` 02 §2.7.4) and cannot also mean "associated type" inside a trait
> body — that is a construct changing meaning by context, and it made
> `Type:Foo = { … };` genuinely ambiguous between an associated type bound to an
> anonymous struct and a nested namespace.

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
#[derive(Default, PartialOrd, ToString, Eq, Hash, Clone, Debug, Ord)]
struct:Config = {
    int32:priority;
    string:name;
};
```

Supported: `Default`, `PartialOrd`, `ToString`, `Eq`, `Hash`, `Clone`, `Debug`,
`Ord`, `Display`.

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
impls take priority** over blanket-generated ones.

> Chapter 13 spells this `impl:Loggable:for:T:where:Printable = { … };`, making
> `where` a colon-separated path segment — a second, unrelated syntactic role for
> a keyword that otherwise guards `pick` arms as a parenthesized expression. The
> bound form above applies the same `<T: Bound>` rule used everywhere else
> (D-030), and the type-first ordering of D-031.

### 2.7 Data Hiding and Opaque Types

Struct fields follow module visibility — private by default, exported with `pub`.
To hide a representation completely, declare it `opaque`:

```nitpick
opaque:DatabaseHandle;
```

The consumer cannot see the internal memory layout.

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

A trait may be used as `dyn Trait` only if:

1. every method takes a `self` parameter — no static methods;
2. no method returns `Self` — its size is unknown behind `dyn`;
3. no method has comptime type parameters.

Non-object-safe traits cannot be used with `dyn`.

---

## 5. Dispatch

### 5.1 Static (Monomorphization)

The default. Trait implementations resolve at compile time, which keeps the
abstraction zero-cost and lets LLVM inline aggressively.

### 5.2 Dynamic (`dyn`)

Explicit opt-in to runtime polymorphism, constructing a fat pointer
(`{ data_ptr, vtable_ptr }`, 16 bytes on 64-bit):

```nitpick
Message:msg = Message{ id: 1i32 };
dyn Serializable:obj = msg;
```

### 5.3 Multi-bound `dyn`

```nitpick
dyn Drawable & Serializable:obj = msg;
```

`dyn A & B` is assignable to `dyn A` — widening by dropping bounds. Each trait
must be object-safe.

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
