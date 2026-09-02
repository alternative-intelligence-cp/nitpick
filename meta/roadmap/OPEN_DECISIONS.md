# Open decisions and unwritten specs (post-0.8)

> The post-0.8 queue, opened at the 0.8-close replanning. The prior queue
> (opened D-061, fully closed at D-079) is archived as
> `done/OPEN_DECISIONS-through-0.8.md`. Every item here traces to the audit at
> `audit-0.8-close/total_audit.md`, blocks a named cycle, and nothing is
> optional — per the standing constraint, anything entering the language must be
> settled before the Astrée trial, because re-verification is unaffordable.
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

| id | proposed | question | needed by | source |
|---|---|---|---|---|
| ~~**S-4**~~ | **D-215** | **SETTLED (user-ratified), LANDED at 1.4.4.** ~~Should `dyn` coercion refuse a channel-carrying concrete?~~ The 1.4.1 walker fixes closed the owned-container laundering of D-183's `gives` rule (mutex/arena/atomic returns now answer truthfully), but a `dyn` cannot: erased content could hold an endpoint, and `contains_channel(DYN) = true` would demand `gives` of every dyn-returning creator — a claim about channels most such returns do not carry. The precise fix is refusing the COERCION of a channel-carrying concrete into `dyn` — D-207's "erased content can hide a borrow [or an endpoint]" reasoning completed at the erasure boundary, where the concrete type is still known. The residual today is runtime-caught (`StaleHandle` via slot generations), never a dangling address. **Recommendation: refuse the coercion; land with D-207's per-scope-joins subcycle (1.4.4).** | 1.4.4 | 1.4.1 |
| ~~**S-6**~~ | **D-235** | **SETTLED (user-ratified 2026-09-01), LANDED at 1.4.7b step 1: a simd vector and a function value ride; the sync primitives, atomics and arenas — and whatever holds one — refuse permanently under TYPE-057; the belt names no rung.** ~~Which element kinds may a channel carry beyond the decided set?~~ (1.4.7, OWED-8.) `NITPICK-TYPE-057` now refuses the DECIDED classes in the checker — a borrow (D-072/D-183), a `dyn` (D-207), an `OwnedFd` (D-185), a `Guard` (D-056), an aggregate holding one — and admits everything that transfers whole (scalars, the tier, `string`/`buffer`, `Handle<T>`, endpoints, owning aggregates). Between the two sit kinds NO decision names, which the backend's admission table (`chan_elem_ok`) still refuses AS A RUNG by this row: `Mutex`/`RwLock`/`CondVar`/`Barrier` and `atomic<T>` (a cell other tasks may hold a borrow of — D-180 sanctions BORROWS of the primitives as spawn crossings, never a by-value transfer), `arena`/`shared_arena` (owned storage other handles point into), a function value (a code address; probably harmless), `simd<T, N>` (a plain value — its refusal looks like an oversight of the 1.3.1 sweep, not a decision), and the meta kinds. **Recommendation:** admit `simd` and function values (they transfer whole; retire the rung for both); refuse the sync primitives, atomics and arenas PERMANENTLY under TYPE-057 with their own message (the D-180 reasoning: what other tasks may borrow cannot be moved out from under them). Until ratified the rung stays, so a `TYPE` code never says "never" about a kind nobody decided. | 1.4 close | 1.4.7 OWED-8 |
| ~~**S-7**~~ | **D-236** | **SETTLED (user-ratified 2026-09-01), lands at 1.4.7b step 4: manifest-root-relative paths in the source manager, for diagnostics and the site table alike.** ~~Should the site table record paths relative to the manifest root, so the artifact cannot depend on how `npkc` was invoked?~~ (1.4.7 close.) D-179's site table stores each source path AS GIVEN: `npkc src/main.npk` from the tree root emits `c"src/frontend/token.npk"`, and the same call with an absolute path emits the absolute one — 1,489 of the 1,647 site constants in a dry-run snapshot refresh, which the fixpoint (stage2 == stage3) and the STAMP both passed, because each compares the emission with itself. D-078 says emitted bytes must not vary with the build tree and D-204's H9 names exactly this leak, but the `repro` stage tests it with absolute inputs from two cwds — which agree BY CONSTRUCTION when the argument is absolute — and the harness's own selfhost emission is invoked absolutely. The committed snapshot is clean only because `bootstrap/seed/README.md`'s commands are relative; the close added a `repro` guard that refuses an absolute site path in `stage1.ll`, which protects the committed artifact and nothing else. **Recommendation:** the source manager records every path relative to the manifest root (the directory holding `nitpick.toml`, found by walking up from the main file) for diagnostics and the site table alike — one spelling, deterministic, and the same bytes the snapshot carries today when invoked from the root; H9's `repro` leg then measures something, since an absolute argument would no longer change the emission. Until decided, the guard and the README's discipline hold the line for the committed artifact only. | 1.4 close | 1.4.7 close |
| ~~**S-8**~~ | **D-093 (annotated)** | **SETTLED (user, 2026-09-02: "the recommendations are fine"), LANDED at 1.4.8 step 2b: `range` is a builtin generic type keyword resolving to `TY_RANGE` under the range expression's own element rule; `range_type.npk` binds, passes and iterates one, `range_rules.npk` refuses a float or bool element and a second argument.** ~~D-093's `range<T>` spelling is not resolvable.~~ (1.4.7b.) D-093 (0.4.2) settles that a range is a value of type `range<T>`, "interned like every other type", and the escape analysis and the channel-element table both classify `TY_RANGE`; but the resolver has no type named `range` — `range<int32>:r = 2i32..4i32;` refuses with TYPE-001 — so the type can only ever be inferred, never written, and nothing in the tree writes one (found by the D-235 probe that tried to). A decided spelling that was never implemented is the dormant-rule pattern. **Recommendation:** implement `range<T>` as a builtin generic type name in the resolver (beside the other builtin generic names; no grammar), with a conformance case that binds and passes one, before the frontend freeze. The alternative — an annotation on D-093 striking the spelling — leaves a type with no name, which is D-093's own argument against. | before the frontend freeze | 1.4.7b |
| **S-9** | — | **Should a rejection test's expected diagnostics be matched EXACTLY, as BUILD_REFERENCE §7.1 states ("unexpected diagnostics fail a test as surely as missing ones"), rather than as a subset of what was reported?** (1.4.8 Part D.) Both runners implement the subset rule the Python harness has carried since 0.8: every expected code must be reported (at its line and column when spelled), extras pass, and notes are their own channel. The spec sentence describes a rule nothing enforces — the dormant-rule pattern, in the test runner this time. **Recommendation:** enforce it, in both runners in one step, after measuring how many rejection files carry unasserted extras (the two `--verdicts` lists plus one script make the measurement): a cascade diagnostic a test does not assert is either a second finding the test should name or a cascade the checker should not emit, and either is worth knowing before 1.5 reads these suites as evidence. Until decided, parity is measured on the rule as implemented, and §7.1 says so. | 1.4.8 close | 1.4.8 Part D |
| **S-10** | — | **Should the suites `npkg test` runs beyond the `[[test]]` targets be DECLARED in `nitpick.toml` rather than built into both runners?** (1.4.8 Part D.) §7.1 settled `[[test]]` with three kinds and a `path`; the harness then grew the real-parser sweep, the five rejection suites, the programs and fixtures, the runtime floor's tests and the acceptance suite as hardcoded loops, and `npkg` mirrors them exactly so the parity diff covers the full tree. A manifest that declares four of fourteen suites is a manifest a reader cannot trust to say what `npkg test` runs — the stale-document shape D-204 refused for flags. **Recommendation:** extend `[[test]]` with a `stage` key naming the tool that judges the suite (`parse`, `resolve`, `check`, `compile`, `run`, `runtime`) and a `recursive` flag, declare every suite, and have BOTH runners read the one table (the harness refusing a stage it does not know, loudly) — one change to both, landing once parity has held on the hardcoded shape so the diff can prove the move changed nothing. | before the SWITCH | 1.4.8 Part D |
| ~~**S-5**~~ | **D-216** | **SETTLED (user-ratified): the consuming `pick (move(v))` — ownership transfers into the matched arm; lands at 1.4.3b.** ~~An owning enum payload cannot be read back out — enums with owning payloads are write-only containers.~~ TYPE-046 correctly refuses a `pick` arm binding an owning payload (the binding is a copy; two owners, one double free), and no move-binding form exists in patterns — so `enum:Res = { Note(string); }` can be constructed and dropped but its string can never be recovered. The missing form is a CONSUMING destructure: a `pick` over `move(v)` whose arms receive ownership of what they bind (the enum's own drop then does not run — the arm took the payload). Spelling and semantics are a language decision. Natural home: beside S-2's loop-carried move work (1.4.3), which is already in the move analysis. | 1.4.3 (or its own slot) | 1.4.1 |

## 3. Decisions blocking 1.4 (self-hosting)

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

## 4. Decisions blocking 1.5 (verification) and 1.6 (Astrée)

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
| **G-2** | next free | **The full integer-width set** (`int1/2/4`, `int512`–`int4096`) is accepted by lexer/impl but has no layout in TYPE_REFERENCE, and `tt_int` computes size/align 0 for sub-byte widths. Enumerate with a stored-as-byte rule, or trim the grammar. | type-sys #18 |
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
numbers as of 23e9e79; sweep with the next doc-sync pass.

---

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
motivates must land BEFORE the Astrée trial — re-verification is
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
4. **C-10…C-12 before 1.4 anything** — the fixpoint must be measuring the right
   thing before self-hosting is declared.
5. **C-14…C-17 before 1.5 anything**, and ~~C-19 answered before 1.5
   exits~~ — C-19 CLOSED by D-232→D-233 (2026-09-01); 1.6's bring-up gate
   is ordinary scheduled work with no external dependency.
6. **G-1, G-2 whenever the frontend freeze is formally declared** — cheap now,
   re-verification later.

This is more than one sitting's work, as the previous queue's closing note said of
its own. That is expected and is not a reason to compress it.
