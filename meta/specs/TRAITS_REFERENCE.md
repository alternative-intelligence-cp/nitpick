# Nitpick Traits, Generics, and Dispatch

Adopted from `FORMAL_DRAFT/13_traits_and_generics.md` with corrections applied for
D-001…D-030. Chapter 06's overlapping trait and generic material is superseded —
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

impl:Serializable:for:Message = {
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

impl:Iterator:for:Range = {
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
impl:for:Point = {
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
impl:Loggable:for:<T: Printable> = {
    func:log_str = string(T:self) { pass("[LOG]"); };
};
```

`T` is substituted with each concrete type implementing `Printable`. **Concrete
impls take priority** over blanket-generated ones.

> Chapter 13 spells this `impl:Loggable:for:T:where:Printable = { … };`, making
> `where` a colon-separated path segment — a second, unrelated syntactic role for
> a keyword that otherwise guards `pick` arms as a parenthesized expression. The
> bound form above applies the same `<T: Bound>` rule used everywhere else
> (D-030).

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

### 3.2 Instantiation

Generic functions instantiate implicitly through ordinary call syntax; the
compiler uses lookahead to distinguish a generic call from a `<` comparison.

```nitpick
int32:val = extract_value<int32>(my_container);
```

Where that is ambiguous, the turbofish `::<T>` is the fallback:

```nitpick
int32:val = extract_value::<int32>(my_container);
```

### 3.3 Nested Generics

Closing brackets need no separating space — the lexer splits `>>`.

```nitpick
Handle<Node<int64>>:my_handle;
```

> `>>` is also the right-shift operator. This is a **known lexer/parser
> interaction** and must be handled explicitly rather than left to the
> implementer (D-030).

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
