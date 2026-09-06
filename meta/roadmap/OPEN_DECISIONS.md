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
| **LIVE-1** | `limit`/`requires`/`ensures`/`invariant` compile to nothing — no check, no rung refusal (D-068 violation) | Yes — npkc emitted a `requires`-carrying fn as a bare `sdiv`, a `limit` binding as a bare `alloca` ([../audit-0.8-close/probes/limit_drop.npk](../audit-0.8-close/probes/limit_drop.npk)) | Add `NITPICK-RUNG-001` refusals naming cycle 1.4, same shape as `prove` |
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
| ~~**S-41**~~ | **D-266** | **SETTLED (user, 2026-09-06: "yes, ratify it as stated with the frozen-selector rule"), lands at 1.5.2h (the recommendation as written, the frozen-selector rule stated in the decision, and one sharpening: a view has NO address, since the one pointer type carries no mutability and every address is a write path — asking the same question of TYPE-063 found DEF-24, the implicit receiver address).** ~~OPEN.~~ (Raised 2026-09-05 with D-264.) A BORROWING `pick` binding form. Under D-216 the only `pick` that may bind an owning payload is the consuming `pick (move(v))`; under D-264 a `T` payload counts as owning, so a generic enum with payloads cannot derive `Eq`, `Ord`, `PartialOrd` or `Clone` (DERIVE-006, as a `string` payload never could), and no program can compare two `Opt<string>`s without consuming one. The gap S-23's settlement named ("whether the language wants a BORROWING `pick` form … is a separate question") is concrete now. **Recommendation:** a lending `pick` binds its payloads AS BORROWS -- each binding a read-only view of the payload IN PLACE, under the borrow rules (no escape, no move out of it, no drop of it; the escape analysis treats it as `@` of the selector), spelled with the binding forms the language has -- so the derive generator's picks are sound as written, `Opt<string>.eq` is derivable, and the consuming form keeps its meaning. A language change: the checker's binding types (a view of `T`), the analyses' borrow treatment, the emitter's binding by address instead of by copy, and a decision on the spelling (D-004's second-class borrows are the frame). Owner: the user (a decision); implementer: a subcycle of its own, before 1.5.3 lowers contracts over enums or as its first step. | 1.5.3 | D-264's derive consequence; the workbench's containers will meet it at their first `Eq` over a payload enum |
| ~~**S-42**~~ | **D-265** | **SETTLED (user, 2026-09-06: "lets go with your recommendation on S-42 and ratify it"), LANDED at 1.5.2g (2026-09-06) (the recommendation as written; the z3 asymmetry stated in the decision: a solver's output is a committed VERDICT, a toolchain's is checked bytes).** Raised 2026-09-06 by the library workbench's first CI run. `nitpick-time`'s CI built the pinned compiler commit `aaffb87` on GitHub's runner and got a `build/npkc` of `3c05818c…` where the workbench's machine and the compiler tree both get `a3b0dadc…`; `build/npkrt.o` is `c9ddbcff…` on both. Nothing the compiler side claimed is contradicted: BUILD_REFERENCE §5 says the same inputs, THE TOOLS INCLUDED, give the same bytes, and D-204's pin is a VERSION (20.1.2, asked of the tools) -- a version is not a binary, and two builds of 20.1.2 (this machine's Ubuntu `1:20.1.2-0ubuntu1~24.04.3`, the runner's apt source) differ in distro patches and configure-time defaults. The `repro` stage measures one machine (working directory, `llc` twice, absolute site rows). What IS claimed across machines and never yet measured across two: the compiler's EMISSION, `build/npkc.ll` -- same source and snapshot, same text anywhere (D-078, D-236); if it differs, that is a compiler defect. The ladder leaves six files (`builder.o`, `builder`, `npkrt.o`, `npkc.ll`, `npkc.o`, `npkc`) and the first digest that differs names the stage; the workbench's CI records them per run. **Recommendation:** (a) the pin STAYS a version -- a tool-binary digest in `[toolchain]` would refuse every machine but one, hostile to the libraries' CI and to any user, for a property that belongs to the toolchain; (b) `npkg build` prints the six digests at its end and the compiler side publishes the emission's digest with every pin notice, the emission being the cross-machine claim; (c) the first cross-machine comparison of `npkc.ll` is the measurement to make (the workbench's runner against this machine), and only a difference THERE opens a compiler item. Owner: the user (a), the `npkg` writer (b, small), the workbench (c). | 1.5.3's open or the library era | the first library CI runs are the trigger; the digests are already being recorded |

## 2f. Compiler defects reported by the library workbench (owner: the `src/` writer — scheduled as 1.5.1b, before 1.5.2)

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

**DEF-24 — OPEN, scheduled as 1.5.2h step 0 (found 2026-09-06, planning
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

## 4. Decisions blocking 1.5 (verification) and 1.6 (the analyzer evidence — "Astrée" until D-233)

> **1.5.1 LANDED (2026-09-03)** — the verification surface TYPES (D-220/D-221's typing halves; `meta/roadmap/1.5/1.5.1.md`): `limit<R>` names resolve, `Rules` bodies type over `$`, every proposition is a `bool`, contract expressions admit only what a proposition can evaluate anywhere and call only named `never fails` `pure` functions; the five questions it raised were ratified as **D-241** (D-163's contract row retires), **D-242** (purity is a declared `pure` clause with a `Pure` column on every builtin), **D-243** (`old(expr)` a keyword operator, admitted in invariants), **D-244** (`main`/`failsafe` carry no contract) and **D-245** (`result` a keyword with a leaf node); S-13 closed at its step 1. Found on the way: macro expansion SHARED verify nodes across expansions (the last expansion resolved won — a miscompile the day 1.5.3 lowered a contract in a macro-emitted function; expansion clones them now).
>
> **1.5.0 LANDED (2026-09-03)** — the skeleton with the D-007 division pair end to end (D-218/D-219; `meta/roadmap/1.5/1.5.0.md`). C-17→D-218's items (1)–(11) are all implemented or scoped: the SMT emitter, the determinism profile, per-function processes, the integer encoding, the ownership-trusting memory model, the content-hash identity, `llvm.assume` elision, the `undef` ban, and TCB.md. The catalogue's remaining kinds land 1.5.1–1.5.8.

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
  wrong. Owner: the doc-sync pass at 1.5.3's close.

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
