# Open decisions and unwritten specs (post-0.8)

> The post-0.8 queue, opened at the 0.8-close replanning. The prior queue
> (opened D-061, fully closed at D-079) is archived as
> `done/OPEN_DECISIONS-through-0.8.md`. Every item here traces to the audit at
> `audit-0.8-close/total_audit.md`, blocks a named cycle, and nothing is
> optional — per the standing constraint, anything entering the language must be
> settled before the evidence campaign closes (D-233 restated the basis: proof
> invalidation, not a one-shot trial), because re-verification is unaffordable.
>
> Proposed decision numbers start at **D-142** (last settled: D-141). Numbers
> are suggestions; the letters (LIVE-*, B-*, C-*) are the stable handles the
> cycle plans cite. (1.0.9 settled D-165–D-172 for its own T-1…T-8, so the
> numbers once suggested for G-2 and G-3 are taken; they read "next free".
> **1.4.0 settled its whole batch as D-201…D-209** — the D-153…D-157 once
> suggested for C-10…B-4 had long been issued to other decisions.)

**Priority order for every judgement below: safety > correctness > performance >
developer comfort.**

---

## 0. Immediate — live safety holes — **CLOSED at 0.9.0**

Both landed as cycle 0.9's opening subcycle: the five verification carriers
refuse with `NITPICK-RUNG-001` naming 1.4 under BOTH compilers
(`tests/rejection/contract_requires` / `contract_ensures` / `limit_param` /
`limit_local` / `loop_invariant`, spans pinned), and division/remainder emit
the D-007 guard trapping through D-142's `npk_trap` route — `DIV_BY_ZERO`
−4097, `INT_MIN_OVERFLOW` −4098 — proven by executed-exit tests
(`div_guard`/`rem_guard`/`div_min`/`div_ok`). The rows stay as the record:

| # | Item | Confirmed | Fix |
|---|---|---|---|
| ~~**LIVE-1**~~ | `limit`/`requires`/`ensures`/`invariant` compile to nothing — no check, no rung refusal (D-068 violation) | Yes — npkc emitted a `requires`-carrying fn as a bare `sdiv`, a `limit` binding as a bare `alloca` ([../audit-0.8-close/probes/limit_drop.npk](../audit-0.8-close/probes/limit_drop.npk)) | Add `NITPICK-RUNG-001` refusals naming cycle 1.4, same shape as `prove` **CLOSED at 1.5.3 (2026-09-06): nothing compiles to nothing — `limit` checks since 1.5.2 (D-251), `requires`/`ensures`/`invariant` since 1.5.3 step 1 (D-221), each a trap in every build and a row in the manifest; the 0.9.0 rungs retired with them.** |
| **LIVE-2** | integer `/` `%` lower to unguarded `sdiv`/`urem` — div-by-zero is LLVM UB, no refusal | Yes — observed in the same IR | Emit the D-007 zero-check guard, or rung-refuse `/` until 0.9 hardens it; also define `INT_MIN / -1` |

---

## 1. Decisions blocking 1.0 (generics, traits, dyn) — **CLOSED at 1.0-open**

All six settled as **D-156–D-161** (the 1.0 opening act; see DECISIONS.md).
C-1 → D-156 (`npk.<module>.<spelling>` quoted symbols, program-unique module
names, linkonce_odr); C-2 → D-157 (`Self` nowhere but the receiver, full-tree
walk; rule 3 unified); C-3 → D-158 (declaration-indexed per-(impl,trait)
vtables of adapter thunks; own methods only; ambiguity is an error); C-4 →
D-159 (data + one vtable word per trait, canonically ordered bounds; widening
is a static value rebuild); C-5 → D-160 (TY_ASSOC + in-trait resolution +
impl-binding substitution; assoc-mentioning methods are not object-safe);
C-6 → D-161 (family impls via target-between-generics-and-trait — one
contained production change; per-instance impls turned out to ALREADY work).
**Fully implemented at 1.0.4b/1.0.4c**: the blanket-versus-family
disambiguation needed no heuristic (D-111 already requires a blanket impl to
name a trait, so the segment count decides), overlap is refused AT THE IMPL
with no call required, and derive-on-generic **synthesizes** the family form
rather than taking the interim refusal — with no bound, since a derived body
uses the operators and never a trait method. The rows stay as the record:

| # | Proposed | Item | Blocks | Source |
|---|---|---|---|---|
| **C-1** | D-142 | **`%Name` / symbol mangling scheme** — reversible, hash-free (D-064 §6 settled *that*, not *how*). Must specify: module-canonical-name (files identified by path differ per importer, and must not embed build paths per D-078), generic-argument encoding, comptime-value encoding, LLVM quoting (`Container<int32>` is not a legal bare identifier → `%"mod.Container<int32>"`), and the linkage that folds identical specializations (`linkonce_odr`?). **Blocks 1.0 start.** Confirmed by two audits (modules #5, grammar #7). | 1.0 start | D-064 §6; 0.8 README defers it |
| **C-2** | D-143 | **Object safety must refuse `Self` outside the receiver.** `bool(Self:self, Self:other)` passes today (`resolve_type.npk:1610-1618` checks only the return node); behind a vtable the erased second arg is read at the wrong layout. **Safety.** Extend rule 2 to walk nested types (`Optional<Self>`, `Self[]`) in any non-receiver position; restate TRAITS_REFERENCE §4.2. | 1.0 | grammar #1 |
| **C-3** | D-144 | **`dyn` method dispatch semantics** — `find_method` has no `TY_DYN` path, TRAITS_REFERENCE §5.2 shows only assignment never a call, no test calls through a `dyn`. Specify: which traits' methods are reachable, supertrait-method reachability, the ambiguity rule, return typing — before vtable lowering. | 1.0 | grammar #2 |
| **C-4** | D-145 | **Multi-bound `dyn` ABI.** Contradicted three ways: `types.npk:471` interns every `TY_DYN` at 16 bytes; `type_trait.npk:1168` says N+1 words; specs show `{ptr,ptr}`. Settle one layout (per-trait vtable words, or a combined vtable with a prefix/subview rule) and make it carry `dyn A & B → dyn A` widening at runtime. | 1.0 | grammar #3 |
| **C-5** | D-146 | **Associated types must be referenceable or descoped.** They parse and bind but there is no `TY_ASSOC`, no in-trait resolution, no projection syntax (`T.Item`) — so TRAITS_REFERENCE's own `Iterator::next → Item` does not typecheck. Give them a type kind + resolution + impl-binding substitution, **or** descope assoc-typed signatures from 1.0 by decision. | 1.0 | grammar #4 |
| **C-6** | D-147 | **Impls over a generic type family, and derive on generics.** `impl:Container<T>:Trait` is grammatically inexpressible (the generic list *replaces* the target, D-031), and `#[derive]` on a generic struct emits a broken `impl:Container:Eq`. Decide: add an `impl:<T…>:Type<T>(:Trait)` form (a grammar change — weigh against frontend finality) or make per-instance impls the doctrine; make derive refuse generic subjects by name either way. Folds in: default-method dispatch on concrete receivers (grammar #6) and object-safety rule 3's three contradictory statements (grammar #8). | 1.0 | grammar #5,#6,#8 |

---

## 1b. Raised DURING cycle 1.0 — still open

Section 1's rows were the cycle's opening blockers and are closed. These were
found while implementing it, so they are recorded here rather than folded into a
closed table.

| # | Proposed | Item | Blocks | Source |
|---|---|---|---|---|
| ~~**C-20**~~ | **D-164** | **SETTLED at 1.0.6b's close: `T.Item`, a dotted suffix in type position**, implemented at 1.0.6c — with the bound-ambiguity rule and D-159's module-qualified key as part of the same change. The bare-`Item`-through-the-bounds alternative was rejected for ambiguity under two bounded parameters; descoping was rejected because it makes `assoc` versus a trait parameter depend on what a downstream consumer wants rather than on the trait's own semantics. ~~**How is an associated type PROJECTED from outside its trait?** D-160 says `T.Item` "rides the EXISTING dotted-path type grammar **if the parser already admits it**". **It does not** — measured at 1.0.6, six `NITPICK-PARSE-001` — and the fallback the decision names, `Iterator.Item`, is dotted too, so its "no new token, no new node kind" is unachievable for either candidate. Neither the spec (§2.3 shows declaration and binding only) nor the prototype (`AssociatedTypeDecl`/`AssociatedTypeBinding`, no projection node) has ever had one, so this is D-160's own addition rather than a restatement. **It matters**: every binding spells its type, so generic code over `Iterator` cannot declare a variable holding what `next()` returned. Candidates: add `.Name` as a type suffix (one branch in `p_suffixes` plus one `TypeKind`, and it also gives qualified type paths, which are impossible today — but it REOPENS D-159's tie, and needs a bound-ambiguity rule for a `T` whose two bounds both declare `Item`); or descope external projection by decision. **C-21 is now CLOSED**, so the comparison is real and this is decidable.~~ | ~~1.0.7~~ | 1.0.6 |
| ~~**C-21**~~ | — | **CLOSED at 1.0.6b**, and it was three defects rather than one: the reported break (the call side never bound the trait's parameters, fixed with the same `trait_binding` the impl side has used since D-111), plus two regressions the suite was green over — 1.0.5c's vtable thunk reading the trait's signature unbound, and 1.0.5b's symbol change leaving every call through a BOUND naming a symbol that no longer existed. All three were invisible because no test called a method through a generic parameter's bound; `trait_bounds.npk` now does. ~~**A generic trait used as a generic function's BOUND breaks the trait's own parameter.** `trait:Producer<T> = { func:make = T(Self:self); };` typechecks alone and with an impl; adding `func:twice<P: Producer<int32>> = …` makes the TRAIT's own `T` report `NITPICK-TYPE-001` at its declaration. **Verified pre-existing** by building HEAD in a separate worktree, so it predates 1.0.6. It means the "use a generic trait instead of an associated type" route does not currently work — which is why C-20 cannot be decided against it yet. Owned by **1.0.6b**.~~ | ~~1.0.6b~~ | 1.0.6 |

---

## 2. Decisions blocking 1.1 (async, concurrency) — and the 0.10 dependency

1.1 additionally **hard-depends on cycle 0.10** (arenas). Beyond that:

| # | Proposed | Item | Blocks | Source |
|---|---|---|---|---|
| **B-2** | D-148 | **`Duration`, a monotonic clock, and executor timers** — used by every deadline API (D-056/62/71/83, all of IO/concurrency), defined in no spec. Specify: the `Duration` type + layout (i64 ns vs timespec pair) + arithmetic + overflow; the **relative-span vs absolute-timepoint** question (every API names the param `deadline` but types it as a span); `CLOCK_MONOTONIC` acquisition through the floor; futex-timeout executor integration; and a pinned `DEADLINE_EXCEEDED` code in the D-141 space. **Safety — hard blocker for 1.1.** ~~Struck at 1.1.3~~ — **SETTLED as D-176**: `Duration` = prelude `{ int64:ns }`; parameters are relative spans named `within`; `mono_now()` through the floor; absolute-monotonic futex waits; `DEADLINE_EXCEEDED` pinned at −4107. | 1.1 start | concurrency F1 |
| **C-7** | D-149 | **Coroutine lowering** — currently one sentence. Specify the coro ABI (switched-resume vs async), the await suspend/resume protocol and `result_slot` ownership, how `drop work()` spawns and how the enclosing scope tracks spawned tasks for the D-062 join, and the cooperative wind-up token's home + wake-parked protocol. Depends on the 0.10 **executor frame allocator** (distinct from surface `arena<T>`, which cannot size coroutine frames — see 0.10). ~~Struck at 1.1.4~~ — **SETTLED as D-177**: hand-lowered switched-resume state machines (the `@llvm.coro` assumption reversed — the emitted IR stays the program); frame = header + crossing locals, one exact size per async fn (D-153's bucket); result_slot owned by its frame; spawn links the child onto the scope's join list; the wind-up token is a frame word polled at every resume. Under D-163 a spawned task's error is never discarded: `drop work()` keeps its spelling, and the task's `Result` error reaches the D-062 join, which relays the first child error, verbatim, after every child has finished (settled with the user in D-163 rule 4 — structured concurrency's rule, the natural completion of lexical task lifetime). The join is designed to this; it is not open. | 1.1 | concurrency F2 |
| ~~**C-8**~~ | D-150 | **Narrow the borrow-across-await rule.** D-004 rule 4 ("no borrow across an await") contradicts the async I/O traits (a slice param is a borrow held across the call's own await) and the channel-endpoint-across-spawn model; the shipped escape check enforces a third, narrower variant. Since D-032/62/83, an intra-task borrow cannot outlive its frame across a suspension. **Proposed:** a borrow may be passed into and held by a directly-awaited callee; what remains refused is a borrow crossing a **spawn** (task or thread) — which `escape_spawn_args` already implements. Keep `BORROW_SUSPENDED` for the residue. Also fixes the borrow-checker deep dive's obs. #1. **SETTLED as D-180** (1.1.8): narrowed exactly as proposed, on evidence D-177 created — the suspend walk marks every address-taken local in an `async` function as crossing and stage D frames it, so the dangling case cannot be spelled and the blanket rule was refusing the async I/O surface for a hazard the compiler had removed. BORROW-004 is wired to the spawn form (the aliasing half, per D-083); BORROW-005 retires with its reasoning kept. | 1.1 (done) | concurrency F3; borrow deep dive |
| **C-9** | D-151 | **Construction & threading APIs** — no channel constructor; `Job` undefined (closures removed, D-018 → fn-ptr + owned context? `dyn`?); `Thread.spawn`/executor-creation unspecified though D-083 hangs the join deadline on "where the executor is created"; actor definition syntax absent; CondVar mutex-handoff protocol unstated; async trait methods unaddressed in TRAITS_REFERENCE (a `dyn Writer` coroutine's frame sizing at the callsite is genuinely hard). Also settle `atomic<T>`'s permitted-`T` set, method return types (`compare_exchange`'s `{T,i1}`), and Result-exemption. | 1.1 | concurrency F4,F6 |
| ~~**B-3a**~~ | **D-184** | ~~**io_uring vs epoll** — decide one initial mechanism (proposed: epoll + timerfd first, io_uring as a measured upgrade behind the same suspension interface), and the buffer-ownership rule for in-flight kernel I/O (an io_uring SQE holds the buffer past the call's return — it must be owned, not borrow-backed). Scope: whether 1.1's executor even includes the file/socket reactor or only futex-parking + timers + channels.~~ **SETTLED at 1.1.12a as D-184: epoll, and only epoll — no timerfd** (the executor's own sleeper machinery carries every deadline; `epoll_pwait`'s timeout is the same deadline the futex wait took). `suspend_io` + prelude `io_ready` + deferred `io_unwatch`; EPOLLONESHOT with the TASK frame as payload; eventfd as the cross-thread wake channel. io_uring is **not going in before Astrée** — a decision, not a deferral: an SQE owns its buffer past the call's return, a categorically larger surface. The rung also forced the **task-identity rule** (every waiter registration resolves `cur_task`, fixing a latent nested-wait lost-wakeup) — see D-184. | 1.1 (done) | concurrency F5 |

---

## 2b. A safety decision 1.1.12b surfaced (pre-Astrée; owner: the user)

| id | proposed | question | needed by | source |
|---|---|---|---|---|
| ~~**S-1**~~ | **D-186** | **A `string` cannot say "view", and a view can outlive its body.** `string_slice` returns a VIEW (`cap == 0`, ptr into the source's body — D-183's ownership bit), so `x = string_slice(x, lo, hi);` frees the body the view points into: a THREE-TOKEN silent use-after-free, found when `path_parse` wrote it and the quarantine caught the 0xAA. The type system cannot see it — owner and view share the type `string`, and the ownership bit is runtime state. The prelude now copies before reassigning (discipline, not enforcement). **Candidates:** (a) `string_slice` returns an OWNED COPY — one allocation per slice, and the whole silent-dangling class disappears (recommended: safety > performance, and the compiler's own process-lifetime-view uses stay correct, just costlier); (b) a distinct view TYPE (`strview`?) the checker can escape-analyze — bigger, precise, D-070's slice story extended to strings; (c) refuse `x = f(x)` shapes where f is view-returning — narrow, misses flows through locals. ~~**Must be decided before the Astrée trial**~~ **SETTLED as D-186 (user-ratified): candidate (a)** — `string_slice` returns an owned copy; `string_from_bytes` stays the explicit view primitive over caller-owned buffers. The fix's fallout also closed D-183's field/element OVERWRITE leak (drop-old on owning field and managed-element targets). | 1.4 (verification prep) | 1.1.12b; D-185 |

## 2c. An analysis hole D-186's fallout exposed (owner: 1.3's analysis pass)

| id | proposed | question | needed by | source |
|---|---|---|---|---|
| ~~**S-2**~~ | **D-208** | **SETTLED at 1.4.0, lands at 1.4.3.** ~~The moved-from analysis is straight-line: a `move(x)` re-executed by a LOOP is not refused.~~ `modmap_members` moved one `move`-parameter at every member — every table entry aliased one body — and it type-checked. Latent-harmless while the aliased strings were views; D-186's owned slices turned the duplicate drops into a double free `npk_small_free`'s bitmap caught on the first stage-1 run. The compiler source is fixed (intern once, reuse the id), but the CLASS is open: the 0.5 use-after-move analysis needs loop-carried states (a binding moved anywhere in a loop body is moved-from at the body's head unless reassigned on every path). Same fixed-point shape the read-before-assign analysis already runs — extend, don't invent. | 1.4 (before self-hosting is declared) | D-186 fallout; 1.1.13-era find |

## 2d. ~~Two `sys`/emit robustness gaps the Bridge's shm work surfaced~~ CLOSED by D-192 at the 1.1 interlude

Both rows were one hole — `sys` carried the bare-builtin UNKNOWN type. D-192
types the call (`Result<int64>`, D-048) and its arguments (integer-family ≤64
bits, kernel ids, pointers; arity 1..7; unknown-typed arguments refuse with
"bind it to a typed name first"), picks the extension by signedness (`zext`
for unsigned and kernel ids — the blind `sext` smeared a `uint32` high bit),
and gives `?|` the `?!` unknown-operand fallback (typer AND emitter halves).
`tests/types/rejection/sys_args.npk` (ten refusals), `sys_typed.npk`,
`unwrap_unknown.npk`. **The residue became P-3 below** (§3): a still-unknown
builtin's `?|` default or argument shape mismatch surfaces only at llc until
the builtin surface is typed from a signature table.

| id | what | evidence | fix |
|---|---|---|---|
| ~~**S-3a**~~ | **CLOSED (D-192).** ~~`sys` accepts a non-integer, non-pointer argument and emits invalid IR.** A `Result`-typed argument (a `never fails` constant called WITHOUT `raw`, so `{i64,i32}`) reaches the sys arg loop's fallback branch, which `sext`s it — `sext {i64,i32} to i64`, which llc rejects. The frontend should refuse a `sys` argument that is not an integer or a pointer, at the call, rather than letting the emitter produce a cast llc kills. | `shm_sealed.npk` with `sys(SYS_FTRUNCATE(), …)` (raw-less) — llc "invalid cast opcode for cast from '{ i64, i32 }' to 'i64'". | a typing check on sys arguments (integer or pointer only), TYPE-level.~~ |
| ~~**S-3b**~~ | **CLOSED (D-192).** ~~`expr ?\| fallback` fails to lower (EMIT-002) when `expr` is a `sys` call carrying a POINTER argument**, while `?!` over the same call lowers fine and `?\|` over an integer-only sys call lowers fine. | The five sys calls in `shm_create_sealed`/`shm_unmap`: with `?\|`, only the two with pointer args (memfd_create, munmap) EMIT-002'd; switching all to `?!` compiled. | isolate the `?\|` lowering over a Result whose value carries a pointer-derived temp; likely a temp-liveness or clean-since-mark interaction. ~~Worked around in nbridge by using `?!`~~ **(1.1.13a correction: that workaround was a MISUSE — `?!` is unwrap-or-TRAP-as, and v3 §4.2 bars the Bridge from trapping; nbridge now binds-and-fails at every site, see D-188. The `?\|` lowering gap itself still stands and still wants the isolation.)**~~ |

## 2e. Questions the instruments raised (owner: the user)

~~**S-13**~~ **CLOSED at 1.5.1 step 1 (2026-09-03, V-22): `check_parity` fails on any nonzero `npkg test` exit, listed after the verdict diff.** ~~(1.5.0, 2026-09-03): parity does not surface a non-verdict `npkg` failure.~~ The harness's `check_parity` diffs verdict lines and byte-compares the artifacts, but reads npkg's exit code only for a trap (3) or could-not-run (2), not a plain failure (1) -- and a `npkg test` self-check or toolchain-pin failure is a `note_failure`, not a verdict, so it is invisible to parity (found when npkg's `right-verdict` self-check bug, identical to the Python one the gate caught, did not fail parity). Recommendation: `check_parity` fails on any nonzero npkg exit; a full `npkg test` in the parity path already runs the self-check, so surfacing its exit is the whole fix. Small, and it strengthens what a green parity means.

| id | proposed | question | needed by | source |
|---|---|---|---|---|
| ~~**S-4**~~ | **D-215** | **SETTLED (user-ratified), LANDED at 1.4.4.** ~~Should `dyn` coercion refuse a channel-carrying concrete?~~ The 1.4.1 walker fixes closed the owned-container laundering of D-183's `gives` rule (mutex/arena/atomic returns now answer truthfully), but a `dyn` cannot: erased content could hold an endpoint, and `contains_channel(DYN) = true` would demand `gives` of every dyn-returning creator — a claim about channels most such returns do not carry. The precise fix is refusing the COERCION of a channel-carrying concrete into `dyn` — D-207's "erased content can hide a borrow [or an endpoint]" reasoning completed at the erasure boundary, where the concrete type is still known. The residual today is runtime-caught (`StaleHandle` via slot generations), never a dangling address. **Recommendation: refuse the coercion; land with D-207's per-scope-joins subcycle (1.4.4).** | 1.4.4 | 1.4.1 |
| ~~**S-6**~~ | **D-235** | **SETTLED (user-ratified 2026-09-01), LANDED at 1.4.7b step 1: a simd vector and a function value ride; the sync primitives, atomics and arenas — and whatever holds one — refuse permanently under TYPE-057; the belt names no rung.** ~~Which element kinds may a channel carry beyond the decided set?~~ (1.4.7, OWED-8.) `NITPICK-TYPE-057` now refuses the DECIDED classes in the checker — a borrow (D-072/D-183), a `dyn` (D-207), an `OwnedFd` (D-185), a `Guard` (D-056), an aggregate holding one — and admits everything that transfers whole (scalars, the tier, `string`/`buffer`, `Handle<T>`, endpoints, owning aggregates). Between the two sit kinds NO decision names, which the backend's admission table (`chan_elem_ok`) still refuses AS A RUNG by this row: `Mutex`/`RwLock`/`CondVar`/`Barrier` and `atomic<T>` (a cell other tasks may hold a borrow of — D-180 sanctions BORROWS of the primitives as spawn crossings, never a by-value transfer), `arena`/`shared_arena` (owned storage other handles point into), a function value (a code address; probably harmless), `simd<T, N>` (a plain value — its refusal looks like an oversight of the 1.3.1 sweep, not a decision), and the meta kinds. **Recommendation:** admit `simd` and function values (they transfer whole; retire the rung for both); refuse the sync primitives, atomics and arenas PERMANENTLY under TYPE-057 with their own message (the D-180 reasoning: what other tasks may borrow cannot be moved out from under them). Until ratified the rung stays, so a `TYPE` code never says "never" about a kind nobody decided. | 1.4 close | 1.4.7 OWED-8 |
| ~~**S-7**~~ | **D-236** | **SETTLED (user-ratified 2026-09-01), lands at 1.4.7b step 4: manifest-root-relative paths in the source manager, for diagnostics and the site table alike.** ~~Should the site table record paths relative to the manifest root, so the artifact cannot depend on how `npkc` was invoked?~~ (1.4.7 close.) D-179's site table stores each source path AS GIVEN: `npkc src/main.npk` from the tree root emits `c"src/frontend/token.npk"`, and the same call with an absolute path emits the absolute one — 1,489 of the 1,647 site constants in a dry-run snapshot refresh, which the fixpoint (stage2 == stage3) and the STAMP both passed, because each compares the emission with itself. D-078 says emitted bytes must not vary with the build tree and D-204's H9 names exactly this leak, but the `repro` stage tests it with absolute inputs from two cwds — which agree BY CONSTRUCTION when the argument is absolute — and the harness's own selfhost emission is invoked absolutely. The committed snapshot is clean only because `bootstrap/seed/README.md`'s commands are relative; the close added a `repro` guard that refuses an absolute site path in `stage1.ll`, which protects the committed artifact and nothing else. **Recommendation:** the source manager records every path relative to the manifest root (the directory holding `nitpick.toml`, found by walking up from the main file) for diagnostics and the site table alike — one spelling, deterministic, and the same bytes the snapshot carries today when invoked from the root; H9's `repro` leg then measures something, since an absolute argument would no longer change the emission. Until decided, the guard and the README's discipline hold the line for the committed artifact only. | 1.4 close | 1.4.7 close |
| ~~**S-8**~~ | **D-093 (annotated)** | **SETTLED (user, 2026-09-02: "the recommendations are fine"), LANDED at 1.4.8 step 2b: `range` is a builtin generic type keyword resolving to `TY_RANGE` under the range expression's own element rule; `range_type.npk` binds, passes and iterates one, `range_rules.npk` refuses a float or bool element and a second argument.** ~~D-093's `range<T>` spelling is not resolvable.~~ (1.4.7b.) D-093 (0.4.2) settles that a range is a value of type `range<T>`, "interned like every other type", and the escape analysis and the channel-element table both classify `TY_RANGE`; but the resolver has no type named `range` — `range<int32>:r = 2i32..4i32;` refuses with TYPE-001 — so the type can only ever be inferred, never written, and nothing in the tree writes one (found by the D-235 probe that tried to). A decided spelling that was never implemented is the dormant-rule pattern. **Recommendation:** implement `range<T>` as a builtin generic type name in the resolver (beside the other builtin generic names; no grammar), with a conformance case that binds and passes one, before the frontend freeze. The alternative — an annotation on D-093 striking the spelling — leaves a type with no name, which is D-093's own argument against. | before the frontend freeze | 1.4.7b |
| ~~**S-9**~~ | **D-237** | **SETTLED (user, 2026-09-02: the recommendation as written), lands at 1.4.8b step 1.** ~~Should a rejection test's expected diagnostics be matched EXACTLY,~~ as BUILD_REFERENCE §7.1 states ("unexpected diagnostics fail a test as surely as missing ones"), rather than as a subset of what was reported?** (1.4.8 Part D.) Both runners implement the subset rule the Python harness has carried since 0.8: every expected code must be reported (at its line and column when spelled), extras pass, and notes are their own channel. The spec sentence describes a rule nothing enforces — the dormant-rule pattern, in the test runner this time. **Recommendation:** enforce it, in both runners in one step, after measuring how many rejection files carry unasserted extras (the two `--verdicts` lists plus one script make the measurement): a cascade diagnostic a test does not assert is either a second finding the test should name or a cascade the checker should not emit, and either is worth knowing before 1.5 reads these suites as evidence. Until decided, parity is measured on the rule as implemented, and §7.1 says so. | 1.4.8 close | 1.4.8 Part D |
| ~~**S-10**~~ | **D-238** | **SETTLED (user, 2026-09-02: the recommendation as written), lands at 1.4.8b step 2.** ~~Should the suites `npkg test` runs beyond the `[[test]]` targets be DECLARED in `nitpick.toml` rather than built into both runners?~~ (1.4.8 Part D.) §7.1 settled `[[test]]` with three kinds and a `path`; the harness then grew the real-parser sweep, the five rejection suites, the programs and fixtures, the runtime floor's tests and the acceptance suite as hardcoded loops, and `npkg` mirrors them exactly so the parity diff covers the full tree. A manifest that declares four of fourteen suites is a manifest a reader cannot trust to say what `npkg test` runs — the stale-document shape D-204 refused for flags. **Recommendation:** extend `[[test]]` with a `stage` key naming the tool that judges the suite (`parse`, `resolve`, `check`, `compile`, `run`, `runtime`) and a `recursive` flag, declare every suite, and have BOTH runners read the one table (the harness refusing a stage it does not know, loudly) — one change to both, landing once parity has held on the hardcoded shape so the diff can prove the move changed nothing. | before the SWITCH | 1.4.8 Part D |
| ~~**S-11**~~ | **D-239** | **SETTLED (user, 2026-09-02: the recommendation as written), LANDED at 1.4.8c: `Error` and the prelude's names refused at every type-namespace declaration, `assoc` and generic parameters included, by the loader under RESOLVE-001; `owned_names.npk` pins six shapes.** ~~Should `Error` — D-179's compiler-known error type, resolved by name ahead of every user lookup so that it "cannot be shadowed into meaning less" (`resolve_type.npk`) — join the names a program cannot declare, and should an `assoc` declaration be held to RESOLVE-001 like every other declaration?** (1.4.8b step 1.) Found resolving `tests/types/rejection/assoc.npk` under D-237: its trait declared `assoc:Error = int32;` at 1.0.6, before D-179 made `Error` a type, and the checker then read the word two ways — the impl-signature comparison resolved the method's `Error` to the builtin (so the impl's `int32` mismatched, `TYPE-014`, even with `assoc:Error = int32;` written in the impl), while the object-safety walk matched it by name as the trait's assoc ("returns an associated type"). Hidden by the subset rule since 1.1. Measured with the built checker: a module-level `struct:Error = { … };` is ACCEPTED where `struct:Duration` is refused — RESOLVE-001 protects prelude-DECLARED names and `Error` is compiler-known, not declared — and `assoc:Duration = int32;` is accepted and shadows the prelude's `Duration` inside its trait (D-160's nearer-binding rule), where a module-level `Duration` is refused. **Recommendation:** one rule, no exception by declaration kind — a name the compiler or the prelude owns cannot be declared by a program anywhere, `assoc` included — under RESOLVE-001's own rationale ("a local one would silently take over") and blueprint facet 1 (`Error` means one thing in every scope); `Error` joins the protected set explicitly, since it is the one type name that is neither a keyword nor a prelude declaration; a rejection test pins `struct:Error`, `error:Error`, `trait:Error`, `assoc:Error` and `assoc:Duration`; the object-safety walk's by-name assoc match then agrees with resolution by construction. Until decided the test spells its default `Fault` and says why. | before 1.5 reads the suites as evidence | 1.4.8b step 1 |
| ~~**S-12**~~ | **D-240** | **SETTLED (user, 2026-09-02: the recommendation as written), LANDED at 1.4.8c at the three sites, each at its emitter, the second expectations removed.** ~~Should a sharper refusal suppress the generic one it was written to replace?~~ (1.4.8b step 1.) D-237's exact matching surfaced three sites where two rules report one mistake: `builtin_args.npk` 36:27 (`TYPE-054` for `..^` into a builtin AND `TYPE-007` for the spread argument's type), `builtin_args.npk` 95:5 (`TYPE-007` "`drop` needs a `Result`" AND `TYPE-042` "`drop` discards a VALUE"), and `impl_old_blanket.npk` 22 (`TYPE-012`, whose header says it exists so the reader is not left with the generic "is a trait, not a value type" — AND that generic `TYPE-002`). All three are named in their tests now, so nothing is hidden; the question is diagnostics quality. D-157's rules 1 and 2 are the precedent for one mistake, one report, and `type_trait.npk`'s no-binding branch cites it. **Recommendation:** yes, where the sharper rule fires the generic one stays silent, each site fixed at its emitter with the test's second expectation removed in the same commit; a small, self-contained item for the 1.4.9 close or the first 1.5 subcycle that touches the checker. | 1.4.9 or early 1.5 | 1.4.8b step 1 |
| ~~**S-5**~~ | **D-216** | **SETTLED (user-ratified): the consuming `pick (move(v))` — ownership transfers into the matched arm; lands at 1.4.3b.** ~~An owning enum payload cannot be read back out — enums with owning payloads are write-only containers.~~ TYPE-046 correctly refuses a `pick` arm binding an owning payload (the binding is a copy; two owners, one double free), and no move-binding form exists in patterns — so `enum:Res = { Note(string); }` can be constructed and dropped but its string can never be recovered. The missing form is a CONSUMING destructure: a `pick` over `move(v)` whose arms receive ownership of what they bind (the enum's own drop then does not run — the arm took the payload). Spelling and semantics are a language decision. Natural home: beside S-2's loop-carried move work (1.4.3), which is already in the move analysis. | 1.4.3 (or its own slot) | 1.4.1 |
| ~~**S-19**~~ | **D-246** | **SETTLED (user, 2026-09-03: the recommendations as written), lands at 1.5.1b step 4.** Statement-end temporaries. An owning value no place takes is a temporary of the statement that produced it, dropped when that statement ends on every path (relay/pass/fail/return included; a trap runs none, D-014); TAKEN — and not dropped — when bound, assigned, passed to a `move` parameter, stored into a literal slot, sent, or returned; a coroutine's only cross-suspension temporary is an `await` argument, frame-resident (D-178). Closes D-183's recorded item. Recommendation: ratify as `1.5.1b.md` §6 step 4 states it. | 1.5.1b step 4 | §2f's probes: 260 KiB vs 429 740 KiB |
| ~~**S-20**~~ | **D-247** | **SETTLED (user, 2026-09-03: the recommendations as written), lands at 1.5.1b step 5.** `List<T>` is compiler-known and OWNING: declared in the prelude, `type_drops` true, its generated drop drops the `count` elements through `T`'s drop then frees the block; move-only under TYPE-046 (which also refuses the aliasing copy a `wild` block permits). Alternatives decided out: `buffer`-backed (leaks the elements), a user drop hook (a destructor design), manual `list_free` + `defer` at 153 holders. Recommendation: ratify. | 1.5.1b step 5 | `npkc src/main.npk` at 11.0 GiB |
| ~~**S-21**~~ | **D-248** | **SETTLED (user, 2026-09-03: the recommendations as written — both halves), lands at 1.5.1b step 1.** The file header is mandatory, and entry points are the root's: every file's first declaration is `mod:<basename>;` (RESOLVE-012, one code, two texts — missing, mismatched), so a header can never load a sibling and the loader can finally say "your header is wrong"; `main`/`failsafe` outside the root module refuse (RESOLVE-013). The sweep is 240 header-less files plus a one-line pin shift D-237 verifies. Alternative (header optional, identified by name) named in §7 and recommended against. Recommendation: ratify both halves. | 1.5.1b step 1 | DEF-2 |
| ~~**S-22**~~ | **D-249** | **SETTLED (user, 2026-09-03: the recommendations as written), lands at 1.5.1b step 2.** A `Views` column in BUILTIN_REFERENCE (`—` or the 1-based argument whose storage the result aliases; `string_bytes` 1, `string_from_bytes` 1), generated like `Pure`; the escape analysis treats such a call — and the range-view `arr[lo...hi]` — as a borrow rooted where that argument is rooted, so D-004 rule 2 and rules A/B apply unchanged. Recommendation: ratify (a hard-coded pair of names would be a parallel authority beside the 1.4.2 table). | 1.5.1b step 2 | DEF-3 |
| ~~**S-23**~~ | **D-250** | **SETTLED (user, 2026-09-03: "ratify S-23 as recommended, add step 3b"), lands at 1.5.1b step 3b — and the struct half was measured the same day: `#[derive(Eq, Ord)] struct:Outer = { Inner:i; int32:b; }` with `Inner` derived the same refuses TYPE-034 and TYPE-008 inside `<derived-1>`, because every derived comparison is an operator and operators are refused on named types; the step covers named types in structs and enums alike.** ~~Derived comparisons on an enum WITH A PAYLOAD~~ (DEF-4, §2f). Today `#[derive(Eq)]` on a payload enum refuses inside `<derived-1>` (the generated body is `self == other`, which needs the trait it is writing) and `#[derive(Ord)]`/`PartialOrd` compile to a TAG-ONLY order (`gen_cmp_enum`, by a 1.0.9d design comment that was right for `Hash` and is wrong for an order: `Literal(7).cmp(Literal(9))` is `Equal`). **Recommendation:** (1) a derived `Eq`, `Ord` and `PartialOrd` on an enum compare the TAG first (declaration order, as today) and, for equal tags, the payload of that variant through the payload type's own `==`/`eq` and `cmp` — scalars by operator, user types through their impl, derived or written — generated as a `pick` over both operands per variant; (2) a payload whose type is not `Eq`/`Ord` refuses AT THE DERIVE SITE naming the user's declaration and the payload type (the D-194 simd wording), never inside `<derived-1>`; (3) a payload that OWNS (a `string`, a `List<T>`) refuses the derive by name too — a `pick` over a borrowed enum cannot bind an owning payload without consuming it (S-5/D-216 made the consuming form; no borrowing form exists), so until one does the impl is written by hand and the refusal says so; (4) `Hash` stays tag-only (D-123, legal); (5) `gen_eq_enum`'s stale comment ("that is why `Ord` on an enum is refused rather than generated") goes. Lands as a 1.5.1b step (proposed 3b, between DEF-1's builders and D-246: frontend-only, no emission of `src/` moves) with `tests/derive/` cases deriving all five on a payload enum and a program pinning `Less/Equal/Less` (the reporter's 321). Whether the language wants a BORROWING `pick` form (which would let (3) generate instead of refuse) is a separate question, not needed to close DEF-4. | 1.5.1b step 3b | DEF-4 / O-N10 |
| ~~**S-24**~~ | **D-253** | **SETTLED (user, 2026-09-04: "ratify all seven as recommended"), scheduled as 1.5.2b (frontend and prelude only, after 1.5.2, before 1.5.3).** ~~OPEN (raised at 1.5.1b step 3b, 2026-09-04).~~ A derived comparison over a GENERIC PARAMETER field compares by operator (D-161's no-bound story), so `#[derive(Eq)] struct:Box<T>` works and `#[derive(Ord)]` on the same is refused by the checker inside `<derived-1>` (`<` on an opaque `T`, D-107) — before and after step 3b. The method form (`self.v.cmp(other.v)` under a synthesized `T: Ord`) would lift that, and needs the prelude to implement `Eq`/`Ord`/`PartialOrd` for the scalars, which it does for none today (the plan's §5b rule 1 assumed it did). **Recommendation:** implement the three traits for every scalar in the prelude (an `impl` per width, generated from the width ladder like the `Hash` impls), synthesize `T: Eq`/`T: Ord` bounds on a derived impl whose subject is generic, and take the method form for parameter fields — one rule for every named spelling. Costs: a bound on a derived impl changes which instantiations compile (a `Box<Point>` needs `Point: Ord`), which is the truthful requirement; the prelude grows ~30 impls. A 1.5.x step once decided. The workbench, asked: nothing there is blocked today (no library wants an order over a parameter yet); the first to meet it would be `nitpick-regex`'s generic container, ordering a `Vec<T>` of small scalars, and a hand-written comparator serves until then — information, not a request. | — | 1.5.1b step 3b |
| ~~**S-25**~~ | **D-247 (annotated)** | **SETTLED (user, 2026-09-04: "ratify all seven as recommended"), LANDED at 1.5.1b step 5b: the struct AND its functions live in the prelude.** ~~OPEN (raised at 1.5.1b step 5, 2026-09-04).~~ Step 5b's scope: `List<T>` moves into the prelude after the close-out refresh — the STRUCT ONLY, or the struct AND its functions (`list_init`, `list_push`, …)? Recommendation: BOTH — a compiler-known owning type whose operations need an import is one spelling in the prelude and another at every use, the context-dependent shape the blueprint rule refuses, and `list_push`'s `move T:v` is the one place the emitter's take-by-`move` (D-246) meets a prelude-declared callee. The move needs a BRIDGING BUILD because the prelude is embedded in a compiler at its own build and `src/` already uses `List` (1.5.1b.md §6, step 5b). |
| ~~**S-26**~~ | **D-254** | **SETTLED (user, 2026-09-04: "ratify all seven as recommended"), LANDED at 1.5.1b step 5; D-251 adds the one exception (no move out of a sub-place of a LIMITED binding).** ~~OPEN (raised at 1.5.1b step 5, 2026-09-04; implemented as the fix, pending ratification).~~ A `move(place)` or `pass place` out of a FIELD or an ELEMENT of an owning aggregate leaves the type's canonical VACANT value (D-225) in the place; the aggregate stays live, its later overwrite drops nothing (D-186's unconditional field drop is then correct), its scope-exit drop releases the remaining fields, and only a WHOLE-binding move clears a drop flag (D-183). This closes D-183's recorded partial-move item both ways: before it, a field move cleared the whole root's flag (every sibling leaked) and, because the field overwrite drops unconditionally, a field moved out and then reassigned freed the moved-out value a second time — `saved = move(r.env); r.env = move(frame);` in the resolver's constant folding, invisible to the compiler (its `main` exits without drops) and a heap fault in three unit tests the day `List<T>` began to own. A vacant List grows from zero on its first reservation. The checker's D-065 whole-binding invalidation is unchanged (conservative). **Recommendation: ratify as the settled meaning of a partial move** — one rule ("after `move`, the source owns nothing") for both spellings, no new syntax, no field-granular flags; the alternative, refusing partial moves, would strike the resolver's own idiom. `partial_move.npk`; DECISIONS D-183's dated note. |
| ~~**S-28**~~ | **D-251** | **SETTLED (user, 2026-09-04: "ratify all seven as recommended"), LANDED at 1.5.2 steps 1–3 the same day.** ~~OPEN (raised at 1.5.2 planning, 2026-09-04; `1.5.2.md` §9.1 — the semantics batch, needed before step 1).~~ (a) A limited binding's check runs AFTER the write, over the binding's WHOLE current value, at its initialiser, at every assignment to it or to any part of it (a field or element store re-checks the root), and at the callee's entry — one shape, sync and coroutine alike (L-3). (b) The residue is `LimitViolated`, −4111, through D-142's route with `npk_chain_reset` first; 1.5.3's three are `RequiresViolated`/`EnsuresViolated`/`InvariantViolated` at −4112…−4114, reserved now (L-14). (c) **A limited binding has no address**: `@`, `$$m` AND `$$i` of a place rooted at a limited local or parameter refuse (NITPICK-TYPE-063), and so does a `move`/`pass` out of a proper sub-place of one (S-26's vacate is a write no rule can admit); a limited value passes by value, and a whole-binding move is a read (L-4). Measured: all three spellings and a store through each are ACCEPTED today. (d) A `limit` where no write point exists refuses (NITPICK-TYPE-064): a trait signature's parameter (accepted and silently dropped by the impl today), a `wild`/`wildx` binding (accepted today), a `comptime` function's parameters and locals; `main`/`failsafe`'s parameters under D-244's arm (accepted today). (e) `limit-subsume` rows are one per DIRECT call site of a callee with limited parameters (the callee the checker recorded), guard `no`, elision `none`, exactly the ratified catalogue (L-10). **Recommendation: ratify (a)–(e).** The one alternative worth naming is (c)'s relaxation — admit `@x`/`$$m x` as a direct call argument of a call whose result holds no address, that call then a write point of `x` whose row is always `open` — which keeps `list_push(@xs, v)` on a limited list at the price of a second write-point class, a result-shape rule and per-read hypotheses for escaped names; the by-value rewrite covers it. | 1.5.2 step 1 | 1.5.2 planning |
| ~~**S-29**~~ | **D-252** | **SETTLED (user, 2026-09-04: "ratify all seven as recommended"), LANDED at 1.5.2 step 4 the same day.** ~~OPEN (raised at 1.5.2 planning, 2026-09-04; `1.5.2.md` §9.2 and L-13 — needed by step 4 only).~~ The caller-side bypass D-220 names ("caller discharge is an elision like any other"): a SYNC function with a limited parameter emits its body under `@"<sym>.body"` and the ordinary symbol as the CHECKED ENTRY (the entry checks, then a `tail call` of the body); every non-call reference — function values, vtable slots, spawn entries, stubs — names the ordinary symbol by construction; a DIRECT call whose `limit-subsume` row is discharged calls the body, every other call and every call of a coroutine the checked entry; the row's elision reads `elided`/`retained` and D-218.7's catalogue changes the `limit-subsume` guard column from `no` to `yes (the callee's entry check, at that call)` with a dated note; a belt in both runners: every `.body` occurrence is a `call`/`tail call` callee or its own `define`, and the count of `.body` callees equals the discharged `limit-subsume` rows. **Recommendation: ratify.** Without it nothing a caller proves ever removes a limited PARAMETER's check — the common placement pays the full price in every build, and D-068's "constrained code reaches the speed of unconstrained code" is false for it. Struck, steps 0–3 stand unchanged and the row is evidence only. | 1.5.2 step 4 | 1.5.2 planning |
| ~~**S-30**~~ | — (a README row) | **SETTLED (user, 2026-09-04: "ratify all seven as recommended"): 1.5.4b "the remaining theories" is in the README's map, planned when 1.5.4 closes.** ~~OPEN (raised at 1.5.2 planning, 2026-09-04; `1.5.2.md` §9.3).~~ D-218.4/5 ratified the QF_BV crossing for bitwise operations, the scaled unbounded-Int encoding with ERR-sentinel rows for `tbb`/`tfp` (and `dim256` through it), and the two float tiers — and the 1.5 subcycle map assigns none of them to a subcycle; 1.5.8's `err-exit` rows presuppose the twisted encoding. Until they land every `limit` over such a subject, and every division or overflow in those families, is `unencoded` or absent (its guard retained — safe, unproven). **Recommendation: a new subcycle 1.5.4b, "the remaining theories", between 1.5.4 and 1.5.5**, planned when 1.5.4 closes so the path-condition machinery exists first; the README's map gains the row on ratification. | before 1.5.8 | 1.5.2 planning |
| ~~**S-27**~~ | **D-255** | **SETTLED (user, 2026-09-04: "ratify all seven as recommended"), LANDED at 1.5.1b step 5.** ~~OPEN (raised at 1.5.1b step 5, 2026-09-04; implemented as the fix, pending ratification).~~ The statement after `wild_release_all()` in its block must be `exit` — TYPE-062. The call unmaps every chunk of both regimes (D-151), so no drop, no allocation and not even the trap route (which allocates its origin chain) can run after it; a `main` that released and then RETURNED ran its scope-exit drops over unmapped memory the day `List<T>` began to own, and the runtime's refusal then died in its own trap route — an uncontrolled stop. What must be measured after the release goes into `exit`'s operand, which is evaluated after the call (`argv_after_release.npk`, `leak_cleanup.npk` rewritten so; 45 test files carried a stray second call, collapsed). **Recommendation: ratify** — one shape, greppable, and the only one under which "controlled shutdown" survives the release. |
| ~~**S-31**~~ | **D-257** | **SETTLED (user, 2026-09-05: "I say go with those" — as recommended), lands at the 1.5.2b step the row names.** ~~OPEN (raised by 1.5.2b's planning, 2026-09-05; `1.5.2b.md` §9 Q1).** Should the prelude implement `Clone` and `Debug` for every scalar beside D-253's three? A derived `Clone` over a generic subject is a double free waiting for an owner (DEF-18) and the fix — a member-wise clone under `T: Clone` — needs `int32.clone()` for `Box<int32>` to keep working; `Debug` under its truthful bound needs `int32.debug()` the same way. **Recommendation:** ratify both — `Clone` as `pass self` for every scalar, `Debug` as the scalar's `ToString` (one meaning) for every scalar that has one; generated with the region. | 1.5.2b step 2 | 1.5.2b planning |
| ~~**S-32**~~ | **D-258** | **SETTLED (user, 2026-09-05: "I say go with those" — as recommended), lands at the 1.5.2b step the row names.** ~~OPEN (1.5.2b §9 Q2).** Amend D-250 clause 2: a builtin SCALAR member of a derived body is reached through the prelude's impl of the trait being derived, not by operator — so the float `nan` answer and the twisted ERR trap live in one place, and a derived `Ord` over a `bool`, a kernel identifier or a float is a refusal at the user's declaration instead of a lie (`partial_cmp` over a `flt64` field answers `Equal` for `nan` today) or a `<derived-N>` type error. **Recommendation:** ratify; the alternative special-cases floats in the generator, a second home for float semantics. | 1.5.2b step 3 | 1.5.2b planning |
| ~~**S-33**~~ | **D-258** | **SETTLED (user, 2026-09-05: "I say go with those" — as recommended), lands at the 1.5.2b step the row names.** ~~OPEN (1.5.2b §9 Q3).** Should a derived `Debug` reach a named or parameter member through `debug` (bound `T: Debug`) rather than through `ToString` as it does today for named fields? **Recommendation:** ratify — the trait being derived is the trait reached, one rule; it changes what a nested named field renders as under a derived `Debug`, which no test pins. | 1.5.2b step 3 | 1.5.2b planning |
| ~~**S-34**~~ | **D-257** | **SETTLED (user, 2026-09-05: "I say go with those" — as recommended), lands at the 1.5.2b step the row names.** ~~OPEN (1.5.2b §9 Q4).** Should `string` implement `Eq`/`Ord`/`PartialOrd` in the prelude (byte-lexicographic order), and a `string` FIELD derive through them while a `string` PAYLOAD stays refused by the pick rule? **Recommendation:** ratify — without it `Box<string>` can never derive `Eq`, and no program can supply `string: Eq` for everyone. | 1.5.2b step 2 | 1.5.2b planning |
| ~~**S-35**~~ | **D-259** | **SETTLED (user, 2026-09-05: "I say go with those" — as recommended), lands at the 1.5.2b step the row names.** ~~OPEN (1.5.2b §9 Q5).** Should every diagnostic that lands inside a `<derived-N>` file be RE-HOMED to the subject's declaration with the derive named, and both runners refuse a `<derived-` path as a belt? **Recommendation:** ratify — one mechanism for every residual case of D-250's recorded gap, and the class becomes unreintroducible. | 1.5.2b step 4 | 1.5.2b planning |
| ~~**S-36**~~ | **D-257** | **SETTLED (user, 2026-09-05: "I say go with those" — as recommended), lands at the 1.5.2b step the row names.** ~~OPEN (1.5.2b §9 Q6).** `Hash` for the rest of the ladder: the hand-listed impls stop at 64 bits. **Recommendation:** generate the mechanical set (the wide ints, `tbb128/256`, the ternary four, the flags — the value truncated to 64 bits, as the existing rows do) in step 2 and decide the floats (`-0.0 == 0.0` must hash equal), `tfp`, `frac` and `complex` (canonical ERR) OUT — a program writes those and says what they mean. | 1.5.2b step 2 | 1.5.2b planning |
| **S-37** | — | **OPEN for the library era (the user, 2026-09-05: not this subcycle, as recommended).** (1.5.2b §9 Q7.) An orphan rule: a program may implement a prelude trait for a builtin the prelude does not cover (`impl:bool:Ord`, measured admitted), and two libraries doing it collide under coherence. **Recommendation:** not this subcycle; recorded for the library era with the shape "an impl names at least one type or trait declared in its own manifest's tree" — the workbench is the first to meet it. | — (the library era) | 1.5.2b planning |
| ~~**S-38**~~ | **D-262** | **SETTLED (user, 2026-09-05: "lets go with your recommendation"), LANDED at 1.5.2d (2026-09-05): the frontend's three scaling defects fixed (the floor-only probe's frontend 0.72 s → 0.07 s, the compiler's own build 242 s → 20 s and 13.4 GB → 113 MB peak), unreferenced prelude items not emitted (the probe's IR 845,283 → 50,561 bytes), the belt in both runners; a checked-once prelude is OUT by decision.** ~~OPEN (the user, raised 2026-09-05 at the 1.5.2c close by the library workbench, nitpick-libs_s0, its W-27).~~ The prelude's generated scalar impls (D-257, 1.5.2b step 2: 348 rows in thirteen families) are EMITTED INTO EVERY PROGRAM whether reached or not, and the workbench measured the price differentially on the two pinned compilers, `94874ce` (the 1.5.1b close) and `0dfddac` (the 1.5.2c close), same inputs, same machine: a 14-line program that does nothing but `exit 0i32` with a `failsafe` emits 845,282 bytes of IR where it emitted 456,517 (+388,765), takes 0.85 s where it took 0.10 s (8.5×), and peaks at 102,404 KiB where it peaked at 21,456 KiB (4.8×); a 30,000-row table program goes 1.18 s → 2.05 s and 74 MB → 119 MB. The `.ll` delta is 388,765 bytes TO THE BYTE for 22 of 30 library programs compiling clean under both; the eight that differ (388,125…390,837) are every derive or enum program, where 1.5.2b/1.5.2c changed the semantics — so the constant is a fixed prelude increase, not a compile-time regression. 1.5.2b measured and recorded the compiler's OWN build (+2.2% IR, +14% frontend time, "not a failure") and never the fixed per-program cost, which is what a library harness pays per program rather than per program size: `nitpick-regex`'s 63-program harness would roughly double. Blocks nothing; correctness untouched. **Recommendation:** (1) reachability-driven emission of NON-GENERIC prelude bodies — a prelude function (an impl method or a free function) is emitted only when reachable from the program's roots (`main`/`failsafe`, the vtables its `dyn` coercions build, its function values, its spawns, its drop bodies, the runtime seams), by the demand walk the emitter already runs for generic instances (`note_family_instance`): deterministic, semantics-neutral, and it keeps unreached prelude bodies out of the analyzers' evidence in 1.6 and out of 1.5.3's per-function contract obligations; (2) measure the FRONTEND's share first — parsing, resolving and typing the prelude is paid per compile whatever is emitted, and if it is most of the 0.75 s the question becomes a checked-once prelude, a larger design; (3) either land (1) as a subcycle before 1.5.3 or accept the price by decision and record it in D-257's landed note. Owner: the user (a decision); implementer: the `src/` writer. | 1.5.3, or accepted | the library workbench's differential measurement, 2026-09-05 (O-N4 stays discharged: 2.05 s against the original 281 s) |
| ~~**S-39**~~ | **D-263** | **SETTLED (user, 2026-09-05: "i am fine with your recommendation"), LANDED at 1.5.2e step 1 (2026-09-05; `list_in_main.npk`, `prelude_only_builtin.npk`): the prelude's `List<T>` stores through `alloc_managed`, the floor's untracked entry, prelude-only by TYPE-054; D-151 keeps counting every `wild` block.** ~~OPEN (the user, raised 2026-09-05 at the 1.5.2d close).~~ An owning `List<T>` local alive in `main` at `exit 0` is reported by D-151 as `WildLeak`, exit 94, under the 1.5.2c-close compiler as well (found writing `generic_move_out.npk`; every `List` test in the tree keeps its list inside a function that RETURNS, which is why it stayed latent). Two decisions meet here and both are right on their own terms: D-183's amendment keeps the scope-drop walk off the `exit` path ("walking the entire live program state to free it on the way out adds failure modes to exactly the path that exists to have none"; 1.4.4's rider: `exit` runs joins and defers and nothing else), and D-151 counts every WILD allocation alive at `exit 0` as a leak. A `List<T>` is MANAGED by D-247 -- owning, move-only, dropped at scope exit -- but its buffer is spelled `wild` in the prelude (`alloc` in `list_init`/`list_reserve`), so it is the one managed value D-151 counts; a string leaked the same way is invisible, because the managed heap is not the tracked one. **Recommendation:** (a) the List's buffer allocates through the managed heap's untracked entry (a channel ring's `npk_alloc_internal`, role 0, is the precedent: managed storage the kernel reclaims at exit and D-151 never counted), so a `List` in `main` at `exit 0` is what every other managed value already is, and the exit path stays free of the drop walk as D-183 decided -- SCOPED to the prelude's `List<T>`, whose regime D-247 made managed: D-151 keeps counting every `wild` block, because a hand-written container that chose `wild` (the workbench's `Vec<T>`, its P-23) relies on that count as its only enforcement of an unpaired free, and nobody should "fix" a `Vec` that traps at exit; NOT (b) running `main`'s unwind at `exit`, which reopens the D-183 argument for the one path meant to have no failure modes -- unless the user wants drops with SEMANTICS (a buffered writer's flush, an `OwnedFd`'s close) to run at a normal exit, which is a different and larger question (`LineBufWriter` in `main` today loses unflushed bytes at `exit` exactly as it would at a crash; the kernel closes descriptors). Owner: the user (a decision); implementer: the `src/` writer, small. | 1.5.3 or the library era | `generic_move_out.npk`'s first shape; the workbench's libraries will meet it at their first `List` in `main` |
| ~~**S-40**~~ | **D-264** | **SETTLED (user, 2026-09-05: "the recommendation for the new decision sounds fine to me. lets ratify it"), LANDED at 1.5.2f step 1 (2026-09-05): a bare type parameter -- and `Self` in a trait's default body -- is MOVE-ONLY in the body that names it; a copy of a `T` place is `move(...)` or `.clone()`; the lending `pick` and the derive generator ask the same question; seven sites in the tree respelled.** ~~OPEN.~~ (The library workbench's O-N19, DEF-23.) Inside a GENERIC body the move-only rule (D-183, TYPE-046) was not asked of a bare type parameter: `require_move_if_owning` asked `type_drops`, false for an unsubstituted `T`, so `T:answer = s[i]` at an owning `T` compiled, linked and ran with two owners of one heap body (exit 170, the poison read); the same statement with `string` written out was refused. Present at every pin; 1.5.2d step 4 only made the consequence runnable. Measured on `f6dfce9` with the rule in place: the compiler, `npkg`, the tools, `tests/conformance`, `tests/frontend`, `tests/accept` and `tests/verify` refuse nothing new; five backend programs and `lib/ntensor.npk` carry seven sites, every one a by-value `T:v` parameter STORED into an owning slot. | before 1.5.3 | `nitpick-time/tests/probe/defect/generic_owning_copy/`, five cases across four pins |
| ~~**S-41**~~ | **D-266** | **SETTLED (user, 2026-09-06: "yes, ratify it as stated with the frozen-selector rule"), LANDED at 1.5.2h step 1 (2026-09-06: a lending `pick`'s bindings are read-only VIEWS in place with no address, TYPE-066; the selector frozen inside a binding arm, TYPE-067; the four derives over `string` and `T` payloads) (the recommendation as written, the frozen-selector rule stated in the decision, and one sharpening: a view has NO address, since the one pointer type carries no mutability and every address is a write path — asking the same question of TYPE-063 found DEF-24, the implicit receiver address).** ~~OPEN.~~ (Raised 2026-09-05 with D-264.) A BORROWING `pick` binding form. Under D-216 the only `pick` that may bind an owning payload is the consuming `pick (move(v))`; under D-264 a `T` payload counts as owning, so a generic enum with payloads cannot derive `Eq`, `Ord`, `PartialOrd` or `Clone` (DERIVE-006, as a `string` payload never could), and no program can compare two `Opt<string>`s without consuming one. The gap S-23's settlement named ("whether the language wants a BORROWING `pick` form … is a separate question") is concrete now. **Recommendation:** a lending `pick` binds its payloads AS BORROWS -- each binding a read-only view of the payload IN PLACE, under the borrow rules (no escape, no move out of it, no drop of it; the escape analysis treats it as `@` of the selector), spelled with the binding forms the language has -- so the derive generator's picks are sound as written, `Opt<string>.eq` is derivable, and the consuming form keeps its meaning. A language change: the checker's binding types (a view of `T`), the analyses' borrow treatment, the emitter's binding by address instead of by copy, and a decision on the spelling (D-004's second-class borrows are the frame). Owner: the user (a decision); implementer: a subcycle of its own, before 1.5.3 lowers contracts over enums or as its first step. | 1.5.3 | D-264's derive consequence; the workbench's containers will meet it at their first `Eq` over a payload enum |
| ~~**S-42**~~ | **D-265** | **SETTLED (user, 2026-09-06: "lets go with your recommendation on S-42 and ratify it"), LANDED at 1.5.2g (2026-09-06) (the recommendation as written; the z3 asymmetry stated in the decision: a solver's output is a committed VERDICT, a toolchain's is checked bytes).** Raised 2026-09-06 by the library workbench's first CI run. `nitpick-time`'s CI built the pinned compiler commit `aaffb87` on GitHub's runner and got a `build/npkc` of `3c05818c…` where the workbench's machine and the compiler tree both get `a3b0dadc…`; `build/npkrt.o` is `c9ddbcff…` on both. Nothing the compiler side claimed is contradicted: BUILD_REFERENCE §5 says the same inputs, THE TOOLS INCLUDED, give the same bytes, and D-204's pin is a VERSION (20.1.2, asked of the tools) -- a version is not a binary, and two builds of 20.1.2 (this machine's Ubuntu `1:20.1.2-0ubuntu1~24.04.3`, the runner's apt source) differ in distro patches and configure-time defaults. The `repro` stage measures one machine (working directory, `llc` twice, absolute site rows). What IS claimed across machines and never yet measured across two: the compiler's EMISSION, `build/npkc.ll` -- same source and snapshot, same text anywhere (D-078, D-236); if it differs, that is a compiler defect. The ladder leaves six files (`builder.o`, `builder`, `npkrt.o`, `npkc.ll`, `npkc.o`, `npkc`) and the first digest that differs names the stage; the workbench's CI records them per run. **Recommendation:** (a) the pin STAYS a version -- a tool-binary digest in `[toolchain]` would refuse every machine but one, hostile to the libraries' CI and to any user, for a property that belongs to the toolchain; (b) `npkg build` prints the six digests at its end and the compiler side publishes the emission's digest with every pin notice, the emission being the cross-machine claim; (c) the first cross-machine comparison of `npkc.ll` is the measurement to make (the workbench's runner against this machine), and only a difference THERE opens a compiler item. Owner: the user (a), the `npkg` writer (b, small), the workbench (c). | 1.5.3's open or the library era | the first library CI runs are the trigger; the digests are already being recorded |
| ~~**S-43**~~ | **D-267** | **SETTLED (user, 2026-09-06: "ratify both as recommended"), LANDED at 1.5.3 (2026-09-06: REACH-004 for a literal at step 0, the guard at step 1, the row at step 2; `failsafe_post.npk` exits 70).** ~~OPEN.~~ `failsafe-post` gets a RUNTIME GUARD. D-218's catalogue ratified the kind with guard `no`; D-014 §3.3 says a `failsafe` returning 0 is a contradiction, and measured on `fe42dba` a computed `exit 0i32` in `failsafe` compiles and ships (an empty body is REACH-001 already; a non-positive LITERAL becomes REACH-004 at 1.5.3 step 0). **Recommendation: guard YES** — at every `exit` in `failsafe`, `<code> > 0` or trap −4113 (`EnsuresViolated`, the compiler's own `ensures`), which the trap route's re-entry rule (VERIFICATION §4.6) turns into exit 70, uncatchable: a failed program never reports success to its operator whatever path computed the code; one compare per `exit` in one function. The alternative (`no`) leaves a computed `exit 0` reported as an open row and shipped. Owner: the user; the plan (`1.5/1.5.3.md` §4) proceeds under the recommendation. | 1.5.3 | D-014 §3.3; the catalogue's guard column |
| ~~**S-44**~~ | **D-268** | **SETTLED (user, 2026-09-06: "ratify both as recommended"), LANDED at 1.5.3 step 2 (2026-09-06); D-252 amended.** ~~OPEN.~~ A `requires` row at an `await` and at a `dyn` call, word `retained`; D-252 amended to record `limit-subsume` at an `await` the same way. D-252 recorded no `limit-subsume` row at a coroutine callee's call ("evidence of a narrowing no build can use"). A `requires` row there changes no build either, but it is the only place the program's correctness at that call is ever PROVEN: without it a program's `dyn` and `async` calls are never verified against their callees' preconditions, only trapped. **Recommendation: record them** (`retained`: the guard is the callee's entry check, never bypassed there), and amend D-252 so `limit-subsume` at an `await` is recorded the same way — one rule for the two kinds. The alternative keeps D-252's text and records `requires` rows only at direct sync calls. Owner: the user; the plan proceeds under the recommendation. | 1.5.3 | D-252's rationale; the catalogue's `requires` row |
| ~~**S-45**~~ | **D-269** | **SETTLED (user, 2026-09-07: "I'm good with your recommendations for those questions"), LANDED at 1.5.4 step 4 (2026-09-06): `npkc --elide` refuses an undischarged `prove` with `NITPICK-VERIFY-001`, the plain build lowers it to nothing; `prove_open.npk`.** An undischarged `prove` refuses the VERIFIED build, and the plain build lowers `prove` to nothing. VERIFICATION §1.2 and CONTROL §4.4 say a counterexample fails compilation; D-218.7 ratified `prove` with guard `no`; C-14/D-219 say an undischarged obligation retains its guard and a `prove` has none to retain, so a verified artifact with an unproven claim in it can only be refused. **Recommendation:** `npkc --elide` refuses an undischarged `prove` (`open`, `budget`, `unencoded`, or absent from the manifest) with `NITPICK-VERIFY-001` at the statement, `npkg verify` fails through it, and the plain build is untouched (no check, no trap — guard `no` as ratified). The alternative gives `prove` a runtime guard in every build (an amendment to D-218.7, a new trap identity, and a second spelling of `ensures`). Measured on `b2f7d94`: zero `prove` in `src/`, `lib/`, `npkg/`; seven test files mention it. Owner: the user; the plan (`1.5/1.5.4.md` step 4, L-15/L-16) proceeds under the recommendation. | 1.5.4 | VERIFICATION §1.2; D-218.7; D-219 |
| ~~**S-46**~~ | **D-270** | **SETTLED (user, 2026-09-07: "I'm good with your recommendations for those questions"), LANDED at 1.5.4 step 3 (2026-09-06): `loop-step` is kind 18, guard `yes`, trap `-4101`, in the catalogue table and both runners' trap tables; a literal step has neither compare nor row; `loop_step.npk`.** The counted loop's step guard becomes a catalogue kind, `loop-step`. D-022: a non-literal step is "a proof obligation, falling back to a runtime check that traps to `failsafe`"; the runtime check exists (`-4101`, `BadStep`, at every counted loop's entry) and no kind names it, so the manifest is not the inventory of guards P-12 says it is. **Recommendation:** add the kind — goal `step > 0`, guard `yes`, rows from 1.5.4, trap `-4101` — as an amendment to D-218.7's list, whose own rule is "every carried obligation appears or the manifest has holes"; a LITERAL step is the checker's (DEF-28, TYPE-068) and gets no guard and no row. The alternative leaves the guard row-less and states the hole in §7b. Measured: three counted loops in `src/`, all literal steps; `emit_counted` traps for every step today. Owner: the user; the plan (step 3, L-13) proceeds under the recommendation. | 1.5.4 | D-022; D-218.7; P-12 |
| ~~**S-47**~~ | **D-271** | **SETTLED (user, 2026-09-07: "I'm good with your recommendations for those questions"), LANDED at 1.5.4 step 4 (2026-09-06): `tests/rejection/` and its `[[test]]` entry are gone, `inline_mod.npk` is a positive program, D-085's rule re-homed to BUILD_REFERENCE §7.1.** The rung suite (`tests/rejection/`) retires when the last rung falls. After `prove` and `assert_static` lower, `ll_rung` has no caller and no construct in the language rungs (measured on `b2f7d94`: every remaining `iv_rung`/`pv_rung` is reached only through an `LlType` state nothing constructs). The suite's README says the directory shrinking measures subset 1 disappearing; this is that measurement's last step. **Recommendation:** retire the directory and its `[[test]]` entry; keep `inline_mod.npk` as a positive program in `tests/backend/programs/` whose hidden function is called (the audit's hole — declared code silently shed — caught by a wrong answer); keep `check_rung_names_open_cycle` for a rung a later cycle adds; re-home D-085's rule ("the parser never restricts, the backend does") to BUILD_REFERENCE §7.1, where the grammar sweep enforces the first half. The alternative keeps an empty suite both runners must special-case, asserting nothing. Owner: the user; the plan (step 4, L-22) proceeds under the recommendation. | 1.5.4 | D-085; `tests/rejection/README.md` |
| ~~**S-48**~~ | **D-272** | **SETTLED (user, 2026-09-07: "I'm good with your recommendations for those questions"), LANDED at 1.5.4 step 4 (2026-09-06): the proposition is a hypothesis after its row; `prove_lemma.npk` discharges a division through it.** A discharged `prove` is knowledge after its site (a lemma). VERIFICATION §1.2: the solver "constructs a mathematical proof that the expression holds"; a proven proposition is a fact of every execution that passes the statement. **Recommendation:** the encoder pushes the proposition as a hypothesis after its row (L-7's order), so `prove` states an intermediate fact later obligations use — a proof outline in source. Sound because a verified build refuses an undischarged `prove` (S-45) and a plain build elides nothing: a later row discharged through an unproven claim ships in no artifact. The alternative keeps `prove` a documentation-only check whose fact the encoder forgets one line later. Owner: the user; the plan (step 4, L-15) proceeds under the recommendation. | 1.5.4 | VERIFICATION §1.2 |
| ~~**S-49**~~ | **D-274** | **SETTLED (user, 2026-09-10: "yes ratify"), lands at 1.5.4d (the successor plans it ahead of 1.5.4b).** (1.5.4c step 0, 2026-09-09; found landing D-273.)** A member-less `mod:name;` written after a file's header loads `name.npk` (MODULE_REFERENCE §1.1's external-file module) and collection binds `name` as a MODULE SYMBOL whose scope is EMPTY — `collect_module` opens one over its zero members — so under D-273 `name.f()` reports "module `name` has no member `f`" (TYPE-019) where it reported "`name` is a module, not a value" (TYPE-007) before, and neither reaches the loaded file. D-273 §4 gives an ALIAS the scope of the file it names; the import form names a file the same way and carries nothing. **Recommendation:** the file-module import's symbol carries the loaded file's scope too — set where the alias's is, from the module the loader found — so `mod:util;` then `util.f()` means what `use "./util.npk" as util;` means; one field, one lookup, no third kind of module symbol. Measured on `f071d43`: no program in the tree calls through the form (`entry_in_module.npk` uses it to load a sibling; `whole_grammar.npk` parses it). Owner: the user; lands with 1.5.4c step 1 if ratified. | 1.5.4c | D-273 §4; MODULE_REFERENCE §1.1 |
| ~~**S-50**~~ | **D-275** | **SETTLED (user, 2026-09-10: "ratify"), lands at 1.5.4d (the successor plans it ahead of 1.5.4b).** (1.5.4c step 0, 2026-09-09; found writing `mod_qualified.npk`.)** An error constant declared INSIDE an inline module hashes its identity under the FILE's name — `error_code_of` takes the resolver's `module_name`, the file's basename (D-179) — so `hidden`'s `Boom` in `mod_qualified.npk` is `mod_qualified.Boom`, its `failsafe` arm is spelled `(mod_qualified.Boom)` and the reach analysis reports it so; a `(hidden.Boom)` arm would hash a name no constant has and match nothing, silently. D-273 made `?! hidden.Boom` a value of that constant (the arm the reach analysis demands is the file-qualified one). **Recommendation:** an inline module does NOT qualify the identities it declares — the file does, one namespace per D-179's "module.Name" — and the qualified arm's first segment is checked against the module names the program knows (a segment naming no module is RESOLVE-002 at the arm, never a silent non-match), so the mistake is refused rather than matched against nothing. Measured: no error constant is declared inside an inline module anywhere in the tree or the library workbench. Owner: the user. | 1.5.4c | D-179; D-273 |
| ~~**S-51**~~ | **D-276** | **SETTLED (user, 2026-09-10: "all of those things sound fine to me. please proceed."), LANDED at 1.5.4d step 2 (2026-09-10): the loader, the import pass and the file-module link descend into inline modules — `inline_imports.npk`, `inline_import_unknown.npk`.** ~~OPEN (the user; raised by 1.5.4d's planning, 2026-09-10, on `1ef034a`).~~ A `use` written INSIDE an inline module binds nothing and a `mod:name;` written inside one loads nothing — both silent until a later name fails (`use "./plib.npk".*;` inside `mod:m = { … }` then `f()` in `m` is RESOLVE-002 "cannot find `f`"; `mod:plib;` inside `m` then `plib.f()` is TYPE-019 "no member"): `graph_load_imports` and `module_apply_imports` walk a FILE's top-level items only. It matters because an inline module is sealed from its own file's imports (`scope_lookup` stops at the first module scope and the file's `use` bindings live in the file scope — measured: a file-level `use "./plib.npk".*;` does not reach `m`'s functions), so today an inline module can reach nothing but the prelude and its own members, and the one construct that could change that is a silent no-op. **Recommendation:** an import means the same thing wherever it stands (the blueprint rule): the loader, the import pass and the file-module link descend into inline modules, binding into the inline module's scope under the same rounds and refusals as at file level (a file-module import loads relative to the FILE). The alternative — refusing an import inside an inline module by name — is smaller and keeps the box sealed. Either is a small change; the silent no-op is the only wrong state. Lands as 1.5.4d step 0b if ratified (`meta/roadmap/done/1.5/1.5.4d.md` §2 item 3). | 1.5.4d | D-273; MODULE_REFERENCE §1.1, §2 |
| ~~**S-52**~~ | **D-277** | **SETTLED (user, 2026-09-10: "ratify as recommended"), LANDED at 1.5.4b step 0 (2026-09-10).** ~~OPEN (the user; raised by 1.5.4b's planning, 2026-09-10, on `12a6a78`)~~ — what a shift by an amount outside `0..width-1` means.** Measured 2026-09-10 on `12a6a78`: `x << n` with `n = 40` on an `int32` passes the checker silently and the emitter writes a bare `shl i32` — POISON in LLVM for any amount at or past the width; the run exited 0 by coincidence (x86 masks the count). No emitted guard, no row, no reach arming; the constant folder refuses a distance `< 0 || >= 64` for every width (an `int32` by 40 folds to `2^40`); the specification says nothing about the amount (OP_REFERENCE §4; D-210 leaves shifts "unchanged"). Options: TRAP (`ShiftRange`, `-4115`; a KNOWN amount — a literal or a folded constant — refused at compile time as `NITPICK-TYPE-070`, a computed one guarded by one unsigned compare `n <u W` that covers a negative amount too, a `shift-range` row elided into `llvm.assume` when discharged — the language's posture for division, overflow, bounds and the counted loop's step), MASK (`n mod width`, x86's habit — silently a different shift than the author wrote), SATURATE (`0`, or `-1` for a signed right shift — the mathematical limit, silently a different shift). **Recommendation: TRAP**, the folder's bound the type's width, every evaluator agreeing (DEF-29's lesson). Lands as 1.5.4b step 0 (`meta/roadmap/done/1.5/1.5.4b.md` §2). | 1.5.4b | D-218 (4), (5), (7); `meta/roadmap/done/1.5/1.5.4b.md` |
| ~~**S-53**~~ | **D-278** | **SETTLED (user, 2026-09-10: "ratify as recommended"), LANDED at 1.5.4b step 2 (2026-09-10).** ~~OPEN (the user; raised by 1.5.4b's planning, 2026-09-10, on `12a6a78`)~~ — the ERR-sentinel rows for the twisted kinds.** A new obligation kind per twisted operation, or ERR as a VALUE the terms carry with the existing guards' rows? Counted (1.5.4b §1): the only twisted-kind trap is `-4100` (`TbbErr`), at a comparison on a twisted operand and at a cast out of the family under either spelling (D-144 as amended); a twisted division never traps (a zero divisor is ERR); the prelude's `tbb` has no arithmetic at all, `tfp` 91 arithmetic sites and 70 compares, `dim256` 725 generated compares. **Recommendation: no new kind** — `err-exit` (kind 14, in the catalogue since 1.5.0 as 1.5.8's) is the row at every `-4100` guard, ERR is `MIN(width)` as a distinguished value in every twisted term (the emitter's exact `ite` shapes: saturate-to-ERR, the `tfp` floor multiply and truncating divide, the round-trip narrow), a twisted division records no row, and both runners' trap table stays a function of the kind. Lands as step 2. | 1.5.4b | D-218 (4), (5), (7); `meta/roadmap/done/1.5/1.5.4b.md` |
| ~~**S-54**~~ | **D-279** | **SETTLED (user, 2026-09-10: "ratify as recommended"), LANDED at 1.5.4b step 1 (2026-09-10).** ~~OPEN (the user; raised by 1.5.4b's planning, 2026-09-10, on `12a6a78`)~~ — which bitwise operations cross into QF_BV.** All five (`& | ^ << >>`) and `~`, plus the flag families' `|`/`&`/`~` (D-230) — with pure-Int forms FIRST wherever an operand is a numeral: a shift by a literal is `(mod (* x 2^k) 2^W)` with the signed wrap, a right shift `(div x 2^k)`, a low-bits mask `(mod x 2^j)`, a single-bit mask `(* (mod (div x 2^j) 2) 2^j)`, `~x` is `(- (- x) 1)`; measured at 504–1,287 rlimit regardless of width (2048 included), and they are the shapes the wide Dragon4 and `tfp_render` bodies write. A crossing the encoder does not take is an `open` row an author cannot close; the Int forms are not a crossing at all. **Recommendation: yes, as written.** Lands as step 1. | 1.5.4b | D-218 (4), (5), (7); `meta/roadmap/done/1.5/1.5.4b.md` |
| ~~**S-55**~~ | **D-280** | **SETTLED (user, 2026-09-10: "ratify as recommended"), LANDED at 1.5.4b step 1 (2026-09-10).** ~~OPEN (the user; raised by 1.5.4b's planning, 2026-09-10, on `12a6a78`)~~ — the crossing's width.** Measured 2026-09-10 (1.5.4b §1's table): the Int-to-bit-vector crossing (`int2bv`/`bv2nat` under `(set-logic ALL)`) on a masked-divisor row costs 191 rlimit at 32 and 64 bits, 883,930 at 128 (4% of the budget), 3,591,364 at 256 (18%), and EXHAUSTS the budget at 2048 (`unknown`, 21.9 s) — s3's "the exact width is affordable" (§4b) held for the bit-vector theory alone; a bit test with an OPAQUE mask read by an Int inequality exhausts the budget even at 32 bits (`budget`, safe, unproven). **Recommendation: `BV_CROSS_MAX_BITS = 64`** as one named constant with the measurements beside it — general operations wider than that stay opaque (as today), the Int forms of S-54 cover the wide widths' real shapes — and the gate that no `discharged` row of `nitpick.obligations` regresses when the terms enter the cones. Moving the constant later is a measurement, not a decision. Lands as step 1. | 1.5.4b | D-218 (4), (5), (7); `meta/roadmap/done/1.5/1.5.4b.md` |
| ~~**S-56**~~ | **D-281** | **SETTLED (user, 2026-09-10: "ratify as recommended"), LANDED at 1.5.4b step 3 (2026-09-10).** ~~OPEN (the user; raised by 1.5.4b's planning, 2026-09-10, on `12a6a78`)~~ — floats: tier 1's terms, the tier column, and tier 2's soundness conditions.** D-218 (5) is ratified ("floats two-tier — Z3 QF_FP for tier-1 obligations, Real-interval abstraction for heavy non-linear, the manifest recording which tier discharged what, undischarged = retained runtime guard"); this asks whether the DESIGN 1.5.4b §2 step 3 writes is the one. Tier 1: `flt32`/`flt64` as `(_ FloatingPoint 8 24)`/`(11 53)`, literals as their exact IEEE bit patterns from the emitter's own folded bits, `+ - * / #sqrt` under RNE, the comparisons with the emitter's own `fcmp` NaN behaviour per operator, `frem` and every cast OUT of a float opaque until 1.5.8's `cast-range` rows; no new row kind (floats never trap) — what tier 1 buys is that a `limit`, a contract clause, an `invariant` or a `prove` over floats is an encoded row (today `unencoded`/`open`); measured 4.35M rlimit (0.77 s) for a bounded-quotient row beside an Int hypothesis. The manifest's tier column (P-10, today the constant `int`) fed from the encoder: `int`/`bv`/`fp`/`-`, and `real` written by the runner for a tier-2 discharge. Tier 2: for every row whose cone holds a float, a second query in which every float term is a Real with the standard rounding model `|r - v| <= eps*|v| + eta` per operation (`sqrt` included), asked by the runner only when tier 1 answers `unknown`, under THREE soundness conditions or not at all — (i) every float symbol in the cone bounded below and above by float-comparison hypotheses against numerals (which exclude NaN and, with both bounds, infinities), (ii) every intermediate's magnitude proven within the normal range by conjoining `|v| <= MAX_NORMAL` to the goal, (iii) the goal a comparison or a Boolean combination of comparisons; measured 4,252 rlimit for the sqrt shape that exhausts tier 1's budget. **Recommendation: yes.** The alternative — tier 2 as a later subcycle — leaves D-218 (5) half-landed with no float row in the tree to force the question. Lands as step 3. | 1.5.4b | D-218 (4), (5), (7); `meta/roadmap/done/1.5/1.5.4b.md` |
| ~~**S-57**~~ | **D-282** | **SETTLED (user, 2026-09-10: "ratify as recommended"), LANDED at 1.5.4b step 4 (2026-09-10).** ~~OPEN (the user; raised by 1.5.4b's planning, 2026-09-10, on `12a6a78`)~~ — `simd` lanes.** Per-lane terms (N scalar terms per `simd<T, N>` expression, N <= 16 by D-194's cap; the constructor, splat, elementwise ops and compares, a numeral-index lane, `.any()`, `.len`, the ordered reductions) and ONE `div-zero` row per `simd` division over the lane conjunction (one `div-min` for a signed element), which retires `record_unencoded_division` — the last `unencoded` producer but a `limit` over a subject no theory covers (a struct), which stays and is named. **Recommendation: yes.** Keeping `unencoded` saves nothing and leaves P-12 open. Lands as step 4. | 1.5.4b | D-218 (4), (5), (7); `meta/roadmap/done/1.5/1.5.4b.md` |
| ~~**S-58**~~ | **D-283** | **SETTLED (user, 2026-09-10: "I am fine with the three recommendatins you made for the decisions"): the reading confirmed as D-283; NOTED on D-282 at 1.5.4e step 2 (2026-09-11).** ~~OPEN (the user; raised by 1.5.4b step 4, 2026-09-10, on `044a04d`)~~ — a reading of D-282 to confirm. D-282's text gives a `simd` EXPRESSION its lanes (the constructor, a splat, the elementwise forms, a numeral-index lane, the reductions) and says "anything else N opaque lanes". As landed, a `simd` BINDING has lanes too: N symbols of the element type, a new set at every write, defined equal to the written value's lanes where those are known, fresh and opaque at every invalidation, restore and merge — because every real `simd` value lives in a local, and the decision's own tests (`a / simd(k)` under a `limit` on `k`; a lane read by `[2]` as a divisor) are unprovable when the read of the local is opaque. Sound by construction: a lane is defined only from a value the walk saw written, and opaque wherever a path could differ. **Recommendation: confirm the reading** (a `simd` local is a binding like any scalar, its lanes its versions); the alternative — expressions only — keeps every `simd` row over a local `open`. | 1.5.4b | D-282; `meta/roadmap/done/1.5/1.5.4b.md` step 4's record |
| ~~**S-59**~~ | **D-284** | **SETTLED (user, 2026-09-10: "I am fine with the three recommendatins you made for the decisions"), LANDED at 1.5.4e step 0 (2026-09-11).** ~~OPEN (the user; found by 1.5.4b step 4b, 2026-09-10, on `044a04d`; DEF-38 in §2f)~~ — do `simd` integer lanes carry D-210? Measured: a `simd<int32, 4>` `+`, `-` or `*` lowers to a bare vector `add`/`sub`/`mul` and WRAPS — a lane holding `INT_MAX` plus a lane holding one read back negative (exit 5) — where a scalar `int32` traps `IntOverflow`, and the reach analysis arms nothing for it. D-194 (1.3.1) wrote the division guard any-lane and said nothing of overflow; D-210 (1.4.2b) named plain integers; TYPE_REFERENCE §14 says "elementwise on identical types". So the same `+` traps on an `int32` and wraps on an `int32` lane — the blueprint rule broken by a family, and D-210's Therac shape open in it. **Recommendation: yes — D-210 per lane**: the vector overflow intrinsics (`llvm.sadd.with.overflow.<N x iW>` and its siblings, legal at every lane width), one any-lane test, `IntOverflow` trapped and armed by the reach analysis for an integer-lane `+ - *` (its comment already claims the family "rides its element's rule"), a `simd_ovf_trap.npk` program, and the encoder's `overflow` rows over the lane conjunction when 1.5.8 lands them (D-282's shape). No `tbb`-lane vector exists (D-194's elements), so no saturating variant is needed. The alternative — wrap by decision — needs a D-210 amendment naming the family and a reason it should differ from its scalar. | 1.5.8 (the overflow rows), or before as a small step | D-194, D-210, D-282; DEF-38 |
| ~~**S-60**~~ | **D-286** | **SETTLED (user, 2026-09-11: "s60 to s-62 look fine to me … consider them ratified"), LANDED at 1.5.5 step 1 (2026-09-11).** ~~OPEN (the user; raised by 1.5.5's planning, 2026-09-11, on `cb8cbb0`; the plan is `meta/roadmap/done/1.5/1.5.5.md` §2.1–2.4, §2.9)** — the aliasing half of D-004, never settled (0.5.1: "a different question … not settled here"). Measured: `$$i`/`$$m` are one node the checker types as `T->`, the bindings analysis reads `$$m` as the write that fills a destination (D-118) and nothing else distinguishes them — two `$$m` of one element, or a `$$m x` held while `x` is assigned and then written through the pointer, compile and run (exit 32); the spec's own example `$$m int32:a = arr[i];` does not parse (the qualifier forms never existed; the operator form is the language's). **Recommendation: ratify the plan's rules** — `$$m` EXCLUSIVE, `$$i` SHARED (many readers), `@` a plain address that claims nothing but counts as a write-capable ACCESS (the pointer type carries no mutability); lifetimes LEXICAL — a call argument for its call, a pointer local's whole value for the holder's whole scope (non-lexical lifetimes and two-phase borrows decided OUT: D-004's "no lifetime inference", and the prototype's `LivenessAnalyzer` is the machinery it declined); a claim stands only as a whole call argument or a pointer local's whole value, a holder is used and not copied, every other position refuses by name (`BORROW-014`, "spell `@` for an address that claims nothing"); a conflict — an access of an overlapping path under a live claim, per the plan's table — with statically overlapping paths is `BORROW-013`, with computed indices the guard of S-61; a claim on storage a plain-address holder reaches in scope is `BORROW-013`; exclusivity is decided among accesses that spell the SAME ROOT, stated as the limit; `borrow_imm`/`borrow_mut` struck from AST_REFERENCE. The tree's cost: five sites (four in `src/`, one acceptance case), one of them a real conflict (`diaglist_sort` moves an element out from under a live `$$i` of it). | 1.5.5 step 1 | 1.5.5 planning |
| ~~**S-61**~~ | **D-286** | **SETTLED (user, 2026-09-11: "s60 to s-62 look fine to me … consider them ratified"), LANDED at 1.5.5 step 2 (2026-09-11).** ~~OPEN (the user; raised by 1.5.5's planning, 2026-09-11, on `cb8cbb0`; the plan's §2.5–2.6)** — the shape of the computed-index check, and the new obligation kind. The prototype's model: the plain build REFUSES two `$$m arr[i]`/`arr[j]` and only `--verify` accepts them when z3 proves `i != j` (its NITPICK-023; the 1.5 README's row "the conservative refusal Z3 then relaxes" carries that reading). **Recommendation: a runtime guard in EVERY build that the verified build elides** — D-068's shape, the `limit`/`shift-range` precedent: at an access whose path is computed against a live claim, ONE byte-range compare per party (the held pointer's storage against the access's: `p < q+size_q && q < p+size_p` on `ptr` operands, exact for elements, rows and fields, no captured index, no frozen name, no frame slot) trapping `BorrowOverlap` (−4116, a new prelude identity the reach analysis arms); the row `disjoint` (kind 20, guard `yes`, `-4116` in both runners' trap tables, not an assume kind — its discharge removes the compare as `loop-step`'s does) whose goal is the inequality of the index pairs along the paths' common prefix, the party's index terms captured when its claim was encoded (versions are never reused, so the capture is exact under later assignments); one row per site. D-068's three reasons apply verbatim (a safety property never depends on a flag; a reader of `$$m arr[i]` could not tell whether the program compiles without knowing the build); program VALIDITY has never depended on the flag anywhere (a `prove` is the reverse direction); an `open` row keeps its guard as an `open` `limit` does. The alternative is the prototype's model, recorded above. | 1.5.5 step 2 | 1.5.5 planning |
| ~~**S-62**~~ | **D-287** | **SETTLED (user, 2026-09-11: "s60 to s-62 look fine to me … consider them ratified"), LANDED at 1.5.5 step 1 (2026-09-11).** ~~OPEN (the user; raised by 1.5.5's planning, 2026-09-11, on `cb8cbb0`; DEF-43 in §2f)** — a `fixed` binding has no address. Measured: `fixed int32:x = 1i32; int32->:p = $$m x; <-p = 2i32;` passes the checker and every analysis (ASSIGN-002 refuses a second ASSIGNMENT and sees no store through a pointer) and silently overwrites a `fixed` local; the same through `@G` of a `fixed` GLOBAL — an LLVM `constant` in read-only memory since D-211 — kills the process with SIGSEGV (exit −11): an UNCONTROLLED crash, `failsafe` never runs. 89 `fixed` bindings across `src/`, `lib/`, `npkg/`, `tools/` and `tests/`; none address-taken, none the receiver of a pointer-receiver call. **Recommendation: `@`, `$$i`, `$$m` and the implicit pointer-receiver address of a place rooted at a `fixed` binding — local, parameter, global — refuse at the checker, `NITPICK-TYPE-071`, beside TYPE-063 (the same reasoning: the pointer type carries no mutability, so an address of an immutable is a write path no rule sees; pass it by value or declare it plain).** The alternative, a read-only pointer type, is one the language decided against; tracking every pointer to a `fixed` through every call is the analysis S-60 declines for `@`. | 1.5.5 step 1 | 1.5.5 planning |
| ~~**S-63**~~ | **D-288** | **SETTLED (user, 2026-09-11: "lets ratify the recommendations for the 8 questions and you execute the steps for this session"); LANDED at 1.5.6 step 3 (2026-09-11).** ~~OPEN (the user; raised by 1.5.6's planning, 2026-09-11, on `149dbf6`; the plan is `meta/roadmap/done/1.5/1.5.6.md` §2.2, §3)** — where the floor's evidence lives: `runtime/npkrt.spec` (contracts, invariants, bounds, boundary promises, the shared-state classification), `runtime/models/*.model` and `runtime/npkrt.obligations` beside the floor, the manifest in the `nitpick.obligations v1` format written only by `npkg verify --record` and held by both runners; the kinds `floor-spec`/`floor-model` in the one catalogue; the translator in `npkg/` writing the compiler's directory shapes so both runners' z3 loops decide the floor unchanged; `open` a run failure in the floor (nothing elides). **Recommendation: ratify as written.** Alternative costed: floor rows in `nitpick.obligations` — a second producer in one file and an exemption in every belt that assumes an emitted function.~~ | 1.5.6 | 1.5.6 planning |
| ~~**S-64**~~ | **D-288** | **SETTLED (user, 2026-09-11: "lets ratify the recommendations for the 8 questions and you execute the steps for this session"); LANDED at 1.5.6 step 3 (2026-09-11).** ~~OPEN (the user; raised by 1.5.6's planning, 2026-09-11, on `149dbf6`; the plan is `meta/roadmap/done/1.5/1.5.6.md` §2.3, §3)** — the floor's theory: D-218 (4)'s partition exactly as programs' (Int with range axioms, D-279's Int forms plus four floor idioms named and stated sound, the ≤ 64-bit crossing, floats in two tiers), memory a byte-addressed uninterpreted function with explicit store chains, no array sort. Measured: QF_BV cannot close the 128-bit division's identity at any width tried (a multiplier equivalence); QF_ABV's `memcpy` at 16 bytes needs 20× the budget; the Int partition decides the division's inductive step at rlimit 3,093 and `memcpy` at 64 bytes at 166,810. **Recommendation: ratify.**~~ | 1.5.6 | 1.5.6 planning |
| ~~**S-65**~~ | **D-288** | **SETTLED (user, 2026-09-11: "lets ratify the recommendations for the 8 questions and you execute the steps for this session"); LANDED at 1.5.6 step 3 (2026-09-11).** ~~OPEN (the user; raised by 1.5.6's planning, 2026-09-11, on `149dbf6`; the plan is `meta/roadmap/done/1.5/1.5.6.md` §2.4, §3)** — loops and the residue: an invariant when the spec supplies one (quantifier-free, universals instantiated at the goal's free symbols — the single-instance rule, stated with its incompleteness and the `(inst e)` escape), unwinding to a stated `N` otherwise with `[≤N]` in the row; no ghost variables, no per-row budget; the residue by name (`npk_udivmod128`'s multiplicative identity, `fmod`'s loop exit value, `npk_wild_live_count`'s sum), each with its reason in TCB.md §5. **Recommendation: ratify.** Alternative costed: a larger budget for floor rows — declined; the 100× run bought nothing for the multiplier and the array case was an encoding.~~ | 1.5.6 | 1.5.6 planning |
| ~~**S-66**~~ | **D-289** | **SETTLED (user, 2026-09-11: "lets ratify the recommendations for the 8 questions and you execute the steps for this session"); LANDED at 1.5.6 step 5 (2026-09-12).** ~~OPEN (the user; raised by 1.5.6's planning, 2026-09-11, on `149dbf6`; the plan is `meta/roadmap/done/1.5/1.5.6.md` §2.7, §3)** — the primitive models: six bounded transition systems (`park-unpark`, `futex-mutex`, `channel-table`, `shared-arena`, `driver-registry`, `trap-route`) in SMT-LIB2 through the pinned z3 under the one profile, sequentially consistent over the atomics with the shared-state classification as the ordering evidence, depth `K` and preemption bound `D` recorded per row (BPOR's bound, symbolically), the r6 taxonomy's classes as the bad predicates, NEGATIVE CONTROLS required `sat`, a correspondence belt over block labels; liveness residue by name; 1.5.7's harness the instrument for the real code. Measured: the park/unpark model decides `unsat` at K 14/D 6 in 0.76 s and its no-re-check control `sat` in 0.28 s. **Recommendation: ratify.** Alternative costed: TLA+/TLC — a second engine against D-218 (1), a second profile, a JVM on the workbench; declined.~~ | 1.5.6 | 1.5.6 planning |
| ~~**S-67**~~ | **D-290** | **SETTLED (user, 2026-09-11: "lets ratify the recommendations for the 8 questions and you execute the steps for this session"); LANDED at 1.5.6 step 0 (2026-09-11).** ~~OPEN (the user; raised by 1.5.6's planning, 2026-09-11, on `149dbf6`; the plan is `meta/roadmap/done/1.5/1.5.6.md` §2.9, §3)** — the shared-state rule: every word two threads can reach is atomic or ordered by a NAMED happens-before edge, classified per word in the spec file's `(shared …)` section, held by a belt in both runners (an unclassified access, or a plain access of an `atomic` word, fails by name; a `lock` word's accesses must be dominated by the lock); the four promotions of step 0 (DEF-44, DEF-45, DEF-46, DEF-48) are the rule's first application, and a plain access that is by design (`owner-only`, `born-before-publish`, `once-before-threads`) is classified and stated in TCB.md §5, never silently promoted. **Recommendation: ratify; step 0 runs before the rest.**~~ | 1.5.6 | 1.5.6 planning |
| ~~**S-68**~~ | **D-288** | **SETTLED (user, 2026-09-11: "lets ratify the recommendations for the 8 questions and you execute the steps for this session"); LANDED at 1.5.6 step 6 (2026-09-12).** ~~OPEN (the user; raised by 1.5.6's planning, 2026-09-11, on `149dbf6`; the plan is `meta/roadmap/done/1.5/1.5.6.md` §2.10, §3)** — TCB.md's disposition column and its `floor-syscalls` and `floor-residue` regions GENERATED from the spec file, the floor manifest and the floor's text, held by `check_tcb_floor_current`'s extension in both runners; §5 finalized with seven acceptances (the kernel-effect table, the per-thread constants `npk_exec`/`npk_tls_self`, the SC modelling assumption with the shared-state classification, the bounds of bounded rows, the two opaque calls, the failsafe region's size, the stop's deadline). **Recommendation: ratify.**~~ | 1.5.6 | 1.5.6 planning |
| ~~**S-69**~~ | **D-291** | **SETTLED (user, 2026-09-11: "lets ratify the recommendations for the 8 questions and you execute the steps for this session"); LANDED at 1.5.6 step 1 (2026-09-11).** ~~OPEN (the user; raised by 1.5.6's planning, 2026-09-11, on `149dbf6`; the plan is `meta/roadmap/done/1.5/1.5.6.md` §2.9, §3; DEF-47 in §2f)** — the trap route's whole-program stop. D-063 says other threads stop BEFORE `failsafe` gets control; the floor sets `@npk_frozen` (which stops the next RESUME on every executor) and nothing else, and its `@npk_in_failsafe` is a plain load-then-store: two threads trapping at once run two `failsafe`s, a thread trapping while another's `failsafe` runs takes the re-entry arm and `exit_group(70)`s the process mid-safing, and a task running on another thread at the trap keeps running to its next suspension point. The design: a preallocated thread registry (64 slots, CAS-claimed, the driver registry's shape), a stop signal (SIGUSR1; `rt_sigaction` at start with `SA_RESTORER` and a two-instruction `rt_sigreturn` stub; a handler that counts itself and parks forever), `cmpxchg` arbitration of the failsafe holder (a re-entering holder exits 70 as today; any other loser parks), the winner signalling every other live thread and waiting for the count under the join deadline BEFORE the drivers and `failsafe`; `npk_exit` from a non-holder parks. Three syscalls join the floor (`rt_sigaction`, `tgkill`, `getpid`); the `trap-route` model is built on today's floor first (its bad predicates `sat`, the negative control) and then on the new (`unsat`). **Recommendation: build it in this subcycle (step 1).** Alternative costed: record the gap as residue — declined; this is the actuator scenario the language exists for.~~ | 1.5.6 | 1.5.6 planning |
| ~~**S-70**~~ | **D-292** | **SETTLED (user, 2026-09-11: "lets ratify the recommendations for the 8 questions and you execute the steps for this session"); LANDED at 1.5.6 step 2 (2026-09-11).** ~~OPEN (the user; raised by 1.5.6's planning, 2026-09-11, on `149dbf6`; the plan is `meta/roadmap/done/1.5/1.5.6.md` §2.9, §3)** — the trap route's allocator. D-014 and the heap's header say the trap path allocates nothing and `failsafe` runs on preallocated state; today a `failsafe` body that allocates takes the same heap mutex the program was using — a thread that trapped inside the allocator, or (after S-69) a thread stopped mid-allocation, holds it forever and the `failsafe` hangs with no deadline. The design: a 1 MiB `.bss` failsafe region that `npk_alloc_impl` bumps from once a failsafe holder exists (16-aligned, zeroed by construction, never freed), frees and shrinks no-ops during `failsafe`, exhaustion the re-entry exit 70, the size stated in TCB.md §5 as the bound a `failsafe` body lives within. **Recommendation: build it (step 2).** Alternative costed: a checker rule refusing allocation reachable from `failsafe` — declined (it refuses diagnostics and the library tier's `failsafe` idioms; the region makes the documents' claim true instead of forbidding what a `failsafe` is for).~~ | 1.5.6 | 1.5.6 planning |
| ~~**S-71**~~ | **D-218 (amended)** | **SETTLED (user, 2026-09-17: "ratify it, and let s7 make the 1.5.8 corrections" — said to the outgoing seat `nitpick-compiler_s6` and relayed at the s6→s7 hand-off); LANDED at 1.5.6 step 4 (2026-09-11) under its recommendation.** ~~OPEN (the user; raised by 1.5.6 step 4, 2026-09-11; LANDED under its recommendation in the same step; the user can decline it without a verdict moving).~~ The determinism profile gains `lp.dio=false` — z3 4.16.0's Diophantine-equation sub-solver off. Found deciding `npk_hs_put_dec`'s per-length rows: the fourteen-digit row answers `unsat` in 8 s and the process returns 200 s later, the eighteen-digit row had not returned after 22 minutes, and `(exit)` behaves the same; gdb at the slow tail shows `lp::dioph_eq::imp::undo_add_term_method` → `shrink_matrices` → `eliminate_last_term_column` under `smt::theory_lra::pop_scope_eh` — the sub-solver undoing its terms at the row's `(pop)` by a big-rational matrix elimination. Under P-13 a solver that does not return is a build failure and neither runner carries a clock to cut it. With the sub-solver off the pop is instant and NO VERDICT MOVES: the compiler's 411 encoded rows (368 manifest rows) and the floor's 350 rows give the same answer row for row with it on and off, measured on the step's final emission (the floor's set decides in 325 s against 587 s). The non-returning pops were measured on the step's earlier encoding — the flat `(div v 10^k)` quotients, one rendering change away from the final one — so the amendment is robustness and speed, not a condition of the rows deciding today; `nitpick.obligations` is re-recorded for its header line only. D-218 (2) names fixed seeds, no wall clock and `rlimit` the sole budget, all of which hold; the option is a solver strategy, deterministic, in the manifest like the rest. **Recommendation: ratify as a D-218 (2) amendment.** Alternatives costed: one z3 process per row with no `push` (the same row then answers `unknown` — z3's non-incremental preprocessing differs, so every verdict would have to be re-baselined); a wall clock (refused by D-218 (2) by name). | 1.5.6 | 1.5.6 step 4 |
| ~~**S-72**~~ | **D-293** | **SETTLED (user, 2026-09-17: "the recommendations you had for those questions looks fine to me"); LANDED at 1.5.6b step 3 (2026-09-17): the row, the generated tables, the hand-written `declare` gone, `hwconc_builtin.npk` (exit 0; exit 20 on the floor as it stood, at its first call).** ~~OPEN (raised by 1.5.6b's planning, 2026-09-17, on `b7d60dc`)~~ — finish D-181 §4's promise, or decide it out? D-181 (SETTLED) says "`hardware_concurrency` is `sched_getaffinity`"; the floor implements `@npk_hardware_concurrency` and `emit_program.npk` declares it in every module, but NO builtin row reaches it, so no program can call it -- which is exactly why DEF-52 lived there (nothing ever executed the symbol). **Recommendation (ratified): add the builtin** -- `hardware_concurrency() → int64`, never fails, `effect`; a pool that cannot ask sizes itself by a literal, which D-073's own table names as the prototype's defect. Alternative costed: decide it out and delete the floor symbol (a smaller TCB) -- declined, the library tier's pools need it. | 1.5.6b | 1.5.6b planning |
| ~~**S-73**~~ | **D-294** | **SETTLED (user, 2026-09-17: "the recommendations you had for those questions looks fine to me"); LANDED at 1.5.6b step 4 (2026-09-17): `owned_builtin_name` in the loader's owned-names walk, `owned_builtin_names.npk` (six refusals, six accepted shapes); an `extern` METHOD of a builtin's name is refused too — its generated stub is a module-level function (D-190) — reported once, at the method.** ~~OPEN (raised by 1.5.6b's planning, 2026-09-17, on `b7d60dc`)~~ — may a program declare a function with a builtin's name? Today it may: a builtin is a resolver FALLBACK (`resolve.npk` looks a name up in scope first), and a program's own `mono_now` silently wins over the clock -- measured, `raw mono_now()` answers the program's 7, and a `read_file` of another signature and contract is typed and called by its binding (exit 42). Consistent, not a defect; D-239 refuses the same for types. **Recommendation (ratified): refuse it, `NITPICK-RESOLVE-001` at a module-level `func:` whose name is a builtin's; methods exempt (a per-type namespace).** Cost today: zero sites over the table's 65 names in the compiler tree and in `nitpick-libs`. It IS the library listener's class (a new refusal), and from it on every ADDED builtin reserves a name. Alternative costed: document the fallback -- declined, a default that varies by a circumstance invisible at the call. | 1.5.6b | 1.5.6b planning |
| ~~**S-74**~~ | **D-288 (amended)** | **SETTLED (user, 2026-09-17: "the recommendations you had for those questions looks fine to me"); lands at 1.5.6b step 1.** ~~OPEN (raised by 1.5.6b's planning, 2026-09-17, on `b7d60dc`)~~ — TCB.md §5's eighth acceptance asks a reader to ACCEPT the kernel-effect table ("read against the kernel's documentation") and names the two rows that measurement found WRONG (`sched_getaffinity`, `rt_sigaction`; §2g E-4). **Recommendation (ratified): narrow it** -- the table one generated authority, its write-region rows held to the running kernel on every run in both runners by a probe that demands equality, and what a reader still accepts said in words (paths no probe reaches; the kernel the suite ran on; bytes, not mappings). | 1.5.6b | 1.5.6b planning |
| ~~**S-75**~~ | **D-295** | **SETTLED (user, 2026-09-17: "go with your recommendations on all three"); LANDED at 1.5.6b step 4d (2026-09-17): a belt in both runners (`floor.check_models_explicit`, `npkg/floor_explore.npk`), five findings byte for byte, held to each other on twelve planted texts; TCB.md §5's thirteenth acceptance narrowed; its first finding was the self-check's own toy model, unsafe in two steps.** ~~OPEN (raised by 1.5.6b step 2, 2026-09-17).~~ Should the floor's protocol models be decided a SECOND way — by explicit-state search over their whole reachable space — as a standing belt in both runners? **What was measured** (the step's record; `meta/roadmap/done/1.5/tools/model_bfs.py`, a probe outside the gates): the seven models are SMALL (68 to 1,086 reachable states) and plain breadth-first search reads each in under half a second with no depth bound, no preemption bound and no solver. It found no bad state anywhere in any model, every control's bad state inside its bounds, no step blocked by a variable's range, and no disagreement with z3 wherever both speak — and it found what the bounded rows cannot say: in three models the depth is SMALLER than the diameter (`channel-table` K 12 against 15, `driver-registry` 10 against 11, `park-unpark` 14 against 19; 70 reachable states outside the bounds), and `park-unpark` cannot be unrolled to its diameter under the profile (K 19 answers `unknown`). TCB.md §5's thirteenth acceptance asks the reader to accept exactly that ("a defect that needs more steps than K, or more thread switches than D, is outside what was proven"). **Recommendation: PROMOTE it — a belt in both runners, beside the z3 rows, not instead of them.** `floor.check_models_explicit` (harness) and `npkg/floor_explore.npk` (npkg), the same findings by name: a bad state reachable ANYWHERE (`floor-model-bad-reachable`), a control whose bad state is reachable nowhere (`floor-control-blind`, today's name, without the bound), a step a range blocks (`floor-model-range-blocks` — a transition silently removed), and THE TWO READINGS DISAGREEING inside (K, D) (`floor-model-readings-disagree`). That last one is the point: today a model's MEANING has exactly one reader in the tree — `npkg/floor_model.npk`'s unroller writes the SMT text for BOTH runners, and the harness's Python side reads only the `(ir …)` forms for the correspondence belt — so the two-runner rule, which holds every other belt, does not reach what a model means. Two algorithms over one model text is that rule applied to the evidence itself — 1.5.6's stutter hole (an unroller that made a halted protocol read as a safe one) would have been a disagreement on its first run instead of a thing the self-check's toy model happened to show. With it the thirteenth acceptance narrows to LIVENESS alone for the seven models: the bounds stop being where the safety claims stop. Cost: about 150 lines of Python and 400 of Nitpick plus self-check cases from one set of texts (a reachable bad state, its control, a range that blocks, a planted disagreement); under a second per run; no solver, no manifest change (the rows stay the rows). It would land as a step of 1.5.6b before the close. **Alternatives costed:** (b) leave it a probe — the measurement stands in the record and nothing holds it, so the next model edit is covered by the bounded reading alone; (c) REPLACE the unrolling with the search ("one mechanism rather than two") — declined: they are not two mechanisms for one job but two independent readings that check each other, the manifest's rows and D-040's discipline are built on the solver's verdicts, and a model that outgrows enumeration (a wider range, a fourth thread) still has the symbolic reading. | 1.5.6b | 1.5.6b step 2 |
| ~~**S-76**~~ | **D-296** | **SETTLED (user, 2026-09-17: "go with your recommendations on all three"); LANDED at 1.5.6b step 4b (2026-09-17): `check_callable_name` at every site that types a binding, by the binding's TYPE (a `pick` PATTERN binding has no annotation, and is covered); `callable_builtin_names.npk` (seven refusals, the accepted shapes beside them -- the library listener's negative case among them: a function-typed FIELD named after a builtin, declared and called through its receiver). A struct pattern binds its field by name, so destructuring such a field IS refused.** ~~OPEN (raised by 1.5.6b step 4, 2026-09-17).~~ Does D-294 reach a PARAMETER or a LOCAL of FUNCTION TYPE named after a builtin? **Measured while landing D-294:** `func int64() never fails:path_exists = eight;` inside a function makes `raw path_exists()` call the local's function value for the rest of that scope (exit 8) — the builtin's name taken over, lexically and in view, where D-294's module-level case was invisible at the call and crossed imports. The module-level twin is impossible (a module binding cannot carry a function value, TYPE-035), a binding of any OTHER type named `alloc` or `read` is harmless (calling it is a type error, loud), and a function-typed FIELD is reached through its receiver like a method. The tree has 15 function-typed bindings and none is named after a builtin; `nitpick-libs` is theirs to measure. **Recommendation: YES, extend it — a parameter or a local whose annotation is a function type may not take a builtin's name, `NITPICK-RESOLVE-001` at the binding.** D-294's own principle is that the builtin table's names are the compiler's "in the FUNCTION namespace", and a function-typed binding IS in it: it is called by its bare name. One rule with no "unless it is a local" is what the blueprint asks (a construct does not change meaning by context), a prover reading `alloc(…)` never looks for a shadow of any kind, the check is syntactic (the annotation's node kind) and costs zero sites today. It is a new refusal, so the library listener is told with the code before it lands; it would be one more step of 1.5.6b. **Alternative costed:** leave it — a local shadow is ordinary lexical scoping and visible in the function's own text; declined as the recommendation because "visible if the reader looks" is the argument D-294 already rejected for the module-level case, and the exception would be the only place a bare builtin call can mean something else. | 1.5.6b | 1.5.6b step 4 |
| ~~**S-77**~~ | **D-297** | **SETTLED (user, 2026-09-17: "so, I think i am good with all your recommendations" — the sentence that settled S-77…S-83 together, first-hand to `nitpick-compiler_s7`); LANDED 2026-09-17 as its own landing before 1.5.7 step 0: the net is `120 + 10·checks + 60·B` seconds per file in both runners, B the file's rows the manifest the run is held to records `budget` (matched by hash, kind and symbol; a `--record` run, a verify test and a planted case have none to trust and take the larger bound, B = checks; the tier-2 twin counts its own rows so recorded; a model's control keeps the one-row floor), the failure line naming the net; `hang-net`/`hang-net-untrusted` in both self-checks from one planted text. `npk_small_free` is decided under 610 s where it was 250, `npk_int_to_string` under 260 where it was 200, no other file moves; no verdict, manifest or ladder row moved.** ~~OPEN (the user; raised by 1.5.6c step 0, 2026-09-17).~~ The solver's HANG NET (P-13: `120 + 10·checks` seconds per file, both runners; a killed solver is a build failure and never a verdict) is sized as if a row cost ten seconds, and a row the manifest records as `budget` burns the WHOLE rlimit by definition — about 33 s each on this machine. Measured over every floor file in the runners' own mode (one process, push/pop, the profile, four harnesses running beside it): `npk_small_free`, whose six residue rows each run to the budget, takes **194–203 s of its 250 — 81% of the net**; the next file is at 20% (`npk_hs_put_dec`, 69 s of 350) and every other under 10%. It has passed every run. A machine a quarter slower, or a fifth and sixth concurrent harness (D-228's width is 6), trips it — a red run that is no verdict, which R5 answers with "re-run alone" and which a flake-free instrument would not produce. Found because 1.5.6c step 0's first spelling of its fix took the same file to 357 s; the step was made to fit the net (`apart-when`, 194 s) rather than the net to fit the step, and the margin is raised here on its own. **RECOMMENDATION: the net learns what the committed manifest already says — `120 + 10·checks + 60·(rows of the file recorded `budget`)`, read from the manifest the run is held to (a `--record` run, which has none to trust, takes the larger bound for every row), in both runners with P-13's text amended.** It stays a hang net — a wedged solver (S-71's Diophantine wedge ran 22 minutes and more) is still caught, six minutes later at worst for this file — and no verdict can move: the net never was one. The alternative is to leave it and rely on R5. Not a language question; an instrument's bound, which is why it is asked rather than taken. |
| ~~**S-78**~~ | **D-298** | **SETTLED (user, 2026-09-17: "so, I think i am good with all your recommendations" — one sentence for S-77…S-83, first-hand to `nitpick-compiler_s7`); LANDS across 1.5.7 (the transformer and its belt at step 0, LANDED 2026-09-18; the shim at step 1).** ~~OPEN (raised by 1.5.7's plan, 2026-09-17).~~ The schedule explorer's shim: hand-written LLVM IR in `runtime/explore/` (RECOMMENDED — D-203's form for exactly this kind of code; the plan's §2.2 says why Nitpick cannot express it: no mutable module state under D-211, and code inside the floor's critical sections may not allocate, trap or call the floor), or keep exploration out of the tree until it can be Nitpick. |
| ~~**S-79**~~ | **D-299** | **SETTLED (user, 2026-09-17: "so, I think i am good with all your recommendations"); LANDS at 1.5.7 step 1.** ~~OPEN (raised by 1.5.7's plan, 2026-09-17).~~ Explicit markers — a program opts in with `// explore: N` or out with `// explore: no <reason>`, and a belt in both runners (`explore-unmarked`) holds every `// stress:` program to one or the other (RECOMMENDED), or "every `// stress:` program is explored" implicitly, the nine that cannot be (real child processes) living in a runner's skip list. |
| ~~**S-80**~~ | **D-300** | **SETTLED (user, 2026-09-17: "so, I think i am good with all your recommendations"); LANDS at 1.5.7 step 1.** ~~OPEN (raised by 1.5.7's plan, 2026-09-17).~~ Seeds per explored unit per run: 1,000 (RECOMMENDED — MEASURED: 35 programs × 1,000 seeds = 35,000 schedules, all correct, both oracles silent, 298 s beside four harnesses, about five minutes per runner) against 100 (about 30 s). Per-run cost is not the constraint; coverage is. |
| ~~**S-81**~~ | **D-301** | **SETTLED (user, 2026-09-17: "so, I think i am good with all your recommendations"); LANDS at 1.5.7 step 3.** ~~OPEN (raised by 1.5.7's plan, 2026-09-17).~~ `LOST-WAKE` / `LOST-FUTEX-WAKE` are RED runs even when the exit code is right (RECOMMENDED — MEASURED: a lost wakeup in this runtime is LATENESS under D-071's deadlines, virtual time hides it completely, and `nested_wait`'s planted defect is found on 100 of 100 seeds by the oracle, on 0 by exit code, on 0 of 10 stress runs), or reported without failing. |
| ~~**S-82**~~ | **D-302** | **SETTLED (user, 2026-09-17: "so, I think i am good with all your recommendations"); LANDS at 1.5.7 step 5.** ~~OPEN (raised by 1.5.7's plan, 2026-09-17).~~ The spec's caller hypotheses EXECUTED at every call of every explored schedule (the plan's §2.4: generated checkers at each specified symbol's entry, `ASSUMPTION <symbol>: <clause>` the verdict, an unevaluable clause listed by name) inside this subcycle (RECOMMENDED — it is where 1.5.6c's sixteenth acceptance gets its test; MEASURED: `npk_small_free`'s old clause false on 5,276 of 5,276 calls, the `apart-when` clause on none), or its own subcycle after. |
| ~~**S-83**~~ | **D-303** | **SETTLED (user, 2026-09-17, on his stated condition: "The only thing i'm not positive about is the C shim you mentioned. What exactly is it's purpose? If it's just an extra layer of verification for us that doesn't get shipped then i don't see a problem with it." — answered: a reference oracle for the hand-written IR, a measurement tool, never linked into anything the artifact is); LANDED at 1.5.7 step 0 (2026-09-18): `meta/roadmap/done/1.5/tools/explore_prototype/` with its README and amended headers.** ~~OPEN (raised by 1.5.7's plan, 2026-09-17).~~ Keep the planning prototype's C shim in the tree, OUTSIDE every gate, as the IR shim's BEHAVIOURAL REFERENCE, held to it schedule hash for schedule hash at step 1 (RECOMMENDED — it turns the hand port of ~375 lines of C into ~900 lines of IR from a review into a measurement; the zero-dependency rule governs what ships, and this ships nowhere, built by nothing), or no C file in the repository at all, the IR validated by the units and the negative controls alone. |
| ~~**S-84**~~ | **D-304** | **SETTLED (user, 2026-09-18: "go with all four" — the sentence that settled S-84…S-87 together, first-hand to `nitpick-compiler_s11`, after his answer to the question itself: "As far as decreases, I'm not sure really what it does but if we need it for verification I say we add it"); LANDS at 1.5.8c.** ~~OPEN (raised by 1.5.8's planning, 2026-09-18).~~ `terminate` has no surface: `decreases` appears in no lexer, parser, grammar or specification, yet D-218 (7) ratified "`decreases`-style variants on recursion and unbounded loops". RECOMMENDED and settled: `decreases E` on `while`/`when` (beside `invariant`) and on functions (beside `requires`/`ensures`), an integer measure CHECKED IN EVERY BUILD (`DecreasesViolated`, 4119, when it fails to shrink or goes below zero) and proven where it can be; REQUIRED on every `while`/`when` as `decreases E` or the new keyword `unbounded` (TYPE-072), after the tree is swept; OPTIONAL on recursion (D-305's stack check is its net). Declined: optional everywhere (a silent hang the default); required on recursion too (285 functions in 103 recursive groups in the compiler, for a stop D-305 makes controlled); inference as the rule; a fuel budget. | 1.5.8c | 1.5.8 planning |
| ~~**S-85**~~ | **D-305** | **SETTLED (user, 2026-09-18: "go with all four"); LANDS at 1.5.8 step 2.** ~~OPEN (raised by 1.5.8's planning, 2026-09-18; DEF-59, DEF-60).~~ A stack overflow is SIGSEGV with no `failsafe` (the floor installs only SIGUSR1's action; `npkc` under `ulimit -s 2048` exits 139), and a spawned thread's one guard page can be jumped by a frame larger than a page (151 of the compiler's own functions at `-O0`, the largest 120,904 bytes). RECOMMENDED and settled: LLVM's split-stack prologue on every emitted function (the exact frame compared against a per-thread limit BEFORE it is allocated), `StackExhausted` (4118) through `__morestack`, every thread's stack the floor's (the main thread moved onto 8 MiB), signals on per-thread signal stacks, `failsafe` on a stack of its own (an overflow inside it exits 70), the check never elided (its necessity rests on frame sizes the backend decides and on every call path). Declined: an IR-level check (it runs after the frame is written); a guard-page handler (a big frame jumps the guard without faulting); a bigger stack; inheriting `RLIMIT_STACK`. | 1.5.8 | 1.5.8 planning |
| ~~**S-86**~~ | **D-306** | **SETTLED (user, 2026-09-18: "go with all four"); LANDS at 1.5.8 step 1 (the guard) and 1.5.8b (the row).** ~~OPEN (raised by 1.5.8's planning, 2026-09-18; DEF-58).~~ A float's `=>!` cast to an integer lowered to a bare `fptosi`/`fptoui` — LLVM poison for NaN, ±∞ or an out-of-range value; measured: one program exits 3 at `-O0` and 9 after `opt -O2`, whose fold turned all of `main` into `unreachable`. RECOMMENDED and settled: `=>!` keeps dropping the fraction toward zero, and a value with no integer meaning traps `CastRange` (4117), scalar and `simd` any-lane; `cast-range` is the guard's row. Declined: saturation (a number the author never wrote); ERR (plain integers have none); a Result-returning spelling. | 1.5.8 | 1.5.8 planning |
| ~~**S-87**~~ | **D-307** | **SETTLED (user, 2026-09-18: "go with all four"); LANDS at 1.5.8 step 3.** ~~OPEN (raised by 1.5.8's planning, 2026-09-18; the question `nitpick-compiler_s10` put to the user through this seat).~~ Any other hardware fault — a `wild`/`wildx` pointer, a `sys` buffer, JIT code, a floor defect — kills with the kernel's default action. RECOMMENDED and settled: SIGSEGV/SIGBUS/SIGILL/SIGFPE handled on each thread's signal stack, entering the trap route as `MachineFault` (4120); `failsafe` on its own stack; a second fault inside it re-enters (SA_NODEFER) and exits 70. What stays uncontrolled (a fault in the kernel's own delivery, and what a fault already corrupted) is recorded in TCB.md §5. Declined: the default action; SIGSEGV alone; `failsafe` on the signal stack. | 1.5.8 | 1.5.8 planning |
| ~~**S-88**~~ | **D-308** | **SETTLED (user, 2026-09-19: "as far as the limit thing, I am in agreement with your recommendation. It sounds like it extends limit into an even more capable construct than the one I originally imagined. Lets ratify that." — first-hand to `nitpick-compiler_s11`); LANDS at 1.5.8b step 6.** ~~OPEN (raised by 1.5.8b's planning, 2026-09-19).~~ To the solver a field read is opaque, and D-220's `limit` binds no field. A throw-away prototype measured the compiler's own 2,343 `overflow` rows: 1,158 discharge today, and an ASSUMED length bound adds 167. RECOMMENDED and settled: a struct field may carry `limit<R>` with a single-field rule. It is checked at every write to the field, is a fact at every read, has no address, and its rule holds of the field's vacant value (TYPE-077). The prelude `List`'s `count` and `cap` carry it, and the built-in lengths' bound (2^47) is PROVEN where every length is made. Declined: a compiler-known axiom (unsound for `List`'s writable fields), no fact, and struct-wide relational rules. |
| ~~**S-89**~~ | **D-309** | **SETTLED (user, 2026-09-19: "those recommendations sound fine to me as well. Lets ratify those too." — the sentence that settled S-89 and S-90 together); LANDS at 1.5.8b step 3 and its close.** ~~OPEN (raised by 1.5.8b's planning, 2026-09-19).~~ The overflow rows nothing proves. RECOMMENDED and settled: they keep their guards, no bound is written into the tree for a count's sake, the residue is reported per function, and the compiler's speed is measured with and without the elided checks. Declined: a `limit` sweep through `src/` to maximise the proven share. |
| ~~**S-90**~~ | **D-310** | **SETTLED (user, 2026-09-19: "those recommendations sound fine to me as well. Lets ratify those too."); LANDS at 1.5.8b step 2.** ~~OPEN (raised by 1.5.8b's planning, 2026-09-19; DEF-70).~~ A certain constant overflow. RECOMMENDED and settled: an integer `+ - *` or negation of compile-time constants is folded, and refused when its value does not fit (NITPICK-TYPE-076), as D-277's TYPE-070 refuses a known out-of-range shift. Declined: the run-time trap that always fires. |
| ~~**S-91**~~ | **D-311** | **SETTLED (user, 2026-09-19: "that all sounds fine and now I do remember it. I think that means your recommendation is a go from me." — after the D-037 → D-210 history was set out for him); LANDS at 1.5.8b step 2 with D-310.** ~~OPEN (raised 2026-09-19 by the library workbench, `nitpick-libs_s4`, measuring D-310's reach; DEF-71).~~ D-148 prescribes `0u64 - 1u64` for `uint64`'s maximum, "exact by D-037's defined wrap". D-210 replaced that wrap with a trap, and the folder never followed, so D-310 would refuse the prescribed spelling (two library sites, none in this tree's code). RECOMMENDED and settled: bit construction (`~0u64`; `(1u64 << 63u64) \| k`), with the folder obeying D-210 as the run time does. Declined: exempting `fixed` initialisers, widening the literal envelope, prelude-named maxima. |
| ~~**S-92**~~ | **D-312** | **SETTLED (user, 2026-09-19: "I agree. the % family makes the most sense to me too as it's actually describing what will happen, not just a nifty shorthand way of doing a thing. It goes right along with the 'blueprint philosphy' we have with the other operators ... lets roll with it."); LANDS at 1.5.8b step 4.** ~~OPEN (raised by the USER, 2026-09-19, from D-210 §3's deferred operator question: "if we plan on putting it in ever it needs to be before I begin Nikola work and in time to go through all the verification stuff before I do").~~ A dedicated wrapping-arithmetic spelling. The library workbench measured the consumers in sight: FNV-1a 64 per byte, and splitmix64. RECOMMENDED and settled: `+% -% *%` and `+%= -%= *%=`, read "modulo 2^N", on plain integers and integer `simd` lanes, refused elsewhere (TYPE-078). No guard, no row, no REACH arm; plain LLVM `add`/`sub`/`mul`; exact `mod 2^N` terms; folded with the wrap. Declined: `&+`, `+!`, methods, a wrapping type family. |
| ~~**S-93**~~ | **D-313** | **SETTLED (user, 2026-09-19: "Lets go with A. As far as keyword, I think sealed is fine. ... i'd rather overlap some than come up with some word that doesn't accurately represent what is happening."); LANDS at 1.5.8b step 1.** ~~OPEN (raised by 1.5.8b's planning, 2026-09-19; DEF-72, DEF-73).~~ Nitpick had no field visibility, and two confirmed memory-safety holes followed (a string's `.len` writable; the prelude `List`'s `cap` writable, corrupting the heap). RECOMMENDED and settled: a `sealed` field qualifier, meaning read anywhere and written only by the declaring module, with struct literals, moves out and write-capable addresses counting as writes (TYPE-079). The compiler-known containers' headers are sealed by definition, the prelude `List`'s three fields are sealed, and `src/`'s direct writes move to checked prelude operations with a bridging refresh. Declined: special-casing only the compiler-known containers, run-time validation only, and read-and-write privacy. |
| ~~**S-94**~~ | **D-314** | **SETTLED (user, 2026-09-19: "hidden sounds fine to me. it says what it does. i like it"); LANDS at 1.5.8b step 1 with D-313.** ~~OPEN (raised by 1.5.8b's planning, 2026-09-19; DEF-74).~~ `List` element access is raw wild-pointer indexing through `items` in any module, unchecked, with no opt-out at the site (about 1,700 sites). RECOMMENDED and settled: a `hidden` field qualifier, neither read nor written outside the declaring module (TYPE-080), completing `sealed`. `List<T>` is indexed `l[i]`, bounds-checked against `count` with the slice's guard and row. The prelude `List`'s `items` is hidden and `count`/`cap` sealed, it gains checked pop/truncate/clear/insert/remove/swap_remove, and the sites move to `l[i]`. Declined: a List-only special case, a `wild` acknowledgement at every raw index, and `private`. |

## 2e-bis. ~~Two language questions 1.5.8b raised~~ BOTH SETTLED — S-95 as D-315 (2026-09-23), S-96 as D-316 (2026-09-24)

| # | Item | Recommendation |
|---|---|---|
| ~~**S-96**~~ | **SETTLED as D-316 (user, 2026-09-24: "both of your recommendations are fine").** ~~The sixteen genuine event loops of the tree (the executor's run loop, the reactor's wait, a driver's dispatch, `npkg`'s output drains): `unbounded` with the reason on the line above, or a measure over a trip budget?~~ Raised by 1.5.8c's plan (§3) at 1.5.8b step 7. | `unbounded`, with the reason on the line above — D-304 §4's own words for an event loop; a trip budget is "a number nobody can defend", D-304's objection to fuel; the reasons are greppable, so the list of opt-outs is auditable. |
| ~~**S-95**~~ | **SETTLED as D-315 (user, 2026-09-23: "i am sure your recommendation is likely fine as long as it doesn't compromise on safety anywhere" — and it compromises none: the sentence enforced nothing, and 6c added the ceiling check). LANDED at 1.5.8b step 6c.** ~~What does "legal only in `wild` context" mean for `#wild_slice` (and `#wild_ptr`)?~~ BUILTIN_REFERENCE's `#wild_slice<T>(ptr, len)` row has said it since D-070, and 1.5.8b's plan (§2.7) read it as a rule to enforce at step 6c "as TYPE-054's rule for a builtin's call context". Measured at 6c: no such rule exists for either builtin, and the phrase has no checkable definition anywhere — the only context rule either carries is TYPE-061 (neither may appear in a `pure` body, D-242), and neither the checker nor any decision defines a "`wild` context" (a `wild`-qualified receiving binding? a function that holds `wild` storage? a module that imports `nsys`?). What 6c DID enforce is the ceiling: both producers' lengths are held to `[0, 2^47]` and trap `OutOfBounds` outside it (D-308 §7). | **Strike the sentence.** The `#wild_` spelling IS the acknowledgement — the greppable opt-out the TOS system asks for (D-019 for `#wild_ptr`, D-070 for `#wild_slice`) — and TYPE-061 already bars both from the one context where an unverifiable extent could launder into a proof. A rule keyed on the RECEIVING binding's regime would be a second mechanism saying the same thing in a place the author does not write it, which the blueprint philosophy argues against. If a rule is wanted, the one that fits the language is the existing spelling: the call itself. Until settled, the row keeps the sentence with a note pointing here, and nothing enforces it — as nothing ever did. |

## 2e-ter. ~~Three questions 1.6.0's planning raised~~ ALL SETTLED 2026-09-25, the day they were asked — S-99 as D-319, S-100 as D-320, S-101 as D-321 (the user: "i like those recommendations. lets ratify them.")

| # | Item | Recommendation |
|---|---|---|
| ~~**S-99**~~ | **SETTLED as D-319.** ~~Is NIKOS v2.4.0 — the user's own port of IKOS to LLVM 20, found on the workbench at `REPOS/nikos` and recorded nowhere in the tree — the IKOS candidate of the 1.6.0 gate, weighed as any tool and decided out if it loses?~~ Raised by 1.6.0's plan (§3); the history (D-217 struck NIKOS from 1.5; D-233 named IKOS with "the port the toolchain plan always called NIKOS") is recapped in the decision. | Yes: NIKOS at `94b54c2c` is the candidate, measured under the same rows and rule as Clam, ownership no criterion; port items land in the nikos repository under a new pinned commit if it wins; decided out with its scorecard if it loses. The user's reason, recorded in D-319: a transform between the artifact and the analyzer (IKOS at LLVM 14; Astrée's C) makes the evidence about the transform. |
| ~~**S-100**~~ | **SETTLED as D-320.** ~~Ratify the gate's decision rule before the numbers exist?~~ 1.6.0's plan §2.5. | Ratified as written: soundness on planted defects first (a miss disqualifies the class), then reach times the share of alarms real or remediable by the floor model, determinism a must-hold, cost the tie-breaker, port distance recorded and never deciding. |
| ~~**S-101**~~ | **SETTLED as D-321.** ~~Alive2's budget and its solver: a one-line patch to a resource limit, and a static link against the pinned z3 — under a stated rule for recorded patches?~~ 1.6.0's plan §3; D-218 (2), D-233 and D-265 recapped there. | The patch (`rlimit` in place of Z3's `timeout`), kept in the tree with its digest part of the pin; Alive2 linked statically against `libz3.a` of the pinned z3 commit; the rule that a workbench tool's patch is part of its pin, applied only by the build script. |

## 2f. Compiler defects reported by the library workbench (owner: the `src/` writer — scheduled as 1.5.1b, before 1.5.2) — **CLOSED as a queue at the 1.5 close (2026-09-25): every entry DEF-1…DEF-94 carries its disposition — FIXED with its landing, or SETTLED by a decision (DEF-19/20 → D-260/261, DEF-36 → D-285, DEF-38 → D-284); a defect found from here goes to the cycle that finds it**

Raised 2026-09-03 by the `nitpick-libs` orchestrator (`nitpick-time` cycle
0.0.0, probe 04) against compiler commit `950bb1d`, under ORCHESTRATION R6 —
recorded, stopped, escalated; nothing in the library is shaped around either.
The reproduction, the curves and the recipes:
`REPOS/nitpick-libs/nitpick-time/tests/probe/defect/README.md` (their O-N4);
`big_fixed_array_cost.npk` beside it is the four-second case and becomes the
regression test when the defect closes.

**DEF-1 — compile time and memory are QUADRATIC in the size of one
declaration, on three independent axes.** (1) elements of one module-level
`fixed` array of `{ int64; int32 }`: 4 000 rows 4.19 s / 473 MiB, 8 000 rows
15.83 s / 1.73 GiB, 30 000 rows 281 s / 30.9 GiB — a ratio approaching 4 per
doubling in both columns; the cost is in the DECLARATION (a `main` that never
reads the table costs the same), not struct-specific (`int64[4000]` 1.41 s),
and not "many constants" (4 000 separate `fixed int64` bindings cost 0.61 s /
58 MiB) — it is the size of ONE declaration. (2) statements in one function
body (`acc = acc + k;` ×N): 1 000 0.87 s / 134 MiB, 4 000 7.03 s / 1.27 GiB.
(3) bytes in one string literal: 60 k 5.2 s, 480 k 308 s, memory FLAT — a
separate pathology (quadratic time, linear space) from the other two. What it
costs: TM-007 compiles the IANA tzdb (26 838 rows) into the binary as `fixed`
module state; a 16 GiB machine cannot build the library and every consumer
pays it. **The ask is on `npkc` alone, no language change**: the
array-initialiser and function-body paths linear (or near enough that 30 000
rows is seconds and hundreds of megabytes), the string-literal path linear in
time. Suspects to measure first, from the tree's own history: an accumulating
`string_concat` per element or per byte (the 1.4.8 `lib/nproc.npk` capture
was exactly this shape and spent 17 of 56 minutes in the kernel), a per-node
window copy in the AST scratch pool, and a per-statement re-walk in the
checker or the obligation walk. Measurement discipline the reporter learned
the hard way: every timing must be paired with `npkc` exit 0 — a `failsafe`
missing an arm fails fast and looks like a fast compile.

**DEF-1's cause, measured 2026-09-03 (this session, read-only — no `src/`
edit while the 1.5.1 prefix harnesses run):**
- **Stage bisection: the frontend is linear on all three axes.**
  `tools/check.npk` on the 8 000-row table 0.32 s / 26 MiB where `npkc`
  costs 17.5 s / 1.9 GiB; on 4 000 statements 0.41 s where `npkc` costs
  7.3 s / 1.33 GiB; on the 480 k-byte literal 0.07 s where `npkc` costs 23 s.
  The emitted IR is linear in size on every axis (8 000 rows is one 482 KB
  constant line). So it is not "total source bytes" (the reporter's second
  theory) and not a checker re-walk: it is the BUILDING of three pieces of
  text in `src/backend/`, three loops of one shape — an accumulator
  re-concatenated per element. Axis 1: `emit_global_array_const`
  (`src/backend/ir/ir_expr.npk:9479`, `out = cat3(out, ell.text, …)` per
  row; `emit_global_struct_const` beside it, per field). Axis 2:
  `emit_site_tables` (`src/backend/emit_program.npk:1981`, `ls = cat3(ls, …)`
  and `ps = cat5(ps, …)` per D-179 trap site — a statement with an overflow
  check IS a trap site, so 4 000 statements are 4 141 rows of
  `@npk.site.paths`). Axis 3: the string-literal escaper
  (`src/backend/ir/ir_expr.npk:193`, `enc = string_concat(enc, esc_byte(b))`
  per BYTE — 480 k iterations each copying the whole prefix is the whole of
  axis 3). The writer already owns the linear idiom: `Sink`
  (`src/frontend/diagnostics.npk:268`, `sink_to_buffer`/`sink_write`) grows
  geometrically and is what every `irw_line` writes through. The fix writes
  the pieces into a sink instead of returning an accumulated string; the
  emitted bytes do not change, and the harness's `selfhost`/`nf-inert`/`repro`
  stages say so.
- **Why the memory is quadratic on axes 1 and 2 and flat on axis 3: an owning
  TEMPORARY is never dropped.** Two probes, built with the 1.5.1 worktree's
  compiler, exit 0 both: `t = string_concat(t, "b")` 20 000 times peaks at
  260 KiB — the old body is freed when the binding is overwritten; `t =
  string_concat(string_concat(t, "b"), "c")` 20 000 times peaks at
  429 740 KiB — the inner call's result is an unbound temporary passed as an
  argument, and nothing ever frees it: it lives until the process ends.
  `cat3`/`cat4`/`cat5` (`src/backend/ir/ir_writer.npk:305`) are exactly that
  shape, so the accumulators above leak their whole prefix once (`cat3`) or
  three times (`cat5`) per element: 8 000² / 2 × ~60 B ≈ 1.9 GiB, the
  measured 1.86 GiB; 3 × 4 141² / 2 × ~47 B ≈ 1.2 GiB, the measured
  1.33 GiB; the escaper's bound reassignment frees and stays flat. This is
  D-183's recorded "statement-end temporaries" item — DECISIONS, written at
  1.2.4a for `dyn`: "temporaries do not drop yet — that cell rides the
  recorded statement-end-drops debt" — never scheduled since. A
  managed-regime defect, not an emitter shape: every Nitpick program pays it
  at every `f(g(x))` whose inner result owns memory.
- **The compiler's own footprint is the same two facts at scale.** `npkc
  src/main.npk` peaks at 11 516 824 KiB (11.0 GiB) of resident memory for
  15.6 MB of output; `check src/main.npk` — the frontend alone — at
  10 956 716 KiB; `parse_check` on the one file at 584 KiB (it parses one
  file, so it attributes nothing — the ten gigabytes are loading,
  resolution, checking and the analyses). Two causes, both by
  construction: the temporaries above, and `List<T>`
  (`src/frontend/list.npk`, 1.4.7) is a `wild` block whose own comment says
  "nothing here drops" — 148 struct fields across 28 files hold Lists that
  are never freed, so every per-function analysis table lives until exit.
  The compiler is a bump allocator that happens to build itself; a 16 GiB
  machine builds it with no margin, and four harnesses in parallel do not
  fit (the 1.4.8 UI freeze has a candidate cause). A `wild` block behind a
  copyable struct is also an aliasing hazard the checker cannot see: two
  copies of one List, one `list_push` that `ralloc`s, and the other copy's
  `items` dangles.

**DEF-2 (their O-N8) — a root file whose `mod:` name differs from its basename
is ACCEPTED when a sibling carries that basename**: the loader compiles the sibling too,
merges both files into one module, and emits two `define i32 @main` at exit 0;
`llc` refuses the result. Delete the sibling and the refusal is exemplary
(RESOLVE-005 names the rule and anticipates the self-header case), so the
loader knows the rule and skips it when the given name resolves to a
different file. Silent invalid IR at exit 0 is the "silence is not success"
class; the fix is a basename check at the ROOT before the name is resolved as
a module, with a two-file case in `tests/modules/rejection/` (the reporter's
six-line reproduction is in the README above).

**DEF-3 (their O-N9, raised 2026-09-03) — a `uint8[]` view returned out of
the frame that owns its string compiles clean and reads freed memory.**
`string_bytes` on a local `string` (1.1.12c), returned: exit 0, and the
caller's byte 0 is not what was written — measured by the reporter. The
rule exists and is enforced beside it: returning `@x`, or a struct literal
holding `@local`, is `NITPICK-BORROW-001` (D-004 rule 2). The slice view a
view-maker returns is a borrow of its source and is not treated as one:
`src/frontend/analysis/` names neither `string_bytes` nor `string_from_bytes`
(D-186's "one remaining view-maker") anywhere — the only view the analyses
know is the range-view `arr[lo...hi]`, and only the suspend walk knows it
(D-191). An under-enforcement, no language change: the borrow walk learns
that a view-maker's result borrows its operand, on every path a borrow is
refused today (return, struct literal, channel, store into an outer place,
the launder-through-a-call rule), with the reporter's reproduction as the
case in `tests/analysis/rejection/`. It bites every parser in the ecosystem
(any function taking `uint8[]`). The reporter's disposition — obey "a view
is a parameter, never a return value", enforced by their own harness check —
is conformance with a written rule, so nothing of theirs is stalled.

**Recommendation (revised 2026-09-03 after the bisection):** a dedicated
subcycle **1.5.1b** immediately after 1.5.1 closes and before 1.5.2 (the
user's call, 2026-09-03: "finish up 1.5.1b before we move on") — `src/` work
under the one-writer rule, FIVE commits, each under a full harness, in this
order, DEF-1 measured before it is touched (the three axes and the two
probes as programs with a wall-clock and a peak-RSS belt) so every fix is a
number and not a claim:
1. **DEF-2**, the loader's basename check at the root — six lines,
   deterministic, independent of everything else.
2. **DEF-3**, the view-makers as borrows in the escape analysis — a
   refusal, the reporter's reproduction as its case.
3. **DEF-1's three builders through a `Sink`** — emitted bytes
   byte-identical; this alone makes the reported curves linear in time AND
   memory (a sink creates no prefix-sized temporaries).
4. **Statement-end temporaries** (proposed **D-246**, the user's): an owning
   temporary no consumer takes is dropped at the end of the statement that
   created it, on every path out of the statement (`?|`/`?!` relays
   included); the coroutine case is bounded by D-178 (an `await` is its
   statement's first evaluation, so the only temporaries alive across a
   suspension are the await's own arguments, which take frame slots). An
   emission change under `selfhost`, `nf-inert`, opt-O2 and the stress
   loops, with the two probes as its programs. D-183's debt, closed.
5. **`List<T>` becomes an owning managed structure** (proposed **D-247**):
   its block a `buffer` (D-200's owning byte cell; growth is
   `buffer_new(2n)` + copy + the field-overwrite drop), so a List drops with
   its holder and is move-only under TYPE-046 — which also refuses the
   aliasing copy today's `wild` block permits — and the 148 holders are
   swept under the checker's own refusals. With 4, this is what brings the
   compiler's own build from 11 GiB to whatever the live data actually is,
   measured per stage before and after.
The library re-pins its toolchain when it lands; the landing message states
whether `build/` was written after the fix commit (rider below).

Three riders from the reporter (2026-09-03), each binding on 1.5.1b:
- **The reproduction is citable at a commit**: `nitpick-time`
  `8066e6229c77aed28be7ab471209962a03534b0f` on `main` carries the 4 000-row
  case, the README with all three curves, the recipes, and the probe-04
  verdict. The curve is ONE measurement by one agent until their
  independent verifier (dispatched to regenerate the axis-1 points at
  1 000 / 2 000 / 4 000 rows, each checked for exit 0) reports; the
  reporter first wrote "verified" before the answer existed and corrected
  it within the hour — the same trap one layer up, named by them. This
  session's own data point at the 1.5.1 tree is recorded below; if the
  verifier contradicts the curve, DEF-1 waits and DEF-2 (six lines,
  deterministic) does not.
- **Reproduced here, independently, 2026-09-03** — the compiler built from
  `efd6a4d` plus 1.5.1's steps 2–5 (the worktree's `quickemit` binary), on
  a machine running four harnesses, the reporter's committed 4 000-row file
  and three files regenerated from its shape (`mod:` renamed to the
  basename — the first attempt kept the original header and every point
  "compiled" in 0.04 s at exit 1, RESOLVE-005: the reporter's trap, met on
  the first try), every point at exit 0: 1 000 rows 0.49 s / 56 MiB,
  2 000 rows 1.31 s / 148 MiB, 4 000 rows 5.88 s / 580 MiB, 8 000 rows
  17.49 s / 1.86 GiB — ×2.7 / ×4.5 / ×3.0 in time and ×2.6 / ×3.9 / ×3.3 in
  memory per doubling. Quadratic, on a second build of a second tree.
- **The baseline is taken at the commit the fix starts from** — 1.5.1's
  close — with the README's recipes, never against the reporter's numbers,
  which are at `950bb1d`: measuring against those would conflate the fix
  with everything 1.5.1 changed in the frontend. The recipes are
  parameterised by N and regenerate in seconds at small N.
- **The re-pin needs one fact beside the fix commit**: whether `build/` was
  written AFTER it (the workbench pins `build/npkc` and `build/npkrt.o` and
  records whether the tree was clean, because a dirty tree makes the binary's
  label the nearest commit rather than its provenance). State it in the
  landing message so the re-pin needs no second round trip.
- No schedule pressure: O-N4 blocks their 0.0.5 and 0.5, not 0.0.1–0.0.4,
  and the workbench works the nine probes it does not touch meanwhile.

**DEF-3's reproduction is committed** (2026-09-03, `nitpick-time` `0667ecb`,
`tests/probe/defect/view_escape/`, six cases and a verbatim `TRANSCRIPT.txt`,
independently verified PASS at `9113487`): `case1_borrow_returned` (`@x`
returned — REFUSED, BORROW-001), `case2_borrow_in_struct` (a struct literal
holding `@local` — REFUSED), `case3_view_returned` (`string_bytes(local)`
returned — NOT refused, exit 0), `case4_view_in_struct`, `case5_read_after_free`
(the caller reads the freed bytes and asserts the allocator's 0xAA poison —
deterministic `exit 170`, not "usually garbage"), `case6_view_param_legal` (the
legal shape, so a fix cannot over-refuse). Cases 1 and 2 make it
under-enforcement rather than a design question; 1.5.1b step 2's
`view_escape.npk` carries the same six shapes.

**Blocking status, stated by the workbench's author (2026-09-03, their W-27:
an escalation says what it blocks):** DEF-1 BLOCKS `nitpick-time` 0.0.5 and
0.5 (the tzdb table at 26 838 rows: 281 s and 30.9 GiB, so no 16 GiB machine
and no CI builds the library in its shipping shape). DEF-3 BLOCKS all of their
`src/fmt/` and probes 09–10, by the author's explicit ruling against the
workbench's own "conformance" reading — a rule enforced only by a harness
check the library writes for itself is a thin guarantee for a use-after-free
and protects no consumer. DEF-2 blocks nothing (raised for correctness). DEF-4
below blocks nothing of theirs. The order DEF-2 → DEF-3 → DEF-1 is the one
that unblocks them fastest and is the order 1.5.1b already has.

**DEF-4 (their O-N10, raised 2026-09-03; reproduction at `nitpick-time`
`eb8d6b4`, `tests/probe/defect/derive_payload_enum/`, three cases and a
transcript) — `#[derive]` on an enum WITH A PAYLOAD: `Eq` does not compile,
`Ord` compiles to a tag-only order.** On `enum:Part = { Literal(uint16);
Year4; }`, `#[derive(Eq)]` is refused `NITPICK-TYPE-034` inside `<derived-1>`
("`Part` has no built-in `==`: derive or implement `Eq`" — the derived body
`pass (self == other);` needs the trait it is writing, and the span names a
synthetic file the user cannot open); `#[derive(Ord)]` on the same
declaration compiles, and `Literal(7).cmp(Literal(9))` answers `Equal` at
exit 0 with no diagnostic anywhere (their case 2 exits 221 — one digit per
comparison — where 321 is right). The loud half is inconvenient; the quiet
half is a wrong answer a sort or a binary search believes. Read against the
tree: `gen_eq_enum` (`src/frontend/macro/derive.npk:427`) writes `self ==
other` for every enum, which is the built-in tag equality a payload-less enum
has and a payload enum does not; `gen_cmp_enum` (316) compares `self =>!
int32` — the tag — BY DESIGN, its comment saying "a payload is not compared;
the order is over the variants, which is what deriving `Ord` on an enum has
always meant" (D-123's reasoning for `Hash`, which is legal for a hash and
wrong for an order). No file in the compiler's tree derives anything on a
payload enum (`enum:Season`/`Tag`/`Level` are payload-less), so the payload
path was written and never run — coverage, not regression. **What is asked:**
a derived `Eq` that compiles and compares tag then payload; `Ord`/`PartialOrd`
that compare the payload after the tag rather than stopping at it; and a test
in this tree that derives on a payload enum. `Hash` hashing the tag only is
NOT asked (a colliding hash is correct, if weak; D-123 stands). **Not
blocking them** (`nitpick-time` exposes one payload enum, `FmtPart.Literal`,
and no rule needs a derive on it); it blocks the first library that wants a
derived comparison on a payload enum. **Proposed as S-23 below, and as a step
of 1.5.1b for the user to ratify** — the standing rule is that a defect a
real program finds is fixed before planned work.

**Found by 1.5.1b step 0 itself, both fixed in it (runtime only):**
- **The argv and environ arrays lived in the releasable heap.**
  `npk_cstr_slice` built both `{ptr, len}` arrays with `npk_alloc_internal`
  and the comment said "never freed" — but `wild_release_all` unmaps every
  chunk wholesale, so a program that released and then read an ENTRY of its
  own argv or `environ()` read unmapped memory: `src/main.npk` releases
  before every exit, and a `failsafe` may do both. Found because the first
  version of the NPK_HEAP_STATS report walked the environment slice at exit
  and the compiler's own build faulted; `tests/backend/programs/
  argv_after_release.npk` exits 0 on the step-0 runtime and segfaults (139)
  on the 1.5.1-close runtime. The arrays are a page-rounded `npk_hmap`
  mapping outside the chunk and large tables now, which is what "outlives
  everything" required all along. BUILTIN_REFERENCE's `environ` and
  `wild_release_all` rows say so.
- **`npk_aalloc`'s over-aligned path took no lock.** Since the heap mutex
  arrived (1.2.5b) `npk_alloc_impl` has locked around `npk_large_new`, whose
  `npk_lg_insert` mutates the large table; `npk_aalloc`'s `wide:` path called
  the same function unlocked. Two threads asking for an over-aligned block
  could race the table. Found placing the accounting, which needs the lock
  too; locked now.
- **D-151 and D-188 see no managed body** (the workbench's note, 2026-09-03,
  confirmed): D-151 counts `wild` blocks, D-188 counts live drivers; a leaked
  `string` body passes both at exit 0, which is why "exit 0 proves no leak"
  was never a gate for managed memory and why step 0's instrument reads the
  allocator's own `peak_live` instead. No document in this tree pairs the two
  as a leak guarantee; the workbench swept its six repositories for the
  pattern (nine sites in `nitpick-parse` alone).

**DEF-5 (their O-N11, raised 2026-09-03; reproduction at `nitpick-time`
`b092a9e`, `tests/probe/defect/missing_failsafe/`, three cases and a
transcript; probe 11 at `0f86d6e`) — a root with `main` and no `failsafe`
compiled at exit 0, and the arm contract was discharged by deleting the
handler.** The loud half: the emitter wrote every trap path as a call to
`@npk_failsafe`, which nothing defined, and `llc` refused the result —
against D-013. The quiet half, the serious one and the same shape as DEF-4:
`reach_settle` returned at `failsafe_decl == 0` before the named-coverage
loop, so REACH-002 was asked of programs that had a handler and of nothing
that had none — import a raiser, call it, omit the `failsafe`, no diagnostic
at all. Blocks nothing of theirs (every shipped program has a handler; their
harness stops reading `npkc` exit 0 as well-formed). **Landed as 1.5.1b step
1b**: `NITPICK-REACH-003` at `main`, listing the identities the absent
handler owes; a root with neither stays legal; a `failsafe` outside the root
is step 1's RESOLVE-013.

**DEF-6 (their O-N14, raised 2026-09-04 by `nitpick-regex` 0.0.1, verified
by an independent verifier there and against this tree's step-3 compiler) —
a non-root module compiled alone was refused by `llc`.** `npkc` emits
`call i32 @npk_failsafe(...)` into every unit — the prelude's resume
scaffolding alone carries seven, so a comment-only module has them — and
never emitted a `declare` for it; only the program root DEFINES it (D-013).
So every module that is not a root compiled at `npkc` exit 0 and failed at
`llc` with `use of undefined value '@npk_failsafe'`, which made
BUILD_REFERENCE §4.1's "each module compiles to its own object" a model the
compiler could not deliver, for every library in the ecosystem (W-27: blocks
per-module objects and separate compilation; touches no rule or API). The
same subject as DEF-5 from the other end: the root supplies the handler,
everyone else declares it, and the compiler enforced neither half. **Landed
as 1.5.1b step 3c**: a unit whose root module does not declare `failsafe`
emits `declare i32 @npk_failsafe(i32)`, and an `object` stage in both
runners compiles every module under `tests/backend/objects/` alone to an
object `llc` must accept (a comment-only module and a library with trap
sites), its undefined symbols the runtime's and that one handler — §4.1
measured on every run, never documented alone again.

**DEF-7 (their O-N13, the same report) — a `pub use` after a plain `use` of
the same path re-exported nothing, silently.** `symtab_bind_import`'s
idempotent re-import branch returned the prior binding and discarded the
repeat's flags, `SYM_PUB` among them, so the same two lines meant different
things in the two orders and the consumer's "cannot find X" pointed a file
away from the cause (W-27: blocks nothing — the other order works — and costs
each person who meets it once, expensively; six umbrella modules planned
there). **Landed as 1.5.1b step 3c**: the prior binding takes the
visibility the repeat asks for; `tests/accept/reexport/` carries the
contrast (their §E2 against §E3) as a three-file accept unit, and
MODULE_REFERENCE's transitivity paragraph says order does not matter.

**O-N12 (raised 2026-09-03 by `nitpick-regex` 0.0.0 against `950bb1d`,
confirmed by the workbench from this tree; their W-27) — `>>>` and
`string_repeat` were documented and absent.** TYPE_REFERENCE's bitwise table
carried a `>>>` row ("right shift (unsigned)", `lshr`) beside `>>` ("signed",
`ashr`); the lexer has no `>>>`, and `ir_expr.npk`'s one shift arm already
emits `lshr` for an unsigned operand and `ashr` for a signed one — so the row
described a synonym that did not exist and mislabelled the operator that does,
which is what cost the reporter a probe. BUILTIN_REFERENCE §2 listed
`string_repeat` under a sentence calling the list "fast compiler intrinsics",
against the section's own header (the planned `nlibc` surface; only the
marked tables are builtins). Blocks nothing (W-27: `>>` on an unsigned operand
IS logical, measured at bit 63). **Settled the recommended way — the
documents, not the implementation — at 1.5.1b step 2**: the `>>>` row is
struck with a note (one operator; the operand's signedness decides), and §2's
sentence names the marked-table rows as the only names that resolve, with
the list kept as the unclaimed library surface it is (their call: no library
in the ecosystem builds string utilities today, and striking the name would
trade a fixed problem for a lost intention). Their
RX-111 (D-070's bounds check does not apply to a `wild T->` block — by D-070's
own title) is theirs and not a defect here; this tree's own D-070 citations
(VERIFICATION_REFERENCE's `bounds` row) say array, slice or buffer.

**DEF-8 (found by 1.5.1b step 5 on 2026-09-04, by `list_fds.npk` the day
`List<T>` began to own; latent since 1.2.3) — `pass` of a COPYABLE field
cleared the root's drop flag.** `clear_root_flag`, the emitter's half of
"`pass` moves implicitly" (D-183), cleared the root binding's flag for every
`pass` rooted at an owning local, whatever the passed value's type: `pass
h.n` over an `int64` left `h`'s `OwnedFd` undropped on every call; a
function returning `xs.count` leaked every `List`. The checker's own rule
(bindings.npk: nothing moves because it was passed) never agreed with it.
**Fixed in step 5**: the clear is gated on the passed value's type dropping,
after substitution — inside a generic body the recorded type is the
template's `T`, and the gate's first build freed every `List<string>`
element twice. `move(h.n)` and a nested `pass w.inner.n` gate the same way;
`pass_field.npk` pins all three under descriptor exhaustion. The
whole-binding rule for an OWNING projection is unchanged (its sibling leak
stays D-183's open partial-move item). Blocks nothing of the workbench's, and the reason is narrower than
first stated (their O-N16, 2026-09-04): their recipes DO pass copyable fields out
of locals (`pass self.count` out of a by-value `Vec<T>`), but a library's
hand-written container never drops — a `wild T->` and two counts own nothing to
the layout; only the prelude's `List<T>` is recognised as owning — so the local
whose flag the old clear cleared had no drop to skip. They are untouched
because their containers are outside the recognition, not because they do not
write the shape.

**DEF-9 (found the same day, by the reproduction of DEF-8 passing before the
fix) — every descriptor-exhaustion proof in the suite depended on the
machine's soft descriptor limit.** `overwrite_owned.npk` (1.1.12b),
`list_fds.npk` and `pass_field.npk` open a few thousand times and expect a
leak to surface as EMFILE, which happens only under a soft `RLIMIT_NOFILE`
near the Linux default of 1024; the development session sets 1,048,576, so
each of them passed against a leaking build. **Made an instrument in step
5**: `nitpick.toml`'s `[limits] nofile` (1024; 64–1048576) is one number
both runners lower their OWN soft limit to before spawning anything, so every
tool and program inherits it (`lib/nsys.npk`'s `prlimit64` pair for `npkg`,
`resource.setrlimit` for the harness); a hard limit below it is refused by
name before anything runs; both print the ceiling they run under; and
`fd_ceiling.npk` opens until refused into a `List<OwnedFd>` and requires the
count under the ceiling — run by hand under the session's default it exits 5,
measured thirty times. BUILD_REFERENCE §1 and §7.1 carry the table.

**DEF-10 (found by 1.5.1b step 5's first cumulative-prefix harness, 2026-09-04;
latent since 1.2.3, live since 1.1.12b's overwrite rule) — a `move` out of a
field dropped the moved-out value at the field's next assignment.** The
emitter's `move(place)` cleared the ROOT binding's flag (a partial move
treated as a whole one — every sibling leaked) and, for a root with no flag
(a pointer parameter), cleared nothing; D-186's field overwrite then dropped
the old value unconditionally, so `saved = move(r.env); r.env = move(frame);`
in `type_resolve.npk`'s constant folding freed `saved`'s list at its second
line and the restore put a freed block back. The compiler never saw it: its
`main` exits, and `exit` runs no drops. Three frontend unit tests
(`type_layout`, `type_generic`, `expr_types`) died with SIGSEGV out of
`npk_heap_bad`'s trap route over the corrupted heap. **Fixed in step 5 as
S-26**: a `move` or `pass` out of a field or element leaves the type's
canonical vacant value (D-225), the aggregate stays live, and a vacant List
grows from zero. `partial_move.npk`. Blocks nothing of the workbench's: a
library function that moves a field out of a struct it was lent and puts one
back was freeing the caller's value; their recipes do not do this (their
before-numbers stand).

**DEF-11 (found by 1.5.1b step 5's first cumulative-prefix harness, 2026-09-04)
— a `main` that released the heap and then returned.** `type_layout.npk`,
`type_generic.npk` and `expr_types.npk` ended `main` with `wild_release_all();
pass 0i32;`; until step 5 nothing in `main` dropped, so the return was inert.
With `List<T>` owning, the scope-exit drops of their resolvers ran over
unmapped memory; the runtime refused the free (`npk_small_check`'s live-magic
edge, the block's chunk gone and remapped) and the trap route then faulted on
the same heap — SIGSEGV instead of a controlled stop. **Fixed in step 5 as
S-27**: TYPE-062 requires the statement after `wild_release_all()` to be
`exit`; the three tests exit, `argv_after_release`/`leak_cleanup` measure
inside `exit`'s operand, `npkg` decides its code before releasing, and the
stray second call 45 test files carried is gone. Blocks nothing of the
workbench's; a library never calls the release.

**DEF-12 (found with DEF-11, 2026-09-04) — the trap route died after
`wild_release_all()`.** The main thread's TLS block, which `npk_exec` reads
through `%fs:8` on every trap's way to `failsafe`, was an internal heap
allocation, so the release unmapped it and any trap raised afterwards — the
refused free in DEF-11, or D-210's overflow inside `exit`'s own operand, the
one place TYPE-062 still lets code run after the release — was a
segmentation fault, an uncontrolled stop. **Fixed in step 5**: the block is a
raw mapping (`npk_hmap`), in neither table, unmapped by nothing; the process
ends with it. `release_trap.npk` overflows inside `exit`'s operand after the
release and expects `failsafe`'s 93 (139 on the old floor, measured). D-151's
leak accounting never saw the block either way.

**DEF-13 (found by the close-out refresh's harness, 2026-09-04; latent since
1.2.3, exposed by S-26) — the diagnostic sort read slots it had moved out of.**
`diaglist_sort`'s walk-back read `list.items[k-1]` after the shift had moved
those elements up, which the old bit-copy `move` tolerated (the bytes stayed)
and the vacate does not; the first compiler built by the step-5 emitter — the
refreshed snapshot, compiling `tools/check.npk` — put a zeroed diagnostic in
second place and lost the last one, and five rejection tests changed their
verdict at once while every program, the selfhost fixpoint and `repro` stayed
green. **The lesson is the instrument**: byte identity of stage 2 and stage 3
proves the compiler compiles ITSELF consistently, not that the tools it builds
behave as the old builder's did; the refresh's own harness — tools and the
compiler under test built by the NEW snapshot — is the first run of that
compiler's semantics over the suite, and it is the proof a refresh needs. The
seed README says so now. **Fixed as step 5c**: the sort carries its hole the
way an insertion sort does — one move out, neighbours read through borrows,
each shift one move up, one fill — correct under either meaning of `move`.

**DEF-14 (found by 1.5.2's PLANNING on `8dbef43`, 2026-09-04, by a
twelve-line probe; latent since 1.5.0 step 3; not the workbench's, recorded
here because this is the defect list the `src/` writer reads) — the 1.5.0
encoder keeps a stable symbol for a local whose address a call takes, so a
definition of another local in terms of it outlives the write, and a
division guard was DISCHARGED that the program then defeats.**
`bind_define` gives an address-taken name a SYMBOL and withholds only its
DEFINITION (`smt_encode.npk:307-326`, "an address-taken local is never
defined"), so two reads of `x` around `bump(@x)` are one term `|x.1|`, and
`int32:a = x + 1i32;` is the fact `(= |a.1| (+ |x.1| 1))` that the call
falsifies. For `int32:a = x + 1i32; int32:z = raw bump(@x); int32:q = 100i32
/ (x - a);` z3 answers `unsat` for the `div-zero` row (the divisor is
provably −1 under the surviving definition); a manifest carrying that row
fed to `npkc --elide` emits `sdiv i32 100, %t17` behind an `llvm.assume`
with no `-4097` trap; linked and run, the verified build dies with
**`Floating point exception (core dumped)`, exit 136** — where the plain
build exits 21 through `failsafe`'s `DivByZero` arm. The uncontrolled stop
the language exists to prevent, produced by the verified build. An
escaped PARAMETER has the same hole one line earlier: `enc_param` runs
before `enc_body`'s `collect_escaped`. **Fix scheduled as 1.5.2 step 0
(L-0): an escaped name is never NAMED** — no symbol, every read a fresh
opaque term with the type's range axiom; the escape set computed before
the parameter loop; `tests/verify/divz_escaped.npk` (`div-zero open 1`,
`expect-exit 21`) and `divz_escaped_param.npk` pin it. **FIXED at 1.5.2 step
0 (2026-09-04)**: the compiler's own 141 rows, re-decided with the pre-fix
and the fixed compiler and joined by (symbol, ordinal, kind), moved NOT ONE
verdict and re-hashed not one row — no cone of the compiler's own set
mentions an address-taken integer local — so `nitpick.obligations` is
byte-identical and no `--record` was needed. Blocks nothing of the
workbench's (no library runs the verified build); `1.5.2.md` §4.3 carries
the full measurement and the record its numbers.

**DEF-15 (found by 1.5.2b's PLANNING on `20976d1`, 2026-09-05, by a
fourteen-line probe; latent since 1.0.4b, when family impls landed) — a
family impl's bound is DECLARED AND NEVER ENFORCED.** `find_method` and
`type_implements` (`type_trait.npk`) match `impl:<T: Ord>:Box<T>:Ord` to
`Box<Point>` by DECLARATION identity and never ask whether `Point: Ord`
holds; `blanket_applies` is the only place an impl's bounds are read, and
only for the blanket form. The checker accepts `Box<Point>.cmp(…)` with
`Point` implementing nothing; the emitter's `impl_decl_for` finds no impl,
qualifies the method by the trait's module and emits a call to
`@npk.prelude.Point:Ord.cmp`, which nothing defines — **`llc` refuses the
IR** ("use of undefined value"). An accepted program the compiler cannot
compile, and D-064 §1's promise broken on the instantiation side. The
prelude's two bounded family impls never met an unsatisfying argument, so
nothing noticed. **Fix scheduled as 1.5.2b step 1 (L-1, L-2)**: a family
impl applies to an instance only when its bounds hold, decided where the
impl is USED (never eagerly — an instantiation is not refused for an impl
it does not use), and the call site reports TYPE-017 naming the impl and
the bound (the derive, when the impl is derived). Without it D-253's
synthesized bound is a comment. **FIXED at 1.5.2b step 1 (2026-09-05,
D-256)**: `family_unify`/`family_fit` in `type_trait.npk`, read by
`find_method`, `type_implements`, `bind_blanket`, the `dyn`-coercion lookup
and the emitter's `note_family_instance`; `tests/types/rejection/family_bound.npk`
and `tests/backend/programs/family_bound_ok.npk` pin both directions, and the
non-positional target shapes positional binding could never serve run.

**DEF-16 (the same planning; latent since 1.0.4c) — the no-bound operator
story admits programs the emitter cannot lower.** `#[derive(Eq)]
struct:Box<T>` writes `self.v != other.v`, the checker admits `!=` on an
opaque `T`, and at `Box<Point>` the emitter meets `!=` on a struct:
`NITPICK-EMIT-002 <derived-1>:3:12` — "a defect in the compiler", reported
against source nobody wrote. **Fixed by 1.5.2b step 3 (L-4, L-5)**: the
body reaches `T` through `eq`, the impl carries `T: Eq`, and `Box<Point>`
needs `Point: Eq` at the frontend. **FIXED at 1.5.2b step 3 (2026-09-05,
D-258)**: P7 is refused at the call, TYPE-017 naming the derive
(`tests/types/rejection/derive_bound.npk`).

**DEF-17 (the same planning; the D-250 class, wider than comparisons) —
derived `Hash` over a NAMED field is TYPE-042 inside `<derived-1>` (`raw`
on the field's may-fail `hash`; `derive_hash.npk` never nests), derived
`Clone` over an OWNING field is TYPE-047 inside `<derived-1>` (`pass self`
of a lent owner), and derived `Hash`/`ToString`/`Debug` over a generic
subject are TYPE-019/036 inside `<derived-1>`.** **Fixed by 1.5.2b step 3
(L-4, L-5, L-6)** — one rule for every member — and step 4 (L-7) re-homes
whatever still reports inside a derived file. **FIXED at 1.5.2b steps 3 and 4
(2026-09-05, D-258/D-259)**: `derive_hash` over a named field, `Clone` over a
`string` field (`derive_clone_owning.npk`) and the seven derives over
`Box<int32>`/`Box<Point>` (`derive_generic.npk`) run; what remains a checker
verdict reports at the derive (`tests/derive/rejection/rehomed.npk`).

**DEF-18 (the same planning; a soundness hole) — a derived `Clone` over a
generic subject aliases an owner.** `impl:<T>:Box<T>:Clone` is `pass self`
checked with `T` opaque, so TYPE-046/047 cannot see the `string` inside
`Box<string>`; the specialization copies the header and both drops free
one body. Measured: a clone inside a returning function exits 95
(`Unreachable` reached `failsafe`) where the arithmetic answer was 0 — the
allocator's instrument caught the second free. In `main` before `exit` it
is invisible (`exit` runs no drops). **Fixed by 1.5.2b step 3 (L-6)**: a
member-wise clone under `T: Clone`; `derive_generic.npk` carries the
returning-function shape as the regression. No program in the tree writes
the shape today. **FIXED at 1.5.2b step 3 (2026-09-05, D-258)**: the
returning-function shape sums to 8 where it exited 95.

**DEF-19 — SETTLED by the user as D-260 (2026-09-05: "lets go with your
recommendations on those"), the recommendation as written; LANDED at 1.5.2c
step 0 (2026-09-05): `NITPICK-TYPE-065` at the selector in both forms,
`tests/types/rejection/pick_optional.npk`.** ~~(found by 1.5.2b step 2's probes on `f090e44`, 2026-09-05; owner:
the `src/` writer, a DECISION first)~~ — a `pick` over an `Optional` whose
arms are the INNER type's patterns is admitted by the checker and refused by
the emitter. Eight lines reproduce it: `func:pc = Ordering?(int32:a, int32:b)
never fails { if (a < b) { pass Ordering.Less; } pass Ordering.Equal; };`
and in `main` `Ordering?:o = raw pc(1i32, 2i32); pick (o) { (Ordering.Less)
{ exit 0i32; }, (*) { exit 3i32; } }` — `tools/check` accepts the program
and `npkc` answers `NITPICK-EMIT-002 …:5:16: the emitter could not lower
this, although the frontend accepted it`. The suite's own spelling is `pick
(o ?? Ordering.Equal)` (`derive_payload.npk:46`), which is what a program
should write; the hole is that the checker never says so. Not a soundness
defect (the compiler refuses, it does not miscompile) and outside 1.5.2b's
scope, so recorded rather than fixed in it. **Recommendation:** decide the
rule — either a `pick` over `T?` matches `T`'s patterns only through `??`
(then the checker refuses the bare form by name, TYPE-0xx, "pick over an
`Optional` needs `??` or a `NIL` arm") or the language admits the bare form
with a mandatory `(NIL)` arm (then the emitter lowers it and PICK's
exhaustiveness counts the arm). The first is the smaller language and the
one the suite already writes; the plan's recommendation is the first.

**DEF-20 — SETTLED by the user as D-261 (2026-09-05: "lets go with your
recommendations on those"), the recommendation as written: generic enums are
IN; LANDED at 1.5.2c step 1 (2026-09-05): `generic_enum.npk`,
`generic_enum_infer.npk`, `derive_generic.npk`'s returned `Opt<T>` section,
`type_generic.npk` ge1..ge3. Found and fixed in the same step: the `pick`
EXPRESSION form typed no arm binding (both spellings now run one
`type_pick_rules`; `pick_expr_bindings.npk`).** ~~(found by 1.5.2b step 3's tests on `a9fff07`,
2026-09-05; owner: the user, a DECISION first)~~ — a GENERIC ENUM parses and
means nothing.
`enum:Opt<T> = { Some(T); None; };` is admitted by the parser (the generics
window is the item's, as a struct's is) and then `T` in the payload is
"there is no type named `T`" (TYPE-001): the resolver binds a struct's own
parameters before reading its fields and never an enum's before reading its
variants; and `Opt<int32>:o = Opt.Some(3i32)` finds "found `Opt`", the bare
declaration -- a variant constructor never instantiates (TYPE-007). No
generic enum exists in `tests/`, `src/`, `lib/`, `npkg/` or `tools/`, and
none is mentioned in TYPE_REFERENCE, TRAITS_REFERENCE or AST_REFERENCE: the
form was never decided in or out, which is D-085's "parses and means
nothing" shape. 1.5.2b's plan wrote a generic-enum derive test (`Opt<T>`
under `Eq`/`Ord`/`Clone`) on the assumption that the form exists; the derive
generator is written for it (a variant pattern and constructor use the BARE
enum name, 1.3.7's rule), the test item is dropped with this note, and the
first generic enum to be written will exercise it. **Recommendation:** decide
it IN -- an enum with a payload of a parameter type is the shape every
`Optional`/`Result`-like user type takes, and the struct machinery it needs
(the own-generics binding at resolution, instantiation of the constructor
from the annotation's arguments; `check_one_instance` already accepts a
TY_ENUM instance) is small -- and schedule it before 1.5.3 lowers contracts
over enums; deciding it OUT means the parser refuses the window on an enum
by name.

**DEF-21 — FIXED at 1.5.2d step 2b (2026-09-05).** (Found by the library
workbench, `nitpick-libs_s1`, on the 1.5.2c close `0dfddac`; owner: the
`npkg` and harness writer.) The undefined-symbol allowlist (`npkg/elf.npk`'s
`runtime_allowlist`, the harness's `runtime_allowlist`) was wrong in both
directions: built from every `define` of `runtime/npkrt.ll`, `internal` ones
included — 57 names the object does not export, so a program naming one passed
the scan and failed at `ld.lld`, where D-206 wants the named refusal — and
missing the two `.globl` names the `module asm` block declares (`_start`,
`npk_clone_raw`), so a program legitimately calling `npk_clone_raw` was refused
as an undefined symbol outside the allowlist: a false refusal of a legal
program, the worse half. The workbench's arithmetic identified the object as
the authority: 109 non-internal defines plus 2 `.globl` names are exactly the
object's 111 GLOBAL symbols. Fixed as stated in both runners (`runtime_exports`:
non-`internal` defines plus `.globl` names; the harness's allowlist is 112 with
`main`), with `allowlist-exported`/`allowlist-globl`/`allowlist-internal` in
both self-checks and BUILD_REFERENCE §4.1's prose corrected. The two halves
shipped together.

**DEF-22 — FIXED at 1.5.2e step 2 (2026-09-05): `emit_member_from`'s `TY_ARRAY` arm answers the constant; `fixed_array_len.npk` holds the three shapes.** ~~OPEN~~ (found by the library workbench, `nitpick-time`, its O-N18, on
`0dfddac`, 2026-09-05; owner: the `src/` writer, small). `.len` on a
FIXED-SIZE ARRAY `T[N]` is accepted by the frontend and refused by the emitter
as `NITPICK-EMIT-002` ("a defect in the compiler rather than in this program
— report it"), at a local `uint8[20]` and at a module `fixed uint8[3]` alike.
Two controls place it at `.len` on the array TYPE: a slice `uint8[]` asking
`.len` compiles and writes, and a local `uint8[20]` indexed without `.len`
compiles and writes. The checker's member table admits `.len` for the array
kind; the emitter's `emit_member_from` has no `TY_ARRAY` arm for it, where the
answer is the constant `N` the type carries. Blocks nothing (the workbench's
buffer names its bound as a constant); the fix is the arm plus a program pinning
`.len` on a local and a `fixed` array. Reproduction:
`nitpick-time/tests/probe/defect/fixed_array_len/`.

**DEF-23 — FIXED at 1.5.2f step 1 (2026-09-05, under D-264; S-40).** Found by
the library workbench (`nitpick-time`, its O-N19) on every pin: TYPE-046 was
not enforced on a bare type parameter inside a generic body — a copy of an
owning element compiled, linked and ran, two owners of one heap body, a
use-after-free that exited 0 (170 when the poison was read). Mechanism:
`require_move_if_owning` returned early unless `type_drops` was true, which it
is not of an unsubstituted `T`. Fixed as D-264 states; `generic_owning_copy.npk`
holds the workbench's controls, `generic_owning_move.npk` the spelling.

**DEF-24 — FIXED at 1.5.2h step 0 (2026-09-06; found the same day, planning
D-266).** Found by reading TYPE-063 (D-251) as the rule a view binding would
mirror: a limited binding refuses `@`, `$$i` and `$$m`, but NOT the implicit
address a method or UFCS call takes when its receiver — the first parameter —
is a pointer (`type_members.npk`'s "a pointer receiver is taken by address",
1.0.9b). Measured on `0ba21ef`: `limit<r_px> Pt:p = Pt{ x: 1i32 }; drop
p.bump();` with `bump = NIL(Pt->:p)` writing `p.x = 0i32 - 5i32` under
`Rules<Pt>:r_px = { $.x > 0i32 };` compiles, runs, violates the rule and traps
nothing — exit 7 on the read `p.x < 0i32` — while `drop bump(@p);` is refused
TYPE-063 at the `@`. Under the verification leg such a binding's `limit` row
is discharged and its guard elided while the program defeats it (DEF-14's
class). Fix: the receiver-address decision asks what `@` asks
(`place_limited`), TYPE-063; `limit_receiver.npk` holds both spellings and
the by-value control.

**DEF-25 — FIXED at 1.5.2i step 1 (2026-09-06; reported the same day by the
library workbench, `nitpick-regex`'s cycle-0.0 audit).** `string_concat` of
two empty strings leaks one block per call: `@npk_string_concat`
(`runtime/npkrt.ll`) allocates `n = al + bl` unconditionally, `@npk_alloc_impl`
hands a real 16-byte block for a zero request (D-150), and the result
`{p, 0, 0}` carries cap 0 — the not-mine bit — so its drop frees nothing.
`@npk_string_slice` has carried the empty branch since D-186. Measured on
`c81efa5`: `string_concat("", "")` in a bare loop under `ulimit -v 65536` exits
92 (`HeapOom`) at 8,000,000 calls where `string_concat("", "a")` exits 0; under
`NPK_HEAP_STATS` at 1,000,000 calls, `allocated=16000000 peak_live=16000000`
against `allocated=1000000 peak_live=1`. The prelude's `impl:string:Clone`
(`string_concat(self, "")`) and the compiler's own copy idiom (234 sites in
`src/`) leak the same way on an empty operand. Fix: the slice's branch in the
concat; `tests/cost/empty_concat.toml` holds the empty loop's peak to the
one-byte loop's.

**DEF-26 — FIXED at 1.5.4 step 0 (2026-09-06). (Found by 1.5.4's PLANNING on
`b2f7d94`, 2026-09-06, by a probe; latent since 1.5.0) — a guard site inside
a `pick` EXPRESSION arm has no obligation row.** `expr_children`'s `ExprPickExpr` arm lists only the
selector ("a value-pick's arms are statements") and nothing walks those
statements, so a division inside `pick (s) { (1i32) { give 100i32 / d; }, … }`
emits its `-4097` trap and records no row — P-12's rule ("every guard site has
a row") broken, the manifest not the inventory it claims to be. Measured: a
`main` with three divisions, one in a pick-expression arm, emits three traps
and two rows. Not a soundness hole (the guard stays); invisible because no
verify test and no function in `src/` has the shape (the trap-by-group belt
would have failed the verify stage on either). Fix (1.5.4 step 0): the arms
walked as the statement form's are, through one helper both forms share;
`tests/verify/pickexpr_div.npk`.

**DEF-27 — FIXED at 1.5.4 step 0 (2026-09-06). (The 1.5.3 handoff's recorded
weakness; confirmed at 1.5.4's planning) — a callee's parameter name that
collides with an ADDRESS-TAKEN caller local binds unnamed in `enc_under`**, because `bind_define` consults
the caller's escaped set (DEF-14) for a substitution binding that is nothing
of the caller's; the call-site `requires` row is `open` and the callee's
`ensures` hypothesis absent. Fail-closed and weak, never unsound. Fix (1.5.4
step 0): a substitution binding never consults the escaped set;
`tests/verify/req_collide.npk`.

**DEF-28 — FIXED at 1.5.4 step 0 (2026-09-06). (Found by 1.5.4's PLANNING on
`b2f7d94`, 2026-09-06, by a probe) — a non-positive LITERAL step of a counted
loop compiles and traps at run time.**
D-022 and CONTROL_REFERENCE §2.4: "a negative step is a compile error; so is a
zero step". `till(3i32, 0i32) { … }` passes the checker and exits 38
(`BadStep`): the rule was never implemented and the runtime guard is the only
enforcement. Fix (1.5.4 step 0): `NITPICK-TYPE-068` at the literal (`TYPE_LOOP_HEAD`);
the computed step's guard becomes the `loop-step` row under S-46;
`tests/types/rejection/loop_head.npk`.

**DEF-29 — FIXED at 1.5.4 step 0 (2026-09-06). (Found writing the step's fix)
— the compile-time evaluator's counted loops disagreed with D-022.** `fold_counted`
(`src/frontend/type_resolve.npk`) took a loop's DIRECTION from the step's sign
(`i = i + step`, `i < hi` unless `step < 0`) and read a two-argument head as
`loop(lo, hi)` and a one-argument one as `till(hi)`. So a descending
`loop(10i64, 0i64, 1i64)` folded to ZERO iterations where the runtime runs
ten, and `till(limit, step)` folded as a loop from `limit` to `step`: a
`comptime func`'s value differed from the same function's run-time value.
Invisible because a comptime function is never emitted (`emit_program.npk`
skips `DECL_COMPTIME`) and the corpus used the evaluator's own vocabulary
(`tests/accept/folding.npk`'s `loop(1i64, n + 1i64)`). Fix (1.5.4 step 0): the
head read by kind, the direction from the bounds, a non-positive step refused
with its own sentence; `tests/backend/programs/comptime_counted.npk` holds a
comptime sum and its run-time twin equal.

**DEF-30 — FIXED at 1.5.4 step 0 (2026-09-06). (Found with DEF-29) — the
counted loop's argument COUNT was never checked.** `check_counted` typed however many arguments the head held; a
two-argument `loop` and a one-argument `till` (the struck do-while reading,
`till (i >= n)` in `tests/analysis/rejection/moves.npk`) typed clean and died
at the emitter as EMIT-002 with no span. Fix (1.5.4 step 0): `loop` takes
three, `till` two, TYPE-068 on the statement otherwise (DEF-28's code, its
second way to fail).

**DEF-31 — FIXED at 1.5.4c (2026-09-09; D-273 landed in three steps: the qualified call is a direct call, `use` over a module path binds through the file forms' binders, an alias and a `pub mod` carry their scope, `std` is owned; `inline_mod.npk` exits 7 through `hidden.fetch(3i32)`; the record is `1.5/1.5.4c.md`). (found at 1.5.4 step 4, 2026-09-06, by the rung suite's retirement;
owner: the user — a language question; **SETTLED as D-273, 2026-09-07 — "I agree with your recommendation so ratify that" — lands at 1.5.4c**) — an inline module's members cannot be
reached from the module that declares it.** MODULE_REFERENCE §1 says modules
"can be defined inline" and nested (`mod:core = { mod:math = { … }; };`) with
`pub` visibility, and the checker has a sentence for the qualified spelling
(`hidden.fetch(3i32)` is TYPE-007 "`hidden` is a module, not a value"), but no
spelling resolves: `use hidden.*;` and `use hidden.fetch;` are RESOLVE-002
("cannot find `fetch` in this scope") and the qualified access is refused. An
inline module's code is emitted (the descent 0.7.7's audit hole was about) and
unreachable. Found because `tests/rejection/inline_mod.npk` — which observed
the descent through a rung refusal — became a positive program that wanted
to CALL the function. Which spelling should reach it (the qualified
`hidden.fetch`, a logical-path `use hidden.*;`, both) is the user's; the
resolver and the checker then implement it, and `inline_mod.npk` exits
through the call.

> **Measured and recommended (2026-09-07, `da1f911`; `nitpick-compiler_s2`).**
> The qualified spelling is the DESIGNED one and is half-built: the checker's
> `namespace_member` (`type_members.npk`) already resolves `m.name` through
> the scope a module symbol opened — a function member types as its function
> type, a global as its type — and the resolver's comment says `math.sqrt`
> was to be told apart from `p.x` there. What fails is the CALL: `m.f(x)`
> parses as a METHOD call, whose typer types the receiver as a value, so
> `hidden` reports TYPE-007 before the namespace is asked. **The alias form
> has the same hole**: `use "./lib.npk" as lib;` binds `lib` as a module
> symbol (MODULE_REFERENCE §2.1's canonical "Namespace (Alias)" import), and
> no test calls through it — `lib.f(x)` is refused exactly as `hidden.f(x)`
> is. `use hidden.*;` is a logical path, which the loader leaves to the
> driver as the standard library's business, so nothing binds. Symbol names
> already nest (`@"npk.two_inline.a.go"`, `.b.go`, `.go` for three `go`s in
> one file — measured, compiles and links), so no naming work is owed. Uses
> in the tree: none outside seven test files (macro-scope semantics, D-239's
> owned names inside modules, RESOLVE-013's inline half, the emission
> descent); the library workbench has none in 170 `mod` uses.
> **Recommendation:** finish the designed spelling, as one subcycle (1.5.4c),
> and make a module symbol mean one thing wherever it stands: (1) a call
> whose base names a module — an inline module, an alias, a nested path
> `core.math.f(x)` — is a DIRECT call of that member, recorded as the callee
> and lowered with no receiver (the method-call node's receiver is a
> namespace), a `pure never fails` member admissible in a contract as any
> named function is; (2) a `use` path whose first segment names a module
> symbol in scope binds that module's public names in every form the file
> forms have (`use hidden.*;`, `use hidden.{f, Point};`, `use hidden.f;`,
> `use core.math.*;`), which is the only way an inline module's TYPES are
> named from outside; a path whose first segment names no module symbol
> stays the standard library's (`std.…`), and `std` joins the names a
> program cannot declare as a module (D-239's table); (3) a private member is
> RESOLVE-004's "private to" from either spelling, and a `pub mod` is an
> exported symbol a file's wildcard import binds, so the qualified call
> works across files too; (4) `inline_mod.npk` exits through the call. The
> alternative — strike the inline form and keep `mod:` meaning exactly
> "this file is" or "load that file" (D-248's "a module is a file") — costs
> the alias path's (1) all the same, deletes ~21 walker arms and rewrites
> seven tests, and removes the one namespace construct D-088 kept when it
> struck `Type:Name = { }`; nothing in the tree would miss it today, which
> is the only argument for it. Owner: the user.

**DEF-32 — FIXED at 1.5.4d step 0 (2026-09-10; the record is
`1.5/1.5.4d.md`). (found 2026-09-10 by planning 1.5.4d's first test, on
`1ef034a`; latent since the reach analysis was written at 1.1.6, D-179) —
the reach analysis recorded a constant's qualifier from the FIRST SITE that
reached it, not from the file that declares it.** `reach_operand` registered
an error constant with `cur_module`, the module being walked, and
`reach_add_decl` kept the first registration; the root is walked first, so a
root that unwrapped an imported constant — `?! pe.E` through an alias, or
`?! E` after `use "./perr.npk".{E};` — recorded `root.E`, `failsafe` was
told "does not name `root.E`" and the CORRECT `(perr.E)` arm (D-179: the
FILE that declares a constant qualifies it) was refused, while the demanded
`(root.E)` would have hashed to a code no constant has and matched nothing
— the exhaustive-`failsafe` guarantee was hollow for any cross-file constant
named in the qualified spelling (measured: three probes; the bare `(E)` arm
was unaffected because it matches by symbol origin, which is how every
program in the tree stayed green). Every qualified arm in the tree names a
constant whose first site is inside its own file. Fixed by the one table:
the resolver records (declaration, qualifier, code) on the `SymbolTable` as
it assigns each code (`symtab_add_error`), and the reach analysis reads the
qualifier from it (`symtab_error_qual`); `cur_module` and `reach_module`'s
name parameter are gone. `mod_file_import.npk` holds both shapes.

**DEF-33 — FIXED at 1.5.4b step 2 (2026-09-10; the record is
`1.5/1.5.4b.md`). (found 2026-09-10 by writing step 2's `err-exit` recorder
against `record_shift`'s shape, on `44992e4`; latent since 1.5.4b step 0,
`11e423f`) — a guard's fact was pushed as a hypothesis wherever a contract
clause was encoded, so a `requires` row could discharge from its own
clause.** `record_shift` recorded the `shift-range` row and pushed its goal
— the amount inside `0..width-1` — as a fact after the site: right for the
body's own walk (every continuing path passed the compare), wrong in TWO
places. Under `quiet` — the mode a contract clause is encoded in for a
call-site row and a rule's clauses for an instantiation — the row is
suppressed (L-1) but the push was not, so `requires n < 32i32 requires (x <<
n) != 0i32` at a call had `0 <= n < 32` asserted in the CALLER's context,
where nothing had checked it: the call-site row proved `n < 32` from itself,
the verified build named the callee's `.body` past its checked entry
(D-252), and a call with `n = 41` ran the body its contract forbids. And
LOUD — a `requires` at the function's own entry, an `ensures` at a seam, an
`invariant` at a loop head are encoded for their own rows with their guards
as the function's sites (L-5) — the fact sat in the contract row's own cone,
and the check that runs the guard (`.req`, the seam's check, the head's) is
exactly what that row's discharge removes: `requires (1i32 << n) != 0i32`
alone was discharged of itself, `.req` elided, and `g(41)` ran unchecked.
Measured (`tests/verify/req_shift.npk`): step 1's compiler reads `requires
discharged` at both calls and at `g`'s entry; step 2's reads `open` at all
four `requires` rows and at the three shift rows. No row of the compiler's
own manifest moved (no contract clause in `src/` shifts). Fixed by one
rule, which the `err-exit` recorder was then written under: under a quiet
encoding a guard records nothing and pushes nothing; inside a loud clause
(`in_clause`, set by `contract_conj` and `invariant_conj`) it records its
row and pushes nothing (`record_shift`, `record_err_exit`).

**DEF-34 — FIXED at 1.5.4b step 2 (2026-09-10). (found 2026-09-10 by step
2's first probe — a `tryte` parameter's balanced bound `-29524` as a
numeral; latent since `smt_int` was written at 1.5.0, reachable since 1.5.4b
step 1 folded a `fixed` global into its numeral) — `smt_int` spelled a
negative numeral's magnitude as `0u64 - (v =>! uint64)`, an unsigned
underflow D-210 traps, so the compiler DIED under `--obligations` and
`--elide` on the first negative numeral any row carried.** A body reading
`fixed int32:NEG = -4i32` — `100i32 / (x + NEG)` — exits 3 (a trap inside
the compiler, `smt_int` at the top of the backtrace) with step 1's compiler;
no test had one, and the compiler's own sources fold no negative constant
into a row. Fixed: `|v|` is `(0 - (v + 1)) + 1` computed wide, exact at
`INT64_MIN`. `tests/verify/neg_numeral.npk` holds the constant and the
pattern (DEF-35).

**DEF-35 — FIXED at 1.5.4b step 2 (2026-09-10). (found 2026-09-10 while
reproducing DEF-34; latent since the `pick` lowering at 0.9.7) — a negated
literal pattern `(-1i32)` was admitted by the checker and the exhaustiveness
analysis, encoded by the walk (1.5.4 step 1's `enc_pattern_value` has the
arm), and refused by the EMITTER as `NITPICK-EMIT-002`: `pattern_const`
folded a literal, a bool, a char, an error constant and a variant tag, and
not a `-` over a literal.** Fixed with the arm (`ir_stmt.npk`);
`neg_numeral.npk`'s `sign` runs it and discharges a division under it.

**DEF-36 — SETTLED as D-285 (user, 2026-09-10: "I am fine with the three recommendatins you made for the decisions"), LANDED at 1.5.4e step 1 (2026-09-11). (found 2026-09-10 by 1.5.4b step
2's first full harness, `tests/verify/req_shift.npk`) — the runners' trap
belts count a guard's trap by its TEXT, `@npk_trap(i32 -4097)`, and a
program's own `r ?! DivByZero` lowers to the same text.** The `?!` operator
traps with the CALLER's code (0.9.7): `?! E9` with a user constant's hash,
`?! DivByZero` with `-4097` — exactly the spelling the belts count as a
`div-zero` guard, so a verified build of a program that unwraps with a
system error whose code a guarded kind uses (`-4097`, `-4098`, `-4100`,
`-4101`, `-4111`…`-4115`) fails the belt as "N traps for 0 retained" in
both runners. Conservative (a false red, never a false green) and rare,
but a legal program shape the belts cannot tell from a guard. The test
unwraps with `E9` for now, as every other verify test does. **Recommended
fix:** the `?!` lowering calls a distinct floor entry (`npk_raise`, one
line in `npkrt.ll` tail-calling `npk_trap`, added to the exports allowlist
and the §2d emitter-only rows) so a raise and a guard differ in the text
the belts read — a D-203 floor addition, hence the user's; the belts then
count `@npk_trap(` alone and nothing a program spells reaches it.

**DEF-37 — FIXED at 1.5.4b step 4b (2026-09-10). (found 2026-09-10 by
1.5.4b step 3's tests, measured at the close on `044a04d`) — the reach
analysis demanded a `DivByZero` and a `DivOverflow` arm of every `failsafe`
in a program whose only division was a float one.** A float `/` or `%` is
IEEE and total (D-143): the emitter writes a bare `fdiv`/`frem`, nothing
traps, and the demanded arm was one nothing can enter — the twisted
families' case, suppressed since 1.3.2 with the float family left out. Both
sites (the expression and the compound `/=`/`%=`) read the float kind now,
and a `simd` division reads its element's kind first (integer lanes keep
the any-lane guard and the arm, float lanes arm nothing).
`accept/float_total.npk`, `rejection/reach_simd_div.npk`; the three float
verify tests dropped the arms they carried.

**DEF-38 — SETTLED as D-284 (S-59; user, 2026-09-10: "I am fine with the three recommendatins you made for the decisions"), LANDED at 1.5.4e step 0 (2026-09-11). (found 2026-09-10 by 1.5.4b step 4b, on `044a04d`) — a `simd` integer lane's
`+ - *` WRAPS on overflow where its scalar traps (D-210).** `emit_simd_binop`
writes a bare vector `add`/`sub`/`mul`; measured: a lane holding `INT_MAX`
plus a lane holding one read back negative (exit 5), no trap, no arm
demanded (the reach analysis's `IntOverflow` gate is `TY_INT` alone, and
its comment's "`simd` rides its element's rule" describes nothing the
emitter does). The fix under S-59's recommendation is small and measured
there: the vector `with.overflow` intrinsics any-lane, the arm armed, a
program that traps, and 1.5.8's `overflow` rows over the lane conjunction.

**DEF-39 — FIXED at 1.5.4e step 0 (2026-09-11) under D-284 (found 2026-09-10 by 1.5.4e's
planning, on `18b93e1`) — a compound assignment on a `simd` target is
admitted by the checker and refused by the emitter as EMIT-002.** `v += w`
on a `simd<int32, 4>` (and every `op=`) types, and `ir_stmt`'s compound
path hands it to `emit_arith_value`, which dispatches no vector — the
expression form's vector arm sits in `emit_binary` alone — so the statement
dies as an internal defect. The rule of 1.3.3 (both spellings through one
core) was kept for scalars and never reached the family. Fixed by the one
dispatch: `emit_arith_value` sends a `simd` operand type to the vector
binop, which carries the division, shift and (D-284) overflow guards; the
encoder records the compound form's any-lane rows at the target's site.

**DEF-40 — FIXED at 1.5.4e step 1 (2026-09-11) under D-285 (found 2026-09-10 by 1.5.4e's
planning, on `18b93e1`) — the `!!!` statement bypasses the trap entry.**
`emit_trap` lowers `!!! e` to a chain reset, a direct `@npk_failsafe(e)` and
`@npk_exit` — never `@npk_trap` — so a `!!!` sets no frozen flag (D-063), no
re-entry guard (a `failsafe` that traps after it runs `failsafe` again where
the trap route ends at 70) and kills no driver (D-188's registry walk is on
the trap path). Under D-285 it lowers to `npk_raise`, the program's entry to
the one trap route; `trap_stmt_reentry.npk` pins the 70.

**DEF-41 — FIXED at 1.5.4e step 0 (2026-09-11; found by that step, on
`c63807d`) — a compound shift through a field or element (`s.f <<= n`) had
its `ShiftRange` guard and no obligation row.** The emitter has guarded the
compound spelling since D-277 (1.5.4b step 0) for every target; the
encoder's field-target branch recorded the division's row for `s.g /= n`
and nothing for the shift, so a verified build carried a `-4115` trap the
belts could not account for — measured with a twelve-line probe (one trap
in the IR, no row in `rows.txt`). The scalar row (`record_shift`) and, for
a `simd` place, the any-lane row are recorded there now;
`verify/shift_field_compound.npk` pins it, and the division beside the
shift shows the shift's fact at work (its `div-min` discharges: after the
guard, `n` is in `0..31`).

**DEF-42 — FIXED at 1.5.5 step 0 (2026-09-11) under D-004 rule 3's own text (found
by 1.5.5's planning, 2026-09-11, on `cb8cbb0`) — a borrow assigned to a
holder declared OUTSIDE the referent's block is accepted.** `int32->:p =
NULL; if (true) { int32:x = 41i32; p = $$m x; } … <-p` passes the checker
and every analysis and exits 42: `escape_assign`'s bare-local branch marks
the holder and compares no scopes ("A BARE LOCAL NAME, and nothing else"),
so rule 3's "not provably shorter-lived" is unchecked for a local holder.
Benign at run time BY CONSTRUCTION — every local is a function-lifetime
alloca (D-173) and an address-taken local is frame-resident to the
function's end in a coroutine — and unstated as a rule; the aliasing
lifetimes of S-60 assume a holder never outlives its root. The fix, landed: the
holder's declaring scope may not be an ancestor of the root's (or of a
copied holder's) — `check_holder_scope` in `escape_assign`'s bare-local
branch, over every root the value carries (`collect_local_roots`: the
`@`/`$$` operands, a holding binding, a view-maker's place, a pick
expression's `give` values, the generic children); BORROW-002 with its own
sentence; `rejection/borrows.npk` gains `outer_holder` and `outer_copy`,
`accept/borrows.npk` `same_block_holder` and `inner_holder`.

**DEF-43 — FIXED at 1.5.5 step 1 (2026-09-11) under D-287 (S-62; TYPE-071) (found by 1.5.5's planning, 2026-09-11,
on `cb8cbb0`) — a write through a pointer to a `fixed` binding is accepted,
and through a `fixed` global it is an uncontrolled crash.** `fixed
int32:x = 1i32; int32->:p = $$m x; <-p = 2i32;` compiles and overwrites `x`;
`int32->:p = @G; <-p = 3i32;` with `fixed int32:G = 1i32;` at module level
compiles and dies with SIGSEGV (exit −11) — `G` is an LLVM `constant` in
read-only memory (D-211), the store faults, and `failsafe` never runs. The
definite-assignment analysis owns `fixed` (ASSIGN-002, a second
ASSIGNMENT) and no analysis sees a store through a pointer; the pointer
type carries no mutability. Measured: 89 `fixed` bindings in the tree,
none address-taken. The fix is the rule S-62 recommends (TYPE-071: a
`fixed` binding has no address), at 1.5.5 step 1.

**DEF-51 — FIXED at 1.5.6 step 4 (2026-09-11) (found by that step, on the
b4 worktree, reading `npk_read_file`'s growth to specify it; RENUMBERED from
DEF-49 at step 5, 2026-09-11 — step 0 had already declared DEF-49 for the
root's owner word, and the library listener's board caught the collision from
the two notices. The later declaration moves, never the earlier: step 4's
landed commit message and its execution record carry the old number with a
dated note, because a pushed citation is not rewritten) — `read_file`
and `read_stdin` leaked every buffer they outgrew.** Both double the buffer
from 64 KiB (`npk_alloc_internal`, the managed heap's untracked entry), copy
the bytes into the new block and continue -- the old block, owned by nobody
after the copy, was never freed: a 256 KiB file cost 64 + 128 + 256 KiB of
dead storage per read, invisible to D-151 (which counts `wild` blocks) and
to every test until a cost unit measured it. Measured on the b3 floor with
`tests/backend/programs/read_big_many.npk` (fifty reads of a 256 KiB file,
each result dropped) against `read_big_once.npk`: `peak_live` 23,724,055
against 1,245,207 -- twenty-two times the one read's. Fixed in the floor:
`grow` returns the superseded buffer through `npk_dalloc` after the copy;
`peak_live` 1,048,599 in both, and `tests/cost/read_file.toml` holds the
many-reads peak within twice the one read's on every run. Found by
specification: the spec's frame over the caller's objects refused to prove
until the growth's memory was read closely.

**DEF-44 — FIXED at 1.5.6 step 0 (2026-09-11) under D-290 (found by 1.5.6's planning, 2026-09-11, on `149dbf6`) — a frame's `windup` word is stored plain across
threads and loaded plain at every resume.** `npk_windup_all` stores `1` into
slot 2 of every task on the join list from the JOINER's thread (its own
comment rouses "another thread's executor"), and `npk_step` loads the word
with a plain `load i32` before the `seq_cst` compare-exchange on `wake_at`
that would order it; the ordering the comment claims holds only for a task
the sweep found sleeping. LangRef: a non-atomic load that races with a
store returns `undef` — the value D-218 (10) banned from the emitted IR,
minted by the floor at run time. The fix is a `release` store and an
`acquire` load (one instruction each on x86; the model changes, the
behaviour does not).

**DEF-45 — FIXED at 1.5.6 step 0 (2026-09-11) under D-290 (found by 1.5.6's planning) — a channel's generation is read plain outside the lock that writes
it.** `npk_ch_get` loads slot 6 with a plain load before taking the channel
lock (the early stale check) while `npk_ch_reclaim` and `npk_ch_open` store
it plain under the lock; a handle resolved on one thread against a reclaim
on another is a racing read whose `undef` feeds a compare both of whose
outcomes are safe today (the lock's re-check follows), which is why nothing
ever failed. `monotonic` on the three accesses.

**DEF-46 — FIXED at 1.5.6 step 0 (2026-09-11) under D-290 (found by 1.5.6's planning) — `@npk_frozen` is stored plain by the trapping thread and loaded
plain by every executor.** The word D-063 rests on ("after a trap nothing
is resumed, anywhere") is read racily by exactly the threads it is meant to
stop (`npk_step`, `npk_frozen_get`). `seq_cst` on the store and the loads.

**DEF-47 — FIXED at 1.5.6 step 1 (2026-09-11) under D-291 (found by 1.5.6's
planning) — two threads trapping at once ran two `failsafe`s, and a
trap during another thread's `failsafe` exited the process mid-safing.**
The `trap-route` model (1.5.6 step 5) is the standing evidence: its
`two-failsafes`, `step-after-failsafe` and `exit-mid-failsafe` predicates
are unreachable at K 14 / D 6, and its `old-arbitration`, `no-stop` and
`old-loser-exits` controls -- the pre-step-1 route, exactly as it was --
reach all three, which is what makes the proof mean something.
`npk_trap`'s `@npk_in_failsafe` is a plain load-then-store: both threads
read 0, both store 1, both kill the drivers and both run `failsafe`
concurrently; a later trapper reads 1, takes the re-entry arm and
`exit_group(70)`s while the first `failsafe` is still safing. D-063's
"other threads stop before `failsafe` gets control" is not implemented —
`@npk_frozen` stops the next resume on every executor and nothing stops a
task that is running when the trap happens. The measurement the model
records: the `trap-route` model's bad predicates are `sat` on today's
floor.

**DEF-48 — FIXED at 1.5.6 step 0 (2026-09-11) under D-290 (found by 1.5.6's planning) — `npk_thread_join` reads the child's `CHILD_CLEARTID` word with a
plain load in its wait loop.** The kernel writes it; a futex word is read
atomically everywhere else in the floor (glibc's `lll_wait_tid` reads it
atomically for the same reason). No optimiser runs over `npkrt.ll` and the
trampoline's asm clobbers memory, so nothing hoists the load today; the
finding is the model's and the fix one word (`monotonic`).

**DEF-49 — FIXED at 1.5.6 step 0 (2026-09-11) under D-290 (found by step 0's
classification of the `owner` word, 2026-09-11, on `0e742ce`) — a thread's
root task was never roused: its `owner` word named the SPAWNING thread's
executor.** The emitter stamps a frame's `owner` (slot 11) where the frame
is born, with the creating thread's executor — right for a task, which D-032
never migrates — and nothing re-stamped a thread's root, which runs on the
executor `npk_thread_start` builds. Every wake of a root blocked on a
channel, lock, condvar or barrier (`npk_ch_wake_one`'s rouse) set the
PARENT's park word and woke the parent's futex; the child's executor slept
to the root's own deadline and found the value then — the right answer late,
the class the 1.4.4 join fix closed, invisible to every test because the
suite's thread roots send with room or wait with deadlines shorter than the
test's patience. Measured: a root receiving five values sent 10 ms apart
with a 3 s recv deadline took over 15 s (`thread_root_wake.npk` exited 43).
`npk_thread_start` stamps the root's `owner` with the executor it built,
before the clone that publishes it; the program exits 42 in under a second.

**DEF-50 — FIXED at 1.5.6 step 0 (2026-09-11) under D-290 (found by step 0's
classification of the globals, 2026-09-11, on `0e742ce`) — the executable-page
count was a plain read-modify-write across threads.** `npk_wildx_alloc` and
`npk_wildx_free` incremented and decremented `@npk_wildx_live` with a plain
load, add and store, under no lock: two threads allocating or freeing JIT
pages at once lost updates, and the count feeds the exit-time leak check
(D-151) — a program that freed every page could exit through `WildLeak`, and
one that leaked a page could exit 0. Both are `atomicrmw` now and the exit
read is atomic; the same step moved the heap initialiser's unlocked call in
`npk_wildx_alloc` and `npk_aalloc` under the heap mutex, where
`npk_alloc_impl` had always taken it. `wildx_threads.npk` churns 600 pages
across two threads and exits 0 only when the count agrees.

**DEF-52 — FIXED at 1.5.6b step 0 (2026-09-17): the mask is zeroed by one
`llvm.memset` before the kernel sees it; `runtime/tests/hwconc.ll` exits 0 where
it exited 1 (the floor answering 1008 on a 48-thread machine), and the new
`alloca-not-defined` belt holds the class in both runners.** ~~OPEN~~ (found
2026-09-17 on `b7d60dc` by `nitpick-compiler_s7`, taking up lead E-4 of §2g) — `npk_hardware_concurrency` popcounts 120
bytes of UNINITIALISED STACK.** The floor hands the kernel an `alloca [16 x
i64]` it never zeroes (`runtime/npkrt.ll` ~1841), asks `sched_getaffinity(0,
128, mask)`, and popcounts all sixteen words. The RAW syscall writes only as
many bytes as the kernel's own cpumask and returns that count — it is glibc's
wrapper that zero-fills the rest, and the floor has no glibc. Measured on this
machine with a sentinel-filled buffer: the call returns **8**, the kernel
writes bytes 0..7 (48 bits set, the machine's 48 threads) and **leaves bytes
8..127 untouched — all 960 sentinel bits survive**. So the symbol answers 48
plus however many bits the last deeper call chain left in 120 bytes of stack:
a value never written, D-227's family, and in LLVM's terms a load of storage
nothing initialised. **Unreachable today** — `emit_program.npk:968` DECLARES
the symbol in every module and nothing calls it; no builtin reaches it — which
is the cheapest moment to find it. Its `(boundary "…")` sentence promises "the
popcount of the sched_getaffinity mask over 1024 bits, at least 1", which the
code does not keep. **What hid it from 1.5.6's method:** the kernel-effect row
for syscall 204 (`npkg/floor_smt.npk` ~3097) says the kernel writes `arg2` —
the REQUESTED 128 bytes — so a specification of this symbol would have been
decided with the garbage tail read as kernel-written. The fix is the floor's
(bound the popcount by the returned count, or zero the buffer first — D-203:
reviewed IR), with the row corrected to `result` and a program that calls the
symbol once something can; `npkrt.o`'s digest moves, so the notice names both.

**DEF-53 — FIXED at 1.5.6b step 0 (2026-09-17): all eight hoisted, and D-173's
own check (`check_allocas_hoisted` / `ir_allocas_hoisted`) run over
`runtime/npkrt.ll` in both runners -- one line each, no second mechanism.**
~~OPEN~~ (found 2026-09-17 on `b7d60dc` by `nitpick-compiler_s7`, auditing
DEF-52's class over all fifteen of the floor's allocas) — D-173 WAS NEVER EXTENDED TO
THE FLOOR: eight of its fifteen allocas sit outside their entry blocks, two of
them inside loops.** D-173 (SETTLED, 1.0.9a: "allocas are hoisted to the entry
block") was written when the seed-built compiler segfaulted on exactly this,
and `check_allocas_hoisted` / `ir_allocas_hoisted` have held it over every
EMITTED module since (the compiler's own emission: 60,776 allocas, none outside
an entry block). The belt was never pointed at the hand-written
`runtime/npkrt.ll`; pointed there unchanged, it reports eight. An alloca
outside the entry block is DYNAMIC -- it moves the stack pointer each time it
executes and nothing returns the space before the function does. Measured
under the pinned flags (an `alloca i64` in a loop body, 4,000,000 iterations):
**SIGSEGV at `llc -O0` and at `llc -O2`.** Two of the eight are inside loops:
`npk_windup_all`'s `%onew` -- 16 bytes of stack per unfinished child on a
reactor-armed executor, in the function that runs when a join's deadline has
ALREADY EXPIRED, the degraded state -- and `npk_thread_join`'s `%ts`, 16 bytes
per return of its futex wait (a wake, EINTR, a spurious return). A child
thread's stack is 2 MiB: 131,072 iterations from the guard page, then a
SIGSEGV inside the runtime with no `failsafe` -- the uncontrolled stop. Remote
in count, exact in kind. One more is bounded at 16 a call (`npk_park_sleep`'s
`%tmp8`, in the event loop) and five execute once a call. The fix hoists all
eight and adds ONE LINE to each runner (D-173's own check, over the floor) --
a rule written for one spelling of a construct was owed to the other.

**DEF-54, DEF-55, DEF-56 — FIXED at 1.5.6b step 4c (2026-09-17), each in the one
function named below, each with a program that runs the shape and a control
that still refuses: `fn_value_pattern.npk` (exit 42; `llc` REJECTS the module
under the compiler as it stood), `fn_field_raw.npk` + `raw_field_rules.npk`,
`trait_fn_param.npk` + `trait_callback_sig.npk`.** ~~OPEN~~ (found 2026-09-17 on `129d56f` by
`nitpick-compiler_s7`, all three by writing D-296's probes and its rejection
test; owner: the compiler seat, 1.5.6b step 4c). THE FUNCTION-VALUE CORNER:
three places where a rule written for one shape of a function value was never
given to its twin.**
- **DEF-54 — a call through a function value bound by a `pick` PATTERN emits a
  direct call of a symbol that does not exist.** `(Op.Run(f)) { raw f(); }` is
  admitted by the checker, and `emit_call` (`src/backend/ir/ir_expr.npk`)
  decides "indirect" by ENUMERATING the binding kinds it knows — a local
  (`SYM_STMT`), a parameter — and falls through to the direct-call path for
  everything else: a pattern symbol (`SYM_PAT`) is read as a declaration and
  the module says `call { i64, i32 } @"npk.prelude.f"()`. `llc` rejects it:
  an internal defect reaching the user as an assembler error, for ANY name.
  `emit_pipe` beside it decides the right way round (direct ONLY for a
  declared function, a value otherwise).
- **DEF-55 — `raw o.f(x)` over a `never fails` function-typed FIELD is
  TYPE-042, while `raw (o.f)(x)` — the same call in its other spelling — is
  accepted.** `unwrap_licence_of` (`type_expr.npk`) reads the recorded callee
  FUNCTION TYPE for a `CallExpr` and only a method DECLARATION for a
  `MethodCallExpr`; a field call has no declaration, so the licence answers
  "not `never fails`" about a callee whose type says it is. `drop` shares the
  function and the defect.
- **DEF-56 — a trait method with a function-typed parameter can never be
  implemented:** TYPE-014, "does not have the signature the trait declares",
  about two signatures spelled identically. `tt_func` interns the parameter
  window's START INDEX (an allocation artifact, known since 0.9.6:
  `types_agree` in `type_expr.npk` compares function types STRUCTURALLY for
  that reason), and `same_after_self` (`type_trait.npk`) — the impl-versus-
  trait comparison — has no function-type case, so two spellings of one
  function type never match. Any function-typed parameter or return, `never
  fails` or not.

**DEF-57 — FIXED at 1.5.7 step 4 (2026-09-18), in `npk_step`'s `frozen:` block:
an executor that sees the frozen flag and is not the failsafe holder PARKS, and
the holder takes the re-entry exit 70. `trap_one_failsafe.npk` keeps seed 371
(X-11) and gives `Unreachable` its own exit (72), so the schedule names the
failure if it ever returns; the `trap-route` model gained the error code it
lacked (`wrong-error` and `holder-parks`, each with a control).** ~~OPEN~~ (found
2026-09-18 on `9c8baf2` by `nitpick-compiler_s8`, by step 4's own full harness —
the explorer's FIRST floor find; fixed in step 4's landing by
`nitpick-compiler_s10`) — AN EXECUTOR THAT WATCHED A TRAP COULD WIN THE FAILSAFE
HOLDER WITH `Unreachable`. `npk_trap` stores `@npk_frozen` (D-063: resume
nothing) and THEN claims `@npk_in_failsafe` by cmpxchg (D-291 (3)). `npk_step`'s
`frozen:` block is older than D-291 and answered the flag by calling
`npk_trap(-4102)`, which entered the arbitration with a code of its OWN. In the
two-instruction window between a trapper's store and its claim, another
thread's executor that began a step read the flag, trapped `Unreachable` and
could WIN. `failsafe` then ran with `Unreachable` where the fault was the
trapper's `DivByZero`, and the real trapper lost and parked (D-291's loser
rule, working as written). A `failsafe` that keys its shutdown on the error,
which is what it exists to do, took the wrong branch. Deterministic under the
explorer: seed 371 of `trap_one_failsafe` (the late thread traps first and is
preempted at its claim; the early thread's executor sees the flag on its first
step). Seeds 370 and 372 exit 41, 40 stress runs never reached the window, and
`trap_two_threads` has the same shape but its 1,000 seeds did not land there.
The `trap-route` model could not see it, because it had no error code: all four
of its bad predicates (two failsafes, a step after failsafe, an exit
mid-failsafe, a blocked failsafe) stayed unreachable. **The fix keeps D-291's
text and `npk_trap`'s order** (the freeze first, as (4) states). The watcher is
D-291's "any other loser": it parks, and the winner's stop walk signals and
counts it. The holder itself keeps the re-entry exit 70 — its own `failsafe`
driving the executor, which no program can do (`failsafe` is never `async`,
TYPE-043) — because a holder that parked would end nothing. The fix as first
proposed (claim before publishing, every watcher parks) would have parked the
holder as well; the model's `frozen-parks-holder` control reaches
`holder-parks` with exactly that change.

**DEF-58 — FIXED at 1.5.8 step 1 (2026-09-18; D-306): the ordered compare before the conversion traps `CastRange`, scalar and `simd` any-lane, and REACH arms it at both cast spellings (its first draft read `=>` alone and armed nothing — found by the step's control); `cast_range_fold.npk` is this defect's own probe, exiting 40 at both levels.** ~~OPEN~~ A FLOAT'S `=>!` CAST TO AN INTEGER WAS LLVM POISON. (found
2026-09-18 on `e3bf48c` by `nitpick-compiler_s11`, planning 1.5.8 — by reading the lowering `cast-range` was to
elide, and finding none.) `emit_cast`'s float→int arm, written at 0.9.4, is a bare `fptosi`/`fptoui`, and
`emit_simd_cast`'s is the same per lane. For NaN, ±∞ or a value whose truncation the target cannot hold the result
is POISON — not a truncation and not any number — and a branch on it is undefined. Measured with
`flt64:f = 3000000000.0; int32:i = f =>! int32;` then `if (i < 0i32) { exit 3i32; }`: exit 3 at the pinned `-O0`,
exit 9 after `opt -O2`, which folded the whole of `main` to `unreachable` so that the binary ran into the next
function. Over a runtime value both levels exit 3 — the hazard is whatever the optimiser can see, which is why no
test of the running program found it and the harness's `-O2` leg could only have met it on a folded constant. The
twisted families' ENTERING arms carry an ordered range guard since 1.3.2 ("the select keeps fptosi off poison");
the plain integer's arm was never given the rule, and `smt_kinds.npk`'s header claimed "casts trap (D-148)".

**DEF-59 — FIXED at 1.5.8 step 2 (2026-09-19; D-305): every emitted function carries LLVM's split-stack prologue, the floor's `__morestack` is `StackExhausted`, every thread's stack is the floor's (the main thread moved onto 8 MiB), `failsafe` runs on a stack of its own; `npkc` compiles itself under `ulimit -s 1024`, and `stack_exhausted.npk`, `stack_budget_own.npk` and `stack_failsafe_overflow.npk` exit 60, 0 and 70 where the previous compiler and floor died of SIGSEGV (139).** ~~OPEN~~ A STACK OVERFLOW WAS AN UNCONTROLLED STOP. (found 2026-09-18 on
`e3bf48c` by `nitpick-compiler_s11`, planning 1.5.8.) The floor installs one signal action, SIGUSR1's (D-291), so a
recursion deeper than its stack dies of SIGSEGV with the kernel's default action and no `failsafe` — the event the
language's two exit paths exist to forbid. Measured on the compiler compiling itself: `ulimit -s 4096` succeeds,
`ulimit -s 2048` exits 139. And the main thread's budget was the SHELL's: the same program stops at a different
depth under a different `ulimit`.

**DEF-60 — FIXED at 1.5.8 step 2 (2026-09-19; D-305): the prologue compares the WHOLE frame against the limit before allocating it (`leaq -0x10028(%rsp), %r11; cmpq %fs:0x70, %r11` for `stack_bigframe_thread.npk`'s 64 KiB frame), so a frame larger than the guard traps `StackExhausted` instead of stepping over it.** ~~OPEN~~ A SPAWNED THREAD'S ONE GUARD PAGE COULD BE JUMPED. (found
2026-09-18 on `e3bf48c` by `nitpick-compiler_s11`, planning 1.5.8.) `npk_thread_start` maps "guard page + 2 MiB";
`llc -O0 -stack-size-section` over the compiler's own IR finds 151 frames larger than a page (the largest,
`emit_expr_kind`, 120,904 bytes). An overflow that enters such a frame moves the stack pointer PAST the guard, and
the frame's writes land in whatever mapping lies below — the stack-clash class: silent corruption of another
thread's stack or the heap, not even DEF-59's crash. The main thread was spared by the kernel's own 1 MiB guard gap.

**DEF-61 — FIXED at 1.5.8 step 1 (2026-09-18): every float↔integer conversion past 64 bits is integer arithmetic in the emitter (`emit_f2i_wide`, `emit_i2f_wide`); `cast_wide.npk` holds eighteen known answers through `int128`…`int4096` at both levels, and no optimised object has an undefined symbol.** ~~OPEN~~ A FLOAT↔128-BIT INTEGER CAST COULD NOT BE LINKED. (found 2026-09-18 on
`e3bf48c` by `nitpick-compiler_s11`, probing DEF-58's neighbourhood.) `flt64 =>! int128` and `int128 =>! flt64` are
admitted (`CAST_LOSSY`) and lower to `fptosi`/`sitofp` at `i128`, which `llc` turns into compiler-rt calls
(`__fixdfti`, `__floattidf`) at both levels: the zero-dependency scan refuses the link by name. Loud, never unsafe —
but the language admitted what the backend could not build. Routing through `i256` (which LLVM expands inline) is
not a fix: `opt -O2` narrows `sitofp (sext i128 to i256)` back to the `i128` libcall (measured). The fix lowers
every float↔integer conversion above 64 bits by hand (1.5.8's K-4).

**DEF-62 — FIXED at 1.5.8 step 0 (2026-09-18; the heading said "OPEN, fixed at" until 1.5.8's close): `npkg`'s VERIFIED-BUILD BELT NEVER COUNTED `BorrowOverlap`.** (found
2026-09-18 by the identity survey of 1.5.8's planning.) `npkg/verify.npk`'s list of trap codes the belt counts is a
hand list of nine (`-4097 -4098 -4111 -4112 -4113 -4114 -4101 -4115 -4100`); 1.5.5 step 2 added `disjoint`'s
`-4116` to the kind tables of both runners and to this list in the Python twin only, which derives its codes from
`TRAP_OF_KIND`. The twins agreed on every verdict because every program's `disjoint` traps did match its rows;
had they not, the Python runner would have failed the unit and `npkg` passed it — a belt present in one twin and
absent from the other, which `parity` sees only as a verdict difference. The fix derives `npkg`'s list from its
own `trap_of_kind`, as the Python one is.

**DEF-63 — FIXED at 1.5.8 step 0 (2026-09-18; the heading said "OPEN, fixed at" until 1.5.8's close): THE FLOOR TRANSLATOR'S HEAP TRAP CODES WERE CROSSED.** (found 2026-09-18 by
the same survey.) `npkg/floor_smt.npk:3252-3254` maps `@npk_heap_bad`, `@npk_heap_badreq` and `@npk_heap_oom` to
−4103, −4103 and −4104; the floor traps −4102, −4104 and −4103 (`npkrt.ll:4292, 4304, 4298`). No spec clause reads
`trap_code` yet, so no verdict rests on the table — a latent wrong fact in an instrument, corrected before a clause
can rest on it.

**DEF-64 — FIXED at 1.5.8 step 2b (2026-09-19): the census reads `module asm` in both runners. A syscall's number comes from the `mov $N, %eax` immediately before it (`floor-asm-syscall-unread` otherwise); a `module asm` line the census cannot read is `floor-asm-line-unread`. TCB.md's membership table lists the seven `module asm` symbols, `trusted (module asm)`. Its syscall table counts 29 numbers, `rt_sigreturn` among them, with rows for `npk_clone_raw`, `npk_sigreturn`, `__morestack`, `__morestack_non_split` and `_start`, and `npk_thread_start` reaches `clone` and `exit`, which it always did. The explorer states each one it cannot route in `runtime/explore/unrouted.txt`, held to the census (`explore-asm-unlisted`/`-stale`/`-malformed`). Seven self-check cases in each runner.** ~~OPEN, fixed at 1.5.8 step 2b (split from step 2 on 2026-09-19 so the stack check could meet its harness sooner): THE FLOOR'S SYSCALL CENSUS COULD NOT SEE `module asm`.~~ (found 2026-09-18
by the explorer survey of 1.5.8's planning.) The kernel-effect table, TCB.md §4b's syscall boundary and the
explorer's transform all read `call i64 @npk_sys6(i64 N` in function bodies; a syscall written in a `module asm`
block is invisible to all three. One already is: `rt_sigreturn` (15), the SIGUSR1 handler's restorer (D-291) — no
kernel-effect row, not among §4b's "27 numbers". Step 2 adds `module asm` of its own (the `__morestack` stubs, the
stack trampolines), so the census reads `module asm` from then on.

**DEF-65 — FIXED at 1.5.8 step 1b (2026-09-19): the join releases a joined thread's stack mapping.** ~~OPEN~~
(found 2026-09-19 by `nitpick-compiler_s11`, reading `npk_thread_start` and `npk_thread_join` to plan 1.5.8's stack
step.) A JOINED THREAD'S STACK WAS NEVER RELEASED. `npk_thread_start` maps "guard page + 2 MiB" through `npk_hmap`
and nothing ever unmapped it: every spawned thread kept its mapping and every page it touched until the process
ended. Measured with a program that spawns and joins N threads in turn, each touching about 350 KB of its stack:
maximum RSS 3,456 KB at N = 20 and 142,080 KB at N = 400 -- linear, never returned. D-073 removed `Thread.detach`
because it "returns success and leaks a 2 MiB stack"; the join itself did the same, for every thread. The fix
records the mapping's base and length in the thread's TLS block (fields 6 and 7, written before the clone) and the
join unmaps it once the kernel has cleared the tid word -- from `mm_release` on, the thread runs no user code
again. `thread_stack_release.npk` caps its own address space at 192 MiB and spawns and joins 300 threads: 0 on the
fixed floor, `HeapOom` (92) linked against the previous one.

**DEF-66 — FIXED at 1.5.8 step 3b (2026-09-19): per-slot pools.** Each registry slot has one trampoline block and one executor, reborn for every thread in the slot and never freed. The slot is reserved first. The join unmaps the stack and closes the thread's epoll set BEFORE it retires the slot, and the eventfd stays with the pool entry, at most 64 ever. Measured: 700 reactor-using threads spawned and joined in turn exit 0 under the runners' 1,024 descriptors, where 510 died with `Unreachable`; 1,600 threads peak at 104 bytes of live heap, where they peaked at 371,304 (`tests/cost/threads.toml`). ~~OPEN, scheduled as 1.5.8 step 3b: A JOINED THREAD'S TLS BLOCK, EXECUTOR AND REACTOR DESCRIPTORS ARE
NEVER RELEASED.~~ (found 2026-09-19 by `nitpick-compiler_s11`, designing DEF-65's fix.) Beside the stack, each
spawned thread allocates a TLS block and an executor (`npk_alloc_internal`, never freed) and, once it has waited on
I/O, an epoll descriptor and an eventfd its executor never closes -- about 150 bytes and two descriptors per
thread, for the process's life. The descriptors are the functional hazard: under the runners' `nofile` of 1024 a
program that spawns reactor-using threads in turn exhausts descriptors after some five hundred. Freeing them at
the join is NOT safe as written: D-291's stop walk reads a published slot's TLS block without a lock (a walker
that read the slot's state before the retire would read freed memory), and a channel waker that popped a frame
before its task finished holds the owner executor's address past the thread's exit. The design the step carries:
the TLS block and the executor come from static per-slot pools -- the thread registry's fixed 64 slots make them
bounded -- reused with the slot, never freed, so a stale walker reads valid memory (at worst the next thread in the
slot, which the walk should stop anyway) and a stale waker's writes land on a reused executor's atomic words (a
spurious wake, which the clear-then-recheck protocol already tolerates); the reactor's two descriptors ride the
pooled executor and are reused rather than closed. The arguments go into `runtime/npkrt.spec` beside the
classification rows and the pools' reuse into the models the waker and the stop walk already have, before the step
lands.

**DEF-69 — FIXED at 1.5.8 step 3c (2026-09-19): A STANDARD DESCRIPTOR CLOSED AT STARTUP.** (found 2026-09-19 by
`nitpick-compiler_s11`, reading the reactor for step 3b.) Descriptor numbers 0, 1 and 2 are the kernel's lowest,
and a process started with one of them closed hands that number to the next descriptor anything creates. Two faces,
both measured on step 3b's floor:
- **Data corruption.** A program started with stderr closed (`2>&-`) that creates a data file gets descriptor 2
  for it. Everything meant for stderr then lands in the file. Measured: the file held the program's one byte `D`
  followed by the floor's `heap: allocated=8 peak_live=8 count=1` line (`NPK_HEAP_STATS`). A program's own
  `std_err()` writes do the same.
- **The reactor's sentinel.** The executor reads epoll descriptor 0 and eventfd 0 as "not created", and 0 is a real
  descriptor when stdin is closed. Traced: `epoll_create1` = 0, the eventfd added to it, then at the next
  registration a SECOND `epoll_create1` = 4. The first set leaks, with the eventfd still in it.

The fix: at startup, before anything creates a descriptor, `npk_start` checks 0, 1 and 2 (`fcntl` F_GETFD) and
opens `/dev/null` onto any that is closed. The kernel gives the lowest free number, which is the one checked; any
other answer traps −4102. And the reactor's "absent" becomes −1 at every site, since a program may still close its
own stdin later: the main executor, the pool entries (at boot, before any thread), the six readers, and the join's
store.

**Fixed as planned, with one change of means.** `npk_std_fds` runs right after `npk_tls_boot` (a refusal takes the
trap route, which reads the TLS block). The main executor's initializer and every reader and writer of the two
reactor words use −1 for "none": the join tests `>= 0` and stores −1, `npk_io_register` tests `>= 0` for the set and
for the kept eventfd, `npk_park_sleep` is armed when the set's word is `>= 0`, and the two rousers and
`npk_io_unwatch` skip when their word is negative. **The pool entries get their −1 from a STATIC initializer, not
from a loop at boot**, which is what this entry said. The eventfd word is atomic everywhere (D-290), and 64 atomic
stores before `main` would be 64 counted steps in every explored run, moving every schedule — DEF-67's cause. The
initializer is sixty-four identical entries (every other word 0), and needs no step. The kernel-effect table gained
`72 fcntl`, held to `F_GETFD` by its option set. Measured, each test against step 3b's floor by hand:
`std_fds_closed` (the program re-executes itself with stderr closed) exits 7 there, the data file holding the
diagnostic, and 0 here. `reactor_fd_zero` (the program closes its own stdin) exits 10 there — the join left the
thread's epoll set, descriptor 0, open. A variant without that case exits 14 there — the second wait made a second
set. The test against this floor with only `npk_park_sleep`'s test reverted exits 82 after exactly one second: a
ready pipe was answered at its deadline, because the idle wait on a set that is descriptor 0 was the futex's.

**DEF-68 — FIXED at 1.5.8 step 3 (2026-09-19): A WRITE TO A PIPE WITH NO READER KILLED THE PROCESS, WITH NO
`failsafe`.** (found 2026-09-19 by `nitpick-compiler_s11`, writing TCB.md §5's list of what D-307 leaves
uncontrolled.) The kernel raises SIGPIPE in a thread that writes to a pipe or socket whose read end is closed, and
SIGPIPE's default action terminates the process. The floor installed no action for it, so any program whose output
was piped into a reader that exits early (`prog | head`) died uncontrolled. Measured: a program writing 4 KiB blocks
to stdout, piped into `true`, exited 141 (128 + SIGPIPE). The Bridge had avoided it only on its own sockets, with
`MSG_NOSIGNAL` and a comment naming it "the exact event this architecture exists to prevent"; the floor's `write`
had nothing. The fix is one more action in `npk_fault_arm`: SIGPIPE → `npk_pipe_handler`, which returns (SA_RESTORER
| SA_ONSTACK | SA_RESTART), so the write answers EPIPE as a value. It is a handler rather than SIG_IGN because an
ignored disposition survives `execve` into every child the floor spawns (drivers, tools) and a caught one resets to
the default. `sigpipe_epipe.npk` makes a pipe, closes its read end and writes: 32 (EPIPE) on the fixed floor, 141 on
the floor before; the shell-pipe probe exits 3 (its own answer to the error) where it exited 141.

**DEF-67 — FIXED at 1.5.8 step 2c (2026-09-19): the explorer HOLDS. `hold-at:` sites (`NPKX_HOLD1..4`) hold the first thread to arrive until another passes the same site, or until nothing else can step. It is in both shims (held to each other hash for hash, 40 held runs), both runners' control readers and a self-check case in each. DEF-57's window is `frozen-traps.ctl`: 72 on 100 of 100 seeds with the pre-fix block planted and held; 41 on 100 of 100 blind; and on the FIXED floor, held, 41 on 100 of 100, the real floor's own evidence that the fix holds with the window open. A kept seed now claims nothing (VERIFICATION_REFERENCE §10).** ~~OPEN, scheduled as 1.5.8 step 2c: A KEPT SEED WENT STALE AND NOTHING SAID SO.~~ (found 2026-09-19 by
`nitpick-compiler_s11`, executing 1.5.8 step 2's K-11.) X-11 keeps a seed that found a defect (`// explore-seed:
S`) and runs it first on every run: "a schedule that found a defect once is the cheapest regression test the
project will ever own". That works only while the seed still names that schedule. Step 2 added one routed
`sigaltstack` per thread, which moved `trap_one_failsafe`'s k from 106,239 to 106,242. With DEF-57's pre-fix block
planted, seed 371 now exits 41, and so do all of seeds 1..60,000. The unit's run was green throughout, because a
kept seed is replayed on the fixed floor and never checked to still reach anything. Without K-11's by-hand
re-search, DEF-57's only real-floor regression would have stopped testing its window with no signal. Re-searching
is not the fix: the window is one point wide, about 1 in 150,000 blind seeds reach it, and every floor change that
adds a step moves it again. Nor can a directed control (X-15) reach it, because both parties' next step is the
same site. Step 2c gives the explorer a HOLD directive: the first thread to arrive at a named site waits until
another thread has passed the same site, or until nothing else can run. DEF-57's window becomes a `.ctl` found
without a seed and decided on every run. Step 2c also settles X-11 for every kept seed: each claims its defect
through a control that plants it, or it claims nothing.

**DEF-70 — FIXED at 1.5.8b step 2 (D-310; 2026-09-19): A NEGATED LITERAL IS EMITTED AS A CHECKED SUBTRACTION.** (found 2026-09-19 by `nitpick-compiler_s11`, reconciling a prototype's `overflow` rows with the emission's guards.) `-1i32` lowers to `llvm.ssub.with.overflow.i32(i32 0, i32 1)` and a trap branch, because every integer negation routes through `emit_arith_value` as `0 - x` (D-210 §1's rule, right for a variable). The compiler's own emission holds 84 such guards, and 85 with both operands constant. They are checks that can never fire, each inflating the guard count the verified build's belts must match. D-310 folds a constant pair, and a negated literal never overflows (a width's minimum has no positive literal, D-148).

**DEF-71 — FIXED at 1.5.8b step 2 (D-311; 2026-09-19): THE CONSTANT FOLDER WRAPS WHERE THE RUN TIME TRAPS.** (found 2026-09-19 by the library workbench, `nitpick-libs_s4`, measuring D-310's reach, and verified by `nitpick-compiler_s11`.) `fixed uint64:B = 0u64 - 1u64;` folds to 2^64−1, and the same subtraction at run time traps `IntOverflow`. One expression has had two meanings since D-210 landed at 1.4.2b: the folder kept D-037's wrap. D-148 prescribes that very spelling for `uint64`'s maximum, and so does LEXICAL_REFERENCE §6.2. Measured with a probe on 1.5.8 step 4's tree: `~0u64` equals the folded value, and the run-time subtraction exits 93. The folder obeys D-210 from step 2, with the refusal TYPE-076, and the maximum is `~0u64` (D-311).

**DEF-72 — FIXED at 1.5.8b step 1 (D-313; 2026-09-19): A COMPILER-KNOWN CONTAINER'S HEADER IS WRITABLE BY ANY PROGRAM.** (found 2026-09-19 by an Explore survey for D-308's length facts, CONFIRMED by `nitpick-compiler_s11`.) `.ptr`, `.len` and `.cap` of `string`, `cstring`, a slice and `buffer` are typed as assignable places (`type_members.npk`), accepted by `require_place`, and stored through a GEP by the emitter. Probe: `string:s = string_concat("abc", "def"); s.len = 4096i64;` then `string_slice(s, 4000i64, 4096i64)` SUCCEEDS, reading 96 bytes past a 6-byte block. No test exercised it. `string_from_bytes` (BUILTIN_REFERENCE says it "traps on misuse") checks nothing, and `#wild_slice`'s "legal only in `wild` context" is enforced nowhere (both are D-308 step 6's length checks).

**DEF-73 — FIXED at 1.5.8b step 1b (D-313; 2026-09-19): THE PRELUDE `List`'s FIELDS ARE WRITABLE BY ANY PROGRAM, AND NOTHING TIES THEM TO THE BLOCK.** (found and CONFIRMED the same day.) Probe: two one-element lists; `a.cap = 100i64`; forty `list_push(@a, …)` calls run past `a`'s block without reallocating and overwrite `b`'s element (exit 3). The generated drop walks `count` elements, so `l.count = l.cap + k` is a drop over memory the list does not own. `src/` writes `.count` directly about 150 times by text (truncations and pops of its own lists), and those move to checked prelude operations.

**DEF-74 — FIXED at 1.5.8b step 1b (D-314; 2026-09-19): `List` ELEMENT ACCESS IS UNCHECKED RAW-POINTER INDEXING, IN ANY MODULE.** (found 2026-09-19 by `nitpick-compiler_s11`, designing the prelude side of D-313, and confirmed.) The prelude offers `list_init`, `list_reserve` and `list_push` only, so every element access is `l.items[i]`, a `wild T->` index with no bounds check and no opt-out spelled where it is written. Probe: two one-element lists, then `a.items[1..5] = …`, overwrites `b`'s element (exit 3). Sites: `src/` 665, `npkg/` 1,011, `tests/` 22. The library workbench's 103 are all inside their declaring modules.

**DEF-72's fix** (1.5.8b step 1): every write form over `ptr`, `len` and `cap`
of a string, cstring, slice and buffer is `NITPICK-TYPE-079` in every module.
The forms are an assignment through any path, a compound assignment, `@`,
`$$m`, a `Self->` receiver and a stateful operation. `lenwrite` is a case of
`tests/types/rejection/header_writes.npk`. The two primitives' unchecked lengths
(`string_from_bytes`, `#wild_slice`) stay D-308 step 6's.

**DEF-77 — FIXED at 1.5.8b step 1 (D-313's walk; D-056): AN `RGuard`'s `.value`
WAS READ-ONLY ONLY AS AN ASSIGNMENT'S DIRECT TARGET.** (found 2026-09-19 by
`nitpick-compiler_s11`, reading the assignment check while building D-313's
write walk; confirmed by a probe.)
- The rule (D-056, 1.1.11b) asked whether an assignment's target WAS
  `g.value`. So `g.value.x = 5i32` (a part of the element), `@g.value.y` and
  `$$m g.value.x` were accepted: each a write through a SHARED read hold, the
  unsynchronized mutation the write lock exists to serialize.
- No test exercised any of the three.
- Fixed by asking the read-only view in D-313's one write walk at every write
  form, with the same code (TYPE-007) and sentence.

**DEF-78 — FIXED at 1.5.8b step 1 (D-313 §4; D-185): `@f.value` ON AN `OwnedFd`
WAS ACCEPTED.** (found 2026-09-19 by `nitpick-compiler_s11`, the same reading,
confirmed by a probe.)
- D-185's read-only `.value` was enforced for a direct assignment only.
  `@f.value` and `$$m f.value` were accepted, and a store through the pointer
  changes the descriptor the drop closes: one descriptor leaked, another
  double-closed.
- The identity word is SEALED BY DEFINITION now, beside the containers'
  headers, and refused at every write form as `NITPICK-TYPE-079`.
- The direct assignment moved from TYPE-007, so `stream_rules.npk` names 079.

**DEF-73's and DEF-74's fix** (1.5.8b step 1b):
- The prelude's `List` is `{ hidden wild T->:items; sealed int64:count;
  sealed int64:cap; }`. Outside the prelude, `items` is not touched (TYPE-080)
  and `count`/`cap` are not written (TYPE-079).
- Every element access is `l[i]`, bounds-checked against `count`.
- Every change goes through a checked operation: `list_pop`, `list_truncate`,
  `list_clear`, `list_insert`, `list_remove`, `list_swap_remove`, beside
  `list_push`/`list_reserve`.
- Both probes are cases: `tests/types/rejection/list_fields.npk` refuses them,
  and `tests/backend/programs/list_index_oob.npk` is DEF-74's probe written
  `a[i]`, trapping `OutOfBounds` at the first index past the end (it exited 3,
  another list overwritten).

**DEF-79 — FIXED at 1.5.8b step 1b: `ast_init` STORED THE NONE DECLARATION PAST
`count`, SO THE FIRST REAL DECLARATION SAT AT THE "NONE" ID.** (found 2026-09-19
by `l[i]`'s bounds check, on the first build of the swept compiler.)
- Every other AST array pushes its NONE node at index 0. `decls` stored it into
  slot 0 of an EMPTY list, `ast.decls.items[0i64] = declNone`, and left
  `count` at 0.
- The first real declaration was then pushed over it, at DeclId 0, which is the
  id `decl_is_none` reads as absent.
- Latent since 1.4.7 step 2, when `Ast` moved to `List`s. It was harmless in
  practice because that declaration is the prelude's header, which every walk
  skips, and so it was never noticed.
- The stage-2 compiler trapped `OutOfBounds` in `ast_init` on its first run.
  The NONE node is pushed now.

**DEF-80 — FIXED at 1.5.8b step 2 (K-7): THE CONSTANT FOLDER'S OPERATIONS WERE
NOT THE MACHINE'S.** (found 2026-09-19 by `nitpick-compiler_s11`. The first
build of D-310's checker folded the operands of every `+ - *`, and the
compiler trapped `ShiftRange` inside its own folder on a prelude expression
`1u256 << … `; reading the folder beside it found the rest.)

`fold_binary`/`fold_unary` computed in raw `int64` whatever the type:
- a shift of a type wider than 64 bits bounded its amount by the TYPE's width
  and shifted an `int64`, trapping the compiler at an amount of 64 or more;
- a narrow `<<` kept the bits past the width (`1i8 << 7i8` folded to 128, the
  machine's answer −128);
- `~` of a narrow unsigned value was unmasked (`~5u8` folded to −6);
- a `uint64` bit pattern past 2^63−1 was divided, reduced, shifted right and
  compared SIGNED;
- `MIN / -1` and `MIN % -1` were computed, trapping the compiler for `int64`
  and folding an unrepresentable quotient for a narrower width;
- a cast folded a `uint64` bit pattern as another type's value.

Each is now the machine's answer at the width. A constant `MIN / -1` or
`MIN % -1` is TYPE-004, as a constant division by zero is. A result beyond the
64-bit window declines. `tests/backend/programs/constant_fold.npk` holds each
folded value to the same operation computed at run time, and
`tests/types/rejection/constant_division.npk` holds the refusals.

**DEF-75 — FIXED at 1.5.8b step 3: npkg's `read_rows` SKIPPED A `rows.txt` LINE IT
COULD NOT READ.** (found 2026-09-19 at 1.5.8b's planning, reading the two
runners side by side.) A line with the wrong field count or an unknown role was
skipped in silence, where the harness's reader failed the run by name. So a row
could vanish from one runner's belts and not the other's, and the parity stage
compares verdicts, not the rows the belts read. `read_rows` fails with
`ERowsMalformed` now, and a non-numeric group, trap count or clause context
fails it too. Both self-checks hold three planted lines: a well-formed row, an
eleven-field row and an unknown role.

**DEF-81 — FIXED at 1.5.8b step 3: A GUARD INSIDE A LOOP'S INVARIANT WAS ELIDED
ON ITS FIRST VISIT'S PROOF.** (found 2026-09-19 by `nitpick-compiler_s11`, by the
belt that counts one assume per elided guard, which counted one fewer than
`counted_exit.npk` held.) A soundness hole in the verified build, latent since
1.5.3 lowered the head's check.
- The check at a loop head is lowered once and runs at every visit: at entry,
  after each iteration, after each `continue`.
- A guard inside a clause (a division, a shift, and since step 3 an `overflow`)
  had a row in the ENTRY's context alone. The back edge and the `continue`s were
  encoded quiet, and the emitter elided the guard on that one row.
- Probe (`tests/verify/inv_inner_guard.npk`): `while (n < 3) invariant 100 / d > 0
  { d = d - 1; … }` with `d = 1` at entry. The entry's row is discharged and the
  back edge's is not, so the check stays, but the guard inside it was elided.
  The plain build traps `DivByZero` (36). The verified build divided by zero:
  a machine fault (98) at -O0, and `InvariantViolated` (34) where -O2 had
  exploited the undefined division.

The fix:
- The back-edge and `continue` encodings record the inner rows too, under the
  loop's clause context.
- The emitter elides a guard only when every row sharing its (site, kind, space,
  clause context) is discharged.
- `rows.txt` gains a twelfth field, the clause context, and both runners'
  belts count GUARDS: an assume per elided guard, a trap per retained one. A
  guard inside a check that is not emitted (every row of it discharged) counts
  as neither, which a discharged `ensures` seam also needed
  (`ens_inner_guard.npk`).
- Both self-checks hold five cases, the defect's own shape among them.

**DEF-82 — FIXED at 1.5.8b step 5: A
`defer` BODY'S GUARD IS EMITTED ONCE PER EXIT AND HAS ONE ROW.** (found 2026-09-19
by `nitpick-compiler_s11`, reading the encoder beside DEF-81's fix.) The encoder
walks a `defer` body once, with every name opaque. That is sound: the row holds
at any exit. The emitter, though, writes the body at every exit of its scope.
Probe: `defer { discard(n + 1i32); }` in a function with two exits emits two
`IntOverflow` traps for one row. So the runners' belts would count one trap
where there are two, and a verified build of such a function is a red run.
It is never a silent pass: the belts fail closed. No function in the tree has a
guard in a `defer` body today; its 33 are frees and unwatches. The fix direction
is to count what the emitter does. The emitter already knows how many times it
writes each `defer` body. The row's traps become the body's emission count, so
the belts see the traps and assumes the build holds. That means writing a
function's rows after its body is lowered, since the elision answers the
lowering needs are computed before it. Step 5 adds the `bounds` rows, and an
element write in a `defer` is the likeliest shape.

*[FIXED 2026-09-19 at 1.5.8b step 5, as the direction above says: by counting
what the emitter does. A row recorded inside a `defer` body carries that body's
statement id (`SmtObl.defer_stmt`, set while the walk is inside it — nested
bodies need no case, since an inner body's id is counted once per copy of the
outer); the two sites that write a body (`run_defers_down_to`,
`run_exit_defers`) report each copy; and the row's tenth field is computed when
the table is WRITTEN, as one copy's traps times the copies. Writing the whole
row after the lowering was not needed — only the traps field is deferred, so
the table keeps a row's line in four pieces and assembles them at the end, and
the elision answers stay computed before the body as they must be. The batch
the emitter counts into is the function's own rows: function emission never
nests (the `FnEmitter` is one struct; generic instances are drained by
`emit_program`'s worklist between functions). `tests/verify/defer_guard_copies.npk`
holds both directions — an open row whose guard is written twice and a
discharged one whose two copies are both elided — and it FAILS against the
pre-fix accounting, measured: "3 `llvm.assume` for 2 elided guards" and "2
-4099 traps for 1 retained".]*

**DEF-94 — FIXED at 1.5.8d step 0: THE ENCODER'S ESCAPE SET NEVER HELD THE IMPLICIT POINTER
RECEIVER, SO A `Self->` METHOD CALL ON A SCALAR LEFT THE CALLER'S VERSION STANDING.**
(found 2026-09-25 by `nitpick-compiler_s14`, by the first probe of 1.5.8d step 0 -- D-317's
design rests on the escape set, and the plan named "an implicit `Self->` receiver" as one of its
members; the probe asked whether it was.) A method whose parameter 0 is a pointer takes the
receiver's ADDRESS when the receiver expression is not itself a pointer (the emitter's
`emit_method_call`, 1.0.9b; the checker's DEF-24 rule for a limited or `fixed` receiver), so
`drop x.bump()` with `impl:int32:Bump { func:bump = NIL(int32->:self) ... }` writes the caller's
`x`. DEF-14's set (`@`, `$$i`, `$$m`, through any place path) never saw it: the encoder kept
`x.1 = 6` past the call that stored 5, z3 proved `100i32 / (x - 5i32)`'s divisor nonzero, the
`div-zero` row was DISCHARGED, and the elided build under that manifest divided by zero -- exit 98
through D-307's `MachineFault` net -- where the plain build reaches `failsafe` with `DivByZero`
(40). Measured end to end on `624d71f`. Latent since 1.5.0 for scalars (a scalar with a user
`Self->` impl is rare, which is why no row of the compiler's own build moved), and the case
D-317's aggregates would have made common. THE FIX: `collect_escaped_expr` adds the receiver
place's root for every method call whose callee's parameter 0 is a pointer type node while the
receiver's recorded type is not a pointer -- decided off the callee declaration's own parameter
node, which needs no scope to read -- excluding the namespace form (D-273) and the
trait-qualified form `Trait.method(recv, …)` (D-172: the receiver is argument 0 and an address
there is written `@recv`). `tests/verify/recv_escape.npk` pins it: `div-zero open 1`, the guard
kept, both builds exiting 40. The last net (D-307) is what made the unsound elision a controlled
stop rather than a silent wrong answer; it is not what makes the encoding sound.

**DEF-93 — FIXED at 1.5.8c step 3: A `Rules` DECLARATION'S `pub` WAS NEVER STORED, AND
`decl_flags` READ ITS SUBJECT TYPE'S NODE INDEX AS THE FLAG WORD.**
(found 2026-09-24 by `nitpick-compiler_s13`: step 3's second full harness had
`tests/modules/rejection/owned_names.npk` report seven of its eight refusals -- the
prelude-owned `ListLen` case gone -- and a probe showed a program's `limit<ListLen>` had
become RESOLVE-002 on the swept tree while it resolved on step 2's; the loader, the
resolver, the symbol table, the interner and the parser were reverted in turn and none
restored it.) THE CAUSE, read once the bisect had said "not the walks": `p_parse_rules`
took its caller's `flags` and passed none to `ast_add_decl`, and `decl_flags` answers
`d.a` for every kind but a global's -- for a `Rules` declaration `a` is the SUBJECT TYPE's
node index. So `pub Rules<int64>:ListLen` was exported exactly when the `int64` type node's
index in the prelude's AST had bit 0 set (`DECL_PUB` is 1): it had, from 1.5.8b step 6c's
declaration of the rule to step 2, and step 3's clauses in the prelude moved the nodes.
The 1.4.8 global's defect ("whether a `pub fixed` binding was exported depended on that
index's parity") one declaration kind over, and D-239's owned-name refusal for a rule was
the parity's too. THE FIX: the rule's flags ride the high half of its count slot as a
global's do (`c = count | flags << 16`; `decl_flags` and `decl_win_count` read the halves;
AST_REFERENCE §5's row says so); `tests/backend/programs/rules_pub.npk` (a `pub Rules` in
an inline module bound by `use m.{R};`, and `limit<ListLen>` named in a program -- exit 3)
and `tests/modules/rejection/rules_private.npk` (a rule without `pub` refused at the `use`,
RESOLVE-003). The lesson of D-227 again: a fact read from a slot that means something else
is not absent, it is false, and a coincidence keeps it true for as long as it likes.

**DEF-92 — FIXED at 1.5.8c step 4b (found 2026-09-24 by `nitpick-compiler_s13`, measuring 1.5.8c
step 3's cost): THE GENERIC-INSTANCE INTERNER `tt_instance` WAS A LINEAR SCAN OVER THE WHOLE
TYPE TABLE, AND IT WAS 85% OF THE FRONTEND'S INSTRUCTIONS.**
THE FIX (step 4b, the same day): an instance index in the type table beside `tt_intern`'s --
every struct and enum item under the hash of (kind, declaration, argument count, argument ids),
open addressing rebuilt at half load, entered in id order and never overwriting, so the first
equal item along a probe is the earliest, which is what the scan answered; a `tt_struct`/
`tt_enum` item (no arguments) is entered by `tt_intern` under its head's hash, an instance with
arguments by `tt_instance`, the one creator of windowed instances; the hash is kept per item so
a rebuild needs no AST, and a `held` flag says which items the index holds (a hash is no
sentinel, DEF-69). MEASURED, the same alternating A/B under the same load (two full harnesses
and two builds running): the checker over `src/npkc.npk` 137.1 / 136.7 s with the scan (step
4's tree) against 25.1 / 25.2 s with the index -- 5.4x; every program's emission byte-identical
between the two compilers (the index answers exactly what the scan answered, so no type id
moved). The lesson stands with 1.5.2d's: a per-program cost that reads as "the new feature's"
is measured first, and the third linear scan of the type table fell where the first two had.
Measured first, as the rule says: the checker (`tools/check.npk`, built by the same builder from
step 2's tree and from the swept tree) over `src/npkc.npk`, twice each, alternating, under the
same two-harness load -- 73.3 / 74.7 s before the sweep, 94.5 / 94.8 s after (user time; the
sweep adds 28%). Then `valgrind --tool=callgrind` on both checkers over `npkg/main.npk`
(scratch `cg/pre.out`, `cg/swept.out`; 307 G and 410 G instructions): EXCLUSIVE, `tt_instance`
is 84.7% before and 87.2% after, `scope_lookup_local` 3.9% / 3.6%, nothing else above 1%;
INCLUSIVE, 94% of both runs sits under `escape_walk` -> `escape_stmt` -> `struct_field` ->
`struct_field_bound` -> `resolve_type` -> `resolve_user_named` -> `tt_instance`. `tt_instance`
(`types.npk`) walks EVERY item of the type table on every call, comparing kind, declaration,
argument count and the argument ids -- the 1.0.7 identity rule, correct, and O(types) per call
where `tt_intern` has carried a hash index since 1.5.2d. The sweep's measures are field reads on
generic instances (`p.tokens.v.count`, `lx.text.len`, `s.nodes.count`, `fe.cross_stmts.count`),
each a `struct_field` the escape analysis resolves through this scan, which is the whole of the
28%: the checks themselves are free at run time (the compiler's own -O0 build did not move at
1.5.8b step 3 under 1,181 more traps). NOT the sweep's defect and not step 4's to fix: a scaling
defect of 1.5.2d's class in the type table, to be fixed as its own measured landing (a hash index
over (kind, declaration, arguments) beside `tt_intern`'s; the compiler's own build is 73 s of
which this is most). Recorded here so it is not lost; the fix moves no verdict and no language
rule, and re-measures the checker over `src/npkc.npk` before and after.

**DEF-91 — FIXED at 1.5.8b step 7: FOUR PROGRAM TESTS SHARED FIXED `/tmp` PATHS AND
RACED WHEN TWO HARNESSES RAN AT ONCE.**
(found 2026-09-24 by `nitpick-compiler_s12`: step 7's third full harness, running beside
three others in three worktrees, had `fs_basic.npk` exit 91 and `dyn_stream.npk` exit 91
under -O2 only, while the same run's `parity` stage -- `npkg test`, later -- passed both.)
Reproduced deliberately: two copies of either program at once, 30 rounds -- 91 in 11 of
60 (`dyn_stream`) and 20 of 60 (`fs_basic`); one copy alone, 30 rounds, never. The 91 is
each program's OWN `(E9) { exit 91i32; }` reached through a `?! E9` on a read whose file
the other copy had just truncated (gdb at `npk_trap`: `npk_raise` from `work`), not the
ceiling and not the floor. `dyn_stream`, `fs_basic`, `text_roundtrip` and
`streams_file` named `/tmp/npk_<name>` literally; `trap_stops_runner` already put its pid
in the name. THE FIX: the pid in every such name (`sys(raw SYS_GETPID())`, `lib/nsys.npk`),
and the file unlinked at the end (`scrub`, `unlinkat` through `AT_FDCWD`) so a per-process
name leaves nothing behind (a failed check leaves its file for the reader); 60 of 60 in
pairs after. SURVEYED AND LEFT: 33 programs carry 2–5 s join deadlines; several TEST the
deadline itself and none has flaked but `failsafe_alloc` (DEF-85), so they stand -- a red
on one of them under load is READ as DEF-85's class and its deadline raised then, never
re-run. THE LESSON: a test that names a path names a resource every concurrent run of it
shares; the process id is the cheapest partition, and cleanup is part of the name's cost.

**DEF-90 — FIXED at 1.5.8c step 1: AN AWAITED METHOD CALL ON A FAMILY-IMPL INSTANCE
INSIDE ANOTHER GENERIC BODY NAMED ITS SPECIALIZATION AND NEVER RECORDED IT.**
(found 2026-09-24 by `nitpick-compiler_s12`, writing the loop dump tool for 1.5.8c
step 3: the first program to `write` through `std_out()`.) `TextWriter<W>`'s `write`
awaits `self.inner.write(...)`; at `W = LineBufWriter<ByteWriter>` the awaited path
(`async_callee_name`, 1.1.12c) substituted the receiver and spelled
`LineBufWriter<ByteWriter>:Writer.write` -- and, unlike its sync twin
(`emit_method_call`), never called `note_family_instance`, so no instance loop emitted
the specialization: its resume and frame were referenced and never defined, and `llc`
refused the module ("base element of getelementptr must be sized"). Reproduced on
`c5ba885` and on every compiler back to the nested writers (1.1.12c);
`text_roundtrip.npk` reaches the writer through the free `text_flush`, which is why
the suite was green. Not a miscompile: a refusal at `llc`. THE FIX: the awaited
bound-call path records the family instance exactly as the sync path does (one
line, beside the name it already derives); `tests/backend/programs/std_out_nested.npk`
writes and flushes through the nested writer and exits 0.

**DEF-89 — FIXED at 1.5.8b step 6c (the amended landing): THE HARNESS'S


`check_codes_tested` READ A NUMERAL-RETURNING STRING FUNCTION AS A DIAGNOSTIC CODE.**
(found 2026-09-24 by `nitpick-compiler_s12`, running the whole-tree checks in-process
over 1.5.8c step 0's tree before its harness; the running 6c, 6d and 7 harnesses
would each have ended red on it.) `CODE_DECL_RE` recognised a code declaration as
`pub func:NAME = string() never fails { pass "<uppercase, digits, hyphens>"; }`, so
`cast_bounds.npk`'s `len_ceiling_text` — `pass "140737488355328"` (step 6c) — was a
"code" asserted by no test, and the check failed on a rule that does not exist. A
false positive that fails closed, never a miscompile; but a red run reads as a
finding here, and a red on a rule that does not exist is time spent reading nothing.
THE FIX: the regex asks for the code's own shape, `NITPICK-<STAGE>-<NNN>` — which is
how `check_codes_centralised` has always recognised a code literal (`"NITPICK-`), so
the two checks now agree on what a code is. No twin in `npkg` (the check is the
harness's alone). THE LESSON: an instrument that recognises a thing by a LOOSER shape
than the thing's definition will one day match something else; recognise by the
definition.

**DEF-88 — FIXED at 1.5.8b step 6d: A `pick` ARM WITH `_` OVER AN OWNING PAYLOAD
WAS REFUSED BY THE EMITTER — and, once it lowered, the CONSUMING form had to say who
frees the payload `_` discards.**
(found 2026-09-23 by `nitpick-compiler_s12`: step 6c's whole-tree sweep ran the
FULL compiler over every root, where step 6b's had run the checker alone.)
`tests/accept/moves.npk:223` — `pick (r) { (MvokRes.Note(_)) { pass 1i64; }, … }`
over an enum whose payload is a `string` — is admitted by the checker (D-266: `_`
binds nothing, so the lending form takes no copy and no view) and dies as
`NITPICK-EMIT-002` in the emitter, on the compiler at `c5ba885` and on every one
before it that carried the construct. Nobody ran it: `tests/accept/` is the one
suite for every stage and asks the FRONTEND for silence, so an emitter refusal of
an accepted file is invisible there — which is a gap in the suite's shape as much
as in the emitter (the same file's `pick (move(r))` twin lowers). Not a safety
hole: a refusal, never a miscompile. THE FIX (step 6d, `ir_stmt.npk`'s
`bind_payload`): `arms_bind_any` — which decides whether the selector's ADDRESS is
produced for a lending pick — had always excluded `_`, while `bind_payload` counted
POSITIONS, so an arm whose only "binding" was `_` asked for an address nobody made.
One notion now, `enum_bind_is_wild`: `_` binds nothing and keeps its position (binding
i is payload i). A lending arm of wildcards needs no address and binds nothing; a
CONSUMING arm of wildcards alone names nothing and so owns the whole value, which the
enum's drop body frees (D-216's existing branch, now reached); and a consuming
multi-payload arm such as `(Pair.Two(a, _))` drops the `_` payload IN PLACE at the
bind, through its type's drop body — the selector's own drop was cleared by `move` and
no binding owns that slot, so nothing else would. MEASURED under `NPK_HEAP_STATS`
(`pick_lend_wild.npk` once, `pick_lend_wild_churn.npk` two thousand times,
`tests/cost/pick_lend_wild.toml`): peak_live 21 bytes in both, 6,001 allocations in
the churn — both consuming forms free what `_` discards. `tests/accept/moves.npk`
lowers through the full compiler for the first time. Whether `tests/accept/` should
also be EMITTED (not run), as 1.5.1b step 3c's `object` stage does for library units,
is step 7's question.

**DEF-87 — FIXED at 1.5.8b step 6c: `#wild_slice` WITH A NARROW COUNT REACHED
`llc` AS A TYPE ERROR.** (found 2026-09-23 by `nitpick-compiler_s12`, writing the
ceiling guard.) The checker admits any integer as the count (`type_is_integer`), and
the emitter wrote it into the `{ ptr, i64 }` header as an `i64` whatever its width:
a LITERAL count passed, because a constant has no type in the emitted text, and a
narrow VARIABLE count -- `#wild_slice<uint8>(u, c)` with `int32:c` -- was refused by
`llc` on every compiler since the builtin was written (measured on `c5ba885`: "llc
REJECTED the IR"). The count is widened BY ITS SIGN now (`widen_len_i64`: a signed
count sign-extends, so a negative `int32` is a negative length and the ceiling
guard refuses it; an unsigned one zero-extends), and the header takes the widened
value. `len_ceiling.npk`'s case 5 drives the narrow negative count to `OutOfBounds`.

**DEF-76 — FIXED at 1.5.8b step 6c: THREE FLOOR SPEC SENTENCES NAMED
`HeapBadRequest` WHERE THE IR TRAPS `Unreachable`.** (found 2026-09-19 by
`nitpick-compiler_s11`, planning 1.5.8b; scheduled to the step that edits the
allocator's spec.) `npk_dalloc`, `npk_hunmap` and `npk_frame_free` reach their
refusals through `npk_heap_bad`, which traps -4102 (`Unreachable`, D-141's integrity
class: a null or misaligned free, a header that does not validate, a mapping the
kernel and the table disagree about), and each `(boundary "...")` sentence said
`HeapBadRequest` (-4104, `npk_heap_badreq`: a negative or oversized request, a
`calloc` product that wraps, a bad alignment). Measured over every boundary
sentence that names `HeapBadRequest` against the helper each symbol's IR actually
calls: exactly those three were wrong; `npk_alloc_impl`, `npk_aalloc`, `npk_calloc`
and `npk_ralloc` were right (`ralloc` calls both, for its two refusals). The three
sentences name `Unreachable (-4102 through npk_heap_bad)` now. A boundary sentence
is a promise a reader accepts (TCB.md SS5), so a wrong identity in one is a wrong
acceptance; nothing decided by rows was affected.

**DEF-86 — FIXED at 1.5.8b step 6b: THE REACH ANALYSIS DID NOT SEE A RAISE, A
GUARD OR A `fail` INSIDE THE PRELUDE.** (found 2026-09-23 by `nitpick-compiler_s12`,
planning the prelude `List`'s `limit<ListLen>` -- D-308 §6 -- whose checks inside
`list_push` would have been one more prelude-internal trap site the analysis could
not see.) Since 1.1.6 the walk excluded the prelude on the premise, stated in
`pipeline.npk`, that a program reaches the prelude's guards only through machinery
its own text contains -- an index, a division. The prelude's `list_pop` falsified
it the day it was written: `!!! OutOfBounds` on an empty list, in a program whose
own text has no index, reached `failsafe` with no arm ever asked for and landed in
`(*)`. Measured: exit 44 through the wildcard, where the same program with the arm
answers 55, and the compiler had accepted the arm's absence. Nine raises in the
prelude's `List` operations had that shape, and every guard the prelude's own
arithmetic and indexing carry -- the text layer's, Dragon4's, the List
operations' -- and every `fail` of a prelude constant (`BadPath` in the path
functions) were invisible the same way. THE FIX: the walk follows every resolved
callee -- a plain call's symbol, a method call's recorded declaration, a
module-qualified call's (D-273) -- into the prelude and into every import, walks
each function ONCE from a queue drained before the set is read, reaches every
impl of a trait when the callee is the trait's own method (a `dyn` dispatch, or a
bound inside a generic body, where the impl is decided elsewhere: an
over-approximation, in the sound direction), and counts a function named as a
VALUE as reached. The prelude is still not walked WHOLE, which keeps the precision
the exclusion was written for. MEASURED over the tree's 502 root programs: 20
gained an arm they could not have been asked for before -- 13 `IntOverflow`, 7
`OutOfBounds`, 4 `TbbErr` (the bound over-approximation, all four in derive and
dispatch tests whose generics never instantiate at a twisted type), 1
`ShiftRange`, and 2 `BadPath` in the two tools that parse paths -- and no other
diagnostic moved in any of the 140 type and analysis rejection files.
`reach_prelude.npk` refuses the missing arm; `prelude_raise.npk` answers 55
through it. **Owed, with its design and its trigger (E-5, §4):** the bound-call
over-approximation can be narrowed to the impls at the types the enclosing
generic is INSTANTIATED at, read from the checker's instance table
(`inst_count`, `tt_instance`), the day a program pays for it in more than an arm
it cannot enter; today four tests pay one line each.

**DEF-85 — FIXED at 1.5.8b step 7 (the user, 2026-09-24: "both of your recommendations
are fine" — option 1 of the three below): the join deadline is a HANG NET, sixty
seconds, a bound only a real hang reaches, as D-297's solver net and DEF-83's control net
are; the verdict is the region's answer and nothing the scheduler decides. Raised at
1.5.8b step 5's third full harness: `failsafe_alloc.npk`'s VERDICT RESTED ON A
FIVE-SECOND WALL CLOCK.** (found
2026-09-20 by `nitpick-compiler_s11`.) The program's two threads are declared
`joins JOIN_5S`, and its expected answer is `failsafe`'s 45. Under THREE full
harnesses running at once, one run in 40 answered 70 — the trap route's re-entry
code, which is what a second trap inside `failsafe` produces. Measured afterwards on
a quiet machine: 120 consecutive runs, all 45. So the mechanism is the join deadline
slipping under load, the raise landing while `failsafe` is already running, and the
re-entry rule ending the process at 70 exactly as D-291 and D-307 say it should —
the compiler and the floor both did the right thing, and the TEST is what is
load-sensitive. This matters because `NPK_HEAP_STATS` exists precisely so that "a
wall-clock is never a verdict" (1.5.1b step 0), and here a wall-clock sits in a
verdict position: a green run means "the machine was fast enough", which is not the
property the program was written to show (D-292's failsafe region answering without
waiting on a mutex a stopped thread holds). Options, for the user: raise the
deadline far past any plausible load; make the program's answer independent of the
join (the `failsafe` answer is what matters, not that the threads joined); or mark
it as a test whose stress loop runs only on an unloaded machine. Not a defect in the
compiler, the floor or the runners — recorded so the next seat that sees a lone 70
does not go looking for one.

**DEF-83 — FIXED at 1.5.8b step 3: AN EXPLORER CONTROL'S FINDING SEED COULD PASS
ITS WALL-CLOCK NET.** (found 2026-09-19 by 1.5.8b step 1b's first full harness.)
`link-without-cas.ctl` is found by a seed that spins to the shim's step budget,
"about 30 s" by its own comment, where every other seed takes milliseconds. The
per-seed net in both runners was 60 s, the unit explore stage's. With eight
solver batches and a second harness running beside it, the finding seed (6;
step 2's harness found the defect there) passed the net and read "hung". The
control was reported blind: a red run that was no verdict. Both runners' control
seeds now run under a 300-second net: a hang net, never a verdict, as P-13's
solver net is, at ten times the slowest designed seed. Step 1b's harness was
re-run on the same tree.

**DEF-84 — FIXED at 1.5.8b step 3: AN INDEX THROUGH A VALUE NO PLACE HOLDS, OR
THROUGH A `Result`'s `.value`, WAS ADMITTED BY THE CHECKER AND REFUSED BY THE
EMITTER.** (found 2026-09-19 by `nitpick-compiler_s11`: npkg's own self-check,
written for DEF-75, indexed `rr.value[0i64]` and the build stopped at
NITPICK-EMIT-002.) The index arms took the base's ADDRESS. A call's result has
none, and the member arm had no case for a `Result`. Four probes, each
EMIT-002:
- an array returned by a call and indexed;
- an array in a `Result`'s `.value`;
- a `List` in a `Result`'s `.value`;
- a `List` returned by a call.

The first two date from each arm's first version. The `List` shapes date from
step 1b, whose `l[i]` goes through the same address. It was never a
miscompile, since the emitter refused by name, but the checker must not admit
what the emitter cannot lower. The fix:
- A `Result`'s `value` and `err` are places, at the value path's own slots.
- A base rooted in a temporary is spilled to an entry-block slot and read there.
  The checker refuses every write into a temporary (TYPE-024), an owning
  temporary stays its statement's to drop (D-246), and no `await` can fall
  between the spill and its reads (D-178).
- `tests/backend/programs/index_temporary.npk` covers every shape, inside a
  coroutine and inside an `await`'s operand too.
- `tests/cost/index_temporary.toml` holds the temporaries dropped. 50,000
  statements peak at the one statement's 8 live bytes. A planted spill that
  took the temporary peaked at 400,000, and the unit fails it.

## 2g. Re-examination leads for the floor's evidence (owner: the compiler seat; raised at the s6→s7 hand-off, 2026-09-17)

None of these was a known defect when it was recorded. They are the four places
the seat that wrote 1.5.6's specification, models and kernel-effect table said
it would try to prove itself wrong, given another day. They are written down
because TCB.md §4c's generated residue list carries what the evidence does not
COVER and cannot carry where its author is least sure of what it CLAIMS — and a
hand-off message cannot carry it either. Each lead closes by a dated note in
its row saying what was checked, how, and what was found; a lead that finds a
defect declares a `DEF-` in §2f.

| id | the lead | why it is one | how it closes | owner / by when |
|---|---|---|---|---|
| ~~**E-1**~~ | **CLOSED at 1.5.6c steps 0–2 (2026-09-17), by READING — and TWO clauses were FALSE for a legal caller.** Every multi-object section was walked pair by pair against its callers in the floor's IR and in the emitter. (1) `npk_string_concat` asserted its two INPUT strings apart: `string_concat(s, s)` is a legal program, in the tree twice; with the hypothesis deleted all twelve rows still discharged — it was never needed. Fixed by a clause of its own, `(views …)`: apart from objects, never from each other, and READ-ONLY PROVEN (the frame row exempts no byte of a view). (2) `npk_small_free` — the section this lead named — asserted the chunk, its list neighbours and the class's PARTIAL-LIST HEAD apart unconditionally, and in the allocator's most common state (a free into the chunk at the head of the partial list) two of them are the same 64 KiB; its seven discharged rows claimed nothing for the ordinary LIFO free. Fixed by `(lo len apart-when COND)`: in the address space always, set apart only where the body reads it (the chunk was full). Neither was a behavioural defect: evidence that said nothing where it looked like it spoke. No verdict moved (25 hashes over the two symbols). The remaining ten sections are ARGUED in `runtime/npkrt.spec` beside the clause, each argument read against the IR as it was written (three sentences of the first walk did not survive that), resting on six named invariants (TCB.md §5 item 16). Found by the same read, latent, refused now in both runners: a clause head the translator reads ONCE, written twice, was dropped in silence — for `frame`/`ensures-trap`/`ensures-fresh` a claim written and never proven — and a loop sub-clause it does not read likewise. MEASURED FOR WHAT A SOLVER CAN SEE, and recorded for what it did not find: no row of the floor is vacuous that should not be (the five that are belong to the five always-trapping symbols), every section's caller assumptions have a model, and which pair facts each proof needs. A false-for-SOME-caller hypothesis is invisible to a solver; both were found by reading. The plan and its record: `meta/roadmap/done/1.5/1.5.6c.md`. The lead as recorded: ~~**`(objects …)` asserts pairwise disjointness as a HYPOTHESIS.** `objects_facts` (`npkg/floor_smt.npk` ~1330) emits, per pair, a null-or-empty escape or "the ranges are apart"; 18 sections of `runtime/npkrt.spec` carry the clause.~~ | Every row in such a section is proved ASSUMING the caller handed it non-overlapping ranges. A `(summary)` callee's objects are rows at a TRANSLATED call; a caller that is a boundary symbol, or the emitter's own generated call, is checked by nothing. `npk_small_free` is the one its author is least sure of: the chunk, its two neighbours, the class's partial-list head and five globals, argued disjoint from the allocator's layout rather than from anything a row proves. | Section by section: list the callers (the floor's and the emitter's runtime-call sites), argue per caller why the ranges cannot coincide, and write the argument into the section as a comment; a pair that CAN coincide is a defect or a false clause, stop-the-line like any `open` floor row. | the compiler seat; 1.5.8's close-out at the latest, so that 1.6.1's leg A starts from a specification whose caller-side assumptions are written down |
| ~~**E-2**~~ | **CLOSED at 1.5.6c step 3 (2026-09-17), by an instrument rather than the sentence the lead asked for.** What is true of `npk_small_free` is true of nearly every section: of the 31 that have rows AND assume something of their caller, 2 have every caller covered, 18 have a floor caller no row covers, and 15 are EXPORTED, so emitted code calls them and nothing proves the assumption there. TCB.md §4d is that table, GENERATED in both runners from the floor's own calls and `define` lines and from the spec, held current by both (a stale region is a red run), with §5's SIXTEENTH acceptance saying what a reader is asked for. Generated because the hand-written account of who calls what, and how it is covered, was wrong twice in eleven sections — written by the seat that had just read the code. The lead as recorded: ~~**`npk_small_free`'s restated preconditions are established by no translated caller.** Its `requires` (`runtime/npkrt.spec` ~670–676: the chunk table sorted and inside the address space, the ordering over the free pair, the chunk base non-null) repeat `npk_small_check`'s; its one caller is `npk_dalloc` (`runtime/npkrt.ll` ~5512), a `(summary)`/`(boundary …)` section with no translated body.~~ | True by the heap's construction — which is exactly the class of invariant D-233 assigned to 1.6's leg A. Sound as residue; it READS stronger than it is to anyone who does not know the translator never checks callers. | With E-1 (the same walk), plus one sentence in TCB.md §5 or the generated §4c saying in words that a section's `requires` are its callers' to keep and which callers no row covers. | the compiler seat; with E-1 |
| ~~**E-3**~~ (the READ closed at 1.5.6b; the INSTRUMENT closed at 1.5.7) | **THE READ: CLOSED at 1.5.6b step 2 (2026-09-17) — three findings, none on a verdict's path, each a different kind of thing the correspondence belt cannot see.** Case by case, the lead's own list: (1) a DEVIATION — the early-out (`epoll`: the park word set) returns without draining the eventfd, and `epwait` cleared its readability on that path too; faithful now. (2) a MISSING STEP on a named block — the wait's EMPTY return (`n <= 0`: the timeout, or EINTR — this list's "no event" and "a negative return") had no transition at all, where the futex path has had the library's `spurious` return since the model was written; `epempty` now, and the model's reachable states went 263 → 275 → 358 with all four rows `unsat` and all four controls `sat` at K 14, D 5 (largest row 15.3M of the 20M rlimit). (3) a GAP — `due` and `npk_io_register`'s ten blocks were NAMED and nothing of a descriptor's readiness was modelled; the seventh model `reactor-io` (three predicates, three controls; 23 rows, 19 controls in all). The eventfd's event: faithful. Both in one return: argued (the loop's arms touch disjoint state on the executor's own thread, whose next step is the sweep), not modelled. The collapsed `epoll`/`epgo` window: argued sound, and the eventfd's registration confirmed EPOLLIN alone (level-triggered). The packed 12-byte layout: held since step 1 by `kernel_effects.npk`. Reading the models a second way (explicit-state search, no bound) found no bad state in any of the seven and three whose depth is under their diameter — S-75. Record: `meta/roadmap/done/1.5/1.5.6b.md`, step 2. ~~**THE INSTRUMENT half stands as written: 1.5.7's explorer drives the real blocks.**~~ **THE INSTRUMENT: CLOSED at 1.5.7 (2026-09-18).** The schedule explorer drives the REAL epoll blocks of `npk_park_sleep` under a virtual epoll -- a real poll with a zero timeout under the baton, re-probed only after another thread has stepped -- in every explored program with a reactor (`io_ready_basic`, `io_ready_declined`, `reactor_arm_race`, `text_pipe`, `streams_pipe`), 1,000 seeds each per run, with the quiescence oracles live. `reactor-io`'s controls `drop-due-stamp` and `drop-duenow` are explorer controls, found from seed 1, and `park-unpark`'s `drop-epoll-recheck` is recorded as not a floor bug with its measurement (0 in 1,000 on `reactor_arm_race`, written for that shape, where `no-rouse` is found in 84 of 200). A behaviour the model forbids and the code exhibits would be a finding by construction; none was found on the epoll path. The explorer's one floor find, DEF-57, was on the trap route. Record: `meta/roadmap/done/1.5/1.5.7.md`, steps 1–4, and `runtime/explore/controls/README.md`. Original row: **`park-unpark`'s `epwait` step against `npk_park_sleep`'s epoll blocks.** One transition (`runtime/models/park-unpark.model:57`) collapses nine blocks — `epoll epgo deliver dloop done1 drain due dnext out` — with a compound return condition; hand-written with the least mechanical support of any step. The floor reads the event array at stride 12 with the payload at +4, the packed `epoll_event` of x86_64, taken from the layout rather than from a header. | The correspondence belt proves those blocks are NAMED by some step. It cannot prove the step's `next` bindings MEAN what the blocks do. If any model abstraction is wrong, its author bets on this one. | Read the nine blocks against the step's guard and `next`, case by case (no event, the eventfd's event, a descriptor's event, both, a negative return), and confirm the 12-byte packed layout against the kernel's definition; then 1.5.7's explorer drives the REAL blocks under a virtual epoll, and a behaviour the model forbids and the code exhibits is a finding by construction. | the compiler seat; during 1.5.7's planning (the read), and 1.5.7 itself (the instrument) |
| ~~**E-4**~~ | **CLOSED at 1.5.6b step 1 (2026-09-17), by MEASUREMENT and then by an instrument.** Every `writes` row was probed through its raw syscall with sentinel-filled buffers; FOUR things were found, none on a verdict's path: `sched_getaffinity`'s length (the requested one, where the kernel writes `result` bytes -- it hid DEF-52) and its missing bound (found by a refuted `frame-default` row); `rt_sigaction`'s length (8, where the kernel writes 32 -- the unsound direction, latent); and TCB.md's generated head claiming a row for every number while `clone` and `execve` had none. The author's other three named rows measured correct, the packed 12-byte `epoll_event` layout included. The table is ONE authority now (VERIFICATION_REFERENCE SS9.2's `kernel-effects` region, generated into `npkg/floor_kernel.npk`; four hand-maintained lists became zero), its option-dependent rows keyed by (number, option), and `tests/backend/programs/kernel_effects.npk` holds the write rows to the running kernel on every run in both runners (exit 0; exit 42 and 52 with the two old rows put back). ~~The kernel-effect rows written from knowledge rather than from the kernel's definition~~ (`tx_syscall`, `npkg/floor_smt.npk` ~3089–3147). Named by their author: `epoll_pwait` (how many entries it writes; the struct's exact size), `getrandom` (a short return means exactly `[buf, buf+result)`), `sched_getaffinity` (the written length against the requested one), `clock_gettime` (16 bytes). | TCB.md §5 says a reader ACCEPTS these rows as the kernel's promise, which is why they must be right rather than plausible. **[2026-09-17, the first half hour, by measurement with sentinel-filled buffers through the raw syscalls:] two rows are WRONG.** `sched_getaffinity` (204): the row says `arg2`, the kernel writes `result` bytes (8 here, of 128) — an over-approximation, sound for a frame claim, and the thing that hid **DEF-52**. `rt_sigaction` (13), which was not on its author's list: the row says `arg4` (the sigset size, 8), the kernel writes the whole 32-byte action to a non-null `oldact` (bytes 0..31 changed, 32 on untouched) — an UNDER-approximation, the unsound direction, latent only because the floor's one call (`npkrt.ll:251`) passes `oldact = 0`. No recorded verdict rests on either. | EVERY row that claims a write region, measured and not read: a probe per syscall with a sentinel-filled buffer, asserting the bytes the kernel changed are exactly the row's — kept as an instrument, so the table is held to the kernel it runs on instead of accepted; the two wrong rows corrected; `runtime/npkrt.obligations` re-recorded if a verdict moves (D-040). | the compiler seat; BEFORE 1.5.7's plan is written — the explorer's virtual kernel re-implements these same syscalls, so the same facts are its foundation |

## 3. ~~Decisions blocking 1.4 (self-hosting)~~ ALL SETTLED — cycle 1.4 closed 2026-09-02 (1.4.9, `done/1.4/`)

| # | Proposed | Item | Blocks | Source |
|---|---|---|---|---|
| ~~**C-10**~~ | **D-202** | **SETTLED at 1.4.0** (the harness has measured the corrected criterion since 0.8.1 — the fix was spec-only; note the row's "D-085:5747" citation was off, the operative lines were 5798/5825/5883 and D-079:5443). ~~Correct the fixpoint acceptance criterion.~~ BUILD_REFERENCE:188 and D-085:5747 say "stage 1 and stage 2 must be byte-identical" — unsatisfiable (two independent emitters). Restate as "stage-N's *emission of the compiler* equals stage-N+1's (first required pair: stage 1 vs 2), making the stage-2 and stage-3 binaries identical," citing the harness stage as the operative definition. | 1.4 | modules #1 |
| ~~**C-11**~~ | **D-203** | **SETTLED at 1.4.0, executes at 1.4.6** (the committed artifact is the FIXPOINT emission + STAMP; LAYOUT's survival map amended; npkrt.ll re-homes to `runtime/`; D-015's "later" row settled — the floor's permanent form is reviewed hand-written LLVM IR, a Nitpick rewrite decided OUT). ~~Commit the seed IR and fix the deletion plan.~~ `bootstrap/seed/` is empty though four docs assert it holds committed IR; `LAYOUT.md:71` would delete all of `bootstrap/` at self-hosting, destroying the rebuild-from-LLVM-alone path and `npkrt.o` (linked into stage 1). Actually commit the (path-independent, see C-12) seed IR; amend LAYOUT to state which parts of `bootstrap/` survive (at minimum `seed/*.ll` and `runtime/npkrt.ll` until D-015's Nitpick replacement is scheduled — which is itself unscheduled and should be named). | 1.4 | modules #2 |
| ~~**C-12**~~ | **D-204** | **SETTLED at 1.4.0, lands at 1.4.5** (toolchain pinned in `[toolchain]`; `npkseed.py`'s argv-path ModuleID is the one path-dependence left — the harness emission is already path-independent; the `repro` build-twice-cross-cwd stage). ~~Define byte-reproducibility cross-environment.~~ D-078 has no check beyond the same-process fixpoint. Pin the llc/ld.lld version + exact flags in the lock or manifest (a toolchain *input*), add a build-twice-from-different-cwd comparison to the 1.3 procedure, and make seed regeneration path-independent (the seed embeds `ModuleID = '<path>'` today). | 1.4 | modules #3 |
| ~~**C-13**~~ | **D-205** | **SETTLED at 1.4.0** (the normative builder rule is in SUBSET_1 §4; the switch is 1.4.6; adoption is 1.4.7 under D-209 — measured at open: `src/` was still fully subset-1, so §4's gradual-adoption story never happened). ~~Seed-retirement schedule.~~ SUBSET_1 §4 says `src/` adopts each rung's features, but the seed (sole builder until 1.3) lowers only subset 1 — so `src/` adopting a 0.9 construct breaks the builder. Add a normative rule: `src/` may not use any construct the *current builder* cannot compile, and name the cycle at which the builder switches from regenerated seed to committed stage IR. | 0.9–1.1 (pre-1.4) | modules #4 |
| ~~**C-22**~~ | **D-207** | **SETTLED at 1.4.0, LANDED at 1.4.4 (2026-08-29)** (per-scope joins built on the 1.2 scope-exit walk; channel-in-loop and `shared_arena` teardown lifted — `TY_SHARED_ARENA` leaves the `type_drops` excuse table; the `dyn`-element refusal is PERMANENT by decision). ~~Per-iteration channel reclaim needs per-scope joins.~~ D-183's 1.2.5 reclaims a channel at its creating FUNCTION's exit, after the child join — sound because D-062 has joined every task that could hold an endpoint by then. A `channel()` inside a LOOP would need that same ordering per iteration: reclaim at the loop body's scope exit, after joining only the tasks that iteration spawned — and joins are per-function today (one `join_head` list per frame). Until per-scope join machinery exists, `channel()` inside a loop refuses by this row's name; the workaround is creating the channel outside the loop, which is also the design that does not open and tear down a channel per iteration. (Factory channels got owners at 1.2.6 without waiting: the `gives` clause moves the reclaim to the caller's function exit, which exists today.) It is what `shared_arena` teardown waits on: a shared arena's value is a POINTER into storage other threads read, so its release needs the same joined-before-freed ordering, and TY_SHARED_ARENA stays excused from `type_drops` by this row's name (plain `arena` drops since 1.2.5c — it cannot cross a thread, so the value drop is sound). `dyn` channel elements are this row's third tenant: erased content can hide a borrow. | 1.4 | D-183, 1.2.5 |
| ~~**B-4**~~ | **D-206** | **SETTLED at 1.4.0, lands at 1.4.8** (`npkg build`/`test` minimal and real; `npk_spawn` generalizes the driver clone-exec with caller-directed stdio; directory listing via `sys(GETDENTS64)` in `lib/nfs.npk`; the closed-world link written into BUILD_REFERENCE §4; BOTH runners run until parity, harness retirement under SWITCH.md). ~~Schedule `npkg`~~ — the permanent build/test/verify runner. BUILD_REFERENCE assigns it the fixpoint and harness, but no cycle builds it while LAYOUT deletes the Python harness at 1.3. Schedule a minimal `npkg` (build/test/verify) in or before 1.3, and write the D-011 undefined-symbol scan into BUILD_REFERENCE §4 as a permanent pipeline step (it lives only in the throwaway harness today). | 1.4 | modules #7 |
| ~~**P-3**~~ | **D-201** | **SETTLED at 1.4.0, LANDED at 1.4.2** (four commits; two departures recorded on D-201 and in `1.4.2.md` — `read`/`write`'s pointer is `wild any->`, and the transitional rule had to cover `relay`) (the generated signature table; never-fails builtins type BARE, may-fail `Result<T>` — the 13-arm convention generalized; the emitter's parallel authority retired; migration under a transitional rule with the seed flipped in the same commit). ~~Type the whole bare-builtin surface from a generated signature table.~~ D-192 typed `sys`; every OTHER bare-name builtin still types UNKNOWN ("no signature yet", type_access's fall-through), so an argument shape or a `?|` default that disagrees with the floor's actual signature surfaces only at llc — or at RUNTIME (`unwrap_unknown.npk`'s first draft passed a `string` where `write_file` wants a `cstring`, and the kernel refused with the error swallowed). BUILTIN_REFERENCE already carries every signature and the generator already scrapes the file (names, never-fails); emit a `(name → param types, return type)` table, type builtin calls through `check_args` like every other call, and retire the emitter's unknown-operand fallbacks (`?|`/`?!` value-half derivation) plus the floor-signature coercion authority in the call emitter. Blast radius is real — `Result<T>` types materialize at every builtin use site, `raw`-licence and REACH interactions need a sweep — so it is its own subcycle, decided before the fixpoint re-close. | 1.4 | D-192 residue |

---

## 4. Decisions blocking 1.5 (verification) and 1.6 (the analyzer evidence — "Astrée" until D-233) — **cycle 1.5 CLOSED 2026-09-25 (1.5.8d, `done/1.5/`); the 1.6 rows stand**

> **[1.5.8b step 5 (2026-09-19) — E-4, for 1.6's leg B: THE `bounds` RESIDUE IS A FRAME
> PROBLEM, NOT A SOLVER ONE.]** *(This E-4 is §4's; §2g's E-4 is the floor's kernel-effect
> lead, CLOSED at 1.5.6b. Two leads, one number, since step 5 — disambiguated here at step 7
> rather than renumbered, because the plan's step 5 and 6c records, D-308's note, CLAUDE.md
> and the 1.5 README all cite "E-4" for this one.)* The `bounds` rows landed with 831 rows in the compiler's
> own build: 79 discharged, 30 open and **722 `unencoded`**, and the 722 are one shape.
> A `List` is address-taken by every `list_push(@l, …)`, so DEF-14's rule — a name a
> pointer may write is never NAMED — leaves it no length term, and an access through a
> `List<T>->` parameter has none either. Giving those a term is not a matter of a better
> query: two reads of `l.count` are the same value only while nothing writes `l`, and a
> loop body that calls anything may. What is needed is a FRAME CONDITION ("nothing in
> this region writes this object"), which is an effect analysis over the emitted IR —
> exactly 1.6 leg B's abstract interpretation, where an interval domain plus a
> no-store-to-this-object fact is the standard shape. Recorded so the next seat does not
> re-derive it, and so the 722 are read as a scheduled residue (D-309: measured, guarded,
> reported) rather than as a hole. Owner: the compiler seat at 1.6.

> **SETTLED 2026-09-25 as D-318: DECIDED OUT, the design kept, the re-open trigger measured (a
> root paying more than one arm).** **[1.5.8b step 6b (2026-09-23) — E-5, owned by the compiler
> seat: A BOUND CALL'S REACH
> CAN BE NARROWED TO THE INSTANTIATED IMPLS.]** DEF-86 made the reach analysis follow
> calls into the prelude, and a callee that is a trait's own method — a `dyn` receiver, or
> a bound inside a generic body — reaches EVERY impl of the trait, because the walk sees a
> generic body once and the impl is decided per instantiation. Sound, and measured at four
> arms across the tree's 502 roots (`TbbErr` in derive and dispatch tests whose generics
> never instantiate at a twisted type). The narrowing: for a bound call inside generic G,
> read G's instances from the checker's instance table (`inst_count`/`tt_instance`), bind
> the bound parameter per instance, and reach only the impl at that type (`dyn` keeps every
> impl, or walks the program's own coercions instead). Closes when a program pays for the
> over-approximation in more than an arm it cannot enter, or at 1.5.8d's close, whichever
> comes first. (This section's E-4 shares its number with §2g's closed E-4 — two leads, one
> number, since step 5; step 7 disambiguated both in place rather than renumbering.)
> *[1.5.8c step 5 (2026-09-24): its disposition at the close is 1.5.8d's question S-98 —
> the measurement stands at four tests paying one arm each, and the recommendation is to
> DECIDE IT OUT with the design kept here, since the over-approximation is sound and the
> narrowing is a generic-chain walk inside a safety analysis for no safety gain.]*

> **SETTLED 2026-09-25 as D-317 (the user: "lets go with your recommendations for those two
> questions. they look fine to me."); LANDED 2026-09-25 at 1.5.8d step 0 (`c93d80d`): 64 of the 79
> by-value rows discharged; the 116 "pure call" rows were pointer-bound and are E-4's (the step's
> record).** **[1.5.8c step 5 (2026-09-24) —
> E-6, owned by the compiler seat: A BY-VALUE AGGREGATE HAS
> NO VALUE TERM, SO ITS FIELD IS A FRESH TERM AT EVERY READ.]** Found by the `terminate`
> residue report (`meta/roadmap/done/1.5/tools/residue.py`; VERIFICATION_REFERENCE §4b's last
> bullet). `while (i < (src.count => int64)) decreases (src.count => int64) - i` with
> `AssignState:src` a BY-VALUE parameter reads `src.count` as `|_.2|` in the condition and
> `|_.3|` in the measure (`state_copy`'s file), so the entry row `src.count - i >= 0` cannot
> follow from `i < src.count`; and `(raw item_member_count(ast, ld)) - lm` with `DeclNode:ld`
> a by-value local reads the pure call as `|_.60|` and `|_.61|` — a `pure never fails` callee
> is an uninterpreted function of its arguments' TERMS (1.5.3), and an aggregate argument has
> none, so the application is opaque and fresh. Measured over the compiler's own build at
> `d7a8092`: 79 open `terminate` row sites read a by-value aggregate's field, 116 put a pure
> call over one in the measure — 195 of the 499 open, against 240 that read through a
> pointer (E-4's class, which this lead does NOT touch: a pointee may be written by any
> callee holding the pointer). THE DESIGN: an unescaped by-value aggregate binding (never
> `@s`, `$$i s`, `$$m s`, never a pointer receiver — the encoder's existing `is_escaped`)
> carries a versioned term of its own, `s.k`, as a scalar local does; a field read `s.f` is
> the uninterpreted function `(|npk.f.<type>.<field>| s.k)` (nested `s.a.b` composes; a
> `List` field's `count` keeps its length symbol); a write to a field bumps the version and
> states `(= (|f| s.k+1) v)` with every other field of the type equal to its previous value
> (the standard update frame, bounded by the field count); a whole assignment, a `move`, a
> `pass` out of a field and a loop head's havoc bump it as they bump a scalar; a by-value
> argument to a call changes nothing, since the callee cannot write the caller's binding
> (D-004's model — TO BE MEASURED FIRST by a probe, since a lent owning aggregate travels by
> address). A limited field's rule (D-308) is asserted over the applied term at each read,
> as it is over the opaque one today. Sound because no alias to such a binding exists.
> Expected: the 195 rows discharge and a share of the 1,554 open `overflow` rows of the shape
> "sum or difference of two unknowns" with it; the gate over shared rows moves nothing.
> Its disposition is 1.5.8d's question S-97 (recommended: land it as 1.5.8d step 0, before
> the close's refresh, under its own harness and manifest re-record).

> **DEF-95 — FIXED at 1.6.0 step 3b (2026-09-25). THE REACH ANALYSIS ARMED `BadStep` FOR EVERY COUNTED LOOP.**
> Found by the library listener (`nitpick-libs_s6`) planning on `c3bdae2` and measured here: a program whose
> only counted loops are `till (10i32, 1i32)` and `loop (0i32, 5i32, 1i32)` was refused with REACH-002 for not
> naming `(BadStep)`, while the emitter writes the `BadStep` guard only for a COMPUTED step
> (`counted_step_literal`, ir_stmt.npk; VERIFICATION_REFERENCE §7b's `loop-step` row: a literal step is TYPE-068's
> and has no guard) — an identity nothing could raise, one arm per affected root since 1.5.4. The walk asks the
> same predicate now (`reach.npk`, both counted forms); `tests/backend/programs/lit_step_arms.npk` compiles and
> runs without the arm, `tests/analysis/rejection/reach_step.npk` keeps the refusal for a computed step. A demand
> was REMOVED: a root that carries the arm keeps compiling. The two document findings of the same report — the
> archived `loop_dump.npk`'s `use` paths (one level short after the archive) and TYPE_REFERENCE §9.1.2's example
> (`limit` before `sealed`, which the parser refuses) — are fixed in the same landing.

> **DEF-96 — FIXED at 1.6.0 step 3c (2026-09-25). A TWO-PARAMETER `main` COMPILED, AND ITS FIRST PARAMETER READ
> A REGISTER NOBODY WROTE.** Found by the library listener (`nitpick-libs_s6`, on `c3bdae2`; six `nitpick-regex`
> files use the prototype's two-parameter form and discard both) and measured here on `91a7d99`:
> `func:main = int32(int32:argc, cstring[]:argv)` compiled with no diagnostic to `define i32 @main(i32 %a0,
> { ptr, i64 } %a1)`, while the floor's `npk_start_main` passes ONE `{ ptr, i64 }` — so `argc` held the argv
> pointer's low word and a program's `argc == 0` never held (exit 7 with no arguments and with three). The
> checker fixed `failsafe`'s shape (TYPE-044, D-179) and tested `main` by NAME alone (`fn_is_terminal`), never
> its arity, its parameter's type or its return; a `func:main = int64(…)` compiled to `define i64 @main` and
> worked by register accident. D-089 §4 says the signature is FIXED and the two entry points are one rule:
> `main` is now `NITPICK-TYPE-083` unless it takes exactly one `cstring[]` and returns `int32`, and TYPE-044
> covers `failsafe`'s `int32` return as well (`type_stmt.npk`, beside the `failsafe` check;
> `tests/types/rejection/main_sig_{two,none,type,ret}.npk`, `failsafe_sig_ret.npk`). A REFUSAL ADDED, announced
> in advance (NOTICES F8): the listener moves its six files to `cstring[]:_~argv` when it lands.

> **1.5.1 LANDED (2026-09-03)** — the verification surface TYPES (D-220/D-221's typing halves; `meta/roadmap/done/1.5/1.5.1.md`): `limit<R>` names resolve, `Rules` bodies type over `$`, every proposition is a `bool`, contract expressions admit only what a proposition can evaluate anywhere and call only named `never fails` `pure` functions; the five questions it raised were ratified as **D-241** (D-163's contract row retires), **D-242** (purity is a declared `pure` clause with a `Pure` column on every builtin), **D-243** (`old(expr)` a keyword operator, admitted in invariants), **D-244** (`main`/`failsafe` carry no contract) and **D-245** (`result` a keyword with a leaf node); S-13 closed at its step 1. Found on the way: macro expansion SHARED verify nodes across expansions (the last expansion resolved won — a miscompile the day 1.5.3 lowered a contract in a macro-emitted function; expansion clones them now).
>
> **1.5.0 LANDED (2026-09-03)** — the skeleton with the D-007 division pair end to end (D-218/D-219; `meta/roadmap/done/1.5/1.5.0.md`). C-17→D-218's items (1)–(11) are all implemented or scoped: the SMT emitter, the determinism profile, per-function processes, the integer encoding, the ownership-trusting memory model, the content-hash identity, `llvm.assume` elision, the `undef` ban, and TCB.md. The catalogue's remaining kinds land 1.5.1–1.5.8.

The 1.5 surface (grammar/AST/resolution of contracts, Rules, invariants) is built;
everything from *typing* through *Z3* is not. Five decisions, plus the Astrée gate.

| # | Proposed | Item | Blocks | Source |
|---|---|---|---|---|
| ~~**C-14**~~ | **D-219** | **SETTLED (user-ratified during 1.4, recorded early for the handoff — manifest-recorded elision, `--smt-opt` struck).** ~~Elision ownership.~~ VERIFICATION_REFERENCE says `--verify` elides checks; D-040 hangs all reproducibility on `--smt-opt`; both can't hold without reintroducing D-039's timeout-dependent-binary hazard for the artifact Astrée reads. Decide that limit/contract elision is manifest-recorded like every other elision. | 1.5 | verification F3 |
| ~~**C-15**~~ | **D-220** | **SETTLED (user-ratified during 1.4 — three write points, rule names resolve, Rules bodies type, Z3 subsumption).** ~~limit-check placement/typing/subsumption~~ — where checks inject (init only? every assignment? param entry?), the reserved error code, whether `limit` is part of the parameter type; plus close the frontend holes: rule names in `limit<R>` are never resolved (a typo passes silently) and Rules bodies are never typed (`$` untyped, clauses not required `bool`). | 1.5 | verification F2 |
| ~~**C-16**~~ | **D-221** | **SETTLED (user-ratified during 1.4 — violations TRAP, `result` is the success value, `old()` copyable-only, contracts call only pure `never fails`).** ~~Contract runtime semantics under universal `Result`~~ — the "wrap in Result" framing is pre-D-084. Fix: the violation channel (Result-error vs the FORMAL_DRAFT reserved *failsafe* codes 50/51 — they collide), the error codes, `result`'s type (T vs Result<T>), evaluation order/purity, whether `old()` exists for postconditions. And **implement D-014's compiler-injected `ensures result > 0` on `failsafe`** (+ the non-empty-body check) — both currently exist nowhere. | 1.5 | verification F5 |
| ~~**C-17**~~ | **D-218** | **SETTLED (user-ratified during 1.4 — the full architecture: pinned Z3, the determinism profile, per-function fresh processes, the encodings, the catalogue, content-hashed obligation identity; normative text in `1.5/README.md`).** ~~The SMT emitter + invocation architecture~~ — theory choices (bitvectors for wrapping ints, floats, tbb sticky-ERR, Result, slice bounds), the obligation catalogue matching the manifest's `kind` column, the counterexample→span symbol-naming/model-parsing contract, and **the process-spawn primitive** to invoke z3 with (the language has none — *1.4.0 note: the floor is now 157 defines and D-206's `npk_spawn` lands at 1.4.8, which this row inherits*; the stale count read: the floor is 21 symbols with no spawn; `npkg` — which BUILD_REFERENCE says owns the invocation — does not exist → ties to B-4). Note the borrow-checker synergy (VERIFICATION §2.1) presupposes an aliasing/disjointness refusal the 0.5 analyses do not contain — 1.4 must first *create* the error it says it suppresses. | 1.5 start | verification F4 |
| ~~**B-5**~~ | **D-217** | **SETTLED (user-ratified): STRUCK from 1.5 by decision — Astrée is the trial's abstract-interpretation evidence; `[verify.nikos]` refuses by name until a post-1.6 cycle.** ~~NIKOS: specify or defer.~~ A named 1.4 deliverable with zero specification (one flag, one manifest example, one sentence). Either write a NIKOS reference (domains, checks, port-vs-rebuild from the prototype, relationship to Astrée) or strike it from the 1.4 line and schedule separately. | 1.5 | verification F7 |
| ~~**C-19**~~ | **D-232 → D-233** | **CLOSED (2026-09-01) in two steps.** D-232 settled the internal half (C-only working default, the C emitter's design note, the AbsInt contact as the external half); **D-233 then superseded D-232 whole on the commissioned LLVM-native survey** (`research/LLVM_Formal_Verification_Tool_Options.md`, digest `research/digests/llvm-tools-digest.md`): Astrée left the plan, the C emitter is struck, and the evidence moves to the emitted IR — abstract interpretation (Clam/Crab vs IKOS at 1.6.0's measured bring-up gate) + Alive2 translation validation, beside D-218's untouched Z3 spine. No external gate remains anywhere in Phase C; the row's residue (entry points, D-071 mapping, floor stubbing, the manifest in the evidence package) landed in D-233 and `1.6/README.md`. ~~**Astrée input-format gate.** The docs assume Astrée reads "monomorphized output," but the compiler emits LLVM IR and Astrée accepts **C**. Promote the carried "confirm with AbsInt" note to a numbered gate **answered before 1.5 exits**: candidate input formats, and if C-only, schedule the C-emission path now rather than discovering it at the start of a non-renewable 30-day run. Also settle: analysis entry points, the D-071 executor-model mapping, runtime-floor stubbing policy, and whether the SMT elimination manifest is part of the evidence package.~~ | ~~1.6 (answer by 1.5 exit)~~ | verification F8 |
| ~~**B-6**~~ | **D-183 — SETTLED** | **The managed lowering has no cycle.** The memory model's DEFAULT regime is "managed — static ownership, RAII at scope exit", and the backend implements none of it: nothing is dropped at a closing brace, and D-151's own text records the consequence as an accepted interim ("runtime-internal storage … is managed-regime storage whose RAII arrives with the managed lowering, reclaimed wholesale meanwhile by `wild_release_all()` or process death"). So the regime every program gets unless it says otherwise is, today, leak-until-exit. Found at 1.1.10-B from the far end: D-182 makes a channel's generation the guard against a stale endpoint, but the generation only ever moves when a slot is RECLAIMED, and nothing reclaims one — so `StaleHandle` is unreachable from source and every channel outlives its creating scope. The obligations are known and enumerable: what a drop IS per type (channel slot, string body, arena, file buffer, struct field walk), its ORDER against `defer` (D-080 lists both on the same exits) and against D-014's rule that a trap runs neither, `nodrop`'s interaction (D-149), the move analysis deciding which paths still own a value at the brace, and the early exits (`pass`, `fail`, `relay`, `exit`, a suspension that is not scope exit — D-177). **And what a COPY of an owning value is**, which is the same gap seen from the other side: D-065 settled that nothing moves by being passed — ownership transfers only where `move` is written — so passing a `string` by value hands the callee a second pointer to one body, and only a defined drop makes that a question with an answer. D-072 writes `send(move(v), deadline)` in its own signature, but nothing requires the `move`, and requiring it would mean nothing until a drop exists to be suppressed. 1.1.10-B's interim is a rung refusal: a channel whose element owns heap storage does not lower, so the hazard is unreachable rather than silent, and scalars/handles/pointer-free structs (the spec's own `Sample` example) ride today. **RATIFIED AND SCHEDULED (1.1.10-close): its own cycle, inserted between 1.1 and self-hosting rather than folded into a subcycle** — it is a whole missing half of the memory model, it touches every emitted function, and both later cycles are the wrong place for it: 1.4 would otherwise verify a compiler whose default regime is unimplemented, and 1.5 would hand Astrée a program that leaks by design. **It blocks 1.1.11.** Found while renumbering the cycle: `Mutex<T, LEVEL>` owns its data (D-056) and hands out a `Guard`, and CONCURRENCY_REFERENCE §9's own example ends `}   // guard drops here; the lock is released`. That release IS scope-exit RAII. Without it a guard never releases and every `Mutex` deadlocks on its second acquisition — and the alternative shapes are closed: closures are gone (D-018), so there is no scoped-callback form to fall back on. So the managed lowering is not merely "before self-hosting", it is **before sync primitives**, and it should be scheduled as the next cycle rather than deferred behind the rest of 1.1. Channels needed no drop because an endpoint is a copyable handle; a guard is the first type whose whole meaning is its scope. | **before 1.1.11** | 1.1.10-B/D |
| **B-7** | *(instrument, no decision — LANDS at 1.4.1)* | **A new TYPE KIND places an obligation on every type walker, and nothing checks it.** Five of stage 1.1.10-D's seven defects were exactly this: `atomic<T>` and `Channel<T, LEVEL, CAP>` were added in this cycle, and `type_subst`, `type_mentions_param`, `field_holds_ptr`, the escape analysis's pointer question and the vtable emitter had each been written before them. None failed loudly. `type_subst` made a generic function taking a channel uninstantiable; `type_mentions_param` let a channel be opened with a zero-byte element, so a generic pool's jobs all arrived blank with nothing reporting it; `field_holds_ptr` called every struct holding an endpoint pointer-bearing. **None was found by a test of the feature that broke** — each surfaced only when an ordinary program used two features together, which is precisely the failure class `check_kinds_lowered_or_refused` was built for on the expression side. Build the companion: enumerate the `TY_*` constants and require each named walker to mention every one, or to carry a stated reason for its default. The existing seven whole-tree checks each found something on their first run. | before 1.4 | 1.1.10-D |

---

## 4b. ~~The cycle-1.3 batch~~ RATIFIED as D-194…D-200 (user: "go with your recommendations" — Kleene on `&`/`|`, `unit:` declarations in)

The exotic-tier surface, proposed at cycle open with recommendations —
**the full batch lives in `1.3/1.3.0.md`**, one proposal per family:
G-4 `simd<T, N>` v1 surface (elements, lanes, type-directed constructor,
elementwise ops, D-007 vector div guard, reductions as methods, shuffles
OUT by decision); G-5 `tfp*` (native `iN` lowering incl. `i128`/`i256`,
D-144 branch-free ERR, methods not name-families, exact-decimal
`ToString`); G-6 `dim256` (units as exponent vectors over the SI base
dimensions — total algebra, the "if registered" hole dissolves; **USER:
user-declared `unit:` grammar, recommended yes**); G-7 ternary/nonary
(tryte=10 trits, nyte=5 nits, binary-spare ERR, **USER: the Kleene logic
spelling — `&`/`|` carrying the ternary meaning recommended**); G-8
`frac*` (invariant-normalized mixed numbers, operators, exact-or-ERR);
G-9 `complex<T>` (Smith's division on flt, `.abs2()` total); G-10 the
library tier (`lib/nvec.npk`, `lib/ntensor.npk`, tensor rank capped 9
with inline dims, int64 dimensions, heap-owned under 1.2's regime).
Survey findings and two spec fixes are recorded there and in the 1.3
README; the prototype's floating `tfp_ops.cpp` is the obsolete design
(D-036/D-037 supersede it deliberately).

## 5. Frontend-stability items (settle before the frontend is called frozen)

These would force token-table renumbering *after* the "built once, in full" freeze.

| # | Proposed | Item | Source |
|---|---|---|---|
| ~~**G-1**~~ | **D-230** | **SETTLED (user decision 2026-09-01), LANDED at 1.4.8 step 2: one kind `TY_FLAGS`, four families as keywords, the members generated prelude constants from TYPE_REFERENCE §8's marked region; `whence`/`fcmd`/`advice` per the user's families answer (1.4.7b).** ~~D-044's seven bitflag types~~ (`oflags`, `prot`, `mflags`, `fmode`, `fcmd`, `advice`, `whence`) are listed in AST_REFERENCE as parser-known builtins, are required by every syscall wrapper, and exist nowhere — a user type named `oflags` silently shadows a decided builtin. Run the generator to add them now, or supersede D-044 with a library-enum design. Decide before the frontend freeze. | grammar #9 |
| ~~**G-3**~~ | **D-191 — SETTLED at 1.1-close (the user's call): a NEW CYCLE.** The exotic numeric tier is **cycle 1.3**, inserted before self-hosting (the D-183 renumbering precedent, applied again: self-hosting 1.4, verification 1.5, Astrée 1.6 — everything must land before the fixpoint re-close and the verified artifact anyway). The rung strings cite "1.3 (G-3)"; the cycle's map is `1.3/README.md`. ~~The exotic numeric tier has no owner: `vec2/vec3`, `matrix`, `tensor`, `tfp*`, `frac*`, `dim256`, `simd`, `complex` parse and resolve but no cycle lowers them.~~ | ~~before 1.1 closes~~ | 0.9.7 sweep |
| ~~**G-2**~~ | **D-231** | **SETTLED (user decision 2026-09-01), LANDED at 1.4.7b step 2: the sub-byte widths are STRUCK from the grammar and the wide ladder (`int512`–`int4096`) is pinned with layout rows and one executed conformance case (`wide_ladder.npk` runs arithmetic at 1024/2048/4096 bits; `widths_struck.npk` shows `int4` is no longer a type).** D-231's own text opens "G-2, answered in two halves"; this row still read "next free" until the 1.4.9 close read the table. ~~The full integer-width set (`int1/2/4`, `int512`–`int4096`) is accepted by lexer/impl but has no layout in TYPE_REFERENCE, and `tt_int` computes size/align 0 for sub-byte widths. Enumerate with a stored-as-byte rule, or trim the grammar.~~ | type-sys #18 |
| **G-4** | D-163 | **`raw` and `drop` are unchecked, and a bare `f();` discards a `Result` with no keyword.** `type_unwrap` verifies only that the operand is a `Result<T>`; `emit_raw` is an unguarded `extractvalue`; `drop` evaluates and discards; `check_stmt` types an expression statement without reading its type. The `never fails` contract (D-002) names the property all three depend on but is attached to nothing they can see, and D-149 is retiring it. Measured: 262–300 `raw` sites and **741 `drop` sites** in `src/` sit on callees that `fail` — the table accessors (`ast_id_at` ×92 …) and the driver's own stage calls (`drop check_module(…)`), each continuing on node 0 or past a failed stage. Decide: `never fails` on any function, checked; `raw` and `drop` licensed only by it; the value-less statement forms a closed list; the spawn form's error routed to the D-062 join; always on. **SETTLED as D-163**; ~~struck at 1.1.0~~ — the contract, its checks, and the statement rules landed there (the instrument measured the REAL debt: 8,921 may-fail `src/` sites — see `1.1/raw_sweep_worklist.md`); the refusal itself flips at 1.1.2 after the 1.1.1 sweep. | user, post-1.0.0 |

---

## 6. Doc-sync backlog (not blocking; corrosive in aggregate)

Theme F of the audit: ~15 reference-doc passages describing removed constructs or
superseded layouts as current. Individually developer-comfort; collectively a
hazard because an implementer trusts the reference and builds the dead design.
**Not gated to a cycle** — run as a single doc-sync pass, ideally alongside 0.9
(when the type-system docs are being touched anyway). The catalogue with line
citations is [../audit-0.8-close/total_audit.md](../audit-0.8-close/total_audit.md) Theme F. The
`check_decisions_current` instrument (0.9.1) makes the backlog self-reporting so it
does not silently regrow.

**D-233 adds a batch (2026-09-01)** — rationale prose in living references
still citing Astrée as the verification endpoint; each reads "the evidence
campaign (D-233)" now: `TYPE_REFERENCE.md:348` ("tractable for Astrée"),
`TYPE_REFERENCE.md:710` ("the single Astrée run"),
`TRAITS_REFERENCE.md:396` ("Astrée analyses monomorphized output" — doubly
stale, the input was never monomorphized C),
`CONCURRENCY_REFERENCE.md:570` ("the single Astrée run"),
`IO_REFERENCE.md:181/279` ("io_uring refused before Astrée" — D-184's rule
now reads "before the evidence campaign closes"), `SWITCH.md:79`. Line
numbers as of 23e9e79; sweep with the next doc-sync pass. **Swept at the 1.4.9
close (2026-09-02)** — all six read "the evidence campaign (D-233)" now, and
`SWITCH.md`'s line with them.

**1.5.2 adds an item (2026-09-04, found at step 5's doc pass)** —
`src/frontend/parse_decl.npk` spells `DECL_THREAD` and `DECL_DISCARDED` as
ONE bit (2048). They are read on different node kinds (a function's flags,
a parameter's), so no program can meet both today, but a flag two names
share is the next collision waiting, and nothing checks that the `DECL_*`
values are distinct. Owed to 1.5.2b, the next subcycle to touch the
frontend: a bit of its own for `DECL_THREAD` (512 is unused) and a
whole-tree check that every `DECL_*` value is unique, the walkers-total
shape — a fact two names share is the "absent and false spelled the same"
class D-227 was written against. **Planned as 1.5.2b step 0 (2026-09-05,
`1.5.2b.md` L-12): `DECL_THREAD` = 512 and `check_decl_flags_unique` in the
harness.** **LANDED (2026-09-05, 1.5.2b step 0): `DECL_THREAD` is 512, and
the harness's `check_decl_flags_unique` reads every `DECL_*` row of
`parse_decl.npk` on every full run, refusing a shared value, a value that is
not one bit, and a row it cannot read -- each by name, never a silent skip.**

---

- **TYPE_REFERENCE §3.2 lists `==` on `string` as permitted; the compiler
  refuses it (`NITPICK-TYPE-034`) and the answer is `.eq` / `string_equals`**
  (D-250: comparisons of owning and named types are calls, not operators). Found
  by the library workbench (`nitpick-time`, 2026-09-05); the table is what is
  wrong. Owner: the doc-sync pass at 1.5.3's close. **FIXED at 1.5.3 step 3
  (2026-09-06): §3.2 reads `.eq`/`string_eq` and `.cmp`, the operators
  refused.**

## 6b. ~~Float and char `ToString` await floor support~~ CLOSED by D-193 at the 1.1 interlude

D-168 renders `&{ x }` through `ToString`, and the prelude supplies it for every
scalar EXCEPT the floats and the characters. **Floats**: a correct `flt32`/`flt64`
-> decimal string needs a shortest-round-trip conversion the floor does not have,
and a naive one drifts -- the exact numeric-drift failure the safety rationale
forbids. **Characters**: rendering a code point needs an OWNED string built from
its UTF-8 bytes, and the floor's `string_from_bytes` only WRAPS bytes (it would
leak the wild buffer per call), so this needs a floor `npk_char_to_string` (or a
copying builder). Landed at 1.0.9d for the integer widths, `bool`, the `tbb`
widths (ERR), the kernel identifiers, and `uint64` (unsigned rendering in the
prelude); a `flt`/`char` interpolation refuses `TYPE_NOT_STRINGABLE` until the
floor gains `npk_flt_to_string` / `npk_char_to_string`. **Owner: a dedicated
numeric/encoding task, scheduled post-1.0** -- small in surface, correctness-
critical, so each gets its own careful attention rather than riding a larger commit.
This was the user's call at 1.0.9d ("go with your recommendation... so long as it
all gets done eventually").

**CLOSED (D-193), with ZERO new floor surface** — both landed as prelude
Nitpick through the ordinary `ToString` impl lookup. Floats:
`flt_bits_shortest`, Steele–White/Dragon4 over `uint2048` (exact, ties to
even, subnormals and the unequal gap; NO wide division by construction, so
no libcall can appear); bits reach it through a store-as-float/load-as-
integer scratch buffer because `=>!` stays a value conversion. Format:
fixed for decimal exponent in [-4, 15] (mandatory ".0"), `d[.ddd]e±EE`
outside, "±0.0"/"±inf"/"nan". `flt_tostring.npk`: 238 flt64 + 115 flt32
python-generated known answers, values built FROM BITS. Chars:
`codepoint_to_string`, TOTAL — `char32` scalar, `char16`/`char8` code
UNITS with U+FFFD for surrogates/non-ASCII-lone-units/out-of-range — the
owned copy riding D-186's `string_slice` (which is what removed the floor
blocker this row was parked on). `char_tostring.npk`.

## 6c. Planned research: bug/vulnerability statistics vs. language coverage (owner: the user; run before 1.5's trigger)

Stated by the user at 1.1.13a: once the initial design's build is done —
"while we are building libraries or something" — they intend to research
**statistics on the most commonly found sources of errors, crashes, bugs,
and hacks** (CWE-top-25-style data and the like), then audit how well
Nitpick's shipped checks actually cover those classes, and where coverage is
missing, decide whether anything can be added **within reason**. Until then
the standing instruction is to implement and perfect what is already
planned rather than grow new coverage scope mid-build.

**Timing constraint (the D-183-era rule applies):** anything that review
motivates must land BEFORE the evidence campaign closes (D-233) — re-verification is
unaffordable — so the review itself must happen before 1.6 pulls the
trigger, not drift past it. Natural slot: alongside 1.4/1.5, when the
compiler is self-hosting and the library tier is being grown anyway.

**The research began at the 1.4.0 open, the reports are IN, and the
review is DONE** — the briefs and the eight reports live in
`meta/roadmap/research/` (r1..r8.md), distilled into decision-grade
digests in `meta/roadmap/research/digests/`, and the audit
(`meta/roadmap/research/COVERAGE_AUDIT.md`) was **ratified whole as
D-210…D-214** (with the same ratification settling S-4→D-215,
S-5→D-216, and the 1.5 batch early as D-217…D-221). This section's
mandate is discharged; what remains of it is the standing rule that
anything the ratified additions produce lands before 1.6's trigger —
which their scheduling already satisfies (1.4.2b, 1.4.3b, 1.4.4, 1.4.8,
1.5.7).

## 7. Ordering

1. **LIVE-1, LIVE-2 first** (0.9.0) — shipped safety holes.
2. **C-1 (mangling) before 1.0 anything** — the whole cycle's symbol scheme.
2a. **G-4/D-163 decided before 1.0's call-form subcycles, implemented before 1.1 anything** — the cycle's own code is written under the licence, not swept after it, and the spawn form's error channel exists before the join is designed.
3. **B-2 (Duration) + C-7 (coro) before 1.1 anything** — the substrate.
4. ~~**C-10…C-12 before 1.4 anything** — the fixpoint must be measuring the right
   thing before self-hosting is declared.~~ **DONE** — D-202…D-204 at 1.4.0 and
   1.4.5; self-hosting declared at 1.4.9.
5. **C-14…C-17 before 1.5 anything**, and ~~C-19 answered before 1.5
   exits~~ — C-19 CLOSED by D-232→D-233 (2026-09-01); 1.6's bring-up gate
   is ordinary scheduled work with no external dependency.
6. **G-1, G-2 whenever the frontend freeze is formally declared** — cheap now,
   re-verification later.

This is more than one sitting's work, as the previous queue's closing note said of
its own. That is expected and is not a reason to compress it.
