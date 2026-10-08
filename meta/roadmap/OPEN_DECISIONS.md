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
| ~~**S-102**~~ | **SETTLED as D-322 (2026-09-25, the evening; the user: "i have looked at those things and read your recommendations and i think they are fine. lets go with those.").** ~~Which engine wins the gate?~~ Asked with the scorecard at 1.6.0 step 5 (`1.6.0.md`, the step-4 record's last section). | NIKOS wins by D-320's rule 1 (Clam: no memory result, no `div-zero` check — disqualified in all three named properties; NIKOS: disqualified in `uaf` alone); Clam/Crab decided out with its scorecard kept; the measurement that decided is row 2. E-8 (the emission states its data layout) settled in the same sentence as 1.6.1's first step. |


## 2e-quater. ~~Three questions 1.6.1's planning raised, and DEF-107's~~ ALL SETTLED 2026-09-26 (the user: "both of your recommendations sound fine to me. lets ratify those.") — S-103, S-104, S-105 as D-324; S-106 as D-325 (the freeze of a view's root, measured on the tree first, 1.6.1 step 0)

| # | Item | Recommendation |
|---|---|---|
| ~~**S-103**~~ **SETTLED as D-324.** | **Where the NIKOS commits live.** NIKOS is the user's own repository (`alternative-intelligence-cp/nikos`, D-319); the port's commits are written by the compiler seat, one item per commit, each pinned in this tree by commit and digest (D-322 (6)) and measured by the gate's runner before adoption. | The seat commits to a branch `nitpick-port` of that repository and the user merges it to NIKOS's main at the subcycle's close — the user's repository keeps the user's merge, the pin keeps the evidence honest either way. |
| ~~**S-104**~~ **SETTLED as D-324.** | **The second decider, on by default.** With `llvm.assume` imported, the verified build's elided guards become assumptions NIKOS trusts; asked through the assert prover first, NIKOS DECIDES each one and a refuted assume is a ledger class of its own (`assume-refuted`, a stop sign) — leg A independently re-deciding leg B's discharges at ~40 lines. | On by default (`[verify.nikos] assume-check = true`); the plain form (no assumes) and the verified form are both analysed, so the trusting reading is never the only one. |
| ~~**S-105**~~ **SETTLED as D-324.** | **The stage's scope and cadence.** Every real-backend program's plain emission and the compiler's plain emission (7 s at the gate) on every full run; the two gate programs' whole-program forms in the gate's runner as the executor-wall measurement; the compiler's whole form at 1.6.3's measured cadence. | As stated: a run's cost is not a reason to hesitate (the user's standing view), and the whole form's cadence is decided by measurement at 1.6.3 as the README row says. |
| ~~**S-106**~~ **SETTLED as D-325.** | **DEF-107: is a view's root FROZEN while the view is live?** The recap: D-249 (1.5.1b, S-22) made a view-maker's result a borrow for the escape analysis — a view cannot leave its frame — after the workbench returned one and read the free poison; D-286 (1.5.5, S-60…S-62) decided the aliasing half of D-004 as LEXICAL claims (`$$m` excludes, `$$i` shares, `@` claims nothing, non-lexical lifetimes and two-phase borrows OUT) and its conflict table names claims and held `@`s, never a view; D-266 (1.5.2h, S-41) froze a lending `pick`'s SELECTOR while an arm's view is live (TYPE-067) because the same hazard was found there. A view-maker's view (`string_from_bytes`, `string_bytes`) and a range view (`l[lo...hi]`, `s[lo...hi]`) have the escape rule and no freeze: a write to the root — an assignment, `@root` handed to a callee, a `$$m`, a pointer-receiver call, a stateful operation — while the view is live compiles, and the view then reads rewritten or freed memory (measured, six lines, both legs). | **Recommendation:** freeze the view's root for the view's LEXICAL lifetime — from the view local's declaration to the end of its block, exactly D-286's shape for a held claim and D-266's for a pick arm — refusing every write-capable access to the root in that span with one code (the view is a borrow; the rule is D-286's table with the view as a third party), measured on the tree first (the sites where a view of a binding is followed by a write to it in the same block are expected to be few, and each is a real hazard). The alternative — the programmer's contract, documented — leaves a use-after-free reachable in safe code, which the floor forbids. A plan step of 1.6.1's size, its own subcycle if the measurement says so; the listener writes its views as copies until it lands. |

## 2e-quinquies. A question 1.6.1 step 0 raised — S-107, the library listener's O-N27 (registered 2026-09-26; a design input, not a defect)

The listener's text, verbatim (nitpick-libs_s6, 2026-09-26; their `meta/OPEN_QUESTIONS.md`, `RECORD.md`, `BOARD.md`):
"A low-priority design input for S-106 (DEF-107), registered here as O-N27. It is not a soundness defect: the
borrow tracker taints a call's RESULT by SIGNATURE, so it refuses a correct program. From nitpick-libs_s6. Three
of our agents met this independently: nitpick-regex's triage at 3d15ac9, and nitpick-time's 0.1.4b planner and
worker at c3bdae2. Reproduced here at c3bdae2: struct:Box = { string:s; }; func:make = string(Box->:b) never
fails { pass string_concat("small", "!"); }; // cannot alias b — wrap(): Box:b = …; pass raw make(@b); ->
NITPICK-BORROW-001 "a borrow cannot travel up" (D-004 rule 2) — wrap(): Box:b = …; string:r = raw make(@b);
pass r; -> BORROW-001, the same — CONTROLS: the box as a PARAMETER of the returning frame (`wrap_p(Box->:b) {
pass raw make(b); }`) compiles and runs 0; the same string built inline in the owning frame compiles and runs
0. So any `f(Container->) -> string` that BUILDS its result, rather than borrowing it, cannot be returned from
the frame that owns the container, whatever its body does. It is sound, because it accepts nothing unsafe, but
it costs a whole class of constructor. Our libraries meet it at nitpick-time's cycle 0.4 (a formatter's `pass
raw bytes_take(@sink)` wrapper, refused even with a copying take) and at nitpick-regex's cycle 0.6 (replacement
text). WHY NOW: DEF-107 and this are the two faces of the same thing, D-249's view rule keyed on a call's SHAPE
rather than on the value's PROVENANCE. DEF-107 is a false accept (a view outlives its root's mutation). This is
a false reject (an owned copy treated as a view). Regex's triage recommended fixing them as one idea back on
2026-09-06: track where a result came from, not what the signature could allow. If S-106's freeze is decided
with provenance in view, it may settle both. If only one can be had, DEF-107 first: a false accept is a
use-after-free, a false reject an inconvenience. Nothing of ours waits on it today. The working shapes (build
and consume in one frame, or take the container as a parameter and return one level up) are what our code
already does."

| Question | Where it stands | Recommendation |
|---|---|---|
| **SETTLED as D-326 (2026-09-26).** ~~**S-107**~~ — should D-004 rule A's `holds` marking read the callee's summary instead of the call's shape? The recap: D-004 (0.5.0) made a call's result a possible borrow of every `@`/`$$` argument when the result can carry a pointer (rule A, "`launder` closed"), a SHAPE rule because the escape analysis never looked into callees; D-249 (1.5.1b) extended it to views; 1.6.1 step 0 built, for the freeze (D-325), exactly the provenance the listener asked for — per-function summaries at the escape fixpoint: what a result may VIEW (with field paths), what it CARRIES (a pass bit), what a body STORES through each pointer parameter, what it WRITES — and used them for the view parties only, leaving rule A's marking as it was. So `string:r = raw make(@b)` with `make` returning an owned copy is refused at `pass r` today (BORROW-001) while the summaries know `make` views nothing of `b`. | The mechanism exists and is measured; the marking's refinement is a RELAXATION of an accepted refusal (fewer programs refused; nothing unsafe admitted, since a summary's absence — a `dyn` method, a function value — keeps rule A's shape), which changes what the language accepts and so is the user's to ratify, with a D-004 dated note. | **Recommendation: refine rule A's marking by the summaries** — a call's result holds a borrow of `x` when the callee's summary says it may VIEW or CARRY `x` (its value or address), or when the callee is unknown; the tree's and the listener's exposure measured before landing (a relaxation needs no advance notice, D-239's rule is for refusals added). One idea settles both faces, as the listener said. A step of 1.6.1's size, after step 0's harness. **RELAYED APPROVAL, 2026-09-26** (the library listener put the question to him with the recommendation as written and quoted his answer: "as far as I can tell your recommendations for all the questions you asked was fine. Please proceed with them."); the D-number waits for his word in the compiler session, where ratifications are recorded. **SETTLED 2026-09-26 as D-326** (the user, in the compiler session: "the recommendations from earlier you asked about are fine. go with those."); lands as **1.6.1b**, planned execution-grade after 1.6.1 step 1. **LANDED at 1.6.1b (2026-09-26): 2,157 files of the tree and the listener's repositories under both checkers, zero sites moved outside the two new tests** |
| **SETTLED as D-327 (2026-09-26).** ~~**S-108**~~ — a `never fails` by-value read of a generic container: a prelude `Copy`-like marker, or a `never fails` `clone` for the types whose clone cannot fail? The recap: D-264 (1.5.2f) made a bare `T` move-only in a generic body, naming `.clone()` under `Clone` as the way to read one out; the prelude's `Clone.clone` is fallible (`Result<Self>`), so in a `never fails` generic body a clone is `relay`-less and unusable, and `List<T>` has no by-value get. The library listener's design input (2026-09-26, with O-N28): nitpick-regex answers with its own `Pod` marker trait, so that `vec_get<T: Pod>` reads through a `pod_copy`. **Recommendation:** a prelude marker trait `Copy` (the name is the reading, D-239 would own it) with `impl` for every copyable scalar in the generated `scalar-impls` region and a derivable form for a struct of copyables, and `List<T: Copy>`'s `list_get(l, i)` returning `T` `never fails` — one rule (a copy is a copy), no second clone; a `never fails` clone for owning types is not offered, since an allocation can fail. A language question, the user's. **RELAYED APPROVAL, 2026-09-26** (the same relay and the same words cover this row: the `Copy` marker with a derivable form and a `never fails` `list_get` under it, no `never fails` clone for owning types); the D-number waits for his word in the compiler session. **SETTLED 2026-09-26 as D-327** (the same sentence); lands as **1.6.1c**, planned execution-grade after 1.6.1 step 1, with an ADVANCE notice (the name is D-239's). **LANDED at 1.6.1c (2026-09-26): 6,199 rows before and after, 5,685 shared, zero verdicts moved, zero pairs fell; the 514 re-keyed rows a move of type-id names.** | ~~the user~~ D-327 |
| **SETTLED as D-328 (2026-09-26).** ~~**S-109**~~ — who owns `to_cstring`'s block (DEF-122, the library listener's F-007: every call leaks `len + 1` bytes). The recap: D-049 (0.6) made `cstring` a distinct NUL-terminated type so no unterminated bytes reach a syscall and an interior NUL is a refusal, and left it a VIEW of two words (`{ ptr: wild char8->, len }`, TYPE_REFERENCE §3.2.1; no arm in `type_drops_recorded`); D-183 (1.2) gave every owner a drop and `string` its borrowed case at `cap == 0`; D-186 made `string_slice` an owned copy. `to_cstring` is a producer of a fresh copy (`npk_alloc_internal`, managed storage, invisible to D-151) whose result has no owner anywhere: a leak by construction in every program that converts a path. Two shapes, either a language change with an advance notice: (a) `cstring` becomes string-shaped — `{ ptr, len, cap }`, `cap == 0` a borrowed body (a literal, an `argv`/`environ()` element), `cap > 0` `to_cstring`'s, the string's drop body, move-only (TYPE-046); the floor's five `cstring` entries and `_start`'s two slices change layout (24-byte elements), their spec rows move, every `cstring` binding becomes move-only (a binding-to-binding copy is `.clone()`), no call site re-spelled; (b) `to_cstring → Result<buffer>` plus `cstring_of(buffer) → cstring` (a view by the reference's `Views` column, the buffer's root frozen while it lives), no floor layout change, every one of the 126 call sites in the tree and every one in the listener's repositories re-spelled to two lines. Asked by the compiler seat 2026-09-26 (`1.6.1d.md` §3). | OPEN — blocks 1.6.1d step 3's DEF-122. | **(a)**: one rule for the two string kinds (a `cstring` is to the kernel what a `string` is to the program, and a `string` already carries its ownership bit and its literal case); the class "a producer of storage whose result has no owner" closed by the TYPE, not by every caller remembering two lines. |
| **SETTLED as D-329 (2026-09-26).** ~~**S-110**~~ — the range value keeps its spelling (DEF-128, the listener's F-012: `for (uint8:i in 0u8..255u8)` and `for (int8:i in 0i8..127i8)` run ZERO times, silently; an unsigned range across the sign bit likewise). The recap: D-145 (0.9.6) normalised every range value half-open at construction — `lo..hi` stores `hi + 1` — so no consumer asks which spelling built it, and accepted that "an inclusive range ending at the carrier's maximum wraps under the +1 and the bounds guard traps it — a loud outcome for a corner with no honest answer"; true of a SLICE (its guard fires), false of a `for` (no guard: zero trips in silence), and the corner is the everyday byte loop. The other cause, the head's fixed signed compare, is the emitter's and needs no decision. Asked 2026-09-26 (`1.6.1d.md` §2.5, §3). | OPEN — blocks 1.6.1d step 2's DEF-128 (DEF-129 and DEF-130 land without it). | **`range<T>` is `{ T:lo; T:hi; bool:inclusive }`**, built by the literal from its own payload with no `+ 1`, read by every consumer with the flag; D-145 amended on the representation alone (the slice reads the same value). A wider `hi` fails at `uint64`, a `{lo, count}` pair at a count of 2^64, and refusing `lo..MAX` refuses the byte loop. |
| **SETTLED as D-330 (2026-09-26).** ~~**S-111**~~ — `<=>` on floats (DEF-131, the listener's F-015: the operator was never lowered; the checker types it `int32` over any ordered pair). Landing it asks what a NaN answers: `-1`, `0` or `1` cannot say "unordered", and `0` would claim equality. Asked 2026-09-26 (`1.6.1d.md` §2.13). | OPEN — blocks nothing but the float arm of 1.6.1d step 4's DEF-131. | **Refused for floats** (a new code at the operator, "no total order — compare with `<`, `==`, `>` and decide the unordered case yourself"); lowered for every other ordered kind, the twisted kinds through their compare-trap path. |
| **SETTLED as D-331 (2026-09-26).** ~~**S-112**~~ — a constant division that cannot succeed (DEF-133's d7, the listener's F-017). The recap: D-310 (1.5.8b) refuses a certain constant `+ - *` overflow wherever the folder decides an operand pair (TYPE-076 at the node); OP_REFERENCE §1.1 promises the same for a constant division by zero and `MIN / −1` (TYPE-004), and the compiler keeps that promise only where the folder is ASKED — a `fixed` initialiser, `comptime` — while a local `int32:x = 5i32 / 0i32;` compiles and traps `DivByZero` at run time. Asked 2026-09-26 (`1.6.1d.md` §2.15). | OPEN — 1.6.1d step 4's DEF-133 (d7). | **D-310's reach extended to `/` and `%`**: a certain constant division by zero or `MIN / −1` is `NITPICK-TYPE-004` at the node wherever the folder decides the pair; a refusal added, announced in advance. The alternative is the sentence moving to "where the folder is asked". |
| **SETTLED as D-332 (2026-09-26).** ~~**S-113**~~ — a rejection test's silent site (found by 1.6.1d step 1's sweep). D-237 (1.4.8b) holds a rejection file to the SET of codes it names, so a site the checker never reports is invisible whenever its code appears elsewhere in the file: `tests/analysis/rejection/aliasing.npk`'s "a write THROUGH a shared claim's holder" case carried its `expect-error: NITPICK-BORROW-013` line since 1.5.5 and was never reported until DEF-123's fix — the exact hazard the listener's F-008 found, documented in our own suite for a cycle. Asked 2026-09-26. | OPEN — a runner rule (both runners and their self-checks); no step of 1.6.1d waits on it. | **Both runners also hold the COUNT of reported sites per code to the count of `expect-error` lines naming it** (the files already write one line per site), so a silent site fails by name; a file that means several sites under one line is re-spelled. |
| **SETTLED as D-333 (2026-09-26).** ~~**S-114**~~ — a `cstring` across a channel or a spawn after D-328 (asked by the compiler seat 2026-09-26 with the step-3 report). The recap: D-235 (1.4.7b) decided every kind as a channel element and `cstring` refuses as a BORROW beside pointers and slices (`type_contains_borrow_recorded`), because its bytes were never its own; D-328 gives it `string`'s shape and rule — an owned body at `cap > 0`, a borrowed one at `cap == 0` only of a literal or an `argv`/`environ()` element, both process-immortal, exactly `string`'s two cases, and `string` rides. The user, 2026-09-26: "go with your recommendations on those two questions. they look fine to me." | SETTLED — lands with 1.6.1d step 3b (DEF-122). | **A `cstring` rides a channel and crosses a spawn as a `string` does**, D-235's row amended; the alternative (keeping the refusal) cost nothing today, since nothing in the tree sends one. |
| **S-115** — which child's error a scope's join relays (DEF-146, the library listener's F-020). The recap: D-163's exception paragraph (settled with the user at 1.1's planning; C-7/C-9) and CONCURRENCY_REFERENCE §2.2:79 say the enclosing scope's join relays "the first child error, verbatim (D-080), after every child has finished"; D-207 (1.4.4) moved the join into the scope's unwind and kept the arbitration reading one slot; the emitter relays the LAST-spawned child's error (the join walks the LIFO chain and keeps the first error it meets). "First" is defined nowhere: spawn order (the program's text; the same under every schedule) or first in time (a stamp the floor does not keep; two runs of one program could relay two errors under the explorer). Asked by the compiler seat 2026-09-26 (`1.6.1e.md` §2.3, §3). | **SETTLED as D-334** (the user, 2026-09-26: "I am fine with the s-115 thing. go with what we have unless there is a compelling case not to do so."); 1.6.1e step 1 landed under the recommendation the same day (`join_first_spawned.npk` pins spawn order; the listener's `ctl_j3`, slow spawned before fast and expecting E1, answers E2 by the rule — the two readings differ exactly there). | **Spawn order**: the earliest-spawned child that failed, the function's own error still winning over every child's. |
| **S-116** — what `string<char16>` and `string<char32>` MEAN (found at 1.6.1e step 2, by the sweep of the library listener's M11 programs ty0319b/ty0320/ty0321). The recap: TYPE_REFERENCE §3.2's layout table lists `string` = `string<char8>`, `string<char16>` and `string<char32>`, each `{ptr, i64, i64}`; DEF-152 (F-026) made every builtin that takes no type arguments refuse them (`tfp64<Meters>` had been accepted and its unit dropped), and the 1.6.1e plan §2.9 kept `string`'s spelled forms. MEASURED: the compiler has always ACCEPTED the three forms and IGNORED the element width — each is exactly `string`: its elements are bytes, `.len` counts bytes, a read yields a byte — so `string<char16>` is a written argument the compiler does not honour, the class DEF-152 closed for `tfp64<Meters>`. Step 2's first form refused all three (TYPE-016), contradicting the plan and the reference; the landed form keeps the three spellings accepted as before and refuses every other argument on a `string` (`string<int32>`, two arguments, a nested `string`: TYPE-016). | **SETTLED as D-335** (the user, 2026-09-26: "if we are ever gonna have them we go ahead and plan for and implement them now"): wide strings are IN, designed and implemented as subcycle 1.6.1f before the freeze; the recommendation (decide them out) was not taken | **Decide wide strings OUT**: `string<char16>` and `string<char32>` refuse (TYPE-016, "a `string`'s elements are bytes: UTF-8, D-193"), their two table rows struck, `string<char8>` kept as the one spelled alias of `string` — a spelling must mean what it says (the blueprint rule), and a wide string is a feature nobody has designed, not a layout. If Nikola needs UTF-16 or UTF-32 text, the alternative is to DESIGN it now (element type, indexing, conversion, `ToString`) and land it before the evidence campaign closes; accepting the spelling while meaning bytes is the one reading this recommends against. |
| **S-117** — a FROZEN COMPATIBILITY CORPUS as the instrument of the permanent freeze (raised 2026-09-26 by `nitpick-compiler_21`). The recap: the user plans a PERMANENT language freeze at feature-complete -- "a program written in nitpick next year would still compile and run 30 years from now", no breaking change except for security -- and clarified the same evening that the promise is about PROGRAMS, not tools: the compiler, the floor and the toolchain may all move (a newer LLVM, a new target) and the language absorbs it under the hood. Nothing today proves a change is invisible to programs; the per-landing two-checker sweep and emission comparison are its seed, but they compare one landing with its parent, and their corpus is whatever the trees hold that night. | **SETTLED 2026-09-30 as D-336** (the user: "go with your recommendations on all three"): YES -- a corpus frozen AT the freeze, a stage of both runners; a pre-freeze subcycle on the roadmap (`ROADMAP.md`, "The freeze"), nothing built now. | **A corpus, frozen at the freeze**: the tree's suites, the libraries' programs and the fuzzer's accepted programs, each with its recorded verdict (the diagnostic set, or the exit code at both legs); every later change to the compiler, the floor or the toolchain must reproduce every verdict, a stage of both runners, a moved verdict a red run unless it is the named security exception. A language version row in the manifest was considered and NOT recommended: it can be added the day a break is first needed, a missing row read as version 1, so adding it now is complexity with no present need. |
| **S-118** — may `comptime` ORDER STRINGS (raised 2026-09-30 by `nitpick-compiler_22`, reading the library listener's F-027 row `mc0309b`). The recap: MACRO_REFERENCE §8's table lists string "ordering" among what the evaluator can do (recovered from the prototype's `COMPTIME-005`); a string is ordered only by `a.cmp(b)`, which answers an `Ordering` enum (D-093, D-330), and the evaluator holds no enum value and runs no `pick`, so the row cannot work (TYPE-004, measured on `5fbaf4a`). | **SETTLED 2026-09-30 as D-338** (the user: "go with your recommendations on all three"): IMPLEMENT -- 1.6.1e step 3b, its own landing after step 3, planned execution-grade first. **LANDED 2026-10-01 as landing 90** (`1.6.1e.md` §2.10d and its record). | **Implement**: a payload-less enum value, `==`/`!=` on it, `pick` over constants, `a.cmp(b)` on constant strings held equal to the run-time order by a test. The alternative -- striking "ordering" -- leaves compile-time code unable to order strings for good once the freeze is permanent. |
| **S-119** — the lock levels of a dynamically dispatched call: the FLOOR under a `<=` bound, and what the prelude's traits declare (raised 2026-09-30 by `nitpick-compiler_22` from the probes of the lock walk at 1.6.1e step 3; DEF-181). The recap: D-056 (settled with the user at the concurrency planning) proves lock-order freedom by LEVELS — acquisition strictly increases — and says a dynamically dispatched method "declares its maximum acquisition level … and implementations are checked against it. An undeclared method may not acquire at all"; D-113 spells the clause `acquires <= N`. Step 3 made the last sentence true: an implementation of a trait method that declares no clause, reaching any level, is LOCK-002. Two things follow. **(a) THE FLOOR.** `acquires <= N` is a ceiling only: holding level 2 and calling through a `dyn` bounded at 3 passes although an implementation may take level 1 — a downward acquisition the proof does not see (D-056 itself lists "a declared-but-broad dynamic bound" among what its second layer contains by deadlines, not proves absent). **(b) THE PRELUDE'S TRAITS** declare no level, so since step 3 no implementation of `Writer`, `Reader`, `Iterator`, `ToString`, `Eq`, `Ord`, `Hash`, `Clone` or `Debug` may take a lock or wait on a channel, and a program cannot add the clause to a trait it does not own: a mutex-guarded `Writer` cannot acquire inside its own `write`. None exists in the tree or the libraries today (the step's sweep: zero sites). | **SETTLED 2026-10-01 as D-339** (the user: "all the recommendations look fine to me. lets go with those."): (a) close the floor: a call through a `<=`-bounded method while anything is held is LOCK-001, and an exact `acquires N` on a trait's method means its implementations reach N and nothing else; (b) the prelude's traits stay undeclared — the guard is taken outside the call. *Until then:* OPEN — PUT TO THE USER 2026-10-01 by `nitpick-compiler_23`, with the numbers (measured 2026-09-30 on `9efe218`): an `acquires` clause appears NOWHERE in `lib/`, `npkg/`, `tools/`, nitpick-apps or any library, and in nitpick-libs only in the fuzzer's three reproducers (`vf0829`, `vf0830`, `vf0831`); a `<=` bound on a trait method outside our own tests is in ONE file (`vf0830`, a control); so closing the floor (a) costs our own four test files (`lock_levels` ×2, `lock_calls`, `lock_undeclared`) and nothing else, and no implementation of a prelude trait anywhere takes a lock or waits (b). 1.6.1e step 3c (DEF-181) is planned on the answer. | **(a) Close the floor**: a call through a `<=`-bounded method while anything is held is refused (LOCK-001), and an EXACT `acquires N` on a trait's method means its implementations reach N and nothing else — measured on the tree first. **(b) Keep the prelude's traits undeclared**: a lock hidden behind an I/O trait is behaviour that depends on which implementation stands behind the `dyn` — the kind the blueprint rule refuses — and the guard-outside pattern says the same thing where the reader can see it (`Guard<…>:g = relay await m.acquire(d); g.value.write(…)`); an inherent method (unbounded, statically dispatched) or a program's own trait with `acquires <= N` covers the rest. The alternative for (b), a declared level on `Writer`/`Reader`, fixes ONE number for every program and orders every write against every lock a caller holds. |
| **S-120** — WHOSE SCOPE A MACRO ARGUMENT RESOLVES IN (raised 2026-10-01 by `nitpick-compiler_23`, reading DEF-183 against D-057; probes `ha1`, `ha2`). The recap: D-057 (settled at 0.6.0) says "an identifier in a macro BODY resolves in the scope where the macro was written. Always", and that `#caller(NAME)` is "the sole way to reach the call site". An ARGUMENT is in neither sentence: it is the caller's own text, written at the invocation. The expander's own comment says "its identifiers are the caller's" — and the resolver then resolves the substituted argument with the body, in the module scope. So today a caller's local cannot be passed at all (`#twice(n)` with a local `n`: RESOLVE-002, "cannot find `n` in the scope this macro was written in"), and one that shadows a module binding is SILENTLY replaced by it: beside `fixed int32:shared = 100i32;`, `int32:shared = 5i32; #twice(shared)` over `macro:twice = (X) { X + X; };` answers 200, at both legs (DEF-183's silent face). | **SETTLED 2026-10-01 as D-340** (the user: "all the recommendations look fine to me. lets go with those."): R1: an argument is the caller's text and resolves at the invocation site; `#caller(NAME)` stays the body's one way out. *Until then:* OPEN — the user's, put to him 2026-10-01; DEF-183 lands on the answer (the mechanism for either reading is planned, `1.6.1e.md` §2.10c part B). **LANDED 2026-10-01 (landing 92).** | **R1: an argument is the caller's text and resolves at the invocation site** — `#twice(n)` works, the shadowing case answers 10; `#caller(NAME)` stays the BODY's one way out, an argument is the caller's way in, and nothing a body writes changes meaning. The alternative, R2: everything in an expansion resolves where the macro was written, and an argument identifier that would bind differently at the site (or exists only there) is REFUSED at the argument — no local can ever be passed to a macro. |
| **S-121** — A `buffer` HAS NO SAFE TYPED ACCESS (raised 2026-10-01 by `nitpick-compiler_23` from the library listener's IN-2 (1); numbers taken on `9efe218`). The recap: D-200 gave the language `buffer`, the managed owning byte cell, and STRUCK §23's draft verb family — typed access is `#ptr_add` + `<-` over `b.ptr`, or a `#wild_slice(b.ptr, n)`; D-315 says the `#wild_` spelling IS the acknowledgement, so a `#wild_slice` is tied to nothing. The first library that needed a byte sink (nitpick-time, `src/core/bytes.npk`) wrote five unchecked slices over a managed header's `.ptr` — three written through — and one PUBLIC function, `bytes_view`, that RETURNS one: after the buffer grows, the returned view reads freed memory (the library's own `tests/unit/bytes_view_lifetime.npk`: exit 0, reading the 0xAA poison), in a caller that spells no opt-out. `#wild_slice` sites: `src/` 6, tests 7, nitpick-libs 25, apps 0, `lib/` 0; over a managed header's `.ptr`: 5, all in that one file. | **SETTLED 2026-10-01 as D-341** (the user: "all the recommendations look fine to me. lets go with those."): (3) `b[i]`, `b[lo...hi]`, a view frozen like any other (D-325), with (1) as the belt: a `#wild_slice(x.ptr, n)` whose pointer is read from a managed header is tied to `x`. *Until then:* OPEN — the user's, put to him 2026-10-01. | **(3) Give `buffer` checked indexing and slicing** — `b[i]`, `b[lo...hi]`, a view frozen like any other (D-325) — so the safe spelling exists, **with (1) as the belt**: a `#wild_slice(x.ptr, n)` whose pointer is read from a managed header is tied to `x` (the freeze then refuses the growth while the view lives; the library's three write-through uses take a block each). (2), leaving it the author's and documenting D-315/D-325, leaves a hole in every consumer of such a library. |
| **S-122** — WHAT AN ENUM VARIANT'S EXPLICIT VALUE MAY BE, AND WHAT AN UNVALUED VARIANT BESIDE ONE IS (raised 2026-10-01 by `nitpick-compiler_24`; DEF-193, found writing DEF-189's test). The recap: the grammar reads `Name = <expression>;` in an enum body; `variant_tag_of` (1.4.1) has the whole rule — "the DECLARED integer literal where one is given, the POSITION otherwise" — and nothing checks a value at all (`check_decl` has no arm for an enum). So: a value that is not a bare integer literal is silently IGNORED and the position used (`X = 7i32 + 1i32`, `X = BASE`, `X = -3i32` are each 0 as a first variant); two variants may carry one tag (`enum:F = { A = 3i32; B = 3i32; }`: `F.A == F.B` is true; `enum:E = { A; B = 0i32; }` likewise, and a `pick` naming both is refused by `llc`, "duplicate case value"); a value past `int32` is truncated. 455 explicit values exist in the tree, the libraries and the applications, every one a bare literal (2 are macro invocations that expand to one). | **SETTLED 2026-10-01 as D-342** (the user: "all the recommendations look fine to me. lets go with those."): R1, final: a value is a bare non-negative integer literal that fits `int32`; no two variants share a tag; an enum gives EVERY variant a value or NONE — a mix is refused. *Until then:* OPEN — the user's. The interim refusals LANDED 2026-10-01 (DEF-193, `NITPICK-TYPE-093`: a value that is not a bare literal, a value outside `int32`, a shared tag); they foreclose neither answer. | **Refuse what cannot be honoured, now; decide the rest once.** (1) A value is an integer CONSTANT expression, folded by the checker (a literal, a negated literal, a module `fixed` name, arithmetic on those) — until the folder's value reaches the emitter, anything but a bare literal is refused by name. (2) Two variants of one enum never share a tag — refused at the second, naming the first. (3) A value fits `int32`. (4) An unvalued variant is its POSITION, as today (not the previous value plus one): with (2) a collision is a refusal, never a silent second meaning. *[Revised 2026-10-01 by the same seat, putting the question to the user:]* **the simplest rule, and final — R1:** a value is a bare non-negative integer literal that fits `int32` (what landing 85 enforces); no two variants share a tag; and **an enum gives EVERY variant a value or NONE** — a mix is refused, because `{ A = 5i32; B; }` makes `B` 1 (its position) where a reader from C or Rust expects 6, the look-alike trap the language's renaming rule exists to remove (three enums of our own tests mix today; none in the libraries or the applications). R2, the first form above: constant-expression values (`X = BASE + 1i32`, a negative tag), built and verified before the freeze — nothing among the 455 existing values needs one. |
| **S-123** — MAY A MACRO PARAMETER NAME AN EMITTED DECLARATION (raised 2026-10-01 by `nitpick-compiler_24`; MACRO_REFERENCE §10 has recorded it as open since 0.6.0, "rather than invented"). The recap: an argument is an EXPRESSION and substitution replaces identifier expressions (D-057, MACRO_REFERENCE §3). A declaration's NAME is not an expression, so `macro:m = (N) { func:N = int32() never fails { pass 5i32; }; }; #m(7i32);` emits a function literally called `N`, the argument dropped — "unimplemented rather than refused" in the reference's words, and the library listener's `mc0388` pins it as running. Step 3a refuses the half that answered WRONGLY (a body that declares the name and also writes it as an expression: each use was the argument) and left this half as it was. | **SETTLED 2026-10-01 as D-343** (the user: "all the recommendations look fine to me. lets go with those."): refuse at the macro's declaration (`NITPICK-MACRO-011`): a parameter that only names a declaration is an argument silently dropped. *Until then:* OPEN — the user's. | **Refuse it at the macro's declaration** (`NITPICK-MACRO-011`, the sentence saying a declaration's name cannot come from an argument): a parameter that only names a declaration is an argument silently DROPPED, the author almost certainly meant "the function named by the argument", and a second invocation is a duplicate-name error that names neither cause. The alternatives: keep the literal name (today), or substitute a name when the argument is a bare identifier — a real feature (generated names) with its own questions (D-128: a macro never renames what it emits; what a non-identifier argument is). `mc0388`'s expectation moves under the refusal; the library listener is told in advance. |
| **S-124** — MAY A MACRO BODY NAME THE TYPE PARAMETER OF THE DECLARATION IT IS INVOKED IN (raised 2026-10-01 by `nitpick-compiler_24` with DEF-192's fix). The recap: D-057 (settled at 0.6.0) says a name in a macro body resolves where the macro was WRITTEN — "Always" — and that `#caller(NAME)` is the sole way to reach the invocation site; `#caller` is an EXPRESSION form. A type's name never went through the resolver, and the type checker asked the generic parameters of whatever declaration it stood in first, so a body's `T` silently meant the invoking function's or `impl`'s `T` (DEF-192: beside a module `struct:T`, `#size_of<T>()` measured the caller's type, 8 for 4). The fix holds a type's name to D-057: it binds a generic parameter only where the body itself declares it. The consequence is this question: a field `V:item;` spliced into `struct:Box<V>`, or a method spliced into `impl:<T>:Stack<T>` whose signature says `T`, compiled BY THE CAPTURE and is `NITPICK-TYPE-001` now — a body has no way to name the landing declaration's type parameter (`Self` is a keyword and still means the landing type). Measured over 3,601 files of the tree, the libraries and the applications: none — the sweep moves no site outside the new test, so no macro body anywhere names a landing declaration's type parameter. | **SETTLED 2026-10-01 as D-344** (the user: "all the recommendations look fine to me. lets go with those."): R1: leave it so — generics and `#[derive]` are the language's tools for code over a type parameter. *Until then:* OPEN — the user's. DEF-192's fix landed on D-057 as written. | **R1: leave it so.** Generics and `#[derive]` are the language's tools for code over a type parameter; a macro is a local, hygienic shorthand, and nothing in the tree, the libraries or the applications reaches a type parameter through one. The alternative, R2: `#caller(T)` accepted where a type's name stands — the opt-out D-057 already has, extended to types: one production in type position and one exception in the rule, to be designed, tested and verified before the freeze. |
| **S-125** — MAY THE COMPILER COMPUTE IN THE CARRIED FAMILIES: FLOAT, FIXED-POINT, `tbb`, TERNARY (raised 2026-10-01 by `nitpick-compiler_25` with DEF-179's and DEF-197's fix). The recap: D-165 (settled at 1.0.9) says a module binding's initialiser is a compile-time constant — a literal of any kind, a sentinel, an aggregate of them, another binding, or `comptime(…)` — "and so is [refused] any expression the folder cannot fold"; D-310 (1.5.8b) made the folder exact for the plain integers: a constant means what the run time means. The folder has never computed in the other families: a float's arithmetic rounds, a fixed-point value's and a `tbb`'s and a ternary's saturate to a sticky ERR. Landing 87 found it folding them as plain integers into three silent wrong constants and made the rule explicit: a constant of a carried family is WRITTEN (a literal, a negated literal, `ERR`, another binding), so `fixed flt64:TAU = 6.283185307179586f64;` compiles and `fixed flt64:TAU = 2.0f64 * PI;` is NITPICK-TYPE-035, as is `comptime(2.0f64 * PI)`. | **SETTLED 2026-10-01 as D-345** (the user: "all the recommendations look fine to me. lets go with those."): R1, final: one evaluator per operation, the run time's; a derived constant is written as its literal and a table is computed by a function. *Until then:* OPEN — the user's. The refusals landed (they are D-165's own rule, and foreclose neither answer). No `fixed` binding of any carried type exists in 40,928 files of the tree, the libraries, the applications and the fuzzer's corpus. | **R1: written, not computed — and final.** One evaluator per operation: the run time's. A second implementation of float rounding and of each twisted family's saturation inside the compiler would have to agree with the emitted code bit for bit, forever, under verification (DEF-29, DEF-80 and DEF-197 are what a second evaluator of one operation costs), to save a reader a multiplication they can check by hand; a derived constant is written as its literal, and a table is computed by a function. The alternative, R2: the folder learns each family exactly — floats through the compiler's own correctly rounded conversion and IEEE arithmetic, the twisted families through their saturating rules — designed, tested against the run time and verified before the freeze. |
| **S-126** — A FLOAT LITERAL THAT DOES NOT FIT: INFINITY AND ZERO (raised 2026-10-01 by `nitpick-compiler_25`; DEF-205), AND THE 15-DIGIT RULE AFTER DEF-203. The recap: D-148 (settled at 0.9.9) says a numeric literal's value "must fit its type", verified at the literal (NITPICK-TYPE-031) — "the number in the program was not the number written, the exact drift this language exists to make impossible"; its table hands floats to D-143 (0.9.4), which gives `flt32` literals a 15-significant-digit limit and no range rule. So today `1.0e39f32` and `1.0e999f64` are +infinity and `1.0e-60f32` is 0.0, each in silence; and the 15-digit limit's stated reason (two roundings equal one below 16 digits) is false (DEF-203), while the fix for that — the compiler's own exact conversion — makes every `flt32` literal the nearest float whatever its length. | **SETTLED 2026-10-01 as D-346** (the user: "all the recommendations look fine to me. lets go with those."): (a) refuse a float literal that rounds to infinity, and a nonzero one that rounds to zero (`NITPICK-TYPE-031`, both widths; a subnormal result stays); (b) lift the 15-digit rule on `flt32` literals. *Until then:* OPEN — the user's. DEF-203 LANDED (88): the conversion exists at both widths and reports both answers, unread; the 15-digit rule is asked of every spelling of a literal while it stands (DEF-206). (a) is one test of each flag, (b) deletes the `flt32` half of `float_text_ok`; either is a refusal moved and goes in an advance notice. | **(a) Refuse a float literal that rounds to infinity, and a nonzero one that rounds to zero** (NITPICK-TYPE-031, both widths): neither is a rounding of the number written in any useful sense — an epsilon that is zero divides by zero, a limit that is infinite limits nothing — and infinity is spelled by computing it. A subnormal result is ordinary rounding and stays. **(b) Lift the 15-digit rule once DEF-203 lands**: it protects nothing then, and refusing `0.1234567890123456f32` while accepting its fifteen-digit prefix is a rule a reader cannot derive. The alternative for (b): keep it as a style limit (a `flt32` holds about seven digits; sixteen written ones claim a precision the type has not). |
| **S-127** — MAY AN ENUM'S PAYLOAD-LESS VARIANT BE A CONSTANT OUTSIDE THE EVALUATOR: a module binding, and a `comptime(…)` value (raised 2026-10-01 by `nitpick-compiler_28` with D-338's landing). The recap: D-165 (settled at 1.0.9) says a module-level binding's initialiser is a compile-time constant — "a literal, a sentinel, a struct or array of constants, another module binding, or `comptime(…)`" — decided by whether the compile-time evaluator folds it. Until D-338 the evaluator held no enum value, so `fixed Ordering:O = Ordering.Less;` was NITPICK-TYPE-035 and `comptime(Ordering.Less)` NITPICK-TYPE-004. D-338 (2026-09-30) gave the EVALUATOR the value — to compare, and to select on with `pick` — and said "nothing else is added", so landing 90 kept both refusals exactly as they were (the second with its own sentence): every other kind of value the evaluator holds can leave it, and this one cannot. | OPEN — the user's. Landing 90 forecloses neither answer: the value is in the evaluator (`CV_ENUM`), and the two refusals are one test each (`const_init_verdict`, `type_comptime`). | **Admit it, in both positions, as its own landing**: a module binding (and a struct's or an array's member) initialised with a payload-less variant, and `comptime(expr)` whose value is one. The constant is the tag the variant already has (`i32`, or the enum's aggregate with a zeroed payload) — the value `Enum.Variant` builds at run time. It is the same rule D-165 already states ("constant exactly when the folder folds it"), it removes the one kind the forcing form computes and refuses, and after the permanent freeze it could not be added. The alternative: leave it out for good — an enum constant is then written as a function or a number. |
| **S-128** — IS A `comptime func:` ALSO A RUN-TIME FUNCTION (raised 2026-10-01 by `nitpick-compiler_28`; DEF-214, found writing D-338's tests). The recap: `comptime` on a declaration "is the marker that says a function may run at compile time" (D-130, settled at 0.6.7), and MACRO_REFERENCE §10 has listed "what may a `comptime func:` call" as unsettled since then. What the compiler DOES: the checker types a `comptime func:` and a call of it like any other, and the emitter emits no `comptime func:` at all (since 1.0.9c) — so a call outside a constant context (`int32:v = raw dbl(x);` with `dbl` a `comptime func:`) compiles and is refused by `llc` as an undefined symbol, at both legs, on every compiler since (probes `t1`, `t2`). The program is refused either way; by the wrong tool, with no sentence. | OPEN — the user's. DEF-214 lands on the answer. | **No: a `comptime func:` exists at compile time only, and a call of one outside a constant context is refused by name** (the checker, at the call: inside `comptime(…)`, a constant site — an array size, a module initialiser — or another `comptime func:`'s body it is evaluated; anywhere else it is refused, and the sentence says to write `comptime(…)` or an ordinary function). One body then never has two executions that must agree, the evaluator's and the machine's — the class DEF-29, DEF-130 and DEF-196 came from — and the marker means one thing (the blueprint rule). The alternative, R2: emit it as an ordinary function as well, so the same declaration runs at both times; every `comptime func:` is then held to the run time's answer by tests forever, as D-338's twin tests hold two. |
| **S-129** — IS A `fixed` SLICE READ-ONLY THROUGH IT (raised 2026-10-07 by `nitpick-compiler_31`; DEF-230, the library listener's F-047). The recap: D-074 (settled at 0.7) retired the `binary` type because "immutability is a BINDING property in Nitpick rather than a type property, so an immutable byte view is `fixed uint8[]`" — TYPE_REFERENCE §22 teaches exactly that; D-287 (1.5.5) made a `fixed` binding addressless (TYPE-071) and DEF-106's TYPE-086 refuses a write into a PART of one, and both stop at a pointer, slice or handle base, "the storage there is not the binding's own". So today a callee declaring `fixed uint8[]:v` writes `v[0] = 9` and the caller's bytes change (measured at `93bcb66`, both legs), which is what D-287 says and the opposite of what D-074 promised. Measured: every `fixed uint8[]` in the tree (seven files) is the `Writer` trait's `write` parameter and none writes through it; no library uses a fixed slice view. | **SETTLED 2026-10-08 as D-348** (the user: "your recommendations are fine with me"): R1, both steps — (i) the write refusal, landing 98; (ii) `fixed T[]` as a type, planned after it. DEF-230 lands on (i). | **R1: make D-074's promise true, in two steps.** (i) A write through a `fixed` slice — an element, a range, `@`/`$$m` of its elements, a stateful operation on one — is refused (TYPE-086, `place_fixed` walking into a slice base when the slice binding is `fixed`); pointers and handles stay as D-287 has them (an address is not a view). This closes F-047's two programs and refuses nothing that exists. (ii) `fixed T[]` becomes a TYPE — a read-only view: `T[]` converts to it implicitly (fewer rights), never back, so a `fixed uint8[]` cannot be handed to a plain `uint8[]` parameter whose callee writes; the hole (i) leaves. A language change before the freeze, its own landing after the wrong answers. The alternatives: R2, (i) alone, the pass-through hole documented; R3, correct D-074's sentence instead — `fixed` fixes the binding, a view's bytes are the viewed storage's — and the reference stops promising an immutable byte view. Against the no-surprises rule a signature that says `fixed` and lets the callee write is the worse reading. |
| **S-130** — THE PERFORMANCE FINDINGS OF THE LIBRARY SEAT'S BENCHMARK ROUNDS (raised 2026-10-07 by `nitpick-compiler_31`; the listener's O-N36 Gemini Findings 01–03, O-N37 Finding 04, O-N38 items 2–3, O-N39; registered, not decided — every answer is right, and performance is subordinate to safety). The recap, as measured by the listener and its subagent: (1) THE FLOOR IS BUILT AT -O0 (`nitpick.toml`'s `llc-flags`), its `memset` a byte loop; a 10 MB zeroing loop's verified build runs 120,076,387 instructions against an optimized build's 1,326,407, and with the floor through `opt -O2` + `llc -O2` 1,589,667; `alloc_churn`'s allocator 1,245,083,684 → 300,030,724 (C 143,173,989); binary-trees at depth 16: 14,499,678,447 (4.36× gcc) → 6,207,367,212, the -O0 floor 57% of the total; 13_strings: the -O0 floor is 86% of the gap to C. Four floor symbols had to lose `internal` for `opt` to keep them (`npk_start`, `npk_start_main`, `npk_fs_stack_top`/`_limit`). (2) A CHECKED PATH BESIDE THE `{ T, i32 }` ENVELOPE DEFEATS INLINING: `update` (three checked `int64` adds returning a 48-byte struct) costs 240 against LLVM's -O2 threshold 225 and is not inlined — with `+%` it costs 15 and is; `parse_int`'s `decreases s.len - i` survives -O2 at 5 instructions per trip and raises its cost 105 → 305 (964,019,277 instructions against 204,019,261 with `unbounded`; the verified build's 4.7× is the elided check); Collatz's checked `3x+1` keeps the parity test a branch (9.8% mispredicted, 602 ms against clang's 197; `+%` makes it a `cmov`, 236 ms); the envelope repack after a recursive call blocks tail-call elimination (Ackermann(3,10): 44,698,325 calls against 22,345,074). (3) THE ALLOCATOR: a free costs 708.5 instructions on the shipped floor (the 0xAA fill 181, `npk_chtab_find`'s search 227.5, `npk_small_check` 87 plus 41 of guards, `npk_lg_find` 56, the mutex 19) against glibc's 105.1, an allocation 201.9 against 75.7; the heap counters run without `NPK_HEAP_STATS` (1.75%) and the mutex is taken in single-threaded programs (3.92%). (4) COMPILE SPEED at `93bcb66`: `src/npkc.npk` 18.10 s and 173,620 KB; nitpick-time's lib 0.33 s, 4.22 s with `--obligations` (12.8×, all CPU, no solver started). | OPEN — the user's; nothing is blocked. | **Decide (1) now, the rest in the examination phase.** (1) Build the floor through the pinned `opt -O2` + `llc -O2`: the floor's evidence is over its IR TEXT (the spec's rows, the models, the explorer's transformer all read `npkrt.ll`), so the object's optimisation moves no row — to be MEASURED by the landing: the 388 rows re-decided, D-303's sweep, every `// stress:` program, the four symbols' `internal` read before it is lifted; a D-204 pin change (the flag lists) announced in advance. (2) and (3) are language-shaped (the envelope, the overflow checks, the measure check, the poison fill and chunk checks are safety instruments by decision) and belong to the post-1.6 examination with the inline threshold measured as a pin candidate; the mutex in a single-threaded program and the counters are small (5.7% together) and stay until measured against the explorer. (4) `--obligations` 12.8× is the encoder's own cost and is read with E-4/E-6's residue work. |
| **S-131** — THE TOOLCHAIN PIN NO PACKAGE SOURCE SERVES (raised 2026-10-08 by `nitpick-compiler_31` from the library seat's VM test of the install README on Ubuntu 26.04). The recap: D-204 (1.4.5) pins the toolchain as an exact patch release — `20.1.2` — because a patch release can change instruction selection; `npkg build` refuses 20.1.8 against it ("Install the pinned version, or update the pin AND regenerate every expected hash"), and no apt source ships 20.1.2 any more (apt.llvm.org's noble suite and Ubuntu 26.04's archive both ship 20.1.8; apt.llvm.org serves no LLVM 20 for 26.04 at all), so a newcomer cannot satisfy the pin by any route the README can give; with only the pin edited `npkg build` passes in 60.7 s. | **SETTLED 2026-10-08 as D-349** (the user: "i'm fine with moving the pin. … we just can't forget the dependency chain beyond just the compiler itself"): the pin moves to the 20.1 release the distributions serve, as an exact release still, and NIKOS and Alive2 are rebuilt and re-measured against it in the same landing (Clam at LLVM 18 and z3 are unaffected); its own landing after 94…98, `nitpick-compiler_32`'s. | **Move the pin to 20.1.8 and regenerate the expected hashes**, as its own landing: a D-204 toolchain change with every digest re-recorded, the floor's rows and the explorer's schedule hashes re-measured (expected unmoved: both are over IR text or steps), the library side told in advance (its CI pins the same release), INSTALL.md then naming the distributions' `llvm-20` as the route. The alternative: keep 20.1.2 and have the README say §6 is for the project's own machines only — a pin nobody can install tests nothing on a second machine, which is what D-265 exists for. **SETTLED 2026-10-08 01:15 as D-349 (recorded by landing 96) and LANDED 2026-10-08 as D-349's landing (landing 99, `nitpick-compiler_32`)**: the pin is `20.1.8`; measured FIRST with apt.llvm.org's signed noble-20 packages extracted into a user prefix on landing 98's tree — every object and the emission byte-identical to the 20.1.2 ladder's, the fixpoint holding, one `.comment` byte per linked binary moved; NIKOS and Alive2 rebuilt against 20.1.8 and re-pinned (`pins.txt`), the gate's controls and Alive2's smoke re-run; the library seat told in advance (F41) and holding its local runners on a private 20.1.2 prefix until its own re-pin; INSTALL.md names the distributions' `llvm-20` as the route and apt.llvm.org's noble suite for Ubuntu 24.04, whose archive stops at 20.1.2. |

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

> **DEF-97 — FIXED at 1.6.0 step 3d (2026-09-25). A GENERIC INSTANCE USED ONLY INSIDE A GENERIC FUNCTION HAD ITS
> TYPE DEFINITION EMITTED AFTER ITS FIRST USE, AND `llc` REFUSED THE MODULE.** Found by the library listener
> (`nitpick-libs_s6`; nitpick-regex's fourth audit, its N-21) and reproduced here on `8a9eb35`: `struct:Pair<T>`
> with a `Pair<T>:p` local in `mk<T>`, called `raw mk::<int64>(4i64)` and named nowhere else, compiles to an
> `alloca %"npk.zzg1.Pair<int64>"` at line 324 and the `= type { i64, i64 }` line at 835 — `llc`: "Cannot allocate
> unsized type". The instance is interned while the generic body is being emitted, so `emit_late_headers` wrote
> its header after the bodies through `irw_raw`, which goes to the TAIL once any function has begun; a named
> type's definition must precede its instruction uses. 1.5.4c's finding (2) was the same symptom on another
> path (an inline module's struct had no header at all). Fixed: `irw_type_def` (ir_writer.npk) writes a type
> definition into the head while no function has begun and into the frame types' sink — the one spliced ahead
> of every function — once one has; both header writers of `emit_program.npk` use it. The compiler's own
> emission has no late header (213 type definitions, all before its first `define`), so no compiler byte moves;
> `tests/backend/programs/late_instance.npk` exits 0 where it was refused by `llc`.

> **DEF-98 — FIXED at 1.6.0 step 3e (2026-09-25). THE LEXER CLOSED A BLOCK STRING ON TWO QUOTES WHERE THE
> GRAMMAR CLOSES IT ON THREE.** Found by the library listener (`nitpick-libs_s6`; nitpick-regex's probe
> `probe15_block_string_close.npk`) and read here: LEXICAL_REFERENCE §6.3 says `BlockStringLiteral ::= '"""'
> (SourceCharacter - '"""')* '"""'`, so `""` inside the body is two body characters, while `lexer.npk`'s close
> tested `q == '"'` and the NEXT character only, then skipped three — `"""a""b"""` closed at `""b`, ate the
> `b` as the third quote, and the rest was NITPICK-PARSE-003. The ruling: the grammar stands (the reference
> is the language; the lexer deviated) — the close reads three quotes (`lexer_peek3`), and a body may hold
> any run of quotes shorter than three; `"""ab""""` is the body `ab`, the closer, and a stray `"`, exactly as
> the grammar reads it. `tests/frontend/lexer_strings.npk` gains two cases and
> `tests/backend/programs/block_string_quotes.npk` runs them (a wrong length is the exit). `src/` itself may
> not spell a `""` inside a block string until a snapshot carries the fix (D-205).

> **DEF-99 — FIXED at 1.6.0 step 3f (2026-09-25). A MOVE OUT OF `fixed` STORAGE HOLDING AN OWNING VALUE
> COMPILED, AND THE PROGRAM FAULTED.** Found by the library listener (`nitpick-libs_s6`; nitpick-time's 0.1.3
> planner, their O-N20) and read here: `fixed string[2]:NAMES` is emitted as an LLVM `constant` global (D-211),
> and `string:s = move(NAMES[1i64])` stores the vacant value into it — `MachineFault` at -O0, and at -O2 the
> store deleted as the UB it is and the moved string's drop stopping as `Unreachable`; `pass NAMES[i]` (the
> implicit move) and `move(V)` of a `fixed string` (a local: no vacancy written, two owners) reach the same
> end. Measured by the listener at every pin since `0dfddac`; a `.clone()` runs clean. The copy out of such
> storage was already TYPE-046, and D-287 had refused the ADDRESS of a `fixed` binding (TYPE-071) for exactly
> this reason — a write path no rule sees — while the move's own write went unasked. `refuse_move_out_of_fixed`
> (type_expr.npk, beside the limited and sealed twins) refuses at the `move` operator and at `pass` when the
> place is `fixed`-rooted (`place_fixed`) and the value OWNS: `NITPICK-TYPE-084`; a copyable value moved out
> is a plain copy and stays. `tests/types/rejection/fixed_move_out.npk` holds the three shapes and the clone
> control. A refusal ADDED, announced in advance (NOTICES F9, naming 58); the listener's exposure is none.

> **DEF-100 — FIXED at 1.6.0 step 3 (2026-09-25; a document). BUILD_REFERENCE DESCRIBED THREE `npkg` FEATURES
> AS WORKING THAT `npkg` DOES NOT HAVE.** The library listener's registry audit (`nitpick-libs_s6`, the shape of
> its old O-N12 on the subject of its O-N2/O-N5): §7's table said `npkg update` is "the only command that
> resolves versions" while `npkg/main.npk` refuses it by name; §1's example manifest carried `target =
> "library"` and §3 the dependency-root form `use "nfs/path.npk"`, neither with a status, while the `Manifest`
> stores no `target` and no dependency (`rootlist_add` has no caller outside a unit test; a `[dependencies]`
> entry with `use "dep/thing.npk"` is NITPICK-RESOLVE-005). The three passages carry dated status notes now —
> PLANNED, with what works stated beside it — as TYPE_REFERENCE and BUILTIN_REFERENCE do since O-N12. The
> features themselves stay the workbench's O-N2 and O-N5, open, with nothing waiting on them this cycle.

> **DEF-101 — FIXED at 1.6.0 step 3 (2026-09-25; a document). TYPE_REFERENCE §9.3's ENUM-CAST SENTENCE WAS
> STATED FOR EVERY SHAPE.** The library listener (nitpick-time's 0.1.3 worker, measured by `nitpick-libs_s6`
> at `c3bdae2`): "`intN => enum` is impossible in both spellings (D-140)" is true of a payload-carrying enum
> (both spellings TYPE-032) and false of a tag-only one, where `intN =>! enum` manufactures a tag — D-140's
> stated contract, the compiler's own `payload =>! TokenKind` sites among its users — and `intN => enum` is
> TYPE-009. The sentence sat under "Tagged enums with payloads" but said "at every shape"; it is scoped by
> shape now, with both codes named.

> **DEF-102 — FIXED at 1.6.0 step 3g (2026-09-25). AN ASSIGNMENT TO AN OWNING FIELD OF A LENT PARAMETER DROPPED
> THE CALLER'S VALUE; `@p` OF ONE LET A CALLEE FREE OR GROW THE CALLER'S STORAGE.** Found by the library listener
> (`nitpick-libs_s6`; nitpick-regex's 0.0.4d planner, their O-N21) and reproduced here at `6fb85d3`: `func:overwrite
> = NIL(Box:b) never fails { b.s = string_concat(…); }` on a plain `Box:b`, then the caller reading its string's
> first byte — 0xAA, the allocator's free poison (exit 70) — because D-186's overwrite drop ran in a callee that does
> not own the struct (D-004: a plain by-value parameter is LENT — a copy of the header, none of the ownership); a
> whole-binding assignment to a lent parameter was safe by the drop flag while a FIELD had none. The second face,
> the planner's: `@p` or `$$m p` of a lent container handed a callee a way to free or grow the caller's body
> through the copy (a double free at the caller's own drop, or a stale header). THE RULE: a place rooted at a lent
> parameter whose type owns admits no write path — no assignment to it or into it, no `@`, `$$i` or `$$m`, no
> pointer-receiver call, no stateful operation — `NITPICK-TYPE-085` from the one helper every write path asks
> (`refuse_write_path`), the view's rule (D-266, TYPE-066) applied to the loan; a copyable parameter is a copy and
> keeps every write, a `move T:p` owns and keeps them, and the callee that must change an owning value takes it as
> `move`. MEASURED: the compiler's own `src/` has no such site; `npkg` had four (`strset_has`, `floor_emit`,
> `floor_models_current`, `row_function_held` — each taking `@` of a lent set or list to READ), re-spelled as
> pointer parameters with their callers passing `@`; the tree sweep: thirteen sites in tests/, every one a method call through a lent `dyn` parameter, admitted by the exemption; zero sites after it, over 700 files of tests/, lib/ and tools/. Tests:
> `tests/types/rejection/loan_write.npk` (seven write paths, one code), `tests/backend/programs/loan_clone.npk`
> (the two spellings that remain, exit 0). A refusal ADDED, announced in advance (NOTICES F10, naming 59).

> **DEF-103 — FIXED at 1.6.0 step 3g (2026-09-25). A KEYWORD WAS ACCEPTED AS A DECLARED FUNCTION OR TYPE NAME.**
> The library listener's item: `func:release = int32(int32:x) …` declared, and every call to it was PARSE-002
> "expected an expression", since `release` is a keyword wherever it is used — a name that could be declared and
> never named, where a keyword-named BINDING was already refused at its declaration. `p_declared_name`
> (parse_decl.npk) reads the name at the function, trait-method and record sites and refuses a keyword with
> PARSE-001; the four keywords the lexer interns as a name after a `.` (`acquire`, `any`, `trit`, `nit`) stay legal
> method names (the prelude's `Mutex.acquire`). Fields, variants and parameters are unchanged (reached after a `.`
> or never by a bare name). `tests/frontend/parse_decls.npk` gains the pair. The listener's other item — whether
> `T[0]` is a supported type — is a statement, not a defect: TYPE_REFERENCE §9.2 says so now, with
> `zero_len_array.npk` and `zero_len_owning.npk` as its measurement.

> **[A dated note, 2026-09-26 (1.6.1 step 0c): the library listener CORRECTED the exposure figure it gave for this
> defect — "our exposure in src/ none" was wrong, because its sweep looked for lent bare-`T` PARAMETERS alone while
> the switched gates also reach a `T` PLACE read out of a lent or pointed-to container: at c970483 nitpick-regex's
> `vec_get` (`pass v.items[i]` on a lent `Vec<T>`) and `vec_pop` (through a pointer) are refused TYPE-047 and 78 of
> its 223 importers go red. The refusal is correct (the same shape the 3g sweep found in this tree's own getters),
> and their adoption re-spells both; nothing moves in the compiler.]**
>
> **DEF-104 — FIXED at 1.6.0 step 3g (2026-09-25, riding with DEF-102 by the listener's request). A LENT `T` IN A
> GENERIC BODY ESCAPED BOTH LOAN GATES.** Found by the library listener (nitpick-regex's fifth audit, N-25; their
> O-N22) while 3g was in its harness, and read against 3g's own source: `refuse_move_of_borrowed` (TYPE-047: a
> lent parameter is not passed or moved out) and 3g's new `place_lent_owning` (TYPE-085) both gated on
> `type_drops`, which is FALSE for an unsubstituted `T`, and a generic body is checked once — so
> `func:id<T> = T(T:x) never fails { pass x; }` passed the lent value out at `id::<string>(a)` (the caller freed
> it twice, exit 95; returned and read, the free poison) where `func:idstr = string(string:x) { pass x; }` was
> refused, and `@x` of a lent `T:x` was the loan's generic face. D-264 settled the predicate for TYPE-046 at
> 1.5.2f: `type_owns_for_move` treats a bare `T` and `Self` as owning. Both gates ask it now. Measured: the
> compiler's own `src/` and `npkg` build under it; `tests/types/rejection/lent_generic.npk` reports exactly
> {TYPE-047, TYPE-085} with a `move T` control carrying none. A THIRD gate of the same class, found by reading beside the two: `refuse_pass_of_pointee_owned` (a pointee's owning part passed out through a pointer, `pass self.v` on a `Box<T>->` receiver) asked `type_drops` too, and asks the predicate now. The switch found SEVEN sites in the tree's tests, every one a generic identity or a by-value-receiver getter returning a lent `T` (`nf_id<T>`, `identity<T>`, `scale<T>`, `Pair<U, T>.second`/`.tail`, `Cell<Pair<T, int32>>.inner`, `Box<T>.get`) — instantiated only at copyable types, where the copy was harmless and the generic body was the hole — re-spelled in D-264's forms: `move T:x` with `pass move(x)`, and a pointer receiver with `pass move(self.field)` (a move within the pointee, the vacant value left behind), and three more getters of the same shape (`list_at<T>`, `growable_get<T>`, `read_at<T>`) under the third gate — ten in all; every program answers its expected exit, `limit_ok.npk`'s rows are unchanged (vprog PASS), and the sweep after them reports only `lent_generic.npk`'s own two lines. The re-pin the listener holds after notices 59
> and 60 closes the generic face with the rest.

> **DEF-105 — FIXED at 1.6.0 step 3h (2026-09-25). AN IMPORTED `fixed` BINDING'S DECLARED TYPE RESOLVED IN THE
> IMPORTER'S SCOPE, NOT ITS HOME MODULE'S; THE SILENT FORM READ A TABLE WITH THE WRONG STRIDE, PAST ITS END.**
> Found by the library listener (`nitpick-libs_s6`; nitpick-time's 0.1.4 planner, their O-N23) and reproduced
> here: `pub fixed Row[2]:ROWS` imported alone (`use "./rows.npk".ROWS;`) was NITPICK-TYPE-001 "there is no type
> named `Row`" reported at the FIXTURE's line — the loud form — and beside an importer's own same-named `Row`
> with swapped fields it bound the IMPORTER's type (the wrong field read, exit 10), with a third field a 24-byte
> stride over 16-byte rows in the IR's `getelementptr`, row 1 partly past the table's 32 bytes; a scalar `fixed
> Row:ONE` the same; identical at every pin the listener keeps. D-137 says an annotation means what it means
> where it was written, and the symbol table has carried every declaration's HOME scope since 0.8.1
> (`symtab_home_scope`), for signatures and field types; two checker sites resolved a `DeclGlobalDecl`'s
> annotation in the USE's scope — the identifier typer's global arm (type_expr.npk) and the namespace-path
> member arm (type_members.npk). Both resolve in the home scope now; the emitter follows the checker's record
> (the definition itself was already emitted in its home scope, which is why stride and definition disagreed).
> Measured: the listener's case1, case2 and case3 exit 0 where they were TYPE-001, 10 and 10;
> `tests/backend/programs/import_scope_{lib,table,loud}.npk` are the fixture, the silent form read correctly
> beside an importer's wider same-named `Row`, and the loud form compiling. No refusal added.

> **DEF-106 — FIXED at 1.6.0 step 4b (2026-09-25). A WRITE INTO A PART OF A `fixed` BINDING COMPILED AND STORED
> INTO THE LLVM `constant` GLOBAL.** Found by the library listener's fuzzer (`nitpick-libs_s6`; nitpick-fuzz, grid
> cells c0039/c0040/c0185, their O-N24) at c3bdae2 and 6fb85d3 and reproduced here on a79d8df: `FA[i] =
> string_concat(…)` over `fixed string[2]:FA` (exit 95 on both legs: the overwrite drop freed a literal body no
> allocation made), `FB.s = …` over a `fixed Box:FB` (95/95), `FI[i] = 5i64` over `fixed int64[2]:FI` (107 at -O0,
> the SIGSEGV into read-only memory through D-307's net; exit 3 at -O2, the store deleted as the undefined
> behaviour it is), `FP.b = 5i64` (107 / 0) — the two legs disagreeing on what a program means. The whole
> binding's second assignment was ASSIGN-002 (the bindings analysis), its address TYPE-071 (D-287), a move out
> of it TYPE-084 (DEF-99, 3f); a write into a PART was never asked by anything. `NITPICK-TYPE-086` refuses it at
> the one helper every write form already asks (`refuse_write_path`, type_expr.npk, beside the loan and the view
> clauses; `refuse_fixed_write` beside `place_fixed` in type_stmt.npk): an element or a plain field of a `fixed`
> binding, a compound assignment, a stateful operation on a part, a `fixed` LOCAL's element, and anything under a
> `fixed` field; the two shapes the bindings analysis reports — the whole binding, and a `fixed` field's own
> assignment — keep their ASSIGN-002 (D-240), and the address forms and the pointer-receiver call report TYPE-071
> ahead of it. Measured: the listener's four shapes refuse with TYPE-086 and its clone control runs (exit 0);
> the compiler's own `src/`, `npkg`, `lib/`, `tools/` and every test naming `fixed` (127 files) report no site;
> the listener swept 231 files of its six repositories: 63 `fixed` bindings, 0 sub-place writes.
> `tests/types/rejection/fixed_write.npk` holds the seven shapes and the read/clone control. A REFUSAL ADDED,
> announced in advance (NOTICES F11).

> **DEF-107 — FIXED at 1.6.1 step 0 (2026-09-26; D-325, `NITPICK-BORROW-015`). Registered at 1.6.0 step 5b as a
> language question (S-106). A VIEW'S ROOT MAY BE WRITTEN WHILE THE VIEW IS LIVE, AND THE VIEW THEN READS FREED
> MEMORY — in safe code, on both legs.**
> Raised by the library listener (`nitpick-libs_s6`, nitpick-time's 0.1.4b planning) as a question, not a
> claim: their `bytes_take(Bytes->:b)` returned `string_from_bytes(b.body.ptr, b.len)` — a view of the sink,
> typed `string` — and the caller's `string:s = raw bytes_take(@d); drop bytes_clear(@d); drop
> bytes_extend_str(@d, …)` then read `s` rewritten (exit 13) or, past the capacity, freed (exit 12), at
> c3bdae2 on both legs; the copying take (`string_concat("", …)`) runs clean. Reproduced here in six lines on
> de7ba7a: `string:d = string_concat("hello", " world"); string:s = string_from_bytes(d.ptr, 5i64); d =
> string_concat(…);` — the reassignment drops the old body (D-186) — and `s` reads its 0xAA poison: exit 12 on
> the plain build and on the opt-O2 build. WHAT THE RULES SAY TODAY, read against the decisions: D-249 makes a
> view-maker's result a BORROW for the ESCAPE analysis (a view cannot leave its frame, be stored or laundered:
> BORROW-001/012) and nothing else; D-286 decides exclusivity among CLAIMS (`$$i`/`$$m`) and held `@`s with
> lexical lifetimes, `@` claiming nothing, NLL and two-phase borrows OUT — a view is not a claim and its
> root is not frozen; D-266 froze a LENDING `pick`'s selector while an arm's view is live (TYPE-067), for
> that construct alone. So a `string_from_bytes` view, a `string_bytes` view or a range view `l[lo...hi]`
> is a borrow that nothing protects from a write to its root in the same scope — a use-after-free
> reachable without `wild` or `=>!`, the class the language's floor excludes. Not a defect of the listener's
> library (its fix is right) and not of 1.6.0; owned by the user through S-106.
> **SETTLED 2026-09-26 as D-325: the freeze of a view's root for the view's lexical lifetime, measured on the tree
> first and landed as 1.6.1 step 0 with an advance notice naming its code.** THE FIX, AS LANDED (D-325's landing
> note has the design): the six lines were ONE of five reachable faces, each measured reading the free poison on
> c970483 — the direct view; a view of a POINTEE returned through `@d` (the listener's own case); a view of a
> by-value parameter's bytes returned up one frame (the compiler's lexer idiom); a view local moved through a
> pass-through call; a range view of a `List` then a push; and three more found probing — a field viewing another
> field of its own struct (in one frame and through a pointer parameter), a view stored through a pointer-receiver
> method, a view pushed into a `List`. A view is a PARTY of the aliasing walk (D-286's table, the shared row), held
> by its binding from the declaration to the block's end; what a binding views is the escape analysis's
> provenance through calls — per-function summaries grown to its fixpoint (view entries with field paths, a
> pass bit, a store matrix, a mutation summary) in place of D-004 rule A's shape, which refused 235 sites of the
> compiler's own source when read as a freeze. Tests `tests/analysis/rejection/view_freeze.npk` and
> `tests/backend/programs/view_freeze_ok.npk`. A REFUSAL ADDED, announced in advance (NOTICES F13).

> **DEF-108 — FIXED at 1.6.0 step 5c (2026-09-26). A FUNCTION THAT FALLS OFF ITS END COMPILED AND RETURNED A ZERO
> VALUE; A FALLIBLE ONE RETURNED A SILENT SUCCESS; `main` EXITED 0 WITHOUT `exit`.** Found by the library listener's
> cloud fuzzer (nitpick-fuzz; their O-N26), named a defect by the user ("that situation should not even compile as
> even NIL (our void) functions return NIL as the value"), measured by the listener at c3bdae2 and here on de7ba7a on
> both legs: an empty `int64` body reads 0 (`ret { i64, i32 } zeroinitializer`), a missing path returns 0, a `NIL`
> function without `pass NIL` runs, a fallible function with a missing path returns `is_error == false` with value
> 0, `main` without `exit` returns 0. The emitter's `fnem_close` wrote the fall-off return under the comment "subset-1
> sources always return explicitly", checked by nothing. D-323 is the rule; `NITPICK-FLOW-001` (`FLOW_FALLS_OFF`,
> analysis_codes.npk) refuses it from the bindings analysis's `stmt_completes`, the conservative twin of
> `stmt_exits` (bindings.npk). Measured: the compiler's own `src/npkc.npk`, `npkg/main.npk`, every `lib/` and `tools/` file and every test — 906 files under the built checker — hold NO function that falls off its end outside the new test file: the rule adds a check the tree already satisfies, at no cost to it (the listener's exposure at the pin is the check itself). Test: `tests/analysis/rejection/falls_off.npk` (an empty body, a
> missing path, a `NIL` function without `pass NIL`, a fallible function with a missing path — exactly {FLOW-001},
> four sites; the controls — both arms leaving, every pick arm leaving, an infinite loop with no `break`, a trap,
> `pass NIL` — carry no code). A REFUSAL ADDED, announced in advance (NOTICES F12).

> **DEF-109 — FIXED at 1.6.1 step 0 (2026-09-26). A BORROW OF A BY-VALUE PARAMETER'S STORAGE TRAVELLED UP ONE
> FRAME.** Found by D-325's first probe. D-004's exemption for a parameter-rooted borrow — "a parameter's target
> outlives the frame by construction" (0.8.1, `root_is_param`) — is a fact about POINTER parameters; a by-value
> parameter is the callee's copy in the callee's entry slot. Measured on c970483: `func:addr = int32->(int32:x)
> never fails { pass @x; }` compiled (the address of a dead frame slot, read back by luck), and `func:take2 =
> string(string:x) never fails { pass string_from_bytes(x.ptr, 5i64); }` handed the caller a view of its own `d`'s
> bytes that the caller — which passed no `@` for rule A to mark — then freed by reassigning `d`: exit 12, both
> legs. THE FIX: a by-value parameter's FRAME storage (its address, an inline array's range, a view of an inline
> part) cannot travel up (`BORROW-001`) and is this frame's for rule B; a view of the HEAP bytes it references
> (`string_from_bytes(x.ptr, n)`, `string_bytes(x)`, a range view of a `List`, a string or a slice) may, under the
> callee's summary VIEW entry, and the caller's freeze (D-325) then holds the argument -- the compiler's own
> `lexer_init(file, string:text)` returning a `Lexer` over `text`'s bytes is that shape and stays; a TEMPORARY
> handed to such a callee is a view of a temporary (`BORROW-012`). The tree held one site of the address shape
> (none) and the lexer idiom; `tests/analysis/rejection/view_freeze.npk` (11) and (12) pin it.

> **DEF-110 — FIXED at 1.6.1 step 0 (2026-09-26). THE CONSTANT FOLDER'S ENVIRONMENT HANDED OUT A VIEW OF A TEXT IT
> LATER OVERWROTE.** Found by D-325's analysis on `src/frontend/type_resolve.npk`: `foldenv_get` built a
> "deep-viewed" `ConstVal` whose `text` viewed the environment's own entry (D-183's comment: "the env owns the
> text; the caller's copy views"), and `foldenv_set` DROPS the entry it overwrites — so a value read out as a view
> and set back over the entry that owned its bytes (`s = raw id(move(s))` in a `comptime` body) would read freed
> memory at the next read. LATENT: no folded text is owned today (every string constant is a view of the interner,
> `string_concat` does not fold), so the drop frees nothing and the probe could not be written; the code's own
> claim was false and the analysis was right to refuse it. `foldenv_get` hands out an owned copy now (a copy per
> variable read; the folder is not hot).

> **DEF-112 — FIXED at 1.6.1 step 0 (2026-09-26). AN ADDRESS OR A VIEW PUSHED INTO A `List` ESCAPED THE FRAME
> WITH THE LIST.** Found by D-325's probes: `list_push(@ps, @x); pass ps;` with `int32:x` a local compiled on
> c970483 (the caller read 42 by luck of the stack), and a view pushed the same way (`string_from_bytes(d.ptr,
> n)`) escaped too. The `List`'s `items` is `wild`, and D-223 excluded wild slots from rule B's destinations
> because BORROW-011 refuses a borrow entering one -- true of a hand-written wild container, false of the prelude's
> push, which stores a `T` it cannot see as a borrow. THE FIX: `list_push`'s own store matrix (D-325's) names the
> value flowing into the list, so the list holds the borrow and cannot travel up (`BORROW-001`); and for the shape
> rule that remains for unknown callees, `can_connect` reads a `List<T>` as a destination for what a `T` slot holds.
> `tests/analysis/rejection/view_freeze.npk` (13) pins the address shape, its (9) and `p9b` the view shape.

> **DEF-111 — FIXED at 1.6.1 step 0 (2026-09-26). RULE B'S CONNECTION PREDICATE RE-RESOLVED STRUCT FIELD TYPES BY
> NAME ON EVERY QUERY.** Found by measuring the freeze's cost (the rule of 1.5.2d): the checker over
> `src/npkc.npk` went 27.3 s → 89.8 s with the view parties, and `callgrind` over a medium module put 88% of the
> instructions in `can_connect` → `struct_field` → `resolve_named` → `scope_lookup_local` — D-223's
> derivation-aware destination test walks a struct's fields and resolves each field's declared type through the
> scope chain, and answers a question of two TYPES that never changes; the views asked it at every
> pointer-receiver call and every `move(w)` argument, which multiplied a cost the escape pass had carried since
> D-223. Memoised per (kind, holder, src) in an open-addressing table on the analysis (an answer reached by
> exhausting the fuel is not kept): the checker over `src/npkc.npk` 27.3 s → 9.8 s — faster than before the
> freeze by 2.8×, the rule B cost having been the escape pass's hidden majority all along.

> **DEF-113 — FIXED at 1.6.1 step 0 (2026-09-26). A `dyn` HOLDER WAS NEVER A DESTINATION FOR A BORROW.** Found by
> the probes the D-223 counterweight prompted (below, DEF-114): rule B's `can_connect` fell to its final `false` for
> a `dyn` -- a cell holding a concrete value whose type is erased -- so `d.put(@k)` on a `dyn Sink:d` whose impl
> stores its argument, then `pass move(d)`, compiled on c970483, and the returned cell held the address of a dead
> frame's `k`: measured, a program reading through it after the frame died exited 3 (neither the local's 7 nor
> the next frame's 99). A `dyn` and a bare type parameter are read in the CLOSED direction now (D-223's own
> words for generics and unknowns): `can_connect` and `type_reachable_in` answer true for either, a `dyn` passed
> BY VALUE is a destination (its cell, the one caller-owned storage a loan may write, through its methods --
> TYPE-085's exemption), and `escape_connect` marks the binding under it. `tests/analysis/rejection/dyn_dest.npk`
> (1) pins it; `(4)` its lent-parameter face (DEF-115).

> **DEF-114 — FIXED at 1.6.1 step 0 (2026-09-26). A BY-VALUE PARAMETER COULD NOT HOLD A BORROW, SO ONE STORED
> INTO IT THROUGH A CALL TRAVELLED UP WITH IT.** The escape analysis's `holds` table is keyed by a local's
> declaring STATEMENT; a parameter is a declaration, and nothing could mark one. The direct store `b.p = @k` into
> a `move Stash:b` has been BORROW-002 since 0.5.1 (rule 3 sees the store), but the same through a
> pointer-receiver method -- `b.set(@k); pass move(b);` with `set` storing `v` into `self.p` -- reached the
> parameter through the call and marked nothing, in the shape rule's day (the receiver is no pointer argument,
> so `arg_can_hold_one` answered false) and in the matrix's (the destination's root was a parameter the marker
> skipped): compiled on c970483, the caller's copy pointing into the dead frame (measured: the slot still read 7
> by luck of the stack, as DEF-112's did 42). A by-value parameter -- a `move`, a copyable struct, a `move dyn` --
> is this frame's storage (DEF-109) and HOLDS from the call that stores into it (`pholds`, keyed by declaration;
> `escape_binding` and `ident_holds` read it, `escape_mark_place` and the matrix's marker write it), so rules 2
> and 3 guard it as they guard a local. `dyn_dest.npk` (5) and (6); a `move dyn` that holds and is DROPPED here
> is the control that carries no code.

> **DEF-115 — FIXED at 1.6.1 step 0 (2026-09-26). A LENT `dyn` PARAMETER'S CELL -- THE CALLER'S -- RECEIVED A
> BORROW OF THE CALLEE'S LOCAL.** `fill(dyn Sink:dp) { int32:k; dp.put(@k); }` compiled on c970483, and the
> caller's `dyn` then held a dead address (measured: reading through it after `fill` returned answered the next
> frame's bytes -- exit 2). A plain by-value parameter of an owning type is a LOAN (D-004, TYPE-085), and a `dyn`
> is the one owning-by-cell kind whose loan may be WRITTEN, through its methods (TYPE-085's exemption: the call
> passes the caller's object), so the cell is a destination that outlives the frame exactly as a pointer
> parameter's pointee is: the matrix's marker refuses the store (`BORROW-002`, naming the lent cell) where a
> `move dyn` parameter, this frame's own, holds (DEF-114). `dyn_dest.npk` (4).

> **DEF-116 — FIXED at 1.6.1 step 0c (2026-09-26). AN IMPL COULD DECLARE `move` ON A PARAMETER ITS TRAIT LENDS, OR
> LEND ONE ITS TRAIT CONSUMES, AND THE RESULT WAS A DOUBLE FREE OR A LEAK.** The library listener's O-N28, from
> nitpick-regex's adoption planning; reproduced on c970483: `trait:Dup = { func:dup = Self(Self:self) never
> fails; }` implemented as `string(move string:self) { pass self; }` compiled, and `twice<T: Dup>(T:x) { pass raw
> x.dup(); }` over a string handed the caller a second owner of one heap body (the second drop trapped through the
> trap route; a lent `self` written as declared is TYPE-047 at the `pass`, the control); the reverse — a trait's
> `move Self:self` implemented with a lent `string:self` — ran and never dropped what the caller had spent, a leak
> D-151 cannot see (managed storage). The mechanism was the listener's reading: `same_signature`
> (`type_trait.npk`) compares each parameter's TYPE through `same_after_self`, and a function type carries no
> `move`; TRAITS_REFERENCE §2 said an impl must have the trait's signature and the checker enforced the contract
> half (TYPE-041, `pure`, `async`) and never ownership. THE FIX: `check_signature` compares the two DECLARATIONS
> parameter by parameter — an impl's parameter is `move` exactly where the trait's is, in both directions,
> `NITPICK-TYPE-014` at the parameter naming the direction — before the shape comparison; a call through a bound
> or a `dyn` reads the trait's declaration and spends its argument where IT says `move`, which is the reason the
> rule has no exception. `tests/types/rejection/impl_move_sig.npk` pins both directions and the prelude's `Eq`;
> the tree and the listener's repositories hold no such impl (the sweep below).

> **DEF-117 — FIXED at 1.6.1 step 0c (2026-09-26). AT A GENERIC CALL WHOSE ARGUMENT WAS REFUSED, `T` WAS REPORTED
> AS UNINFERRABLE TOO.** The library listener's observation beside O-N28: `poke(@b)` with `poke<T>(T->:p)` and a
> lent owning `b` reports TYPE-085 at the `@` — and TYPE-022 ("`T` cannot be inferred") at the call, a second
> sentence about one mistake (D-240). A refused argument's type is 0, as a context-dependent argument's is, so the
> inference could not tell the two apart; it reads the diagnostics written while the arguments were typed now,
> and an unsolved parameter after a refused argument is reported by nothing (`all_solved`'s `report` flag).
> `tests/types/rejection/lent_generic_infer.npk`: exactly {TYPE-085}.

> **DEF-118 — FIXED at 1.6.1d step 1 (2026-09-26; registered the same day: nitpick-fuzz M9's F-003, the library
> listener's report). A CONSUMING `pick`'S BINDING ESCAPES THE MOVE RULES: a read after its move
> compiles and reads the 0xAA poison (npkc 0, exit 70 at -O0 and -O2), and a second move compiles and double-frees
> (exit 95, `Unreachable`); the local-variable twins are refused MOVE-001.** Reproduced by the listener at
> `c3bdae2`, `c970483` and `9f6f370` (landing 70's compiler, matched by digest); the evidence is
> `nitpick-libs/nitpick-fuzz/findings/F-003-consuming-pick-move-rules/` (README, minimised programs, controls,
> VERDICTS.txt; nitpick-fuzz main `88e6355`). A memory fault in safe code: the move analysis (D-208, 1.4.3;
> D-216's consuming `pick (move(v))`) tracks locals and parameters and not the bindings a consuming arm introduces. **The fix (1.6.1d step 1):** the resolver numbers a `SYM_PAT` symbol as a local, `symbol_slot` answers it, `assign_pick` marks an arm's bindings assigned; `tests/analysis/rejection/pick_binding_moves.npk` (three MOVE-001 sites, the struct destructure among them), `tests/backend/programs/pick_binding_moves_ok.npk`.

> **DEF-119 — FIXED at 1.6.1d step 1 (2026-09-26; registered the same day: F-004). A GAP IN DEF-107's FIX:
> while a view of `x` is live (`uint8[]:v = string_bytes(x);`), `@x` or `$$i x` handed to a callee that MOVES the
> value out through its pointer (`string:t = move(<-p);`) compiles, and the view reads freed bytes (0/70/70).**
> At `9f6f370` the neighbours are refused BORROW-015 — `$$m x` to the same callee, `@x` to an overwriting callee,
> a direct `move(x)` — so the freeze's mutation summary records a WRITE through the pointee and not a MOVE OUT of it
> (`record_mut_of_call`/the mutation paths of D-325: a `move(<-p)` empties the pointee as an assignment would).
> Evidence: `findings/F-004-view-root-freed-through-callee/`. A memory fault in safe code. **The fix (1.6.1d step 1):** `record_mut_of_place` peels one leading dereference; `tests/analysis/rejection/view_root_moved_through_callee.npk`.

> **DEF-120 — FIXED at 1.6.1d step 3 (2026-09-26; registered the same day; F-005). `(<-p) = v` NEVER
> DROPS THE OLD VALUE: an assignment through a pointer dereference to an owning pointee leaks it — 121 grid cells,
> every owning type; 1,000 stores keep 46,043 B live at exit under NPK_HEAP_STATS, and a descriptor stored over is
> left open (exit 26).** D-183 and D-186's overwrite drop covers a binding, an owning FIELD and a managed-array
> ELEMENT (`overwrite_owned.npk`); the dereference place is the fourth store shape and has no drop. Evidence:
> `findings/F-005-store-through-pointer-leak/`. Invisible to D-151 (managed storage), visible to the heap stats.
> **The fix (1.6.1d step 3):** `emit_assign`'s dereference target drops the old pointee before the store when the
> pointee's type owns AND the pointer names MANAGED storage -- the escape analysis's own wild-provenance reading
> (D-223), factored into `src/frontend/analysis/wild_places.npk` so both readers ask one predicate: a `wild`
> binding, parameter or field, an `=>! wild` cast, `#ptr_add`, an allocator's or a wild-returning function's result
> stays drop-free (manual storage holds no value until the author writes one; the closed direction for a wrong free
> is no free), and a plain `T->` -- a live managed value by the language's contract, a `wild` block laundered by
> `=>! T->` included -- drops. With it a moved-out WHOLE binding keeps the type's vacant value (D-254's rule beside
> the field's and the element's), because `p = @x; t = move(x); (<-p) = v;` is legal and the stale header would
> have freed `t`'s body at the store. Measured: 46,043 -> 89 bytes live after a thousand stores, the stored-over
> descriptor closed (exit 26 -> 0), the generic face (`(<-p) = move(v)` at `T = string`) drops too, and three
> `wild` shapes with a bogus header planted under the store run untouched. `tests/backend/programs/deref_store_drop.npk`,
> `deref_store_churn.npk`/`deref_store_once.npk` under `tests/cost/deref_store.toml`.

> **DEF-121 — FIXED at 1.6.1d step 3 (2026-09-26; registered the same day; F-006). A CONSUMING `pick`'S
> BINDING IS NEVER DROPPED AT THE ARM'S END** (a leak; the binding owns what the arm moved into it and nothing runs
> its drop when the arm ends). Evidence: `findings/F-006-consuming-pick-binding-not-dropped/`. DEF-118's sibling:
> the consuming arm's bindings are outside both the move analysis and the drop schedule.
> **The fix (1.6.1d step 3):** THE ARM IS A SCOPE in the emitter, as it always was in the resolver -- `emit_pick` and
> `emit_pick_chain` push a defer frame before `bind_payload`, so a consuming binding falls inside the frame's locals
> window, and run the frame's exit (the flag-tested drops) when the body did not leave by an exit of its own (a `pass`,
> `break`, `give` or `fall` inside the arm walked the frames and dropped it there -- the read-only arm, completing
> normally, was the leak). In a coroutine the binding's flag is a FRAME BYTE at role `50 + ordinal` on the arm's
> statement, reserved by `scan_pick_binds` beside the slot at `7 + ordinal` (an alloca dies at a suspension inside the
> arm). A guard that fails branches on without the frame's exit: the payload is still the selector's, owned once by
> the arm that finally binds it or the wildcard's whole-value drop (DEF-88). Measured: eleven arm shapes two thousand
> rounds at the once twin's peak (884 bytes), the coroutine's arm read after a suspension among them.
> `tests/backend/programs/pick_binding_churn.npk`/`pick_binding_once.npk` under `tests/cost/pick_binding.toml`. The
> arm scope's probes found DEF-138 … DEF-141 below, fixed with it.

> **DEF-122 — FIXED at 1.6.1d step 3b (2026-09-26; registered the same day; F-007; under D-328, the floor's `cstring` layout moved with a two-floor snapshot refresh). `to_cstring`'S BUFFER
> IS NEVER FREED: every call leaks `len + 1` bytes (1,000 calls hold 10,000 B live at exit).** The `cstring`
> result is a view-shaped value whose backing block nothing owns; the fix is the builtin's row (its `Views`/drop
> column) and the drop of what it allocates, or an owning result. Evidence: `findings/F-007-to-cstring-leak/`.
> **The fix (1.6.1d step 3b, D-328):** a `cstring` is string-shaped — `{ ptr, len, cap }`, `cap == 0` a borrowed
> body (a literal, an `argv`/`environ()` element), `cap > 0` the owned copy `to_cstring` makes — with the string's
> drop body, move-only, `.clone()` for a copy; the floor's six symbols take and build the trio; the literal in
> `cstring` position lands with it (TYPE-092 for an interior NUL); `tests/cost/to_cstring.toml` holds the churn's
> peak to the once's (40,000 bytes live after 4,000 conversions before). No call site of `to_cstring` changed
> its spelling; the four binding-to-binding copies of an `argv` element in the tree read the element in place.

> **DEF-123 — FIXED at 1.6.1d step 1 (2026-09-26; registered the same day: F-008). A WRITE THROUGH A
> `$$i` CLAIM'S HOLDER, OR THROUGH AN ARGUMENT HOLDING IT, COMPILES AND RUNS (0/22/22); the reference names it
> BORROW-013, and the write to the ROOT itself is refused BORROW-013.** D-286's conflict table reaches the root's
> accesses; a write THROUGH the shared holder is the case its "a write through a shared holder" sentence names and
> the walk does not check. Evidence: `findings/F-008-shared-claim-holder-write/`. **The fix (1.6.1d step 1):** `refuse_shared_holder_write` (a rootless place bottoming in `<-p`, and `acc_expr`'s dereference case) and `refuse_shared_to_writer` (the callee's mutation summary at the claim's position; an unseen callee refuses, a builtin admits) in alias.npk; `tests/analysis/rejection/shared_holder_write.npk` (seven sites), `tests/accept/shared_holder_reads.npk`.

> **DEF-124 — FIXED at 1.6.1d step 4 (2026-09-26; registered the same day; F-009; the bindings analysis marks a parameter's assignment as a local's, `assign_assign`'s `SYM_DECL` branch; the listener's program exits 22 at both legs with its local twin's heap numbers; `tests/backend/programs/move_param_reinit.npk`, `tests/analysis/rejection/move_param_twice.npk` keeps 1.4.3's rule). A `move` PARAMETER
> RE-INITIALISED AFTER A MOVE CANNOT BE READ (MOVE-001); its local twin compiles.** D-208's loop-carried states
> re-initialise a local; a parameter's re-initialisation is not read as one. An over-restriction. Evidence:
> `findings/F-009-move-param-reinit-refused/`.

> **DEF-125 — FIXED at 1.6.1d step 4 (2026-09-26; registered the same day; F-010; a `move` of an owning place whose type holds no foreign pointer carries the refs recorded AT that place -- `collect_moved_place_refs`, `type_holds_foreign_pointer` in escape.npk -- and the return seam's implicit move reads the same; the listener's swap runs 22 at both legs, `tests/backend/programs/lent_dyn_swap.npk`, `tests/accept/lent_dyn_swap.npk`; the refusals that stay are pinned in `tests/analysis/rejection/moved_field_carries_view.npk`). NEW WITH 1.6.1 STEP 0:
> a swap through a lent `dyn`'s method (`move(self.v)` into a local, then two owning strings moved whole, no view
> anywhere) is refused BORROW-002 at `9f6f370`, citing D-004 rule 3 and D-325 (DEF-115's rule); it compiles at
> `c3bdae2` and `c970483` and runs 22, correctly.** DEF-115's store-into-a-lent-`dyn`-cell rule reads a MOVED whole
> value as a borrow stored; an owned value moved into the cell is the cell's, not a view. An over-restriction.
> Evidence: `findings/F-010-lent-dyn-swap-refused/`.

> **DEF-126 — FIXED at 1.6.1d step 4 (2026-09-26; registered the same day; the listener's O-N29; `type_display_qualified` names a struct or an enum by the module node that holds its declaration -- an inline module by its name, a file by its header -- whenever the two plain texts of a mismatch are equal: "expected `mismatch_same_name.Row`, found `rows_same_name.Row`", measured; `tests/types/rejection/mismatch_same_name.npk`). A TYPE-007 MESSAGE NAMES TWO TYPES BY ONE WORD: an importer's own `struct:Row` beside an imported
> `rows.Row`, with `Row:r = ROWS[1i64];`, is refused "expected `Row`, found `Row`" (the refusal is right; the
> control renamed to `Cell` reads "expected `Cell`, found `Row`").** Qualify a type's name by its module whenever
> the two names in one message are equal. Found by M8: 12 grid cells gained TYPE-007 at 1.6.0 step 3h (DEF-105's
> fix). Reproduced by the listener at `c970483`.

> **DEF-127 — FIXED at 1.6.1d step 1 (2026-09-26; registered the same day: nitpick-fuzz M10's F-011, the library
> listener's report). A `for` BINDING OUTLIVES ITS LOOP IN THE EMITTER: after `for (int64:i in …)` a
> later use of an OUTER local named `i` reads the LOOP's slot (3, not 100); with an outer `int32[4]:v` and a loop
> `int32:v`, `v[3]` after the loop indexes the loop's 4-byte slot as the array and reads 12 bytes past it (-O0 and
> -O2 disagree: 11 / 10); with an outer `string:i`, npkc exits 0 and `llc` refuses the module.** The checker ends
> the binding's scope at the loop (`ctl_no_outer` is RESOLVE-002); the emitter's `for_bind_slot` binds the name
> through `fnem_local`/`fnem_bind` with no mark and no release around the loop, in all three arms and the
> coroutine path. A memory fault in safe code, present at `c3bdae2`, `9126350` and `9f6f370`. Evidence:
> `findings/F-011-for-binding-outlives-loop/` (nitpick-fuzz main `626c22d`). **The fix (1.6.1d step 1):** `emit_for` is a mark/release wrapper over its arms, sync and coroutine alike; `tests/backend/programs/for_binding_scope.npk` (every shape at both legs, a nested shadowing loop and the coroutine twin included).

> **DEF-128 — FIXED at 1.6.1d step 2 (2026-09-26, under D-329; registered the same day: F-012). A `for`
> OVER A RANGE RUNS ZERO TIMES AT ITS TYPE'S EDGES: a signed inclusive range ending at the type's maximum
> (`int8` `0..127`, `125..127`; `int32`/`int64` `max-2..max`) and an unsigned range crossing the sign bit
> (`uint8` `100..200`, `100...200`; `uint32` across 2^31) each run zero times, silently.** Two causes: the range
> literal builds the value HALF-OPEN by a plain `add …, 1` (D-145), which wraps at the maximum so `lo..MAX` has no
> representation at its width; and the head compares `icmp slt` whatever the element's signedness. The fix needs
> the range value to keep its spelling (S-110, the user's; recommended `{ T, T, bool }`). A silent wrong answer
> in the everyday byte loop. Evidence: `findings/F-012-for-range-head-signed-and-wrapping/`. **The fix (1.6.1d step 2):** the value is `{ lo, hi, inclusive }` (D-329) and the `for` reads the flag, compares by the element's signedness, and tests `last` before it adds; `tests/backend/programs/for_range_edges.npk` (every shape the listener measured, the byte loop `0u8..255u8`, the empty shapes, a bound range value of each spelling, and a coroutine twin).

> **DEF-129 — FIXED at 1.6.1d step 2 (2026-09-26; registered the same day: F-013). `loop` AND `till`
> WIDEN AN UNSIGNED BOUND BY ITS SIGN: `loop(100u8, 200u8, 1u8)` counts DOWN from 100 to −55, 156 times;
> `till(200u8, 1u8)` 56 times down to −55; a `uint32` loop across 2^31 traps `IntOverflow`.** `loop_i64` widens
> every operand with `sext`, and the direction and the head compare signed. D-192's class (the blind `zext` of
> 1.4.2), on the counted loop's side. Evidence: `findings/F-013-counted-loop-unsigned-bound-sign-extended/`. **The fix (1.6.1d step 2):** `loop_i64` widens by the operand's own signedness (`zext` for an unsigned one); a `uint64` bound past 2^63 -- the one unsigned value the `int64` counter cannot hold -- traps `IntOverflow` at the head through a guard with its `overflow` row at the operand (D-210: a value that does not fit), the reach analysis arming the identity; `tests/backend/programs/counted_unsigned.npk` (every unsigned shape, the counter's values checked), `counted_u64_high.npk` (the trap), `tests/analysis/rejection/reach_counted_u64.npk`, `tests/verify/counted_bounds.npk` (the rows).

> **DEF-130 — FIXED at 1.6.1d step 2 (2026-09-26; registered the same day: F-014). `till(-3, 1)` RUNS
> THREE TIMES COUNTING DOWN where CONTROL_REFERENCE §2.4 and D-022's table say zero (`till` ascends from 0;
> `limit <= 0` is zero iterations).** `emit_counted` infers the direction from the bounds for `till` as for
> `loop`; the compile-time evaluator's `fold_counted` does the same (DEF-29's fix kept the inference), so both
> agree on the wrong answer. Evidence: `findings/F-014-till-negative-limit-counts-down/`. **The fix (1.6.1d step 2):** `till`'s direction is ascending by construction in `emit_counted` (`asc` the constant 1) and in the evaluator's `fold_counted` (`asc = true` for `till`), so a limit at or below zero is zero iterations in both; `tests/backend/programs/till_negative.npk` holds each shape and its `comptime` twin equal.

> **DEF-135 — FIXED at 1.6.1d step 2 (2026-09-26; found by the step's own probe of the counted loop's operand
> kinds, not by a test of the thing that broke). A TWISTED BOUND HOLDING ERR RAN THE LOOP ZERO TIMES IN SILENCE
> where CONTROL_REFERENCE §2.4's table and D-008 §5 say "traps to `failsafe`": the carrier's MIN sign-extended into
> the counter and the head compared it like any number (`loop(z, err, step)` with `tbb32:err = ERR` exited 33
> where 110 was written; measured on `0fe5217`).** The fix: the operand's ERR is a guard at the head -- `TbbErr`,
> with its `err-exit` row at the operand (the goal "not ERR"), elided where the manifest discharges it;
> `tests/backend/programs/counted_tbb_err.npk` (exit 110), `tests/verify/counted_bounds.npk`.

> **DEF-136 — FIXED at 1.6.1d step 2 (2026-09-26; found by the same probe). A 128-BIT BOUND COMPILED TO `sext i128
> to i64`, WHICH IS NO INSTRUCTION, AND `llc` REFUSED THE MODULE (`loop(0u128, 10u128, 1u128)`: npkc 0, `llc!`);
> a float, a bool or a char as a bound was typed and never refused.** `check_counted` types each operand and
> asked nothing of it (D-022's head shape and the literal step were its whole rule). The fix: a bound is an integer
> that fits the `int64` counter -- `int8`..`int64`, `uint8`..`uint64` (DEF-129's trap past 2^63), or a `tbb` up to
> 64 bits (DEF-135's trap at ERR) -- and anything else is `NITPICK-TYPE-068` at the operand;
> `tests/types/rejection/counted_wide.npk` (seven sites).

> **DEF-131 — FIXED at 1.6.1d step 4 (2026-09-26; registered the same day; F-015; under S-111 as D-330: the emitter composes its own `<` and `>` over operands evaluated once, a twisted pair's ERR guard once, the folder folds a constant pair, the encoder reads `(ite (< a b) -1 (ite (> a b) 1 0))`; floats and a frac refused `NITPICK-TYPE-088`; the listener's three programs exit 0 at both legs; `tests/backend/programs/spaceship.npk`, `spaceship_tfp_err.npk`, `tests/types/rejection/spaceship_float.npk`, `tests/verify/spaceship.npk`). `<=>`
> IS REFUSED BY THE EMITTER (`NITPICK-EMIT-002`) IN EVERY FORM, two `int32` literals included; the checker types
> it `int32` over any ordered pair and no arm lowers it.** Lowered at step 4 for every ordered kind but the
> floats, which are refused by the checker (no total order; S-111). Evidence:
> `findings/F-015-spaceship-not-lowered/`.

> **DEF-132 — FIXED at 1.6.1d step 4 (2026-09-26; registered the same day; F-016; the chain reads each bound through the constant folder, `pat_bound_value`; the listener's three programs exit 4 at both legs; `tests/backend/programs/pick_neg_range.npk`). A `pick` RANGE
> PATTERN WITH A NEGATIVE BOUND (`(-5i32..-2i32)`, `(-5i32..2i32)`, `(-5i32...-2i32)`) IS `NITPICK-EMIT-002`
> where the value pattern `(-3i32)` lowers since DEF-35: the chain reads a bound's payload as a literal and
> answers `iv_broken` for the negated form.** Evidence: `findings/F-016-negative-range-pattern-not-lowered/`.

> **DEF-133 — FIXED at 1.6.1d step 4 (2026-09-26; registered the same day; F-017; d1-d6 the six sentences corrected in TYPE_REFERENCE §3.2 and §28, §4, CONTROL_REFERENCE §2.3; d7 under S-112 as D-331: a certain constant division is refused wherever the folder decides it, `check_const_division`, `tests/types/rejection/const_div_zero.npk` -- the listener's `d7_local_constant_div_zero` refused TYPE-004 as its `fixed` control always was). SEVEN REFERENCE
> SENTENCES THE COMPILER CONTRADICTS, the compiler right or safe in each: TYPE_REFERENCE §3.2's `s.length` and
> `s[0]` (d1, d2: the language's are `s.len` and `string_bytes(s)[i]`), §28's `!=` row (`fcmp one` for the
> emitted `une`, d3), §28's ternary row (`select` for the branches the emitter writes, d4), §4's D-037 wrapping
> sentence for the wide integers (they trap, D-210, d5), CONTROL_REFERENCE §2.3 naming `TYPE-033` for the
> binding-type rule that reports `TYPE-007` (d6), and OP_REFERENCE §1.1 promising a compile-time refusal of a
> constant division by zero or `MIN / −1` that the compiler makes only where the folder is asked (d7 — S-112:
> recommended, D-310's reach extended to `/` and `%`).** Evidence:
> `findings/F-017-reference-sentences-the-compiler-contradicts/`.

> **DEF-134 — FIXED at 1.6.1d step 1 (2026-09-26; found and fixed the same day by the step's own accept file
> `tests/accept/shared_holder_reads.npk`, not by a test of the thing that broke). A BY-VALUE RECEIVER METHOD CALLED ON A POINTER RECEIVES THE POINTER'S BITS: `q.peek()` with `Box->:q`
> and `peek = int64(Box:self)` is admitted by the checker as the one-level auto-dereference `.` promises (D-098;
> the receiver it fits is the pointee's type), and the emitter passed the POINTER where the callee expects the
> struct by value, so `self.n` read the pointer's low word -- 3 at -O0, garbage at -O2 (measured on `0902e48`).**
> The emitter's rule "the receiver's own type decides the form: a pointer is passed, anything else by value"
> (1.0.4) had the auto-ref dual for a `Self->` method on a value (1.0.9b) and no load for a `Self` method on a
> pointer. A silent wrong answer, and a read of a register as a struct, in safe code -- through `@x`, a `$$i`
> holder, a pointer parameter, a UFCS free function and a generic instance alike. **The fix (1.6.1d step 1):** the load in `emit_method_call`'s value branch (a pointer receiver and a parameter that is neither a pointer nor a `dyn`); `tests/backend/programs/byval_recv_ptr.npk`. D-098's dated note.

> **DEF-137 — FIXED at 1.6.1d step 2 (2026-09-26; found by landing 72's RED harness, read before anything
> moved). AN EXPLORED PROGRAM'S SCHEDULE DEPENDED ON THE PID'S DIGIT COUNT: five programs spell a per-process
> temp path with the pid (DEF-91's rule, 1.5.8b step 7), so the path's length -- and with it an allocation's
> size class, and with that the allocator's steps the schedule hash records -- followed the pid; when the
> machine's pid counter wrapped from seven digits to fewer between two replays of one seed inside `npkg`'s
> `parity` run, `trap_stops_runner` gave two schedule hashes and the replay belt (X-7) called the explored
> build nondeterministic.** Reproduced by hand on 72's explored binary: seed 1 replays to one hash under a
> six-digit pid twice and to another under a one-digit pid (a pid namespace), the harness's second hash being
> the post-wrap one; 72's change (the checker's `holds` marking) touches nothing of it. The fix: `lib/nsys.npk`'s
> `sys_pid_tag()`, the pid as eight zero-padded decimal digits (a pid is at most `pid_max`, 4,194,304), and the
> five programs (`trap_stops_runner`, `streams_file`, `text_roundtrip`, `dyn_stream`, `fs_basic`) spell their
> paths with it -- one length whatever the pid. A per-process name in any explored program is spelled with it
> from here on.

> **DEF-138 — FIXED at 1.6.1d step 3 (2026-09-26; found by the step's probe of the arm scope DEF-121 gives a
> consuming `pick`). A `move` OF AN ARM'S OWN BINDING INSIDE ITS `where` GUARD DOUBLE-FREED WHEN THE GUARD FAILED.**
> The bindings a guard reads are copied out of the selector before the arm is chosen; `(Som(x)) where (raw
> eat(move(x)))` spent the payload in `eat`, the guard failed, the next arm `(Som(y))` bound the same payload from
> the selector, and its drop freed the body a second time -- exit 95 through the heap's integrity trap, in safe
> code, on every compiler before this step (the second free was an exit-path drop, which the arm had always run).
> **The fix:** `NITPICK-TYPE-089` at the `move` -- a `move` whose root is a pattern binding of the pick whose guard
> it stands in (a `SYM_PAT` symbol linked to that selector, D-266), through a field or a value-`pick` inside the
> guard alike (`check_arm_control`, type_stmt.npk); a move of anything else in a guard is a conditional move the
> move analysis already tracks. `tests/types/rejection/pick_guard_move.npk` (three sites, two controls).

> **DEF-139 — FIXED at 1.6.1d step 3 (2026-09-26; found by the same probes). A `fall` INTO AN ARM THAT BINDS A
> NAME READ A PAYLOAD THE TARGET'S PATTERN NEVER MATCHED.** `fall label;` enters the labelled arm's body without
> matching its pattern, and the target extracts its bindings from the selector at the body's entry: a `Non` fell
> into `two: (Som(x))` and `x` read the empty payload area as a string (exit 23; a variant with a differently laid
> payload would have read a wild header, and with DEF-121's drop freed it). **The fix:** `NITPICK-TYPE-090` at the
> `fall` when its target arm binds a name (`arm_binds_name`), the walk descending blocks, `if`, loops and `defer`
> and stopping at a nested `pick` (whose `fall`s are its own); the falling arm's own bindings are dropped at the
> `fall` as at every other exit from the arm. `tests/types/rejection/pick_fall_target.npk`.

> **DEF-140 — FIXED at 1.6.1d step 3 (2026-09-26; found by the same probes). A `fall` TO A LABEL NO ARM CARRIES
> WAS NITPICK-EMIT-002** -- the emitter resolved the label against the innermost pick's frame (`emit_fall`) and
> reported a miss as "a defect in the compiler rather than in this program"; the checker never read the label. The
> class of DEF-131, DEF-132 and the listener's DEF-142. **The fix:** the identifier's own `NITPICK-RESOLVE-002` at
> the `fall`, naming the label and the shape (`label: (pattern) { … }`), from the same walk as DEF-139's.
> `tests/types/rejection/pick_fall_target.npk`.

> **DEF-141 — FIXED at 1.6.1d step 3 (2026-09-26; found when the arm scope's churn twin trapped on its first
> round). A CONSUMING `pick` EXPRESSION LEFT ITS SELECTOR ON THE STATEMENT'S TEMPORARIES: THE STATEMENT'S END
> DROPPED THE WHOLE ENUM AFTER AN ARM HAD TAKEN ITS PAYLOAD.** The statement form takes the consuming selector off
> the temporaries (D-216, D-246: the arms own the payloads they bind); the expression form (`ir_expr.npk`'s
> `ExprPickExpr`) computed `pcons` and never did, so `string:r = pick (move(e)) { (Som(x)) { give move(x); }, … };`
> handed the caller a body the statement then freed -- exit 95 on every compiler before this step (measured on step
> 2's), and an arm that read `x` and gave something else freed it twice the moment DEF-121 dropped the binding.
> **The fix:** `temp_take` of the selector in the expression form when it consumes, the statement form's line.
> `tests/backend/programs/pick_value_consume.npk` (three shapes, 500 rounds, exit 0 at both legs).

> **DEF-142 — FIXED at 1.6.1d step 4 (2026-09-26; registered the same day; the library listener's O-N32; `check_enum_arms` in `type_pick_rules`, both spellings: an arm naming a variant the enum lacks, or another type's name before the dot, is `NITPICK-RESOLVE-002` at the pattern naming the enum, the missing name and the variants it has; `tests/types/rejection/pick_missing_variant.npk`). A `pick` ARM NAMING A VARIANT ITS ENUM LACKS IS ADMITTED BY THE
> FRONTEND AND REFUSED BY THE EMITTER AS NITPICK-EMIT-002** (`enum:K = { A; B; };` with a `(K.C)` arm: npkc exits
> 1 at the `pick`, writes no IR, and calls it a compiler defect; without the arm the program runs 0 at both legs;
> without `(K.B)` as well it is PICK-001 "does not cover B", as it should). Reproduced by the listener at
> `c970483`, `c3bdae2` and `9f6f370`. The class of DEF-131, DEF-132 and DEF-140. **Requested and planned:** a
> resolution error at the arm naming the enum and the missing variant.

> **DEF-143 — FIXED at 1.6.1d step 4 (2026-09-26; registered the same day; the library listener's O-N32; a module's name is read by `p_declared_name` -- `mod:error;` is `NITPICK-PARSE-001` at the keyword, DEF-103's rule, and the loader adds no second sentence; the case is `tests/frontend/module_graph.npk`'s last -- written to a pid-tagged temporary file and loaded through the real loader, because a file whose refusal is a parse error cannot sit in the tree, where every file must pass the real parser (D-085; the step's first form was a rejection file, and its harness refused it there)). NITPICK-RESOLVE-012's MESSAGE FOR `mod:error;` DROPS THE
> KEYWORD:** it reads "declares `mod:;` first" -- the offending word, a keyword, renders as nothing. A message
> defect: the text names the token by its interned spelling, and a keyword has none.

> **THE LIBRARY LISTENER'S M11 FINDINGS (nitpick-fuzz F-018 … F-028, main `3d7d924`; reported 2026-09-26 by
> `nitpick-libs_s7`, reproduced by them at `c3bdae2`, `9f6f370`/`1b4f0c6` and their pin `c970483` — all live on
> main). M11 checks the REFERENCES against the compiler: 3,704 of 10,433 reference lines covered, 1,488 claims,
> 1,262 tested, 144 disagreeing. Registered here as DEF-144 … DEF-154, owner the compiler seat, for a subcycle
> **1.6.1e** to be PLANNED execution-grade after 1.6.1d step 3 lands; the user's standing rule orders its steps
> (the silent wrong answers and the memory fault first — DEF-144 … DEF-148 — then the compiler's own refusals and
> traps, then the two tables), its first step goes BEFORE 1.6.1d steps 3b and 4 — the user, 2026-09-26: "the order you
> proposed is fine with me". PLANNED execution-grade 2026-09-26: `meta/roadmap/1.6/1.6.1e.md`.**

> **DEF-144 — FIXED at 1.6.1e step 1 (2026-09-26; F-018). A MACRO'S FREE NAME, STANDING ALONE OR AS A
> COMPARISON'S OPERAND, READS THE CALL SITE'S LOCAL INSTEAD OF THE MODULE BINDING** it names at the definition
> (inside arithmetic it reads the right one) — a hygiene hole in the expansion (D-057/D-127's rule that a macro's
> names bind in its defining scope), a silent wrong answer. **The fix (1.6.1e step 1):** the RESOLVER had it right
> (the body's name bound to the module binding, the symbol recorded on the node); the EMITTER looked the name up in
> the function's local table by NAME, innermost first, and read the caller's local. `ident_slot` (ir_expr.npk)
> resolves an identifier node to its storage by the resolver's symbol — a `SYM_STMT` symbol by its declaring
> statement, a parameter by the prologue's binding, every other declaration (a function, an error constant, a
> global) to `emit_decl_value` before any local is consulted — and the six sites that asked the name ask it.
> `macro_hygiene_names.npk`: the listener's four shapes, a module function under a caller local of its name, a
> module binding under a caller local of another width, `#caller(lvl)` beside the bare name, each in a sync body
> and in a coroutine.

> **DEF-145 — FIXED at 1.6.1e step 1 (2026-09-26; F-019). A `'\u{…}'` ESCAPE IS TYPED `char8` AND TRUNCATED TO
> ITS LOW BYTE** (`'\u{1F641}' == 'A'` is true), and a `char32` cannot take it (TYPE-007) — a silent wrong answer in
> the lexer's or the literal typer's reading of a scalar above U+00FF. **The fix (1.6.1e step 1):** a character
> literal has a WIDTH, carried as the numeric literals carry theirs (the token's and the node's `width`, `WChar8` or
> `WChar32`, one token kind and one node kind): the `\u{…}` escape is `char32` whatever its value (TYPE_REFERENCE
> §2.2), a source character above U+00FF is `char32` too (a code point that does not fit a byte was never a `char8`;
> the truncation was the defect), a plain character or a `\x` escape stays `char8`; the typer, the folder and the
> emitter read the width (`char_lit_bits`). `char_literal_width.npk` (both legs), `char_escape_width.npk`
> (exactly {TYPE-007}: a `\u{…}` escape in a `char8` and a `char16` slot).

> **DEF-146 — FIXED at 1.6.1e step 1 (2026-09-26; F-020; under S-115's recommendation). THE SCOPE-EXIT JOIN RELAYS THE LAST-SPAWNED
> CHILD'S ERROR, NOT THE FIRST CHILD ERROR** CONCURRENCY_REFERENCE:79 promises — D-136's first-child-error
> arbitration read against the join walk (D-207): a silent wrong answer in which error a caller sees. **The fix
> (1.6.1e step 1):** "first" is SPAWN ORDER — the earliest-spawned child that failed, a function of the program's
> text and the same under every schedule the explorer runs (S-115, recommended and ratified the same day as D-334) — so a child's
> error OVERWRITES one a previous child of the walk stored (the chain is a LIFO: the last stored is the earliest
> spawned) and never the function's own; `main`'s join (a bare function) joins every child before it enters
> `failsafe` with the same child's error, where it entered `failsafe` at the first error it met. The origin chain
> records the function once per walk. `join_first_spawned.npk` (six shapes, thread children among them; explored)
> and `join_main_first_spawned.npk` (82). The listener's `ctl_j3` (slow spawned first, fast second, expecting E1)
> answers E2 by the rule: its expectation was the first-in-TIME reading, and the file is theirs to re-spell.

> **DEF-147 — FIXED at 1.6.1e step 1 (2026-09-26; F-021). A `timedwait` THAT EXPIRES WITH NO SIGNAL RETURNS
> SUCCESS AFTER THE FULL WAIT** — an error path (the deadline, `DeadlineExceeded` or the spent guard of 1.1.11)
> becomes a success: a silent wrong answer in a synchronisation primitive. **The fix (1.6.1e step 1):** nothing
> recorded that phase 0 ended by the deadline, and the floor's `npk_mutex_acquire_wait` reads the clock only when it
> must park — so the resumed frame took an uncontended mutex at once. The emitted wait reads the clock at its resume
> (`emit_condvar_wait`'s `reacq`): at or past the absolute deadline it is `DeadlineExceeded` without re-acquiring,
> the lent guard nulled (SPENT, D-056's amendment) and the frame off the condvar's list; a signal before the
> deadline succeeds with the guard whole. No floor byte. `condvar_expiry.npk` (an expiry with the mutex then
> re-acquired at once, a signal, a broadcast; explored); the listener's `t1` answers 0.

> **DEF-148 — FIXED at 1.6.1e step 1 (2026-09-26; F-022). A `shared_arena` CAN BE DESTROYED WHILE A
> SPAWNED THREAD HOLDS IT; the thread then allocates in it and the program ends in `WildLeak` (96)** — a
> use-after-free in safe code; destroying after the join is clean. D-180 sanctions the `shared_arena<T>->` spawn
> crossing on the ground that the join precedes the free (1.4.4's order); `.destroy()` inside the holder's scope
> defeats the order, so `.destroy()` on a borrowed-across-a-spawn arena is the shape to refuse or to sequence.
> **The fix (1.6.1e step 1):** a spawn LENDS its crossing until the block's join — `NITPICK-BORROW-016`. A `drop
> f(…)` on an `async` callee that hands a task a pointer to one of D-180's five kinds pushes a fifth party kind of
> the aliasing walk (`PARTY_LENT`, alias.npk) on the argument's root, sited at the call, live until the enclosing
> block's exit; every write-capable access of the root refuses — `destroy()`, an assignment over it, a `move` out,
> `$$m`, `@root` to a callee whose mutation summary stores over the pointee, and the spawn's own callee read against
> its own lent party (a task that stores over what it was lent is refused at the spawn) — while the kind's own
> concurrent operations (`alloc`/`get`; `acquire`/`read`/`write`/`timedwait`/`signal`/`broadcast`/`arrive`, ONE
> table in escape.npk beside D-180's kinds), `$$i`, a second spawn and a helper that only allocates stay. The
> escape analysis's summaries needed no change: a builtin method records no write, and `destroy` through a pointer
> is TYPE-091 now (DEF-157). `spawn_lent.npk` (seven BORROW-016 sites, the controls silent), `spawn_lent_ok.npk`
> (the owner and two tasks allocating in one arena, destroyed after the join; explored); the listener's `a1`
> refused, `a2`/`a3` run.

> **DEF-149 — FIXED at 1.6.1e step 2 (2026-09-26; F-023; `type_method_call` applies the direct call's rule at the point the method's callee is final -- an impl's method, a trait's default, a `dyn` method, the UFCS free function -- `NITPICK-TYPE-043` for a bare call of an `async` method; the listener's `i1_unawaited_method_call` refused where `llc` had refused its emission; `tests/types/rejection/async_method_bare.npk`). AN UN-AWAITED ASYNC METHOD CALL (`r.read(…)`) IS
> ACCEPTED**; npkc exits 0 and emits a call to an undefined symbol, which `llc` and `opt` refuse — the free-function
> case is TYPE-043; the method spelling escaped the rule (a rule written for one spelling owed to the other).

> **DEF-150 — FIXED at 1.6.1e step 2 (2026-09-26; F-024; three stops, named by the debugger first: c1 and c3 in `record_site` reading a macro body's window by the body's one kind, c2 the constant folder's recursion off the compiler's stack. c1: the reference's own `emit_methods` example spelled its receiver `$$i Box:self` -- never a parameter form -- and the parse error inside the macro body left a window the expander read past; a body with a parse error expands to nothing now (the errors are the report), a body mixing declarations and statements is `NITPICK-MACRO-010` at the statement, and MACRO_REFERENCE §4's example reads `Box:self`. c3: `comptime func:` in a body was classified a statement because the lookahead stopped at the modifier; `p_at_decl_start` reads past the modifiers. c2: no tree nests deeper than 256 levels (`AST_DEPTH_MAX`, a function's body block the first) -- the parser measures each declaration's tree after parsing it (`ast_depth.npk`: the construction log, children first, one pass) and refuses the first subtree past the bound `NITPICK-PARSE-012` once, its own descent in `p_unary` held to the same number, the expander refuses a splice landing past it `NITPICK-MACRO-003`, every whole-body analysis depth reads the constant, and a program that did not parse is not expanded; the step's first form counted the parser's descent alone and a 600-term LEFT chain `1 + 1 + ...` still trapped the folder (found by the successor seat's probe, fixed in the same step); the listener's 250-deep control parses, its 500-deep program is refused, and `tests/backend/programs/expr_depth_max.npk` runs at the bound (254 levels of expression under a body and a statement). `tests/backend/programs/macro_emit_decls.npk` runs the reference's two examples; the MACRO-010 and PARSE-012 cases are `tests/frontend/parse_decls.npk`'s, since a refusal the parser makes cannot be a file in the tree (D-085) -- the step's first form had them as rejection files and its harness refused them). npkc TRAPS (exit 3, no message)** on a macro emitting a
> method into an impl, on a 500-deep expression (with or without a macro; 250 compiles), and on a macro emitting a
> `comptime` function — three uncontrolled stops of the compiler itself (a trap inside the compiler is a `src/`
> defect; the depth one is a recursion with no measure the runtime's `StackExhausted` should have named).

> **DEF-151 — FIXED at 1.6.1e step 2 (2026-09-26; F-025; the mode is the FRONT HALF's -- `front_run_mode(f, root, no_wildx)`, set by the driver and by `tools/check.npk` from the same argument -- and `reject_wildx` walks the program's modules only (a node belongs to the prelude by its span's file) and reads four spellings: a `wildx` local, a `wildx`-returning function, an `=>! wildx …->` cast, a call of `wildx_alloc`/`wildx_seal`/`wildx_call`/`wildx_free` bare or `#`; both runners read `// npkc-flags:` from a rejection or accept file's header and hand the words to the tool; the listener's canary compiles under the flag; `tests/analysis/rejection/no_wildx.npk` (the four spellings), `tests/accept/no_wildx_clean.npk`; harness.py's "build-mode" excuse for WILDX-003 retired). `--extra-picky=no-wildx` REFUSES EVERY PROGRAM, THE
> CANARY INCLUDED, AT ITS OWN PRELUDE** (256 `NITPICK-WILDX-003`, the first at prelude.npk:125): the rule reads the
> prelude's `wildx` machinery as the program's.

> **DEF-152 — FIXED at 1.6.1e step 2 (2026-09-26; F-026; a builtin scalar reached with type arguments is `NITPICK-TYPE-016` at the type -- "`tfp64` takes no type arguments; a unit rides `dim256<Unit>` alone (D-196)" -- in `resolve_named`'s builtin branch; the listener's `d1_tfp64_unit_mismatch_compiles` refused; `string`'s three spelled forms `string<char8>`/`<char16>`/`<char32>` kept as the plan settled (step 2's first form refused them, contradicting TYPE_REFERENCE §3.2; their MEANING is S-116), every other argument on a `string` refused; `tests/types/rejection/builtin_no_args.npk`). `tfp64<Meters>` IS ACCEPTED AND ITS UNIT IGNORED**, so
> `Meters + Seconds` compiles; the same over `dim256` is TYPE-049 — a unit annotation on a kind that carries none
> (D-196: units are `dim256`'s alone) is accepted and dropped instead of refused.

> **DEF-153 — FIXED at 1.6.1e step 3 for its rows (2026-09-30; F-027's sixteen: three landed with 1.6.1d step 3b — the
> `cstring` literal; twelve at step 3, each as `1.6.1e.md` §2.10b READ it on `5fbaf4a` — `ty0176` TYPE-001, `ty0178` and
> `ty0189b` LEX-009, `bi0256` TYPE-043, `cc0380` LOCK-001, `vf0831` LOCK-002, `mc0246` MACRO-004 at the declaration,
> `mc0251` and `mc0344` their two diagnostics, `mc0278` and `mc0046b` compiling, and `mc0085b` a DOCUMENTATION row: a
> spliced `fixed` qualifier TRAVELS since 1.0.8, the refusal the plan had scheduled is withdrawn and MACRO_REFERENCE §2
> corrected; and one, `mc0309b` — compile-time string ordering — settled by the user as D-338 and landing as step 3b. The
> reading changed five of the plan's twelve decisions and found the rows' neighbourhoods, DEF-166 … DEF-190).** The table, as registered: THE ACCEPTED-THOUGH-REFUSED AND
> REFUSED-THOUGH-PERMITTED TABLE: `f512` and `flt32` suffixes, `flt256`, a spliced `fixed` qualifier, a
> self-invoking macro, unchecked channel and `dyn` lock levels (accepted where the reference refuses); the
> compiler's own named holes (EMIT-002 for `suspend_until` and `flt256`, MACRO-006 for a `decreases` measure); a
> string literal in `cstring` position, a declaration-macro alias, `comptime` string comparison (refused where
> the reference permits); and two diagnostics. Each row decided as a rule kept or a sentence corrected, at the plan.

> **DEF-154 — OPEN (F-028, ninety-four rows; owner: the compiler seat, 1.6.1e, with DEF-133's method). THE
> DOCUMENTATION TABLE**: surfaces that do not exist (TYPE_REFERENCE's char and string function tables,
> tensor/matrix, `Stream`, `Actor`, `#align_of`, `--seccomp`, `--extra-picky=no-sys` and `no-wild`), stale spellings
> and codes, examples that do not compile, rules changed by D-180, D-222, D-177 and D-197, self-contradictions
> (`await` yielding `Result<T>` against `T`; `requires` on `never fails`), and emission and encoding misdescribed.

> **DEF-157 — FIXED at 1.6.1e step 1 (2026-09-26; found by the step's DEF-148 probes, registered and fixed the
> same day). `destroy` THROUGH A POINTER FREED THE ARENA THE OWNER'S SCOPE EXIT FREED AGAIN** — `MachineFault`
> (107) in safe code, measured on 3235308: `kill(shared_arena<int64>->:s) { s.destroy(); }` called with `@s` from
> a function that returns. `destroy` CONSUMES its arena (D-152): the move analysis retires the receiver's binding
> and the emitter clears its drop flag — through a pointer parameter there is no flag to clear, so the owner's drop
> ran `npk_sarena_destroy` on a freed structure (a plain `arena`'s destroy is idempotent, its slab freed twice all
> the same); a temporary's `destroy` is freed again at the statement's end (D-246). **The fix:** the receiver of
> `destroy` is storage THIS function owns — a local or a by-value/`move` parameter, or a field or element of one
> reached through no pointer, slice or handle — else `NITPICK-TYPE-091` at the call (`destroy_receiver_owned`,
> type_members.npk, both arena kinds). No program of the tree or the listener's repositories destroyed through a
> pointer (the census: every receiver a binding or a local struct's field). `destroy_owned.npk` (a pointer
> parameter of each kind, a dereference, a pointee's field, a temporary; the controls: a binding, a local's
> field, a `move` parameter).

> **DEF-158 — FIXED at 1.6.1e step 1 (2026-09-26; found by the same probes). `destroy` ON AN ARENA FIELD CLEARED
> THE WHOLE AGGREGATE'S DROP FLAG, SO EVERY OWNING SIBLING LEAKED** — `app.store.destroy()` on `App{ string:name;
> shared_arena<int64>:store; }` left `app.name`'s body live at every scope exit (measured on 3235308 under
> NPK_HEAP_STATS: peak 72,592 bytes over 2,000 rounds against 628 for one). **The fix:** `destroy` VACATES its
> place as a move out of it does (D-254, S-26; `destroy_vacate`, ir_expr.npk): the type's all-zero vacant value
> stored (the null slab a shared arena's drop tests, the empty arena `npk_arena_destroy` frees nothing of) and the
> vacant helper called, so the aggregate stays live and drops its siblings; a WHOLE binding keeps the flag clear
> as its fast path and is zeroed too, so a held `@s` that stores over it (DEF-120's drop of the old pointee) meets
> a vacant value and not the freed arena. The move analysis retires the aggregate whole after a field's `destroy`
> as it does after a field's `move` (its coarseness, not the emitter's); the drop at the scope exit is what the
> fix corrects. `tests/cost/destroy_field.toml` (`destroy_field_churn.npk` against `destroy_field_once.npk`,
> peak 628 and 628 on both legs; 72,592 before).

> **DEF-159 — OPEN (owner: the compiler seat; found by 1.6.1e step 1's probes of DEF-120's own shape, 2026-09-26).
> A VALUE STORED THROUGH A HELD `@x` AFTER `move(x)` LEAKS**: `string->:p = @x; string:t = move(x); (<-p) = v;` —
> the shape DEF-120's record calls legal (a held `@` is D-286's plain party, and `move(x)` is not its conflict) —
> clears `x`'s drop flag at the move and stores a live `v` into the vacated slot through `p`; the scope exit reads
> the flag and drops nothing (measured on this step's compiler under NPK_HEAP_STATS: peak 7,236 bytes over 200
> rounds against 36 for the control that stores nothing — one body per round), and the move analysis holds `x`
> moved-from, so no read of `x` can reach the value either. The cause is the drop FLAG standing beside the vacant
> value: since DEF-120 every moved-out place holds the canonical vacant value, whose drop is a no-op (D-225's
> invariant), so the flag's one remaining job — the fast path — is what hides a value written back through a
> pointer. **Recommended:** the scope-exit drop of a binding whose slot a held `@` can reach runs unconditionally
> over the slot (a drop of a vacant value is a no-op; a re-initialised one is dropped), the flag kept as the fast
> path for a binding no address of which is taken — the emitter already knows which bindings are address-taken
> (the frame-residency scan, D-191); measured on the tree before it lands. Its own landing, after 1.6.1e.

> **DEF-160 — FIXED at 1.6.1d step 4 (2026-09-26; found and fixed the same day by the step's own probe of the
> escape analysis's pointer predicate, not by a test of the thing that broke). AN ENUM PAYLOAD CARRIED AN
> ADDRESS NOBODY TRACKED: `E.Some(@local)` returned from a function compiled since D-261 gave enums payloads,
> and the caller read a dead frame through the payload (exit 3 at -O0, 0 at -O2 on a twelve-line program; the
> same inside a struct, and stored through a call into an outliving pointee).** Two causes, one class: the
> layout recorded no pointer-bearing bit for an enum (`field_holds_ptr`'s "a tag and a word" was true of
> 0.8.1's payload-less enums), so `type_holds_pointer`'s verdict filter dropped every borrow inside one; and
> the enum constructor -- method-call-shaped, no callee, no function type -- was read by the escape analysis
> as an intercepted builtin method whose arguments carry nothing. The fix: the enum-layout arm records the bit
> from its payloads (`tt_set_haspt`, as a struct's), the three pointer tables read it, `type_reachable_in`
> descends payloads, and `call_is_enum_ctor` makes a constructor an unknown callee whose arguments' refs are
> the value's (as a struct literal's field values are). `tests/analysis/rejection/enum_payload_escape.npk`
> pins the three faces (BORROW-001, BORROW-001, BORROW-002). A refusal added, in F22 with the sweep.

> **DEF-161 — FIXED at D-332's landing (2026-09-27; found by D-332's count rule on `tests/types/rejection/
> variadic_rules.npk`). A REFUSED `..^` ARGUMENT TO `sys` WAS FIT-CHECKED TOO:** `sys(60i64, ..^xs)` reported
> TYPE-007 twice at one site -- the spread refusal, then "an argument to `sys` must fit a kernel register ...
> this is `int64[]`" -- one mistake, two reports, D-240's own case (1.4.8c: "a refused `..^` argument is not
> also fit-checked") applied to the general argument path and missed in `sys`'s register check
> (type_members.npk). The operand is still typed, so a mistake inside it is reported; the register rule says
> nothing more about an argument already refused. A set-matching runner could not see it: the file names
> TYPE-007 elsewhere.

> **DEF-162 — FIXED at D-332's landing (2026-09-27; found by D-332's count rule on `tests/types/rejection/
> simd_rules.npk`). A `simd(…)` UNDER A REFUSED ANNOTATION REPORTED THE ANNOTATION MISSING:** `simd<Box,
> 4>:v = simd(Box{ v: 1i32 });` reported TYPE-001 at the annotation and TYPE-001 again at the constructor
> ("`simd(…)` needs a `simd<T, N>` annotation"), four times in the file -- a refused type resolves to 0, the
> value that also means "no expectation", so the constructor read the annotation's refusal as its absence.
> `ExprTyper.expect_refused`: a declaration (which always writes its type) sets it while its initialiser is
> typed when the type was refused, and the constructor is silent when it is set and nothing directs it; it
> can only silence a second report in a program already refused at its annotation (type_expr.npk,
> type_stmt.npk).

> **DEF-163 — FIXED at D-332's landing (2026-09-27; found by D-332's count rule on `tests/types/rejection/
> not_constant.npk`). A REJECTION FILE EXPECTED A REFUSAL D-222 HAD MADE LEGAL, AND THE COMPILER'S MESSAGE
> NAMED A RETIRED KEYWORD:** the file expected TYPE-004 at `int32[RUNTIME_ONE]` ("`fixed` is assigned at run
> time"), but D-222 (2026-08-29) made every module binding a `fixed` one lowered to an LLVM `constant` with a
> compile-time initialiser, so it folds and is accepted; the code was reported at the file's other two sites,
> so no runner noticed for a month -- the silent site D-332 exists to catch. The line is a control now, and
> the two messages that said "a name folds only if it is a `const` global" say "a module-level `fixed`
> binding (D-222)" (type_resolve.npk).

> **DEF-164 — OPEN (2026-09-27; owner: the compiler seat; found by D-332's count rule run over the library
> listener's corpus, `nitpick-time/tests/probe/probe14_error_payload_refused.npk`). THE PARSER'S RECOVERY
> REPORTS ONE MISTAKE TWICE:** `pub error:ETimeValue(ValueFault);` -- an error identity with a payload, which
> the language does not have -- is PARSE-001 at the `(` ("expected `;`") and PARSE-001 again at the `)`
> ("expected `:`"): after the first report the parser resynchronises inside the same declaration and reads
> `ValueFault)` as a new item. D-240: one mistake, one report. Recommendation: the declaration-level
> recovery skips to the declaration's own `;` (the sync point it already knows) before reading an item again;
> measured over the tree and the listener's corpus with both checkers before it lands, since every parse-error
> test's count moves with it. **Two more of the family, measured at 1.6.1e step 3 (2026-09-30) and not fixed by
> it:** an integer run with a bad digit is LEX-003 and then the parser's PARSE-002 "expected an expression" at the
> same token (`int32:a = 12abc;`, the same on `5fbaf4a`), and a float's dangling exponent sign is LEX-009 and then
> PARSE-002 at the operator left behind (`flt64:a = 1.5e+;`) — the lexer's refusal answered by a second sentence
> from the parser. For the first the fix is the one LEX-009's own token takes (D-240): a refused literal stays a
> literal token, so the parser has an operand.
>
> *[2026-10-01, observed by `nitpick-compiler_24`; the same family — one mistake, two reports (D-240) — to be fixed
> with it:]* a literal outside the 64-bit envelope is `NITPICK-LEX-004` AND `NITPICK-PARSE-002` ("expected an
> expression") at the same token; an `impl` method whose parameter type is refused (`TYPE-001`) is ALSO
> `NITPICK-TYPE-014` ("does not have the signature its trait declares"), the comparison reading the refused type
> as a mismatch (`th2`).

> **DEF-165 — FIXED 2026-10-08 (landing 100, `nitpick-compiler_32`): `type_struct_literal` collects the sealed fields a literal writes from outside their module and reports ONCE at the literal, naming them all (`sealed_literal.npk`: the parent reported 13 TYPE-079 at its five literals, this compiler 5; a literal writing one sealed field keeps the per-field sentence; the "declared `sealed` here" note stays under each field). THE READING SETTLED 2026-09-30 as D-337 (one report per literal, naming every sealed field it writes; the fix its own landing with an advance notice) (2026-09-27; owner: the compiler seat; found by D-332's
> count rule over the listener's corpus: `nitpick-regex/tests/rejection/pattern_error_literal.npk`,
> `nitpick-time/tests/probe/probe15_civil_literal_bypass.npk`). A STRUCT LITERAL OF A TYPE WITH SEALED FIELDS,
> WRITTEN OUTSIDE ITS MODULE, IS REPORTED ONCE PER FIELD:** `PatternError{ kind: …, offset: …, span_len: …,
> detail: … }` is four TYPE-079 at one span (D-313's write forms include "a struct literal"). Two readings:
> each sealed field is its own fact (the author learns every field at once), or the literal is ONE mistake --
> constructing the type outside the module that owns its invariants -- and D-240 says one report.
> Recommendation: one report per literal, naming every sealed field it writes, since the fix is the same one
> action (call the owning module's constructor) whatever the count.

> **DEF-166 … DEF-173 — found by 1.6.1e step 3's reading and probes (2026-09-30, `nitpick-compiler_22`; the rules in
> `1.6.1e.md` §2.10b; FIXED at step 3 unless marked).** DEF-166: a float literal's trailing text was IGNORED --
> `1.5garbage`, `2.5e3zz`, `1.0f64x` compiled (the listener's `f512`/`flt32` rows are two instances); `NITPICK-LEX-009`.
> DEF-167: a literal with a fraction and an integer, `tbb` or `char` suffix (`2.5i32`) was typed as the integer --
> its VALUE read from the intern index of its text -- and died as EMIT-002; `NITPICK-TYPE-031` at the literal
> (inside `comptime(…)` and as an array size the folder speaks first -- TYPE-004, one report either way).
> DEF-168: `io_watch` in a synchronous function compiled and, called where no task runs, faulted the machine
> (`MachineFault`, exit 107, in safe code); the three task builtins are TYPE-043 outside an `async` function.
> DEF-169: a park (`suspend_until`, `suspend_io`) inside a `defer` body trapped the COMPILER (exit 3, no message: the
> park's wind-up exit re-emits the defer body, which holds the park -- unbounded recursion in the emitter); TYPE-043.
> DEF-170: the expansion walk missed eight expression positions (a loop's measure and invariant, a function's
> contracts, a `Rules` clause, an array size and a value argument in a type, a pick pattern), each MACRO-006 -- and
> the eight were eight of many: the fix reaches every expression a statement or a declaration carries (every type
> position, a variant's value, a `unit:` and an `error:` right-hand side, attributes, an extern function's `fails on`;
> MACRO_REFERENCE §6), a template's shared nodes the walk reaches are AUDITED (`(#no_such())` as a pattern in a macro
> body passed the front half in silence and died as EMIT-002; MACRO-001 now), and MACRO-006 is reached by no program
> of the tree or the listener's corpus (43 sites before).
> DEF-171: an alias macro (a body that is one invocation) was refused at a struct and an impl/trait site as at module
> level (MACRO-005). DEF-172: a `macro:` declared inside a macro body was MACRO-006, the compiler-defect code, when
> the outer one expanded; MACRO-005 where it is written. DEF-173: MACRO-009 annotated its own notes (the driver
> called `expansion_notes` twice at one return).

> **DEF-174, DEF-175 — FIXED at 1.6.1e step 3 (found by WP-A's implementer inside the float scan, 2026-09-30).**
> DEF-174, A SILENT WRONG ANSWER: **a sign after a float literal was swallowed into the literal** -- the lexer read
> `digits . ident-part*`, then ANY sign, then `ident-part*`, and kept the run as the literal's text, so
> `flt64:y = 3.5f64-x;` with `x == 1.0` stored 3.5 at both legs on `5fbaf4a` (`2.5-1.5` was TYPE-019). The scan
> takes a sign only inside an exponent that has a digit after it. DEF-175: **a `_` separator in a float literal never
> worked** -- `1_000.5tfp64` FOLDED TO 1.0 and `1_000.5f64` was refused by `llc`; the literal's text is kept without
> the separators.

> **DEF-176 — OPEN (2026-09-30; owner: the compiler seat; found by WP-A's implementer). THE EMITTER EXHAUSTS THE
> COMPILER'S STACK ON A NON-FOLDING CHAIN OF 54 OPERANDS:** `int32:y = x + x + … ;` with 53 terms compiles and with
> 54 `npkc` exits 3 with no message (`npk_stack_exhausted` in `emit_expr` <-> `emit_binary`): `emit_expr_kind`'s frame
> is 123,032 bytes, so `AST_DEPTH_MAX` (256, DEF-150) does not protect the emitter -- `expr_depth_max.npk` passes only
> because its chains constant-fold. Reproducer: `wt/22b/.internal/probe/chain54.npk` (copy it into the landing's
> tests). **Recommended:** make the frame small (the function's arms as functions of their own, so one level costs the
> arm it takes and not the sum of every arm's temporaries), measured by the frame size and by a 256-deep non-folding
> chain compiling; a refusal by name is the fallback, never a trap. Its own landing, next after step 3.

> **DEF-177 — OPEN (2026-09-30; owner: the compiler seat; found by WP-A's implementer). A `.` FOLLOWED BY ANY TOKEN IS
> A MEMBER ACCESS BY THAT TOKEN'S PAYLOAD READ AS AN INTERN INDEX** (`parse_expr.npk`, the `PDot` arm): `p.386`
> reads field `x` of a two-field struct and `p.387` reads `y`, compiled and run; `p.99999999` traps the compiler
> (exit 3); `p.5` says "no member `Greater`". **Recommended:** after `.` the parser takes an identifier or one of the
> keywords that name a member after a dot (`acquire`, `any`, `trit`, `nit`, …), and anything else is PARSE-001 at the
> token -- a unit case, not a rejection file (D-085).

> **DEF-178 — OPEN (2026-09-30; owner: the compiler seat; found by WP-A's implementer). A VALUE PATTERN'S LITERAL IS
> NEVER TYPED AGAINST ITS SELECTOR:** over an `int32` selector `(true)`, `(300i64)`, `(44u8)` and `('A')` compile and
> match; `("abc")`, `(2.5f64)`, a fraction range and any pattern over a `flt64` selector pass the checker and die as
> EMIT-002. **Recommended:** `type_pick_rules` types each pattern literal and range bound under the selector's type
> (TYPE-007 for another kind or width, TYPE-031 for a value outside it; a float or a string selector has no value
> patterns -- TYPE-052's sentence for the kinds that cannot select), both spellings.

> **DEF-179 — FIXED 2026-10-01 (landing 87) for `flt64`, `tfp` and `dim256`, and for `flt32` at landing 88 (DEF-203). READ 2026-10-01 (`nitpick-compiler_23`): A DEFECT, NO USER QUESTION (owner: the compiler seat;
> reported by WP-A's implementer).** A module-level `fixed flt64:PI = 3.14f64;` and `fixed tfp64:Q = 2.5tfp64;` are
> `NITPICK-TYPE-035`, on `5fbaf4a` and on `9efe218`. D-165 is the rule: "the initialiser is a compile-time constant
> expression: a literal OF ANY KIND" — the refusal is the folder's (`ConstVal` holds integers, bools, chars and
> strings and no float), not the language's. The workaround today is a function — `func:PI = flt64() never fails
> pure { pass 3.14f64; };` and `raw PI()` (probe `mf2`: clean). Nikola's code will want module-level float and
> fixed-point constants: ITS OWN LANDING, AHEAD OF step 3b — the folder's value gains the float and twisted
> literals, the global's initialiser lowers them, and a float constant folds where a constant is asked for.
>
> **FIXED (landing 87, `nitpick-compiler_25`).** The folder's value gained `CV_REAL` — a float or fixed-point
> constant carried as its literal's decimal text, a sign beside it, never computed — so `fixed flt64:PI = 3.14f64;`,
> `-2.5f64`, `= PI`, `comptime(2.5f64)`, an unsuffixed `0.125` in a float slot, a float array and a struct of
> floats, and the same forms at `tfp` and `dim256<U>`, are constants: the text reaches LLVM's own decimal conversion
> (`double 3.14`) or `tfp_q_decimal`, the conversions a literal in a body goes through. `comptime(…)` of one works
> where it is read. An expression over them stays refused, with its own sentence (S-125).
> `tests/backend/programs/module_constants.npk` (sixty cases, each constant against the same value built where it is
> read; exit 0 at both legs, refused by `f13914c`).

> **DEF-180 — OPEN (2026-09-30; owner: the compiler seat; reported by both implementers). THE PIPE FORM `x |> f` IS
> NOT A CALL TO EVERY READER:** a pipe into a bare builtin (`x |> string_byte_length`, `x |> suspend_until`) is
> silent in the checker and EMIT-002; `type_pipe` records no callee, so a recursive call written as a pipe is no
> edge of the recursion groups (a measured function is TYPE-075 "nobody calls back" and its `decreases` is not
> checked at that call); the lock walk had no pipe arm until step 3. **To do first:** an audit of every reader of
> `ExprTypes.callee` and of every analysis's call arm against the pipe form -- the move rules for a `move`
> parameter, the reach analysis's arms, the provenance summaries, the aliasing walk, the contract rows -- each by a
> probe; then one fix (the pipe records its callee and every reader asks one predicate for "a call's callee and
> arguments") rather than an arm per analysis.

> **DEF-181 — OPEN (2026-09-30; owner: the compiler seat; found by WP-B's implementer's probes of the lock walk --
> each ACCEPTED before and after step 3; `wt/22c/.internal/probe/`).** The lock-order proof (D-056's first layer)
> has six holes: (1) at a DIRECT call the `acquires <= N` bound stands in for a body whose own summary is known, so
> holding 2 and calling an impl bounded at 3 that takes 1 passes (`pa_hole:19`; a measured four-line patch,
> `wt/22c/.internal/patch_F4_minmerge.diff`, moves no file of the tree); (2) a call through a FUNCTION VALUE
> contributes nothing (`pd_self:50`) -- by D-056's own sentence an undeclared dynamic call may not acquire, so a
> function named as a value may reach no level; (3) a bound result holds the callee's LOWEST hold, not its highest
> (take and release 1, return holding 9, then take 5: `pf_hold:19`); (4) a `defer` body is ordered against what was
> held where it is WRITTEN, not at the scope's exit where it runs with every later guard still held (`pf_hold:31`;
> D-207's order is join, defers, drops); (5) a guard taken AFTER a spawn is held across the block's join, and the
> child that needs it waits for ever -- `pm_join` exits 21 (`DeadlineExceeded`) at both legs: a spawn is judged at
> the block's end, against everything held there; (6) a level equal to one of the walk's two sentinels
> (`2147483647`, `-2147483647`) reads as "no level" (DEF-69's lesson: a sentinel is a value the domain never
> produces -- refuse the two values as levels). And one over-restriction: a helper that takes a mutex internally and
> returns an `int32` cannot be bound twice in one block (`pn_helper:11`). With them, **S-119 for the user**: an
> `acquires <= N` bound puts no FLOOR under an implementation (holding 2, a call through `dyn` bounded at 3, an impl
> that takes 1: `pa_hole:26`) -- D-056 lists "a declared-but-broad dynamic bound" among what its second layer
> contains by deadlines; the question is whether to close it (a call through a `<=`-bounded method while anything is
> held is refused, and an EXACT `acquires N` on a trait method means its implementations reach N and nothing else),
> measured on the tree first. A step of its own, 1.6.1e step 3c, planned execution-grade before it starts.

> **DEF-182 … DEF-188 — OPEN (2026-09-30; owner: the compiler seat; found by WP-C's implementer around the macro
> rows; each measured the same on `5fbaf4a` and on step 3's tree; probes in `wt/22d/.internal/probe/`).**
> DEF-182, THE PARSER DROPS WHAT IT HAS NO NODE FOR, IN SILENCE: an attribute before a statement that is not a
> declaration, before a global, a `Rules` block, `use`, `mod`, `extern`, `assoc`, `opaque`, `error`, `unit` or a
> splice is discarded with whatever it holds (`#[note(undefined_name)] while …` compiles), and the second of two
> `decreases` clauses on one loop is discarded where TYPE-072 is the right report -- recommended: refused by name
> where it cannot be kept. DEF-183, **A CALLER'S LOCAL CANNOT BE A MACRO ARGUMENT**: `#twice(n)` with a local `n`
> is RESOLVE-002 "cannot find `n` in the scope this macro was written in" -- the ARGUMENT, the caller's own text,
> is resolved as body text (measured: `wt/22a/.internal/probe/pm_arg.npk`). **AND IT HAS A SILENT FACE (2026-10-01,
> `nitpick-compiler_23`'s probe `ha1`): an argument naming a caller's local that SHADOWS a module binding reads the
> MODULE's** — `fixed int32:shared = 100i32;` at module level, `int32:shared = 5i32; exit #twice(shared);` in `main`
> over `macro:twice = (X) { X + X; };` exits 200 where the caller wrote 10, at both legs, on `5fbaf4a` and `9efe218`.
> READ against D-057: its rules speak of an identifier in a macro BODY, and of `#caller` as the body's way out; an
> ARGUMENT is in neither sentence — **S-120** (§2e-quinquies), the user's. Recommended (R1): an argument's names
> resolve at the invocation site, the body's at the definition; the mechanism for either answer is planned
> (`1.6.1e.md` §2.10c part B), and it lands the day he answers. DEF-184, DEEP
> TYPES TRAP THE COMPILER (exit 3, no message): `List<List<…>>` at about 8,300 levels, `func func …` at about
> 9,350, `->` at about 12,700 -- `AST_DEPTH_MAX` measures expressions and statements and no type node;
> recommended: the depth pass counts type nesting under the same bound. DEF-185, the macro clone has no case for
> `error:`, `unit:`, `assoc:`, `use` or `opaque struct:` in a declarations body (MACRO-006 at instantiation; an
> `assoc:` in an impl-body macro is a plausible use) -- recommended: cloned, or refused by name where the macro is
> declared. DEF-186, generic arguments on a macro invocation (`#one<int32>()`) are silently ignored -- recommended:
> refused. DEF-187 (with DEF-178), a value pattern that is not a literal -- a local `(n)`, a `fixed` name `(K)` -- is
> accepted by the checker and EMIT-002; and `func:lit<comptime int32:K> = int32() never fails { pass K; }` passes
> the frontend and is EMIT-002. DEF-188, MACRO-009's range is the body items' start lines plus one line of slack:
> two adjacent one-line macros get a false note and a diagnostic deep in a multi-line item gets none --
> recommended: the declaration records its closing brace's line. Scheduled with DEF-176 … DEF-180 as 1.6.1e step
> 3d (the rows' neighbours), planned execution-grade before it starts, by severity: the traps (DEF-176, DEF-184,
> DEF-177's), the checker's holes (DEF-178, DEF-187, DEF-180), DEF-183, then the rest.

> **DEF-189 — FIXED at 1.6.1e step 3a (2026-10-01, `nitpick-compiler_24`; registered 2026-09-30, found by WP-C's
> implementer answering the incoming seat's review question). A MACRO PARAMETER WRITTEN IN A TYPE OR A PATTERN WAS
> NEVER SUBSTITUTED, AND WHERE THE MODULE HAD A BINDING OF THAT SPELLING THE BODY SILENTLY READ IT.** `fixed int32:N =
> 2i32; macro:mk = (N) { int32[N]:a = …; … #size_of<int32[N]>() … };` invoked as `#mk(3i32)` built a TWO-element
> array: exit 8 where three elements give 12, the checker clean, -O0 and -O2. MACRO_REFERENCE §3 was the rule — an
> argument replaces EVERY occurrence, substitution traverses the whole emitted subtree — and the clone had departed
> from it since 0.6.1 in four node kinds it copied through by id: a type, a pattern, an attribute, a generic
> parameter. **The fix: THE CLONE IS TOTAL.** `clone_type` (shared when the type holds no expression — nothing
> writes onto a type node and none can then differ — rebuilt when it does, the array size and the `comptime`
> argument through `clone_expr`), `clone_pat` (every pattern its own node: the checker records a destructure's
> binding types ON the pattern and the resolver keys the binding's symbol by it), `clone_attr`, `clone_generic`,
> `clone_param` (a parameter, a `for` binding, a variadic tail and a field: every declaration node is cloned — one
> node under two instantiations was two symbols with one origin), `clone_variant`; a cast's target and a `dyn`
> cast's window; a function's return type, an impl's target and trait, a trait's supertraits, a `Rules` subject, a
> spliced field's type. What the clone clones is BLANKED in a detached original (`blank_type`, `blank_pat`,
> `blank_attr`), where step 3 had to walk a shared node in place; the audit's exception for a template expression
> the walk had reached is removed with the sharing that needed it, and `#caller(name)` in a body's type or pattern is
> legal (it was MACRO-008). **WHAT A PARAMETER CANNOT BE is refused at the macro's declaration, `NITPICK-MACRO-011`
> (new)**, read off the body's five id ranges (the declaration's window records them): the name of a TYPE (`T:x`,
> `T{ … }`, `x =>! T`, a destructure's enum or struct — `macro:mk = (T) { T:x = T{ v: 7i32 }; … }` beside a module
> `struct:T` built the module's type whatever was passed, exit 7), and a name the body DECLARES AND WRITES AS AN
> EXPRESSION (a local, a `for` binding, a pattern's binding, a function's parameter, a generic parameter, an emitted
> declaration — `macro:bump = (N) { int32:N = 1i32; … N + 1i32 … }` read the ARGUMENT at every later `N`, exit 41
> for `#bump(40i32)`); one report per parameter, at its first such spelling; a field and a variant of the spelling
> are left alone, and so is a name the body declares and never uses — MACRO_REFERENCE §10's open question, "may a
> parameter name an emitted declaration?" (`func:N` is literally called `N`), which stays open as **S-123**: the
> step's first form refused every declaration of a parameter's name, and the sweep found the library listener's
> `mc0388` (that very example, expected to run) refused by it. A bare name in a type-argument list is a type's name (D-064 §2), so a compile-time value is written
> `simd<int32, (N)>`, and the sentence says so. A range passed where a whole value pattern stands is MACRO-005, once.
> **Found reading the clone, fixed with it:** a `pub Rules` block a macro emitted LOST ITS `pub` (the clone wrote the
> bare clause count over the flags that ride that slot's high half, DEF-93: `use m.{R};` of it was RESOLVE-003).
> **Probed before any of it was written (the hand-off's two questions):** two instantiations sharing one
> destructuring pattern over `Opt<string>` and `Opt<Two>`, consuming and lending, in either order, and one `for`
> binding shared by two NESTED instantiations — each answered rightly on `9efe218`, the heap numbers the hand-written
> twin's; they are cloned so that this is a fact about the tree and not about the order of a walk. Tests:
> `tests/backend/programs/macro_param_positions.npk` (fifteen positions, each through a value, each body instantiated
> twice with different arguments beside a module binding of the parameter's spelling; exit 0 at both legs; refused by
> `9efe218`), `tests/expansion/rejection/param_misplaced.npk` (MACRO-011 ×16, eight controls), `shared_position.npk`
> re-read (its MACRO-008 case a control), `pattern_range_body.npk` (+ the range argument), unit cases v12…v17 in
> `tests/frontend/expansion_output.npk` (two instantiations own DIFFERENT array types and the SAME `int32`). PART B —
> DEF-183, whose scope an ARGUMENT resolves in — waits for S-120.

> **DEF-190 — OPEN (2026-09-30; owner: the compiler seat; found by the seat's review of step 3's float scan against
> LEXICAL_REFERENCE §6.2). THE NUMERIC SCANS ACCEPT A `_` THE PRODUCTION DOES NOT:** `DecimalLiteral ::= [0-9]
> ([0-9_]* [0-9])?` ends a digit run in a digit; the integer scan has always accepted a run that ends in a separator
> or doubles one (`10_`, `1_i32`, `1_000_i32`, `1__0` -- measured on `5fbaf4a`), and step 3's float scan agrees with
> it (`1_.5f64`, `1.5_f64`, `1__0.5f64`). No value changes -- a separator is ignored -- but a numeral that ends in one
> reads as an unfinished numeral (`30_` where `30_000` was meant compiles as 30), which a safety-critical language
> should not accept; and once the freeze is permanent a scan can be widened and never tightened. **Recommended:**
> both scans are held to the production (LEX-003 for an integer run, LEX-009 for a float's), a unit case each
> (D-085: a lexer refusal is never a rejection file). Exposure measured 2026-09-30: no file of the tree, `lib/`,
> nitpick-libs or nitpick-apps writes one (3,727 `.npk` files scanned). With step 3d. *[2026-10-07:
> the library listener's F-036 (b), O-N36, is this shape -- `10_i32` accepted against the DecimalLiteral
> production, measured at `5fbaf4a` and `93bcb66`; registered here, not as a new number.]*

> **DEF-191 — FIXED at 1.6.1e step 3a (2026-10-01; found by `nitpick-compiler_24` writing DEF-189's test, the
> turbofish case; a silent wrong answer on every compiler since 1.0.2b). A GENERIC CALL REACHED THE FIRST INSTANCE
> RECORDED WHEREVER TWO INSTANCES SHARE A SIGNATURE.** `fninst_for_call` re-derived a call's instance from the
> callee's SUBSTITUTED function type. A type parameter that appears in no parameter type and not in the return type
> leaves every instance with one signature: `func:bytes_of<T> = int64() never fails { pass #size_of<T>(); };` — both
> `bytes_of<int32>` and `bytes_of<int64>` were emitted, and `raw bytes_of::<int64>()` CALLED `bytes_of<int32>`:
> probes `gi1` (exit 44 for 48), `tf1` (two array instances: exit 2), `gi2` (a value parameter beside it, 88 for
> 81), `gi6` (inside a generic body, 55 for 60), `gi7` (awaited: 111 for 182); where a template-shaped instance was
> recorded first, the call named `bytes_of<A>`, a symbol that does not exist (`llc` refused the module). In `wild`
> code — `alloc(n * #size_of<T>())` — the wrong body is the wrong SIZE. Fixed: the checker records each generic call's
> type-argument window on the call (`ExprTypes.callee_targs`, at `type_generic_call`, the one site that types a
> generic call), and the emitter matches the instance by the window's CONTENTS (`fninst_args_are`) — the key
> `fninst_record` dedups by, D-108 as amended; a call with no record is matched by signature only where the
> signature names one instance, and names nothing otherwise (loud, never the first match). Exposure, measured: no
> generic function of `src/`, `lib/`, `npkg/`, `tools/`, nitpick-libs or nitpick-apps has a type parameter that
> appears only in its body (two of the tree's program tests hold one — `macro_walk_positions.npk`,
> `late_instance.npk` — each at a single instance). `tests/backend/programs/
> generic_instance_args.npk` (exit 0 at both legs; `llc` refuses `9efe218`'s emission of it).

> **DEF-192 — FIXED 2026-10-01 (landing 86; registered OPEN the same day at step 3a; owner: the compiler seat; found by `nitpick-compiler_24` asking where a body's TYPE
> names resolve). A TYPE NAME IN A MACRO BODY IS RESOLVED IN THE INVOKING FUNCTION'S SCOPE — THE CALLER'S GENERIC
> PARAMETER CAPTURES IT, IN SILENCE.** `struct:T = { int32:v; }; macro:sz = () { #size_of<T>(); }; func:g<T> =
> int64(move T:x) never fails { int64:r = #sz(); pass r; };` — `raw g(1i64)` answers 8, the caller's `T`, where
> D-057 says the body's `T` is the module's struct: 4 (probe `q3a`, both legs, `9efe218` and step 3a alike). The
> resolver never enters a type: the type resolver binds a type's name itself, and it consults the GENERIC
> parameters of the function (and the impl) it is checking before any scope -- for an expression- or
> statement-position expansion that is the function the invocation stands in. So what captures is exactly a
> generic parameter of the invoking function or impl; a caller's LOCAL or PARAMETER does not (a name inside a type
> is otherwise looked up from the module: probes `q3b`, `q3c`, an array size `int32[N]` in a body reads the module's
> `N` beside a caller's local or parameter `N`). It bears on S-120 from the other side: a type inside an ARGUMENT
> (`#m(#size_of<T>())` in `func:g<T>`) is the caller's text and its `T` IS the caller's. **Recommended:** a type
> written in an expression or statement BODY does not see the invoking function's generic parameters (the parser
> marks the body's type nodes, the clone carries the mark, the type resolver skips the enclosing generics for a
> marked node); an argument's types keep the invocation's; a declarations body keeps the landing scope (an emitted
> function's own generics must stay visible to its own types). ITS OWN LANDING, with DEF-183 or directly after it;
> an advance notice.
>
> **DEF-192, WIDENED 2026-10-01 (`nitpick-compiler_24`, probing for its fix):** the capture is by ANY generic
> parameter the type resolver asks before the scope — the invoking function's (`q3a`) and the enclosing `impl`'s
> (`q3d`: `#size_of<T>()` in a body, invoked in a method of `impl:<T>:Box<T>`, measured the impl's `T`: 8 for 4,
> both legs). A trait's associated type does not capture (`q3e`: 4), and a file-level macro cannot be invoked inside
> an inline module at all (`q4a`: MACRO-007), so no SCOPE captures. Where no module type of the spelling exists the
> capture is what makes a body compile: a field `V:item;` spliced into `struct:Box<V>`, `#size_of<U>()` inside
> `func:g<U>`. D-057's rule refuses those (the name is not in the scope the macro was written in); that a body then
> has NO way to name the landing declaration's type parameter is S-124's question, raised with the fix (landing 86).
>
> **FIXED (landing 86):** `Ast.macro_spans` records where every macro declaration stands in its file (the parser,
> `ast_note_macro`); `ast_macro_at(ast, span)` answers which declaration a node was written in — a clone keeps its
> template's span, so the body's own type nodes (shared by every instantiation where they hold no expression) and
> the copies an instantiation makes answer alike; `macro_may_bind` (type_resolve.npk) is the rule: a type's name
> written in a macro body binds a generic parameter (`generic_bind_in`) or a trait's associated type
> (`resolve_user_named`) only where that is declared in the SAME body. A name written outside every macro body — an
> argument's included — binds as it did. `q3a` and `q3d` answer 4; `h1` (what must still bind: an emitted generic
> function's, struct's and impl's own parameters, an argument naming the caller's parameter, an argument written
> inside another body, and a body invoked inside another body's emitted generic function, which reads the module's
> type) exits 0 where `c6d671d` exits 6. Where the defining scope holds no type of the spelling the body is
> `NITPICK-TYPE-001`, "in the scope this macro was written in" — three shapes that compiled by the capture
> (`tests/types/rejection/macro_type_hygiene.npk`). Measured (the sweep over 3,601 files): 1 site vanished and 6
> appeared, every one in the new rejection test (its REACH-003 under the old checker; its three TYPE-001 with their
> three notes) — no file of nitpick-libs or nitpick-apps moves, and none of the tree's own: nothing anywhere
> compiled by the capture; the emission of 536 of 538 programs of the tree byte-identical and of all 1,562 library
> and application programs that compile. The consequence is S-124.

> **DEF-193 — FIXED 2026-10-01 (landing 85, its own, after step 3a; registered the same day; owner: the compiler
> seat; found by `nitpick-compiler_24` writing DEF-189's test, the
> variant-value case). AN ENUM VARIANT'S EXPLICIT VALUE IS HONOURED ONLY AS A BARE INTEGER LITERAL, AND NOTHING
> CHECKS IT.** `variant_tag_of`: "the declared integer literal where one is given, the position otherwise".
> `enum:B = { X = 7i32 + 1i32; Y = 2i32; };` — `(B.X =>! int32)` is 0, the position, in silence (probe `ev3`: exit
> 3); a `fixed` name and a negated literal likewise; `enum:F = { A = 3i32; B = 3i32; }` compiles and `F.A == F.B`
> is true (`ev5`); `enum:E = { A; B = 0i32; }` gives both the tag 0, and a `pick` naming both is refused by `llc`
> ("duplicate case value in switch", `ev4`); a value past `int32` is truncated. `check_decl` has no arm for an enum
> — D-085's shape: a construct parsed and resolved, and the checker's silence about it invisible. Exposure: 455
> explicit values in the tree, the libraries and the applications, every one a bare literal (two are macro
> invocations that expand to one). **Recommended (S-122, the user's):** at the enum's declaration — a value that is
> not a bare integer literal is refused by name until the folder's value reaches the emitter; two variants of one
> enum sharing a tag are refused at the second; a value outside `int32` is refused. ITS OWN LANDING, next after 3a:
> a program the compiler accepts and answers wrongly.
>
> **FIXED (landing 85):** `check_enum_values` (type_stmt.npk), called from a new `DeclEnumDecl` arm of `check_decl`,
> refuses at the value, `NITPICK-TYPE-093` (new): a value that is not a bare integer literal (an expression, a
> `fixed` name, a negated literal, a `char` or `bool` literal); a value outside `[0, 2^31 − 1]`; and a tag another
> variant already has — at the LATER variant, naming the earlier, and saying which of the two has no explicit value
> and so is its position. A refused value takes no part in the duplicate check (D-240: `{ A = 4294967296i64; B =
> 0i32; }` is one report, though the truncated `A` was 0 too). Reached wherever an enum is declared — an inline
> module, a generic enum, an enum a macro emits (checked after expansion, so a value that is a macro invocation or a
> parameter is judged as the literal it becomes). Probed around: a literal past the 64-bit envelope never reaches
> the check (`NITPICK-LEX-004`, so the node's payload is always the value); `5`, `6u8` and `0FFhex` are literals and
> accepted (the suffix is not read); a variant with a payload takes no value (the grammar's).
> `tests/types/rejection/enum_values.npk`: TYPE-093 ×12 and seven controls; the positive controls are
> `enums_pick.npk` (`Late = 9i32` among unvalued variants, its tag read back) and `macro_param_positions.npk` (a
> value from a macro argument). Measured: the sweep over 3,598 files moves the new test's own sites and
> nothing else; the emission of all 536 programs byte-identical. S-122 stays OPEN for what a value MAY
> be.

> **DEF-194 — OPEN (2026-10-01; owner: the compiler seat; observed by `nitpick-compiler_24`'s probes; with DEF-187,
> step 3d). A `comptime` VALUE PARAMETER IS ACCEPTED WHERE NOTHING LOWERS IT.** `func:staged<comptime int32:LEVEL> =
> int32() never fails { pass LEVEL; };` passes the frontend and is EMIT-002 at the read (`gi5`; DEF-187's last
> sentence, with its probe); `struct:Buf<comptime int32:N> = { int32[N]:a; };` is TYPE-004, "an array size must be
> a constant expression" — a value parameter cannot size a field of its own struct (`cn3`); a generic `comptime
> func:` cannot be folded at all ("there is no type named `T`", `gi8`). Each is a refusal, none silent. To be read
> against D-064 §2 and D-109 (what a value parameter is FOR beyond the compiler-known `Mutex`/`simd` positions)
> before step 3d plans it.

> **DEF-195 — FIXED at 1.6.1e step 3a (2026-10-01; found by `nitpick-compiler_24` asking what DEF-191's fix did to
> the verified build; A SOUNDNESS HOLE OF THE VERIFICATION LEG, on every compiler since 1.5.3). A GENERIC `pure never
> fails` CALLEE WAS ONE UNINTERPRETED FUNCTION FOR EVERY INSTANCE.** `uf_value` named the function per DECLARATION
> (`|uf.<name>.<decl>|`), and an argument is an `Int` whatever its width, so over `func:width<T> = int64(T:x) never
> fails pure { pass #size_of<T>(); };` the calls `raw width(1i8)` and `raw width(1i64)` were ONE term, `(|uf.width|
> 1)`: z3 knew two calls equal that return 1 and 8, discharged the `div-zero` row of `100i64 / ((b - a) - 7i64)`,
> and the verified build elided the guard — it divided by zero, unguarded, where the plain build traps `DivByZero`
> (probe `vg2` on `9efe218`: the plain build exits 97, all four rows of `main` `unsat`). DEF-191's fix alone would
> have widened it (its two bodies were one before, and the model accidentally matched the miscompiled program):
> probe `vg1`. Fixed: the symbol carries the INSTANCE — the call's recorded type arguments (`.t<id>`), and on a
> method call the receiver's type (`.r<id>`: a trait's default body and a family impl are one declaration for every
> `Self`); a generic callee with no recorded arguments has no term. Exposure, measured: no generic function is
> declared `pure` in `src/`, `lib/`, `npkg/`, `tools/`, the prelude or `tests/verify/`.
> `tests/verify/generic_uf_instance.npk` (`div-zero open 2`; the verified build exits 97 like the plain one; on
> `9efe218` both rows are `discharged`).

> **DEF-196 — FIXED 2026-10-01 (landing 86; registered OPEN the same day at landing 85; owner: the compiler seat; found by `nitpick-compiler_24` reading `fold_ident` for
> DEF-192's fix; FIXED in landing 86, prepared). THE COMPILE-TIME EVALUATOR KEEPS A CALL'S VARIABLES BY NAME.**
> `FoldEnv` is one flat list of (name, value) per `comptime func:` call, on a premise its own comment states —
> "there is no nested scope in a body this evaluator accepts" — that has been false since the evaluator learned
> `if`, `while` and the counted loops. Each a silent wrong answer of compile-time evaluation, measured on `9efe218`
> and `c6d671d` alike, and each disagreeing with the same body run at run time (the emitter resolves by symbol since
> DEF-144): (1) a local declared in a nested block OVERWRITES the outer local of its spelling — `int32:x = 1i32; if
> (x == 1i32) { int32:x = 2i32; discard(x); } pass x + 10i32;` is 12 at compile time and 11 at run time (`q5f`: exit
> 187 for 87); over a PARAMETER the same (`c1`: 50 for 3); (2) a macro body's free name, which the resolver binds
> where the macro was written (D-057), READS a local of the function it is invoked in (`q5a`: `{ X + N; }` beside
> `fixed int32:N = 100i32;`, invoked where a local `N` is 1 — 6 for 105); (3) a name written inside a TYPE reads a
> local too, where the checker reads the module's `fixed` binding (D-222: a local is not an array size):
> `#size_of<int32[LEN]>()` with a local `LEN = 2` beside the module's `LEN = 3` evaluates to 8 where the checker's
> type is 12 bytes (`q5d`; through a macro body, `q5c`). The fix: an entry is keyed by the node that declares it —
> the symbol's (origin kind, origin) — `fold_ident` asks only for an identifier that HAS a symbol, an assignment
> writes its target's symbol, and the by-name path is gone.
>
> **FIXED (landing 86):** `FoldEnv` is `{ kinds, origins, vals }`, an entry keyed by the node that declares it
> (`SYM_DECL` and the parameter's declaration, `SYM_STMT` and the local's statement); `foldenv_set`/`foldenv_get`
> take the pair; `fold_ident` asks only for an identifier that has a symbol (`e.a`), and an identifier with none — a
> name inside a type — is never a local; the assignment arm writes its target's symbol. `q5f` 87 (11), `c1` 0, `q5a`
> 105, `q5c` and `q5d` 12; `tests/backend/programs/comptime_scopes.npk` (twelve cases, the run-time twins beside the
> compile-time ones) exits 0 at both legs where `c6d671d` exits 1. Exposure, measured: no program of the tree (538)
> or of the libraries and applications (1,562 that compile) changes its emission — no existing `comptime func:`
> shadowed a binding or named a local after a module constant a body or a type read.

> **DEF-197 — FIXED 2026-10-01 (landing 87; found by `nitpick-compiler_25` probing DEF-179 before building it). A
> MODULE-LEVEL CONSTANT OF A TWISTED FAMILY WAS WRITTEN AS THE FOLDER'S PLAIN-INTEGER READING OF IT — THREE SILENT
> WRONG CONSTANTS.** The folder typed a literal only under an integer suffix; every other suffixed literal was an
> untyped plain integer ("no width was written"), plain arithmetic folded over it, and `emit_global_const_into`
> wrote `int_to_string(cv.num)` whatever the binding's type. Measured on `9efe218` and `f13914c`, both legs: (1)
> `fixed tfp64:Q2 = 2tfp64;` is `constant i64 2` — the Q value of 2.0 is 8589934592 — so `Q2 == 2tfp64` is false
> (exit 1); (2) `fixed tbb8:T = 100tbb8 + 100tbb8;` is `i8 200`, −56, where the run time's `+` saturates to ERR, and
> `fixed tfp64:Q = 2tfp64 + 1tfp64;` is `i64 3`; (3) `fixed tryte:TR = 29524 + 1;` is `i16 29525`, no tryte at all,
> where the run time answers ERR. A function body computed each correctly: the emitter's constant shortcut asks for
> a plain integer type (`fold_node_constant`), the global's renderer asked nothing. With them: a balanced-base
> literal under a `tfp` suffix lost its sign everywhere (`0Tttfp64`, the literal −1, was 0 in a body too:
> `tfp_q_decimal` reads no sign), and `int32[2tfp64]` was an array of two. EXPOSURE: none — no `fixed` binding of a
> `tbb`, `tfp`, `dim256`, ternary or float type exists in 40,928 `.npk` files (the tree, the libraries, the
> applications, the fuzzer's corpus). **FIXED:** `fold_suffixed_literal` gives a literal its suffix's family;
> `fold_computes` holds every operator arm to the plain integers and a flag family; `const_init_verdict` (the
> checker's one gate) reads the family off the recorded type, so an unsuffixed ternary literal under arithmetic is
> caught too; `const_scalar_text` renders by type; `tfp_q_of_int`/`tfp_q_signed` carry the sign. The three shapes
> are NITPICK-TYPE-035 "written, not computed" (`tests/types/rejection/module_const_carried.npk`); the written forms
> are right (`module_constants.npk`, cases 21, 30–39, 62).
>
> **DEF-198 — FIXED 2026-10-01 (landing 87; found by the same probes). A FOLDED VALUE TOOK ITS SLOT'S WIDTH
> UNCHECKED, AND A FOLDED NAME LOST ITS BINDING'S TYPE.** Both silent, in function bodies, on `f13914c`: `int8:a =
> comptime(100 + 100);` stored `i8 200` (the sum without `comptime` is NITPICK-TYPE-076), and `fixed int64:BIG =
> 5000000000; … int32:b = comptime(BIG);` stored `i32 5000000000` — 705032704 — where the plain read `int32:b =
> BIG;` is NITPICK-TYPE-007. `type_comptime` gave an untyped value the slot's type with no range question;
> `fold_ident` returned the initialiser's value with the literal's own type, which for an unsuffixed literal is
> none. **FIXED:** `fold_stamp` gives a folded name its binding's declared type (bounded against a declared type
> that names its own binding: `fixed int32[K]:K = 5;`), and an untyped `comptime` value must fit the integer slot it
> lands in (NITPICK-TYPE-031). `tests/types/rejection/comptime_width.npk`.
>
> **DEF-199 — FIXED 2026-10-01 (landing 87). THE INTEGER-FORM FLOAT LITERAL: `3f64` REACHED `llc` AS `double 3`, AND
> `100000000f32` TRAPPED THE CHECKER.** The grammar gives an integer body any `TypeSuffix`; the checker typed `3f64`
> a `flt64` (as it types `5tfp64` a `tfp64`) and `emit_int` had no float arm, so no program with one ever linked;
> and the `flt32` digit count read the INTEGER payload as an intern index — the digits of an unrelated string, or
> outside the table (exit 3, no message). No file of the tree, the libraries or the applications writes one.
> **FIXED:** `emit_int` lowers it through `float_literal_body` (`double 3.0`; a `flt32` through the same double),
> and the digit count reads the payload's own digits. `module_constants.npk` cases 60, 61, 63.
>
> **DEF-200 — FIXED 2026-10-01 (landing 87). A TOP-LEVEL SENTINEL INITIALISER WAS REFUSED BY A SECOND GATE.** `fixed
> int32?:O = NIL;` and `fixed tbb32:E = ERR;` passed the checker (D-165 lists a sentinel) and were NITPICK-TYPE-004
> from the BINDINGS analysis, which re-folded every module initialiser as a belt, admitted an aggregate literal by
> shape and never a sentinel, and said so in a sentence that still named `const` (the third copy DEF-163 missed).
> Inside a struct literal the same sentinel compiled. **FIXED:** the belt is gone — the checker's
> `const_init_verdict` is the one gate, and the emitter's renderer fails closed behind it. `module_constants.npk`
> cases 33, 34.
>
> **DEF-201 — FIXED 2026-10-01 (landing 87). THREE SITES NARROWED A FOLDED VALUE TO `int32` UNCHECKED.**
> `int8[4294967298]:a = [1i8, 2i8];` compiled as an array of two (`resolve_fixed_array`: `count =>! int32`); a lane
> count of 4294967300 was a four-lane vector; a channel's constant argument likewise, and any folded KIND passed
> there (a string constant read as 0). **FIXED:** each asks an integer constant of a plain type and its range before
> narrowing (NITPICK-TYPE-004 for the array, the sites' own codes for the other two). `comptime_width.npk`.
>
> **DEF-202 — OPEN (2026-10-01; owner: the compiler seat; found by landing 87's probes). A REFERENCE TO AN AGGREGATE
> MODULE BINDING IS NOT A CONSTANT.** D-165 lists "a reference to another module-level binding"; `fixed Pt:Q = P;`
> and `fixed int32[3]:B = A;` are NITPICK-TYPE-035, because the folder holds no struct or array value and the gate
> and the renderer ask only the folder for a name (probe `mj1`; a reference to a sentinel-initialised binding
> likewise). Refused, not wrong. **Recommended:** the gate and the renderer follow an identifier that names a module
> binding to that binding's initialiser (in ITS scope), for every type; step 3d.
>
> **DEF-203 — FIXED 2026-10-01 (landing 88, `nitpick-compiler_26`; found by `nitpick-compiler_25` reading how a `flt32`
> module constant could be spelled). A `flt32` LITERAL IS NOT ALWAYS THE NEAREST FLOAT TO THE NUMBER WRITTEN — A
> SILENT ONE-ULP WRONG CONSTANT, AND D-143'S REASON FOR THE 15-DIGIT RULE IS FALSE.** A `flt32` literal lowers as
> `fptrunc double <text> to float`: two roundings. D-143 says they equal one rounding "exactly when the decimal has
> ≤ 15 significant digits". Measured with exact rationals: `9.51125303839185e-19f32` is `0x218C5C80` through the
> double and `0x218C5C7F` rounded once (probe `mr1`: the compiled program reads back `0x218C5C80`, at both legs); 88
> such fifteen-digit literals in 4,000 sampled float midpoints (the decimal sits within half a double's ulp of the
> midpoint, the double is the midpoint, the tie goes to even). Any one literal is hit with probability near 2⁻³⁰.
> The solver agrees with the emitted program today — `fp_literal` (smt_encode.npk) mirrors the two roundings on
> purpose, to a `Float64` and then narrowed — so the fix moves the emitter AND the encoder together, or they part.
> One consequence beyond the ulp: a `flt32` MODULE constant has no spelling at all without the bits (LLVM takes a
> `float` constant only where it is exact, and has no constant `fptrunc`), which is DEF-179's remaining rung
> (NITPICK-TYPE-035 by name since landing 87). **Recommended, its own landing next:** the compiler's own
> decimal→`flt32` conversion (exact, in the wide integers `tfp_q_decimal` already uses; known-answer vectors
> generated as D-193's were), ONE rounding, emitted as the exact constant for a literal in a body and a module
> constant alike; the 15-digit rule then protects nothing and its fate is the user's.

>
> *[The library seat on advance notice F29, 2026-10-01.]* No library or application file uses `flt32`. Three
> programs of the fuzzer's corpus pin the two-rounding ROUTE and quote the references that state it
> (VERIFICATION_REFERENCE §7c's "a `flt32` literal rounded twice, as the emitter's double-then-`fptrunc` road
> does"; TYPE_REFERENCE §1.4's "through a correctly-rounded double"): `vf0983` and `vf1149` (a twenty-digit
> literal, NITPICK-TYPE-030 today) and `ty0190b`. Those sentences change with the fix, and the notice for it
> names the three programs.
>
> **FIXED (landing 88).** MEASURED FIRST, on `789ffdc`: every road a `flt32` constant takes gave the two-rounding
> float (a body literal, a negated one, an unsuffixed fraction in a `flt32` slot, `comptime(…)`, a struct field, an
> array element, a `simd` lane and splat, a `complex` component: probes `fa1`…`fa7`; the integer form is exact
> through a double), and D-143's OTHER claim was measured for the first time — LLVM's decimal→double parse is
> correctly rounded on the pinned toolchain for every measured text whose exponent is within 24,000 (8,890
> adversarial texts, none different), and WRONG past that (DEF-209, found by this landing's own committed
> measurement after its source was final) — so a `flt64` literal's road is unchanged HERE and changes next. THE FIX:
> `float_round(text, width)` (`numeric.npk`), generic in the format (24 bits/127, 53 bits/1023): the decimal's
> digits are halved (the integer part's bits, least significant first) and doubled (the fraction's, most significant
> first) as digit runs, a window of p+1 bits and a sticky bit kept, rounded once to nearest with ties to even, the
> encoding absorbing the carry; it reports and does not decide the two answers DEF-205 asks about (`inf`, `zero`).
> It agrees with exact rationals on 42,501 adversarial texts at both widths before it was wired (0 different), and
> `tests/frontend/float_round.npk` holds 2,072 of them. The emitter has ONE writer of a float constant
> (`float_const_text`, behind `emit_float_const` and `const_scalar_text`): a `flt32` is `float 0x…`, the bits of the
> double holding that float; there were three writers, each with its own `fptrunc`. `fp_literal` is `((_ to_fp 8 24)
> RNE <the exact real>)`, changed in the same commit: `prove((9.51125303839185e-19f32 => flt64) ==
> 9.511252521403968e-19f64)` was REFUTED on `789ffdc` (the solver held the upper neighbour, as the program did) and
> is discharged. `const_init_verdict`'s third refusal is gone: a `flt32` module constant compiles in every written
> form. Tests: `float_round.npk` (unit), `float_literal_kat.npk` (505 `flt32` and 500 `flt64` literals read back as
> bits at both legs; `789ffdc` answers 44 of the `flt32` ones wrongly), `flt32_literal.npk` (every road, the four
> counterexamples it is built from, module constants), `tests/verify/flt32_round.npk`. One consequence for the
> verifier: `flt32` rows are eligible for the Real-interval tier for the first time (D-281's dated note).
>
> **DEF-204 — FIXED 2026-10-01 (landing 87; found by `nitpick-compiler_25` asking what else read a fixed-point
> literal by its payload). A `pick` ARM OVER A FIXED-POINT SELECTOR NEVER MATCHED — A SILENT WRONG BRANCH.**
> `pattern_const` wrote a value pattern's integer payload whatever the selector's type, and a range's bounds
> likewise: over `tfp64:q = 2tfp64`, `pick (q) { (2tfp64) { … } … }` compared the selector's Q value (8589934592)
> with 2 and took the wildcard (probe `mn2`: exit 1 at both legs, `9efe218` and `f13914c`); `(1tfp64..2tfp64)` over
> 1.5 likewise (`mn1`); a fraction `(2.5tfp64)` or a negated pattern was EMIT-002. The encoder read the same
> patterns as their Q values (`enc_pattern_value`), so a verified build reasoned about an arm the program never
> entered. A `tbb` selector was right (its value is its integer). No `pick` over a fixed-point selector with a value
> pattern exists in the tree's programs, the libraries or the applications (the emission comparisons move nothing).
> **FIXED:** `pat_q_text` (ir_stmt.npk) — the folder carries the pattern's literal and `tfp_q_signed`/`tfp_q_of_int`
> spell it, for a value pattern and each range bound. `tests/backend/programs/pick_fixed_point.npk` (fourteen cases,
> exit 0 at both legs). DEF-178 (a pattern's literal is never TYPED against its selector) stays open: `(2i32)` over
> a `tfp64` selector is still accepted, and is read as the number 2.
>
> **DEF-205 — FIXED 2026-10-01 (landing 91, `nitpick-compiler_27`, under D-346; found by `nitpick-compiler_25` probing what DEF-203's
> conversion must decide). A FLOAT LITERAL PAST ITS TYPE'S RANGE IS INFINITY, AND ONE BELOW ITS SMALLEST VALUE IS
> ZERO — IN SILENCE.** Probe `ms1`, both legs: `flt32:a = 1.0e39f32;` is +inf (`fptrunc double 1.0e39 to float`),
> `flt64:b = 1.0e999f64;` is +inf (LLVM reads `double 1.0e999` as infinity), `flt32:c = 1.0e-60f32;` is 0.0. D-148
> says a numeric literal "must fit its type … verified at the literal (NITPICK-TYPE-031)", and its table leaves
> floats to D-143, which has no range rule: the number in the program is not the number written — an epsilon that is
> zero, a bound that is infinite. **Recommended (S-126, the user's):** NITPICK-TYPE-031 at a float literal whose
> value rounds to infinity or — being nonzero — to zero; a subnormal result is rounding and stays. It needs the
> exact conversion DEF-203 builds (and its `flt64` twin for the range test), so it lands with or after that.
>
> *[Landing 88.]* The conversion exists at both widths and REPORTS both answers (`FloatBits.inf`, `FloatBits.zero`;
> held by `float_round.npk`'s vectors: a text past the range, one below half the smallest subnormal, the exact
> half-way cases on each side). Nothing reads the flags: a literal that does not fit is still infinity or zero.
> The refusal S-126 (a) recommends is one test of each flag in `float_text_ok`, for every spelling.
>
> *[Landing 89.]* A `flt64` literal's constant is the conversion's too (DEF-209): `1.0e999f64` is written as
> infinity's own bits and a nonzero literal below the range as zero's — the same two doubles LLVM's parser gave,
> in silence as before. The flags are still unread.
>
> **FIXED (landing 91, under D-346).** MEASURED FIRST on `eaf6b08`, at both legs: a body literal at both widths in
> both directions, an unsuffixed one in a slot, `comptime(…)`, and two module constants — eight roads, eight silent
> infinities and zeros (probe `fz1`: exit 255). THE FIX: `float_fit_refusal` (`numeric.npk`) — "" where the number
> written, rounded once to the type, is finite and is zero only where zero was written, else the sentence — asked by
> `float_text_ok` of every literal the checker types (NITPICK-TYPE-031 at the literal), and the 15-digit count
> deleted there. PROBED AFTER IT LOOKED COMPLETE, every position a float literal can sit in: a struct field, an
> array element, a `simd` lane, a `complex` component, an operand of `+` and of `<`, an argument, under a sign, a
> cast's operand, a rule's and a contract's bound, a `comptime func:`'s body, a module binding — one report each —
> and A FOURTH ROAD stood open: a literal under a `comptime(…)` operand is read by the compile-time evaluator alone
> (`type_comptime` folds the operand and never types it), so `comptime(1.0e999f64)`, `comptime(-1.0e39f32)`, a
> parenthesised or nested one and a literal handed to a `comptime func:` inside one were still infinity.
> `fold_suffixed_literal` asks the same sentence outside a call (inside one the literal sits in a function's body,
> which the checker types; `comptime(M)` and `comptime(raw f())` over a refused literal add nothing: one mistake,
> one report). AND THE EMITTER HOLDS THE PROMISE: `float_const_text` has no constant for a literal with either flag,
> so a text that reached it by a road nobody asked would be NITPICK-EMIT-002, never a silent infinity. A `pick`
> pattern over a float selector is DEF-178's (EMIT-002 before and after). Tests:
> `tests/types/rejection/float_fit.npk` (eighteen sites, eleven controls at the format's own edges),
> `float_rules.npk` (its three digit sites are controls), `float_literal_kat.npk` (535 `flt32` literals, to 25
> digits and exact midpoints written out, and 504 `flt64`).
>
> *[DEF-206's fourth road, closed by the same landing.]* The suffixed spelling inside `comptime(…)` — `flt32:x =
> comptime(1.0000000000000001f32);` — was accepted on every compiler from landing 88 (probe `fq2`), past the
> 15-digit rule DEF-206 had put on three roads: the evaluator read the literal and nobody asked. The rule it skipped
> is lifted (D-346); the road is the one DEF-205's fix closes for the rule that stands.
>
> **DEF-206 — FIXED 2026-10-01 (landing 88; found probing around DEF-203, as `nitpick-compiler_25` predicted from
> reading `type_numeric_literal`). D-143'S FLOAT-LITERAL RULES WERE ASKED OF THE SUFFIXED SPELLING ONLY.** An
> unsuffixed fraction takes the width of the float slot it sits in (D-092) through `lit_ranged`, which holds no
> float rule: `flt128:x = 1.0;` passed the checker and died in the emitter (NITPICK-EMIT-002, the compiler
> confessing a defect about a program's mistake) where `1.0f128` is NITPICK-TYPE-030, and `flt32:x =
> 0.1234567890123456789;` was ACCEPTED where the same literal with its suffix is refused — and, before DEF-203's
> fix, compiled through the double with nineteen digits, the case the rule was written against
> (`1.0000000596046448309` in a `flt32` slot was 1.0 where the nearest float is the next one up). A THIRD ROAD to a
> width had the same hole: `comptime(…)` of an unsuffixed fraction takes the float slot's (`type_comptime`), so
> `flt128:w = comptime(1.0);` died in the emitter too. **FIXED:** one helper asked of the TEXT, `float_text_ok`
> (type_expr.npk; `float_literal_ok` for a literal node), by the suffix's branch, the contextual one and
> `type_comptime`; every position an unsuffixed fraction can sit in was probed (a module binding, a `comptime` at
> both levels, an argument, a returned value, a struct field, an array element, an operand of `+` and of `<`, a
> `simd` lane, under a sign): NITPICK-TYPE-030 at each. `tests/types/rejection/float_rules.npk` gains the unsuffixed
> and the `comptime` sites and two controls (fifteen digits in a `flt32` slot; any length in a `flt64` slot). The
> sweep: no file of the libraries, the applications or the fuzzer's corpus writes either shape (the four sites that
> appear are the test's own). The 15-digit half of this is a refusal the user may lift (S-126 (b)): it is applied to
> every spelling while the rule stands.
>
> **DEF-207 — FIXED 2026-10-01 (landing 88; found asking how `float_round`'s exponent bound could fail, then asking
> the same of the encoder). A FLOAT LITERAL WITH A LARGE EXPONENT STOPPED THE COMPILER IN A BUILD THAT WRITES
> OBLIGATIONS.** `decimal_to_real` (smt_encode.npk) summed the exponent's digits with no bound and spelled the power
> of ten out digit by digit: `flt64:x = 1.0e999999f64;` under any row did not finish (a million-digit numeral, built
> quadratically; killed at 60 s on `789ffdc`) and `1.0e99999999999999999999f64` trapped the compiler (IntOverflow,
> exit 3) — under `--obligations` only; the plain build of both is a silent infinity (DEF-205). **FIXED:** the
> exponent stops growing once nothing the text holds can bring the number back into range, and a number past 10^400
> or below 10^-400 is written as 10^400 or 10^-400 — the same `to_fp` value at both formats (infinity; zero), and to
> the Real twin a magnitude past every `MAX_NORMAL` or one inside `eta` of the float. A literal in range is spelled
> byte for byte as before (the row hashes of `tests/verify/` are compared, base against new: 128 of 129 files
> byte-identical, the 129th this landing's own `flt32_round.npk`). And the bound `float_round` itself was first
> written with — a fixed cap on the exponent, `tfp_q_decimal`'s — was wrong for one legal text, found before it
> landed: a fraction with a hundred thousand leading zeros and the exponent that undoes them (`float_round.npk`
> holds two such texts; the exponent is bounded by the text's own length now). `tfp_q_decimal` keeps its cap: there
> the same text is refused (NITPICK-TYPE-031), not misread — registered as DEF-208, OPEN, with its neighbours of
> step 3d.
>
> **DEF-208 — OPEN (2026-10-01; owner: the compiler seat; found with DEF-207). `tfp_q_decimal` REFUSES A FIXED-POINT
> LITERAL WHOSE EXPONENT IS PAST 350 WHATEVER ITS DIGITS.** `0.<400 zeros>1e401tfp64` is the number 1.0 and is
> NITPICK-TYPE-031 "does not fit" (the exponent is tested before the point is moved), and the exponent's digits stop
> at a fixed 100,000. Refused, never misread; no file writes such a literal. **Recommended:** the point's final
> place decided as `float_round` decides it (digits and exponent together, the exponent bounded by the text's
> length); step 3d.
>
> **DEF-209 — FIXED 2026-10-01 (landing 89, `nitpick-compiler_27`; found by landing 88's own committed measurement of LLVM's
> parser, `float_vectors.py --llvm`, on its two longest texts). A `flt64` LITERAL WHOSE EXPONENT IS PAST 24,000 IS
> NOT THE NUMBER WRITTEN — LLVM'S PARSER CAPS THE EXPONENT IT READS.** A `flt64` literal is emitted as `double
> <text>` and LLVM decides the bits. Measured on the pinned 20.1.2: `0.<N zeros>1e<N+1>` and `1<N zeros>.0e-<N>` are
> both the number 1.0, and both are the double 1.0 for every N tried up to 23,990; at N = 24,000 the first is 0.1
> (`0x3FB999999999999A`: the exponent 24001 read as 24000); at 24,010 they are 1e-10 and 1e10; from 30,000 on, zero
> and infinity. A silent wrong constant, at both legs, in a literal of some 24 KB — no file of the tree, the
> libraries, the applications or the fuzzer's corpus writes one — and before landing 88 a `flt32` literal had it
> too, through the same double. The solver's term for such a literal is the number written (z3's `to_fp` of the
> exact real), so a verified build reasons about a double the program does not hold. **Decided — the next landing,
> first in the queue (a wrong answer):** the emitter writes `double 0x…` from `float_round(text, 64)` through the
> one writer (`float_const_text`; `float_literal_body` goes), the 53-bit path already held to exact rationals by
> `float_round.npk`'s vectors, the two long texts among them. Every float program's emitted TEXT moves and no value
> does but these: the proof is the two emissions compared after reading every `double <decimal>` of the old one
> through the exact rationals (`fnorm.py`'s method, used in landing 88 for the `flt32` constants), with the
> programs' exits at both legs; `float_literal_kat.npk`'s `flt64` half then tests the compiler's conversion and
> gains the long texts, and `float_vectors.py --llvm` stays as the outside-gate measurement of LLVM's parser. It was
> kept out of landing 88 so that the conversion did not become the single authority for every `flt64` in the commit
> that first used it — decided before the cap was found, and kept after: one landing, one harness.
>
> **FIXED (landing 89).** MEASURED FIRST, on `88aa21d`, through the whole compiler at both legs (the registration
> had `llc` alone): `0.<N zeros>1e<N+1>f64` reads back as 1.0 at N = 23,990, as 0.1 at N = 24,000, as zero at N =
> 30,000 and at N = 100,005; `1<24001 zeros>.0e-24001f64` as 10.0; the 100 KB literal costs 0.45 s end to end, so
> nothing in the frontend is nonlinear in a literal's length. THE FIX: `float_const_text(text, neg, bits)` is one
> branch — `float_round(text, bits)`, a `flt32`'s float widened to the double holding it, the sign bit, sixteen hex
> digits — and `float_literal_body` (the reader that cut the suffix and handed the decimal to LLVM) is gone. ASKED
> HOW IT COULD FAIL: the two readers of a literal's text agree on every legal text (LEXICAL_REFERENCE §6.2's
> production; LEX-009 holds the tail); no emitter site reads a constant's TEXT (a negated literal is an `fneg` on
> the constant; the sign is put on text in the one writer alone); the backend holds no second writer of a program's
> decimal (what it writes beside `double` is its own exact bounds, `cast_bounds.npk` and one `0.0`, which stay); and
> the `flt64` twin of `flt32_literal.npk` — every road — holds 47 decimal constants under `88aa21d` and none under
> this compiler, the two emissions one artifact. THE PROOF THAT NO OTHER CONSTANT MOVED, over every differing pair
> of the emission comparisons (the tree's 34 of 544 programs, the libraries' and applications' 31 of 2,602): (B) the
> two texts are byte-identical once every floating-point constant on both sides is read as its bits, a decimal by
> exact rationals (`fractions.Fraction`, the generator's `ref`) — 65 of 65; (A) LLVM makes one artifact of the two,
> assembled under one module name: the `-O0` object, the `opt -O2` text and the `-O2` object byte-identical — 63 of
> 65, the 2 others being this landing's tests that hold a literal past the cap (there the old artifact is the wrong
> one); and every distinct decimal the old emissions held within the cap, 593 texts (the longest 115 characters, the
> largest exponent 324), through the pinned `llc` against the exact rationals: none read as another double (the 5
> texts past the cap, all in this landing's tests, are each read as another: the defect). THE CONVERSION ITSELF, now
> the one authority at both widths, against the exact rationals on 566,680 adversarial texts at both formats: no
> mismatch. Tests: `float_literal_kat.npk` gains four texts past the cap in its `flt64` family and a `roads`
> function (a module binding, a negated one, an aggregate's element, a literal with no suffix, `comptime(…)` and
> `comptime(-…)`, each with a text past the cap; `88aa21d` exits 26 and, by a probe that masks the roads, fails all
> six); `tests/verify/flt64_past_cap.npk` (two `prove(x == 1.0f64)` rows, discharged under both compilers — and the
> verified binary of `88aa21d` exits 1: it proved a sentence about a double its own binary did not hold);
> `float_round.npk` gains the two 24,001 texts at both widths (2,076 vectors). The encoder is untouched
> (`fp_literal` was already the exact real at the literal's format): 130 of 130 `tests/verify/` files give
> byte-identical rows under both compilers.
>
> **DEF-210 — OPEN (2026-10-01; owner: the compiler seat; found by `nitpick-compiler_27` measuring the library
> seat's documentation note on `#[derive(Copy)]`). A REFUSED `#[derive(Copy)]` OVER AN ARRAY SAYS `Clone`.**
> `dv_refusal` (derive_gen.npk) answers one sentence for `Clone` and `Copy` at an array whose elements are not
> scalars: "an array copies under `Clone` only when its elements are scalars; anything else needs its own clone
> rule" — under `#[derive(Copy)]` over `int64[2][2]` or `string[2]` the diagnostic (NITPICK-DERIVE-006, the right
> code at the right place) names the wrong trait. A wording defect; nothing is accepted or refused wrongly.
> **Recommended:** the sentence names the derive being generated; step 3d.

> *[2026-10-01, landing 90.]* **S-119…S-126 are SETTLED as D-339…D-346** (the user: "all the recommendations look
> fine to me. lets go with those."). What it unblocks among the entries above: DEF-183 part B (D-340: an argument
> resolves at the invocation site — its shadowing face is a silent wrong answer, so it is the queue's first item),
> DEF-205 (D-346: the two refusals and the lifted digit rule), DEF-193's last clause (D-342: an enum values every
> variant or none), the macro-parameter refusal (D-343), DEF-181 (D-339: the floor is closed). The two the tree
> already implements are the record only (D-344, D-345); `buffer`'s indexing is a plan (D-341).

> **DEF-211 — FIXED 2026-10-01 (landing 90, `nitpick-compiler_28`; found probing the conditions a compile-time
> `pick`'s guard would hold, before step 3b was planned). THE COMPILE-TIME EVALUATOR DID NOT SHORT-CIRCUIT
> `&&`/`||`.** `fold_binary` folded both operands and then combined them, so the right operand was evaluated whether
> or not the left one decided: `comptime func:g = bool(int32:d) never fails { pass ((d != 0i32) && ((10i32 / d) >
> 1i32)); }` with `comptime(g(0i32))` was NITPICK-TYPE-004 "this constant expression divides by zero; reached
> through the compile-time call `g`" where the same function answers `false` at run time (`emit_shortcircuit`
> branches), and a recursion guarded by `||` (`(n <= 0i32) || raw count_down(n - 1i32)`) ran to the depth bound,
> NITPICK-TYPE-025. Never a wrong value — both operands folding gives the right answer — a refusal of a right
> program, and DEF-29's class (two evaluators of one operation that disagree). **Fixed**: the left operand is folded
> first and decides where it decides (`false &&`, `true ||`); the right one is folded only otherwise.
> `tests/backend/programs/comptime_pick.npk` holds it (`p_guard`, `sc_and`, `sc_or`, `count_down`, each at compile
> time, at run time and against the literal), and `776383f` refuses the file.

> **DEF-212 — OPEN (2026-10-01; owner: the compiler seat; found by `nitpick-compiler_28` probing what the run time's
> `pick` does, for D-338). TWO VALUE ARMS WITH ONE VALUE ARE ACCEPTED, AND THE EMISSION IS REFUSED BY `llc`.** `pick
> (x) { (1i32) { … }, (1i32) { … }, (*) { … } }` passes every frontend pass — the exhaustiveness analysis reports an
> arm after an unguarded `(*)` (NITPICK-PICK-004, "can never run") and asks nothing of a value an earlier unguarded
> arm already took — and the emitter writes one `switch` with two equal cases: "error: duplicate case value in
> switch" (probe `r2`, both legs). Under the compare chain (a guard or a range anywhere in the `pick`) the same arms
> compile and the second never runs. A program mistake reported by the wrong tool, with no sentence.
> **Recommended:** NITPICK-PICK-004 at a value (or variant) arm whose value an earlier unguarded arm of the same
> `pick` already matches, naming the earlier arm; step 3d.

> **DEF-213 — FIXED 2026-10-01 (landing 90; found asking how the rule "a `pick` is evaluated only inside a frame"
> could fail, which is how `fold_const`'s reset was read). A `comptime func:` THAT RECURSED THROUGH A TYPE STOPPED
> THE COMPILER.** `fold_const` set the evaluation's depth to zero on every entry, and a type resolved in the middle
> of a `comptime func:` body (`#size_of<int8[N]>()` folds `N`) re-enters it — so the recursion bound D-130 added ("a
> comptime function that calls itself blows the COMPILER'S OWN STACK long before a budget measured in total steps
> runs out") did not bound a function that called itself from inside an array size: `comptime func:deep = int64()
> never fails { pass #size_of<int8[comptime(deep())]>(); };` with `comptime(deep())` ended the checker and `npkc`
> with exit 3 and no message (measured on `776383f`). **Fixed**: a fold entered from inside an evaluation is part of
> it (`TypeResolver.fold_frames`, raised and lowered by `fold_call` alone; `fold_const` resets the depth and the
> said-flag only when no frame is open), so the recursion meets `FOLD_DEPTH`: NITPICK-TYPE-025, once, with its chain
> (`deep` (x64)). `tests/types/rejection/comptime_depth_through_type.npk`; its control asks a type's size inside a
> body and goes on evaluating after it.

> **DEF-214 — OPEN (2026-10-01; owner: the compiler seat; found by `nitpick-compiler_28` writing D-338's twin tests,
> which first called one function at both times). A `comptime func:` IS EMITTED NOWHERE, AND A CALL OF ONE OUTSIDE A
> CONSTANT CONTEXT REACHES `llc` AS AN UNDEFINED SYMBOL.** The emitter skips every function declared `comptime`
> (`emit_program.npk`, three sites, since 1.0.9c), and the checker admits a call of one anywhere a call may stand:
> `int32:v = raw dbl(raw opq(4i32));` — and `raw dbl(4i32)` with a constant argument alike — compiles to `call
> @"npk.t1.dbl"` with no definition, and `llc` refuses the module ("use of undefined value"; probes `t1`, `t2`, both
> legs, `776383f` and landing 90 alike). Inside a `+ - *` node the same call IS folded (`emit_const_fold` asks the
> folder about the node), so whether such a program builds depends on where the call stands. Nothing is computed
> wrongly; a program is refused by the wrong tool with no sentence, and the question under it is the language's:
> **S-128** (is a `comptime func:` also a run-time function?). Recommended there: no — the checker refuses the call
> by name. Until it is answered, a test that needs one body at both times writes it twice (`comptime_pick.npk`,
> `comptime_string_order.npk`).

> **DEF-183 — FIXED 2026-10-01 for part B (landing 92, `nitpick-compiler_29`; under D-340; part A was DEF-189's,
> landing 84). A MACRO ARGUMENT RESOLVES AT THE INVOCATION SITE.** Measured on `eaf6b08` before it was built, both
> legs: `#twice(shared)` beside a module `shared` answered 200 for 10 (the module's binding read); `#twice(n)` with
> a local `n` was RESOLVE-002; a local the BODY had declared captured the argument (`int32:tmp = 1i32; ... tmp + X`
> over the caller's `tmp`: 2 for 8), and so did the body's own `for` binding (3 for 30); a declaration macro's
> argument was captured by a PARAMETER of the function the macro emitted (10 for 105) and by a binding of a module
> it emitted (7 for 100); and `comptime(X + 1i32)` over the caller's local `K` folded the MODULE's `K` (101). The
> fix: the expander marks the ROOT of every argument it substitutes with how far out it was written -- one step back
> per expansion it is substituted into, and for a declaration macro's argument `HYG_SITE` (module-level text) with
> one step per module the macro's body emitted around it -- the clone CARRIES a node's marks (a fresh node's flag
> word starts empty: the second defect below), `instantiate_expr` composes the marks that meet on one node into one
> order (the steps back, `HYG_SITE`, the roots; a root followed by a step back cancels, which is a body that is
> exactly a parameter), and the resolver's one `caller_scope` field is a chain of site frames: an expansion's root
> pushes the scope it is reached in, an argument's root steps back to the frame below and resolves there, and what
> stands under it -- a `pick` arm's binding, an invocation of its own, a `#caller` an enclosing body wrote -- sees
> the sites as they were where the argument was written. A frame holds two scopes -- where an argument written at
> the site resolves, and what `#caller` reaches -- because an ALIAS (a body that is nothing but one invocation,
> D-125) roots two expansions at one node: its target's `#caller` reaches the alias's own caller, as it always did,
> while what the alias's body wrote in the target's argument list steps back to the module's scope. A count that
> does not fit the sites is `RESOLVE-INTERNAL`, said, never a fallback to the module's scope (which is the wrong
> answer the marks exist to end). The tests: `tests/backend/programs/macro_arg_shadow.npk` (the shapes the compiler
> before this one compiled and answered wrongly; it exits 1 there) and `macro_arg_site.npk` (the shapes it refused:
> fifteen functions and a coroutine twin), `tests/expansion/rejection/argument_site.npk` (three names not in scope
> where the argument was written), `tests/types/rejection/comptime_argument.npk`. A name inside a TYPE is the
> module's binding wherever written (D-222), so an argument in an array size is unchanged.

> **DEF-220 — WITHDRAWN 2026-10-01 (registered and withdrawn inside landing 92, `nitpick-compiler_29`). NOT A
> DEFECT: `#caller` THROUGH AN ALIAS REACHES THE ALIAS'S CALLER.** -- `macro:w = () { #inner(); };` lets `inner`'s
> `#caller(k)` read `w`'s CALLER (5) where `{ #inner() + 0i32; }` reads the module's (100), and the first reading
> called that two sites out. It is the ALIAS rule: a body that is nothing but one invocation is whatever its target
> is (D-125; MACRO_REFERENCE section 1), and the tree's own `macro_alias_sites.npk` bumps its caller's `acc` through
> one. The counted roots KEEP it (the alias's target's frame inherits what `#caller` reaches); the first build did
> not, and the chain refused that test (`macro_arg_shadow.npk`'s `caller_through_alias` holds both bodies;
> MACRO_REFERENCE section 5 says it.)

> **DEF-221 — FIXED 2026-10-01 (landing 92; found the same way). THE CLONE DROPPED A NODE'S HYGIENE MARKS.** --
> `#caller(n)` written in an INNER invocation's argument (`macro:outer = () { #inc(#caller(n)); };`) was consumed
> into a marked identifier when the outer body was cloned and LOST the mark when the inner macro cloned its
> argument: RESOLVE-002, or beside a module `n` the module's value in silence (101 for 6). The clone carries the
> marks (`clone_expr` and `clone_stmt` are wrappers over the kind dispatch; `macro_arg_shadow.npk`'s
> `caller_in_inner_arg`).

> **DEF-222 — FIXED 2026-10-01 (landing 92; found the same way). `#caller` IN AN ARGUMENT OUTSIDE EVERY MACRO BODY
> WAS ACCEPTED.** -- `#caller` written in an argument OUTSIDE every macro body (`#twice(#caller(x))` in ordinary
> code) was consumed like a body's and the program compiled; it is `NITPICK-MACRO-008` at the form, as it is
> everywhere else outside a body (`tests/expansion/rejection/caller_in_argument.npk`).

> **DEF-223 — FIXED 2026-10-01 (landing 92; found by `nitpick-compiler_29` asking whether the verified build tells
> two locals of one spelling apart). THE SMT ENCODER RESOLVED IDENTIFIERS BY SPELLING: A SOUNDNESS HOLE OF THE
> VERIFIED BUILD, OLDER THAN THE LANDING AND NEEDING NO MACRO** -- the SMT encoder keeps a function's locals by
> SPELLING, innermost first, and looked every identifier up that way. (a) A callee's contract at a call site:
> `requires d > lim` over the MODULE's `lim` (5), called as `safe(5i32)` from a function with a local `lim` (0), was
> proven against the caller's local; the call went past the checked entry and the body -- its division proven safe
> under the requirement -- divided by zero: the verified binary exits 107 (`MachineFault`) where the plain build
> exits 115 (`RequiresViolated`), on `eaf6b08` and every compiler since the rows existed. (b) Two locals of one
> spelling that hygiene crosses: `10i32 / #caller(tmp)` (the caller's `tmp` is 0) in a body that declared its own
> `tmp` (5) had its `div-zero` row DISCHARGED over the body's (107 for 97) -- and through an argument the same from
> the day D-340 lets one be passed, which is why the fix is in this landing. THE FIX, two rules in `smt_encode.npk`:
> an identifier the resolver bound to a module-scope declaration never reads a local's binding (`ident_bind`, at the
> four sites that turn an identifier node into a binding); and a spelling that an identifier inside an expansion
> uses and the function declares more than once is never NAMED (`enc_prepare` puts it in the escape set DEF-14
> built: every read opaque, no version) -- found on the walk DEF-14 needs complete. THE MANIFEST: `npkg verify
> --record` at this tree -- 7,025 obligations (3,397 discharged, 2,717 open, 0 budget, 906 unencoded, 5 checker),
> matching, the verified compiler rebuilding itself byte-identically, the floor's 388 unmoved; `nitpick.obligations`
> 6,695 rows before and 6,730 after, 6,450 shared, ZERO verdicts moved among shared rows, 2 (symbol, kind)
> discharged counts fell (both a MOVE: the one discharged `overflow` row of `clone_expr` and of `clone_stmt` went
> with the body to `clone_expr_node` and `clone_stmt_node`). Tests: `tests/verify/contract_module_name.npk`,
> `hyg_caller_shadow.npk`, `hyg_arg_shadow.npk`. NOT DONE, and said: the bindings are still kept by spelling (some
> sixty sites); the two rules make every case in which a spelling and the resolver's binding can differ either exact
> (a module-level name) or unnamed (two locals hygiene crosses). Keying the bindings by the resolver's symbol would
> give those functions their proofs back, and is the examination phase's.

> **DEF-224 — OPEN (2026-10-01; owner: the compiler seat; found by landing 92's sweep in the library listener's own
> probe, `nitpick-apps/nitpick-posix/tests/probe/probe02a_failsafe_macro.npk`). `failsafe`'S `pick` MAY NOT COME
> FROM A STATEMENT MACRO.** `func:failsafe = int32(Error:e) { #posix_failsafe(e); };` over a macro whose body is the
> exhaustive `pick (E) { ... }` is `NITPICK-REACH-001`: the reach analysis looks for the `pick` among `failsafe`'s
> own statements, and a statement-position expansion is a BLOCK holding them (MACRO_REFERENCE section 5). Until
> D-340 the program stopped earlier -- RESOLVE-002 at the argument `e` -- so the question was never reached; the
> probe states it as its doubt. To be READ before it is called a defect: whether `failsafe`'s `pick` may stand
> inside a block at all (an expansion's, or a hand-written one -- the listener's `probe02e_block_nested.npk`)
> against D-179's exhaustive-`failsafe` rule and REACH-002's reading of the arms. The listener's PX-010 (one shared
> `failsafe` macro for sixty utilities) depends on the answer. Queued by severity (a refusal, no wrong answer); not
> started.

> **DEF-225 — FIXED 2026-10-01 (landing 93, `nitpick-compiler_30`; the library listener's O-N34, found by
> nitpick-time's 0.2.2 planning, measured by the listener at `5fbaf4a` and `c15422e` and here at `2b5ef34`). A BARE
> `#unreachable();` DID NOT LEAVE: FIVE REFUSALS OF CORRECT PROGRAMS, ONE MISSING ROW.** -- The bindings analysis
> decides "does this statement leave" (`stmt_exits`: which arm a merge drops) and "may this statement complete"
> (`stmt_completes`: FLOW-001) from two lists of statement KINDS, and `#unreachable()` is an expression: as a
> statement of its own it was "an expression statement", which falls through. Measured on `2b5ef34`, each beside the
> same arm ending in `!!! Unreachable;` or `exit`, which compiles: `if (r.is_error) { #unreachable(); }` and then
> `r.value` -- `NITPICK-TAINT-001` (the listener's shape; `r ?| #unreachable()` compiles); `if (c) { x = 41i32; } else
> { #unreachable(); }` and then `x` -- `NITPICK-ASSIGN-001`, and through a `pick` on `c` the same; a `fixed` binding
> written in the arm and again below it -- `NITPICK-ASSIGN-002`; a `move` in the arm and a read below it --
> `NITPICK-MOVE-001`; a body whose last statement is `#unreachable();`, plain or a coroutine's -- `NITPICK-FLOW-001`,
> under a message that says "without `pass`, `fail`, `exit` or a trap". The builtin traps through D-142's route
> wherever it stands and produces no value (D-061; BUILTIN_REFERENCE's row), so by D-121's sentence ("an arm that
> leaves contributes nothing to the merge") and D-323's ("or a trap") each was a refusal of a correct program -- loud,
> never a wrong answer. THE FIX: one helper, `stmt_is_unreachable` (bindings.npk: an expression statement whose
> expression is the builtin), read by both predicates beside the trap statement's row. ONLY THE BARE STATEMENT: under
> another expression (`int32:z = #unreachable();`, an argument, an operand) the statement is one that completes, which
> costs precision and decides nothing about evaluation order; `tests/analysis/rejection/unreachable_edge.npk` holds
> that edge (TAINT-001 and FLOW-001, one site each) beside nine controls. The encoder's `stmt_falls_through` keeps its
> own list and reads the builtin as falling through ON PURPOSE (its claim becomes a hypothesis; it claims less). No
> emission changes: the analysis feeds refusals alone, and the emitter always lowered the builtin as a trap and a
> block terminator. The tests: `tests/backend/programs/unreachable_leaves.npk` (ten functions and a coroutine pair,
> every shape refused by the compiler before this one; exit 0 at both legs) and `unreachable_leaves_trap.npk` (the
> error taken: `failsafe` hears `Unreachable`, 42 at both legs -- the read below the arm is never run with an error).

> **DEF-226 — OPEN (2026-10-01; owner: the compiler seat; found by landing 93's probes `p06b` and `p06c`,
> `wt/30a/.internal/probes`). THE `pick (r.is_error)` FORM'S CHECK IS NOT CARRIED OUT OF THE `pick`.** `pick
> (r.is_error) { (true) { exit 10i32; }, (false) { } }` followed by a read of `r.value` is `NITPICK-TAINT-001` -- with
> every leaver in the error arm (`exit`, `!!! Unreachable;`, and a bare `#unreachable();` after DEF-225), on `2b5ef34`
> and after landing 93 alike -- where the `if` form of the same program compiles. `assign_pick` (bindings.npk)
> intersects `must` over the arms that fall through and takes it out of the `pick` (`state_intersect_must`,
> `state_take_must`); it has no such step for `checked`, so the refinement D-121 gives the `pick` form ("the same
> refinement an `if` gets") holds inside the safe arm and ends with it. Definite assignment through the same `pick` IS
> carried (`unreachable_leaves.npk`'s `assigned_pick`). A refusal of a correct program, loud; the spellings that
> compile are the `if` form, `?!` and `?| #unreachable()`. To be READ before it is built: `checked` is a PERMISSION
> and intersects, so the step is `must`'s twin under the same exhaustiveness condition, and a wrong answer towards
> `checked` admits a tainted read (D-007). Queued by severity (a refusal, no wrong answer); not started.

> **DEF-227 — FIXED 2026-10-08 (landing 94, `nitpick-compiler_31`; the library listener's F-037 in its O-N36,
> found by nitpick-fuzz M11 session 9, measured by the listener at `9126350`, `c3bdae2` and `93bcb66` and here at
> `93bcb66`, both legs). A TRAIT OBJECT BUILT BY THE EXPLICIT CAST READ FREED MEMORY: USE AFTER FREE IN SAFE CODE.**
> -- `dyn Speaks:d = move(l) => dyn Speaks;` then `d.say()` answered the allocator's 0xAA poison (170 for 7), with and
> without `move`, over a POD and over a struct owning a `string`, while the implicit coercion `dyn Speaks:d =
> move(l);` and the direct call answered 7. The cast lowers through `emit_fit`, the one path the implicit coercion
> takes, and `emit_fit`'s transfer registers the cell as the statement's temporary (D-246); the cast NODE was also in
> `temp_producer`'s list, so the same cell was registered a second time under the cast's own name. `fnem_temp_take`
> takes ONE entry per name ("the same name never names two live ones" -- true of every producer but this one), so the
> binding took one registration and the statement dropped the other at its end: a read through `d` after the
> statement was a read of freed storage, and the scope's exit freed the cell again (in a function that returns, the
> parent trips the allocator's check: exit 95 at both legs). The fix is the list: a `dyn` cast is not a producer --
> its cell is `emit_fit`'s registration, and a cast that changes nothing produces nothing. `dyn_cast_owner.npk`:
> nine roads (a declaration with and without `move`, an owning struct, an assignment over a live `dyn`, a lent and a
> moved call argument, a return, a field store, a temporary nobody takes, a coroutine across a suspension) and the
> two controls, each in a function that returns; exit 0 at both legs, 95 on the parent. No refusal added or
> removed; the emitted text moves for every program holding an explicit `=> dyn` cast (the second slot, flag and
> stores gone).

> **DEF-228 — FIXED 2026-10-08 (landing 95, `nitpick-compiler_31`, landed by `nitpick-compiler_32`; the library
> listener's O-N38 item 1, found by its benchmark round; measured by the listener at `93bcb66` and `5fbaf4a`, here at
> `93bcb66`, both legs). THE TEMPLATE SPLICE `&{ }` LEAKED ITS `to_string` TEMPORARY.** -- `string:s = \`n&{i}\`;` run
> 1,000 times printed `heap: allocated=27890 peak_live=24004 count=2000` under `NPK_HEAP_STATS`, where the explicit
> `string_concat("n", int_to_string(i))` prints `peak_live=28`: `emit_template` (ir_expr.npk) called `emit_tostring`
> for a non-string item, handed the owned result to `npk_string_concat`, which copies both operands and frees
> neither, and dropped nothing -- one owned block per spliced value and one per intermediate concatenation, never
> freed and invisible to D-151. The fix is D-246's own mechanism: each `to_string` result and each replaced
> intermediate is registered as the statement's temporary once it has been concatenated past, and dropped at the
> statement's end; a string ITEM is never registered (a place's header is borrowed, a call's result is that call's
> own temporary); the template's FINAL value is registered once, by the producer rule, as before. `tleak`: 24,004 ->
> 28 bytes live, the explicit concat's number; `tests/cost/template_splice.toml` holds `template_churn.npk` (1,000
> trips of `n&{ i }-&{ s }`) to twice `template_once.npk`'s peak (42 against 36); `template_owner.npk`'s nine roads.

> **DEF-229 — FIXED 2026-10-08 (landing 96, built by `nitpick-compiler_31`, landed by `nitpick-compiler_32`; the
> library listener's F-041 in O-N36; measured by the listener at three compilers and here at `93bcb66`). THE
> COMPILER DID NOT TERMINATE ON AN UNBOUNDED GENERIC INSTANTIATION.** -- `deep<T>` calling `deep::<Box<T>>(…)`: no
> exit in 60 s at 1.16 GB (the listener: 4.4 GB at 300 s). TRAITS_REFERENCE's cap of 64 existed in ONE place,
> `resolve_named`'s recursion through nested type arguments (`NITPICK-TYPE-018`), and the emitter's transitive
> monomorphization (`fninst_concrete`, ir_expr.npk -- "Phase B's monomorphizer inherits the same constant",
> type_resolve.npk's comment promised) consulted no depth. Two refusals now, one code: (1) THE CHECKER, at the call
> (`type_generic_call`, type_members.npk): a function's direct self-call whose type argument is not the bare
> parameter and mentions one of the function's OWN parameters (`type_mentions_params_of`, with the declaration's
> window -- an enclosing impl's `U` is one instance per `U`, finite) is `NITPICK-TYPE-018` naming the function and
> the type, since every instance asks for one nested a level deeper and the expansion never ends; (2) THE EMITTER'S
> CAP, for the indirect shape (`f<T>` -> `g::<Box<T>>` -> `f::<U>`): `fninst_concrete` and `note_family_instance`
> measure the requested arguments' nesting (`type_nest_depth`) against `GENERIC_DEPTH` (now `pub`, one number for
> both halves) and record nothing past it; the sentence waits in the emitter (`ExprEmitter.inst_refusal`) and the
> asking call -- or the specialization being emitted, for a family method -- carries it out as `iv_refused` (a new
> `IrVal` state, `LL_REFUSED`: an emitter refusal by NAME, where the rung and the defect states were the only two),
> `emit_program`'s `refuse` pushing the code and the sentence as they are. The indirect probe is refused in 0.34 s
> where the parent never exited. `type_mentions_param` moved from ir_types.npk into types.npk beside the two new
> walks. Tests: `tests/types/rejection/generic_self_growth.npk` (four growing self-calls -- `Box<T>`,
> `Pair<T, int32>`, `T->`, a second parameter growing while the first stays -- beside four controls: the bare
> parameter, a concrete type, an enclosing impl's parameter, a growing argument handed to a function that does not
> call back; the parent reports nothing on the file) and `tests/backend/programs/generic_nesting_bounded.npk` (the
> shapes the cap leaves alone: the fuzzer's three-deep control, a cycle that closes at a concrete type, a self-call
> at the bare parameter; exit 0 at both legs). The emitter's refusal has no suite that expects it (a backend refusal
> is tested by no `[[test]]` kind; recorded as measured by hand) -- the one code is asserted by the rejection file.

> **DEF-230 — FIXED 2026-10-08 (landing 98, `nitpick-compiler_32`, under D-348 step (i); the library listener's
> F-047 in O-N36; measured by the listener at three compilers and here at `93bcb66`, both legs). A WRITE THROUGH A
> `fixed uint8[]` LANDED.** `func:m11w = NIL(fixed uint8[]:v) never fails { v[0i64] = 9u8; pass NIL; };` called over
> `arr[0i64...4i64]` changed `arr[0]` in the caller (exit 10 at both legs; a local `fixed uint8[]` view written
> likewise), because `place_fixed` (type_stmt.npk) answered false for an index whose base is a slice, by D-287's
> reading ("the storage there is not the binding's own") -- `fixed` fixed the view's header and not the bytes, while
> D-074 retired `binary` on the promise that "an immutable byte view is `fixed uint8[]`". The user settled it as
> D-348 (2026-10-08: "your recommendations are fine with me"): D-074's promise is made true, in two steps. Step (i),
> this landing: the walk goes THROUGH a slice base to the view's root (a pointer and a handle still stop it), so an
> element write, a compound assignment, a write through a sub-range, through a slice held in a `fixed` aggregate or
> declared a `fixed` field is `NITPICK-TYPE-086` with the view's own sentence (`place_through_slice`), and `@`,
> `$$i`/`$$m` or a pointer-receiver call on an element is `NITPICK-TYPE-071`. `tests/types/rejection/
> fixed_slice_write.npk` (nine sites; the silent controls: a plain view written, a `fixed` view read, ranged, passed
> to a reader and -- step (ii)'s hole -- to a writer taking a plain view); the listener's `s1`/`s2` refused, its
> `ctl_s3` exit 0 and `ctl_s4` ASSIGN-002 as before; the tree's six `fixed uint8[]` (the `Writer` trait's `write`
> parameter) unchanged. A REFUSAL ADDED, announced in advance (F40). Step (ii), `fixed T[]` as a type, is planned
> next.

> **DEF-231 — FIXED 2026-10-08 (landing 97, built by `nitpick-compiler_31` under D-347, landed by
> `nitpick-compiler_32`; the fuzzer's `ty1657` in O-N36, found by M11's TYPE run; measured here at `93bcb66`, both
> legs). A `frac` PRINTED ITS STORED PARTS, WHICH READ AS ANOTHER NUMBER, AND ONE VALUE HAD TWO STORED FORMS.** --
> `-(1 3/8)` printed "-2 5/8" (stored {−2, 5, 8}: the normaliser's last step borrowed from the whole so `num` was
> never negative beside a nonzero whole, the prototype's rule carried into D-198), which a reader of mixed numbers
> takes for −2.625; and −3/8 was {−1, 5, 8} when computed as `(-1) + 5/8` (printing "-1 5/8") and {0, −3, 8} as
> `1/8 - 1/2`, `==` equal, the parts different. D-347 (the user's ruling of 2026-10-05 and his ratification of
> 2026-10-08): the stored parts ARE the readable parts -- whole and num never of opposite signs, |num| < denom,
> value = whole + num/denom in every case, one form per value; the print shows the sign once ("-1 3/8", "-3/8",
> and "-2 5/8" is −2.625); `.whole` −1, `.num` −3. `npk_frac_norm`'s last block shares the sign (a positive whole
> beside a negative fraction borrows one, a negative whole beside a positive fraction carries one), the four
> `ToString` impls print |num| beside a nonzero whole, `emit_frac_cast`'s integer exit is the whole field alone (its
> "plus one when negative" was the floor form's correction), `npk_frac_cmp` needed nothing (it compares improper
> forms, one number under either form), `frac_basic.npk`'s expectations follow the form, and `frac_parts.npk` reads
> the parts, the print and both casts in thirty-three checks (−(1 3/8), −(2 5/8), −3/8 by three routes as one form,
> the positive twins, a borrow and a carry, −1, 0, ERR, the order, `frac64`). TYPE_REFERENCE's invariant sentence,
> its members' and `ToString` rows and D-198's dated note say the rule. A COMPUTED ANSWER changes for every negative
> frac with a fraction (its print, `.whole`, `.num`): the landing's sweep and emission comparison name every
> program that reads one (the record).

> **DEF-232 — OPEN (2026-10-07; the listener's F-048 in O-N36; measured by it at `5fbaf4a` and `93bcb66`). A STRUCT
> WITH A `NIL` FIELD IS ACCEPTED AND EMITTED AS `type { i32, void }`**, which `llc` and `opt` refuse; the control
> without the field runs. LOUD (the wrong tool refuses). Either the checker refuses a `NIL` field by name or the
> layout gives it no slot; to be decided by reading D-105's `NIL` rules.

> **DEF-233 — OPEN (2026-10-07; the listener's F-038 in O-N36). `npkc` EXITS 3 WITH NO MESSAGE on `give` or `fall`
> OUTSIDE A `pick` ARM.** A compiler trap where a refusal by name is owed (the resolver's or the checker's).

> **DEF-234 — OPEN (2026-10-07; the listener's F-034 in O-N36), three rows of the MODULE run.** (a) An `extern`
> method with an `int8[]`/`uint8[]` parameter generates a bridge stub the compiler refuses -- `NITPICK-TYPE-072` at
> `<bridge-1>:10:5`, a `while` with no `decreases` in the GENERATED source (`bridge_stubs.npk`, written before
> 1.5.8c's rule): no driver method can take a byte payload, which nitpick-sockets needs. (b) `use mod.f;` is
> RESOLVE-002, against MODULE_REFERENCE:68/111/119 and D-273 (which lists `use nested.f;` among the forms). (c) A
> constant cycle's RESOLVE-006 never names the cycle. All loud.

> **DEF-235 — OPEN (2026-10-07; the listener's F-036 (a) and F-029 in O-N35/O-N36; measured at `9126350`,
> `5fbaf4a` and `93bcb66`). KEYWORDS ACCEPTED AS DECLARED NAMES AND THEN UNUSABLE: DEF-103's SHAPE, TWICE MORE.**
> `acquire`, `any`, `trit` and `nit` are accepted as a module-level function's name and every call of one is
> PARSE-002 (DEF-103, 1.6.0 step 3g, exempted METHOD names and these four are the names that intern themselves
> only after a `.`); and `wild int8->:buffer = alloc(16i64);` compiles while `int64:buffer` is refused at its
> declaration -- its first use is PARSE-002, so the block can never be freed. F-036 (b), `10_i32` accepted against
> the DecimalLiteral production, is DEF-190's shape (a trailing `_`) and is recorded on it.

> **DEF-236 — OPEN (2026-10-07; the listener's F-039 and F-042 in O-N36), four rows of the AST and TRAITS runs.**
> (a) A `comptime` VALUE PARAMETER passes the checker and is EMIT-002. (c) A non-constant `joins` deadline is
> accepted (D-182 asks a constant). (d) An unknown attribute name is accepted and ignored. (e) `opaque struct` is
> accepted at module level, against AST_REFERENCE:43 and TRAITS_REFERENCE:368 (F-042 is the same row). F-039 (b) is
> not a defect: `<|` takes its function on the RIGHT, as `|>` does, and corrects F-031's pipe row (DEF-239).

> **DEF-237 — OPEN (2026-10-07; the listener's F-045 in O-N36). `??` ON A `Result` IS TYPE-007 WITH ADVICE NAMING
> THE RETIRED `?`** ("`?` is the one that unwraps a `Result`"): followed, the advice is PARSE-011 (D-175). A
> diagnostic's text; `?!`/`?|` are the spellings.

> **DEF-238 — OPEN (2026-10-07; the listener's F-046 in O-N36), two TYPE rows.** (a) A frac `.num` assignment
> passes the checker and is EMIT-002 (D-198: the members are read-only views; the refusal is the checker's to make,
> by name). (b) A `fixed` PARAMETER can be reassigned inside its callee (the ASSIGN-002 the bindings analysis gives a
> local is not asked of a parameter); no effect outside the callee.

> **DEF-239 — OPEN, DOCUMENTATION (2026-10-07; the listener's F-030, F-031, F-032 in O-N35, F-033, F-035, F-040,
> F-043, F-044 in O-N36, and O-N39's two rows; each checked by it against `93bcb66`). REFERENCE SENTENCES THE
> COMPILER CONTRADICTS, BY FILE:** MEMORY_REFERENCE 8 rows (F-030: `wildx_alloc`'s example, `#wild_ptr`'s argument
> and its acceptance outside `wild`, `= nodrop alloc(...)`, the move example's `malloc`/`free`/`buffer`/NITPICK-019,
> the bare `?` fallback twice, the un-destroyed arena "leak" stale since D-183) plus O-N39's MEMORY:370–371 (the
> heap described as single-threaded; `npk_dalloc`/`npk_alloc_impl` lock `@npk_heap_mx`); OP_REFERENCE 5 rows
> (F-031: `**`, the ternary's `is (cond)`, the pipe examples -- read with DEF-236's correction -- and OP:171's `?`);
> CONTROL_REFERENCE 6 rows (F-032: `println`, the removed `MyMacro!(a, b) where` pattern, §4.2's IF-001/IF-002/
> WHEN-001 the compiler never emits, `ok()` removed by D-097); MODULE_REFERENCE 7 (F-033); LEXICAL_REFERENCE 6
> (F-035); AST_REFERENCE 18 (F-040); TRAITS_REFERENCE 13 (F-043); TYPE_REFERENCE 32 (F-044: §6's `tbb128`/`tbb256`
> alignment 8 against §5's 16, §15's layouts -- `vec3` 32 bytes, `matrix<int64>` 40, `tensor<int64>` 104 -- a
> never-fails function emitted as `{ i32, i32 }`, §28's IR columns, eight IR rows whose patterns are absent) plus
> O-N39's TYPE:1583 (§12.2's `%Arena = type { ptr, i64, i64 }` where the compiler emits `npk_arena_make(i64, i64)`
> returning `{ ptr, ptr, i64, i64, i64 }`; BUILTIN_REFERENCE's `arena_make` row is right). The rows live in
> nitpick-fuzz's `findings/F-0NN-…/` at `d44dfe3`. A doc-sync landing, each row READ against the tree before it is
> corrected: a row may be the compiler's defect on reading (a code the reference promises and the compiler never
> emits is one or the other).

> **DEF-240 — OPEN (2026-10-07; the listener's observation in O-N36). `npkg`'s UNKNOWN-STAGE REFUSAL
> (`npkg/manifest.npk:274`) OMITS `explore`**, which `manifest.npk:41` accepts: the list the refusal prints is not
> the list the reader reads.

> **DEF-241 — FIXED 2026-10-08 (landing 95, with DEF-228; found by `nitpick-compiler_31` probing DEF-228's lowering;
> measured at `93bcb66`). A TEMPLATE WHOSE ONLY ITEM WAS A STRING ALIASED IT.** -- `emit_template` passed a lone
> item through as the template's value, and the producer rule registered that header as the statement's temporary:
> `string:t = \`&{ s }\`;` made `t` and `s` one body with two owners, and a function's return freed it twice (the
> allocator's check: exit 95, `Unreachable`, both legs); a lone string-typed CALL item was the same body under two
> registrations, the F-037 shape. A lone string PLACE or CALL item is COPIED now (`npk_string_concat` with the empty
> header), so the template's value is a body of its own; a lone LITERAL -- a template with nothing to splice,
> `\`plain\`` -- is the literal's own header, as `"plain"` is (D-049): nothing owns a constant, so nothing is aliased,
> and the landing's first form, which copied it too, cost an allocation per evaluation where the plain literal costs
> none (found reading the sweep: the fuzzer's `op0391`, "a backtick template with nothing interpolated is its text",
> had moved). `template_owner.npk`'s first road (3 + 3 where the parent trapped) and its tenth; `tests/cost/
> template_literal.toml` holds a thousand splice-free templates to a thousand plain literals' peak.

> **DEF-242 — OPEN (registered 2026-10-08 by `nitpick-compiler_32`, reading landing 97's emission comparison; low
> priority, S-130's neighbourhood: a cost and a churn, not a wrong answer). EVERY EMISSION CARRIES THE PRELUDE'S WHOLE
> ERROR-ORIGIN SITE TABLE, AND A PRELUDE EDIT MOVES EVERY PROGRAM'S TEXT.** -- D-179's site table (`@npk.sitep.N`,
> `@npk.site.paths`, `@npk.site.lines`; read by `npk_chain_reset`/`npk_chain_push` to name a trap's origin) numbers the
> prelude's sites first and the program's after them, and every program's emission carries every prelude site -- 287
> `prelude.npk` rows at landing 96 in a program that calls none of those bodies (D-262's trim keeps items by
> reference and does not reach the table). Measured at landing 97, whose prelude edit added six guard sites and 27
> lines to the frac section: 561 of 561 tree programs and 2,435 of 2,435 compiling library programs emitted
> different text, every one of them by the table's renumbering (every program site's id by six, every prelude site's
> line below the edit by 27, the lookup's bound) and the twenty frac programs by the landing's change beside it --
> read with `meta/roadmap/1.6/tools/sitenorm.py`, which canonicalises the table and compares what is left. The same
> growth moves every verify row's SITE KEY (the program's node ids) while the problem hashes stand. What it costs:
> some 290 path-and-line rows and a 300-entry pair of tables in every program (block_string_quotes: 57,755 of its
> bytes of text), an emission digest that moves for every prelude edit (the library side compares per-program
> digests across a re-pin, F39), and an emission comparison that must be canonicalised before it says anything.
> RECOMMENDED: the table holds the sites of the functions the emission holds, numbered after D-262's trim settles
> (the ids are the emission's own constants, so a per-program numbering is sound), its own small landing with the
> comparison tool as its measure; the examination phase's otherwise.

> **DEF-243 — OPEN (registered 2026-10-08 by `nitpick-compiler_32`; the library seat's O-N40 item 1, found by nitpick-time
> 0.3.1's planning at its pin `5fbaf4a`; measured here at landing 98's compiler, `9fede45`, both shapes beside their
> controls). A BUILTIN NAMED AS A FUNCTION VALUE PASSES THE FRONTEND AND DIES IN THE EMITTER.** -- `func int64() never
> fails:f = mono_now; int64:t = raw f();` is NITPICK-EMIT-002 at the initialiser (5:34), and `raw call_it(mono_now)` with
> `call_it = int64(func int64() never fails:g) never fails { pass raw g(); }` is EMIT-002 at the argument (4:27) -- "the
> emitter could not lower this, although the frontend accepted it; a defect in the compiler" -- where the same two shapes
> over an ordinary function (`seven`) compile and exit 0 at -O0. The checker admits a builtin's NAME wherever a function
> VALUE is expected (the generated signature table, D-201, types a builtin's CALLS and says nothing about the bare name),
> and the emitter has no symbol to hand out: a builtin is the emitter's intrinsic (`ir_runtime.npk`'s generated rows),
> with no body of its own. The two fixes are the listener's own: the frontend refuses a builtin used as a value, with a
> sentence that says so, or the emitter lowers it (a thunk per builtin so named). RECOMMENDED: the refusal -- D-294 already
> makes a builtin's name the compiler's (no program declares one), a builtin's call is typed by its own rules (TYPE-054),
> and a value nothing can call through the type system's one path is a wrong shape, not a missing lowering; the code is the
> plan's to choose (TYPE-054's neighbourhood). Nothing in the tree, the libraries or the applications takes the shape
> (nitpick-time measured it before writing one and wrote none).

> **DEF-244 — OPEN (registered 2026-10-08 by `nitpick-compiler_32`; O-N40 item 2; measured at `9fede45`). THE LOADER
> KEYS A MODULE BY A PATH'S SPELLING, SO A ROOT IMPORTED BACK FROM A SIBLING DIRECTORY IS LOADED TWICE.** -- `a/a.npk`
> (`mod:a; use "../b/b.npk".g; pub error:EA; pub func:f = …`) compiled AS THE ROOT FROM ITS OWN DIRECTORY (`npkc a.npk`
> in `a/`), with `b/b.npk` importing it back (`use "../a/a.npk".EA; use "../a/a.npk".f;`), is NITPICK-RESOLVE-010 at
> `pub error:EA` -- "two error constants derive the same code (D-179, an FNV collision)" -- and, where the root declares
> `main`, NITPICK-RESOLVE-013 at it ("`a.npk` is a module of this program"): the root `a.npk` and the import
> `../b/../a/a.npk`, collapsed TEXTUALLY to `../a/a.npk`, are two spellings of one file, the loader keys the module by
> the spelling, and the file is read twice -- its one error constant declared twice under one code, its `main` once as
> the root's and once as a module's. The same program named from the parent directory (`npkc a/a.npk`) collapses
> `a/../b/../a/a.npk` to `a/a.npk`, the root's own spelling, loads the file once and exits 0; the mirror (`b.npk` as the
> root from `b/`, imported back as `../b/b.npk`) is RESOLVE-013 at b's own `main`. No import cycle is refused here and
> none should be -- the file-level import rounds settle a cycle (D-273's fixpoint); the collision is the SYMPTOM of two
> loads, and the sentence misleads. RECOMMENDED: the identity is the FILE -- D-236's manifest-root-relative path where a
> manifest root exists (the driver already renders every site path that way), the real path (`..` resolved against the
> directory, never the text) where none does -- decided at one place in the loader, so that RESOLVE-010 means what it
> says. Nothing in the libraries or the applications takes the shape (nitpick-time measured and worked around nothing).

> **The subcycle 1.6.1d** (PLANNED execution-grade 2026-09-26 by the compiler seat, `meta/roadmap/1.6/1.6.1d.md`;
> before 1.6.1 step 2; the README row): four landings by severity — step 1 the memory faults (DEF-118, DEF-119,
> DEF-123, DEF-127, DEF-134), step 2 the silent wrong loop counts (DEF-128 under S-110, DEF-129, DEF-130, DEF-135 and DEF-136 found by the step's probes, DEF-137 found by landing 72's red harness), step 3 the leaks
> (DEF-120, DEF-121, and DEF-138 … DEF-141 found by the arm scope's probes), step 3b DEF-122 under D-328 (the floor's
> `cstring` layout moves, with a two-floor snapshot refresh; split from step 3 at the floor's seam, 1.5.8b step 6's
> precedent), step 4 the over-restrictions, the refusals of what the reference
> promises and the documentation (DEF-124, DEF-125, DEF-126, DEF-131 under S-111, DEF-132, DEF-133 under S-112, and
> the listener's DEF-142 and DEF-143) —
> each reproduced from the listener's minimised programs first, its fix measured by the fuzzer's own recipe
> (npkc, llc at -O0 and after opt -O2, ld.lld -static, NPK_HEAP_STATS), and the compiler tree's and the
> listener's exposure swept under both checkers where a refusal moves. The user's standing rule puts the faults
> first: an unsafe program the compiler accepts is the failure class this language exists to prevent.

> **THE SUMMARY OF A TRAIT'S METHOD IS THE UNION OF ITS IMPLS' (1.6.1 step 0; not a numbered defect: the
> summaries never shipped).** The counterweight `tests/analysis/rejection/borrow_pair_plain.npk` stopped refusing
> under the first form of D-325's summaries, and the probes that measured why found a bound method call in a
> generic body (`touch<T: Sink>(T->:bp, Pair->:op) { bp.put(@op.k); }`) and a `dyn` dispatch read as a KNOWN
> callee whose body stores nothing -- the checker records the TRAIT's declaration (a signature with no body, or
> a default any impl may override) as the callee, and its slots were empty. Read as an UNKNOWN callee (every bit
> set), every bound `write` of the prelude's writers refused (a view of a local beside `self.inner`, a `W->`). The
> reading that is both sound and exact is the reach analysis's (DEF-86): the trait method's summary is the UNION
> of every impl's that implements it (`override_of` from the impl table, `merge_summary` after each impl method's
> walk, the fixpoint carrying it) and of its own default body's -- so `Writer.write`, none of whose impls stores
> what it is handed, connects nothing, and `Sink.put`, whose one impl stores `v` into `self.p`, connects `@op.k`
> to `*bp`. Found on the way: a summary composed through a CYCLE of callees grew a path step per round without
> a bound (`TextWriter<W>.write` → `text_write_str` → `tw_write_all(@(tw.inner))` → `Writer.write` ∋
> `TextWriter<W>.write`: `inner`, `inner/inner`, …) and the analysis never settled (BORROW-010 on the prelude);
> a composed path is cut to its first `PATH_MAX_STEPS` (8) steps, a broader place -- the closed direction for
> every reader. `dyn_dest.npk` (2) and (3); `borrow_pair_plain.npk` carries both readings of D-223's verdict.

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

> **[1.6.0 step 3 (2026-09-25) — E-7, for 1.6.2 (leg C), owned by the user: THE EMISSION
> STATES NOTHING OF WHAT THE COMPILER KNOWS ABOUT A POINTER PARAMETER, AND A PER-FUNCTION
> CHECKER CAN HOLD THE OPTIMISER ONLY TO WHAT THE IR STATES.]** Found by the Alive2 smoke's
> no-inlining twin (`meta/roadmap/1.6/1.6.0.md`, the step-3 record): the string drop body
> `npk.drop.3` takes `ptr %v` with no attribute; `opt -O2` infers `nonnull` on the parameter
> and `inbounds` on the `getelementptr` of the header's `cap` field (a load through it must
> be dereferenceable), and Alive2 refutes the pair with a base pointer ten bytes BEFORE a
> fifteen-byte object — the source's plain `getelementptr` reads inside the object, the
> target's `inbounds` one is poison. No emitted caller passes a drop body, a `Self->`
> receiver or a coroutine frame anything but the address of the object itself, so no
> program of ours is miscompiled by the inference; but the IR does not SAY so
> (`dereferenceable` appears nowhere in `build/npkc.ll`), and every translation-validation
> verdict over such a function is bounded by that silence. The lead: the emitter states
> `nonnull`, `noundef` and `dereferenceable(<the pointee's size>)` on every pointer parameter
> whose argument it constructs itself — drop bodies, pointer receivers (DEF-24's and DEF-94's
> `@recv`), a resume's frame pointer, the lent parameters of `.req`/`.measure` predicates —
> and `dereferenceable_or_null` where the vacant value is `null` (D-225). Safety-positive on
> every leg: A's points-to and null checks read the same facts, B's encoder may use them,
> C's per-function pairs become verifiable. Not 1.6.0's (no emission change at the gate,
> §2.7): its cost is one emission change under a snapshot refresh, its measurement the twin
> re-run. The same smoke's fifteen INTER-PROCEDURAL verdicts (IPSCCP's constant returns
> absorbed into the callers, unchanged with the inliner off) are 1.6.2's design question —
> a per-pass `tv` plugin run, or a pipeline whose inter-procedural passes are validated by
> another means — recorded in the step-3 record, not a lead of their own. **SETTLED 2026-09-25
> (the user: "1.6.2 is fine with me. wherever you think is the most logical place for it to go"):
> E-7 is a step of 1.6.2's plan, written at 1.6.0 step 5; the fifteen inter-procedural verdicts and
> this one are re-examined in the testing-and-benchmarking phase the user plans after cycle 1.6.**

> **[1.6.0 step 4 (2026-09-25) — E-8, for 1.6.1 (leg A), owned by the user: THE EMISSION
> CARRIES A TARGET TRIPLE AND NO `target datalayout`, AND AN ANALYZER THAT READS IT AS IT IS
> LAYS EVERY STRUCT OUT UNDER LLVM'S DEFAULT.]** Measured at step 3 and read at step 4
> (`meta/roadmap/1.6/1.6.0.md`, the two records): `opt` and `llc` DERIVE the x86-64 layout from
> the triple — `opt -O2` over our emission writes the string into its output, `{ i32, i64 }` is
> 16 bytes with the `i64` at 8, as the binary has it — but NIKOS does not: its three tools call
> `llvm::parseIRFile` with the default callbacks and its importer takes `module.getDataLayout()`
> as it is, so every NIKOS row of step 3 over a plain, whole or verified form was computed under
> LLVM's DEFAULT layout (`i64` ABI-aligned to 4: `{ i32, i64 }` at 12 bytes with the field at 4,
> `{ i8, i256, i256, i256 }` at 100 bytes where the binary has 112), and a verdict on such an
> access describes a layout the artifact does not have; the two programs' datalayout TWINS
> (step 2's `.dl` forms) gave site lists byte-identical to their plain forms', so the programs'
> rows stand, and the compiler's twin was REFUSED by NIKOS's five-width integer-alignment table
> (a one-token NIKOS fix, `frontend/llvm/src/import/data_layout.cpp:75`, 1.6.1's port list).
> A module with neither line is laid out under the default by `opt` too (measured), which no
> module of ours is. The lead: the emitter writes `target datalayout = "…"` — the x86-64 string
> `llc` derives — beside the triple, pinned as ONE string in `nitpick.toml`'s `[toolchain]`
> (D-204's authority: a stated string nothing consumes is the next stale document, so a belt
> holds it to what `opt` derives from the triple), so that the ARTIFACT says what layout it
> has and any analyzer reads the binary's layout with no transform (D-319) and no twin. The
> binary does not move (`llc` derives the same string; the harness's opt-O2 leg already ran
> under it), the emission's text does (a re-pin notice, the ladder rows), the datalayout twins
> retire (the plain form IS the twin), and the cost is one emission change under a snapshot
> refresh. Not 1.6.0's (§2.7: no emission change at the gate). **Recommendation:** 1.6.1's
> first step, landed with NIKOS's one-token fix and measured by the compiler's own form
> analysed under the binary's layout for the first time; the user decides at S-102's asking.
> **SETTLED 2026-09-25 with S-102 (D-322 (5)): the emitter writes the line, pinned in `nitpick.toml`'s
> `[toolchain]` and held by a belt to what `opt` derives; 1.6.1 step 1, with the snapshot refresh it needs.**
> **LANDED at 1.6.1 step 1 (2026-09-26): the emitter's first two lines, the floor's and the shim's hand-written
> ones, pinned as `[toolchain] triple`/`datalayout`, held by both runners to what the pinned `opt` derives and to
> every emission; the snapshot refreshed; the gate's twins retired. The measurements are under D-322's landing
> note and in `1.6.1.md`'s step-1 record.**

> **[1.6.0 step 4 (2026-09-25) — three defects of NIKOS found by READING its pinned source,
> owed to the user's NIKOS tree (`REPOS/nikos`), each a first commit of 1.6.1's port if NIKOS
> wins the gate; the step-4 record has the lines.]** (1) The pointer pre-analysis BUILDS the
> `inttoptr` constraint and never ADDS it (`analyzer/include/ikos/analyzer/analysis/pointer/
> constraint.hpp:386-399`, against `484-492` which adds), so under `--proc=intra` — the only
> mode that runs the pre-pass — a pointer loaded from integer-derived memory is BOTTOM and
> everything after the load is "unreachable", which means unchecked; two lines. (2) The
> `LibcppCoroAlloc`/`LibcppCoroFree` intrinsics exist in the name table, the intrinsic table
> and the engine and are missing from `constraint.hpp`'s switch and four checkers' switches
> whose default is `ikos_unreachable` — undefined behaviour for a module using `llvm.coro.*`
> (ours has none). (3) The integer-alignment table is filled for the widths {1, 8, 16, 32,
> 64} alone (`frontend/llvm/src/import/data_layout.cpp:75`), so under a datalayout that
> aligns `i128` to 16 the AR size of `{ i128, i1 }` — the result type of
> `llvm.sadd.with.overflow.i128` — is 24 against LLVM's 32 and the module is refused ("llvm
> type and ar type alloc size are different"); one token, or ~15 lines for every `i<N>:` spec.
> **[1.6.1 step 0 (2026-09-26, `nitpick-compiler_s16`) — the three landed in the user's NIKOS tree as
> the `nitpick-port` branch (D-324), three commits on `v2.4.0` (`94b54c2`), pushed to `origin/nitpick-port`
> and never to `main`: `070247c` (the `inttoptr` constraint ADDED — two `this->_csts.add(...)` wraps),
> `c712a3e` (128 sampled into the AR integer-alignment table — AR's `find_alignment_info` answers an
> unsampled width with the first larger entry, else the largest, LLVM's own rule, so the two agree once
> every width LLVM specifies is in the table), `db47f9d` (the coroutine intrinsics in the
> pointer-constraint switch and the five checkers, modelled as the engine already models them). MEASURED
> with the port build against the pinned one, by hand and then under the gate's runner: the compiler's
> datalayout twin (`npkc.plain.dl`) is READ where it was refused (3,325 functions defined, `main`'s the
> one with checks — the executor wall, as the plain form); the eight controls' inter-mode counts are
> identical, and in intra mode the `inttoptr` fix does what the reading said — `uninit.plain` 124 → 137
> `ok` with 75 → 66 `unreachable`, `uninit.planted` 109 → 122 with 81 → 72, `null.planted` 39 → 41 —
> every planted defect's verdict unchanged (7 of 8 in both modes). Found on the way: a module that CALLS
> `llvm.coro.begin`/`llvm.coro.free` is refused by the importer earlier, for its `token` operands
> ("unsupported llvm type"), on the pinned build and the port alike — the `library_function.cpp` mapping
> to `LibcppCoroAlloc`/`Free` is reached by no legal module today; the switches are total for the day it
> is, and the maintainer's tree is told so in the commit. The pin moves with landing 68 (`pins.txt`,
> `engines.sh`'s `NIKOS_SHA`).]**

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
