# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Status: PHASE C UNDERWAY — cycle 1.4 (self-hosting) COMPLETE and cycle 1.5 (verification) has THREE subcycles left — 1.5.8b (the `overflow`/`bounds`/`cast-range` rows, `sealed`/`hidden`, the constants, the wrapping family and field limits), 1.5.8c (`decreases`/`unbounded` with the `terminate` and `stack-depth` rows) and 1.5.8d (the close), the old 1.5.8 having been planned 2026-09-18 as four under D-304…D-307: 1.5.0–1.5.8 have landed (1.5.8, COMPLETE 2026-09-19: the runtime's uncontrolled stops closed — a poisoned float cast, a stack overflow with no `failsafe`, a guard page a frame could jump, the last net for every other fault), the floor itself is specified, modelled, its models read twice, its spec's caller assumptions written down and EXECUTED, every synchronization step of the floor and of each concurrency test run under the schedule explorer (1.5.7), and TCB.md is finalized

The **specification set is complete** — `meta/specs/` holds twenty-one documents and
`DECISIONS.md` records 314 decisions, D-001 through D-314 (this sentence said 240 from 1.4.8c until 1.5.8b's planning). The **plan is in `meta/roadmap/`**,
organised as numbered cycle folders holding `x.y.z.md` subcycle files; finished
cycles move to `meta/roadmap/done/`. Start at `meta/roadmap/ROADMAP.md`.

**Cycles 0.0–1.0 are done** (`meta/roadmap/done/`): the lexer, the AST and parser,
the module/symbol/visibility passes, the type system, the static analyses, macros
with `comptime` and `#[derive]`, IR emission, `nlibc` and the runtime floor, full
type lowering, the memory allocator, and generics/traits/`dyn`. **`npkc` exists**:
`src/npkc.npk` over `src/driver/pipeline.npk` (the one front-half sequence;
`tools/check.npk` is a thin wrapper over it) and `src/backend/`. The harness runs
**272 real-backend programs** (the count at the 1.5.6b close — this sentence said 172 from cycle 1.0 until then; each also re-run through `opt -O2` + `llc -O2`
since 1.3.8) and, since 1.5.4 retired the rung suite, asserts NO `NITPICK-RUNG-001` rejection (1 until 1.5.4 lowered `prove`/`assert_static`; 4 until 1.5.3 lowered the three contract cases; 6 until 1.5.2 lowered the two `limit<Rules>` cases; 8 until 1.4.7's
OWED-8 moved the two channel-element cases to the type checker as `TYPE-057`), and **stage 1 rebuilds itself byte-identically** — the fixpoint has held
through every cycle since 0.8.

**Cycle 1.1 (async and concurrency) is underway.** Landed: `never fails` checked
everywhere with `raw`/`drop` licensed by it (D-163), the `Duration` clock, coroutine
lowering as hand-written switched-resume state machines (D-177/D-178), the typed
`Error` system with origin chains and an exhaustive `failsafe` (D-179), per-thread
executors, real threads over `clone(2)` (D-181), and `atomic<T>` plus channels,
thread pools and actors (D-182). **Cycle 1.2 — the managed
lowering — was inserted ahead of the rest** (D-183): 1.1.11's `Mutex` hands out a
guard whose release IS scope exit, and until this cycle the default regime
implemented no drops at all. Landed through 1.2.3e: the cap==0 ownership bit,
per-type generated `@"npk.drop.<tid>"` bodies, drop flags with move/`pass`
clearing, the move-only rule for owning types (TYPE-046/047), `move T:p`
consuming parameters, and scope-exit drops LIVE for strings, structs and enums
— the compiler drops its own locals and still rebuilds itself byte-identically.
Debug instruments that stay: 0xAA free-poisoning, and `@npk_quarantine`
(npkrt.ll, ships 0) — a never-reuse mode that makes any use-after-free
deterministic, with a poisoned-source tripwire in `npk_string_concat`, and **`NPK_HEAP_STATS`** (1.5.1b step 0): a program whose environment carries
that name prints `heap: allocated=<n> peak_live=<n> count=<n>` — the
allocator's own words, deterministic for a given input — on fd 2 at exit; the
harness's `cost` stage and `npkg test` read it, and a wall-clock is never a
verdict.
**Cycle 1.2 is COMPLETE** (`meta/roadmap/done/1.2/`): drops are live for
strings, structs, enums, `dyn` (which owns a heap cell and drops through
the vtable's slot 0) and arenas, in sync AND async bodies (frame-resident
flags; the wind-up unwind drops too, and the first test to drive a wind-up
found the rouse and the grace-wait defects). Channels reclaim at their
creating function's exit — `StaleHandle` is reachable, slots reuse under
moved generations — and OWNING elements cross channels under the heap's
first futex mutex, with `move` required at the send. A factory says
`gives` after its parameter list (1.2.6): the caller's exit then owns the
returned channels' reclaim, and creating a channel in an unmarked
channel-returning function refuses. A failed `send` drops its element —
the rule a failing callee already applies to its `move` parameters. Owning elements in ARENAS
refuse at `arena_make` until the generated deep-view family lands (D-183).
Phase C renumbered around the cycle (self-hosting 1.3, verification 1.4,
Astrée 1.5; the map is in `ROADMAP.md`). **1.1.11 is COMPLETE**: `Mutex<T,LEVEL>`/`Guard<T>` (the guard's scope-exit
drop IS the release), `RwLock<T,LEVEL>` (`read` → read-only `RGuard<T>`,
`write` → the same `Guard<T>`), `CondVar<LEVEL>` (`timedwait` lends the
guard and reacquires; a timed-out wait SPENDS it — nulled, not held past
the deadline), and `Barrier<N,LEVEL>` (a timed-out party hands its slot
back). Levels feed the 0.5.6 ordering analysis from the receiver's types;
borrows of the primitives are the sanctioned spawn crossings. **1.1.12 (the
reactor) is underway — a landed (D-184, B-3a closed)**: epoll WITHOUT
timerfd (`epoll_pwait` as the armed executor's idle wait, carrying the
sleeper deadline), an eventfd as the cross-thread wake channel,
`suspend_io` + prelude `io_ready` + deferred `io_unwatch` (a registration
lives exactly as long as its wait), and **the task-identity rule**: awaits
drive children inline, so EVERY waiter registration — channels and locks
included — resolves the executor's `cur_task`, not the frame it was
lowered in; the nested-wait lost-wakeup this fixed was latent everywhere
(`nested_wait.npk` regresses it). `sys` takes pointers (`ptrtoint` at the
trampoline — the one place an address becomes a number). **1.1.12b landed
(D-185)**: `OwnedFd` (TY 39 — the drop IS the close; `own_fd`/`release_fd`;
move-only; a channel refuses it, a spawn MOVE is join-bounded legal),
`Path` + `path_parse` (constructor FUNCTIONS — the language has no static
methods), the `Reader`/`Writer` traits (`Self->` receivers, `within`) and
`ByteReader`/`ByteWriter` as prelude retry loops over `io_ready`
(`WouldBlock`/`Interrupted` are named prelude errnos), `Whence` seek, and
TYPE-048 (an impl keeps its trait's `async` — a sync body driven as a
coroutine is corruption). The awaited-method child frame now seeds `Self->`
receivers by ADDRESS. S-1 was **settled by the user as D-186**:
`string_slice` returns an OWNED COPY (`string_from_bytes` stays the
explicit view primitive), and the fallout closed D-183's field/element
overwrite leak — assigning over an owning FIELD or managed-array ELEMENT
now drops the old value (`overwrite_owned.npk` proves both by descriptor
exhaustion). **1.1.12c
landed**: the text layer as GENERIC WRAPPERS — `TextWriter<W>`,
`LineBufWriter<W>` (buffering is a TYPE, never a mode field), `LineEnding`,
unconditional read translation with the split-`\r\n` pending flag,
`string_bytes`, and `std_in`/`std_out`/`std_err` constructors that each own
a CLOEXEC dup (std descriptors stay blocking — the one stated D-071
exception). Three compiler fixes fell out: template fields must resolve
through `struct_field`'s BOUND walk (three raw sites closed — the drop-body
generator was reporting "no type named `W`" on nested generic instances),
the `Ident { Ident :` struct-literal lookahead reads the fourth token, and
awaited bound-calls in generic bodies substitute before trusting the
recorded template symbol. **1.1.12d landed — 1.1.12 (the reactor and the
I/O surface) is COMPLETE**: async methods behind `dyn` — the method slot
holds the concrete RESUME, a size tail (`@"npk.fsz.<frame>"` globals) tells
the caller how big the frame is, and the await site builds it from the
TRAIT's shape through a per-site prefix type and drives the standard loop.
Object safety: async + `Self->` receiver is safe (by-value `Self` alone
still disqualifies); `Self->` receivers admitted generally — and the sync
thunks' by-value-receiver latent (handing a method its own first bytes as
an address) is fixed. `dyn_stream.npk`: one erased `report(dyn Writer…)`,
two writers, two byte patterns. **1.1.13 (the Bridge, D-149 over D-055) is
underway — stage a is COMPLETE (D-187, D-188)**: `#ptr_add<T>`
element-scaled; `atomic_from_ptr::<T>` fused-only over the turbofish;
`lib/nbridge.npk`'s sealed shm (`shm_create_sealed` — the seal is the
architecture's load-bearing line), SCM_RIGHTS pair, and `spawn_driver` →
`bridge_reap` over `npk_driver_clone_exec` (fork-shape clone, allocation-free
child, PDEATHSIG + NO_NEW_PRIVS + dup3 + execve); the DRIVER REGISTRY is
published BEFORE the clone (CLONE_PIDFD writes the pidfd into the slot),
`npk_driver_kill_all` runs on the TRAP PATH before user `failsafe`, and a
clean exit 0 with a live driver refuses — trap −4109 `DriverLeak` (D-188,
the D-151 exit rule's second registry). The Bridge tier never traps: `?!` is
unwrap-or-TRAP-as (a test-main assert, barred from the Bridge by v3 §4.2 —
an EPIPE schedule caught the misuse), so the library binds-and-fails. The
harness grew `tests/backend/fixtures/` + `// argv:` substitution
(`mock_driver.npk` speaks the wire), and the `// stress:` loop — dead in the
real-backend stage until now — runs there again. **Stage b is COMPLETE
(D-189)**: the §6.1 ring header (power-of-two capacity a spawn parameter),
`io_watch` (register-only, two args) + prelude `io_ready2`, dispatch —
descriptor before the SeqCst head publish, EXEC_NOTIFY, the TRIPLE wait
(ctrl/pidfd/stderr), [UNTRUSTED] tail and reply validation,
kill-on-protocol, KILL-ON-DEADLINE (a hang ends now; the graceful §5.4
ladder is close's alone), poisoning — `bridge_close` (rungs end on
DEATH-OR-BUDGET, never on a wait merely returning; the release phase
synchronous behind `closed`), and the stderr tail WOVEN into the waits
(D-180 bars a borrowing drain task) with `bridge_stderr_tail` the reader.
The work flushed two runtime defects: the executor never CONSUMED the
waker's due-now stamp at resume, so every once-woken task busy-polled all
its later waits (a year latent behind retry loops; one cmpxchg at
`npk_step`'s resume site), and D-186's slice copy allocated from the
wild-TRACKED entry (a sliced string alive at `exit 0` was a phantom
WildLeak). **Stage c is COMPLETE (D-190) — 1.1.13 and the Bridge are
DONE**: `extern` blocks lower to GENERATED stubs (`bridge_stubs.npk`, the
derive mechanism's sibling — source synthesized and spliced before
collection; the block binds nothing, the stubs are ordinary async fns,
zero downstream special cases), methods declared IN FULL (`Bridge->`
first, `Duration` last, the v1 wire vocabulary between; departures refuse
EXTERN-001, written contracts EXTERN-002 — D-002's mandatory-contract
parse rule lifted as D-149 scheduled), the INTERFACE HASH rides INIT_REQ
under D-179's error-identity seed (one derived-identity constant
ecosystem-wide) and a stale driver refuses at the handshake, and
`sdk/npkdrv.h` + harness-built C reference drivers prove the wire end to
end (`extern_c_driver.npk`: echo through the ring, driver-reported
refusal → EDriverError, hostile tail → protocol kill, stale interface →
EDriverSpawn). **CYCLE 1.1 IS COMPLETE** (D-191, `done/1.1/`): user
variadics and `..^` landed at the close (the spec's "a variadic call lowers
to building one" — gather, spread, empty tail, and the awaited gather in a
frame slot; methods may not collect, by refusal), the await-edge rungs
became checker rules (SUSPEND-003: no await in a `where` guard) and
internal belts, and the close-out's first exercised programs found three
latent defects: the collector's name was never typable in a body, a
range-view was not counted as address-taking, and stage D's "lives to the
function's end" extension had been a silent no-op since birth (`fn_end`
read off a block span that covers only the opening — every address-taken
local whose last textual use preceded a later suspension sat on the dying
resume stack). **G-3 settled (the user): the exotic tier is NEW CYCLE 1.3**,
and Phase C renumbered a second time — self-hosting 1.4, verification 1.5,
and 1.6 (which was Astrée until **D-233** re-homed the evidence to the emitted
IR; the cycle numbers are unchanged, its topic is not). Adopting `dyn Writer` in npkc's own diagnostics is 1.4
material. **The 1.1 interlude closed the self-contained backlog**: `sys` is
TYPED (D-192 — `Result<int64>` by D-048's contract, register-shaped
arguments refused by name, zext for unsigned/kernel args, `?|` given `?!`'s
unknown-operand fallback; the S-3 rows), and float/char `ToString` landed
(D-193, §6b — shortest-round-trip Dragon4 over `uint2048` in the PRELUDE,
no wide division by construction, 353 python-generated known-answer
vectors; total UTF-8 with U+FFFD for non-scalars). The bare-builtin
signature table is proposed as P-3 (OPEN_DECISIONS §3), owed before 1.4.
**Cycle 1.3 (the exotic numeric tier) is COMPLETE** (`meta/roadmap/done/1.3/`) — 1.3.0 ratified the
whole surface as D-194…D-200 (the survey corrected the tier's story: `tfp`
is Twisted FIXED Point per D-036/D-037, the prototype's floating tfp_ops is
the superseded design; `dim256` units are exponent vectors with `unit:`
declarations ratified; ternary Kleene logic rides `&`/`|`; frac is
invariant-normalized exact-or-ERR; tensors cap at rank 9 with inline dims).
**1.3.1 landed `simd<T, N>`**: TY_SIMD through every walker (the B-7 sweep
enumerated up front), annotation-directed `simd(…)` with splat, elementwise
ops/compares (verdicts are `simd<bool, N>`), bounds-checked lane places,
`.len`, ordered extract-chain reductions (no intrinsics — nothing can
become a libcall), elementwise casts, and D-007 any-lane division guards
(zero → DivByZero, structural INT_MIN/−1 → DivOverflow). TYPE-029 (the
Tier-1 vector refusal) retired with the ctor rung; `.any` joined `.acquire`
in the keyword-after-dot interning. `simd_basic`/`simd_div0`/`simd_divmin`/
`simd_oob` + `simd_rules.npk` (14 refusals). **1.3.2 landed `tfp*`** —
twisted fixed point on native `i32..i256`, the tbb machinery generalized
to both twisted kinds (compare-trap, add/sub, `%`, negation, ERR, is_err)
plus Q-scaled mul/div through widened intermediates with round-trip
narrowing; literals fold EXACTLY in subset-1 limb arithmetic (the seed
still builds src/ — C-13); the cast matrix with sentinel maps and
trap-on-ERR-exit under both spellings; `.floor()`/`.trunc()`; exact-decimal
`ToString` (uint256 core, 100 python-`fractions` vectors); REACH refined
(twisted division arms no DivByZero; TbbErr armed for both families).
Three D-195 amendments recorded: compare-on-ERR TRAPS (D-008 §5 outranked
the carried spec row), raw bitwise/shifts STRUCK (`ERR << 1` is an ERR
laundry), tbb↔tfp casts impossible. **The flagged tbb asymmetry was then
settled by the user and landed as the D-144 amendment**: leaving tbb, ERR
traps under BOTH spellings (`=>!` acknowledges the VALUE's loss; the
0.9.5 carrier read crossed taint as INT_MIN), and the value crossing is
range-classified like any numeric pair — closing a second hole where
`tbb64 => int8` truncated and `tbb32 => uint32` sign-extended raw under
the CHECKED spelling; the prelude's four `tbbN:Hash` impls now spell
their guarded crossing `=>!`. **1.3.3 landed `dim256<Unit>`** — units as
packed 7-exponent SI vectors on interned `TY_DIM` (unit equality IS
type-id equality; a=256 keeps `type_int_bits` uniform), THE ZERO VECTOR
IS `tfp256` (cancellation yields the bare number, scaling needs no
special case, bare `dim256` refuses — one meaning, one spelling),
`unit:Name = algebra;` in the grammar (`unit` is now a keyword) with the
seven SI base names compiler-known and the derived set as PRELUDE
declarations, single-name annotations, `*`//`/` composing vectors with
`+ - %`/compare demanding equality (both units shown in base-product
form, TYPE-049/050), the one cast boundary (`=> tfp256` drops — ERR
RIDES, the crossing never leaves the family; `tfp256 =>! dim256<U>`
asserts), no `ToString` by design (`&{x => tfp256}` is the spelling),
and erased lowering (the TY_TFP-site sweep at 256 bits). Two found
defects: the `pick` ERR-arm demand had not extended to `tfp` (the
wildcard swallowed the taint — now all three twisted kinds demand it),
and **compound assignment was a second implementation of the operators**
— raw `add`/`sdiv` since 0.9.5, so a twisted `+=` wrapped, `/=` by zero
reached the hardware, and a float `+=` emitted integer `add`; the
arithmetic value core is now extracted (`emit_arith_value`) and BOTH
spellings route through it, with `op=` held to the slot's unit.
**1.3.4 landed the ternary/nonary bases** (`trit`/`tryte`/`nit`/`nyte`,
D-197): ONE `TY_TERN` kind, the binary rung storing the VALUE (balanced
order IS numeric order; the sentinels coincide with tbb's carrier
minimums, so `tbb_min_decimal`/`tbb_divlike` serve a third family; the
prototype's packed-trit LUTs are deliberately not carried — §7's
emulation-as-identity warning), overflow past the BALANCED bound → ERR
(the prototype's trit clamp OVERRULED by D-197's uniform text), `/ %` at
the multi-digit pair only (TYPE-051), the user-ratified Kleene `&`/`|`
on the single digits (min/max, NOT = `0 - x`), `.trit(i)`/`.nit(i)`
digit extraction by the offset trick (bounds-checked, ERR-sticky;
`trit`/`nit` joined the after-dot keyword interning), contextual
literals in any base checked EXACTLY against ±29524-style bounds, the
one-family cast matrix with the D-144-as-amended leaving rule, and four
prelude `ToString` impls. Found at implementation: **the same-text
early-out defeated the tbb cast matrix at SAME-WIDTH crossings** —
`tbb32⇄int32` share `i32`, so the crossings 0.9.5's audit hole was
ABOUT rode through unchecked, passing by coincidence (INT_MIN's bit
pattern IS the sentinel); the tbb intercept now precedes the early-out
as tfp's has since 1.3.2. **1.3.5 landed the `frac*` exact rationals**
(D-198): the tier's first NON-SCALAR family — `{whole: iN, num: iN,
denom: uN}`, five invariants after every operation, exact or ERR. The
algorithms are the PRELUDE's, in Nitpick, in `int256` (one core, the
width's bounds as parameters; the emitter unpacks → calls the
deterministic prelude symbol → repacks-or-ERR — the arithmetic a
verifier reads is source, not hand IR); operators `+ - * /` and
comparisons exactly (TYPE-052 gates `%`, pick selectors, and literals —
`int => frac` is the entry); read-only members `.whole/.num/.denom`;
canonical ERR `{minN, minN, 0}` with the disjunctive `is_err`; casts
widen `=>`/narrow `=>!`-absorbing, `=>! flt64` rounds, `=>! int`
truncates toward zero, ERR traps both spellings on exit, and a float
NEVER enters. Found: D-169's non-scalar compare belt called the first
checker-ADMITTED aggregate comparison a defect (the frac arm now
precedes it), plus two reserved-word collisions (`tid`, `fd`) in new
emitter code that only npkc's own parse catches. **1.3.6 landed
`complex<T>`** (D-199): `{T, T}` over the ratified four elements,
`complex(re, im)` reusing the ctor node (the payload names the
keyword), the arithmetic in per-element PRELUDE cores — tfp bodies ride
the language's own Q operators with the any-component-ERR → BOTH
canonicalization an explicit line, float bodies write SMITH'S division
(the naive denominator overflows at √max) with flt32 computing in
flt32 — equality only (no total order exists; per-component, NaN-aware,
taint-trapping), `is_err`/`ERR` on tfp elements only, NO casts either
direction, methods `.re/.im/.conj/.abs2` everywhere and `.abs` float-only
(`llvm.sqrt` declared in the fixed block), four concrete-instance
prelude `ToString` impls ("3+4i"; ERR pairs render "ERR"), TYPE-053.
**1.3.7 landed the library tier** (D-200): `buffer` minimally as the
MANAGED owning byte cell (`buffer_new(int64)` never fails — zeroed,
`len == cap == n`, `n <= 0` the empty answer; the string's trio, the
string's shared drop body, move-only, `==`-refused; §23's draft verb
family/`buffer_free`/`resize` struck by decision — typed access is
`#ptr_add` + `<-`), `#sqrt` (flt32/64 → `llvm.sqrt.*`, the instruction —
a Newton loop computes a DIFFERENT number), `lib/nvec.npk` (vec2/3/4
over `simd<flt64, N>`, ctor FUNCTIONS, dot/cross/length2/length; vec9 as
nine named `flt64` fields with identity and 3×3 mul) and
`lib/ntensor.npk` (`matrix<T>` `{buffer, rows, cols}`, `tensor<T>` rank
≤ 9 dims INLINE, library bounds `fail`s — `tmatrix`/`ttensor` are
`matrix<tryte>` instances, the twisted ERR proven THROUGH the
container). Found: a checker-typed never-fails builtin's Result envelope
rode out under the bare recorded type — `buffer:b = buffer_new(16i64)`
stored 32 bytes into a 24-byte slot (the generic rt path now extracts at
the site; `buffer_new` is the first of its class); a generic struct
literal in a generic factory is the BARE `matrix{ … }` (the
`TextWriter{}` idiom — `matrix<T>{ … }` does not parse); checker-typed
builtins are called WITHOUT `raw` (`own_fd` precedent).
**1.3.8 closed the cycle**: the tier's lowering pinned in the ir_types
fixture (fifteen `ll_is` assertions, no `// ll:` markers — the seed
refuses every tier type, so the pin is the real compiler asserting its
own text), the last G-3 rung retired to a defect confession, and the
harness grew the **opt-O2 leg**: EVERY real-backend program re-runs
through `opt -O2` + `llc -O2` with the same expected exit (stress loops
included), the zero-dependency scan repeated on the optimised object
(`opt` may MINT libcalls), and a missing `opt` failing loudly. The
prototype's optimiser-removed-guarantee lesson is now a standing
instrument, not a one-off audit — and its FIRST full run caught a real
one: the suspend walk recorded no suspension point for the bare
primitives (`suspend_io`/`suspend_until`), so `io_ready`'s `give_up`
deadline sat in an ALLOCA that re-entry re-created — every run since
1.1.12 passed because the fresh slot happened to hold the OLD BYTES at
-O0, and the first optimised build moved the caller's frame and broke
eleven programs deterministically. The walk now records the park as a
suspension (the await rule: past the arguments), `give_up` lives in the
frame, and both levels answer identically. The second hole in the same
analysis (D-191's `fn_end` was the first).
**CYCLE 1.4 (self-hosting) is COMPLETE** (`meta/roadmap/done/1.4/`; closed 2026-09-02 at 1.4.9). 1.4.0 ratified the whole batch
as **D-201…D-209** (`meta/roadmap/done/1.4/1.4.0.md`): the builtin surface
typed from ONE generated signature table (D-201 — never-fails builtins
type BARE, the 13-arm convention generalized; the emitter's parallel
authority retires; the ~1,700-site `raw` shed rides a transitional
rule), the fixpoint criterion restated (D-202 — the harness has measured
the right thing since 0.8.1; the spec's "byte-identical binaries"
sentence was the defect), the committed bootstrap IR + survival map +
THE FLOOR'S PERMANENT FORM IS HAND-WRITTEN LLVM IR (D-203 — the user:
the goal was removing the C/C++ layer, never LLVM; npkrt.ll re-homes to
`runtime/` at the switch), reproducibility pinned and tested (D-204),
the normative builder rule with the switch at 1.4.6 (D-205 — measured at
open: `src/` is STILL fully subset-1; SUBSET_1 §4's gradual-adoption
story never happened), `npkg` + `npk_spawn` + the closed-world link
(D-206), per-scope joins with the `dyn`-element refusal permanent
(D-207), loop-carried moved-from states (D-208), and the adoption scope
(D-209 — generic collections and `dyn Writer` diagnostics in; mass
`&{ }` re-spell, pipeline async, and `src/` macros OUT by decision). The
user's bug/vuln-statistics research (§6c) ran the same day: eight
deep-research reports live in `meta/roadmap/research/` with
decision-grade digests in `research/digests/`, the coverage
audit in `research/COVERAGE_AUDIT.md` **ratified whole as D-210…D-221**
(overflow TRAPS on plain ints, `const`/`fixed`-only module state, the
consuming `pick (move(v))`, the dyn coercion refusal for
channel-carrying concretes, the nfs safety riders, the schedule-
exploration harness, NIKOS struck from 1.5, and the whole 1.5
verification architecture recorded early — new subcycles 1.4.2b and
1.4.3b scheduled), and the research-informed plans landed — cycle 1.5's
README carries the full proposed verification architecture (solver
determinism profile, encodings, obligation catalogue), 1.6's the
analyzer-evidence plan (rewritten at **D-233**; it was the Astrée handbook and
the C-19 question list until the evidence moved to the emitted IR), and 1.4.2–1.4.8 each have
execution-grade subcycle files written for a fresh executor to follow
without asking. 1.4.1 landed the instruments (B-7's walkers-total check
found and fixed the missing TY_ENUM drop arm, a silent payload leak live
since 1.2, plus the contains-walker laundering holes). **1.4.2 (P-3) is
COMPLETE** — four commits, full harness green at each:
BUILTIN_REFERENCE.md's marked regions are now the ONE signature
authority, parsed strictly (`gen_tables.py` hard-fails on a row it cannot
read) and cross-checking the Signature column's `Result<…>` against the
Fails column; `builtins.npk` gained `builtin_sig_special/_count/_param/
_param_move/_ret` and **`ir_runtime.npk` is GENERATED from the same
rows** — the LLVM ABI derived, with a five-token `**ABI:**` note
(`inline`, `sym=`, `ret=`, `args=`, `envelope`) only where a symbol
departs, and the nine emitter-only symbols in their own §2d region. Every
regular builtin call is TYPED (arity, per-argument fit, `move`, spread
refusal; a `never fails` builtin is the BARE value, a may-fail one a
`Result<T>` — D-201 §4), four bespoke arms retired into the table,
`NITPICK-TYPE-054` carries a builtin's own call rules, and **2,215
`raw`/`relay` plus 242 `drop` came off the tree** in 150 files, with the
seed's three wrapped-flag flips and its extract-at-site in the same
commit. The emitter's parallel authority is gone: `result_ll_value_half`
and its `?|`/`?!` consumers, `emit_raw`'s wrapped/inner fallback,
`drop`'s nine hardcoded builtin names, and the argument coercion's blind
`zext` (the D-192 class — a negative `int32` widened into an enormous
positive `int64`) which now reads signedness off the recorded type.
Found on the way: **five latent unchecked pointer reinterpretations in
`src/`** (`ralloc`/`alloc` bytes bound or cast without `=>!`, invisible
while the callee had no type) and the instrument defect that
`check_runtime_sigs_agree`'s derived-inner cross-check had NEVER RUN — it
read the wrapped flag out of a leaked loop variable. Three of step 3's
four defects were caught by the compiler checking ITSELF, none by a test.
**1.4.2b** (D-210 overflow traps on plain ints, D-211 `fixed`-only module
state), **1.4.2c** (D-222 — `const` retired), **1.4.3** (D-208: the move
analysis learns about PARAMETERS, which is where the hole actually was — 26
findings in `src/`, including a live double free), and **1.4.3b** (D-216: the
consuming `pick (move(v))`) followed. **1.4.4 (D-207) is COMPLETE**:
`join_head` is per SCOPE — by a MARK, since the list is already a LIFO stack,
so a scope's exit joins until the head is back where its entry left it (one
pointer per scope, frame-resident at role 41 in a coroutine because a scope
spans suspensions). The order at every scope exit is now **join → defers →
drops → that scope's channel reclaims**, innermost first; D-183 ran the joins
LAST, which put a spawned child's borrowed `Mutex` behind the mutex's own
drop, and `type_drops`' "by the join discipline nobody can still hold it"
comment was a claim the lowering did not keep. `%npk.join` — one block every
exit branched to, and the reason the join ran last — is gone: the return seam
stores the result BEFORE the unwind and returns after it, so the first-child-
error arbitration reads the same slot it always did (D-136 untouched — the
value is still EVALUATED before the defers). Lifted: `channel()` inside a
LOOP (per-iteration reclaim; `chan_loop.npk` proves it by the first
iteration's endpoint answering `StaleHandle` seven reclaims later) and
`shared_arena` teardown (`TY_SHARED_ARENA` drops for real and left two
walker excuse tables). Riders: `exit` runs joins and defers and NO reclaims
(the drain runs generated drop bodies — the walk D-183's amendment keeps off
the controlled-shutdown path), and `.destroy()` on either arena kind clears
the binding's drop flag, the same mechanism `move` uses. The `dyn`-element
channel refusal is PERMANENT by D-207 and its rung now says so. Owning made
`shared_arena` MOVE-ONLY (TYPE-046 keys on `type_drops`), so a borrow became
the only way to share one — and **D-180 was amended (user-ratified) to make
`shared_arena<T>->` the fifth sanctioned spawn crossing**: its hazard test is
"a mutation the holder cannot see", and a shared arena has no mutation at all
(D-154 writes a slot once, before its handle escapes, and never again), while
the joined-before-freed order this cycle built is what bounds the borrow.
Two finds, neither by a test of the thing that broke: the **D-151 leak check
runs only on `exit 0`** — a program reporting success as 42 checks nothing,
which is how the first `shared_arena_drop.npk` passed against a build with
the drop disabled — and **every thread join had been sleeping its entire
five-second deadline since 1.1.9**. `npk_thread_join` waited on the
CHILD_CLEARTID word with `FUTEX_PRIVATE_FLAG`; the kernel's wake from
`mm_release` is a SHARED wake, which a private waiter never receives, so the
word was cleared promptly, the wake went nowhere, the wait ran to its
timeout, and the reload then reported success — right answer, five seconds
late, every time. One token; `mutex_basic` 5.00s → 0.12s, and the mandatory
deadline can once again tell "finished" from "stuck".
**1.4.5 (D-204) is COMPLETE**: the toolchain is a pinned build INPUT —
`nitpick.toml`'s `[toolchain]` carries the exact patch release (20.1.2, not
a minor pin: a patch release can change instruction selection) and the four
flag sets, and every `llc`/`opt`/`ld.lld` invocation is BUILT from those
lists, fifteen call sites and one authority, because a stated flag nothing
consumes is the next stale document. `check_toolchain_pin` asks the tools
themselves (not `llvm-config`, which is a -dev package the build does not
need) and refuses a mismatch loudly; `selfcheck.py` holds its FAILURE path.
A new **`repro` stage** runs the same compiler on the same absolute inputs
from a DIFFERENT working directory and requires the same bytes, then
assembles the compiler's own IR twice and compares the objects — the two
hazard classes the fixpoint cannot see, since IT compares two different
binaries (agreement, not determinism): H1, ASLR and hash-iteration order,
which has no controlling flag and can only be tested, and H9, the build
path leaking into the artifact, which is not hypothetical here because
D-179's site tables embed source paths. Both are clean, measured. Found:
`npkseed.py` never did embed its argv path — `Module` puts the path in a
`path` FIELD while `_path` is the location attribute nothing sets on a
module node, so the ModuleID was `"?"` by accident, one character from the
opposite; it is an explicit constant now.
**1.4.6 (D-203, D-205) is COMPLETE — the builder switched.** The committed
`bootstrap/seed/stage1.ll` is what builds `src/` now; `npkrt.ll` re-homed to
`runtime/`; the Python seed builds nothing. D-205's rule changed meaning with
it: `src/` is no longer bounded by subset 1 but by what the SNAPSHOT can
compile, so a feature enters `src/` only after a snapshot that understands it.
**1.4.7 (adoption, D-209) is at step 3.** Step 1: five copies of the
diagnostic walk became one. Step 2 is COMPLETE — every growable array in `src/`
is a `List<T>` and `ralloc` appears nowhere outside `list.npk`; twenty-two
families, four of which the original enumeration had missed because it keyed on
`ralloc` and one family `alloc`s-and-copies. **D-229 is COMPLETE**: the walk is
generic and borrowing, sorts, and is tested through the capture it exists for;
`impl:Sink:Writer` lives in `diag_writer.npk` beside the walk (REACH is
import-scoped and the impl is async). It is the tree's FIRST impl on a struct
declared in another module, and its first build found that the symbol scheme
had never said which module qualifies a method — definitions and three call
paths disagreed the moment an impl left its trait's module. **The impl's
module, on both ends** (D-156 read as its vtable row already said), with
byte-identical IR for every pre-existing program; `impl_foreign.npk` pins every
shape, and `Trait.default_method(recv)` — refused even in one file — works.
**Step 3 is COMPLETE**: 268 counter loops are `for (intN:i in 0iN...b)` —
`..` is the INCLUSIVE range, `...` the exclusive one — and the 332 that stay
`while` do so by rule: **a `for` captures its bound at entry and a `while`
re-reads it**, so a loop bounded by a container's live count stays a `while`
and the spelling says which (proposed for ratification in 1.4.7.md). Five
match-shaped unwraps became `?|`/`?!`. **OWED-8 is CLOSED**: a type that
cannot be a channel element is `NITPICK-TYPE-057`, the checker's refusal at the
spelling and at the substitution, from one table the backend's belt also reads;
the undecided kinds stay a rung. **OWED-1 is CLOSED**: the one red the
parallel scheme ever produced was a race in the C test fixture (the hostile
tail stored after the completion), reproduced 11 times in 120 under contention
and fixed at its source; the Bridge, the reactor and the optimiser were
exonerated by measurement, and D-228's width calibration is unblocked.
**1.4.7 IS CLOSED (2026-09-01)**: the FNV step took the one copy's `uint128`
spelling (1.4.6's owed item; `bridge_stubs.npk`'s second copy of the trio,
missed by 1.4.2b's collapse, is gone — 175 of 176 backend programs emit
byte-identical IR, the 176th differing by exactly that body), the fixpoint is
declared under D-202, and **the snapshot is refreshed from the adopted tree**
(stage2 == stage3, 15,450,688 bytes). The refresh's dry run found that an
absolute `src/npkc.npk` argument embeds the build path into 1,489 of 1,647
site-table rows — D-078 held by one README line — so the `repro` stage now
refuses an absolute site path in the committed snapshot, and whether the
source manager should record manifest-root-relative paths is **S-7** (the
user's). SUBSET_1 §4 carries its closing edit.
**1.4.7b IS COMPLETE** (`meta/roadmap/done/1.4/1.4.7b.md`): the close's
recommendations were ratified as **D-234** (a `for` captures its bound, a
`while` re-reads it), **D-235** (every kind decided as a channel element:
simd and function values ride, the sync primitives, atomics and arenas refuse
permanently) and **D-236** (manifest-root-relative source paths); D-235 and
D-231 landed, the tfp fold runs in one `uint512`, and D-228's width is
calibrated at 6. D-236's implementation and D-230's `TY_FLAGS` were re-homed
to 1.4.8's open beside the layers they need.
**1.4.8 (`npkg`, D-206) IS UNDERWAY** (`meta/roadmap/done/1.4/1.4.8.md` carries the
execution record and the order). **Part A landed**: the runtime's driver
clone is ONE primitive for every supervised child — `clone_exec`, a ten-word
block with the child's 0/1/2 sources, an optional ctrl fd, and the "every
child-bound fd ≥ 4" rule CHECKED by the runtime (`-EINVAL` before any slot is
claimed) instead of trusted; `spawn_driver` is a caller of it. `environ()` is
a floor builtin (`_start` measures argv and envp with one builder; no syscall
returns the environment and `npkg` needs `PATH`). `lib/nsys.npk` holds the
shared syscall vocabulary (REACH is import-scoped — the tool runner could not
import the Bridge for six constants), `lib/nproc.npk` is the tool runner
(`proc_spawn`/`proc_wait`/`proc_reap`: both pipes captured, every wait
bounded, a deadline kills-reaps-retires), and `proc_tool.npk` proves capture,
the environment passed through, a missing tool's 127 as the CHILD's answer,
and a hung tool killed at a deadline — exiting 0 so D-151/D-188 assert nothing
leaked. The user settled D-230's families and S-8 on 2026-09-02 (the
recommendations as written).
**Steps 2b–6 landed (2026-09-02), validated under D-228's cumulative-prefix protocol after a mid-step-6 UI freeze the recovery lost nothing to.** `range<T>` is spellable (2b, S-8/D-093); the snapshot was refreshed mid-cycle so `src/` and the library it imports may spell the flag families (3); every `open` caller crosses its `oflags`/`fmode` to the floor's word with `=> int32`, `open` itself staying `int64` — the floor is the syscall surface (4); `lib/nfs.npk` is the file-system surface — a sorted listing over `getdents64`, containment answered by OPENING not by string checks, restrictive creation defaults and the at-family (D-213's three riders), with `sys_cwd` and three named errnos in the prelude (5); and **D-236** renders every source path relative to the manifest root the driver finds by walking up from the main file, so the `selfhost` stage's new assertion measures H9 green — zero absolute site rows where 1,479 of 1,637 leaked before (6). Four full harnesses (main plus three cumulative-prefix worktrees) came back 58/58; main is byte-identical to the fully-merged `w456`, and a confirmatory harness on committed main followed. **Part D LANDED (2026-09-02): `npkg/` exists** — twelve modules of Nitpick built by the compiler under test, over the compiler's own path code, list and lexer. `npkg build` runs the README's ladder and produces a compiler BYTE-IDENTICAL to the harness's; `npkg test` builds, runs the runner self-check (§7.1, also `--selfcheck` alone), then every suite the harness runs unit for unit — 908 verdicts on the first full run, every suite count the harness's — with `--only`, `--verdicts PATH`, and `update`/`verify` refusing by name. The undefined-symbol scan reads the object's ELF64 symbol table itself (`npkg/elf.npk`) rather than spawning an unpinned `llvm-readelf`; the harness keeps spawning it and the new **`parity` stage** builds `npkg`, runs `npkg test --verdicts` from the manifest root, diffs the two verdict lists unit for unit and byte-compares `build/npkc` — every per-file harness site now records a verdict. Three things the port found: BUILD_REFERENCE §7.1's "unexpected diagnostics fail a test" is a rule NEITHER runner enforces (subset matching since 0.8; 17 of 131 rejection files carry unasserted extras — **S-9**), nine of those seventeen were `tools/resolve_check.npk` never naming the prelude module (fixed), and the tool runner's capture was quadratic (a `string_concat` per 8 KB read; `npkg test`'s first full run spent 17 of 56 minutes in the kernel — `lib/nproc.npk` accumulates linearly now, and `proc_wait` CONSUMES its `Proc`, which D-004's conservative borrow rule required for the captured text to leave the frame). Whether every suite should be a manifest `[[test]]` entry is **S-10**. Both runners run until `meta/SWITCH.md`; the harness remains the run whose result means the suite is green. **The concluding harness run on the final tree: every stage green, 58/58, and the `parity` stage's first result — 902 verdicts agree between the two runners, npkc byte-identical.** 1.4.8 is closed; S-9 and S-10 were ratified the same day as **D-237** (exact diagnostic matching in both runners) and **D-238** (every suite a `[[test]]` entry with a `stage`, one table both runners read), and land as **1.4.8b** (`meta/roadmap/done/1.4/1.4.8b.md`, execution-grade).
**1.4.8b IS COMPLETE (2026-09-02)**, two commits each under a full harness with
`parity` green. **D-237**: on the error channel the SET of codes a rejection test
reports must EQUAL the set its expectations name — `check_module_rejection`
(harness) and `check_rejection` (npkg) fail every unnamed code by name, the
runner self-check's `unasserted-extra` case proves the rule bites (a negative
control shows the old subset rule accepting it), and 131 of 131 rejection files
pass under it after the eight resolutions. Two of the eight contradicted their
pre-settlement on reading: `definite_assignment.npk` MEANS its `PICK-003` (named,
not wildcarded away), and `assoc.npk`'s `TYPE-014` was a stale-text collision —
its default assoc was named `Error` at 1.0.6, D-179 later made `Error` the
compiler-known type resolved by name ahead of every lookup, and the checker read
the word two ways (the builtin in signature comparison, the trait's assoc in the
object-safety walk); the test spells `Fault`, and whether `Error` joins the names
a program cannot declare — a module-level `struct:Error` is ACCEPTED today where
`struct:Duration` is refused, and an `assoc` shadows a prelude type inside its
trait — is **S-11**; three sites where two rules report one mistake are **S-12**.
**D-238**: every suite is a `[[test]]` entry with a `stage` (`compile` with its
`kind`, `parse`, `resolve`, `check`, `accept`, `fixture`, `program`, `runtime`),
`paths`/`path` and `recursive`; both runners read the one table and refuse an
entry they cannot honour BY NAME before anything runs; the hardcoded loops are
gone from both (the harness dispatches from `run_stage`, npkg from
`run_targets`, each stage a function, each tool built once on first use); the
before/after verdict lists differ in exactly the two recorded ways — the six
duplicated grammar units collapse and the two `nf_twin` twins the old
non-recursive glob missed are swept — and nothing is judged differently.
**1.4.8c IS COMPLETE (2026-09-02)**: S-11 and S-12 ratified the same day as
**D-239** — a name the compiler (`Error`) or the prelude owns cannot be declared
by a program at ANY type-namespace declaration, associated types and generic
parameters included, refused by the loader under RESOLVE-001
(`owned_names.npk`: six shapes; a module-level `struct:Error` had been ACCEPTED
where `struct:Duration` was refused, and an `assoc:Duration` shadowed the
prelude inside its trait) — and **D-240** — one mistake, one report: the old
blanket spelling is recognised by one probe so TYPE-002 never joins TYPE-012,
`drop` over a bare non-`Result` is TYPE-007 alone, and a refused `..^` argument
is not also fit-checked. Refusal-only: the compiler's emission of itself is
unchanged.
**1.4.9 CLOSED THE CYCLE (2026-09-02): self-hosting is declared under D-202.**
The README's refresh, invoked relatively from the tree root, gives stage2 ==
stage3 at 15,631,627 bytes (sha256 `9ce0ec8d3de5b2c83da4a1f11d3f89965728f6cf938f70042ea053eff5defaaf`) from the final tree
(`80784f3`, whose `src/` is 1.4.8c's) — installed as the snapshot with its
STAMP, so the committed builder IS the fixpoint text — and the harness's
`selfhost`, `repro` and `parity` stages are green on the same tree (906
verdicts agreeing between the two runners, `build/npkc` byte-identical). The
close synced the docs (the D-233 doc-sync batch drained, G-2 recorded as
settled by D-231, the width measurement into ORCHESTRATION §4), retired
HANDOFF.md into ROADMAP's and the 1.4 README's "What cycle 1.4 taught" (every
part re-homed; `1.4.9.md` has the map), and archived the folder. **Next: cycle
1.5 (verification)** — its batch D-217…D-221 is ratified,
`meta/roadmap/1.5/README.md` is the map (its opening says where to start), and
1.5.0 is the skeleton: the SMT-LIB2 writer, z3 spawned through `lib/nproc.npk`
under the determinism profile, the obligation manifest, `TCB.md` drafted.
**1.5.0 IS COMPLETE (2026-09-03)**: the pipeline is real — obligations as
SMT-LIB2 text, the pinned z3 one process per function under the determinism
profile, `nitpick.obligations` committed (141 rows, 116 discharged), `llvm.assume`
elision, the D-007 division pair proven end to end, the verified compiler
rebuilding itself, the `undef` ban a check, TCB.md drafted.
**1.5.1 IS COMPLETE (2026-09-03) — the verification surface TYPES**
(`meta/roadmap/1.5/1.5.1.md`, five steps, each under a full harness). A
`limit<name>` RESOLVES at all three sites (a typo is the identifier's own
RESOLVE-002, a non-`Rules` name RESOLVE-011, a refinement cycle RESOLVE-006,
and the resolved rule is written onto the node); a `Rules` body types over `$`
(the same node as a counted loop's counter: the value under consideration),
every clause a `bool`, and a limited binding's declared type is the rule's
subject BY IDENTITY (TYPE-059 — no widening, no type parameter). Every
proposition is a `bool` — `requires`, `ensures`, each `invariant` conjunct,
`prove`, `assert_static` — and a contract expression admits only what a
proposition can evaluate anywhere (TYPE-060): no `await`, `move`,
`relay`/`?!`/`?|`/`drop`, `pick` expression, store or manufactured view, and a
call only to a NAMED `never fails` `pure` function (`raw f(…)`; a builtin bare
by its Fails and Pure columns) — a function value, a field or a `dyn` is
refused because 1.5.3 encodes a contract call as an uninterpreted function per
KNOWN symbol. **Five decisions landed with it**: D-241 (D-163's contract row
retires — `never fails` may carry `requires`/`ensures`/`limit`, since D-221's
trap route is the channel a never-fails body already admits), D-242 (purity is
DECLARED: `pure` is a contract clause checked in the body — TYPE-061: no
`async`/`thread`, no `move` parameter, no impure/indirect callee, no
shared-state receiver, no `wild`, no owning local, no store the caller sees —
and every builtin row carries a `Pure` column generated into `builtin_pure`;
purity never rides a function type), D-243 (`old(expr)` is a keyword operator
with its own node: the value at the function's entry, in `ensures` and
`invariant`, never nested, only of a copyable value), D-244 (`main`/`failsafe`
carry no contract), D-245 (`result` is a keyword with a leaf node — a
parameter could shadow the identifier it used to be, inside its own
postcondition). **Found**: macro expansion SHARED its verify nodes across
expansions, so the last expansion resolved won — invisible while the backend
refused the family, a miscompile the day 1.5.3 lowered a contract in a
macro-emitted function; `clone_verify` now. S-13 closed (parity fails on any
nonzero `npkg test` exit). No obligation rows moved (141), no rung retired
(1.5.2/1.5.3/1.5.4 own them), no snapshot refresh (frontend only; `src/`
adopts none of it until a snapshot understands it — D-205).
**1.5.1b IS COMPLETE (2026-09-04) — the workbench's findings, fixed before
planned work** (`meta/roadmap/1.5/1.5.1b.md`; nine landings, each a
cumulative prefix validated by a full harness, D-228). The floor keeps four
allocator words and prints them under `NPK_HEAP_STATS`, and the `cost` stage
runs in both runners over `tests/cost/` (step 0); every file's first
declaration is its header and the entry points are the root's — `main`/
`failsafe` outside the root refuse, 243 files gained `mod:<basename>;`
(D-248, step 1); a root with `main` and no `failsafe` refuses at `main`,
REACH-003 (step 1b); a view-maker's result BORROWS its operand, by the
reference's `Views` column, and a view of a temporary is BORROW-012 (D-249,
step 2); the builders write into a `Sink`, each byte once, and the three
cost axes hold their bound (step 3); derived comparisons follow the operand's
SPELLING, DERIVE-006 (D-250, step 3b); a unit without `failsafe` declares
`@npk_failsafe` and compiles to an object — the `object` stage in both
runners — and `pub use` re-exports after a plain `use` (step 3c); an owning
value no place takes is a temporary of its statement, dropped when it ends
and on every exit, frame-resident across `await` (D-246, step 4); and
`List<T>` is compiler-known and OWNING, move-only, and lives in the PRELUDE
with its functions since step 5b's bridging refresh (D-247, step 5; S-25 as
recommended, pending the user). **Step 5 found five
defects on the way, all fixed in it**: `pass h.n` cleared the root's drop
flag for a COPYABLE field (an owning local returned by a copyable field leaked
since 1.2.3); every descriptor-exhaustion proof in the suite depended on the
session's soft descriptor limit, so both runners now stand under
`nitpick.toml`'s `[limits] nofile`; a `move` out of a FIELD dropped the
moved-out value at the field's next assignment (the resolver's own constant
folder double-freed, masked by `exit`) — a partial move now leaves the type's
vacant value and the aggregate stays live (S-26); three unit tests released
the heap and then RETURNED from `main` — TYPE-062 now requires `exit` after
`wild_release_all()` (S-27); and the trap route after a release died because
the main thread's TLS block was heap memory — a raw mapping now. The close-out
refreshed the snapshot and set `tests/cost/self.toml`'s ceiling; its first
harness found the diagnostic sort reading slots it had moved out of (DEF-13,
step 5c: byte identity of the fixpoint is not behavioural identity — the
refresh's own harness is the proof, and the seed README says so).
**1.5.2 (`limit<Rules>` live) IS COMPLETE (2026-09-04; `meta/roadmap/1.5/
1.5.2.md`, planned and closed the same day, every question ratified the day
it was asked — S-24…S-30 as D-251…D-255, a D-247 note and the README's
1.5.4b row; six landings, each a cumulative prefix under a full harness,
D-228).** **Step 0 was DEF-14**, a soundness defect found by planning: the
1.5.0 encoder gave an address-taken local a stable symbol and withheld only
its definition, so a definition of ANOTHER local in terms of it outlived a
call that wrote through the pointer, z3 discharged a `div-zero` row the
program then defeats, and the elided build died with a floating-point
exception where the plain build reaches `failsafe` — proven end to end on
`8dbef43` with a twelve-line program. An escaped name is never NAMED now
(every read a fresh opaque term, the escape set computed before the
parameter loop); the compiler's own 141 rows, re-decided with both compilers
and joined by ordinal, moved not one verdict. Then: TYPE-063 (a limited
binding has no address — `@`, `$$m`, `$$i` and a move out of an owning
sub-place refuse) and TYPE-064 (a `limit` where no write point exists — a
trait signature, `wild`, `comptime`; `main`/`failsafe` under D-244) with
`LimitViolated` (−4111) armed by REACH, and the runner self-checks' rung
example moved to `prove`; the check in EVERY build — one generated `i8`
predicate per `Rules` (`src/backend/ir/ir_rules.npk`), the check AFTER every
write over the binding's whole value (initialiser, every assignment to it or
any part of it, the callee's entry — sync and coroutine), the site key's
SPACE (expression/statement/declaration), the rung retired; the `limit` rows
encoded with the rule as a HYPOTHESIS on every later version of the binding
— `limit_loop.npk`'s `div-zero` after a loop is the first cross-kind
discharge — `limit-subsume` rows at every direct call of a sync callee (the
checker records a callee only for method calls; a free call's is its
symbol), elision into ONE `llvm.assume` over the rule's range clauses, both
runners kind-aware with the `none` elision word, the self-checks' `wrong-`/
`right-limit`; the caller-side bypass (D-252: a sync function with a limited
parameter is `<symbol>.body` plus its ordinary symbol as the checked entry,
`.body` inside the quotes; a direct call whose row is discharged names the
body; the belt counts defines, tail calls and direct calls in both runners
— over the SYMBOL text, since the code-only text blanks quoted names and the
step's first harness found the belt counting nothing — with the
`bypass-counted`/`-missing`/`-as-value` cases in both self-checks);
the docs. `nitpick.obligations` never moved. **1.5.2b (derived impls over generic subjects) IS COMPLETE
(2026-09-05; `meta/roadmap/1.5/1.5.2b.md`, planned, ratified — D-256…D-259 — and
closed the same day; six landings, each a cumulative prefix under a full
harness, D-228).** Step 0: `DECL_THREAD` is 512 and `check_decl_flags_unique`
refuses a shared `DECL_*` value, a value that is not one bit, or a row it
cannot read (L-12). Step 1 (D-256, DEF-15): a family impl APPLIES to an
instance only when its target pattern unifies with it and every bound holds —
ONE unifier (`family_unify`, `type_trait.npk`) read by `find_method`,
`type_implements`, `bind_blanket`, the `dyn`-coercion lookup and the emitter's
`note_family_instance`; the measurement found the parameters had been bound
POSITIONALLY on both sides, so `Pair<U, T>`, `Pair<int32, T>` and
`Box<Pair<T, int32>>` — admitted at their declaration since 1.0.4b — now run;
the call site reports TYPE-017 naming the impl (or the derive), the parameter
and the bound; every overlap report carries a note at the earlier impl. Step 2
(D-257): the prelude's `scalar-impls` GENERATED region — 348 rows in thirteen
families from LEXICAL_REFERENCE's BuiltinType production, `Debug` exactly
where a `ToString` exists, `Hash` for the mechanical rest of the ladder,
`dim256` per DISTINCT unit vector (per NAME the prelude refused itself:
`Hertz`/`Becquerels`, `Grays`/`Sieverts`, `Candela`/`Lumens`, and
`Radians`/`Steradians` ARE `tfp256`), `string`'s `Eq`/`Ord`/`PartialOrd` by
hand, `gen_tables.py --check` and the harness's `check_generated_current`;
found: the tier scalars' method intercepts (`tfp`, `dim256`, the ternary
family, `complex`, `simd`) refused EVERY name outside their fixed sets in the
checker AND the emitter (whose `emit_tfp_method` lowered `to_string` as
`floor`), so the region could exist and never be called — both sides now send
a name outside the type's own set to the impl table through one predicate per
kind. Step 3 (D-258; DEF-16, DEF-17, DEF-18 closed): `dv_class` reaches every
member through the trait being derived — a scalar through the prelude's impl
(`raw`), a named type or parameter through its own (`relay`), a `string`
field through the prelude's four, a `simd`/pointer by `.any()`/address under
`Eq` and copy under `Clone`, the rest refused by name (DERIVE-006 per derive)
— the head synthesized on exactly the parameters the body reaches,
`Clone` member-wise, `Debug` through `debug`; found and fixed: the emitter's
`emit_tostring` could not lower an interpolated bound parameter or a
family-impl `ToString` (EMIT-002 where the checker admitted). Step 4 (D-259):
every generated line goes through one sink recording (subject, trait,
member), `front_run` re-homes every diagnostic in a `<derived-N>` file to the
derive's declaration with the derive and the member named, both runners refuse
a `<derived-` path with `derived-path`/`derived-path-control` in both
self-checks, and D-240 widened: a `raw` over a refused operand is not asked a
second question (six unit cases counted the pair since 1.1.2). **Recorded for
the user (OPEN_DECISIONS §2f)**: DEF-19 — a `pick` over an `Optional` enum
with variant patterns is admitted by the checker and refused by the emitter
(`pick (o ?? Ordering.Equal)` is the suite's spelling); DEF-20 — a GENERIC
ENUM parses and means nothing (no `T` in its payloads resolves, a variant
constructor is the bare enum), never decided in or out. Measured and
recorded, not failures: the compiler's own IR grows 2.2% (every prelude impl
body is emitted whether reached or not) and the frontend over `src/npkc.npk`
takes 14% longer; `nitpick.obligations` never moved. **The two findings were
ratified the same day as D-260 (a `pick` does not select on an `Optional`,
TYPE-065) and D-261 (generic enums are IN, a family as a generic struct is)
and landed as 1.5.2c.** **1.5.2c IS COMPLETE (2026-09-05;
`meta/roadmap/1.5/1.5.2c.md`, planned, ratified and closed the same day; three
landings, each a cumulative prefix under a full harness, D-228).** Step 0
(D-260): a `pick`'s selector may not be an `Optional` — TYPE-065 at the
selector, statement and expression form, before the arms are read; `pick (o ??
default)` and `== NIL` are the spellings. Step 1 (D-261): a generic enum is a
family — `bind_instance` is `pub` and binds a struct's OR an enum's window, and
every payload-type read goes through it (the layout per instance, so
`Opt<string>` owns and `Opt<int32>` does not; the pattern bindings; the
constructor's fit; the emitter's `variant_payload_slot_at`, which construction,
`pick` binding and the drop body share); `enum_instance_for` supplies a
constructor's or a bare variant's instance — the expected type, else inferred
from the payload by `unify_into`, else TYPE-022 naming the parameter — built
through `make_instance` and recorded so `check_instantiations` judges it; the
emitter substitutes a generic body's enum at `emit_ctor` and through
`pick_sel_tid` at every `pick` reader. **Found on the way, fixed in step 1**:
the `pick` EXPRESSION form typed NO arm binding — only the statement form
called the binding typer and the lending-form refusal — so `give t.x` over a
bound `t` was accepted unchecked and died as EMIT-002 on a PLAIN enum, a struct
given where an `int32` was expected passed the fit check against nothing, and
an owning payload could be copied out of a lending pick expression (1.4.3b's
hole, open in one of the two spellings); the statement form's whole prelude is
ONE function now, `type_pick_rules`, called by both forms
(`pick_expr_bindings.npk` pins the four rules that became live). Two `pick`
binding sites in `ir_stmt.npk` that resolved the payload node by hand read
`variant_payload_slot` now. Step 2: the docs. `nitpick.obligations` never
moved. **1.5.2d IS COMPLETE (2026-09-05; `meta/roadmap/1.5/1.5.2d.md`; the
library workbench's S-38, ratified the same day as D-262; four landings, each
a cumulative prefix under a full harness).** The workbench measured a fixed
per-program cost at the 1.5.2c close — a floor-only program at +388,765 bytes
of IR, 0.10 s → 0.85 s, 21 MB → 102 MB, constant to the byte across its
programs — and the measurement D-262 asked for first put the frontend's share
(88%) in three SCALING DEFECTS, not in the prelude's size: the bindings
analysis allocated one state slot per statement and declaration OF THE WHOLE
PROGRAM for every function and copied it at every branch (57% of the run), and
the type table and the string interner deduplicated by linear scan. Step 1: the
resolver hands each parameter and local a dense per-function ordinal
(`SymbolTable.local_ord`/`stmt_ord`/`fn_slots`), `state_new` is sized by it,
and both tables carry hash indexes — the probe's frontend 0.72 s → 0.07 s and
96 MB → 4.4 MB, the compiler's own build 242 s → 20 s with its peak 13.4 GB →
113 MB; every rejection file reports the same set and every program compiling
no changed source emits byte-identical IR. Step 2 (D-262 §1): AN UNREFERENCED
PRELUDE ITEM IS NOT EMITTED — the writer records each non-generic prelude
function, each prelude impl's methods and vtable and each prelude generic
instance as an item, and `irw_trim_items` keeps an item only if a symbol it
defines is referenced from outside every item or from a kept item, a fixpoint
over the emitted TEXT (a reference is an `@` token, so no kind of use is
enumerated and an unanticipated one keeps its item); the probe's IR 845,283 →
50,561 bytes and 608 → 14 functions, `llc` 0.46 s → 0.02 s, the compiler's own
IR keeps 101 of 685 prelude functions, all referenced. Found on the way: the
first prelude function's item straddled the head-to-tail sink switch; the
prelude's generic instances are recorded by call sites in dropped bodies and are
items too; `src/frontend/prelude.npk` shared the language prelude's qualifier
and is `prelude_names.npk`; the elision cross-check counts a row only for a
function the emission holds. The belt (`check_prelude_trimmed`,
`ir_prelude_trimmed`, self-check cases in both runners) runs on every module the
compiler under test emits and NOT on the snapshot's (the tools, `npkg build`'s
`npkc.ll`, the runner self-check's cases). Step 2b (DEF-21, the workbench's
finding): the undefined-symbol allowlist is the runtime's EXPORTS — the 57
`internal` defines out, the `module asm`'s two `.globl` names in. Step 3: the
docs; `self.toml`'s ceiling tightened to the new peak. Step 4 (the workbench's
O-N17, found during the subcycle): a generic function moving OUT of an indexed
element at an owning `T` links -- `emit_move_out` builds the vacant helper's
symbol from the place's type THROUGH the specialization, and a registered drop
type the emitter cannot lower is EMIT-002 instead of a body silently not
emitted. Found writing its test and recorded as **S-39** (the user): an owning
`List<T>` local alive in `main` at `exit 0` is a `WildLeak` by construction --
`exit` runs joins and defers and no drops (D-183's amendment), and a List's
buffer is the one managed storage D-151 counts. `nitpick.obligations` never
moved. **1.5.2e IS COMPLETE (2026-09-05; `meta/roadmap/1.5/1.5.2e.md`; three
landings under full harnesses).** S-39 ratified as **D-263**: the prelude's
`List<T>` stores through `alloc_managed`, the managed heap's untracked entry,
PRELUDE-ONLY by the reference's `**Prelude-only**` marker (generated into
`builtin_prelude_only`; TYPE-054 from any other module, because a hand-written
`wild` container relies on D-151's count as its enforcement of an unpaired
free), `ralloc` keeping a block's role — a `List` alive in `main` at `exit 0`
exits 0 where it exited 94, and the exit path stays free of the drop walk as
D-183 decided. DEF-22 (the workbench's O-N18): `.len` on a fixed-size array
lowers to the constant its type carries. **1.5.2f IS COMPLETE (2026-09-05;
`meta/roadmap/1.5/1.5.2f.md`; two landings under full harnesses).** S-40, the
workbench's O-N19, ratified as **D-264**: a bare type parameter — and `Self` in
a trait's default body — is MOVE-ONLY in the body that names it, because a
generic body is checked once for every type it is instantiated at;
`type_owns_for_move` is the one predicate, `require_move_if_owning`, the
lending-pick binding check and the derive generator ask it (TYPE-046 with the
parameter's own reason; DERIVE-006 for a `T` payload under `Eq`/`Ord`/
`PartialOrd`/`Clone`, as for a `string`'s — `Hash`/`ToString`/`Debug` bind
nothing and still generate). Before it, `T:x = s[i]` at an owning `T` compiled,
linked and ran with two owners of one heap body — a use-after-free that exited
0. Seven sites in the tree, all a by-value `T:v` stored into an owning slot,
say `move T:v` and `move(v)` now; nothing else refuses anew. **Left open:
S-41**, a borrowing `pick` binding form — settled the next day as D-266
(1.5.2h). **1.5.2g IS COMPLETE (2026-09-06;
`meta/roadmap/1.5/1.5.2g.md`; two landings under full harnesses).** S-42, the
library workbench's first CI finding — the pinned commit's `build/npkc`
differed between this machine and GitHub's runner while `npkrt.o` did not —
ratified as **D-265**: D-204's toolchain pin is a VERSION and stays one (a
tool-binary digest would refuse every machine but one; z3's digest pin,
D-218.1, is the deliberate asymmetry — a solver's output is a committed
verdict, a toolchain's is checked bytes), the claim that holds across
machines is the EMISSION (`build/npkc.ll`; a difference there is a compiler
defect), every `npkg` ladder run prints one `sha256` line per intermediate
in ladder order with the harness's `parity` stage holding each to an
independent digest, and every pin notice quotes the lines with the
emission's named. **1.5.2h IS COMPLETE (2026-09-06;
`meta/roadmap/1.5/1.5.2h.md`; three landings under full harnesses).** S-41
ratified as **D-266**: a lending `pick (v)` binds VIEWS — each binding the
payload in place, read-only, typed as itself, no copy at the bind, no address
(TYPE-066: an assignment, `@`/`$$i`/`$$m`, a pointer-receiver call, a stateful
operation, and no view of a stateful kind at all), the selector's root FROZEN
inside an arm that binds (TYPE-067), a `move` or `pass` of a view TYPE-047; the
consuming form unchanged. One fact recorded once: the resolver links a pattern
symbol to its selector and `sym_is_view` reads the spelling. The binding's slot
holds the payload's ADDRESS in sync and coroutine bodies alike, and the
selector's root is frame-resident when an arm binds — a view across an
`await` is sound by residency, as `@x` is. The derive generator's `Eq`/`Ord`/
`PartialOrd`/`Clone` generate over `string` and `T` payloads again. **DEF-24**,
found by planning: TYPE-063 refused `@`/`$$i`/`$$m` on a limited binding but
not the implicit address a pointer-receiver method call takes — a limited
struct written through `Self->` with no trap — fixed at step 0, the site views
then mirror. Found on the way: `drop` over a refused operand reported TYPE-042
as a second sentence; it has the `raw` arm's D-240 short-circuit now.
**1.5.2i IS COMPLETE (2026-09-06; `meta/roadmap/1.5/1.5.2i.md`; two landings
under full harnesses).** DEF-25, the library workbench's report: `string_concat`
of two empties allocated a real 16-byte block (D-150's answer to a zero
request) and returned it with cap 0, so its drop never freed it — 16 bytes per
empty call since the primitive was written, the prelude's `string:Clone` of an
empty string and the compiler's own `string_concat(x, "")` copy idiom
included; the runtime's concat takes the branch `string_slice` has carried
since D-186, and `tests/cost/empty_concat.toml` holds the empty loop's peak to
the one-byte loop's (a factor of millions on the old runtime).
**1.5.3 (contracts live) IS COMPLETE (2026-09-06; `meta/roadmap/1.5/1.5.3.md`;
S-43 and S-44 ratified the same day as D-267 and D-268; four landings, each a
cumulative prefix under a full harness, D-228).** Step 0: the three trap
identities (`RequiresViolated` −4112, `EnsuresViolated` −4113,
`InvariantViolated` −4114), REACH arms each where its construct is declared,
a `failsafe` `exit` whose literal is not positive is REACH-004, `old(...)`
names the enclosing function's own parameters (D-243 read literally). Step 1:
CONTRACTS ARE CHECKED IN EVERY BUILD — a `requires` in the callee's `.req`
predicate (the function's parameter list verbatim, one trap per clause at the
clause's site), called from the checked entry of a sync function, which now
splits for a `requires` as it does for a limited parameter, or at state 0 of a
coroutine; an `ensures` at every return seam (`pass`, and the long form on a
success) with `result` the value in register and `old(...)` a snapshot at the
body's start (an alloca, or a frame slot at role 60+k); a loop `invariant` at
every head of every loop shape; `failsafe`'s exit code held positive at the
`exit` (the re-entry rule ending the process at 70); LIVE-1's four rungs
retired. Step 2: CONTRACTS ARE PROVEN WHERE THEY CAN BE — a `requires` row at
every call with a recorded callee (`bypass` at a direct sync call, `held` at an
`await` or through a `dyn`: recorded, never elided) and at every function's
entry, an `ensures` row per return point, a loop's entry, preservation and
`continue` rows with the invariant and the condition hypotheses inside the
body and the invariant a hypothesis after a loop nothing `break`s (the
prelude's `npk_gcd256` division discharged through its guard), `failsafe`'s
postcondition per `exit`, conformance rows in a space of their own, a `pure
never fails` callee an uninterpreted function and a callee's `ensures`
knowledge at every success-only unwrap; `rows.txt` carries each row's site,
role, group and traps, both runners' belts count sites and traps BY GROUP and
spell the manifest word by role; `nitpick.obligations` re-recorded at 178 rows
(the compiler's own 37 `failsafe-post` rows). Step 3: the docs. Found on the
way: the rung test `inline_mod.npk`'s hidden construct was a `requires` (a
`prove` now); every verify test names its `failsafe`'s rows, since every
`failsafe` has them (`expect-obligation: none` names no program any more).
**1.5.4 (path conditions, the counters, `prove`/`assert_static`) IS COMPLETE
(2026-09-06; `meta/roadmap/1.5/1.5.4.md`; six landings, each a cumulative
prefix under a full harness, D-228; S-45…S-48 landed under their
recommendations and were ratified 2026-09-07 as D-269…D-272).** Step 0 fixed five
findings: DEF-26 (a division inside a pick EXPRESSION's arm had no
obligation row since 1.5.0 — the arms were never walked; one enumeration,
`stmt_expr_ids`, now feeds every walker that must see into a statement's
expressions), DEF-27 (a callee parameter sharing a name with an address-taken
caller local bound unnamed in the contract substitution), DEF-28/DEF-30 (the
counted loop's head: a literal zero step compiled and trapped at run time
where D-022 promised a compile error, and the argument count was never
checked — `NITPICK-TYPE-068`), DEF-29 (the compile-time evaluator's counted
loops took their direction from the step's SIGN and read `till(limit, step)`
as `loop(lo, hi)`: a comptime descending sum was 0 where its run-time twin is
55). Step 1: PATH CONDITIONS — a branch condition is a hypothesis in its arm,
its negation after an arm that never falls through, a `pick` arm's pattern
(encoded under the SELECTOR's type: the checker types no pattern for its own
sake) and guard, a loop's negated condition after it, `when`'s two blocks; 13
of the compiler's 21 open rows discharged. Step 2: THE MERGE — an arm's facts
survive it guarded by its condition, versions merge as `ite` after `if`/
`when`/`pick`, a pick expression's value is its arms' `give` chain, `&&`/`||`
/ternary arms; `divz_after_branch.npk` keeps 1.5.0's promise; the walk 31.7 →
34.9 s on the compiler's own build. Step 3: THE COUNTERS — `$` and a range
`for`'s binding as terms (start, in-range body symbol with the residue class
of a numeral step, incremented at the preservation rows, the exit value after)
and the new kind `loop-step` for a computed step (S-46), a literal step
getting neither compare nor row. Step 4: `prove` lowers to nothing and is a
guard-less row that becomes a hypothesis after its site (S-48), the VERIFIED
build refusing an undischarged one with `NITPICK-VERIFY-001` (S-45);
`assert_static` folds at the frontend (`NITPICK-TYPE-069`); every `pick` and
`assert_static` is a `checker` row (`c` in `rows.txt`, tier `-`, word `none`);
a verify test's `expect-error` means the verified build refuses; the rung
suite retired (S-47) and both runner self-checks' negative construct became a
type mismatch the snapshot and the compiler under test refuse alike. Step 5:
the docs. `nitpick.obligations` at 184 rows (six `exhaustive checker`), 15 of
24 open rows discharged, none moved the other way. **Recorded for the user:**
DEF-31 — an inline module's members cannot be reached from the module that
declares it (`use nested.*;` is RESOLVE-002, `nested.fetch(...)` is
TYPE-007), found when the rung file became a positive program; which
spelling should reach them is a language question — ratified as D-273 and
landed as 1.5.4c. **1.5.4c (a module symbol means one thing, D-273) IS
COMPLETE (2026-09-09; `meta/roadmap/1.5/1.5.4c.md`; three landings, each a
cumulative prefix under a full harness, D-228).** The qualified call `m.f(x)`
is a DIRECT call of the member — an inline module's, an alias's, a nested
path's — through ONE walk (`namespace_path`, with the visibility rule at every
hop taken from outside), ONE direct-call typer (`type_direct_call`, factored
out of `type_call`) and ONE direct-call emitter (`emit_direct_call`, out of
`emit_call`); the node's `ns_decl` slot records what a path resolved to, and
every reader of a method call's receiver skips it when set (the 36 sites are
classified in the record). `nested.MAX` reads a binding, `?! nested.Boom` an
error constant (the reach analysis reads the record). `use nested.*;`, `use
nested.{f, Point};`, `use nested.f;`, `use core.math as cm;` bind a module
symbol's public names through the file forms' own binders — the only way an
inline module's TYPES are named from outside — a first segment naming no
module is RESOLVE-002 (R-1), a later one the module lacks RESOLVE-007, a
private one RESOLVE-003 from either spelling, `std` owned (RESOLVE-001); a
module symbol carries its scope wherever it is bound, so `helpers.f()` works
across files after `use "./h.npk".*;`, and order never matters by the
fixed-point import loop (R-2 without a second pass; the `tests/accept/` pair).
**Found on the way, fixed**: `Trait.method(recv, s)` passed no callee to the
argument check, so a `move` parameter never demanded `move(s)` through the
qualified spelling — the callee freed the string and the caller freed it
again; an async METHOD spawn (`drop j.run()`) armed no `DeadlineExceeded`
where the direct form did; a struct read through a module was typed invalid
with NO diagnostic and died as EMIT-002; the emitter wrote no type header for
a struct or enum declared inside an inline module; and the checker resolved a
body's type names inside an inline module in the FILE's scope, so a module's
own function could not name the module's own struct. **Recorded for the
user**: S-49 (a member-less `mod:name;` import binds a module symbol with an
EMPTY scope, so `name.f()` reports "no member" rather than reaching the file
— recommended: carry the loaded file's scope, as the alias does) and S-50 (an
error constant declared inside an inline module hashes under the FILE's name,
so its `failsafe` arm is `(file.Name)` and `(nested.Name)` matches nothing —
recommended: the file qualifies, and an arm's first segment is checked
against the module names the program knows). **Both ratified 2026-09-10 as
D-274 and D-275 and LANDED the same day as 1.5.4d** (`meta/roadmap/1.5/
1.5.4d.md`; four landings, each a cumulative prefix under a full harness,
D-228): a member-less `mod:name;` import carries the loaded file's scope —
`graph_load_file_module` records the entry it loaded and `graph_collect_all`
links the symbol to it LAST, before any import round can copy an empty scope
into a re-export — so `name.f()`, `use name.{g};` and a `pub mod:name;`
across files are one meaning with the alias; and every `pick` arm over an
`Error` is checked at the arm by the checker (`type_pick_rules`, both
spellings, `failsafe`'s and any other): an identity no declared constant
hashes to is RESOLVE-002 with the fix named (R-3, the PAIR check —
`(nested.Boom)`, `(file.Nosuch)`, `(nosuch.Boom)`, `(x.DivByZero)` all
matched nothing silently before), any other shape TYPE-007 (R-4, where each
died as EMIT-002 in the emitter); ONE `error_identity` (intern.npk) for the
resolver, the emitter and the checker, and the resolver's (declaration,
qualifier, code) rows on the `SymbolTable`. **Found by the first test, fixed:
DEF-32** — the reach analysis recorded a constant's qualifier from the module
of the first SITE that reached it (the root, walked first), so a root that
unwrapped an imported constant was told to name `(root.E)`, a pair no code
hashes under, and the correct `(file.E)` was refused — the exhaustive-
`failsafe` guarantee hollow for cross-file constants named qualified since
1.1.6 (the bare `(E)` matches by symbol origin and hid it). **Found by
planning and ratified the same day as D-276 (S-51)**: a `use` or a
`mod:name;` written INSIDE an inline module bound and loaded nothing,
silently, while an inline module is sealed from its own file's imports — so
the one construct that could reach an import from inside one was a no-op;
the loader, the import pass and the file-module link are one walk shape now
and descend into inline modules under the same rounds and refusals as at
file level (`inline_imports.npk`). `nitpick.obligations` never moved.
**1.5.4b (the remaining theories, D-218 items 4 and 5) IS COMPLETE
(2026-09-10; `meta/roadmap/1.5/1.5.4b.md`; S-52…S-57 ratified the day they
were raised as D-277…D-282; seven landings — steps 0–4, 4b, 5 — each a
cumulative prefix under a full harness, D-228).** Step 0 (D-277): a shift's
amount is DEFINED for `0 ≤ n < width` and nothing else — a known amount
outside it is TYPE-070 at the shift, a computed one is one unsigned compare
trapping `ShiftRange` (−4115) and the `shift-range` row (the planning probe
had shown `shl` poison reaching LLVM unguarded; the folder had bounded every
width by 64); the snapshot refreshed by the seed README's bridging variant
because forty roots' `failsafe`s had to name the new constant. Step 1
(D-279, D-280): every bitwise operation is encoded — the Int forms wherever
an operand is a numeral (a shift by `k` as `mod`/`div` by `2^k`, the
low-bits, one-bit and cleared-low-bits masks, `~`), at any width for a few
hundred rlimit, and the bit-vector crossing (`int2bv`/`bv2nat`, exact by
construction) at words of at most `BV_CROSS_MAX_BITS = 64` — measured free
at 64 bits, 4% of the budget per row at 128, 18% at 256, the budget at 2048
— with THE GATE that no discharged row regresses held at every step; a flag
family is an unsigned 32-bit word to the encoder. Step 2 (D-278): the
twisted kinds are unbounded `Int` in the carrier's range with ERR = `MIN` a
VALUE the terms carry, every operation the emitter's own saturate-to-ERR
`ite`, and `err-exit` rows live at every `TbbErr` guard (a compare's
operands, a cast out under both spellings, a checked crossing into or
within a family) — the compiler's own manifest 147 → 368 rows (+213
`err-exit`, the prelude's generated impls), a twisted division rowless;
three defects found and fixed: **DEF-33, a soundness hole with two faces —
a guard's fact pushed under a QUIET encoding (a call-site contract row
proved `n < 32` from itself) and inside a LOUD clause (`requires (1i32 <<
n) != 0i32` was discharged of itself and its check elided)** — closed by
one rule that is the meaning of a proposition from here on: A PROPOSITION
HOLDS ONLY WHERE ITS EVALUATION DOES NOT TRAP (every guard met inside a
clause is conjoined into the proposition's term and pushed as a hypothesis
nowhere); DEF-34 (`smt_int` trapped on a negative numeral — the compiler
died under `--obligations` at the first negative numeral in any row,
reachable since step 1's `fixed` fold); DEF-35 (a negated literal pattern
refused by the emitter). Step 3 (D-281): floats in two tiers —
`flt32`/`flt64` as IEEE-sort terms with every operation the emitter's
instruction (`fp.add`… under RNE, `fp.sqrt` for `#sqrt`, the ORDERED `fcmp`
predicates, a literal `to_fp` of its exact decimal), the manifest's tier
column fed by the encoder (`int`/`bv`/`fp`/`-`; `real` for a tier-2
discharge — the constant `int` until now), and the Real-interval twin
(`NNNN.t2.smt2`, every float a Real within `ε·|v| + η` of the exact
operation) written only under three soundness conditions (every float
symbol bounded both ways by comparison hypotheses; every intermediate's
magnitude within the normal range CONJOINED to the goal; the goal a
comparison), asked by both runners only after a tier-1 `budget`:
`flt_tier2.npk`'s `#sqrt(a*a + b*b) >= 0.0` is `unknown` in QF_FP at the
rlimit and `unsat` in the twin — D-218 (5)'s shape, end to end. Step 4
(D-282): a `simd<T, N>` value is N scalar terms — through expressions AND
bindings (S-58, the reading recorded for the user) — and a `simd`
division's any-lane guard is ONE `div-zero` row (one `div-min` for a signed
element) over the lane conjunction, a shift's one `shift-range` row; the
`unencoded` producers of 1.5.0 retire — and the step's first harness caught
the vector emitter's guards ignoring the manifest (rows discharged, traps
kept; fixed: each elides into one `llvm.assume`, the belts being the
instrument that saw it where a check of rows alone was green). Step 4b (DEF-37): the reach analysis
demanded `DivByZero`/`DivOverflow` arms of every program whose only
division was a float one — IEEE-total, a bare `fdiv`/`frem`, an arm nothing
can enter; both sites read the float kind now, a `simd` division its
ELEMENT's. **Recorded for the user: S-59/DEF-38 — a `simd` integer lane's
`+ - *` lowers to a bare vector `add` and WRAPS where its scalar traps
(D-210), measured (a lane at `INT_MAX` plus one read back negative)**, and
DEF-36 (a program's `?! DivByZero` and a guard's trap share one text, so
the runners' belts count it — an `npk_raise` floor entry is the recommended
fix, D-203's). The compiler's own set: 368 rows in 197 function files,
decided in 3.9 s under the profile. **1.5.4e (the 1.5.4b close's three) IS COMPLETE (2026-09-11;
`meta/roadmap/1.5/1.5.4e.md`; ratified 2026-09-10 as D-283…D-285; three
landings, each a cumulative prefix under a full harness, D-228).** Step 0
(D-284): a `simd` integer lane's `+ - *` computes through
`llvm.{s,u}{add,sub,mul}.with.overflow.<N x iW>` — legal at every lane
count and width, measured through `opt -O2` with no libcall — the overflow
lanes folded to one any-lane test trapping `IntOverflow`, the intrinsic
declared once per module after the bodies (`llctx_note_decl`); an integer
`.sum()` folds through the scalar core and traps per step; the reach
analysis reads a lane's element kind. DEF-39 fixed with it: `v op= w` on a
`simd` was admitted by the checker and EMIT-002 in the emitter since 1.3.3
— `emit_arith_value` dispatches a `simd` operand to the vector binop now,
and the encoder records the compound form's any-lane rows at the target's
site. DEF-41, found on the way: a compound shift through a field or element
had its `ShiftRange` guard and no row (the field-target branch recorded the
division's and not the shift's), so a verified build carried a trap the
belts could not account for. Step 1 (D-285): `npk_raise` in the floor — one
call of `npk_trap` under the program's name, class `syscall`, an export by
construction — is where `?!` and `!!!` enter, every guard keeping
`npk_trap`; the belts count `@npk_trap(` alone and each runner's self-check
holds the pair; DEF-36 closes (`?! DivByZero` in a verified build), and
DEF-40 with it: `!!!` had called `npk_failsafe` directly — no frozen flag,
no re-entry guard, no driver kill — and `trap_stmt_reentry.npk` exits 70
where it exited 33 through a second `failsafe`. `npkrt.o`'s digest moved for
the first time since DEF-25's fix; the notice to the library listener named
both. Step 2: the docs. `nitpick.obligations` never moved.
**1.5.5 (the aliasing half of D-004) IS COMPLETE (2026-09-11; `meta/roadmap/1.5/
1.5.5.md`; planned execution-grade on `cb8cbb0`, S-60…S-62 ratified the same day
as D-286 and D-287; four landings, each a cumulative prefix under a full
harness, D-228).** Measured first: nothing decided aliasing — two `$$m` of one
element ran (exit 32), the spec's qualifier example never parsed (the operator
form is the language's) — and planning found DEF-42 (a borrow assigned to an
OUTER holder accepted; rule 3 unchecked for local holders, benign only by the
emitter's construction) and DEF-43 (a write through a pointer to a `fixed`
module binding was a SIGSEGV with no `failsafe`). Step 0 (DEF-42):
`check_holder_scope` in `escape_assign`'s bare-local branch over every root the
value carries. Step 1 (D-286, D-287): `src/frontend/analysis/alias.npk` — `$$m`
EXCLUDES, `$$i` SHARES, `@` claims nothing but is a write-capable access;
lifetimes LEXICAL (a call argument for its call, a pointer local's value for
the holder's whole scope; a `defer` body sees its blocks' claims; NLL and
two-phase borrows OUT); a claim only as a whole call argument or a pointer
local's whole value, a holder used and not copied (BORROW-014 elsewhere); the
conflict table by the paths' common prefix (BORROW-013 for a static overlap);
one forward walk per function, no fixpoint; TYPE-071 (a `fixed` binding, or a
`fixed` field, has no address) beside TYPE-063; the tree's cost five sites
(`diaglist_sort` had moved an element out from under its own live `$$i`). Step
2 (D-286 §5): a computed-index overlap is a RUNTIME GUARD in every build — the
byte-range compare at the access in `addr_of`, `BorrowOverlap` (−4116) armed by
reach — that the verified build elides through the `disjoint` row (kind 20;
`-4116` in both runners' tables; the party's index terms memoised when its claim
was encoded; one row per site): the spec's own example discharges under the two
rules' Int `%` forms. Step 3: the docs. Found on the way: step 0's root
collector skipped the custom-shaped kinds (a call's arguments, a literal's
values), and the compiler's own `failsafe` cannot name a new prelude identity
until a snapshot refresh (D-205). `nitpick.obligations` did not move; no
snapshot refresh.
**1.5.6 (the floor's spec and the executor primitives) IS COMPLETE
(2026-09-12; `meta/roadmap/1.5/1.5.6.md`; S-63…S-70 ratified the day they
were asked as D-288…D-292; six landings, each a cumulative prefix under a
full harness, D-228).** The floor had been the one part of the artifact with
no evidence of its own: hand-written LLVM IR, permanent by D-203, trusted
because nothing could check it. It now has a SPECIFICATION
(`runtime/npkrt.spec`: 137 sections stating what each symbol does to memory
— `requires`/`ensures`/`frame`/`objects`, loop invariants and exact
unrollings, `(summary)` calls, `(ensures-trap …)`, and a `(boundary "…")`
promise where the kernel is the answer), a TRANSLATOR that reads the floor's
own IR against it (`npkg/floor_smt.npk`), six PROTOCOL MODELS with sixteen
controls (`runtime/models/`), and an enumerated SYSCALL BOUNDARY. 370 rows
live in `runtime/npkrt.obligations`: 363 discharged, 7 residue, none
refuted — and **an `open` floor row is a run failure by name**, because
nothing in the floor is a guard to retain: a counterexample there is a
defect or a false claim, both stop-the-line. **Four floor defects were fixed
on the way, none found by a test**: D-290 promoted four plain cross-thread
accesses that race in LLVM's model (a frame's `windup`, a channel's
generation, `@npk_frozen`, the join's tid word); D-291 made a trap a
whole-program event as D-063 always said (a thread registry, a SIGUSR1 stop
handler, `cmpxchg` arbitration of the failsafe holder, the stop walk under
the join deadline — two threads trapping at once used to run two
`failsafe`s); D-292 gave `failsafe` a 1 MiB preallocated region to allocate
from, so a handler can never block on the heap mutex a stopped thread holds;
and **DEF-51** — `npk_read_file`/`npk_read_stdin` leaked every buffer they
outgrew, invisible to D-151 and to every test, found because a frame claim
would not discharge. TCB.md is FINALIZED: three generated regions (the
membership table with each symbol's class and disposition, the syscall
boundary, and what the evidence does not cover) that cannot go stale without
a red run. **S-71, RATIFIED by the user 2026-09-17 as an amendment to D-218
(2)**: the determinism profile carries `lp.dio=false` — z3 4.16.0's Diophantine sub-solver undoes its terms at
every `(pop)`, so a row that answered `unsat` in 8 s returned 200 s later
and a larger one not within 22 minutes, a wedged solver under P-13 with only
the runners' hang net to catch it; off, no verdict moves anywhere.
**1.5.6b (the floor's evidence, re-examined by measurement) IS COMPLETE
(2026-09-17; `meta/roadmap/1.5/1.5.6b.md`; planned execution-grade the day of
the s6→s7 hand-off, S-72…S-76 ratified the day each was asked as D-293…D-296
and an amendment to D-288; nine landings — steps 0–4, 4b, 4c, 4d, 5 — each a
cumulative prefix under a full harness, D-228).** The outgoing seat named four
places it would try to prove 1.5.6's evidence wrong, and the first, taken up by
MEASUREMENT, paid within the hour. Step 0, the stack rule: **DEF-52** —
`npk_hardware_concurrency` handed the raw `sched_getaffinity` an unzeroed
128-byte mask, the kernel wrote 8 bytes and returned the count (the zero-fill is
glibc's wrapper's, and this runtime has none), and the popcount over all
sixteen words answered 1008 on a 48-thread machine; **DEF-53** — D-173's
entry-block rule had never been pointed at the hand-written floor: eight of
fifteen allocas outside their entry blocks, two in loops (a loop-body alloca
SIGSEGVs at the pinned `-O0` and `-O2`). All fifteen sit in their entry blocks
and are fully DEFINED there, held by D-173's own check over the floor and a
new `alloca-not-defined` belt. Step 1: the kernel-effect table is ONE generated
authority (VERIFICATION_REFERENCE §9.2's `kernel-effects` region; four
hand-maintained lists became zero) and `kernel_effects.npk` holds every write
row to the RUNNING kernel, demanding equality — four things were wrong in the
table, one in the unsound direction, and `npk_hardware_concurrency`, specified
at last, has six rows of which exactly one is refuted on a floor without step
0's memset: the method would have found DEF-52. Step 2: `park-unpark` read
against `npk_park_sleep` case by case — a deviation, a MISSING STEP (the epoll
wait's empty return: the model's reachable states went 263 → 358, no verdict
moved) and a gap, the seventh model `reactor-io`. Step 3 (D-293):
`hardware_concurrency()` is a builtin — its own program exits 20 on the old
floor at its FIRST call. Steps 4 and 4b (D-294, D-296): a builtin's name is
the compiler's — a module-level function, an `extern` METHOD (its stub is one)
and any CALLABLE binding of that name are `NITPICK-RESOLVE-001`. Step 4c: three
defects of the function-value corner found by writing 4b's test (DEF-54: a call
through a pattern-bound function value emitted a symbol that does not exist;
DEF-55: `raw o.f(x)` refused where `raw (o.f)(x)` was accepted; DEF-56: a trait
method with a function-typed parameter could never be implemented). Step 4d
(D-295): **the models are read a SECOND way** — exhaustive explicit-state
search, no bound and no solver, a belt in both runners beside the solver's
rows, the twins byte-identical on twelve planted texts; three of seven models'
depths were smaller than their diameters, a model's meaning had had ONE reader,
and the belt's first finding was the runner self-check's own toy model,
commented safe and unsafe in two steps. TCB.md §5's eighth and thirteenth
acceptances are narrowed. `runtime/npkrt.obligations`: 379 rows, 372
discharged, 7 `budget`, 0 open; `nitpick.obligations` never moved; the floor's
bytes moved once.
**1.5.6c (what the floor's spec ASSUMES of its callers) IS COMPLETE
(2026-09-17; `meta/roadmap/1.5/1.5.6c.md`; approved by the user to run before
1.5.7's plan; five landings, each a cumulative prefix under a full harness,
D-228).** A section's `requires` and `(objects …)` are HYPOTHESES of its rows,
and nothing checks the callers no row covers. **Two were FALSE for a legal
caller**, neither a behavioural defect, both evidence that said nothing where
it looked like it spoke, and no solver could have said so — they were found by
READING: `npk_string_concat` assumed its two inputs apart (`string_concat(s,
s)` is in the tree twice; the proof never needed it) and `npk_small_free`
assumed a chunk apart from the head of the list it was on, the ordinary LIFO
free. Step 0: `(lo len apart-when COND)` — a range in the address space always,
set apart only where the body reads it. Step 1: `(views (lo len) …)` — ranges
handed over to READ: apart from objects, never from each other, read-only
PROVEN by the frame row; and two silences refused in both runners (a clause
head the translator reads once, written twice; a loop sub-clause it does not
read). Step 2: every other apartness argued in the spec beside its clause, on
six named invariants. Step 3: TCB.md §4d GENERATED in both runners — per
section, the callers a row covers and the callers nothing does (31 sections: 2
covered, 18 with an unproved floor caller, 15 exported) — with §5's sixteenth
acceptance. No verdict moved (25 hashes over two symbols), no floor byte, no
ladder row. **S-77 — the solver's hang net leaving `npk_small_free` at 81% of
its bound — was settled as D-297 and LANDED 2026-09-17 as the s8 seat's first
landing, before 1.5.7 step 0:** the net is `120 + 10·checks + 60·B` seconds per
file in both runners, B the file's rows the committed manifest records `budget`
(B = checks with no manifest to trust); `npk_small_free` is decided under 610 s
where it was 250, and nothing else moved.
**WHAT REMAINS OF CYCLE 1.5 (corrected 2026-09-17 — this file said "its last
subcycle" from the 1.5.6 close until then; the README's map was right
throughout): THREE subcycles since 1.5.8's close (2026-09-19) — 1.5.8b, the
`overflow`, `bounds` and `cast-range` rows, PLANNED 2026-09-19
(`meta/roadmap/1.5/1.5.8b.md`), 1.5.8c, `decreases`/`unbounded` with the
`terminate` and `stack-depth` rows, and 1.5.8d, the cycle's close. **1.5.8b's
planning measured first, and the user settled SEVEN questions the day each was
asked (D-308…D-314).** A struct field may carry `limit<Rules>` (D-308). The
overflow rows nothing proves stay guarded, measured and reported (D-309). A
certain constant overflow is refused (TYPE-076, D-310). `uint64`'s upper half
is built with bit operations, `~0u64` (D-311). The wrapping family `+% -% *%`
was the user's own question (D-312). And planning found THREE memory-safety
holes, each confirmed by a probe: a string's `.len` writable (DEF-72), the
prelude `List`'s `cap` writable (DEF-73), and `List` elements reached by
unchecked raw-pointer indexing everywhere (DEF-74). The user settled the field
qualifiers `sealed` (read anywhere, written only by the declaring module;
D-313) and `hidden` (neither; D-314), with `List` indexed `l[i]` and checked.
They land first, before any row. **Step 1 LANDED 2026-09-19**: `sealed` and
`hidden` are keywords and field qualifiers — TYPE-079 a write to a sealed field
from outside its module (every write form: an assignment through any path, a
compound, a struct literal, a `move`/`pass` out of an owning one, `@`, `$$m`, a
`Self->` receiver, a stateful operation; `$$i` reads), TYPE-080 any touch of a
hidden one, TYPE-081 either off a field — "outside" drawn where a private
member's line is; the compiler-known headers (`ptr`/`len`/`cap` of string,
cstring, slice and buffer, and `OwnedFd.value`) are sealed BY DEFINITION in
every module (DEF-72 fixed); and two holes of the same class found on the way
and closed: an `RGuard`'s `.value` was read-only only as an assignment's direct
target (`g.value.x = 5`, `@g.value.y`, `$$m g.value.x` were accepted — writes
through a SHARED hold; DEF-77), and `@f.value` on an `OwnedFd` was accepted
(DEF-78). Every program's IR is byte-identical. **Step 1b LANDED 2026-09-19**:
the prelude's `List` is `{ hidden wild T->:items; sealed int64:count; sealed
int64:cap; }` (DEF-73, DEF-74 fixed), `l[i]` is the element bounds-checked
against `count` and `l[lo...hi]` a checked view, a list behind a pointer is
`(<-p)[i]` (TYPE-082 refuses `p[i]` on a pointer to an array, a slice or a
`List`: pointer arithmetic that type-checked), and the prelude's checked
operations `list_pop`/`list_truncate`/`list_clear`/`list_insert`/
`list_remove`/`list_swap_remove` are how a list changes; 2,011 element accesses
swept, the compiler's own `OutOfBounds` guards 13 → 765, and the new check
found DEF-79 on its first run (the NONE declaration stored past `count` since
1.4.7). A bridging snapshot refresh carries it. **Step 2 LANDED 2026-09-19**:
a constant `+ - *` or negation is computed exactly at its width and refused
when it does not fit (TYPE-076, D-310; `0u64 - 1u64` among them — D-311's
`~0u64` and `(1u64 << 63u64) | k` build the upper half), emitted as its
constant when it fits (DEF-70: all 85 constant checked operations gone), and
the folder's every other operation is now the machine's (DEF-80: a wide shift
trapped the compiler, a narrow `<<` and `~` were not truncated, `uint64`
divided and compared signed). **Step 6 LANDED 2026-09-19** (D-308 §§1–5,
the field-limit MECHANISM; §§6–7 — the 2^47 ceiling, the length producers'
checks, the built-in length fact and the prelude `List`'s own rule — are step
6b's, split at the decision's seam because they move the floor's bytes): a
struct field may carry `limit<Rules>`, a rule about that ONE field, checked
after each of its three write points and a FACT at every read. The rule must
hold of the field's vacant value, decided by the constant folder at the
declaration (TYPE-077) — and a rule the folder cannot decide there is refused,
since nothing decides it later. A limited field has no address, through a
pointer to its struct as well (TYPE-063 extended: the case a rule about a
BINDING could never see). `field_limit.npk` is the payoff — a `div-zero`, a
`div-min` and three `overflow` rows discharge on facts that exist only because
a field carries a rule — and the compiler's own emission is BYTE-IDENTICAL, so
no verdict moved. **Step 3 LANDED 2026-09-19**: every plain-integer
`+ - *` and negation is an `overflow` row at its guard's site (one over a
`simd` operation's lanes, one with N−1 traps for an integer `.sum()`), a
discharged row's branch becomes one `llvm.assume`, and the compiler's own
manifest is 2,439 rows — 1,273 discharged, 1,161 open, 0 budget, 0 unencoded.
D-309's residue is measured and reported by shape (a bounded counter
discharges; a length, an unbounded counter, a sum of two unknowns and the
prelude's numeric cores stay), no bound was written into the tree to close a
row, and the speed says the guards are free: removing 1,181 of the compiler's
2,307 `IntOverflow` traps does not move a 70-second compile. **The obligation
table now holds only what the emission holds** (D-262's rule carried to the
rows: 145 prelude functions' rows left it, the 213 `err-exit` of 1.5.4b among
them). Three defects were found on the way: **DEF-81**, a soundness hole — a
guard inside a loop's invariant was elided on the proof for the head's FIRST
visit, and the verified build divided by zero where the plain build trapped
(rows per context now, `rows.txt`'s twelfth field, and the belts count guards);
**DEF-84** — an index through a call's result or a `Result`'s `.value` was
admitted by the checker and refused by the emitter (EMIT-002), for arrays since
each arm was written; and **DEF-83** — an explorer control's finding seed,
which spins to the step budget by design, could pass its 60-second net under
load and read as a blind control (300 s now). **Step 4 LANDED 2026-09-19**: the
WRAPPING FAMILY `+% -% *%`, with `+%= -%= *%=` (D-312, the user's own
question) — arithmetic modulo 2^N at its trapping twin's precedence, with no
guard, no obligation row and no `failsafe` arm, refused by name on every kind
that owns its own arithmetic (NITPICK-TYPE-078, sixteen shapes pinned), folded
WITH the wrap (in `uint128`, since the compiler's own `+ - *` trap), and
modelled exactly by the encoder as `(mod t 2^N)` — two's complement for a
signed width — so a row after the site knows the value. FNV-1a 64 and
splitmix64 reproduce their published sequences, and the prelude's `fnv_mix`
adopted `*%`: a 128-bit multiply, an `IntOverflow` guard that can never fire
and a truncation became one `mul i64` in every program that hashes
(`intern.npk`'s copy of the same step waits for a snapshot that parses the
operator, D-205). **Step 5 LANDED 2026-09-19**: the `bounds` and `cast-range`
rows — an index inside its bound at every checked access (one row for a range
slice's pair) and a float's `=>!` cast to an integer inside its target's, each
goal the EMITTER's own guard read back, each elided into one `llvm.assume`
where discharged. A CONTAINER'S LENGTH IS ONE TERM (`(|npk.len| base)` per
binding, ended by a write to the binding), which is what lets a loop over
`xs.len` prove its own accesses; the compiler's own manifest is **3,040 rows —
1,136 discharged, 1,177 open, 722 `unencoded`**, and the 722 are one shape: a
`List` is address-taken by every `push`, so DEF-14 leaves it no length term
(E-4 re-homes that residue to 1.6's leg B, where a frame condition is the
tool). DEF-82 is fixed — a `defer` body's guard is ONE row and one trap PER
COPY the emitter writes. Four defects of the step's own making were found by
its instruments, not by reading: an element WRITE had a guard and no row (71
unaccounted traps), the range slice asked for its elision under the wrong site
key (the row read `discharged` while the guard stayed), the length symbol's
sentinel was 0 where `sym_new` hands out ids from 0 (DEF-69's rule again), and
a shared symbol's range axiom sat in the region of its first read, so two
functions lost discharged rows the gate then refused. The old 1.5.8 was PLANNED 2026-09-18 by
`nitpick-compiler_s11` as those four (`meta/roadmap/1.5/1.5.8.md` §0),
because planning MEASURED first and found three of its five kinds standing on
uncontrolled stops: DEF-58 (a float's `=>!` cast to an integer was LLVM poison
— one program exits 3 at `-O0` and 9 after `opt -O2`), DEF-59 (a stack
overflow killed with no `failsafe`; `npkc` under `ulimit -s 2048` exits 139),
DEF-60 (a spawned thread's one guard page could be jumped by a frame larger
than a page — 151 of the compiler's own). The user ratified S-84…S-87 in one
sentence ("go with all four") as D-304 (`decreases`/`unbounded`), D-305 (the
split-stack prologue on every emitted function, `StackExhausted`), D-306
(`CastRange`) and D-307 (`MachineFault`, the last net).** **1.5.8 IS COMPLETE
(2026-09-19; ten landings — steps 0, 1, 1b, 2, 2b, 2c, 3, 3b, 3c and 4 — each a
cumulative prefix under a full harness, D-228).** A float's `=>!` cast to an integer
traps `CastRange` (armed where one exists). A joined thread's stack is
unmapped. **Every function the compiler emits checks its frame against the
thread's limit word at `%fs:0x70` (`"split-stack"`, one text: `ll_fn_open`),
and every stack is the FLOOR's**: 8 MiB for main, 2 MiB for a thread and 1 MiB
for `failsafe`, whatever the shell's `ulimit -s` says. Each has a guard, a
signal stack and a 64 KiB reserve. `StackExhausted` is armed in every program.
The floor's own frames never check, and the `floor-stack-reserve` belt proves
they fit (1,712 of 16,384 bytes). Since step 4's one-hop snapshot refresh the
builder, and every tool it compiles, checks its own stack too. The syscall census reads `module asm` (2b, DEF-64), and the
explorer can HOLD a thread at a site until another passes it (`hold-at:`, 2c,
DEF-67): a kept seed goes stale when a step is added before its window, so
DEF-57's regression is now a held control, `frozen-traps.ctl`. **The last
net (step 3, D-307):** SIGSEGV, SIGBUS, SIGILL and SIGFPE enter the trap route
as `MachineFault` (armed in every program) on the thread's signal stack, with
SA_NODEFER so that a fault inside a fault's `failsafe` exits 70. SIGPIPE is
caught, so a write to a pipe with no reader answers EPIPE (DEF-68: it killed
the process, exit 141). A program faults the CPU only through JIT code
(`wildx`), which is how the tests do it. A spawned thread's trampoline block and
executor come from per-slot POOLS reborn for each thread in the slot (3b,
DEF-66): the join unmaps the stack and closes the epoll set before it retires
the slot. A standard descriptor closed at startup is opened onto `/dev/null`
before `main` (3c, DEF-69: a data file opened next became descriptor 2 and
took every stderr write), and the reactor's "none" is −1 at every site — 0 is
a descriptor once a program closes its stdin. The plan's execution record says
what each step found.
**1.5.7 (D-212's schedule-exploration harness) IS COMPLETE (2026-09-18;
`meta/roadmap/1.5/1.5.7.md`; eight landings, steps 0–7, each a cumulative
prefix under a full harness, D-228; seats s8 then s10)**: the REAL floor and
each concurrency test's own IR transformed, every synchronization step a
point of a seeded PCT schedule, the blocking syscalls virtual, quiescence
oracles, the spec's caller hypotheses executed, fourteen negative controls,
and the explorer's first floor find, DEF-57 (VERIFICATION_REFERENCE §10 is
the whole of it; TCB.md §5's seventeenth acceptance says what it does not
cover). The record, step by step: planned and measured by s7 with a throw-away prototype, approved
by the user 2026-09-17 in one sentence (S-77…S-83 → D-297…D-303), and its
**step 0 LANDED 2026-09-18** (`meta/roadmap/1.5/1.5.7.md`): the ONE
transformer (`npkg/explore.npk`; `tools/explored.npk` is the harness's
snapshot-built entry — a program cannot hold two modules named `explore`) puts
a scheduling point before each of the floor's 78 atomic step lines and routes
each of its 63 `@npk_sys6(` calls to the shim, and the TOTALITY BELT
(`explore-step-escapes`) counts in both runners — the harness has no
transformer of its own, by design (X-9: the property is an equality of two
counts); both self-checks hold the transformer's output to one literal over a
planted floor; the planning prototype's C shim is kept OUTSIDE every gate as
the IR shim's behavioural reference (D-303). **Step 1 LANDED 2026-09-18**:
`runtime/explore/npkx.ll`, the shim in hand-written IR (the baton, virtual
futex/epoll/clock, PCT with ordered bands and the fairness bound, exact
replay by seed), held to the C reference SCHEDULE HASH FOR SCHEDULE HASH on
all 30 signal-free programs × 20 seeds under a twelve-process load
(`meta/roadmap/1.5/tools/explore_prototype/hashcmp.sh`) — and the port found
X-13: the floor's `mmap` trims made a step count depend on an ADDRESS, so
both shims now place anonymous mappings at a 64 KiB-aligned bump pointer
with `MAP_FIXED_NOREPLACE`; the `explore` stage in both runners (a
`[[test]]` entry: per unit the measuring run, 1,000 seeds, the first seed
replayed to the same hash, PCT's bound printed); `// explore: 1000` on 30
`// stress:` programs, `// explore: no <reason>` on 14 (ten real-child, four
trap-route until step 2), the marker belt `explore-unmarked` in both
runners. **Step 2 LANDED 2026-09-18**: the signals are virtual
(`rt_sigaction` remembered, `tgkill` marks its target and makes a blocked one
runnable, the handler runs in the target's context at its next grant), so the
trap route is explored like everything else — the four trap-route programs
say `// explore: 1000` and agree with the C reference hash for hash on 20
seeds each; 34 of 34 explorable programs do. **Step 3 LANDED 2026-09-18**:
the quiescence oracles are live in every explored run (D-301: `LOST-WAKE`
and `LOST-FUTEX-WAKE` are red verdicts read off the REAL executor state at
quiescence — a lost wakeup here is lateness an exit code cannot see; the
shim's three struct offsets are held to the floor's type lines by
`explore-oracle-offsets` in both runners), and the CONTROL MECHANISM under
`runtime/explore/controls/` (a `.ctl` plants a defect by one exact-line
substitution and names the verdict the explorer must reach within N seeds —
`explore-control-blind` otherwise; `store-release` and `no-rouse` landed, both
found at seed 1; 680 clean runs, no false positive). **Step 4 LANDED
2026-09-18: the nineteen model controls walked** — eleven are `.ctl`s (ten
new), five measured NOT A FLOOR BUG (the stamp-as-deadline in
`npk_sl_earliest` is a second line of defence the `park-unpark` model has no
deadline to represent; a handle is minted by the open that publishes its slot
and a reclaim is the scope exit, once), one NOT OBSERVABLE (a write into freed
memory: the model's), two NOT EXPLORABLE (the driver pair is real-child); the
grammar grew several `old:`/`new:` pairs, the program's-answer verdicts
`wrong-exit`/`exit N`, the LATENESS verdict `late N` (the shim prints `vrun=`;
X-17), and DIRECTED controls (`preempt-at:` names a site of the floor at which
the shim demotes the arriving thread — a change point at a place, X-15 — for
the three windows one step wide that blind PCT cannot land on); and the walk
found and fixed THREE DEFECTS OF THE SHIM, each mirrored in the C reference
and re-swept (38 of 38 explorable programs agree hash for hash): a wait whose
deadline had already passed slept virtually until quiescence where the kernel
returns at once (X-14 — 3 false LOST-WAKEs in 100 seeds), the fixed fairness
bound resonated with a period-three thread so it never rested holding the lock
a control needed (X-16 — jittered by a seeded draw), and the measuring run's
steps were read off the wrong line when the defect fired at seed 0. Four
explored programs were written for shapes none had (`io_ready_declined`,
`shared_arena_race`, `trap_one_failsafe`, `reactor_arm_race`). **And the step's
own harness found the explorer's FIRST FLOOR DEFECT, DEF-57, fixed in the same
landing**: `npk_trap` publishes the frozen flag before it claims the failsafe
holder, and `npk_step`'s `frozen:` block, older than D-291, answered the flag
by trapping `Unreachable` ITSELF. So an executor that merely watched a trap
could win the holder in that two-instruction window, and `failsafe` ran with
the wrong error (seed 371 of `trap_one_failsafe`; 40 stress runs never reached
it). A watcher parks now and the holder keeps the re-entry exit 70. The
`trap-route` model, blind for want of an error code, gained one: `wrong-error`
and `holder-parks`, each with a control. The floor's bytes moved. **Step 5 LANDED
2026-09-18: the spec's caller hypotheses EXECUTED (D-302)**: `npkg/explore_req.npk` writes one entry checker per section of
`runtime/npkrt.spec` that has rows and a `requires`/`objects`/`views` clause —
31 checkers, 236 of 240 hypotheses evaluated over the entry state in i128 at
every call of every explored schedule, the 4 sorted-table `requires` that name
the free symbol `j` LISTED BY NAME (the stage's output, TCB.md §4d's new
column); a false one is the shim's verdict `ASSUMPTION <symbol>: <clause>`, and
the SPEC control `unconditional-apartness.ctl` (X-18: `spec-old:`/`spec-new:`
pairs) plants 1.5.6's clause that 1.5.6c found false by reading and sees it on
`drop_string`'s 27th step. The D-303 sweep after it found a REAL RACE in both
shims (X-19): the kernel clears a thread's `CHILD_CLEARTID` word after its last
virtual step, and the joiner's read of the word decided a step count — a
thread's end is now settled before anyone else steps (the clone's ctid word
handed to `npkx_spawned`, the next baton holder waiting for the clear). Found
on the way: the harness's `define_headers` read one line, so three multi-line
defines had no header and `check_spec`'s free-symbol check skipped them in
silence. **Step 6 LANDED 2026-09-18: the program's own steps** (X-20, X-21): every
explored unit's emitted IR goes through the same transformer's PROGRAM MODE
before `llc` — a point before each atomic step of its defines (`atomic<T>`,
`atomic_from_ptr` lower inline, in the program), each `sys` call's
`@npk_sys6(` routed, sites numbered from 1,000,000 — under the counting belt in
both runners. The census found no explored program holding a program-level
atomic (an `atomic<T>` borrow cannot cross a spawn, D-180), and 23 `sys` calls
in seven programs that had run as real syscalls with no point before them.
`atomic_threads.npk` counts to 400 from two threads through `atomic_from_ptr`
over `wild` storage, and the PROGRAM control `atomic-lost-update.ctl`
(`program-old:`/`program-new:` pairs over the named program's SOURCE) splits
its `fetch_add` into a `load` and a `store`. Found at seed 1 (55 of 100);
without the program's points, 0 of 100. **Step 7 LANDED 2026-09-18: the docs and the
close** — VERIFICATION_REFERENCE §10 (the explorer, whole), TCB.md (the explorer
among the standing instruments; §5's thirteenth and sixteenth acceptances
narrowed by dated notes; a seventeenth for what it does not cover: weak memory,
real-child programs, the trampoline, schedules beyond the seeds, liveness), the
landing notes of D-212 and D-298…D-303, E-3's instrument half closed. **1.5.8 is not a close-out footnote**: VERIFICATION_REFERENCE
§7b's catalogue assigns it the five obligation kinds that have NO rows in
`nitpick.obligations` — `overflow`, `bounds`, `cast-range` (guards to elide)
and `terminate`, `stack-depth` (none) — D-210 §4 commits cycle 1.5 to proving
the overflow traps away, and 1.6's leg B lists those rows as evidence arriving
from 1.5. Sized at `b7d60dc`: 2,248 of the 2,503 guard-trap call sites in the
compiler's own emission (90%) are `IntOverflow`, against the 273 guards the
verified build elides today (202 manifest rows). `terminate` has no surface syntax (`decreases`
appears nowhere in the lexer, the parser, the grammar or a spec), so 1.5.8's
planning opens with a language question — the user's — and the library
listener is owed the keyword-or-refusal answer, named with its code, before a
re-pin could surprise it. **Read the map, the catalogue and the manifest, not
a summary of them** — a hand-written summary of a held fact has no check on it.
**The decisions this cycle settled: D-224…D-233.** `exit` is process exit in
every body (D-224); declared-uninitialised managed storage holds its canonical
vacant value (D-225 — `OwnedFd`'s vacant is −1, not zero); the index type
follows the count (D-226); **a memoised layout fact is never read before it is
computed** (D-227 — the query ensures, the caller does not remember, and
`_recorded` is the explicit opt-out); the orchestration rules are normative
(D-228); the diagnostic walk is generic and borrowing and prints span-sorted
(D-229); D-044's flag types get implemented as one `TY_FLAGS` kind (D-230);
the sub-byte integer widths are struck and the wide ladder pinned (D-231);
and **D-233 replaced Astrée with LLVM-native analyzers over our own emitted
IR**, striking the C emitter (D-232, superseded).
**Four defects came out of D-227's neighbourhood, none found by a test of the
thing that broke**: `tt_grow` never zeroed two of its four side arrays (latent
since 1.2.5); the three memoised bits were read before computation, disabling
TYPE-046, D-215 and `gives` wherever the window was open; a payload-less enum's
bits were never written at all; and two live TYPE-046 violations in `src/`
itself, where a `PlaceVal` owning a string was copied into a consuming
parameter. All four share one root — **a fact that is ABSENT and a fact that is
FALSE were spelled the same way**, which is what the new `absent-fact` harness
stage now makes impossible to reintroduce.

**A concurrency test runs 40 times, not once.** `// stress: N` in a program makes
the harness require the same exit code every run. Two serious defects hid behind
single green runs — `npk_exit` calling `exit` rather than `exit_group`, so a
threaded program's status was whichever thread finished last, and a channel wake
landing between registering and sleeping, which the sleeper-push then erased.
Neither reproduced in fewer than about twenty runs.

`tools/check.npk` still runs the whole frontend over a program and exits 0 on a
clean one — Phase A's checker, now one thin `main` over the shared pipeline.

It refuses a program that returns a borrow, launders one through a call, reads an
unassigned binding, writes a `fixed` binding twice, uses a moved-from binding,
double-frees, takes the address of a temporary, leaves a `pick` arm uncovered, lets
`(*)` swallow ERR, reads a tainted `Result.value`, acquires a lock downward, expands
a macro without bound, names something in a macro body its defining scope does not
have, splices a body where it does not fit, evaluates a `comptime` that never
finishes, derives a trait that cannot be derived, writes a struct literal that
omits a field, discards a `Result` with a bare `f();`, lets a `never fails`
function `fail`/`relay`, drops a trait's `never fails` in an impl, or `fail`s
inside a `defer` (D-163, 1.1.0) — each with its own code, its own span, and a
case in one of the six rejection suites showing it refuse.

**Two dozen whole-tree checks run on every harness invocation** and each found
something on its first run: `check_kinds_typed` (every expression kind is typed),
`check_kinds_lowered_or_refused` (every Expr/Stmt/Decl kind lowers, refuses by
name, or is confessed — plus the LIVE-1 carrier accessors stay read; its first
run found five expression kinds dying as internal defects), `check_codes_tested`
(every code has a case, or a stated reason), `check_codes_centralised` (no code
literal outside a `*_codes.npk`), `check_ll_types_agree` (`// ll:` markers match
the lowering), `check_runtime_sigs_agree` (npkrt.ll vs seed vs
the ir_runtime table BUILTIN_REFERENCE now generates — the spec in the loop
since 1.4.2, and its derived-inner leg found dead on the day it was fixed),
`check_builtin_sig_texts` (every type text the generated signature table hands
the checker is one `builtin_text_type` can intern, and no arm of it is dead —
it caught a half-done `wildx any->` on its first run), and
`check_zero_dependency` (the undefined-symbol scan). A ninth,
`check_decisions_current`, REPORTS rather than fails: stale decision-log
candidates print on every full run for the doc-sync pass. They diff the compiler
against the thing that describes it, which is how cycle 0.6 found every one of
its holes — none was found by a test.

### Building and testing

```
python3 bootstrap/harness/harness.py                    # everything, ~20 minutes + the parity stage's `npkg test`
python3 bootstrap/harness/harness.py --only type_stmt   # one test, ~1 minute
```

It assembles the committed snapshot (`bootstrap/seed/stage1.ll`) into the
BUILDER, has the builder compile `src/npkc.npk` into the compiler under test
(D-205 — the Python seed in `bootstrap/generator/` retired as a builder at
1.4.6), compiles each suite with that compiler, links against
`runtime/npkrt.ll` via `llc` and `ld.lld`, runs the result, and compares the
exit code. It also feeds every source through the **real** parser
(`tools/parse_check.npk`) and re-checks that every AST node kind is reachable.

**`npkg` (1.4.8, D-206) is the permanent runner, and it runs beside the
harness until `meta/SWITCH.md`.** Build it with the compiler under test and
run it from the tree root — it finds `nitpick.toml` by walking up, builds
into `build/` (gitignored), and `npkg build` leaves `build/npkc`:

```
python3 bootstrap/harness/quickemit.py --keep npkg/main.npk   # builds .internal/quickemit/p_main_npk
.internal/quickemit/p_main_npk build                          # the ladder: floor, builder, src/ -> build/npkc
.internal/quickemit/p_main_npk test                           # every suite, ~25 minutes; --only SUBSTR to iterate
.internal/quickemit/p_main_npk test --selfcheck               # the runner self-check alone (§7.1)
.internal/quickemit/p_main_npk test --verdicts out.txt        # plus one verdict line per unit (the parity diff's input)
```

The harness's `parity` stage does all of this on every full run and diffs the
verdicts, so a green harness already says the two runners agree; `npkg test`
by hand is for iterating on `npkg` itself, and its `--only` skips the sweeps
exactly as the harness's does.

For the middle of a subcycle, where the question is "does this one rule fire on
this one file", there is a faster loop that builds the checker once:

```
python3 bootstrap/harness/quickcheck.py tests/analysis/rejection/borrows.npk
```

And the same loop for the backend — build `npkc` once, then compile, link and
RUN programs with it, printing the exit code (or the refusal, or llc's first
error; `--ir` prints the IR too, `--keep` leaves the `.ll`/`.o`/binary behind):

```
python3 bootstrap/harness/quickemit.py tests/backend/programs/dyn_slots.npk
```

**The verification leg (1.5.0, D-218/D-219).** The compiler emits every
function's obligations and reads a manifest of verdicts; `npkg` owns z3:

```
.internal/quickemit/p_main_npk verify              # the VERIFIED build: obligations decided by the pinned z3, held to nitpick.obligations, guards elided, the verified compiler rebuilt from itself
.internal/quickemit/p_main_npk verify --record     # write nitpick.obligations from this run -- a deliberate re-baseline, committed with the change that moved it
.internal/quickemit/p_main_npk verify --explain    # plus build/verify/explain.txt: a model per open row, a reason per budget row, a core per discharged one
.internal/quickemit/npkc file.npk --obligations D  # the compiler's half by hand: D/NNNN.smt2, index.txt, rows.txt
z3 smt.random_seed=0 sat.random_seed=0 rlimit=20000000 lp.dio=false -smt2 D/0001.smt2   # one file, the profile spelled out
```

The harness's `verify` stage does the same over `tests/verify/` (each file
names its rows with `// expect-obligation: KIND VERDICT N`, exactly) and over
the compiler itself, and its `parity` stage byte-compares `npkg`'s verified
compiler with its own. A verdict that moves is a red run, never a rebaseline
(D-040): run `--record` only in the commit that changes what is proven.

Since 1.5.2 the leg carries `limit<Rules>`: every write point of a limited
binding is a `limit` row (its rule over the new value, the rule a hypothesis
on every later version — so a division by a limited divisor discharges), every
direct call of a sync callee with limited parameters a `limit-subsume` row,
and the elided build removes a discharged write point's check for one
`llvm.assume` over the rule's range clauses and lets a discharged call name
the callee's `.body` past its checked entry. A limited binding has no address
(TYPE-063) — pass it by value — and a `limit` refuses where no write point
exists (TYPE-064). Two ways to read a row by hand: `.internal/quickemit/npkc
FILE --obligations D` then z3 on `D/NNNN.smt2`, and a hand-written manifest
in `nitpick.obligations`'s shape fed to `--elide` (outside every gate, P-27).

**The cost leg (1.5.1b step 0).** `tests/cost/*.toml` are units judged by the
allocator's numbers under `NPK_HEAP_STATS` (BUILD_REFERENCE §7.1's `cost`
row): DEF-1's three recipes at N and 4N, the two temporaries probes, and the
compiler's own build. A unit marked `expect = "fail"` must FAIL its bound until
the commit its `until` names lands; the day it holds, the unit fails until
those two lines are removed in the same change. By hand:

```
NPK_HEAP_STATS=1 .internal/quickemit/npkc file.npk > /dev/null   # the line is the last on stderr
```

Neither is a substitute for the harness; both skip every whole-suite check.
`quickcheck` watches nothing — rebuild it after every edit to `src/`, since a
stale binary answering an old question is the failure mode to expect;
`quickemit` rebuilds itself when anything under `src/`, `lib/` or `bootstrap/`
is newer than its cached `npkc`.

Three things to know before you use it:

- **`--only` is for iterating, never for concluding.** It skips every whole-suite
  check — node-kind reachability, the real-parser sweep, module rejection — and
  its output says so twice. **Nothing is committed on the strength of a filtered
  run**; do a full one first.
- **A test's expectation lives inside the test**, as an `exit` code per case. A
  failure reports `exited N, expected 0`; find `exit Ni32` in the file to see
  which case broke.
- **Every test builds the whole frontend through the seed**, which is why even one
  test costs about a minute. That is the floor, not something to optimise around.

Five more, each of which cost a debugging cycle in 1.4 (the executor HANDOFF
that carried them retired at the cycle close):

- **Strings are move-only owners** (TYPE-046): no binding-to-binding copies;
  pass as plain arguments freely; consume with `move T:p`; in emitter code
  rebuild a name per use rather than holding one binding across lines.
- **The walkers-total instrument refuses a half-done type-kind change**
  (`check_type_walkers_total`, with excuse tables in `harness.py`). When it
  fires, complete the change or update the excuse WITH A TRUE REASON — never
  silence it.
- **A backend fix does not reach the tools until the snapshot carries it.**
  The harness compiles `tools/` with the SNAPSHOT, so a checker rule in
  `src/frontend/` is in the built tools at once and an emitter fix is not
  (`bootstrap/seed/README.md`, the mirror of D-205).
- **`src/`'s own code is checked like everyone else's since the switch** —
  overflow traps, the escape analysis, move-only owners. A trap inside the
  compiler is a `src/` bug, not a test bug; `gdb -ex "break npk_trap" -ex run
  -ex bt` on the built `npkc` names it in one shot.
- **Never rewrite `done/` archives or settled DECISIONS text** — annotate
  with dated notes (the D-085/D-202 pattern).
- **Both runners stand under `nitpick.toml`'s `[limits] nofile` (1024; 1.5.1b
  step 5).** A descriptor-exhaustion proof means something only under one
  known soft `RLIMIT_NOFILE`; this machine's session sets 1,048,576, and every
  such proof in the suite passed against a leaking build until the runners
  lowered their own limit. Running a program BY HAND runs it under the
  session's limit — `(ulimit -n 1024; ./prog)` is the spelling that matches
  the runners, and `fd_ceiling.npk` exits 5 without it.
- **`pass h.n` over a copyable field no longer clears `h`'s drop flag** (DEF-8,
  since 1.2.3): the clear is gated on the passed value's type dropping.
- **`wild_release_all()` is followed by `exit`, or TYPE-062** (1.5.1b step 5):
  nothing may run after the release, and a `main` that released and then
  RETURNED ran its drops over unmapped memory the day `List<T>` began to own.
  Measure after the release inside `exit`'s operand.
- **A `move` or `pass` out of a FIELD leaves the type's vacant value** (S-26):
  the aggregate stays live and drops its siblings; a vacant `List` grows from
  zero.
- **A derived body reaches every member through the trait it derives** (D-258,
  1.5.2b): a scalar through the PRELUDE's generated impl, a named type or a
  parameter through its own, so a generic subject's derive is
  `impl:<T: Eq>:Box<T>:Eq` and `Box<Point>.eq` needs `Point: Eq` — asked at the
  CALL (D-256), never at the instantiation. A family impl's bound is enforced
  where the impl is used, its parameters bound by UNIFYING the target pattern
  (`Pair<int32, T>` is a legal family target). A derived diagnostic reports at
  the derive's declaration (D-259); a `<derived-` path in any runner's output is
  a compiler defect and fails the unit. A program's own `impl:int32:Ord` is
  TYPE-013 against the prelude's; `impl:bool:Ord` is still admitted (S-37).
- **A builtin kind's fixed method set lives in the checker AND the emitter**
  (1.5.2b step 2): both intercepts are gated on the type's own names through
  one predicate in `types.npk`, and every other name goes to the impl table.
  A new builtin kind with methods owes the same gate on both sides.
- **A generic enum's payload-less variant needs the annotation** (D-261,
  1.5.2c): `Opt<int32>:o = Opt.None;` — a bare `Opt.None` where nothing is
  expected has nothing to infer from and is TYPE-022 naming `T`. A constructor
  infers from its payload (`pick (Opt.Some(big))` is `Opt<int64>`), an
  unsuffixed literal teaches nothing. A `pick` over an `Optional` spells `??`:
  `pick (o ?? Ordering.Equal) { … }` (D-260, TYPE-065).
- **A rule written for one spelling of a construct is owed to the other.** The
  `pick` expression form typed no arm binding from 1.0.9c until 1.5.2c while
  the statement form did (`type_pick_rules` is the one function now). When a
  construct has two spellings — statement and expression `pick`, `=>`/`=>!`,
  `?!`/`?|` — grep for the twin before calling a rule landed.
- **A prelude function is in an emitted module because something referenced
  it** (D-262, 1.5.2d): an assertion on a prelude symbol in emitted IR needs a
  use in the program, and a belt over emitted IR must know WHICH compiler
  emitted it — the committed snapshot carries an emitter change only after a
  refresh, so the tools, `npkg build`'s `npkc.ll` and the runner self-check's
  cases (compiled with the builder by design) are not asked. An instrument that
  counts sites (`llvm.assume` per discharged row) counts only the functions the
  emission holds.
- **A `T` is move-only in a generic body** (D-264, 1.5.2f): store a by-value
  `T:v` only as `move T:v` + `move(v)`; read an element out as
  `move(s.items[i])`; a lending `pick` binds a `T` payload as a VIEW (D-266).
  `move(x)`
  spends `x` at a scalar too (MOVE-001 on a later read of `x`).
- **The ladder prints its digests, and the emission's is the one that
  travels** (D-265, 1.5.2g): `npkg build` ends with one `sha256` line per
  intermediate; quote `build/npkc.ll`'s to another machine, since the object's
  and the binary's belong to the toolchain build. An emitted `.ll`'s BYTE
  COUNT is path-dependent (D-236's root-relative site paths), so a size
  compared across directories is the object's.
- **A lending `pick`'s binding is a VIEW, and a view has no address** (D-266,
  1.5.2h): read it, pass it by value, `give` it if copyable. A method that
  takes `Self->` on a payload needs the consuming form `pick (move(v))` or a
  by-value receiver; the selector cannot be written inside an arm that binds
  (TYPE-067) — assign after the `pick`, or bind `_`; a payload of a stateful
  kind (an arena, a lock, an atomic, a channel, a `dyn`) cannot be viewed at
  all. `drop` over a refused operand stays silent (D-240), as `raw` has since
  1.5.2b — a second sentence there is the checker's defect, not the program's.
- **A primitive's empty case is the runtime's to handle, and its neighbour is
  the model** (DEF-25, 1.5.2i): `string_slice` allocated nothing for an empty
  result since D-186 while `string_concat` beside it allocated a real block
  and returned cap 0. When one floor primitive handles an edge, read its
  siblings for the same edge; a leak of managed storage is invisible to D-151
  (which counts `wild` blocks) and shows only in `NPK_HEAP_STATS`.
- **`exit` runs joins and defers and no drops** (D-183's amendment, 1.4.4): an
  owning local of `main` is never dropped by a program that exits, so a
  program test with one in `main` measures the storage's REGIME under D-151,
  not the drop (D-263 moved the prelude's `List` buffer to managed storage for
  exactly that). Put a drop's behaviour under test in a function that RETURNS.
- **A contract is a check in every build and a row in the manifest** (1.5.3):
  `old(...)` names the enclosing function's PARAMETERS only (D-243 read
  literally — a local has no value at entry), `result` is the success value at
  the seam, a contract's call is `raw f(…)` of a `pure never fails` function
  (an uninterpreted function to the solver: `sq(3)` has no value, so a row
  over it is `open` unless the callee's `ensures` says enough). A function
  with a `requires` SPLITS like one with a limited parameter (`.body` plus the
  checked entry, which calls `<sym>.req`), so a belt over its symbols counts
  the predicate; a coroutine's check runs at state 0 and its call-site rows
  are `held`. `failsafe`'s `exit` must be positive: a literal that is not is
  REACH-004, a computed one traps to 70. Clauses repeat their keyword
  (`requires a requires b`), never a comma; `use` is a keyword, so no function
  is named `use`.
- **A `prove` is refused by the VERIFIED build only** (1.5.4, S-45/D-269): the plain
  build lowers it to nothing; `npkc --elide`, and so `npkg verify`, refuses an
  undischarged one with `NITPICK-VERIFY-001` naming the row's verdict. A
  discharged `prove` is a hypothesis for what follows (S-48). `assert_static`
  needs a proposition that folds (TYPE-069) and, in a `comptime` body, is
  evaluated per call — as is a `prove` there. A `bool` proposition always has
  at least an opaque term, so a `prove` row is `open`, never `unencoded`.
- **The counted loop's head is checked** (1.5.4, D-022): `loop(start, limit,
  step)`, `till(limit, step)`, a literal step positive — TYPE-068; a computed
  step is the `loop-step` row and keeps its compare. The compile-time
  evaluator agrees with the emitter on direction now (DEF-29), and `tests/`
  carried the evaluator's old two-argument vocabulary in three places.
- **The encoder is path-sensitive** (1.5.4): a branch condition is a fact in
  its arm, the versions after an `if` merge as `ite`, `$` and a range `for`'s
  binding are terms. A pattern's literal is encoded under the SELECTOR's type
  (the checker records no type on a pattern's own literal). Every `pick` is an
  `exhaustive checker` row a verify test must name — one per `pick`, both
  spellings, counted in CODE lines (a comment's `pick (` is not one).
- **A runner's negative self-check case runs under the SNAPSHOT** (1.4.6), so
  its construct must be refused identically by the snapshot and the compiler
  under test: a rule added this cycle cannot be it (1.5.4 step 4's first
  npkg self-check said `RUNG-001` where the new compiler said `TYPE-069`).
- **A module member is called `m.f(x)`, read `m.MAX`, imported `use m.*;`**
  (D-273, 1.5.4c): the call is a DIRECT call (no receiver — `raw m.f(x)` for
  a `never fails` member, `await m.run()` for an `async` one, `drop m.idle()`
  to spawn it), an alias (`use "./m.npk" as m;`) and a `pub mod` bound by a
  wildcard import carry their file's scope, and a private member refuses from
  either spelling (RESOLVE-003). An inline module's TYPES are named from
  outside only through `use m.{T};`/`use m.*;` — there is no qualified type
  path, for files either. A member-less `mod:name;` import carries the
  loaded file's scope (D-274, 1.5.4d) — one meaning with the alias. An
  error constant declared inside an inline module is the FILE's identity:
  its arm is `(file.Name)`, or `use m.{Name};` and `(Name)` (D-275); an
  arm over an `Error` whose identity no loaded file declares is RESOLVE-002
  naming the fix, and a literal, a global, a range, a bound destructure or
  `ERR:` over an `Error` is TYPE-007 — in `failsafe` and in any `pick (r.err)`.
  A `use`, a `pub use` or a `mod:name;` INSIDE an inline module binds and
  loads as at file level (D-276) — an inline module is sealed from its
  FILE's imports, so a name reaches one by being imported INTO it.
- **A `$$i`/`$$m` claim is a whole call argument or a pointer local's whole
  value, and `@` claims nothing** (D-286, 1.5.5): a claim's life is LEXICAL —
  end it with a block before touching its root again; a holder is used, not
  copied; a claim anywhere else is BORROW-014 and the fix is `@`. A read of
  the root under `$$m`, a write under `$$i`, a claim on storage a held `@`
  reaches, and a write through a shared holder are BORROW-013; two
  computed-index accesses are guarded at run time (`BorrowOverlap`) and
  proven away by the verified build (`disjoint`). Exclusivity is decided
  among accesses that spell the SAME ROOT — two pointer bindings that alias
  one storage are two roots.
- **A `fixed` binding has no address** (D-287, 1.5.5): `@`, `$$i`, `$$m` and
  a pointer-receiver call on one are TYPE-071, a `fixed` field included.
  Pass it by value or declare it plain.
- **The compiler's own `failsafe` cannot name a NEW prelude identity until a
  snapshot refresh** (D-205, found at 1.5.5 step 2): the builder is the
  snapshot, whose prelude is the old one, so `(BorrowOverlap)` in `npkc.npk`
  was RESOLVE-002 in the builder's eyes; an arm no identity reaches is
  accepted, so the arm waits for the refresh that carries the identity.
- **Measure before attributing a cost** (1.5.2d): the prelude's +0.75 s per
  program read as the price of D-257's 348 impls and was, to five sixths, the
  bindings analysis sizing its state by the whole program. `perf` cannot open
  events on this machine; `valgrind --tool=callgrind` on the checker over a
  floor-only probe answers in 30 s, `callgrind_annotate --inclusive=yes` reads
  it.
- **A shift's amount is checked** (D-277, 1.5.4b): `x << n` is defined for
  `0 ≤ n < width(x)` only — a literal, a negated literal or a `fixed`
  constant outside it is TYPE-070 at the shift (both spellings), a computed
  amount traps `ShiftRange` (−4115), so a `failsafe` in a program with a
  computed shift must name `(ShiftRange)` — forty roots learned that at
  step 0, the compiler's own `types.npk` unit packing among them. The
  `shift-range` row's goal is a hypothesis after the site.
- **The bit-vector crossing stops at 64 bits** (D-280): `& | ^` and a
  non-numeral shift on a wider word are opaque to z3 — safe, unproven — and
  the Int forms (D-279) are what a wide-width row can use: a shift by a
  literal, a low-bits mask `& (2^j − 1)`, a one-bit mask, `~`. Write a wide
  mask as a numeral and the row decides at 2048 bits; write it as a
  computed word and the row is `open` by design. Two unknowns under `&`
  read by an inequality exhaust the budget even at 32 bits.
- **Every twisted compare and cast out is an `err-exit` row** (D-278,
  1.5.4b): the compiler's manifest grew 213 rows at step 2, most of them
  the prelude's generated impls, so new twisted-typed code in `src/` or the
  prelude moves the manifest (`--record` in the same commit, D-040). A row
  under an `is_err` test or a branch's bounds discharges; a bare compare of
  two opaque values is `open` and keeps its trap. A twisted division has no
  row — its zero divisor is ERR.
- **A proposition holds only where its evaluation does not trap** (DEF-33,
  1.5.4b step 2): a guard met inside a `requires`, an `ensures`, an
  `invariant`, a `Rules` clause or a `prove` — a shift's amount, a
  division's pair, a twisted operand — is part of the proposition, never a
  free hypothesis. So `requires (1i32 << n) != 0i32` proves `0 ≤ n < 32` at
  every call and in the body, and `$ != 0tbb32` as a rule holds only where
  `$` is not ERR. An encoder that pushes a guard's fact from inside a clause
  proves the clause from itself (measured, twice).
- **A float row needs bounds for tier 2** (D-281, 1.5.4b): floats never trap
  and carry no row of their own, but a `limit`, a contract or a `prove` over
  floats is decided — in QF_FP first, and where that exhausts the budget, by
  the Real-interval twin ONLY IF every float symbol in the cone is bounded
  below and above by comparisons against literals (a rule, a `requires`, a
  path condition) and the goal is a comparison. `prove(a + b > a)` is `open`
  (NaN, or `b ≤ 0`); `prove(a == a)` over a parameter is `open` (`fp.eq`'s
  NaN reading, tier 1 only); an unbounded `#sqrt` claim is `budget` and the
  verified build refuses the `prove`. A float `/` or `%` arms no
  `DivByZero` (DEF-37, step 4b) — a `failsafe` names the two only where an
  integer division exists.
- **`simd` lanes are terms, and a `simd` local's lanes are its versions**
  (D-282, 1.5.4b): a constructed or splat local, a lane read `v[2]`, `.len`,
  `.sum()` are known to the solver; a `simd` division is ONE `div-zero` row
  over all lanes, so one unconstrained lane keeps the whole guard. A
  `simd(...)` constructor needs an annotated context (`simd<int32, 4>:v =
  simd(k);`). **A `simd` integer lane's `+ - *` and an integer `.sum()` TRAP
  `IntOverflow`** as their scalars do (D-284, 1.5.4e), so a program with
  integer lanes names `(IntOverflow)`; `v op= w` on a `simd` lowers (DEF-39)
  and carries the guards.
- **The floor is specified now, and an `open` floor row is a red run**
  (D-288/D-289, 1.5.6): `runtime/npkrt.spec` says what each symbol does,
  `runtime/models/` says what its protocols do, and both are decided on
  every full run. Touching `runtime/npkrt.ll` means the spec may need to
  move with it — `npkg verify --floor-only` is the fast loop, and
  `.internal/quickemit/p_floorspec_npk <ABSOLUTE root> --emit DIR` writes
  the rows by hand (the root must be absolute: `Path` is). A spec clause the
  floor refutes fails by name; a `budget` row is residue and must carry a
  `(residue "…")` sentence saying why; a model's control that stops being
  `sat` is a model gone blind. After any floor, spec or model edit run
  `bootstrap/harness/tcb_floor.py --write`, which regenerates TCB.md's three
  marked regions, and re-record with `npkg verify --record` in the same
  commit (D-040).
- **A program's raise is `npk_raise`; a guard's trap is `npk_trap`** (D-285,
  1.5.4e): `?!` and `!!!` enter the trap route through the former, so a
  verified build's belts (which count `@npk_trap(i32 CODE)`) no longer
  mistake `?! DivByZero` for a guard — unwrapping with a system code is
  legal again — and `!!!` freezes, kills drivers and observes the re-entry
  rule (it called `failsafe` directly before, DEF-40). A compound shift
  through a field or element records its `shift-range` row (DEF-41).
- **Every alloca of the floor sits in its entry block and is fully DEFINED
  there** (the stack rule, 1.5.6b step 0; DEF-52, DEF-53): an alloca outside
  the entry block is dynamic (it moves the stack pointer each time it runs —
  D-173, which held every EMITTED module since 1.0.9a and had never been
  pointed at the hand-written floor), and a kernel-written buffer is zeroed
  first because how much the kernel writes is the kernel's promise (the raw
  `sched_getaffinity` writes 8 of the 128 bytes asked and RETURNS the count;
  the zero-fill is glibc's, and this runtime has none). Define by STORES
  where the function carries rows or runs hot — a `llvm.memset` cost
  `npk_hs_put_dec` 8 of its 23 rows — and by one whole-size memset for a cold
  buffer. `alloca-not-defined` is the belt, in both runners.
- **The kernel-effect table is ONE authority** (1.5.6b step 1; D-288 as
  amended): VERIFICATION_REFERENCE §9.2's `kernel-effects` region, generated
  into `npkg/floor_kernel.npk` and into the probe's claims. To make the floor
  issue a NEW syscall: the row first (effect, buffer, length, bound, the
  option set if the effect depends on an option), `gen_tables.py`, then a
  case in `tests/backend/programs/kernel_effects.npk` — the probe demands
  EQUALITY with what the running kernel writes, because an over-approximating
  row is sound and is what hid DEF-52. A `(boundary "…")` section emits NO
  rows whatever else it claims: moving a symbol from promised to proven means
  removing the sentence. A new section or model needs `tcb_floor.py --write`
  BEFORE `npkg verify --record` (the TCB belt runs before the solver) and
  again after.
- **A named block is not a modelled one** (1.5.6b step 2; E-3): the
  correspondence belt proves every atomic operation and syscall of a modelled
  symbol sits in a block some step NAMES; it cannot see a `next` that means
  something else, a named block's path with no transition (`park-unpark` had
  no step for the epoll wait's empty return), or a block named and not
  modelled at all. Read a model against the code CASE BY CASE. A row's budget
  margin is `(get-info :rlimit)` after each `(check-sat)` of the emitted file
  (the runners' push/pop mode; the rlimit is per check), and
  `meta/roadmap/1.5/tools/model_bfs.py` reads a model's whole reachable space
  with no bound — a measurement, outside the gates.
- **A builtin's name is the compiler's** (D-294, D-296; 1.5.6b steps 4, 4b):
  a module-level `func:` — or an `extern` METHOD, whose stub is one — named
  after a bare-name builtin is `NITPICK-RESOLVE-001`, and so is any CALLABLE
  binding of that name inside a function: a parameter, a local, a `for`
  binding or a `pick` pattern's binding whose TYPE is a function type
  (decided by the type at the sites that type a binding, never by a
  spelling — a pattern binding has no annotation). Methods are exempt, a
  function-typed FIELD is exempt (reached through its receiver), a binding
  of any other type is fine (`int64:read` cannot be called) — but a STRUCT
  PATTERN binds its field by name, so destructuring a function-typed field
  named after a builtin is refused. So ADDING a row to a `builtins` region
  reserves a name in every program: measure it across the tree, the
  libraries and the apps, and tell the library listener before it lands.
  `gen_tables.py` takes `--check` and nothing else — ANY other argument
  writes.
- **When a decision ENUMERATES the cases it knows and defaults the rest, the
  default must be the safe reading** (DEF-54, 1.5.6b step 4c): `emit_call`
  listed "a local" and "a parameter" as function VALUES and defaulted every
  other symbol to "a declared function" — so a function value bound by a
  `pick` pattern was called as a symbol that does not exist, and `llc`
  rejected the module. `emit_pipe`, beside it, had always defaulted to "a
  value". The same corner held two more twins that never got their rule
  (DEF-55: `raw o.f(x)` vs `raw (o.f)(x)`; DEF-56: the impl-versus-trait
  comparison had no function-type case, so a trait method with a callback
  could not be implemented) — `tt_func` interns a function type's parameter
  window by its START INDEX, so two spellings of one function type never
  share an id and every comparison of them must be structural.
- **The floor's models are read TWICE, and a probe with two cases in one
  file is read against the file** (D-295, 1.5.6b step 4d): the solver's
  bounded rows and an exhaustive explicit-state search
  (`floor.check_models_explicit`, `npkg/floor_explore.npk`) must both hold;
  a bad state reachable ANYWHERE, a control that cannot reach inside the
  bounds, or a step a variable's range blocks is a red run by name. Editing
  a model: `npkg verify --floor-only` runs both readings; TCB.md's generated
  paragraph prints each model's reachable states, so `tcb_floor.py --write`
  after a model edit. The twins must stay byte-identical — validate
  expressions whole and in one order, hold arithmetic operands under 2^31
  (plain integers trap here and Python's do not). And: a runner's fixture
  that CLAIMS a property is evidence of nothing until something decides it
  (the self-check's toy model was commented safe for a cycle and was unsafe
  in two steps).
- **The harness's `--only` reaches the UNIT tests alone** (found at 1.5.6b
  step 4): a new rejection file is iterated with its stage's tool by hand
  (`quickemit.py --keep tools/resolve_check.npk`, then the binary over the
  file) or with `quickcheck.py`, and concluded by the full run.
- **A floor spec clause is a HYPOTHESIS ABOUT THE CALLER, and TCB.md §4d says
  who keeps it** (1.5.6c; E-1, E-2): a section's `requires`, `(objects …)` and
  `(views …)` are assumed by its rows and PROVEN only at a translated call of
  a `(summary)` symbol; an untranslated floor caller and emitted code (the
  symbol is exported) are checked by nothing. So write a structural
  assumption's ARGUMENT beside the clause, and choose the form by what is
  TRUE for every legal caller: ranges that may coincide are `(views …)`
  (read-only, proven by the frame row) or a bare `requires`, never objects;
  an object the body touches on some paths alone is `(lo len apart-when
  COND)` — never an `ite` inside a range. A false hypothesis is invisible to
  a solver (the rows just hold vacuously for that caller): the two found were
  found by reading. `tcb_floor.py --write` writes FOUR regions now; a new
  CALL in the floor, or a `define` that gains or loses `internal`, moves §4d.
- **What the translator reads once is written once** (1.5.6c step 1): a
  second `objects`, `views`, `frame`, `ensures-trap`, `ensures-fresh`,
  `residue`, `boundary` or `summary` in a section, a loop sub-clause other
  than `invariant`/`unroll`/`inst`/`objects`, is refused by both runners —
  each was dropped in silence before, a claim written and never proven.
- **A solver cost is measured in the RUNNER'S OWN MODE** (1.5.6c step 0): one
  z3 process over the whole file under the profile, wall-clock BESIDE rlimit
  (`/usr/bin/time -f %es z3 smt.random_seed=0 sat.random_seed=0
  rlimit=20000000 lp.dio=false -smt2 FILE`). A row split out of its file, or a
  cost in rlimit alone, measured a spelling as free that took its file from
  202 s to 357 s — past the hang net (P-13; since D-297 `120 + 10·checks +
  60·B` seconds per file, B the file's rows the committed manifest records
  `budget`, and B = checks with no manifest to trust), which is a red run and
  no verdict. `npk_small_free` stood at 81% of the flat net (S-77) and stands
  at a third of its 610 s; a red on the net names the net and is READ, not
  re-run — the solver wedged (S-71's class) or the file is one the manifest
  does not justify.

- **A concurrency test is EXPLORED as well as stressed** (D-299, 1.5.7): every
  `// stress:` program says `// explore: N` or `// explore: no <reason>`
  (`explore-unmarked` otherwise), and a seed that once failed is kept as
  `// explore-seed: S`, run first forever. An explored failure prints its
  replay line; run it by hand under a BARE environment (`env -i NPKX_SEED=…
  NPKX_K=… NPKX_D=… NPKX_TRACE=1 ./prog < /dev/null` — the shim reads
  /proc/self/environ, first match wins), and `NPKX_TRACE=2` prints every step
  as `thread site` for diffing two schedules. After ANY change to the floor,
  the shim or the transformer, run D-303's alternating sweep
  (`meta/roadmap/1.5/tools/explore_prototype/hashcmp.sh`, the harness-built
  tool in OUT): it found X-13 and X-19, where the replay belt could not.
- **A control proves sight of its OWN defect only when the schedule that finds
  it goes through that defect** (DEF-57, 1.5.7 step 4): two directed controls
  had reached their verdicts through DEF-57's window, and went blind when it
  was fixed. Read a control's ROUTE, not only its verdict. A directed site
  cannot reorder two arrivals at ONE place: the later arrival is demoted below
  the earlier.
- **An executor that sees `@npk_frozen` is watching a trap, not making one**
  (DEF-57): it parks, as D-291's losers do, and only the failsafe holder
  re-enters (exit 70). Only a real fault may enter `npk_trap`; a watcher that
  did could win the holder, and `failsafe` then ran with the wrong error.
- **An `atomic<T>` cannot cross a spawn** (D-180 sanctions the locks and
  `shared_arena` only): threads share an atomic through `wild` storage and
  `atomic_from_ptr` (the Bridge's shape; `atomic_threads.npk`). A program's
  own atomic and `sys` steps are points of the explored schedule since 1.5.7
  step 6, their sites numbered from 1,000,000.
- **Every emitted function checks its stack; the floor's do not** (D-305,
  1.5.8 step 2). The prologue compares against the thread's limit word at
  `%fs:0x70`. A floor `define` never carries `"split-stack"`, and the
  `floor-stack-reserve` belt holds the floor's deepest chain within a quarter
  of the 64 KiB reserve, so a new call chain or a large alloca in the floor
  moves that measurement. `ulimit -s` sizes nothing: main has 8 MiB, a thread
  2 MiB and `failsafe` 1 MiB, each the floor's own mapping. A recursion test
  measures the floor's budget.
- **Every `failsafe` names `(StackExhausted)` and `(MachineFault)`** (D-305,
  D-307; the arms went in everywhere at 1.5.8 step 0). Every program can reach
  both, so REACH-002 refuses a new root's `failsafe` without them. **A float
  cast to an integer can trap** (D-306): `f =>! int32` is `CastRange` for NaN,
  an infinity or a truncation outside the target. A program with such a cast
  names `(CastRange)`, and REACH reads both cast spellings.
- **A sentinel is a value the domain never produces** (DEF-69, 1.5.8 step 3c).
  The reactor's "none" was 0, "safe because fd 0 is stdin", and nothing kept
  stdin open. It is −1 now, and the floor opens `/dev/null` onto any of 0, 1, 2
  closed at startup. Before choosing a "none", ask what makes the value
  impossible, and whether anything enforces that.
- **A floor global whose initial value is not zero gets a STATIC initializer,
  never a boot loop** (1.5.8 step 3c). An atomic store before `main` is a
  counted step in every explored run, and a boot loop of them moves every
  schedule (DEF-67's cause). An initializer also needs its named type's body
  ABOVE it in the file, or `llvm-as` reports "initializer with struct type has
  wrong # elements". A startup step the floor must add (a routed syscall)
  still moves schedules: re-run every explorer control after it.
- **A kept explorer seed goes stale with no signal** (DEF-67, 1.5.8 step 2c):
  one step added before a one-point window moves it out of every seed. The
  HOLD directive (`hold-at:`, `NPKX_HOLD1..4`) holds the first thread to
  arrive at a site until another passes it, which finds such a window without
  a seed. That is how `frozen-traps.ctl` regresses DEF-57 now.
- **A spawned thread's floor state is a pool entry, reborn per thread**
  (DEF-65, DEF-66; 1.5.8 steps 1b and 3b). The join unmaps the stack and closes
  the epoll set, and only then retires the registry slot. The next thread in
  the slot rewrites every word of the TLS block and the executor except the
  two reactor words: the epoll word stays as the join left it, and the eventfd
  is kept. So a new executor or TLS word must be written at the rebirth in
  `npk_thread_start`.
- **An expectation is a `//` comment of its OWN LINE** (1.5.8b step 4): both
  runners read `// expect-error: CODE` and `// expect-error-at: N` only from a
  comment that is the whole line, and a file in a rejection suite with no
  PARSED expectation is treated as a FIXTURE and skipped in silence. Writing the
  expectation after the code (`discard(x);  // expect-error-at:14 …`) therefore
  runs nothing: `wrap_kinds.npk` asserted sixteen refusals and ran zero, and only
  `check_codes_tested` noticed, because its code was new. Both runners now refuse
  a file that spells `expect-error` after code, by name.
- **A change to emitted TEXT breaks the emitter's unit tests** (1.5.8 step 2):
  `tests/backend/ir_expr.npk`, `ir_func.npk` and `ir_stmt.npk` compare whole
  functions' text exactly, and step 2's `"split-stack"` broke eighteen of their
  expected `define` lines. Run `harness.py --only tests/backend/` before
  committing an emission change. And a harness run lists its failures ONLY in
  its summary: a run stopped early (the first step-2 run was) says nothing about
  the stages it passed.
- **A floor fix is measured against the floor before it** (every 1.5.8 step).
  Keep the test's object (`quickemit.py --keep`), assemble the previous
  step's `runtime/npkrt.ll` with the pinned flags, link the two with
  `ld.lld -static`, and run it (under `ulimit -n 1024`). A test that passes on
  both floors tests nothing about the fix. Where one program checks several
  things, check each case against its own defect: remove the earlier cases, or
  revert one site of the fix.

- **A `List` is indexed `l[i]`, and changed only through its operations**
  (D-313, D-314; 1.5.8b step 1b). `items` is `hidden` and `count`/`cap` are
  `sealed` outside the prelude: read `l.count`, index `l[i]` (bounds-checked
  against `count`), view `l[lo...hi]`, and change the list with `list_push`,
  `list_pop`, `list_truncate(@l, mark)`, `list_clear`, `list_insert`,
  `list_remove`, `list_swap_remove`. Behind a pointer the element is
  `(<-p)[i]` — `p[i]` on a pointer to an array, a slice or a `List` is
  TYPE-082, since it would index the POINTER. Assigning over an owning element
  drops the old value (a managed array's rule), and `list_clear`/
  `list_truncate` drop what they remove where a bare `count` write leaked it.
- **A field may carry its own `limit<Rules>`** (D-308, 1.5.8b step 6):
  `limit<r_level> int64:n;` in a struct body is a rule about THAT FIELD,
  checked after every write to it — a struct literal's value, an assignment
  through any path (a pointer's included), a compound — and a FACT at every
  read, which is what lets a row over `t.n + 1` decide where an opaque field
  read could not. The rule must hold of the field's VACANT value (TYPE-077,
  decided by the constant folder at the declaration: `0`, or `false`), and a
  rule the folder cannot decide there is refused, since nothing decides it
  later — so the subject is a plain integer, a `bool` or a `char`. A limited
  field has NO ADDRESS (TYPE-063), through a pointer to its struct as well.
  Arming is at the WRITES, so importing a struct and never writing it demands
  no `failsafe` arm.
- **A constant means what the run time means** (D-310, D-311; 1.5.8b step 2):
  a `+ - *` or negation of constants that does not fit its type is TYPE-076
  where it is written, `0u64 - 1u64` included — spell a `uint64` past 2^63−1
  with bit operations (`~0u64`, `(1u64 << 63u64) | k`). A fitting one is
  emitted as the constant, no guard. The ecosystem's FNV basis is
  `0xCBF5DAE484222325` ON PURPOSE (D-190), not the textbook `0xcbf29ce4…` —
  a comment claiming otherwise was stale, and changing the constant would move
  every error code and interface hash.
- **Every plain `+ - *` and negation has an `overflow` row** (D-309; 1.5.8b
  step 3): the guard's own site, one row over a `simd` operation's lanes, one
  row with N−1 traps for an integer `.sum()`, none for a node the folder
  writes as its constant. So new arithmetic in `src/`, `lib/` or the prelude
  moves `nitpick.obligations` (`--record` in the same commit, D-040), and a
  verify test names its own rows (`expect-obligation: overflow VERDICT N`):
  read each against its line, never copy a count. An `open` row keeps its
  guard, and D-309 writes no bound into the tree to close one. A proposition
  includes its arithmetic's range (DEF-33), so `prove(d * d + 1 > 0)` is
  `open` for an unbounded `d`.
- **A guard inside a clause check has a row per context the check runs in**
  (DEF-81, 1.5.8b step 3): a loop head's check runs at the entry, the back
  edge and each `continue`, and its guards are elided only when every such
  row is discharged; a guard inside a check that is not emitted is gone with
  it. `rows.txt` has TWELVE fields (the twelfth the clause context), both
  runners fail a malformed line by name (DEF-75), and both belts count
  GUARDS: an assume per elided guard's trap, a trap per retained one. The
  obligation table holds only functions the emission holds (D-262's rule,
  carried to the rows).
- **An explorer control's finding seed may spin for half a minute** (DEF-83,
  1.5.8b step 3): a `wrong-exit` control found through the shim's step budget
  runs to the budget by design, so a control seed's net is 300 s in both
  runners where a unit seed's is 60. A red control that says `hung` is READ:
  a finding seed killed by load reads exactly like a blind control.
- **Modular arithmetic is spelled `+% -% *%`** (D-312, 1.5.8b step 4): the
  wrapping family computes modulo 2^N and never traps, where `+ - *` trap
  (D-210) — a hash's mix step, a PRNG, a checksum. No guard, no obligation
  row, no `failsafe` arm, and the folder folds a constant wrap WITH the wrap
  where its trapping twin is TYPE-076. Plain integers and `simd` integer lanes
  only: every other kind is `NITPICK-TYPE-078` naming the kind. The compound
  forms are `+%= -%= *%=`. The prelude's `fnv_mix` uses `*%` since this step;
  `intern.npk`'s copy of the same step keeps the `uint128` spelling until a
  snapshot parses the operator (D-205), and the two agree to the bit.
- **`sealed`/`hidden` draw the private-member line** (D-313, D-314; 1.5.8b
  step 1): code outside the module that declares the struct may read a sealed
  field and may not write it (every write form — an assignment through any
  path, a compound, a struct literal, a move out of an owning one, `@`, `$$m`,
  a `Self->` receiver, a stateful operation; `$$i` reads), and may not touch a
  hidden one at all. The compiler-known headers (`ptr`/`len`/`cap` of string,
  cstring, slice, buffer; `OwnedFd.value`) are sealed in EVERY module.

- **An index's proof is a length TERM, and a length term belongs to a binding**
  (D-070's rows, 1.5.8b step 5): `xs.len` and `l.count` read off a BINDING are
  one symbol, so a loop written over the length proves the accesses inside it
  and the verified build drops their compares. A write to the binding ends the
  symbol; a name a pointer may write (DEF-14) never had one, which is why every
  `List` access — the container is address-taken by `push` — is an `unencoded`
  row that keeps its guard (722 of the compiler's own; E-4 sends that residue to
  1.6). So a `bounds` row discharges for a slice, a string, a buffer or a fixed
  array whose length is named and stable, and a `prove`-like bound on an index
  (a `limit<Rules>`) discharges too. A float's `=>!` cast to an integer carries
  a `cast-range` row over the OPERAND; the resulting integer stays opaque.
- **A guard inside a `defer` body is one row and several traps** (DEF-82,
  1.5.8b step 5): the walk sees the body once, the emitter writes it at every
  exit that runs it, and the row's `traps` field is one copy's times the copies.
  When adding a construct the emitter may write more than once, ask what the
  belts will count — they compare the IR's traps and assumes against the rows,
  and they fail closed.

### Reserved words that read like ordinary names

Each of these has cost an edit-build-fail cycle, because the error arrives as a
parse failure some lines away from the mistake:

| Looks like a name | Actually |
|---|---|
| `pid`, `tid`, `fd`, `uid`, `gid` | the five kernel identifier **types** (D-042) |
| `limit` | the verification keyword (`limit<Rules>`) |
| `any` | the type |
| `as` | a keyword |
| `comptime`, `derive` | keywords — so `mod:comptime;` and `mod:derive;` are not modules, and the loader reports `NITPICK-RESOLVE-005` at the `mod:` line as though the file were missing |
| `move`, `buffer`, `raw` | keywords that read like ordinary local names |
| `assoc` | the associated-type keyword (D-160) — so `bool:assoc;` is not a field |
| `on` | a keyword — so `Node?:on = nd;` is parsed as the expression `Node ? …` and fails at the `:` |
| `is_err`, `defaults`, `any` | keyword forms (`is_err(x)`, D-096), a struck-but-reserved operator word (D-167), and the type — each has cost a build as a local name |
| `channel`, `atomic`, `thread`, `joins` | the constructor, the type, the function modifier and the contract clause (D-181/D-182) — `thread` in particular reads like an ordinary noun |
| `error` | the declaration keyword (D-179) — so `error` is not a local name, and `Result`'s field is `.err` |
| `gives` | the factory contract clause (D-183 1.2.6) — a channel-returning creator hands its channels' reclaim to the caller |
| `Mutex`, `Guard`, `acquire`, `RwLock`, `RGuard`, `CondVar`, `Barrier` | the sync primitives' type keywords (D-056, 1.1.11) — `acquire` interns itself only after a `.` |
| `unit` | the unit-declaration keyword (D-196, 1.3.3) — `unit:Hertz = 1 / Seconds;`; it reads like the most ordinary local name in any measurement code |
| `trit`, `nit` | the single-digit ternary/nonary type keywords (D-197, 1.3.4) — like `acquire`/`any`, each interns itself as a NAME only after a `.` (the digit extraction `t.trit(i)`) |
| `oflags`, `prot`, `mflags`, `fmode` | the four flag-family TYPE keywords (D-044/D-230, 1.4.8) — `prot` and `fmode` in particular read like the most ordinary locals in any file code; their members (`O_RDONLY`, `PROT_READ`, `MAP_SHARED`, `S_IRUSR`, …) are prelude constants, so those names are taken too |
| `fails`, `end` | the `never fails` contract clause's second word (D-002/D-163) and the `when`/`then`/`end` control-flow family's terminator (LEXICAL_REFERENCE's keyword table) — each cost the 1.4.8 executor a build |
| `in`, `mod` | the `for … in` keyword and the module-declaration keyword (`mod:name;`) — each cost the 1.5.0 executor a build, as a local named `in` (a byte source) and one named `mod` (a module name) |
| `old`, `result`, `pure` | the verification keywords 1.5.1 added (D-243, D-245, D-242): `old(expr)` is a keyword operator (the value at entry), `result` a leaf keyword (the success value, `ensures` only — `Result` with a capital R is the type, and `result`'s token is `KwResultValue` for that reason), `pure` a contract clause. `old` was a local in the SMT encoder and a test, `result` a field in `FnSig` and four unit-test fixtures — every one renamed; each reads like the most ordinary local name there is |
| `sealed`, `hidden` | the field qualifiers 1.5.8b step 1 added (D-313, D-314): `sealed int64:bal;` is written only by the declaring module, `hidden` is not touched outside it. Both read like ordinary names — the compiler had a local and a field named `sealed`, seven tests an inline module named `hidden`, and five more a function or a local named `hidden` (all renamed: `after_wildcard`, `sealed_any`, `nested`, `secret`) |

The worst offenders are **gone**: before D-147 (0.9.9) the balanced and hex
literal forms could begin with a letter, so `an`, `bn`, `cn`, `dn`, `tt`,
`ban`, `FFhex` were numbers, and each cost an edit-build-fail cycle when used
as a name. Now **every numeric literal begins with a decimal digit** — those
are ordinary identifiers, and the values ride a value-neutral leading zero
(`0dn` is −4, `0FFhex` is 255). The legacy `0x`/`0b`/`0o` prefixes were
removed by the same decision.

Three more shapes that are not what a C or Rust habit expects:

- **Adjacent string literals do not concatenate.** `"a" "b"` is two literals, not
  one; use `string_concat`.
- **`discard(expr);` and `defer { … }`** take parentheses and no trailing
  semicolon respectively — `discard x;` and `defer { … };` are both parse errors.
- **A file's `mod:` name must match its basename**, or the loader reports
  `NITPICK-RESOLVE-005` at line 1 rather than anything about the name.

**`npkc` now means `src/npkc.npk`** — the harness builds and runs it for every
backend stage (IR on stdout; `llc` and `ld.lld` after, per BUILD_REFERENCE §4).
The `npkc` on PATH (`/usr/local/bin/npkc`) is still the *installed* C/C++
prototype's compiler, not this project's output; nothing installs ours yet.
(The prototype's source is `../ARCHIVE/nitpick-prototype/`.)

```
src/          # THE COMPILER — Nitpick source only; nothing else belongs here
  frontend/   #   built once, in full (analysis/, macro/)
  backend/    #   grown rung by rung (ir/, layout/)
  driver/     #   manifest, module graph, subprocess invocation
bootstrap/    # seed/ — THE COMMITTED SNAPSHOT: stage1.ll + STAMP + README (D-203).
              #   This is what builds src/ since 1.4.6; read seed/README.md
              #   before touching it. generator/ made the FIRST one and builds
              #   nothing now; harness/ runs beside `npkg` until meta/SWITCH.md
              #   retires it (D-206) — parity is a stage on every full run.
runtime/      # npkrt.ll — the runtime FLOOR, hand-written LLVM IR, PERMANENT
              #   (D-203). In every artifact; re-homed out of bootstrap/ at 1.4.6
tools/        # check/resolve_check/parse_check — the real frontend, for the harness
tests/        # FIVE rejection suites, named by the stage that refuses:
              #   modules/rejection/ (loader), types/rejection/ (type checker),
              #   analysis/rejection/ (a static analysis), expansion/rejection/
              #   (macro expansion), derive/rejection/ (the derive reader);
              #   the backend-rung suite retired at 1.5.4 when the last rung
              #   fell (S-47, D-271); nitpick.toml's [[test]] table is
              #   the one list of suites both runners read (D-238)
              #   accept/ is ONE suite for all of them — silence has no stage
              #   conformance/ (subset 1 compiles and runs), frontend/, grammar/
meta/specs/   # language specs — see below
meta/roadmap/ # the plan; meta/roadmap/done/ archives completed cycles
meta/LAYOUT.md# the tree, and why it departs from ../ARCHIVE/npkc-native
.internal/    # gitignored scratch area — never commit anything from here
```

**`src/inc/` is gone.** It was listed as "shared headers / includes"; Nitpick has
modules, not headers. See `meta/LAYOUT.md` for that and the four other departures
from the `npkc-native` decomposition, each with the decision that forced it.

`meta/specs/` holds ten `.md` reference documents carried over from
`../ARCHIVE/nitpick-next/meta/specs/` (the Gemini experiment), plus two written here:

- `PROTOTYPE_DELTA.md` — what changed between the prototype's specs and these,
  and which questions the carried-over set leaves open.
- `PRE_PLANNING_REVIEW.md` — safety concerns, cross-document contradictions,
  missing specs, and a suggested decision order. **Read this before planning
  implementation work.**

⚠️ **The carried-over specs contradict each other in several places** and have
not yet been reconciled — they were written for a separate experiment and some
content came from a verbal retelling of prototype-vs-new differences. Do not
treat any single one as authoritative without checking `PRE_PLANNING_REVIEW.md`
Part 3 first. Notably, the memory model (GC vs RAII) is **an open decision**, not
settled fact, despite `SPEC_GAPS_AND_AMBIGUITIES.md` reading as resolved.

## Why this language exists: Nikola

Nitpick will be released publicly for general safety-critical use, but that is
not its primary purpose. Nitpick is the **host language for Nikola**, a
physics-based AGI, and essentially every unusual decision in the language traces
back to Nikola's requirements. Without this context most of the pedantry looks
like over-engineering; with it, it is load-bearing.

**Nikola's intended users are why the safety bar is where it is.** The primary
use case is a companion for neurodivergent children, extending later to children
in long-term hospital stays, and eventually a teacher's-assistant role where each
student gets a tutor that can also help with homework at home. Several of these
goals involve **robotics**.

Repeated safety reviews of the engineering documents surfaced the finding that
drives the design: **small drift in numbers can produce behavior resembling PTSD
or schizophrenia**. Around vulnerable children that is categorically
unacceptable, and preventing it outranks schedule and effort.

Two language features follow directly:

- **`Result<T>` everywhere, no exceptions.** Errors are values the caller is
  forced to handle.
- **`exit` only from `main` or `failsafe`.** Anything uncaught must be caught by
  the runtime and routed through `failsafe` so shutdown is *controlled*. An
  uncontrolled stop with actuators live is a physical safety event, not a
  debugging inconvenience.

This is also the second, stronger reason for the zero-dependency rule below:
**past the FFI barrier the runtime cannot intercept a fault** and route it
through `failsafe`, which breaks the controlled-shutdown guarantee outright.

**Performance is a first-class requirement** — Nikola is computationally enormous
and will not reach intended speed until purpose-built hardware exists;
demonstrating viable performance is what funds getting there. **But performance
is explicitly subordinate to safety.** Never trade a safety property for speed.
Raise the tradeoff instead.

When a safety mechanism looks excessive or redundant, preserve it. The standing
instruction is that these requirements remain as they are or become *more*
pedantic if required.

## The hard constraint

Nitpick is a safety-critical language, and this compiler is subject to formal
verification requirements. Consequently:

**No external dependencies. No C, no C++, no Rust, no Python, no third-party
packages of any kind.** Everything in the trusted computing base must be
verifiable, and an unverified third-party toolchain or runtime breaks that
guarantee.

This is not a stylistic preference and it is not negotiable. When a task appears
to need a dependency, the correct response is to surface the tradeoff and design
an in-house replacement — never to quietly add one. Expect a large share of the
low-level work to be hand-written LLVM IR, which is the level at which the
project can do systems work without inheriting a runtime.

The build-out is therefore much larger than a compiler of comparable scope would
normally be. The prototype (see `../ARCHIVE/nitpick-prototype`) exceeded 50k lines *with* heavy
C/C++ dependency use; this implementation is expected to be bigger precisely
because those dependencies are being replaced with verifiable in-house code.

## Bootstrap strategy: the capability ladder

The frontend is built **once, in full**. The backend is grown **incrementally**,
rung by rung. The entire point of this arrangement is to avoid rewriting the
parser at every bootstrap stage — a failure mode that the predecessor efforts hit
repeatedly.

**"Built once" means do not REBUILD — not do not extend.** The failure this was
written against is concrete: the previous attempt's parser supported only certain
keywords at each step, so every rung meant going back and teaching it more
syntax. The fix was to have the parser accept the whole language and forward what
it does not yet understand, letting the BACKEND carry the incompleteness as a
named refusal. That is where the ladder lives.

Practical consequence when proposing changes: treat the frontend as the stable
component and the backend as the part that advances. A change that would require
reworking the lexer/parser/AST **to unblock a backend rung** is almost always the
wrong shape — the rung should refuse by name instead. But **adding a production
because the LANGUAGE genuinely needs one is ordinary work**, not a violation of
this; judge it on its real costs (node-kind coverage across every walker,
verification surface, downstream obligations). Raise it either way, because a
grammar change is a language change and those are the user's call.

## Memory model

The language has four allocation regimes, spelled as modifiers in
source. **`DECISIONS.md` is the authority here**, not
`../ARCHIVE/nitpick-prototype-docs/specs/memory_specs.txt`, which still describes a collector this
language does not have:

| Modifier   | Regime                                                      |
|------------|-------------------------------------------------------------|
| *(default)* | Managed — static ownership, RAII at scope exit               |
| `stack`    | Stack-scoped                                                 |
| `wild`     | Unmanaged / manual (paired with `defer` blocks and `nodrop`) |
| `wildx`    | Executable memory — W^X, backs the JIT                       |

**There is no `gc` and no tracing collector.** D-003 dropped both: static
ownership covers unique and scoped data, and **arenas with `Handle<T>`** cover the
graph-shaped and cyclic data a collector would otherwise be needed for. `gc` is
not a keyword in the lexer.

This table listed `gc` as a fifth regime until cycle 0.5.3, and the default row
read "implicit GC / RAII" — both carried over from the prototype, both
contradicting a decision settled long before.

Anything touching allocation, lifetimes, drop semantics, or codegen for
references must be reasoned about against **all** of these, not just the one
being edited. `wildx` in particular carries the W^X invariant: a page is never
simultaneously writable and executable, and the JIT depends on that transition
being correct.

## What this replaces, and what must change

`../ARCHIVE/npkc-native` is the direct predecessor and the most useful structural
reference: a self-hosted Nitpick frontend (`.npk` sources) organized as
`src/frontend/`, `src/backend/`, `src/driver/`, `src/tools/`. Its module
breakdown — `lexer`, `parser`, `type_system`, `type_checker_*`, `borrow_checker`,
`symbol_table`, `module_{table,resolver,loader}`, `diagnostics`, `source_location`
— is a reasonable starting decomposition.

**But its backend is exactly what this project exists to eliminate.** `npkc-native`
reached the C++ nitpick backend (LLVM 20, Z3, IKOS) through an FFI bridge
(`src/backend/ffi_bridge.npk`). That bridge, and everything behind it, is
disallowed here. Read `../ARCHIVE/npkc-native/MAPPING.md` for the frontend
decomposition; ignore its backend arrangement.

One transferable frontend technique documented there: Nitpick has no OOP
inheritance, so the C++ AST class hierarchy is expressed as tagged enums over
composable structs rather than base/derived nodes.

## Reference material (read-only, archived)

**Moved to `../ARCHIVE/` on 2026-09-02** — the user's tidy-up executed the
repository half of `meta/SWITCH.md` early: the prototype is archived on GitHub as
`alternative-intelligence-cp/nitpick-prototype`, its documentation as
`nitpick-prototype-docs`, the rest archived or deleted. All of it stays
browsable and must never be modified:

- `../ARCHIVE/nitpick-prototype-docs/specs/` — the PROTOTYPE's language
  specification, split by topic (`memory_specs.txt`,
  `formal_verification_specs.txt`, `safety_systems_specs.txt`,
  `compiler_specs.txt`, `pointer_system_specs.txt`, `traits_oop_specs.txt`, …).
  `FULL_specs.txt` is the ~14k-line consolidated version. **It was the authority
  on language semantics when this repo opened; `meta/specs/` and `DECISIONS.md`
  are the authority now**, and where they disagree with it the difference is
  recorded in `PROTOTYPE_DELTA.md`.
- `../ARCHIVE/nitpick-prototype-docs/reference/COMPILER_ARCHITECTURE.md` —
  pipeline walkthrough for the C++ prototype (preprocessor → lexer → parser →
  type/borrow check → IR gen). Good for *what the stages do*; its
  implementation is dependency-laden and is not a model to copy. The same
  directory has `TYPE_SYSTEM_DESIGN.md`, `TRAITS_AND_BORROW_SEMANTICS_RFC.md`,
  `UNDEFINED_STATE_PREVENTION.md`, `GC_TUNING_GUIDE.md`, `abi.md`,
  `RESERVED_WORDS.md`.
- `../ARCHIVE/nitpick-prototype/` — the ~26k-file C/C++ prototype compiler.
  Useful as a behavioral oracle; its dependency choices are **not** precedent.
- `../ARCHIVE/nitpick-proofs/` — verification harnesses (`esbmc/`, `frama-c/`,
  `smt/`).
- `../ARCHIVE/nitpick-bootstrap/`, `../ARCHIVE/nitpick-next/` — earlier
  bootstrap attempts; `../ARCHIVE/npkc-native/` — the frontend decomposition
  above.

## Ecosystem conventions

- Source extension is `.npk`; package manifest is `nitpick.toml`.
- `npkc` is the compiler, `npkpkg` the package manager. Both resolve on PATH
  today (`/usr/local/bin/`) as the prototype's INSTALLED binaries, so treat
  them as the *old* compiler, not this one.
- **The repository is `alternative-intelligence-cp/nitpick`** (renamed from
  `nitpick-native` on 2026-09-02; the local checkout is renamed at the 1.4
  close). Everything lives under that organisation, never under a personal
  account. `alternative-intelligence-cp/nitpick-docs` exists again, empty: the
  home of the OFFICIAL documentation (HTML, man pages, Markdown), built after the
  compiler is finished — until then `meta/specs/` is the specification.
- **LLVM 20.1.2** is the toolchain, matching the version the prototype targets.
  Ubuntu/Mint ship only versioned binaries (`llc-20`, `opt-20`, …) because LLVM
  14, 18, and 20 coexist on this machine, so unversioned names are provided by
  symlinks in `~/.local/bin` pointing into `/usr/lib/llvm-20/bin`. Available
  unversioned: `llc`, `opt`, `lli`, `llvm-as`, `llvm-dis`, `llvm-link`,
  `llvm-config`, `llvm-extract`, `llvm-reduce`, `bugpoint`, `llvm-jitlink`,
  `llvm-mc`, `llvm-objdump`, `llvm-readelf`, plus `FileCheck` / `not` /
  `split-file` for test harnesses. `clang` is on update-alternatives and already
  resolves to 20.
  - Verify with `llvm-config --version` (expect 20.1.2). If unversioned names
    stop resolving, the symlinks are the thing to check, not the packages.
  - `lld-20` is installed and symlinked; `ld.lld --version` reports 20.1.2, so
    the linker is version-matched with the rest of the toolchain.
- Note the naming migration in flight across the docs: older material uses
  earlier project names. Prefer current naming in new code.
