# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Status: PHASE C UNDERWAY — cycle 1.4 (self-hosting) COMPLETE: the compiler is self-hosting under D-202, `npkg` runs beside the harness, and cycle 1.5 (verification) is NEXT

The **specification set is complete** — `meta/specs/` holds twenty-one documents and
`DECISIONS.md` records 240 settled decisions. The **plan is in `meta/roadmap/`**,
organised as numbered cycle folders holding `x.y.z.md` subcycle files; finished
cycles move to `meta/roadmap/done/`. Start at `meta/roadmap/ROADMAP.md`.

**Cycles 0.0–1.0 are done** (`meta/roadmap/done/`): the lexer, the AST and parser,
the module/symbol/visibility passes, the type system, the static analyses, macros
with `comptime` and `#[derive]`, IR emission, `nlibc` and the runtime floor, full
type lowering, the memory allocator, and generics/traits/`dyn`. **`npkc` exists**:
`src/main.npk` over `src/driver/pipeline.npk` (the one front-half sequence;
`tools/check.npk` is a thin wrapper over it) and `src/backend/`. The harness runs
**172 real-backend programs** (each also re-run through `opt -O2` + `llc -O2`
since 1.3.8) and asserts 6 `NITPICK-RUNG-001` rejections on every full run (8 until 1.4.7's
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
deterministic, with a poisoned-source tripwire in `npk_string_concat`.
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
absolute `src/main.npk` argument embeds the build path into 1,489 of 1,647
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
BUILDER, has the builder compile `src/main.npk` into the compiler under test
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
z3 smt.random_seed=0 sat.random_seed=0 rlimit=20000000 -smt2 D/0001.smt2   # one file, the profile spelled out
```

The harness's `verify` stage does the same over `tests/verify/` (each file
names its rows with `// expect-obligation: KIND VERDICT N`, exactly) and over
the compiler itself, and its `parity` stage byte-compares `npkg`'s verified
compiler with its own. A verdict that moves is a red run, never a rebaseline
(D-040): run `--record` only in the commit that changes what is proven.

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

**`npkc` now means `src/main.npk`** — the harness builds and runs it for every
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
tests/        # SIX rejection suites, named by the stage that refuses:
              #   modules/rejection/ (loader), types/rejection/ (type checker),
              #   analysis/rejection/ (a static analysis), expansion/rejection/
              #   (macro expansion), derive/rejection/ (the derive reader),
              #   rejection/ (backend rung); nitpick.toml's [[test]] table is
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
