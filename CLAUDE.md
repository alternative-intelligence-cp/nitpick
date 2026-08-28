# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Status: PHASE C UNDERWAY — cycle 1.2 COMPLETE; the managed regime is real

The **specification set is complete** — `meta/specs/` holds twenty-one documents and
`DECISIONS.md` records 183 settled decisions. The **plan is in `meta/roadmap/`**,
organised as numbered cycle folders holding `x.y.z.md` subcycle files; finished
cycles move to `meta/roadmap/done/`. Start at `meta/roadmap/ROADMAP.md`.

**Cycles 0.0–1.0 are done** (`meta/roadmap/done/`): the lexer, the AST and parser,
the module/symbol/visibility passes, the type system, the static analyses, macros
with `comptime` and `#[derive]`, IR emission, `nlibc` and the runtime floor, full
type lowering, the memory allocator, and generics/traits/`dyn`. **`npkc` exists**:
`src/main.npk` over `src/driver/pipeline.npk` (the one front-half sequence;
`tools/check.npk` is a thin wrapper over it) and `src/backend/`. The harness runs
**112 real-backend programs** and asserts 8 `NITPICK-RUNG-001` rejections on every
full run, and **stage 1 rebuilds itself byte-identically** — the fixpoint has held
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
Astrée 1.6. Adopting `dyn Writer` in npkc's own diagnostics is 1.4
material. **The 1.1 interlude closed the self-contained backlog**: `sys` is
TYPED (D-192 — `Result<int64>` by D-048's contract, register-shaped
arguments refused by name, zext for unsigned/kernel args, `?|` given `?!`'s
unknown-operand fallback; the S-3 rows), and float/char `ToString` landed
(D-193, §6b — shortest-round-trip Dragon4 over `uint2048` in the PRELUDE,
no wide division by construction, 353 python-generated known-answer
vectors; total UTF-8 with U+FFFD for non-scalars). The bare-builtin
signature table is proposed as P-3 (OPEN_DECISIONS §3), owed before 1.4.
**Cycle 1.3 (the exotic numeric tier) is UNDERWAY** — 1.3.0 ratified the
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
as tfp's has since 1.3.2.

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
the lowering), `check_runtime_sigs_agree` (npkrt.ll vs seed vs ir_runtime,
three ways), and `check_zero_dependency` (the undefined-symbol scan). An eighth,
`check_decisions_current`, REPORTS rather than fails: stale decision-log
candidates print on every full run for the doc-sync pass. They diff the compiler
against the thing that describes it, which is how cycle 0.6 found every one of
its holes — none was found by a test.

### Building and testing

```
python3 bootstrap/harness/harness.py                    # everything, ~20 minutes
python3 bootstrap/harness/harness.py --only type_stmt   # one test, ~1 minute
```

It compiles each suite with the **throwaway Python seed** in
`bootstrap/generator/` (D-085 — a generator, never a dependency of the artifact),
links against `bootstrap/runtime/npkrt.ll` via `llc` and `ld.lld`, runs the
result, and compares the exit code. It also feeds every source through the **real**
parser (`tools/parse_check.npk`) and re-checks that every AST node kind is
reachable.

For the middle of a subcycle, where the question is "does this one rule fire on
this one file", there is a faster loop that builds the checker once:

```
python3 bootstrap/harness/quickcheck.py tests/types/rejection/borrows.npk
```

And the same loop for the backend — build `npkc` once, then compile, link and
RUN programs with it, printing the exit code (or the refusal, or llc's first
error; `--ir` prints the IR too, `--keep` leaves the `.ll`/`.o`/binary behind):

```
python3 bootstrap/harness/quickemit.py tests/backend/programs/dyn_slots.npk
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
The `npkc` on PATH is still the *old* C++ prototype (`../nitpick/build/npkc`), not
this project's output; nothing installs ours yet.

```
src/          # THE COMPILER — Nitpick source only; nothing else belongs here
  frontend/   #   built once, in full (analysis/, macro/)
  backend/    #   grown rung by rung (ir/, layout/)
  driver/     #   manifest, module graph, subprocess invocation
bootstrap/    # THROWAWAY seed + generator (D-085) — never in an artifact
tools/        # check/resolve_check/parse_check — the real frontend, for the harness
tests/        # FOUR rejection suites, named by the stage that refuses:
              #   modules/rejection/ (loader), types/rejection/ (type checker),
              #   analysis/rejection/ (a static analysis), rejection/ (backend rung)
              #   accept/ is ONE suite for all four — silence has no stage
              #   conformance/ (subset 1 compiles and runs), frontend/, grammar/
meta/specs/   # language specs — see below
meta/roadmap/ # the plan; meta/roadmap/done/ archives completed cycles
meta/LAYOUT.md# the tree, and why it departs from ../npkc-native
.internal/    # gitignored scratch area — never commit anything from here
```

**`src/inc/` is gone.** It was listed as "shared headers / includes"; Nitpick has
modules, not headers. See `meta/LAYOUT.md` for that and the four other departures
from the `npkc-native` decomposition, each with the decision that forced it.

`meta/specs/` holds ten `.md` reference documents carried over from
`../nitpick-next/meta/specs/` (the Gemini experiment), plus two written here:

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
normally be. The prototype (see `../nitpick`) exceeded 50k lines *with* heavy
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
`../nitpick-docs/specs/memory_specs.txt`, which still describes a collector this
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

`../npkc-native` is the direct predecessor and the most useful structural
reference: a self-hosted Nitpick frontend (`.npk` sources) organized as
`src/frontend/`, `src/backend/`, `src/driver/`, `src/tools/`. Its module
breakdown — `lexer`, `parser`, `type_system`, `type_checker_*`, `borrow_checker`,
`symbol_table`, `module_{table,resolver,loader}`, `diagnostics`, `source_location`
— is a reasonable starting decomposition.

**But its backend is exactly what this project exists to eliminate.** `npkc-native`
reached the C++ nitpick backend (LLVM 20, Z3, IKOS) through an FFI bridge
(`src/backend/ffi_bridge.npk`). That bridge, and everything behind it, is
disallowed here. Read `../npkc-native/MAPPING.md` for the frontend decomposition;
ignore its backend arrangement.

One transferable frontend technique documented there: Nitpick has no OOP
inheritance, so the C++ AST class hierarchy is expressed as tagged enums over
composable structs rather than base/derived nodes.

## Reference material (read-only siblings)

These live outside this repo and must never be modified:

- `../nitpick-docs/specs/` — the language specification, split by topic
  (`memory_specs.txt`, `formal_verification_specs.txt`, `safety_systems_specs.txt`,
  `compiler_specs.txt`, `pointer_system_specs.txt`, `traits_oop_specs.txt`, …).
  `FULL_specs.txt` is the ~14k-line consolidated version. **This is the
  authority on language semantics.**
- `../nitpick-docs/reference/COMPILER_ARCHITECTURE.md` — pipeline walkthrough for
  the C++ prototype (preprocessor → lexer → parser → type/borrow check → IR gen).
  Good for *what the stages do*; its implementation is dependency-laden and is
  not a model to copy.
- `../nitpick-docs/reference/` — also has `TYPE_SYSTEM_DESIGN.md`,
  `TRAITS_AND_BORROW_SEMANTICS_RFC.md`, `UNDEFINED_STATE_PREVENTION.md`,
  `GC_TUNING_GUIDE.md`, `abi.md`, `RESERVED_WORDS.md`.
- `../nitpick/` — the ~26k-file C/C++ prototype compiler. Useful as a behavioral
  oracle; its dependency choices are **not** precedent.
- `../nitpick-proofs/` — verification harnesses (`esbmc/`, `frama-c/`, `smt/`).
- `../nitpick-bootstrap/`, `../nitpick-next/` — earlier bootstrap attempts.

## Ecosystem conventions

- Source extension is `.npk`; package manifest is `nitpick.toml`.
- `npkc` is the compiler, `npkpkg` the package manager. Both resolve on PATH
  today — `npkc` points at the prototype build (`../nitpick/build/npkc`), so
  treat it as the *old* compiler, not this one.
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
