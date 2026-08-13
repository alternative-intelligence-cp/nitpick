# Grammar Chapter Adoption — Conflict List

Full read of `FORMAL_DRAFT` chapters **01 (Lexical)**, **04 (Expressions)**, and
**05 (Statements & Control Flow)** against D-001…D-021 and the carried-over
reference specs.

> **Correction.** `FORMAL_DRAFT_AUDIT.md` §6 recommended adopting these three
> "immediately — no conflicts found". That assessment came from a structural skim,
> not a full read, and it was **wrong**. The chapters carry ~25 conflicts. Four are
> semantic disagreements with the existing reference specs that require a decision;
> the rest are mechanical.

---

## Part A — Semantic conflicts requiring a decision — **ALL RESOLVED**

These were not stale text. Two documents described **different languages**, and one
of each pair had to be chosen.

| | Question | Resolution |
|---|---|---|
| **A1** | `till` / `loop` semantics | **D-022** — both are counted. `till(limit, step)` ascends from 0; `loop(start, limit, step)` infers direction from the bounds. **`step` is always positive.** `FORMAL_DRAFT` 05's do-while/infinite reading is struck. |
| **A2** | `prove` compile-time or runtime | **Compile-time Z3 obligation.** `FORMAL_DRAFT` 05 §5.8 is the outlier and is struck. |
| **A3** | C-style `for` | **D-023** — range form only, with a required typed binding. |
| **A4** | raw / multi-line strings | **D-024** — both retained. Chapter 01's omission described a prototype parser gap, not a design decision. |

### A1. `till` and `loop` mean completely different things

| | `CONTROL_REFERENCE.md` §2.4 | `FORMAL_DRAFT` 05 §5.4.3–5.4.4 |
|---|---|---|
| `loop` | **counted iteration** — `loop(start, limit, step)`, counter exposed as `$` | **infinite loop** — `loop { }`, exited via `break` |
| `till` | **counted iteration from 0** — `till(limit, step)`, counter as `$` | **do-while** — condition evaluated *after* the body |

Both spellings cannot be right. This affects the parser directly and it changes
what `$` means.

### A2. `prove` — compile-time proof or runtime assertion?

- `VERIFICATION_REFERENCE.md` §1.2 and `FORMAL_DRAFT` 12.5.1: `prove` is a
  **compile-time Z3 proof obligation**, path-condition aware; failure to prove
  **rejects compilation** with a counterexample.
- `FORMAL_DRAFT` 05 §5.8: "**`prove(cond);`**: A **runtime** assertion. If the
  condition is false, the program panics immediately."

These are different features. The verification chapters are far more detailed and
internally consistent, which argues they are correct — but a runtime-checked
variant may also be wanted, under a different name.

### A3. C-style `for` loops

`FORMAL_DRAFT` 05 §5.4.2 shows both `for (int32:i = 0; i < 10; i++)` and
`for (i in 0..10)`. `CONTROL_REFERENCE.md` §2.3 shows only the range form, and
with a **typed** binding: `for (int64:i in 1..3)`.

Two sub-questions: is the C-style three-clause form supported at all, and does the
range form require an explicit type on the loop variable?

### A4. Raw and multi-line string literals

- `OP_REFERENCE.md` §9 lists `r"..."` (raw) and `"""..."""` (triple-quote,
  multi-line) as **current** operators.
- `FORMAL_DRAFT` 01 §1.6.3: "While older versions of Nitpick supported raw strings
  and multi-line strings, the current compiler (v0.61.82) parser throws a syntax
  error if these are encountered. They have been **intentionally omitted** from
  this grammar."

---

## Part B — Conflicts with settled decisions (mechanical)

### Chapter 01 — Lexical Analysis

| # | Location | Issue | Fix |
|---|---|---|---|
| 1 | §1.4 `MemoryQualifier` | includes `"gc"` | remove (D-003) |
| 2 | §1.5 `Operator` | `=>` present, **`=>!` missing entirely** | add — `=>!` is one of only two cast forms (D-021) |
| 3 | §1.5 `Operator` | `#` listed among value operators | reclassify as the compiler-directive sigil (D-020) |
| 4 | §1.6.1 `SentinelLiteral` | includes `"unknown"` as a writable literal | `unknown` is compiler-assigned only (`TYPE_REFERENCE` §27) |
| 5 | §1.4 `BuiltinType` | `fix256` | rename to `dim256` (`SPEC_GAPS` §3 — rename still owed project-wide) |
| 6 | §1.4 `BuiltinType` | `tbb8`…`tbb64` only | add `tbb128`, `tbb256` (`TYPE_REFERENCE` §6) |
| 7 | §1.4 keywords | no `fails`, `on`, `never` | required by the FFI error contracts (D-002) |
| 8 | §1.4 `BuiltinHelper` | no `is_err` | required by `tbb` ERR checking (D-008) |
| 9 | §1.6.2 vs `FORMAL_DRAFT` 02 §2.4 | ternary literal is suffix-form here (`01Tt` + `"t"`), prefix-form there (`0t1T0`) | pick one |

### Chapter 04 — Expressions

| # | Location | Issue | Fix |
|---|---|---|---|
| 10 | §4.2 precedence 1 | `->` listed as a **postfix member access** operator | remove — `->` is type-position only (D-006) |
| 11 | §4.5 | "**`->` (Dereferencing Member Access)**: Dereferences a pointer and accesses the member field" | strike — `.` handles all member access and auto-dereferences (D-006) |
| 12 | §4.2 precedence 4, §4.5 | `#` as a unary **pin** operator on GC objects | strike (D-020) |
| 13 | §4.2 precedence 3 | cast level lists `=>` only | add `=>!` (D-021) |
| 14 | §4.5 | "`@` … yielding a typed pointer (`T->`)" | `@` yields a **second-class borrow**, not a first-class pointer (D-004) |
| 15 | §4.4.1 | "**`result ?! `**" — shown with **no argument** | takes exactly one `tbb32` error code (D-009) |
| 16 | §4.6 | `sys!!!` documented | removed (D-001) |
| 17 | §4.4 | "TBB (**Three-State Bit**)" | TBB is **Twisted Balanced Binary** |
| 18 | §4.1 | "chained assignments (`a = b = 5`)" | tension with 05 §5.3.1 rejecting `=` in conditions; also a blueprint question — worth deciding whether assignment-as-expression survives |

### Chapter 05 — Statements and Control Flow

| # | Location | Issue | Fix |
|---|---|---|---|
| 19 | §5.9 | "`defer` … executed when the scope exits (whether normally, by a `return`, or **via a panic**)" | **`defer` does not run on a trap** (D-014) |
| 20 | §5.7 | "`pass expr;` … sugar for `return ok(expr);`" and "`fail expr;` … `return err(expr);`" | `ok()` is the taint-clearing builtin, not a `Result` constructor; `err()` is undefined. `pass`/`fail` construct `Result` directly (`TYPE_REFERENCE` §11.2) |
| 21 | §5.6 | `pick` arms shown **without** separating commas | `CONTROL_REFERENCE` §1.2 requires commas between arms |
| 22 | §5.6.3 | `(!)` "unreachable" marker introduced | interacts with `pick` exhaustiveness and the **required `ERR:` arm** for `tbb` selectors (D-008 §5.1) — needs reconciling, not just adopting |
| 23 | §5.6.3 | `give` yields a value out of a `pick` | not in `CONTROL_REFERENCE`; also contradicts §5.1's "statements do not yield values" |
| 24 | §5.1 vs 04 §4.1 | "statements do not yield values" vs "almost every construct is an expression" | the two chapters disagree on the fundamental expression/statement split |
| 25 | §5.8 | `prove` — see **A2** | |

---

## Part C — What these chapters add that is genuinely new and wanted

Not everything is a conflict. These are worth keeping:

- **Full W3C-EBNF token grammar** (01) — the parser foundation, and nothing in the
  carried-over set covers it.
- **Complete operator precedence table** (04 §4.2), 18 levels — likewise absent.
- **Loop labels** (05 §5.5) — `outer: loop { … break outer; }` for breaking out of
  nested loops.
- **`pick` destructuring** (05 §5.6.1) for enums and structs.
- **Template literal tokenization** (01 §1.6.4) — `TEMPLATE_START` / `TEMPLATE_PART`
  / `INTERP_START` / `INTERP_END` / `TEMPLATE_END`.
- **Numeric literal bases** (01 §1.6.2) — suffix-form hex/binary/octal/ternary/nonary
  with `_` separators.
- **`if` rejects assignment in conditions** (05 §5.3.1) and **stray-`else` detection**
  (§5.3.2) — both good pedantry, consistent with the language's posture.
- **`?|` / `defaults`** scoped expression fallback (04 §4.4.2).

---

## Recommended sequence

1. Decide **A1–A4** — they change what the parser must accept.
2. Apply the mechanical fixes in Part B during adoption.
3. Fold the Part C additions into the existing reference docs, or adopt the
   chapters wholesale and retire the overlapping sections of
   `CONTROL_REFERENCE.md` and `OP_REFERENCE.md`.

Note that chapter 05 overlaps `CONTROL_REFERENCE.md` substantially, and chapter 04
overlaps `OP_REFERENCE.md`. Adoption is a **merge**, not an addition — which is
why the A-list has to be settled first.

---

# Chapter 13 (Traits & Generics) + Chapter 06 (Functions) — Conflict List

Read in full, together, because they overlap and disagree.

## Part D — Design problems requiring a decision

### D1. `Type` means two different things

Same syntax, two unrelated constructs, disambiguated only by where it appears:

| Context | Meaning | Source |
|---|---|---|
| top level | **namespace / module grouping** — `Type:Counter = { … };`, holds functions and structs, no member variables | `FORMAL_DRAFT` 02 §2.7.4 |
| inside `trait` / `impl` | **associated type** — `Type:Item;` declares, `Type:Item = int32;` binds | `FORMAL_DRAFT` 13 §13.2.3 |

This is a direct blueprint violation — a construct changing meaning by context is
the exact thing the philosophy exists to prevent. It is also genuinely ambiguous
to parse: inside a trait body, `Type:Foo = { … };` could be either an associated
type bound to an anonymous struct, or a nested namespace.

### D2. Traits are combined with `&` in some places and `+` in others

| Purpose | Symbol | Source |
|---|---|---|
| supertrait requirement | `&` — `trait:Ordered = Equatable & { … };` | 13 §13.2.2 |
| generic bound | `&` — `T: Renderable & Serializable` | 06 §6.3 |
| multi-bound `dyn` | **`+`** — `dyn Drawable + Serializable:obj` | 13 §13.5.3 |

Three places meaning "this type satisfies several traits", two different symbols.

## Part E — Conflicts with settled decisions (mechanical)

| # | Location | Issue | Fix |
|---|---|---|---|
| 26 | 13 §13.2.5 | `@derive(Default, Eq, …)` | `#[derive(…)]` — `@` is address-of only, and derive annotates a declaration, so it takes the attribute form (D-020) |
| 27 | 13 §13.3.3 | arena access shown as `ptr->node_arena.alloc(…)` | `.` handles all member access and auto-dereferences (D-006) |
| 28 | 13 §13.6.1–13.6.2 | `@cast<T>` / `@cast_unchecked<T>` presented as equal alternatives to `=>` | both removed; `=>` and `=>!` are the only cast forms (D-021) |
| 29 | 13 §13.6.1 | destructive casts "emit warnings" (`NITPICK-062`, `NITPICK-063`) | `=>` is a **compile-time error** where loss is possible; `=>!` is the opt-out (D-021) |
| 30 | 13 §13.6.3 | "use `int64` to track opaque C addresses rather than casting to typed pointers" | struck — `int64` addresses defeat leak checking and escape analysis (D-012); use `#wild_ptr<T>(addr)` (D-019) |
| 31 | 06 §6.1, §6.6 | examples use `return a + b;` / `return bytesRead;` | `pass` / `fail` are the return keywords |
| 32 | 06 §6.1, §6.3 | `void` used as a Nitpick return type | `void` is `extern`-only; use `NIL` |
| 33 | 06 §6.1.2 | "`extern` … return bare types (`T`) … not wrapped in `Result<T>`" | all functions including `extern` return `Result<T>` with mandatory error contracts (D-002) |
| 34 | 06 §6.4 | lambdas/closures capturing environments via `npk_gc_alloc` | closures removed (D-018) |

## Part F — Chapter 06 and 13 disagree with each other

| Construct | Chapter 06 | Chapter 13 |
|---|---|---|
| trait declaration | `trait:Reader { … };` — **no `=`** | `trait:Serializable = { … };` |
| impl | `impl Reader for FileStream { … }` — space-separated | `impl:Serializable:for:Message = { … };` — colon-separated |
| generic parameters | **before** the name, with bounds: `func<T: Renderable & Serializable>:process = void(T:item)` | **after** the name, no bounds shown: `func:extract_value<T> = T(Container<T>:c)` |

Chapter 13's forms match the house style used everywhere else — `func:name = `,
`struct:name = `, `Rules<T>:name = ` — and `SPEC_GAPS` §3 independently specifies
the after-the-name placement (`struct:Name<T> = { … }`, `func:my_func<T> = …`).
**Chapter 06 is the outlier on all three.**

Note chapter 13 never shows a *bounded* generic, so wherever the bound syntax
lands it has to be written, not copied.

## Part G — Gaps

- **`Self` is not a keyword.** Used six times in chapter 13 (`func:to_bytes = buffer(Self:self);`)
  but absent from the chapter 01 keyword list and from `LEXICAL_REFERENCE.md`.
- **`where` has two unrelated syntactic forms**: a `pick` arm guard,
  `MyMacro!(a, b) where (a > b)`, and a path component in blanket impls,
  `impl:Loggable:for:T:where:Printable`. Both express constraints, but one is a
  parenthesized expression and the other a colon-separated segment.
- **`>>` splitting**: `Handle<Node<int64>>` requires the lexer to split `>>`,
  which is also the right-shift operator. A known parser interaction that needs
  stating explicitly rather than being left to the implementer.

## Part H — What chapter 13 adds that is wanted

Default methods, supertraits, associated types, inherent impls, derive macros,
blanket impls, `opaque` types, coherence (at most one impl per trait/type pair),
object-safety rules for `dyn`, monomorphization, and multi-bound `dyn` with
widening. None of this exists in the carried-over set, and traits are referenced
by `TYPE_REFERENCE.md` §18 with no governing document.

Also confirms **UFCS** independently: 13 §13.2.4 states inherent methods are
"dispatched statically via UFCS: `Point_magnitude(p)`" (D-006).
