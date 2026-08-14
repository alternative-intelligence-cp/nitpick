# Prototype → Canonical Spec Delta

Compares the specs carried over from `nitpick-next/meta/specs/` (10 Markdown
reference docs) against the prototype's specs in
`../nitpick-docs/specs/` (topic-split `.txt` files, compiler v0.78.3).

The two sets are organized differently — the prototype splits by topic across
~28 `.txt` files, the carried-over set uses 10 consolidated `.md` references —
so this is a semantic diff, not a textual one.

**Status of this document:** a record of what changed and what is still open.
**§3's two items are now settled** — see the markers there. Sections 4 and 5 were
added later and are current.

---

## 1. Removals

| Removed | Prototype source | Notes |
|---|---|---|
| **Borrow checker** | `formal_verification_specs.txt` §5 "Z3 Borrow Checker Integration" | The `$$m` mutable-borrow syntax and Z3 index-disjointness proving are gone. `SPEC_GAPS` states the borrow checker was removed *because* the GC handles memory safety by default. This is the single largest change. |
| **NLL (Non-Lexical Lifetimes)** | `memory_specs.txt` §1.1 | Default managed memory no longer "crosses its NLL last-use point" — now plain scope exit. Consistent with dropping the borrow checker. |
| **`unknown` as a *user-writable* value** | `safety_systems_specs.txt` §3 | ⚠️ **Narrowed, not removed** — see §3.4. `unknown` survives in `TYPE_REFERENCE.md` §27 but is redefined as a compiler-assigned taint the user cannot write, applied to `Result.value` on `fail()`. The prototype allowed `int32:val = unknown;` directly and scoped it to div-by-zero / out-of-bounds. `ok()` survives in both. |
| **Legacy alloc aliases** | `memory_specs.txt` §3 | ⚠️ **Inconsistent, not removed.** `MEMORY_REFERENCE.md` §4 lists only `alloc`/`calloc`/`ralloc`/`dalloc`, but `BUILTIN_REFERENCE.md` §1 explicitly preserves `free` and `realloc` as legacy aliases. The two carried-over docs disagree. |
| **Legacy function syntax** | `mod_system_specs.txt` §4.1 | Prototype supported `func name(a: int32) -> int32` and `fn`. Now explicitly banned — canonical `func:name = type(args)` only. |
| **Module search-path order** | `mod_system_specs.txt` §2.3 | Prototype specified `cwd → -I <dir> → stdlib/ → /usr/lib/aria → NITPICK_PATH`. No replacement given. |
| **Diamond import guarantee** | `mod_system_specs.txt` §2 | Type identity across multiple import paths without duplicate-definition errors — dropped. |
| **`--extra-picky` rule catalogue** | `safety_systems_specs.txt` §5 | The four documented rules (`literal-suffixes`, `explicit-widening`, `shadow`, `wild`) and the `warn-`/`no-` parameterization survive only as a passing mention of `no-wild`. |
| **Arena `reset()` / `destroy()`** | `memory_specs.txt` §4.2 | Dropped from the arena surface. |
| **Arena chained member access** | `memory_specs.txt` §4.2 | `app.my_arena.alloc()` resolving through embedded struct fields — dropped. |
| **`Handle<T>` non-packing rationale** | `memory_specs.txt` §4.1 | The explanation of why the layout is deliberately not bit-packed is gone; the layout itself survives. |
| **K Framework / kprove backend** | `formal_verification_specs.txt` §7 | The metatheory verification backend section is dropped, though `SAFETY_ARCHITECTURE.md` still refers to "K-Semantics on `exit`". |

## 2. Additions and changes

| Change | Where | Notes |
|---|---|---|
| **Pin operator `#`** | `MEMORY_REFERENCE.md` §2, `OP_REFERENCE.md` §6 | New. Prevents the GC from moving or reclaiming an object so a pointer can be handed to FFI safely. The bridge that makes `gc` and `wild` interoperate. |
| **`extern` now returns `Result<T>`** | `MODULE_REFERENCE.md` §5.2 | Flagged in-doc as a "critical deviation". Prototype: `extern` returned bare values and `raw` was not used. Now *all* functions including FFI return `Result<T>`; unwrap with `raw` / `_!`; the optimizer is specified to strip the wrapper for zero runtime overhead. |
| **Hybrid generational GC formalized** | `SPEC_GAPS` §1 | Nursery / old generation, safepoints, shadow-stack root scanning, card-table write barriers. The prototype specs never pinned the collector design down. |
| **`=>` becomes compile-time** | `SPEC_GAPS` §2 | Safe cast no longer traps to failsafe on data loss — it is now a **compile error**, with `=>!` as the explicit opt-out. Prototype `OP_REFERENCE` still described it as triggering failsafe. |
| **Arithmetic never returns `Result<T>`** | `SPEC_GAPS` §4 | Overflow is handled by `tbb` types entering a **sticky ERR** state. Plain types (`int32`) accept standard overflow. Chosen explicitly to keep expressions readable. |
| **`Result<T>` error field widened** | `SPEC_GAPS` §2 | `tbb8` → `tbb32` (POSIX errnos exceed 127, HTTP codes exceed 255). `is_error` tightened to strict `bool` so no arithmetic is possible on it. |
| **`fix` → `tfp` / `dim`** | `SPEC_GAPS` §3 | `tfp` is plain fixed-point; dimensional analysis moves to `dim256<Joules>`. Resolves a naming collision in `TYPE_REFERENCE.md`. |
| **UFCS removed** | `SPEC_GAPS` §3 | No method-call syntax. All operations are free functions (`string_trim(s)`) or pipelines (`s \|> string_trim()`). `.` is struct field access only. **But see §3.2.** |
| **`->` restricted to type position** | `SPEC_GAPS` §4 | No longer doubles as member access. `.` handles all field access and auto-dereferences pointers. |
| **`pick` exhaustiveness enforced** | `SPEC_GAPS` §4 | Must cover all values or supply `(*)`. |
| **`pick` guards + macro patterns** | `CONTROL_REFERENCE.md` §1.3 | `where` clauses on arms, and matching against macro invocations. |
| **`pick` labelled fallthrough** | `CONTROL_REFERENCE.md` §1.2 | No implicit fallthrough; label the target arm and use `fall label;`. |
| **Rules composition / subsumption** | `VERIFICATION_REFERENCE.md` §2.1 | Rules may reference other Rules via `limit<OtherRule>`. Z3 proves one ruleset subsumes another, enabling narrowing at call sites without redundant checks. New. |
| **Verification levels table** | `VERIFICATION_REFERENCE.md` §5.1 | Levels 0–3 given explicit contents; prototype had a one-line parenthetical. |
| **Sugar operator family** | `OP_REFERENCE.md` §5 | `_?` (drop), `_!` (raw), `_~` (discard), `?\|` (defaults), `??` (null coalesce), `?.` (safe navigation). Prototype had `raw`/`drop` as bare keywords only. |
| **`<=>` spaceship** | `OP_REFERENCE.md` §3 | Three-way comparison. New. |
| **`cstring` type + `to_cstring`** | `TYPE_REFERENCE.md` §3.2.1 | Replaces the prototype's implicit `string` → `const char*` passing, and supersedes the interim `as_cstring(string) → char8[]`. A `char8[]` carries no termination guarantee; `cstring` does, and `to_cstring` rejects interior NULs (D-049). |
| **`any->` opaque pointer** | `MODULE_REFERENCE.md` §5.3 | Replaces the prototype's `?*` / `?->`. |
| **Operator overloading banned** | `OP_REFERENCE.md` header | Stated as a formal-verification rule; operator meaning fixed at language level. |

---

## 3. Open items

These are contradictions or omissions that survive into the carried-over specs.
They need a decision before the affected frontend components are built.

### 3.1 Memory model — RAII or GC? — **SETTLED by D-003: no tracing GC**

> Static ownership with RAII/NLL and arenas. `SPEC_GAPS` §1's hybrid generational
> GC is rejected, and its claim that the borrow checker was removed is wrong —
> arenas cover the cycle case, which was the collector's last advantage.
> `MEMORY_REFERENCE.md` §1.1's "implicit RAII / deterministic destruction" is
> correct as written.


`MEMORY_REFERENCE.md` §1.1 describes default managed memory as
"Implicit RAII/Scope-based" with "deterministic destruction". `SPEC_GAPS` §1
resolves the same question the other way: a **hybrid generational GC**, and
states the borrow checker was removed precisely because the GC provides safety.

These are not compatible — deterministic scope-exit destruction and tracing
collection imply different codegen, different `defer` semantics, and a different
answer to what `nodrop` opts out of. `SPEC_GAPS` is newer (Aug 13 vs Aug 11), so
its resolution presumably wins, but `MEMORY_REFERENCE.md` §1.1 was never updated
to match.

### 3.2 UFCS — banned, but arenas still use it — **SETTLED by D-006: UFCS is retained**

> `SPEC_GAPS` §3 is simply wrong. UFCS and method-call syntax were never removed;
> the only change in that area was dropping `->` as a member-access operator, so
> `.` handles all member access and auto-dereferences. Arena and `atomic<T>` code
> sitting in the same repository contradicted the claim as it was written.


`SPEC_GAPS` §3 states plainly that Nitpick does **not** support method calls and
that `.` is strictly struct field access. `MEMORY_REFERENCE.md` §5.2 is titled
"Arena UFCS Dispatch" and shows:

```nitpick
Handle<int64>:h = my_arena.alloc();
int64:val = my_arena.get(h) ?! 0i64;
my_arena.free(h);
```

Either arenas are a specified exception to the no-UFCS rule, or this section
needs rewriting to free-function form (`arena_alloc(my_arena)`). Inherited
unchanged from the prototype, where UFCS was supported language-wide.

### 3.3 `?!` arity is inconsistent (blocks: parser, `Result<T>` lowering)

Three incompatible readings appear, but they are **not evenly weighted**:

| Source | Form | Meaning |
|---|---|---|
| `TYPE_REFERENCE.md` §11.2 | `expr ?! errCode` | traps to failsafe with that code |
| `AST_REFERENCE.md` §3 (`EmphaticUnwrapExpr`) | `expr ?! err_code` | same |
| Prototype `safety_systems_specs.txt` §2.2 | `read_file() ?! 99i32` | same |
| `OP_REFERENCE.md` §5 | `val = fn() ?!;` | **niladic** — outlier |
| `MEMORY_REFERENCE.md` §5.2 | `my_arena.get(h) ?! 0i64` | reads as a **default value** — that is `?` semantics |

Three sources including the AST definition agree on `expr ?! errCode`. Treat
`OP_REFERENCE.md` as the error and fix the `MEMORY_REFERENCE.md` call site, which
is ambiguous either way (is `0i64` a failsafe code or a fallback?).

### 3.4 `unknown` was narrowed — confirm the narrowing was intended

`unknown` survives, but its meaning changed substantially and it is documented in
only one place (`TYPE_REFERENCE.md` §27):

| | Prototype | Carried-over |
|---|---|---|
| Who writes it | User: `int32:val = unknown;` | **Compiler only** — "not a type the user can write directly" |
| When applied | Div-by-zero, out-of-bounds, panic-like scenarios | Assigned to `Result.value` when `fail(errCode)` is used |
| Propagation | NaN-like through operations | Same — `unknown + 1` → `unknown` |
| Clearing | `ok(val)`, `is unknown` postfix check | `ok(val)` or checking `Result.is_error` |

The prototype's rationale was **fail-operational** behavior for aerospace:
continue in a degraded state rather than trap. Narrowing it to "the value field
of a failed Result" removes it as a general degraded-computation mechanism —
division by zero now traps to failsafe (per `OP_REFERENCE.md` §1) rather than
producing a taint that propagates.

For a system driving robotics, *which* of those two behaviors is wanted is a
genuine safety decision, not a documentation detail. It is also **absent from
`SAFETY_ARCHITECTURE.md`** entirely, even though `TYPE_REFERENCE.md` calls it a
"Layer 2 Safety Taint" — so the document describing the layer model omits one of
the layer's mechanisms.

### 3.5 Module system deferred

`SPEC_GAPS` §5 defers import syntax and filesystem mapping to "the Module Loader
cycle (Cycle 0.4.0)", and the search-path order documented in the prototype was
dropped without replacement. `MODULE_REFERENCE.md` gives the syntax but not
resolution order.


---

## 4. Superseded claims in `FULL_specs.txt`

`FULL_specs.txt` is the ~14k-line consolidated prototype specification and
remains the authority on language semantics generally. These specific passages
are **prototype-era and have been settled the other way** here. Recorded because
each is load-bearing for a decision, and a reader who finds the passage first
would reach the wrong conclusion.

| Passage | Claim | Settled as |
|---|---|---|
| §15.1.3 | "`string` guarantees internal null-termination" | **No.** `string` is `{ptr, len, cap}` and is not NUL-terminated. `to_cstring` exists because it is not — D-049. Also logged as conflict 53 in `GRAMMAR_ADOPTION_CONFLICTS.md`, now settled |
| §15.1.3 | "`int8->` is a Fat Pointer containing bounds metadata" | **No.** Pointers are thin — D-038 |
| §15.1.2 | raw strings `r"…"` and multi-line `"""…"""` are "currently unsupported and will throw syntax errors in v0.61.82" | A statement about a prototype build, not a language decision. The grammar carries `RawStringLiteral` |

The first two sit in the same paragraph, which is about FFI and C-string decay —
an area the prototype handled by making `string` C-shaped and pointers
bounds-carrying. Both choices were reversed: `cstring` carries the C-shaped
representation as a distinct type (D-049) and pointers are thin (D-038), with
bounds living in the types that own them.

`nitpick-docs/` is read-only reference material and is not edited to match. This
table is the correction of record.


---

## 5. Macro documentation — located, and what it conflicts with

The claim that macros had no governing document was **wrong**. Two exist in
`nitpick-docs`:

| Document | Lines | Content |
|---|---:|---|
| `specs/macros_meta_specs.txt` | 40 | `macro:` definitions, `comptime`, `cfg`, inlining hints, the `pre!` preprocessor |
| `reference/MACRO_AUTHORING_GUIDE.md` | 122 | derive macros, attribute macros, `comptime` intrinsics and budget |

Neither was among the ten spec files carried into `meta/specs/`, which is why the
gap appeared. The system they describe is substantial and mostly sound; it
predates several decisions taken here.

### Conflicts to resolve before adoption

| # | Documented | Conflicts with | Resolution |
|---|---|---|---|
| 1 | Invocation `name!(args)` | D-046 | **`#name(args)`.** Both documents predate the change |
| 2 | `cfg!(target_os = "linux")` | D-046 | **`#cfg(...)`** |
| 3 | `@type_name(T)`, `@has_field(T, "f")`, `@field_names(T)` | D-020, `BUILTIN_REFERENCE` | **`@` is address-of and is never a builtin prefix.** Becomes `#type_name<T>` etc. — same error class already corrected for `@sizeof` and `@cast` |
| 4 | `pre!` text-substitution preprocessor | the spec's own opening line | see below |
| 5 | `#[comptime]` attribute *and* `comptime func:` modifier | blueprint philosophy | one spelling. Keep the **modifier**; `AST_REFERENCE` already carries `comptime` in `FunctionDecl.modifiers` |
| 6 | Derive list of 6 traits | `TRAITS_REFERENCE.md` §129 lists 8 | `TRAITS_REFERENCE` is newer — `Default` and `PartialOrd` are also derivable |
| 7 | `#[gpu_kernel]`, `#[gpu_device]` | zero-dependency rule | **SETTLED by D-055.** Kept, with narrowed meaning: a codegen target emitting no host-callable symbol. GPU and GUI run out of process, so the vendor runtime's failure arrives as a value rather than an unroutable fault |

### `pre!` should be removed outright

`macros_meta_specs.txt` opens by stating that Nitpick *"rejects C-style
text-substitution macros in favor of AST-aware, hygienic macros"* — and then §5
documents a text-substitution preprocessor with positional `#%1` parameters. The
document contradicts itself.

For a compiler under formal verification the objection is decisive: **text
substitution runs before parsing, so the artifact analysed is not the text anyone
reviewed.** Every hygiene and type-safety guarantee the language offers is
bypassed, which both documents say explicitly.

It is also already on the way out — the authoring guide calls `pre!` "deprecated
and will be removed in a future cycle," having itself replaced `%macro`. The
migration path simply terminates one step earlier than written: `%macro` and
`pre!` both migrate to `macro:` or `comptime`, and neither survives.

Removing it also retires another `!` suffix form, after D-001 removed `sys!!!`
and D-046 removed `!!`.

### What is worth adopting as-is

`comptime` is well specified and needs no change beyond the `@` → `#` correction:
the `comptime func:` modifier, the `comptime(expr)` forcing form, LLVM global
embedding for returned structs and arrays, `--trace-comptime`, `--comptime-budget
<N>`, and a blocklist during evaluation of allocation, I/O, closures, and raw
pointer access. That blocklist is the property that makes compile-time evaluation
verifiable, and it is already stated.

### The `comptime` block question, settled

Three documents give three answers. `macros_meta_specs.txt` §2 has the
`comptime func:` modifier and a `comptime(expr)` forcing form. `FORMAL_DRAFT` 07
§7.2 has the modifier and a `comptime { … };` **block**. `AST_REFERENCE` had only
the modifier.

**Settled as modifier + `comptime(expr)`; the block does not exist.**
`FORMAL_DRAFT`'s own example binds `int32[]:baked_data` inside the braces and
then expects to use it, which requires a block that does *not* introduce a scope
— contradicting 05 §5.2, where every block does. A brace form that scopes
everywhere except here changes meaning by context. `comptime(expr)` says the same
thing with an ordinary expression and an ordinary binding, and `AST_REFERENCE`
now carries `ComptimeExpr`.

### Where `comptime` ends and macros begin

`FORMAL_DRAFT` 07 §7.2 claims `comptime` allows "macro-like metaprogramming
without requiring a separate macro language." It does not replace macros, and the
boundary is worth stating: **`comptime` computes values; macros generate
declarations.** Baking a lookup table is `comptime`. Synthesising an
`impl:Trait:for:T` from `#[derive(...)]` is a macro. Neither subsumes the other,
so both stay — but nothing else should be added to either without checking which
side of that line it falls on.


### Where the macro semantics actually live

Neither macro document specifies hygiene, expansion order, or what a macro may
emit. **The regression tests do** — 49 `.npk` files under `nitpick/tests/`, with
the semantics stated in header comments keyed to `MACRO2-DEC-001…007` and
`COMPTIME-006/007`. No user code anywhere declares a macro; the whole corpus is
tests.

Recovered and settled as D-057: defining-scope hygiene with `#caller(NAME)` as
the sole opt-out, fixed-point module-level expansion preceding `comptime`
folding, multi-declaration emission, struct-field and impl-method splicing, and a
bound on expansion so the fixed-point loop terminates.

Worth noting for the port: the K semantics (`nitpick.k`) model **zero- and
one-argument substitution only**, and `SEMANTIC_GAPS.md` rates macro expansion
"Low" priority. The K definition is therefore a much weaker model than the
implementation, and re-deriving the semantics from K would have lost hygiene,
splicing, and multi-declaration emission entirely.
