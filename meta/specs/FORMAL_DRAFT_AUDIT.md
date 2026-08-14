# `FORMAL_DRAFT` Adoption Audit

Assessment of `../nitpick-docs/specs/FORMAL_DRAFT/` (16 chapters, 2,317 lines of
`.md` plus a 4,638-line consolidated `FULL_formal.txt`) against decisions
D-001 … D-017.

**Verdict: adopt as the specification base.** It is more complete, more rigorous,
and more internally consistent than the ten carried-over `.md` references, and it
supplies four chapters missing from that set entirely. It also needs targeted
correction in 14 places, catalogued below.

---

## 1. What it adds that we do not otherwise have

| Chapter | Fills |
|---|---|
| **11 Concurrency** | async/await, `atomic<T>`, threading model, SeqCst policy — the carried-over set has **no** concurrency document despite `TYPE_REFERENCE` specifying `atomic<T>` and `Future<T>` |
| **13 Traits & Generics** | traits, supertraits, associated types, default methods, derive macros, blanket impls, coherence, object safety, monomorphization, `dyn` |
| **14 Modules & Build** | module resolution and build integration |
| **15 Standard Library** | stdlib organization |
| **01 Lexical Analysis** | full W3C-EBNF token grammar — the foundation for the parser |
| **04, 05** | expression precedence and statement grammar |
| **10 ABI & Hardware** | calling conventions, hardware interface |
| **00 Front Matter** | formal notation conventions (W3C EBNF dialect) |

Given the **full-frontend-first** bootstrap strategy, chapters 01, 04, 05, and 13
are the highest-value acquisitions — they are exactly the frontend specs needed
before the parser is built.

## 2. Where it independently confirms decisions we made

Worth noting, because several were derived from reasoning rather than from a
source. Independent agreement is evidence the reasoning was sound.

| Decision | Confirming text |
|---|---|
| **D-008** (ERR sentinel) | 2.3.1 — "`tbb8`: Range `[-127, +127]`. Sentinel: `-128`" — the exact balanced ranges derived in D-008 |
| **D-008 §6** (casts are not straight bit ops) | 2.3.1 — "**Sentinel Discontinuity:** … implicit widening of TBB types is forbidden … Explicit `tbb_widen<T>()` intrinsics must be used." Supplies an intrinsic name D-008 lacked. |
| **D-009** (`?!` takes one arg) | 12.2.2 — `read_file() ?! 99i32`. Fourth independent source. |
| **D-016** (SeqCst) | 11.4.3 and 2.6.3 |
| **D-005** (`void` is FFI-only) | 2.7.3 — "`void`: Strictly reserved for C FFI bindings" |
| **D-003** (borrow checker) | 12.5.5 — Z3 borrow-checker integration, `$$m` disjointness |
| **D-017** (arena/thread gap is real) | Chapter 11 covers atomics and threading but says **nothing** about arenas under concurrency — the gap is genuine, not an oversight in the carried-over set |

It also **restores the `--extra-picky` catalogue** (12.7) dropped from the
carried-over specs, and documents the **K Framework metatheory backend** (12.8).

Chapter 12.1 additionally names Nikola explicitly as a target application, which
is useful corroboration that the safety architecture was designed around it.

## 3. Conflicts with settled decisions — must be corrected on adoption

| # | Location | Conflict | Resolution |
|---|---|---|---|
| 1 | 0.4, 12.1, 12.3 | Layer 2 `unknown` "gracefully degrades … division by zero, out-of-bounds, null dereference" | **D-007** — behavior is type-directed (`tbb` → sticky ERR, plain types → trap). `unknown` is narrowed to `Result.value` taint per `TYPE_REFERENCE` §27. Rewrite Layer 2. |
| 2 | 6.1.2 | "`extern` … return bare types (`T`). They are **not** wrapped in `Result<T>`" | **D-002** — all functions including `extern` return `Result<T>`, with mandatory `fails on` / `never fails` contracts. |
| 3 | 8.2 | `sys!!!` documented; `sys!!` described as panicking rather than returning `Result` | **D-001** — `sys!!!` is removed; `sys`/`sys!!` both return `Result<int64>`. |
| 4 | 6.4 | Closures allocate their environment "via the GC (`npk_gc_alloc`)" | **D-018** — closures removed entirely. Strike 6.4; keep only the `dyn Trait` half of 9.7.3's fat-pointer layout. |
| 5 | 13.6.3 | "It is **illegal** to cast an integer directly to a pointer type … use `int64` to track opaque C addresses rather than casting to typed pointers" | **D-019** — prohibition stands for ordinary code, suspended by `#wild_ptr<T>(addr)` in `wild` context only. The `int64`-for-addresses recommendation is struck per **D-012**. |
| 15 | 8.1, 13.6 (throughout) | Builtins prefixed with `@` — `@sizeof`, `@alignof`, `@offsetof`, `@len`, `@ptr_add`, `@ptr_sub`, `@typeof`, `@typeInfo`, `@type_name`, `@fieldType`, `@has_field`, `@field_names`, `@is_comptime`, `@cast`, `@cast_unchecked`, `@derive` | **D-020** — `@` is address-of **only**; `@cast<T>(x)` reads as "address of cast". Rewrite all to the `#name<T>(...)` form. LLVM's own `@llvm.*` intrinsic names are unaffected. |
| 16 | — (`OP_REFERENCE.md` §6) | `#` listed as the **pin** operator, "prevents the Garbage Collector from moving the memory" | **D-020** — obsolete under D-003. Nothing relocates memory implicitly any more, so `#` is freed for its single compiler-directive meaning. `MEMORY_REFERENCE.md` §2 struck. |
| 6 | 13.3.3 | Arena chained access shown with `ptr->node_arena.alloc(...)` | **D-006** — `.` handles all member access and auto-dereferences; `->` is type-position only. |
| 7 | 6.1 | Example uses `return a + b;` and "implicitly returning `void`" | `pass`/`fail` are the return keywords; void functions do not exist (return `NIL`). Stale text. |
| 8 | 6.1.2, 12.6.2 | "**`pub func`** … implicitly return a `Result<T>`"; "If a function defines a `requires` block, Nitpick implicitly transforms its return type into a `Result<T>`" | Both imply `Result` wrapping is conditional. It is **universal** except `main` and `failsafe`. |
| 9 | 8.1.3, 13.6.1 | Destructive casts "panic at runtime" / "emit warnings" | `SPEC_GAPS` §2 — `=>` is a **compile-time error** on possible data loss; `=>!` is the explicit opt-out. |
| 10 | 12.7.2 | Explicit widening "must be cast explicitly using `as`" | `as` is not a Nitpick operator. The cast family is `=>` / `=>!`. |
| 11 | 2.3.3 | `fix256` | `SPEC_GAPS` §3 — renamed: `tfp` for plain fixed-point, `dim` for dimensional analysis. |
| 12 | 8.7 | `pre!` legacy text-substitution preprocessor documented | `AST_REFERENCE` §5 states the `pre()` text processor was dropped in favor of AST-native macros. Decide explicitly; **recommend dropping** — a text-substitution layer that bypasses AST hygiene is hard to reconcile with formal verification. |
| 13 | 8.3.2 | `#[lexical_drop]` and `#[nll_drop]` attributes | **D-003/D-004** — `#[lexical_drop]` exists to force deterministic RAII "bypassing standard GC", which is now the only behavior. Redundant; remove. |
| 14 | 11.3 | System threading interfaces with "`nitpick-libc`" | That repository is a musl tree. Should be **`nlibc`**. |

## 4. Internal contradictions within `FORMAL_DRAFT` itself

These are not conflicts with our decisions — the draft disagrees with itself, or
with the carried-over specs, and must be reconciled either way.

| Location | Contradiction |
|---|---|
| 6.5/6.6 vs 13.2 | Trait syntax differs: `impl Reader for FileStream` (ch 6) vs `impl:TraitName:for:TypeName = { … };` (ch 13). Ch 13 matches the `func:name =` house style and should win. |
| 12.1 vs 12.4 | `failsafe` is "Layer 3" in 12.1 and "the un-bypassable **Layer-1** safety net" in 12.4. |
| 8.1.1 vs `BUILTIN_REFERENCE` §4 | `@sizeof(T)` vs `#size_of<T>` — different sigil *and* different call syntax. |
| 8.1.3 vs 13.6.1 vs `OP_REFERENCE` | Three spellings for the same operation: `cast<T>(expr)`, `@cast<T>(expr)`, and `=>`. Ch 13.6.1 calls `=>` and `@cast<T>` "semantically identical". Pick one primary; the blueprint philosophy argues against three. |
| 8.1.4 | `calloc(size)` takes one argument here, `calloc(count, size)` everywhere else. |
| 2.4 vs `TYPE_REFERENCE` §7 | `tryte` is "10 trits packed into a `uint16`" (draft) vs "6 trits" (`TYPE_REFERENCE`). Both cannot be right; 3^10 = 59,049 fits `uint16`, 3^6 = 729 does not fill it. Draft is likely correct. |

## 5. The two that needed design work — both now settled

### 5.1 Closure environments had no allocator (conflict 4) → **D-018**

Chapter 6.4 allocated closure environments with `npk_gc_alloc`, which D-003 left
without an implementation.

**Resolved by removing closures entirely.** They proved to be implemented but
unused — zero uses across `libn` (58 files), `nitpick-posix` (164), `nstr`, and
`nmath`, and absent from all ten carried-over topic specs. Traits already provide
stateful callbacks with a named type, a visible owner, and a call graph the
verifier can follow.

Strike 6.4. Keep the `dyn Trait` half of 9.7.3's fat-pointer layout; drop the
closure half. Function pointers (lambdas without capture) remain.

### 5.2 Integer-to-pointer casting was illegal, making an allocator unwritable (conflict 5) → **D-019**

13.6.3 declared integer-to-pointer casting illegal and recommended tracking raw
addresses as `int64` — the convention D-012 rejects, because `int64` addresses
defeat leak-checking, escape analysis, and Z3 pointer reasoning.

The sharper problem was that the rule as written made `nlibc` **impossible to
write**: `mmap` returns an address, and nothing could turn it into the
`wild int8->` that `BUILTIN_REFERENCE` §1 says `alloc` returns.

**Resolved:** the general prohibition stands, suspended by exactly one named
construct legal only in `wild` context:

```nitpick
wild int8->:page = #wild_ptr<int8->>(addr);
```

This follows the standard Nitpick shape — a guarantee that is absolute by
default and suspended only through a named, greppable, auditable construct.

---

## 6. Recommended adoption sequence

1. ~~**Adopt chapters 01, 04, 05 immediately** — no conflicts found.~~
   **✅ DONE, but the "no conflicts" assessment was wrong.** That came from a
   structural skim; a full read found **~25 conflicts**, four of them semantic
   disagreements requiring decisions (D-022 `till`/`loop`, `prove` compile-time
   vs runtime, D-023 `for`, D-024 string literals). See
   `GRAMMAR_ADOPTION_CONFLICTS.md`.

   Adopted as: `LEXICAL_REFERENCE.md` (new), plus merges into `OP_REFERENCE.md`
   §0 (precedence) and `CONTROL_REFERENCE.md` §1.2.1–1.2.2, §2.3–2.5, §4.

   **Lesson for the remaining chapters:** a structural skim does not surface
   conflicts. Chapters 02, 06, 08, 11, 12, and 13 were assessed the same way and
   their conflict lists should be treated as lower bounds, not complete.
2. ~~**Adopt 13 (traits/generics)**~~ **✅ DONE** — adopted as `TRAITS_REFERENCE.md`.
   Nine conflicts found (Parts D–G of `GRAMMAR_ADOPTION_CONFLICTS.md`), two of
   them design problems: `Type` meaning both "namespace" and "associated type"
   (D-028), and traits combining with `&` in two places but `+` in a third
   (D-029). Chapter 06's trait, impl, and generic-parameter forms were the
   outliers and are superseded (D-030).
3. ~~**Adopt 11 (concurrency)**~~ **✅ DONE** — adopted as `CONCURRENCY_REFERENCE.md`.
   Only two conflicts, but five substantive **gaps**: the chapter is 82 lines and
   predates D-003, so nothing in it answers an ownership question. `atomic_new`
   and coroutine frames both allocated with no owner (D-033, D-034); task
   migration collided with D-017's single-threaded arena (D-032); `Future<T>` is
   never mentioned despite being a specified type; and task spawning was dropped
   from the prototype, leaving no documented way to start a concurrent task.
4. ~~**Adopt 12 (safety)**~~ **✅ DONE** — merged into `SAFETY_ARCHITECTURE.md`
   and `VERIFICATION_REFERENCE.md` rather than adopted as a new file, since both
   already covered parts of it. Eight conflicts and five gaps
   (`GRAMMAR_ADOPTION_CONFLICTS.md` Parts M–O). The chapter had **no mention of
   `tbb` sticky ERR** despite D-008 making it the primary fail-operational
   mechanism, and **no mention of the memory-safety mechanisms** despite
   `--verify-memory` claiming to verify use-after-free. 12.7's `--extra-picky`
   catalogue and 12.8's dual-backend framing were kept.
5. **Adopt 02 (types)** after fixing conflict 11, and fold in D-005 and D-008.
   Note 2.3.1's `tbb_widen<T>()` intrinsic should be carried into D-008.
6. **Settle §5.1 and §5.2**, then adopt 06 (functions) and 08 (builtins) with
   conflicts 2, 3, 7, 8, 12, 13 corrected.
7. **Adopt 03, 07, 09** — already reviewed; 09 needs the GC sections removed per
   D-003, and 09 §9.7.1's `Result` layout corrected per D-005.
8. ~~**Assess 00b, 10, 14, 15**~~ **✅ DONE** — `GRAMMAR_ADOPTION_CONFLICTS.md`
   Parts T–X. **All 16 chapters are now assessed.**

   Chapter 15 is the most out-of-date in the draft: it states that Nitpick
   "deliberately omits a discrete `char` type", which is flatly contradicted by
   `TYPE_REFERENCE.md` §2 and by the principle behind D-005.

   Two genuine open questions surfaced, both expensive to change later:
   **fat versus thin pointers** (Part W) and **the LLVM/Z3 dependency boundary**
   (Part X). **Both are now settled** — D-038 makes pointers thin, and LLVM and
   Z3 are invoked as **tools**, not linked as libraries, with Z3 reached over a
   text interface until a Nitpick equivalent exists.

   Chapter 10 contributes three things found nowhere else: strict left-to-right
   evaluation order, the symbol-mangling scheme, and the reserved failsafe error
   codes.
