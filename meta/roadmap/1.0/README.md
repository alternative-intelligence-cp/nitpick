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
| 1.0.4b | **Family impls (D-161 amended)** — the segment count decides (D-111 already requires a blanket impl to name a trait, so no lookahead heuristic is needed); parse, resolve, check, dispatch, emit | D-161 |
| 1.0.4c | **Overlap refusal and derive-on-generic** — D-161's remaining clauses: an overlapping family + per-instance impl refused AT THE IMPL (today only at a call, as ambiguous-method), and `#[derive]` on a generic subject synthesizing the family form or refusing by name (today it emits a broken impl and reports arity errors against `<derived-N>`) | 1.0.4b |
| 1.0.5 | **`dyn` — object safety, dispatch, ABI** — C-2/C-3/C-4 together (they are one boundary); vtable layout and the multi-bound/widening mechanism | C-2,C-3,C-4 |
| 1.0.5b | **A method's symbol names its impl (D-156 amended)** — found at 1.0.5c's opening by reading emitted IR: two impls of one trait on different types both emit `@"npk.<module>.<name>"` and `llc` refuses the module, and two impls inheriting one trait DEFAULT collide the same way from a single declaration index. Symbols become `<Target>:<Trait>.<name>`, computed once and used by both emission and call sites; folds in D-159's module-qualified bound order; adds `check_no_duplicate_symbols`, since nothing has ever looked | 1.0.5 |
| 1.0.5c | **`dyn` lowering** — the backend half of D-158/D-159: per-(impl,trait) vtables in trait-declaration order, per-impl adapter thunks, construction, call-through-slot, and widening as a value rebuild; retires the `TY_DYN` rung. Also closes D-159's tie-break hole by keying the canonical bound order on a MODULE-QUALIFIED name (two same-named traits currently tie and fall back to declaration index, which shifts under an unrelated edit) | 1.0.5 |
| 1.0.6 | **Associated types** — the C-5 resolution (type kind + projection, or the descope) | C-5 |
| 1.0.6b | **A generic trait's own parameter, under a bound** — found at 1.0.6 and verified pre-existing: naming a generic trait as a generic function's BOUND makes the trait's own `T` stop resolving at its declaration. Blocks C-20 (the projection spelling), because the "declare the trait generic instead" alternative has to work before it can be weighed | 1.0.6 |
| 1.0.6c | **`T.Item` — a dotted suffix in type position (D-164)** — the projection D-160 assumed the parser had; one branch in `p_suffixes` plus one `TypeKind`, resolved through the bound and substituted per impl, with two bounds declaring one assoc refused naming both. **Carries D-159's obligation**: a dotted spelling makes two same-named traits nameable in one file, so the canonical `dyn` bound order must key on a module-qualified name in the same change | D-164 |
| 1.0.6d | **Normalising a projection, and D-159's qualified key** — `Counter.Item` exists but nothing turns it into what the impl bound, and it cannot be fixed in `resolve_type`: `ImplTable` is declared above it, so a resolver field is a real import cycle. Opens by choosing where assoc normalisation lives (the architecture's answer for anything needing impls is a later pass). Also carries D-164's obligation: a dotted spelling now parses, so the `dyn` bound order needs the module-qualified key D-159's amendment demands | D-164 |
| 1.0.6e | **A `dyn` is built at every slot, not only at a declaration** — found at 1.0.7's opening by reading `emit_dyn_from`'s callers: `fits` admits a concrete value into a `dyn` slot at sixteen sites and the emitter widens at one. A call argument and a `pass` fail at `llc`; an ASSIGNMENT into an existing `dyn` stores the 4-byte value over the 16-byte slot, dispatches through it, and **segfaults** — an uncontrolled crash from a program the checker accepted. One slot helper (`emit_fit`, the backend's half of `fits`), every site, and `check_slot_sites_agree` pairing the two lists | 1.0.5c |
| 1.0.7 | **`Optional<T>` lowers, and the first generic collection** — `{ i8, T }` compiler-known exactly as `Result<T>` is (the map's "library type" wording is corrected: D-099 strikes readable members, so a library struct would need a special case), `NIL` as `zeroinitializer`, the wrap as `emit_fit`'s second case, `== NIL` as a tag test, `??` lazy, `?.` flattening; plus a generic `List<T>` as a test program (`wild T->`, `#size_of<T>()` in a generic body, inherent and trait family impls, a `T?` accessor) at two element types. Non-scalar `==` becomes a named refusal (today it reaches `llc`); its meaning is 1.0.9's | 1.0.6e |
| 1.0.8 | **The diagnostic message is printed** — found at 1.0.4c: `Diagnostic.message` is written at 79 sites in `src/` and read by nothing, because both drivers define their own `emit_line` that prints `CODE path:line:col` and the one renderer that keeps the message drops the path. One shared renderer, the other two deleted, all 79 messages read against what their code does (1.0.4c found one whose two roles were bound to declaration order), and `check_one_renderer` to stop the drift recurring | 1.0.4c |
| 1.0.9 | **The 1.0 tail: no refusal names 1.0** — 0.9.7's rule (a cycle does not close while a refusal string names it) applied to the 27 sites that say "1.0", 24 of which 0.9.7 re-pointed there as "next", not as an owner. Each is lowered, converted to an internal defect where the frontend already refuses the shape, or re-pointed BY NAME; five decisions it needs (D-165 globals, D-166 what `for` iterates, D-167 the defaults operator, D-168 `&{ }` via `ToString`, D-169 non-scalar `==`, D-170 grouping a type around a `dyn`, D-171 an inherent impl over a generic family — both spellings, the user's call); the `for`-binding checker hole; and `check_rung_names_open_cycle`, so the next cycle's close is a check rather than a grep. **Cycle 1.0 closes here** | 1.0.7, 1.0.8 |

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
