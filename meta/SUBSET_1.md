# Subset 1

Established in cycle **0.0.1**. The authority on what the compiler's own source
may use until the backend climbs past this rung.

## What this is

**Subset 1 is the part of Nitpick the seed lowers**, and therefore the part the
compiler's own source is written in until stage 1 can lower more.

It is **not** a dialect to be escaped later (D-085). We write in our own language
from day one; subset 1 is an honest statement of what our own backend can lower
yet, and it **shrinks to nothing as the rungs are climbed.**

It is bounded from both sides, which is the whole difficulty:

- **Large enough** to write a complete compiler frontend in.
- **Small enough** that a throwaway seed can lower it.

## The principle that decided every borderline case

> **Push complexity into the compiler, away from the seed.**

The seed is the **least-audited** artifact in the chain — hand-generated, thrown
away, and the one place a Thompson-style problem could hide (D-085). The compiler
is the **most**-audited: it is checked by stage 1, verified in cycle 1.3, and is
the artifact of record.

So wherever a capability could live in either, it goes in the compiler, even when
that makes the compiler's source more verbose. Verbosity in an audited artifact is
cheap. Complexity in an unaudited one is not.

Two consequences follow, and they are the two biggest exclusions below:

- **No generics in subset 1** — the compiler hand-writes concrete collections
  instead of the seed implementing monomorphization.
- **No UFCS, no counted loops, no `?`-family operators beyond what is essential**
  — each is sugar, and sugar in the seed is unaudited code.

---

## 1. Included

### 1.1 Types

| Included | Notes |
|---|---|
| `int8` `int16` `int32` `int64` | |
| `uint8` `uint16` `uint32` `uint64` | |
| `bool`, `char8` | not integers; no arithmetic (semantic types) |
| `string` | `{ptr, i64 len, i64 cap}` |
| `cstring` | `{ptr, i64}` — NUL-terminated, not ours to free (D-049). Added by D-089: it is `main`'s parameter type |
| `T[]` slices | `{ptr, i64 len}` (D-070) |
| `T[N]` fixed arrays | value types, static bounds |
| `T->` pointers | thin, one word (D-038) |
| `tbb32` | **error codes only** — see below |
| `NIL` | zero-sized (D-084) |
| `Result<T>` | **builtin, the only parameterized type** — see below |

**`tbb32` is included as an error-code type only.** It is needed whether we like
it or not: `Result`'s error field is `tbb32` (D-069) and `failsafe` takes one
(D-013). Subset 1 permits **assignment, comparison, and passing** — and **no
arithmetic**, which means the seed needs no ERR, no stickiness, and no saturation
logic. `tbb32` is `i32` to the seed and nothing more.

**`Result<T>` is builtin and the only parameterized type.** The seed does not
implement generics; it hard-codes one type constructor with a known shape,
`{T, tbb32}` per D-069, instantiated per `T` actually used. `Result<NIL>` is
`{i32}` per D-084.

### 1.2 Aggregates

- **`struct`** — plain field aggregation, `.` field access with auto-deref.
- **`enum`** — tagged, with **payloads of at most one machine word**.

The payload restriction is what makes enum lowering trivial in the seed: every
enum is `{i32 tag, i64 payload}`, uniform, with no max-payload sizing and no
alignment puzzles.

**The frontend satisfies it naturally**, which is why it is affordable:

- identifiers are **interned**, so a token payload is an `int32` table index, not
  a `string`;
- AST nodes are **indices into concrete node arrays**, not pointers, so
  `enum:Expr = { Binary(int32); Call(int32); … }` carries an index per variant.

That second point is worth stating for its own sake: **there are no pointers in
the AST.** Index-based nodes are easier to verify than a pointer graph, and they
sidestep needing `arena<T>` — which is generic and therefore out.

### 1.3 Functions

- `func:name = RetType(Type:param, …) { … };`, `pub` for export.
- **`func:main = int32(cstring[]:argv)`** — one parameter, always (D-089). A
  program that ignores the command line writes `cstring[]:_~argv`, the
  declaration-site discard, which is what `_~` was invented for.
- **`Type:_~name`** on any parameter: deliberately unused. The seed records the
  annotation and does not enforce it — "a discarded parameter may not be read"
  is a check, and the seed lowers rather than checks (§2).
- **Every function returns `Result<T>`** (D-013), except `main` and `failsafe`.
- `main` and `failsafe` exactly as specified — `failsafe` mandatory, non-empty,
  returning a positive value (D-014).
- `pass` / `fail` / `return Result{…}`.
- Recursion (the parser is recursive descent).

### 1.4 Statements and expressions

| Included | Notes |
|---|---|
| `if` / `else if` / `else` | conditions are strictly `bool` |
| `while` | **the only loop** — see exclusions |
| `pick` | with single-word payload destructuring |
| bare blocks `{ }` | lexical scope; how a critical section is written (D-082) |
| assignment | a **statement**, not an expression (D-060) |
| arithmetic, comparison, logical, bitwise | on integers; wrapping per D-037 |
| `@` address-of | yields a **second-class borrow** (D-004) |
| `<-` dereference | |
| `p[i]` on a pointer, `a[i]` on an array or slice | |
| assignment to a **place** — `x.f = v`, `a[i] = v` | not just to a bare name |
| `s.ptr` / `s.len` / `s.cap` on a `string` | |
| `raw` / `_!` | unwrap without checking |
| `relay` / `_^` | **propagate** (D-080) |
| `drop` / `_?` | discard a `Result` |
| `fixed` | immutable binding — **enforced by the seed** |
| `defer` | runs on normal exit paths |
| casts `=>` / `=>!` | |

> **These last five were added in cycle 0.0.6**, when the first real compiler
> source was written. The conformance suite had exercised every construct
> *individually* and passed, but not in the combinations a compiler actually
> needs: `list.items[n] = d` requires assignment to a **place** rather than to a
> bare name, and the seed had only ever lowered values. A suite that covers
> constructs one at a time can still miss what using them together requires.

**Integer literals require a width suffix in expression position** — `42i32`,
never `42`. This is `--extra-picky=literal-suffixes` (`SAFETY_ARCHITECTURE.md`)
applied unconditionally to our own sources: width is never inferred in the code
that can least afford sizing ambiguity, and the seed needs no inference rule.

The exception, found while writing the parser: **an array size in *type* position
is a bare count** — `int32[4]`, as `TYPE_REFERENCE.md` §9.2 writes it. It is a
count, not a typed value, so the rule does not reach it. The lexer stays
positionless and records an unsuffixed literal without complaint; the *parser*
enforces the rule where it applies.

**`fixed` is enforced rather than parsed-and-ignored.** It costs the seed a
trivial "no assignment to this binding" check, and it buys a real correctness
property in our own source from the first line. Parsing a safety annotation and
ignoring it is the worse of the two options by a wide margin.

### 1.5 Modules

One form only:

```nitpick
use "relative/path.npk".*;
```

Wildcard, relative path, no selective imports, no aliases, no logical paths.
`pub` marks exports. Sufficient to write a multi-file compiler, and a fraction of
the resolution logic in `MODULE_REFERENCE.md` §2.3.

### 1.6 The runtime floor

The seed emits calls to a small set of symbols implemented as **hand-written LLVM
IR**, which is what D-015 already specifies for the first rung:

| Symbol | Implementation at seed time |
|---|---|
| allocation | **a bump allocator that never frees** |
| `dalloc` | a no-op |
| `read_stdin` | the read(2) loop, growing until EOF — 0.2.7 |
| `to_cstring` | copies and NUL-terminates, and **fails on an interior NUL** (D-049) — 0.3.0 |
| `read_file` | `openat`/`read`/`close`, returning the **positive errno** on failure so ENOENT is distinguishable from EACCES — 0.3.0 |
| `path_exists` | `openat`/`close`, testing READABILITY. Ambiguity-is-an-error means probing every root before deciding, and probing by full read would be O(file) per root — 0.3.2 |
| `exit` | raw syscall |
| `memcpy` / `memset` | the symbols LLVM emits calls to |
| `string_concat`, `int_to_string` | enough to build a diagnostic message |
| `write_raw(fd, ptr, len)` | the write syscall; **not** `Result`-wrapped, like `alloc` |

**Never freeing is correct here, not a shortcut.** The compiler is a process that
runs once and exits; reclamation buys nothing and an allocator is exactly the kind
of subtle code that should not be in the least-audited artifact. Real allocation
arrives with `nlibc` in cycle 0.8.

---

## 2. Excluded, and what the compiler's source does instead

| Excluded | What the source does instead |
|---|---|
| **generics** | hand-written concrete collections — `TokenList`, `DiagList`, `NodeList`, … |
| **traits, `impl`, `dyn`** | plain functions; the diagnostic sink is a concrete type until D-075's `dyn Writer` is lowerable |
| **`async` / `await`** | the compiler is synchronous; nothing in it needs suspension |
| **macros, `comptime`** | written out longhand |
| **contracts, `prove`, `limit<Rules>`** | verification arrives in cycle 1.3 |
| **UFCS method calls** | `Type_method(x)` rather than `x.method()` — `.` is still field access |
| **`for` / `loop` / `till`** | `while`. Every extra loop form is seed work, and `while` expresses all of them |
| **`?` `??` `?!` `?.` `?\|`** | explicit `is_error` checks, then `raw`; `relay` covers propagation |
| **string interpolation `&{ }`** | explicit concatenation |
| **floats** | the compiler needs none. Float **literals are lexed and stored as text**, so the seed does no float arithmetic |
| **`tbb` arithmetic, LBIM, `frac`, `tfp`, `dim`** | not needed by a compiler |
| **`wildx`, atomics, threads, channels** | not needed by a compiler |
| **`Optional<T>`, `arena<T>`, `Handle<T>`** | parameterized; index-based node arrays replace the arena |
| **operators on anything but `iN`** — notably **`+` on `string`**, which the language defines as concatenation | `string_concat`. §1.4 already said "on integers"; this row exists because `a + b` on two strings *looks* included, and the checker now refuses it by name rather than letting the emitter write `add { ptr, i64, i64 }` for llc to reject a stage later |

### Known gaps in the seed, recorded rather than worked around

- **Global `const` initialisers are not lowered.** The checker collects a
  `GlobalDecl` and the emitter ignores it, so `pub const string:CODE = "…";`
  compiles and then does nothing. Diagnostic codes are centralised as functions
  returning a literal instead (`src/frontend/diag_codes.npk`) — the
  centralisation is what matters, and the spelling changes when the rung lands.
- **`break` does not run `defer` blocks registered inside the loop body.** Scope
  exit is a normal exit path (D-014), so the real compiler must close this; the
  emitter carries a comment at the site.

### What the seed does *not* check

The seed lowers; **stage 1 checks.** The seed does not verify `pick`
exhaustiveness, `Result` handling discipline, escape analysis, or definite
assignment. It compiles exactly one program in its life — our compiler, which is
under our control — and every one of those checks is implemented properly in the
compiler itself, where it is audited.

The one exception is `fixed`, per §1.4, because it is nearly free and guards our
own source.

---

## 3. The rung diagnostic

A construct outside subset 1 is rejected during **lowering**, by the backend,
with:

> **`NITPICK-RUNG-001`** — *construct not supported at this backend rung*

It must name the construct and the rung that enables it, so the message is
actionable rather than a refusal.

**It is never a parse error.** The frontend accepts the whole grammar from day
one (D-085); the parser is complete before the backend exists, and capability
restriction lives entirely in lowering. `tests/rejection/` enforces exactly this
and asserts on both halves — the right code, *and* the absence of a syntax error.

That directory is written before there is a parser to break, because the failure
it guards against is not a bug anyone introduces deliberately. It is what happens
when a partial grammar gets re-widened rung by rung, and it is what ended
`nitpick-bootstrap`.

---

## 4. How subset 1 disappears

Each backend rung lifts part of it, and the compiler's source adopts what the rung
enables:

| Rung | Lifts |
|---|---|
| 0.9 full type lowering | `tbb` arithmetic, LBIM, floats, richer enum payloads |
| 1.0 generics and traits | generics, `dyn`, UFCS — the concrete collections become replaceable |
| 1.1 async | `async` / `await` |
| 1.3 verification | contracts, `prove`, `limit<Rules>` |

By self-hosting (1.2) the restriction is mostly historical, and **no migration
phase is needed** — the source was always Nitpick. That is the whole difference
between this and seeding from a foreign compiler.
