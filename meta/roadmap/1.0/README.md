# Cycle 1.0 — Generics, traits, `dyn`

**Phase C.** Monomorphization and the whole trait/`dyn` boundary. This is the cycle
where the concrete hand-written collections (`TokenList`, `NodeList`, …) become
replaceable by generics, UFCS method syntax lands, and dynamic dispatch appears.

> This is a **detailed map, not a full subcycle plan** — its subcycles get their
> own files when the cycle is reached, once its six gating decisions are settled
> (the same discipline the repo applies to every not-yet-current cycle). What is
> planned now is the decision set that must precede it and the subcycle shape those
> decisions imply, so the sharpening is bounded work rather than open discovery.

## The state this cycle starts from (the audit's finding)

The monomorphization **mechanics are genuinely ready** — definition-time checking
(D-064/D-107), dedup by mangled name (D-108), the depth-64 cap with a diagnostic,
turbofish-only expression position, comptime value params (D-109), supertraits/
blanket-impls/inherent-namespace — all implemented and tested (grammar audit's
"confirmed solid"). What is **not** ready is everything at the trait/`dyn` boundary
the checker never had to answer, because you could declare these constructs but not
yet lower them. Six decisions close that boundary; without them the rung forces the
frontend rewrites D-085's whole strategy exists to prevent.

## Decisions in (all six before lowering — see `../OPEN_DECISIONS.md` §1)

- **C-1 — `%Name` / symbol mangling.** *Blocks the cycle's start*, because every
  instantiated type and function needs a link-level name, and the scheme must cover
  module-canonical-name, generic args, comptime values, LLVM quoting, and folding
  linkage. The cycle's **first act** is this decision. Confirmed by two audits.
- **C-2 — object safety must refuse `Self` outside the receiver** (safety; a vtable
  reads the erased arg at the wrong layout).
- **C-3 — `dyn` method dispatch semantics** (no checker path exists today).
- **C-4 — multi-bound `dyn` ABI** (contradicted three ways; the widening has no
  mechanism).
- **C-5 — associated types referenceable or descoped** (TRAITS_REFERENCE's own
  `Iterator` doesn't typecheck).
- **C-6 — impls over generic families, and derive on generics** (inexpressible
  today; derive emits a broken impl).

## Subcycle shape (to be filled when reached)

| # | Topic | Gated on |
|---|---|---|
| 1.0.0 | **The mangling scheme** — decide C-1, implement reversible `%"mod.T<...>"` names + folding linkage; retire the interim duplicate-name refusal | C-1 |
| 1.0.1 | **Type identity by declaration, end to end** — the struct-literal context hole (D-162), the duplicate-name refusal retired as 1.0.0 planned, spanless refusals made audible; two instruments (`check_diags_spanned`, `check_identity_by_decl`) | 1.0.0 |
| 1.0.2 | **Monomorphization lowering** — instantiate, dedup, emit; the depth-cap diagnostic prints the instantiation stack (D-064 §6, currently one sentence — grammar #12) | C-1 |
| 1.0.2b | **Generic function lowering** — one body per argument set: `FnInstTable` records what the program asks for (deduped by arguments, D-108's rule for functions), the emitter walks it, the call site re-derives its specialization from its argument types; retires the `a generic function` / `a generic call` rungs | 1.0.2 |
| 1.0.3 | **UFCS method calls** — `x.method()` lowering; the concrete collections begin to become replaceable | — |
| 1.0.4 | **Traits and impls** — trait-method dispatch on concrete receivers incl. inherited defaults (grammar #6); impls over generic families (C-6) | C-6 |
| 1.0.5 | **`dyn` — object safety, dispatch, ABI** — C-2/C-3/C-4 together (they are one boundary); vtable layout and the multi-bound/widening mechanism | C-2,C-3,C-4 |
| 1.0.6 | **Associated types** — the C-5 resolution (type kind + projection, or the descope) | C-5 |
| 1.0.7 | **`Optional<T>` and the generic stdlib** — now that generics lower, the parameterized library types (grammar #13's `Optional` refusal names 1.0) | — |

## Watch for

- **D-163 lands at this cycle's boundary and is implemented at 1.1's opening;
  three of this cycle's subcycles carry a hook for it.** The licence reads a
  call's *resolved callee* for every call form, so: the UFCS subcycle (1.0.3)
  keeps `callee_decl` answering for `x.m()` as for `f(x)`; the traits/impls
  subcycle (1.0.4) makes `check_signature` compare the contract window, not only
  the types (an impl may not drop the trait's `never fails`); the `dyn` subcycle
  (1.0.5) `find_method` `TY_DYN` path returns the *trait's* declaration (the
  licence reads the trait, never the impl). The generic-stdlib subcycle (1.0.7)
  declares `Optional<T>`'s accessors `never fails`. D-156's mangling includes a
  function type's never-fails flag where a function type is a generic argument.
  None of this is the rule itself — it is the slot the rule plugs into, built once.

- **A `type_name` compare is an identity bug until proven otherwise.** D-090 put
  identity in the declaration; the struct-literal shortcut put it in the name and
  was wrong the day it was written (0.6.7), found closing 1.0.0's duplicate-name
  gap (1.0.1). `check_identity_by_decl` lists every such use; a new one needs a
  reason.

- **Frontend finality vs the C-6 grammar question.** C-6 may need an
  `impl:<T…>:Type<T>` form — a *grammar change*, which the bootstrap strategy treats
  as almost always the wrong shape (a change forcing parser rework). Weigh the
  per-instance-impl alternative hard before touching the grammar; if the grammar
  must change, that is a decision to raise explicitly, not slip into a subcycle.
- **This is where the concrete collections retire**, but not all at once — the source
  adopts generics only as the builder allows (C-13), so the `TokenList`-style types
  live until 1.2 switches the builder, then convert. Don't rewrite `src/` to generics
  mid-1.0.
- **The `%Name` scheme touches 1.2's reproducibility** — symbol names are emitted
  bytes, so landing C-1 correctly here (hash-free, path-free) is what keeps the 1.2
  fixpoint stable. Getting it wrong is a re-fixpoint.
