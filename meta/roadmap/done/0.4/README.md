# Cycle 0.4 — Type system and checking

`TYPE_REFERENCE.md` and `TRAITS_REFERENCE.md`, written in Nitpick.

Cycle 0.2 turned one file into an AST. Cycle 0.3 turned a program into a symbol
table — it knows what names **exist**. This cycle is where the compiler learns
what things **are**, and it is the largest in Phase A.

## The line 0.3 deliberately stopped at

Name resolution binds `p` and leaves `.x` alone, because saying anything about
`.x` needs `p`'s type. Every one of those deferrals lands here:

| Deferred | Needs |
|---|---|
| `p.x` | `p`'s type, and its fields |
| `p.method()` | the receiver's type, then UFCS resolution (D-006) |
| `Net.Disconnect` vs `math.sqrt` | whether the base is an enum or a module |
| a `const` cycle through a struct | field types, and whether a `->` breaks it |
| every type name in every signature | this cycle, from the first subcycle |

## What makes this cycle different from the last two

**Almost nothing here is a walk.** 0.2 and 0.3 were structural: visit every node,
do the obvious thing, and the hard part was not forgetting a case — which is why
`check_kinds_reachable` and `resolve_audit` were worth building.

Type checking is not like that. The hard part is the **rules**, and a rule that is
subtly wrong produces a program that compiles and misbehaves. So the mechanical
completeness checks matter less here, and the **negative** tests matter far more:
what must be *refused* is the specification.

## The three rules that shape everything else

### `Result<T>` is universal (D-013)

Every function returns `Result<T>` except `main` and `failsafe`. The AST records
the **success** type — `func:f = int32()` declares `int32` — and the wrapping is
this cycle's job, not the parser's.

The reason is the blueprint philosophy's first facet, and it is worth keeping in
view while implementing: **a caller never has to ask whether this particular
function needs error handling.** One rule, no exceptions to remember.

### A type's meaning outranks its representation

`bool` and `char8` are one byte each and are **not integers**. No arithmetic, no
implicit conversion. `tbb32` is `i32` and is an error-code type, not a number.
`fd`, `pid`, `tid`, `uid`, `gid` are distinct types that compare and do not add
(D-042).

Every one of those is representationally identical to something else, which is
exactly why the type system has to hold the line — nothing downstream will.

### A generic body is checked ONCE, at its definition (D-064)

Treating each parameter as an opaque type satisfying exactly its declared bounds
and **nothing else**. Instantiation checks only that the arguments satisfy the
bounds; the body is never re-checked.

The consequence is the part to implement deliberately: **a body may not use any
capability its bounds do not declare.** There is no duck typing. And the reason
is not elegance — Astrée analyses monomorphized output, so a body that happens to
be correct for the instantiations a program contains may still be wrong in
general, and that is precisely the kind of gap a single verification run must not
have to find.

## Subcycles

| | Topic |
|---|---|
| **0.4.0** | The type representation — canonical types, layouts, and type identity |
| **0.4.1** | Type resolution — `TypeNode` → `Type`, including 0.2.8's reachable corners |
| **0.4.2** | Expression typing — literals, operators, and the semantic-type rules |
| **0.4.3** | Casts — `=>` refuses possible loss, `=>!` is the sole opt-out (D-021) |
| **0.4.4** | `Result<T>` — universality, and the unary family (`raw`, `drop`, `relay`, `?`, `?!`) |
| **0.4.5** | Member access, calls, and UFCS — the questions 0.3.5 left |
| **0.4.6** | Traits and impls — coherence, object safety, associated types |
| **0.4.7** | Generics — checked at the definition (D-064) |
| **0.4.8** | Diagnostics, the suites, and closing the cycle |

## What "done" looks like

`tools/check.npk` takes a root `.npk` file and reports every type error in the
program, with spans. **At the end of Phase A the artifact is a checker**: it
validates sources completely and emits nothing.

Cycles 0.5 and 0.6 add the analyses and macros on top of it; Phase B then makes it
emit.

## Two things to decide early rather than discover

Neither is settled in the spec set, and both will block a subcycle if left.

### What makes two types the same type

`TYPE_REFERENCE.md` gives layouts, not an identity rule. Is `int32` equal to
`int32` by name, by structure, or by interned pointer? The answer decides how
every comparison in the checker is written, and changing it later touches all of
them.

**Recommendation: canonical, interned types compared by index** — one `int32`
object for the whole program, so equality is an integer compare and no structural
walk exists to get subtly wrong. It also makes a type usable as a map key when
0.4.7 needs to deduplicate monomorphized instantiations.

### Whether a bare `Result<T>` is spellable in a signature

Every function already returns one. Can a programmer *write* `Result<int32>` as a
return type, and if so does it mean `Result<Result<int32>>` or the same thing?

**Recommendation: writing it is an error that says so.** `func:f = Result<int32>()`
is either redundant or a nesting nobody wants, and both readings are worse than
refusing the spelling. `Result<T>` remains writable in a *variable* declaration,
where it is the ordinary way to hold a call's outcome before unwrapping it.
