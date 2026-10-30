# BUILDING_AND_TESTING.md — how the compiler is built and tested, and every gotcha in full

Moved VERBATIM out of `CLAUDE.md` at landing 157 (Q70, 2026-10-23). READ IT before any chain, record, harness or landing.
`CLAUDE.md` carries one digest line per gotcha below, ending with the gotcha's bold lead quoted so it can be found here. A new
gotcha's full text is added at the end of the list below, and its digest line to `CLAUDE.md`.

---

### Building and testing

```
python3 bootstrap/harness/harness.py                    # everything: about THREE HOURS since 1.5.8b step 3 (below)
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
.internal/quickemit/p_main_npk test                           # every suite, ~2.5 hours (the verify sweep); --only SUBSTR to iterate
.internal/quickemit/p_main_npk test --selfcheck               # the runner self-check alone (§7.1)
.internal/quickemit/p_main_npk test --verdicts out.txt        # plus one verdict line per unit (the parity diff's input)
```

**A full run takes about three hours, and most of it prints nothing** (corrected
2026-09-23; this file said "~20 minutes" and "~25 minutes" from 1.4.8 until then):
1.5.8b took the compiler's manifest from 368 rows to more than 3,000, so the
verify sweep decides an order of magnitude more per program — about 47 seconds
each over 115 programs, twice (the harness's own `verify` stage and the `parity`
stage's `npkg test`), and the `parity` stage prints nothing until it ends. A log
quiet for an hour is normal; check the process's live children, not the log.
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
  it. **The same rule found DEF-92** (1.5.8c step 4b): the sweep's measures
  read as a 28% frontend cost and were field reads on generic instances
  reaching `tt_instance`, the third linear scan of the type table (85% of the
  checker's instructions); its index made the whole frontend 5.4x faster. When
  a cost appears with a change, profile over `npkg/main.npk` (callgrind, ~15
  min) and read the EXCLUSIVE top before reading the change.
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
  `meta/roadmap/done/1.5/tools/model_bfs.py` reads a model's whole reachable space
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
  (`meta/roadmap/done/1.5/tools/explore_prototype/hashcmp.sh`, the harness-built
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
- **A declaration's flags live where its kind says, and a slot read as a flag
  word must BE one** (DEF-93, 1.5.8c step 3; the 1.4.8 global's lesson again):
  `decl_flags` answers `d.a` for most kinds, a global's and a rule's `c` high
  half. A `Rules` declaration's `pub` was never stored until this step, so its
  export was the parity of its subject type's node index -- true by coincidence
  for a cycle, false the day the prelude's AST moved. When a kind's `a` holds a
  type or a window, give it a flags home before its first `pub` is written, and
  test the visibility both ways (`rules_pub.npk`, `rules_private.npk`).
- **An expectation is a `//` comment of its OWN LINE** (1.5.8b step 4): both
  runners read `// expect-error: CODE` and `// expect-error-at: N` only from a
  comment that is the whole line, and a file in a rejection suite with no
  PARSED expectation is treated as a FIXTURE and skipped in silence. Writing the
  expectation after the code (`discard(x);  // expect-error-at:14 …`) therefore
  runs nothing: `wrap_kinds.npk` asserted sixteen refusals and ran zero, and only
  `check_codes_tested` noticed, because its code was new. Both runners now refuse
  a file that spells `expect-error` after code, by name.
- **A per-process temp path spells its pid FIXED-WIDTH** (`sys_pid_tag()`, DEF-137; 1.6.1d
  step 2): an explored program's schedule hash counts the allocator's steps, and a path one
  byte longer crosses a size class -- when the pid counter wrapped from seven digits to
  fewer between two replays of one seed, the replay belt called the explored build
  nondeterministic (landing 72's red). A red run is READ: this one's cause was a wrap
  between two replays, in a test program's own spelling, not in the landing's change.
- **A counted loop's operands are widened by their own signedness, `till` never infers a
  direction, and a bound is an integer that fits the `int64` counter** (DEF-129, DEF-130,
  DEF-135, DEF-136; 1.6.1d step 2): `loop(100u8, 200u8, 1u8)` counts up 100 times now
  (it counted DOWN 156), `till(-3i64, 1i64)` runs zero times at run time and in a
  `comptime` body alike (D-022's table), a `uint64` bound past 2^63 traps `IntOverflow`
  at the head (a program with one names the arm), a `tbb` bound holding ERR traps
  `TbbErr` (it ran the loop zero times in silence), and a 128-bit, float, bool or char
  bound is TYPE-068. The two guards are rows (`overflow`, `err-exit` at the operand).
- **A store through a pointer drops the old pointee when the pointer names MANAGED
  storage** (DEF-120, 1.6.1d step 3): `(<-p) = v` releases what `p` points at before
  the store, as `s.f = v` and `l[i] = v` do -- unless `p` is a `wild` binding,
  parameter or field, an `=>! wild` cast, `#ptr_add` or an allocator's result, where
  it drops nothing (manual storage holds no value until the author writes one). The
  reading is `wild_places.npk`'s, the escape analysis's own (D-223): a plain `T->` is a
  live managed value by contract, so a `wild` block cast to a PLAIN pointer by
  `=>! T->` and then stored through is dropped like any other -- spell the cast
  `=>! wild T->` for storage that holds no value yet.
- **A moved-out whole binding is VACANT** (DEF-120, 1.6.1d step 3): `move(x)` and `pass
  x` zero the slot and run the vacant fixup, as a field's or element's move has since
  D-254 -- a held `@x` then reads the vacant value, never the stale header. Every
  `move`/`pass` of an owning local moves the emission's text.
- **A consuming `pick`'s arm is a SCOPE, and its bindings drop at the arm's end**
  (DEF-121, 1.6.1d step 3): a read-only arm's payload is freed when the arm completes,
  an exit inside the arm (`pass`, `break`, `give`, `fall`) drops it there, a moved-out
  binding leaves with the move. In a coroutine the binding's flag is the frame byte at
  role `50 + ordinal` on the arm's statement (`scan_pick_binds` reserves it beside the
  slot at `7 + ordinal`; an alloca dies at a suspension inside the arm). The EXPRESSION
  form takes its consuming selector off the statement's temporaries as the statement
  form does (DEF-141): before it `give move(x)` handed out a body the statement freed.
- **A `where` guard decides and does not consume; a `fall` lands on an arm that binds
  nothing** (DEF-138, DEF-139, DEF-140; 1.6.1d step 3): `move(x)` of the arm's own
  binding inside its guard is `NITPICK-TYPE-089` (a failing guard hands the payload to
  the next arm, which bound and freed it a second time), `fall two;` into an arm whose
  pattern binds a name is `NITPICK-TYPE-090` (the target's pattern is never matched), and
  a `fall` to a label no arm of the `pick` carries is RESOLVE-002 (it was EMIT-002).
- **A consuming `pick`'s binding is a binding to the move rules** (DEF-118, 1.6.1d
  step 1): `pick (move(e)) { (Som(x)) { … } }` binds an owner; a second `move(x)` or a
  read after one is MOVE-001, and an arm inside a loop re-binds the name each
  iteration. The lending form's binding is a view and was never movable.
- **A write THROUGH a `$$i` holder is BORROW-013** (DEF-123, 1.6.1d step 1): `(<-p) = v`,
  `move(<-p)`, `@(<-p)` handed to a writer and a `Self->` call on `<-p` with `p = $$i x`, and
  a `$$i` claim (or a pointer holding one) handed to a callee whose summary writes through
  that position — a `dyn` method or a function value counts as writing. A reader spells
  `$$i` to a callee that writes nothing, or `@`; a writer claims `$$m`. And a `move(<-p)`
  inside a callee is a WRITE of the pointee in its summary (DEF-119): `@x` to such a callee
  while a view of `x` lives is BORROW-015.
- **A `for` binding ends with its loop, and a by-value receiver through a pointer is the
  pointee** (DEF-127, DEF-134; 1.6.1d step 1): the emitter releases the loop name at the
  loop's end (an outer name of the same spelling reads its own slot after it), and
  `q.peek()` with `Box->:q` and `peek = int64(Box:self)` loads the pointee before the call —
  the checker always admitted the shape; the emitter had passed the pointer's bits.
- **A rejection file's silent site is invisible to D-237's set rule** (1.6.1d step 1's
  sweep, S-113): `aliasing.npk` documented a shape its checker never reported for a cycle,
  because the code appeared elsewhere in the file. When a rule lands, sweep the tree under
  BOTH checkers and read every appeared site — a test file's own case may be among them.
- **ANY `src/` change moves `nitpick.obligations`, and the in-process checks cannot
  see it** (1.6.0 step 3b, found by a red harness): a row's hash is its problem
  text, and that text names types by their INTERNED IDS (`|npk.f.<TYPEID>.field|`,
  the sorts), allocated in interning order across the whole program — so a
  fourteen-line change in `reach.npk` that interned one new type renumbered every
  type interned after it and re-keyed 82 rows of `smt_encode`, `ir_expr` and
  `reach` with no verdict moving. Every `src/` change runs `npkg verify --record`
  in its commit and quotes `manifest_gate.py`'s two zeros (verdicts moved among
  shared rows, discharged counts fallen); "the manifest unchanged" is a
  measurement the harness's verify stage makes, never an assumption.
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
  1.6). So a `bounds` row discharges for a slice, a string, a buffer (indexable
  since D-341, landing 128: `tests/verify/bounds_buffer.npk`) or a fixed
  array whose length is named and stable, and a `prove`-like bound on an index
  (a `limit<Rules>`) discharges too. A float's `=>!` cast to an integer carries
  a `cast-range` row over the OPERAND; the resulting integer stays opaque.
- **A guard inside a `defer` body is one row and several traps** (DEF-82,
  1.5.8b step 5): the walk sees the body once, the emitter writes it at every
  exit that runs it, and the row's `traps` field is one copy's times the copies.
  When adding a construct the emitter may write more than once, ask what the
  belts will count — they compare the IR's traps and assumes against the rows,
  and they fail closed.
- **Every length is at or below 2^47, and the allocator is why** (D-308 §7,
  1.5.8b step 6c): a request above 140,737,488,355,328 bytes is `HeapBadRequest`
  at every allocator entry — ONE unsigned compare in each of `npk_alloc_impl`,
  `npk_fs_alloc` and `npk_aalloc` (the aligned entry bypasses the core, so a
  single check would leave a hole), which is also the sign check. Exactly
  2^47 is legal and the kernel answers `HeapOom`. The four spellings of the
  number (the floor's three compares, the prelude's `ListLen`, `cast_bounds.npk`'s
  `len_ceiling`, the spec's promise on `npk_alloc_internal`) are held to one
  value by `check_len_ceiling_agree`. The allocator core is a `boundary`
  symbol: the ceiling is a PROMISE kept by one instruction and accepted at
  TCB.md §5 (10), never a row z3 decides — do not write "proven" of it.
- **`string_from_bytes` and `#wild_slice` are guarded, and arm `OutOfBounds`**
  (D-308 §7; K-18): the two producers of a length no allocation makes hold it
  to `[0, 2^47]` at the CALL — the emitter's guard keyed on the call node, the
  encoder's `bounds` row under the same key, REACH's arm through
  `builtin_caller_len` (the one predicate all three read). A narrow count is
  widened BY ITS SIGN first (DEF-87: an `int32` variable count reached `llc`
  as a type error on every compiler before 6c). A program calling either
  names `(OutOfBounds)`.
- **A `List`'s `count` and `cap` carry `limit<ListLen>`, so a program that
  pushes to one names `(LimitViolated)`** (D-308 §6, 1.5.8b step 6c): the
  prelude's own writes are checked (they can never fire — `count < cap <=
  2^47` by the ceiling — but REACH is syntactic, as it is for the program's
  own `+`), and 63 of the tree's roots learned the arm at 6c. `ListLen` is a
  prelude-owned name (D-239) and `pub`: a library container may carry it.
- **A prelude RULE's predicate is an item** (found at 6c): `emit_program.npk`
  brackets it as it brackets a function, or D-262's trim keeps
  `@"npk.prelude.ListLen"` in a program that never touches a `List` and
  `check_prelude_trimmed` fails on the first such program. Anything new the
  prelude emits per module — a predicate, a vtable, a helper — needs the same
  bracket.
- **The encoder's length fact must look THROUGH a pointer** (found at 6c):
  `b.cap` on a `buffer->` is auto-dereferenced (D-098), so the operand's type
  is the pointer's; a fact keyed on `has_len_term(operand type)` missed it and
  `b.cap + 1` stayed open. Any fact pushed at a member read should ask the
  pointee's kind when the base is a pointer.
- **A `while`/`when` loop states why it ends** (D-304, 1.5.8c step 1):
  `while (c) decreases n - i invariant P { … }` or `while (c) unbounded { … }`,
  the measure clause BEFORE `invariant`, exactly one of the two; `for`, `loop`
  and `till` take neither (TYPE-072). The measure is a plain integer
  (TYPE-073), checked at the top of the body each time the condition holds --
  below zero (signed) or not below the previous trip traps `DecreasesViolated`,
  so a program with a `decreases` names `(DecreasesViolated)`. A loop with NO
  clause is TYPE-072 since step 4 (the tree was swept at step 3). The `terminate`
  rows (entry, preservation, one per `continue`) discharge a counter loop's
  measure; a verify test names them (`expect-obligation: terminate discharged N`).
- **The frame roles 42, 43 and 44 are the measures'** (1.5.8c steps 1 and 4):
  a coroutine's `while`/`when` with a checked `decreases` keeps the previous
  measure and the first-visit flag in the frame (42, 43), and a coroutine of a
  recursive group that states a measure keeps its entry measure `m0` at role
  44 on the body's block. Roles in use on a statement: 0–7+, 20–25, 30+k
  (channel stashes), 41 (join mark), 42/43/44, 50+k (a consuming `pick` binding's
  flag byte, 1.6.1d step 3), 60+k (`old`).
- **A FUNCTION's `decreases` is checked at every call inside its recursive
  group** (D-304 (5), 1.5.8c step 4): `func:fact = int32(int32:n) decreases n
  never fails { … }` -- the clause among the contracts, before `never fails`.
  The groups are Tarjan over the checker's recorded calls (a `dyn` or a
  function-value call is no edge), so a measure on a function nobody in its
  group calls back is TYPE-075, a cyclic group with a measure on some member
  and none on another is TYPE-074, and a function measure wider than 64 bits
  is TYPE-073 (the members' measures compare in one `i128`; a loop's keeps
  any width). The check is `m0 >= 0 && m1 < m0` before the call -- the callee's
  measure over the arguments against the caller's at entry, through the
  generated `<sym>.measure` predicate -- so a call under `n > 0` with `n - 1`
  discharges its `terminate` row and a call under `n != 0` alone does not
  (`m0 >= 0` is open). A recursive call inside a `requires` clause is not
  checked (the predicate has no snapshot) and its row is `unencoded` with no
  trap. The `stack-depth` row (kind 13, `d` in `rows.txt`) is DERIVED by both
  runners after all files are decided -- one per cyclic group, `discharged`
  iff the group is measured and every call row discharged -- and `index.txt`
  has FIVE fields now (`NNNN symbol checks group measured`): a planted
  obligations directory (a self-check case, a tool) must write all five.
- **Every `while`/`when` in the tree states its clause, and a new one must**
  (1.5.8c step 3; TYPE-072's `neither` shape refuses from step 4). Write the
  measure with the loop: a counter's `bound - i`, a scan's bytes left
  `len - pos`, a walk along a chain built in order `count - c`, the parser's
  tokens left `raw p_left(p)`, a doubling under `c < cap` `cap - c`,
  `while (v > 0)` is `decreases v`, a hash probe carries a probe counter
  bounded by the table; a call as the bound is HOISTED into a local before
  the loop (`int32:n = raw f(x); while (i < n) decreases n - i`), never
  written as the measure unless the callee is `pure never fails` (TYPE-060:
  `p_left`, `item_member_count`, `d_at`, `ast_id_at`, the round constants are).
  A worklist that grows as it drains, a fixed point, a loop bounded by a
  deadline, a spin on the clock and an event loop are `unbounded` with the
  reason on the line above (D-316); a measure is never a trip budget. The
  reader's record is `meta/roadmap/done/1.5/tools/decreases_read.txt`; the tool
  (`decreases_sweep.py` over `loop_dump.npk`'s dump, built with `quickemit
  --keep`) writes the provable shape and applies the record.
- **A measure decides only over STABLE terms, so write the bound as a local**
  (1.5.8c step 5's measurement): `x.count - i` with `x` a POINTER parameter is
  a fresh opaque term at every read (DEF-14: a callee holding the pointer may
  write it), and so is a field of a BY-VALUE aggregate or a `raw f(s)` over
  one, because an aggregate has no value term yet (E-6). The sweep's `hoist`
  idiom — `int64:n = x.count;` before the loop where the body cannot change
  it, `decreases n - i` — is what the solver can read; a measure the check
  keeps is still a correct measure, only unproven. The residue by cause is
  `python3 meta/roadmap/done/1.5/tools/residue.py ROOT OUT.txt` over a tree's
  `build/verify/obl`.
- **A `decreases` in a `comptime` body is checked by the evaluator** (1.5.8c
  step 3): `fold_while` evaluates the measure each trip and a violation is a
  counterexample, TYPE-069 at the loop, as a `prove` there is (L-18).
- **Every `failsafe` names `(DecreasesViolated)`** (1.5.8c step 3): the
  prelude's loops carry measures, so nearly every program can reach it; the
  runners' generated failsafes (harness.py, selfcheck.py, npkg/suites.npk,
  npkg/selfcheck.npk) carry the arm too. REACH-002 names it where a root
  forgot.
- **Two hoisted counts in one scope collide** (1.5.8c step 3): the sweep's
  `hoist` declares `T:NAME` before each loop it decides, and two loops over
  the same window in one function are RESOLVE-001 -- keep the first local and
  drop the second declaration; the loop then reuses it.
- **The generated prelude is regenerated after a sweep of `src/prelude/`**
  (1.5.8c step 3): the compiler embeds `prelude_source.npk`, so a compiler
  built before `gen_tables.py` ran carried the UNSWEPT prelude and every
  smoke run over it measured nothing about the prelude's loops.
- **Emitting a branch to the label that follows does not mark the block
  terminated** (1.5.8c step 1): `x.fe.terminated = true` after a `br` makes the
  emitter skip everything up to the next `s_start`, and the elided measure
  check silently dropped the loop body that way -- the assume belt (fewer
  assumes than discharged guards) and the verified binary's exit code both
  caught it. Set the flag only where the block really ends (a trap's
  `unreachable`, a `ret`), and let `irw_label` open the next one.
- **A test's join deadline is a HANG NET, never a verdict** (DEF-85, 1.5.8b
  step 7): it bounds the trap route's stop walk (D-291), and under load a tight
  five-second bound made `failsafe_alloc.npk` answer the re-entry exit once in
  forty runs. Choose a bound only a real hang reaches (sixty seconds there), as
  the solver's net (D-297) and the explorer's control net (DEF-83) are chosen;
  a red that is a deadline under load is READ, not re-run.
- **A program test's temp path carries its pid** (DEF-91, 1.5.8b step 7): a
  literal `/tmp/npk_<name>` is shared by every concurrent run of that program,
  and four harnesses in four worktrees made `dyn_stream` and `fs_basic` answer
  their own `(E9)` exit through a truncated file. Spell it
  `string_concat("/tmp/npk_<name>.", int_to_string(sys(raw SYS_GETPID()) ?! E9))`
  and unlink it at the end (`scrub`); `trap_stops_runner.npk` is the model.
- **An accepted file is also EMITTED** (1.5.8b step 7): `tests/accept/`'s
  verdict is the frontend's silence AND the compiler under test lowering the
  file, in both runners. An `EMIT-002` there is a compiler defect by its own
  message; for a cycle the suite could not see one (DEF-88 sat in `moves.npk`
  since it was written). Put a construct the checker admits into an accept
  file and the emitter is now asked too.
- **`_` in a `pick` pattern binds nothing and keeps its position** (DEF-88,
  1.5.8b step 6d): the emitter's `enum_bind_is_wild` is the one notion, shared
  with `arms_bind_any` and the checker. A CONSUMING arm that discards a payload
  with `_` still owns it: all-wildcards drops the whole value through the
  enum's drop body, a mixed `(Two(a, _))` drops payload 1 in place at the bind.
  When adding a pattern shape to the emitter, ask who frees what the pattern
  does not name — and measure it with a churn twin under `NPK_HEAP_STATS`.
- **A whole-tree sweep with the CHECKER sees no emitter refusal** (found at
  6c): step 6b's sweep ran `tools/check` and reported nothing for
  `tests/accept/moves.npk`; the same sweep with `npkc` found the file dying as
  `EMIT-002` — pre-existing, since `tests/accept/` asks only the frontend
  (DEF-88). Sweep with the full compiler when the question is "does it
  compile".
- **A by-value struct's field is one term while nothing writes the binding**
  (D-317, 1.5.8d step 0): `w.count` in a loop's condition and in its
  `decreases` are the same `(|npk.f.T.count| w.1)`, so the measure decides
  and the rows over `w.count - i` follow; a field write bumps the version with
  the update frame (the other fields keep their values), a whole assignment,
  a `move` out of a field and a loop that writes any part of it bump it too.
  What ends the term for the whole function is an ESCAPE: `@s`, `$$i s`,
  `$$m s`, `list_push(@s.items, …)` — and, since DEF-94, a method call whose
  receiver is a pointer (`s.bump()` with `bump = NIL(Self->:self)`), because
  the emitter passes `@s`. A pointee's fields (`x.count` with `Reach->:x`)
  stay fresh per read (E-4, 1.6's). A `pure never fails` call takes a struct
  argument's identity only when the struct is PLAIN DATA (no pointer, no
  container, no enum inside): a pure function may read the heap through a
  reference its argument holds. A `prove` over a struct's fields is the way
  to see what the encoder knows (`tests/verify/agg_frame.npk`). **An encoder
  change moves EXISTING verify tests' rows** (three at this step, each read
  one by one), and `--only` cannot reach the verify suite: run `vprog.py`
  over every file of `tests/verify/` before the harness (twenty minutes
  against three hours).
- **An impl's parameter is `move` exactly where the trait's is** (DEF-116, 1.6.1
  step 0c): the caller spends its argument where the TRAIT's declaration says
  `move` — through a bound or a `dyn` it reads nothing else — so an impl that
  adds `move` to a lent parameter frees what the caller still owns and one that
  lends a parameter its trait consumes leaks what the caller spent; both are
  `NITPICK-TYPE-014` at the parameter. Write the impl's parameters as the trait
  declares them, and `.clone()` inside when the body needs to own.
- **The emitter resolves an identifier by the resolver's symbol, never by its
  name** (DEF-144, 1.6.1e step 1): `ident_slot` (ir_expr.npk) is the one reader —
  a local by its declaring statement, a parameter by the prologue's binding, a
  function, an error constant or a global to its own emission BEFORE any local of
  that name is consulted. A new emitter site that resolves an identifier NODE asks
  it; `fnem_lookup_idx` by name is for a name the emitter itself minted. A `for`
  binding is a `DeclParamDecl` node in a BLOCK scope (resolve.npk declares it
  so): a `DeclParamDecl` symbol is a parameter only when its scope is the
  FUNCTION scope — the first form read `for (int64:li …)` shadowing parameter
  `li` as the parameter, and npkg's `xfn_inline` trapped. A macro
  body's free name read the caller's local for a cycle because of the by-name
  lookup, and only the lone-identifier and comparison shapes showed it (the
  constant folder reads the symbol, so arithmetic was right by accident).
- **A character literal has a width** (DEF-145, 1.6.1e step 1): `'\u{…}'` is a
  `char32` literal whatever its value, a source character above U+00FF is one too,
  a plain character or a `\x` escape is `char8` — carried as the numeric literals
  carry theirs (`Token.width`/`ExprNode.width`, `WChar8`/`WChar32`; one token kind,
  one node kind; `char_lit_bits` reads it). A `char8` slot refuses the escape
  (TYPE-007); `128512char32` is the other spelling.
- **The join relays the EARLIEST-SPAWNED child's error** (DEF-146, 1.6.1e step 1;
  S-115, ratified as D-334): a child's error overwrites a
  previous child's in the LIFO walk and never the function's own; `main`'s join
  joins every child before `failsafe`. A test that expects "the first to fail in
  time" (the listener's `ctl_j3`) answers the other error by the rule.
- **An expired `timedwait` fails without re-acquiring, the guard spent** (DEF-147,
  1.6.1e step 1): the emitted wait reads the clock at its resume; a waiter loop
  that swallows the error (`?! E`) traps where it silently looped before, so bound
  the loop and handle the error.
- **A spawn LENDS its crossing until the block's join** (DEF-148, 1.6.1e step 1;
  `NITPICK-BORROW-016`): after `drop f(@s)` the owner may `alloc`/`get` a shared
  arena or acquire/signal a lock, lend it again, or hand `@s` to a helper that
  only allocates — and may not `destroy` it, assign over it, `move` it out or `$$m`
  it before the block ends. The concurrent-set table is `crossing_concurrent_method`
  (escape.npk), beside D-180's five kinds (`spawn_crossing_kind`); a new kind or
  operation goes there, and nowhere else.
- **A `cstring` is string-shaped and OWNS the copy `to_cstring` makes** (D-328,
  1.6.1d step 3b): `{ ptr, len, cap }`, `cap == 0` a body it does not own (a
  literal in `cstring` position, an `argv`/`environ()` element), `cap > 0` the
  owned copy, dropped at the scope exit; move-only under TYPE-046 — read an
  `argv` element in place (`argv[0i64].len`), `.clone()` for a copy, `move` to
  transfer. `cstring:cs = "Hello";` is the compile-time form (D-049), an interior
  NUL `NITPICK-TYPE-092` at the literal. The floor's six `cstring` symbols take
  and build the trio, so a floor edit near them keeps the third word; the
  generator's LLVM text for `cstring` is `{ ptr, i64, i64 }` (gen_tables.py).
  A cstring in `src/`/`tools/` is never `.clone()`d until a snapshot's prelude
  carries the impl (the bridging rule); the tools read `argv` in place.
- **A floor ABI change refreshes the snapshot in TWO hops** (1.6.1d step 3b,
  `1.6.1d.md` §2.9): the old builder on the OLD floor compiles the new `src/`
  (a body of the old ABI, an emitter of the new), that compiler on the OLD
  floor compiles `src/` into stage2, stage2 on the NEW floor compiles it into
  stage3, `cmp` silent, stage2 installed. `quickemit.py` links the builder with
  the tree's floor and is WRONG during such a step: build the dev compiler by
  hand (`.internal/twofloor_dev.sh`'s shape) and link what it compiles against
  the new floor.
- **`destroy` takes the OWNING place** (DEF-157, DEF-158, 1.6.1e step 1): a binding,
  or a field or element of one reached through no pointer — `NITPICK-TYPE-091` for
  a pointer parameter's pointee, a dereference, a pointee's field or a temporary
  (the owner freed the arena again: `MachineFault` in safe code). A destroyed field
  is vacated, not its aggregate's flag cleared (the siblings leaked). And DEF-159
  is OPEN: a value stored through a held `@x` after `move(x)` leaks, because the
  moved binding's flag is cleared — do not write that shape in `src/` until it
  lands.
- **`<=>` orders an integer, a `char`, a kernel identifier and the twisted kinds
  that order** (DEF-131, D-330; 1.6.1d step 4): `-1`/`0`/`1` as `int32`, the
  operands evaluated once, a `tfp`/`dim`/`tern` pair's ERR guard once (a program
  with one names `(TbbErr)`). A float pair and a frac pair are `NITPICK-TYPE-088`
  (no total order); a `string` and a `tbb` have no ordering at all (TYPE-008,
  D-093) -- `a.cmp(b)` for strings. The folder folds a constant pair; the
  encoder's term is `(ite (< a b) -1 (ite (> a b) 1 0))`, so a `prove` over the
  result decides.
- **A certain constant division is refused wherever the folder decides the
  pair** (D-331, 1.6.1d step 4): `5i32 / 0i32`, `7i32 % 0i32`, `(-127i8 - 1i8) /
  -1i8` are `NITPICK-TYPE-004` as a local's initialiser or an argument, not only
  in a `fixed` initialiser -- the typer folds the operands as `check_const_overflow`
  does and lets the folder's own division arm report ONCE (the diagnostic list
  keeps one copy of an identical finding, so a `fixed` initialiser's later fold
  adds nothing). `-128i8` is TYPE-031 (the literal `128i8` does not fit before the
  minus applies): spell a minimum `(-127i8 - 1i8)`.
- **An enum payload carries an address as a struct's field does** (DEF-160,
  1.6.1d step 4): the layout records an enum's pointer-bearing bit from its
  payloads (`tt_set_haspt` in the enum arm), `field_holds_ptr`, `type_holds_pointer`
  and `type_reachable_in` read it, and the constructor `E.Some(@local)` -- a
  METHOD-CALL-SHAPED node whose receiver names the enum (`call_is_enum_ctor`) --
  is an unknown callee to the ref collector that CARRIES every argument and views
  none, writes through none. Before this step `E.Some(@local)` left its frame in
  silence (exit 3 at -O0 through a dead frame). A new predicate over types must
  answer for `TY_ENUM` by its payloads, never "a tag and a word".
- **A moved-out owning value carries what its place holds** (DEF-125, 1.6.1d step
  4): `move(self.v)` of a `string` (or any type with no foreign pointer inside)
  carries the refs recorded AT `self.v`, not `self`'s PASS ref --
  `collect_moved_place_refs`; the return seam's implicit move reads the same. A
  swap through a lent `dyn`'s method compiles; a view stored in a field and moved
  out is still BORROW-001.
- **A `move` parameter's re-assignment is an assignment** (DEF-124, 1.6.1d step
  4): `x = raw nw();` after `move(x)` clears the moved bit (`assign_assign`'s
  `SYM_DECL` branch marks a `DeclParamDecl`); a parameter moved twice with no
  assignment between stays MOVE-001. A consuming `pick` arm's binding is the third kind and is marked the same way
  (DEF-256, landing 132: the `SYM_PAT` branch) -- a new binding kind owes `assign_assign` its own branch, or its
  assignment is read as nothing.
- **A `pick` does not select on a pointer, and a new TYPE rule can silence an ANALYSIS file whole** (DEF-257, landing
  133): `pick (w)` over a pointer is TYPE-065 at the `pick` (compare it, `w == NULL` for a `wild` one, or `pick (<-p)`);
  an enum holding a local's address is still the escape analysis's BORROW-009. The type checker runs before the
  analyses, so a rule it gains can make an analysis-suite rejection file report its one new code and nothing it
  expected (`untracked_borrow.npk`'s `pick (@h.t)` did) -- a shape `count_check.py` cannot see, since it compares only
  the codes on both sides. Run `.internal/handoff_35/set_check.py WORKTREE` (the runners' exact set and count) on a
  WIP before its chain.
- **The pipe operators are struck** (D-362, S-139; landing 133): `x |> f` and `f <| x` are NITPICK-PARSE-014 at the
  operator -- write the call `f(x)`. A pipe was a second spelling of a call, and the forward form had double-freed a
  `move` argument in safe code (DEF-180). The `PipeExpr` node is still built after the refusal (the grammar is never
  partial, D-085), so its typer and emitter arms stay as unreachable as `DefaultsExpr`'s.
- **An arm over an enum names one of ITS variants** (DEF-142, 1.6.1d step 4):
  `(K.C)` over a `K` without `C`, or `(Other.A)`, is `NITPICK-RESOLVE-002` at the
  pattern (`check_enum_arms` in `type_pick_rules`, both spellings) where it was
  EMIT-002. A `mod:` name is an identifier (DEF-143): `mod:error;` is PARSE-001 at
  the keyword, and the loader adds no second sentence (a header node with name 0
  is the parser's report).
- **A mismatch between two same-named types names their modules** (DEF-126,
  1.6.1d step 4): "expected `same_name.Row`, found `rows.Row`" --
  `type_display_qualified` finds the module NODE holding the declaration (an
  inline module by its name, a file by its header); only when the plain texts are
  equal.
- **A range pattern's bound is read through the folder** (DEF-132, 1.6.1d step
  4): `(-5i32..-2i32)` lowers (`pat_bound_value`); a bound that does not fold is
  still the frontend's broken promise (`iv_broken`).
- **A function the expression recursion passes through at every level holds a DISPATCH, not the arms** (DEF-176,
  landing 142): allocas are hoisted to the entry block (D-173), so a frame is every arm's locals and every arm's call
  temporaries, paid at each level of a nested expression -- `emit_expr_kind` held 1,565 slots and a 54-operand sum
  made `npkc` exit 3 with no message, below the AST's own bound. An arm with locals is a function of its own, a
  recursive operation evaluates its operands in a small frame and combines them in another (`emit_binary` /
  `emit_binary_values`), and a walker the emitter calls at every level (the folder) owes the same. `chain_deep.npk`
  holds the bound; the encoder is DEF-269. Count a function's slots with a script over `npkc.ll`'s `alloca` lines.
- **The parser's descent has ONE bound, and a type is part of it** (DEF-184, DEF-177; landing 143): `p_too_deep`
  (parser.npk) is asked by `p_unary` once per expression level, by `p_parse_type` once per type level and by
  `p_suffixes` once per `->`/`?`/`[]`/`.` suffix -- a type 9,000 deep had trapped the checker. A new recursive parse
  rule, or a loop that builds nesting without recursing, asks it too. And after `.` or `?.` the parser takes an
  identifier or a member keyword (`p_member_name`): a token's payload is an intern index only for an identifier.
- **`pkill -f PATTERN` from a Bash tool call kills the call itself when the
  pattern's text appears anywhere in that call's command line** (found four times
  in one evening): assemble the pattern from a variable (`P="emit_cm"; pkill -f
  "${P}p.sh"`) and put NOTHING else naming the target in the same call.
- **`cd X && <command> &` runs its `cd` in the background job, not in the shell** (found at landing 95): the
  foreground shell stays where it was, so a `$(git rev-parse HEAD)` on the next line reported another worktree's sha
  while the background command ran in the right one. Put the `cd` on its own line, or wrap the pair in `( … )`. And
  when stopping a chained wrapper (`bash -c 'a; b; echo DONE'`), kill the WRAPPER before its running step, or the
  wrapper runs the next step the moment the first dies.
- **An `async` METHOD is called under `await` or spawned with `drop`** (DEF-149,
  1.6.1e step 2): TYPE-043 at a bare `r.read(buf, d)`, `obj.run()` or a `dyn`'s
  method as at a bare free call -- applied where `type_method_call` makes the
  callee final. Before it the bare method call compiled to a call of the
  coroutine's undefined symbol and `llc` refused the module.
- **A macro body is declarations or statements, not both; a broken body expands
  to nothing; a declaration's modifiers are read past** (DEF-150, 1.6.1e step
  2): a statement in a declaring body is `NITPICK-MACRO-010` (once), a body with
  a parse error expands to nothing (the errors are the report; `record_site` had
  read the window past its end and trapped), and `comptime func:`/`async func:`
  in a body are declarations (`p_at_decl_start` peeks past the modifiers). The
  reference's `emit_methods` example spelled `$$i Box:self`, never a parameter
  form: a receiver is `Box:self` or `Box->:self`.
- **A rejection file names ONE `expect-error` line PER SITE** (D-332, landed
  2026-09-27): both runners count each code's reported sites against the lines
  naming it, so a file that names several sites under one line fails (`X
  reported at N site(s), expected at M`), and so does a SILENT site -- the one
  the set rule of D-237 could not see (`not_constant.npk` expected a refusal
  D-222 had made legal, for a month, because its code was reported elsewhere in
  the file). Write each site's line with its `expect-error-at`, measure the
  header over the step's checker, and READ every site: the rule's first run
  found three compiler defects (DEF-161…DEF-163) among thirteen files.
- **A refusal the PARSER makes is a unit case, never a rejection file** (D-085;
  1.6.1e step 2 and 1.6.1d step 4, two red harnesses in one night): the `grammar`
  stage feeds EVERY file of the tree through the real parser and requires it to
  parse, so a file whose expected refusal is PARSE-0NN, or a MACRO code the
  parser raises (MACRO-010), fails that stage and `parity` with it. Test it in
  `tests/frontend/parse_*.npk` (lex a string, `p_parse_decl`, count the
  diagnostics and read the code), or on a temporary file through the loader
  (`tests/frontend/module_graph.npk`'s `mod:error;` case).
- **No tree nests deeper than 256 levels** (`AST_DEPTH_MAX` in ast.npk, DEF-150,
  1.6.1e step 2): a function's body block is level 1, so a top-level statement's
  expression may be 254 tall (`expr_depth_max.npk`). The parser measures each
  declaration's tree after it (`ast_depth.npk`: the AST's construction log,
  children first, one pass) and refuses the first subtree past it with
  `NITPICK-PARSE-012`, once per declaration; its own descent (`p_unary`, once
  per level) is held to the same number. A LEFT chain `1 + 1 + ... + 1` is one
  level of the parser's recursion and 600 levels of tree: counting the parser's
  nesting alone left it trapping the folder, found by probing the shape the
  listener had not. The expander refuses a splice landing past the bound
  (MACRO-003, site depth + clone height); every analysis depth reads the one
  constant. A NEW node constructor must push the height and the log beside the
  flag byte, and a new node kind needs its children in `ast_depth.npk`.
- **The build mode is the front half's** (DEF-151, 1.6.1e step 2):
  `front_run_mode(f, root, no_wildx)` -- npkc and `tools/check.npk` hand it the
  same `--extra-picky=no-wildx`; `reject_wildx` walks the PROGRAM's modules (a
  node is the prelude's by its span's file) and reads four spellings (a `wildx`
  local, a `wildx`-returning function, an `=>! wildx …->` cast, a `wildx_*`
  call). A rejection or accept file names its flags in a `// npkc-flags:` header
  line and both runners pass them to the tool; a field of an unassigned local
  cannot be written by the caller (ASSIGN-001), which is why the mode is an
  argument.
- **A builtin scalar takes no type arguments** (DEF-152, 1.6.1e step 2):
  `tfp64<Meters>` is TYPE-016 (it was accepted and the unit dropped); a unit
  rides `dim256<Unit>` alone (D-196). `string` keeps its three spelled forms
  `string<char8>`/`<char16>`/`<char32>` (TYPE_REFERENCE §3.2), and every other
  argument on a `string` is TYPE-016 -- but the two wide forms are BYTE strings
  today (the width is ignored), which is S-116, settled as D-335: wide strings are IN, designed and
  implemented as 1.6.1f before the freeze -- until it lands, do not write one.
- **A module constant of a float, fixed-point, `tbb` or ternary type is WRITTEN, not computed** (DEF-179, DEF-197; landing
  87): a literal, a negated literal, `ERR`, another module binding of that type, or `comptime(…)` of one — `fixed
  flt64:TAU = 6.283185307179586f64;`. An expression over one (`2.0f64 * PI`, `100tbb8 + 100tbb8`, `29524 + 1` in a
  tryte) is NITPICK-TYPE-035: the folder computes in the plain integers only (`fold_computes`), and it carried these
  families as plain integers into three silent wrong constants until this landing. A `flt32` module constant is
  TYPE-035 by name until DEF-203 lands. In the folder a new numeric family is CARRIED (`CV_REAL`, or a typed `CV_INT`
  no operator arm accepts) and rendered by the type the checker recorded (`const_scalar_text`) — never `cv.num` alone.
- **A folded value is held to the type it lands in** (DEF-198, DEF-201; landing 87): a module binding read by name has
  its DECLARED type (`fold_stamp`: `comptime(BIG)` over an `int64` binding is an `int64`), an untyped `comptime(…)`
  value must fit its slot (TYPE-031), and an array size, a lane count and a channel's constant argument are plain
  integers in `int32` range. Every `fold_const` starts with fresh fuel, so a fold that RESOLVES A TYPE needs its own
  bound (`stamp_depth`: `fixed int32[K]:K = 5;` trapped the first form of this change).
- **A literal's PAYLOAD is its value only for a plain integer, a `tbb` and a ternary** (DEF-197, DEF-204; landing 87):
  a `tfp`/`dim256` literal's number is its Q constant (`tfp_q_of_int`, `tfp_q_decimal`), a float's is its text, and a
  fraction's payload is an intern index. Three readers wrote the payload whatever the type — the global's renderer,
  the `pick` pattern lowering, the `flt32` digit count — and each was silent or a trap. A new reader of a literal node
  asks the TYPE first (the node's recorded type, or the selector's for a pattern).
- **`tid` is a type keyword** (again): a parameter named `tid` is PARSE-002 "expected an expression" at its first USE,
  lines from the declaration. `pid`, `tid`, `fd`, `uid`, `gid` are never names.
- **Two emissions that differ only in how a constant is SPELLED are compared as ARTIFACTS and through a reference**
  (landing 89): assemble both under ONE module name (an emission states no `source_filename`, so `llc` names the module by
  its input file and the name reaches the object -- the same text under two names is two objects) and require the `-O0`
  object, the `opt -O2` text and the `-O2` object byte-identical; beside it, read every constant of both texts as its bits
  through an exact reference and require the texts equal (`wt/27a/.internal/tools/objcmp.sh`, `fnorm2.py`). Either leg
  alone says less: the objects say LLVM read both alike, the reference says the new spelling is the number written.
- **A test must hold the text that tells the roads apart** (landing 89): a short `flt64` literal is read alike by LLVM's
  parser and by `float_round`, so a road test with short texts cannot see a writer that bypasses the conversion.
  `float_literal_kat.npk`'s `roads` puts a text past LLVM's cap on every call site of the one writer. And a GENERATED
  `uint64` past 2^63 (a negative double's bits) is built, not written: `(…u64 | (1u64 << 63u64))`, LEX-004 otherwise (D-311).
- **The compile-time evaluator runs statements only inside a `comptime func:` call** (D-338, landing 90): a `pick`
  EXPRESSION folds only there (`TypeResolver.fold_frames`, raised and lowered by `fold_call` alone; never `fold_depth`),
  because the emitter and the typer ask the folder about run-time nodes (`emit_const_fold`, `check_const_overflow`) and
  an arm's assignment evaluated into the resolver's environment would never run. Any new evaluator arm that runs
  STATEMENTS asks the same counter; a pure expression arm (`Enum.Variant`, `a.cmp(b)`, `?!`) folds anywhere. An arm of an
  expression `pick` ends in `give`: a `pass` out of one is not evaluated at compile time (the statement form returns).
- **An enum value in the evaluator is its variant's POSITION, and since D-356 it leaves as the variant's TAG** (D-338;
  landing 123): `CV_ENUM` holds the enum's type and the member index `variant_tag_of` takes; `==` on two is identity
  (tags are unique, DEF-193). A consumer of `fold_const` that WRITES one asks `variant_tag_of`, never `num` -- the two
  coincide for an unvalued enum alone (`const_enum_text`, for a module binding, a struct's or an array's member and
  `comptime(…)`; a tagged enum's variant is its aggregate with a zeroed slot, `ll_enum_tag_const` beside the body); a
  GENERIC enum's variant is not folded (its instance is not in the expression) and stays TYPE-035. `a.cmp(b)` is recognised by the receiver's VALUE
  (the folder holds no call record) and its `Ordering` is found in the prelude's own scope, never by spelling.
- **A `comptime func:` is emitted nowhere** (DEF-214, OPEN; S-128): calling one outside `comptime(…)`, a constant site
  or another `comptime func:` reaches `llc` as an undefined symbol. A test that needs one body at compile time AND at
  run time writes it twice (`c_f` / `r_f`: `comptime_pick.npk`), and its run-time arguments come from a function the
  compiler cannot fold -- or the "run-time" side is the evaluator again.
- **The evaluator short-circuits, and a fold entered from inside an evaluation is part of it** (DEF-211, DEF-213;
  landing 90): `false && X` and `true || X` do not evaluate `X`; `fold_const` resets the depth and the said-flag only
  when no frame is open, so a type resolved mid-body (`#size_of<int8[N]>()`) no longer zeroes the recursion bound.
  When adding a bound, ask what RESETS it: this one was reset by the front door of its own module.
- **Two value arms with one value reach `llc`** (DEF-212, OPEN): the checker reports an arm after `(*)` (PICK-004) and
  nothing for a repeated value; the `switch` lowering is then invalid IR. Do not write one in a test of something else.
- **A relayed answer is recorded with where it was said** (landing 90): the user ratified S-119…S-126 in another seat's
  session; the decisions carry his words, the date, the session, and the questions as they were put -- never a
  paraphrase of the recommendation. The seat that holds the queue records it in its NEXT commit and tells the others.
- **A float constant has ONE writer, and it writes BITS at both widths** (DEF-203, DEF-209; landings 88, 89):
  `float_const_text` (ir_expr.npk) behind `emit_float_const` and `const_scalar_text` -- `0x` and sixteen hex digits: the
  double `float_round` makes of the text, or for a `flt32` the double holding its float. A new site that spells a float
  constant from a program's text asks it: a decimal handed to LLVM is read by a parser that caps the exponent at 24,000,
  and `fptrunc double <text> to float` is two roundings. The only decimals an emission holds are the emitter's own exact
  bounds (`cast_bounds.npk`, one `0.0`). The emitter and the encoder's `fp_literal` are ONE number by one rounding each:
  change either and the other moves in the same commit, or a verified build proves things about a float the program does
  not hold.
- **`float_round(text, width)` is the conversion, and its answers come from exact rationals** (numeric.npk; DEF-203): both
  formats, the `inf`/`zero` flags reported and unread (DEF-205, S-126). `tests/frontend/float_round.npk` and
  `tests/backend/programs/float_literal_kat.npk` are GENERATED by `bootstrap/generator/float_vectors.py` (`--write`; the
  harness's `check_generated_current` runs `--check`): never edit a vector by hand, and after touching the generator
  regenerate in the same commit. Both halves of the KAT test the compiler's conversion through the emitter since DEF-209
  (a red there is the compiler's); `float_vectors.py --llvm` is the measurement of LLVM's parser, outside every gate.
- **A `comptime(...)` operand is typed by NOBODY: the compile-time evaluator alone reads it** (DEF-205, landing 91).
  `type_comptime` folds its operand and never hands it to the typer, so a rule the checker asks of a literal or a name is
  skipped under `comptime(` unless the evaluator asks it too. A float literal's fit rule is asked in
  `fold_suffixed_literal` -- at `fold_depth <= 0` only (inside a call the literal sits in a function's body, which the
  checker types, and a report with a call chain behind it would be a second sentence), in the checker's own sentence at
  the literal's own span, so a literal both read is one report. A new rule about a literal owes the same question: where
  does the evaluator read this without the typer?
- **A float literal FITS its type, and its length is free** (D-346, landing 91): one that rounds to infinity, or a nonzero
  one that rounds to zero, is NITPICK-TYPE-031 (`float_fit_refusal`, numeric.npk); a subnormal result stays; an infinity
  is computed (`1.0e308f64 * 10.0f64`), never written. The 15-digit rule on `flt32` is gone. The emitter writes no
  constant for a literal that does not fit (`float_const_text` answers ""): if EMIT-002 ever names a float literal, a
  road reached the emitter that neither reader asked -- find the road, do not loosen the belt.
- **A float literal's rules follow its WIDTH, whatever gave it one** (DEF-206, landing 88): `float_text_ok` is asked by the
  suffix's branch, the contextual one (an unsuffixed fraction in a float slot) and `type_comptime` (an untyped folded
  fraction). A new rule about a float literal goes there, once -- "`flt128` has no literal" sat in the suffix's branch
  alone for a cycle; and after closing a hole, probe EVERY position the construct can sit in (the `comptime` road was
  found by a twelve-line probe after the first fix looked complete, and its suffixed twin by another at landing 91).
- **A bound on a text is stated against the text** (DEF-207, landing 88): an exponent's digits are summed only while
  `ex <= text.len + 1000` -- past that no digit of the literal can bring the number back into range, so the answer is
  forced; a FIXED cap misreads a fraction whose leading zeros outnumber it, and NO cap traps (`ex * 10` overflowed in
  the encoder) or builds a million-digit numeral. When a function builds something whose size a literal chooses, ask
  what a nine-digit exponent does to it -- in the plain build and under `--obligations`.
- **A measured claim has two halves, and a measurement has a domain** (landing 88): D-143 said two things about float
  literals -- the `flt32` half was false and the `flt64` half had never been checked. A first rig of 10,015 texts held
  it, and the landing was scoped on that; the COMMITTED measurement, with two texts written for another purpose (an
  exponent that undoes a hundred thousand leading zeros), found LLVM's parser capping the exponent at 24,000 (DEF-209).
  "None different" is a statement about the texts tried: say what family was not in them. And a reference function that
  builds the number is not safe on every legal text: `1.0e999999999` hung the first vector generator.
- **A float literal is its production, and its tail is a suffix or nothing** (DEF-166, DEF-174, DEF-175; 1.6.1e step
  3): `NITPICK-LEX-009` for any other tail (`1.5f512`, `3.14flt32`, `2.5e3zz`), the token still a float literal so the
  parser adds nothing (D-240); a sign belongs to a literal only inside an exponent with a digit after it -- `3.5f64-x`
  computed 3.5 before, in silence -- and the kept text carries no `_`. A lexer refusal is a unit case
  (`tests/frontend/lexer_numeric.npk`), never a rejection file. `f128` lexes and TYPE-030 refuses it. Both scans still
  accept a trailing or doubled `_` (`10_`, `1__0`): DEF-190, OPEN -- do not write one.
- **A literal with a fraction takes a float or fixed-point suffix** (DEF-167, 1.6.1e step 3): `2.5i32`, `2.0i32`,
  `1.5u8`, `2.5tbb8` are TYPE-031 at the literal. And `flt256`/`flt512` are TYPE-001 in every type position: the
  generated scalar table still answers both words, so the refusal stands AHEAD of it in `resolve_named`.
- **The task builtins are `async`'s** (DEF-168, DEF-169; 1.6.1e step 3): `suspend_until`, `suspend_io` and `io_watch`
  are TYPE-043 outside an `async` function, the two parks also inside `defer`; `io_unwatch` is legal anywhere. A park's
  wind-up exit runs the function's defers -- a park inside a defer body made the emitter lower the body inside itself.
- **A channel's LEVEL is a lock level** (1.6.1e step 3): `send`/`recv` are waits at it -- LOCK-001 at or below a held
  level, through a callee or a spawned task too -- and a wait raises no hold (`LockTable.hold_max`: what a call may
  leave HELD is fed by `acquires N`, an `acquire`/`read`/`write` and a callee's own hold, never by a wait). A test that
  sends with a guard held puts the channel ABOVE the mutex (`mutex_basic.npk`: level 6 over 5).
- **An implementation of an undeclared trait method acquires nothing** (D-056, D-113; 1.6.1e step 3): a trait method
  with no `acquires` clause gives its default body and every impl a bound of nothing -- LOCK-002 where the body can
  reach any level, a channel wait included (the sentinel is read through `bound_is_nothing` alone) -- and an exact
  `acquires N` on a trait's method is a checked ceiling that does not stand in for the body. So NO impl of a prelude
  trait (`Writer`, `Reader`, `Iterator`, `ToString`, ...) may take a lock or wait on a channel: take the guard outside
  the call and write through the guarded value. The lock walk reads the checker's call record (`exprtypes_callee`); an
  analysis that resolves a method call asks it too, never `find_method` on the receiver's recorded type (which misses a
  pointer receiver, UFCS, `Trait.m(x)` and a default body's `self.m()`). DEF-181's six holes and the floor are CLOSED
  (landing 127, D-339): "lock-order freedom is proven" is the compiler's claim now, for what the first layer covers.
- **A `<=`-bounded trait method is called with nothing held; a hold is visible or it is nothing** (D-339, DEF-181; landing
  127): `acquires <= N` is a CEILING ONLY -- a call that resolves to the trait's method (a `dyn`, a bound `T: Trait`,
  `Trait.m(x)`, a default body) while anything is held is LOCK-001, and so is a call of any function that makes one (it has
  no floor; the sentence names the bounded method) -- so a trait method callers must reach under a guard declares its level
  EXACTLY (`acquires N`: every call judged at N, an implementation reaching below N LOCK-002 as above N was). A binding
  raises the held level only to a hold it can SEE: the callee's own `acquires N`, a bounded trait method, `m.acquire(d)`
  itself, or a `Guard`/`RGuard` in the result's TYPE (inside a struct too: the layout bit `tt_hasguard`, ensured first) --
  so `int32:a = raw helper(); int32:b = raw helper();` compiles where it was "holds 1 and can acquire 1", a wrapper that
  hands a `Lease`-like struct on without a clause hands back NO hold (write `acquires N` on it, or return the `Guard`), and
  the level raised is the HIGHEST the result may carry. A function named as a VALUE must reach nothing (LOCK-002 at the
  name). A `defer` body and a spawn (`drop child(@m)`) are judged at their block's END against its final held level
  (D-207: join, defers, drops) -- take the guard in an inner block that ends before the join, or spawn after it. The two
  sentinel values `2147483647`/`-2147483647` are refused as levels (LOCK-003 in a clause, TYPE-016 in a lock type). In
  locks.npk the one reading of a call is `call_lo`/`call_hi`/`call_floorless`; a new consumer asks them, never the table.
- **A macro cycle is refused at its declaration, and the edge is what the body writes** (DEF-172, DEF-173; 1.6.1e step
  3): MACRO-004 at the cycle's first macro in source order, uninvoked cycles included; `#ign(#a())` inside `a` is an
  edge even if `ign` discards its argument. The round bound (32) reports at the invocation left standing (a probe walk
  that instantiates nothing), and a chain of exactly 32 settles. A `macro:` inside a macro body is MACRO-005. A
  program `check_macro_decls` refuses is not expanded.
- **The expansion walk reaches every expression, and every instantiation is its own copy of the body** (DEF-170,
  DEF-171, 1.6.1e step 3; DEF-189, step 3a): an invocation in a contract, a measure, a `Rules` clause, an array size or
  a `comptime` argument inside any type, a pattern, an attribute is expanded (no program reaches MACRO-006 now). The
  clone is TOTAL: an expression, a statement, a declaration, a pattern, an attribute, a generic parameter and a
  verification node are cloned, each one; a TYPE is cloned exactly when it holds an expression and is otherwise the
  template's node (nothing writes onto a type; the checker DOES write a destructure's binding types onto its pattern
  and the resolver keys a symbol by a declaration node — ask "does a later pass write onto it or key a table by it"
  before sharing a node). An alias (`macro:a = () { #b(); };`) is passed through at every declaration site. A new
  position that holds an expression needs its arm in the walk (`expand_in_*`), in `blank_*` and in the clone
  (`clone_*`) — what the clone clones is BLANKED in a detached original — or the audit calls it a hole.
- **A macro parameter is substituted wherever an expression stands, and cannot be a type's name or a name the body
  declares and uses** (DEF-189, 1.6.1e step 3a): `int32[N]`, `(LO..HI)`, `#[derive(TR)]`, a cast's target, a signature's
  types all take the argument. `NITPICK-MACRO-011` at the macro's declaration (uninvoked too) for `T:x`, `T{ … }`,
  `x =>! T`, a destructure's type, and for a local, a `for` binding, a pattern binding, a function parameter, a generic
  parameter or an emitted declaration of the parameter's spelling THAT THE BODY ALSO WRITES AS AN EXPRESSION. A
  parameter that only names an emitted declaration (`func:N`, literally called `N`) is MACRO_REFERENCE §10's open
  question, S-123 — a sweep over the library listener's corpus is what caught the first form refusing it: READ what
  a new refusal reaches there against the reference's own "open" lists before calling it a defect fix. A BARE NAME IN A TYPE-ARGUMENT LIST IS A TYPE'S
  NAME (D-064 §2): a compile-time value there is written in parentheses, `simd<int32, (N)>`. (DEF-183's argument rule landed as D-340, landing 92; DEF-192 at
  landing 86.)
- **A generic function's instance is its TYPE ARGUMENTS, at every reader** (DEF-191, DEF-195; 1.6.1e step 3a): the
  checker records them on the call (`exprtypes_callee_targs`: the window's start plus one, 0 for none) and the emitter
  matches `fninst_for_call` by their contents; the encoder puts them — and a method call's receiver type — in a `pure
  never fails` callee's function symbol (`CallEnc.inst`). Two instances can share a substituted SIGNATURE (a parameter
  that appears only in the body), and two calls can share argument TERMS (an `Int` is an `Int` at every width): a new
  reader that tells instances apart by either is the same defect. After fixing a wrong answer in the emitter, ask
  what the verified build's MODEL said about the same program — DEF-195 was found by that question.
- **An enum variant's explicit value is a bare integer literal in `[0, 2^31 − 1]`, distinct from every other tag**
  (DEF-193, landed 2026-10-01; S-122 the user's): `X = 7i32 + 1i32`, `X = BASE`, `X = -3i32` and a tag two variants
  share are `NITPICK-TYPE-093` at the declaration (each was accepted and wrong: the expression ignored, the position
  used). An unvalued variant's tag is its POSITION among the members, never the previous value plus one —
  `{ A = 1i32; B; }` is refused, because `B` is 1 too. `check_decl` had no arm for an enum: when a declaration kind
  has no arm in a checker, nothing about it is checked, and the silence reads as acceptance (D-085's shape).
- **A compile-time binding is the resolver's, never its spelling** (DEF-196, landed 2026-10-01): the evaluator keys a
  `comptime func:` call's variables by the declaring node (`FoldEnv` = kinds/origins/vals; `fold_ident` asks only
  for an identifier that HAS a symbol). A nested block's local is a new binding, a macro body's free name is the
  module's, and a name inside a type (`int32[LEN]`) is the module's `fixed` binding and never a local (D-222). The
  lesson is DEF-144's again: wherever a walker looks a NAME up itself, ask whether the resolver already bound it —
  a by-name table beside a resolver is a second, weaker resolver, and its own comment ("no nested scope here") was
  true only on the day it was written.
- **A type's name in a macro body resolves where the macro was written** (DEF-192, landed 2026-10-01; D-057): it
  binds a generic parameter only where the body declares it; the invoking function's, struct's or `impl`'s
  parameters are invisible to it (`V:item;` spliced into `struct:Box<V>` is TYPE-001 — it compiled by capture; S-124
  asks whether a body should have a way). `ast_macro_at(ast, span)` says which macro declaration a node was
  written in (a clone keeps its template's span; `Ast.macro_spans` is filled by `p_parse_macro`) and
  `macro_may_bind` is the one rule: a NEW lookup that answers a type's name from anything but a scope (a generic
  window, an associated type) owes the same question. An argument's type names stand at the invocation.
- **When a test answers wrongly, hand-write the shape without the feature under test before reading the feature's
  code** (1.6.1e step 3a): the turbofish case of DEF-189's test failed, and the same two calls written with no macro
  failed identically — DEF-191, a defect of generics three cycles older than the macro the test was about.
- **A compile-time failure names its call chain** (`fold_chain`; 1.6.1e step 3): "; reached through the compile-time
  calls `a` -> `b`", outermost first; the folder asks "already said" of the sentence WITHOUT its chain (`fold_said`),
  so one expression is one report whichever path reaches it first.
- **Read every delegated diff whole, and ask its author how each mechanism FAILS** (1.6.1e step 3's hand-off): three
  implementation agents in three worktrees each found defects beyond the design, one package reached integration with
  no seat review, and the incoming seat's question about one mechanism (the shared template node) produced DEF-189.
  The measurements say a change does what was asked; only a reading says what else it does. No more than three agents
  at once (they took the five-hour usage window from 12% to 65% in two hours).
- **`failsafe` must name what the PRELUDE can raise on the program's behalf**
  (DEF-86, 1.5.8b step 6b): the reach analysis follows every resolved call into
  the prelude and every import, so a program that calls `list_pop`, formats a
  float, hashes through a bound or parses a path is asked for the arms those
  bodies can reach — `(OutOfBounds)`, `(IntOverflow)`, `(TbbErr)`, `(BadPath)`
  — exactly as it is asked for its own. A `dyn` or bound call reaches EVERY impl
  of the trait (an over-approximation; E-5 owns narrowing it), so a generic
  bound over a prelude trait can demand `(TbbErr)` a program never instantiates.
  REACH-002 names each missing identity; add the arm with the code `(*)` would
  have answered and nothing else changes. The prelude is still not walked whole.
- **Every module of ours states the pinned layout and triple** (E-8, D-322 (5);
  1.6.1 step 1): the emitter's first two lines are `target datalayout = "…"` and
  `target triple = "…"`, the floor and the explorer shim carry the same two by
  hand, and `nitpick.toml`'s `[toolchain] triple`/`datalayout` are the pins both
  runners refuse a manifest without and hold every emission, the floor and the
  shim to. The layout is held to what the pinned `opt` derives from the triple
  (`opt` keeps a wrong layout line as written and `llc` accepts one in silence,
  so the line proves nothing about itself; `llvm-as` completes a partial string,
  so the pin is the full canonical one). A change to either string is a change
  to the emitter's constants, the floor, the shim AND the pin in one commit, with
  a snapshot refresh; the object does not move for the line (measured). A
  library's manifest carries the same two rows — checked, never chosen. The
  gate's whole-program form drops every `target` line of the floor (the program's
  header governs): a second layout line is "invalid redefinition" to `llvm-as`.
- **A call's result holds a borrow by PROVENANCE, not by shape** (D-326, 1.6.1b): a
  callee that BUILDS its result from what it is handed (`string_concat("made:",
  b.s)`, a copy of a view, a method's rendering) hands back nothing of the caller's
  frame, and the caller may return it — `string:r = raw make(@b); pass r;` compiles.
  A callee that returns a view, an address, or a struct carrying either still
  refuses at the caller's `pass` (BORROW-001), as does a `dyn` method or a function
  value (an unknown callee keeps the shape rule) and a trait method through a bound
  where ANY impl views (the union). When adding a reader of the summaries
  (`collect_call_refs`), read it with `x.reporting` off unless the walk is one whose
  report belongs there (a binding's or a return's) — the collector reports a view
  of a temporary as it walks — and never ask it about a bare builtin (its verdict is
  the `Views` column's; `escape_expr` discards the arm's).
- **`Copy` is a prelude-owned MARKER, and the bound `T: Copy` licenses the copy**
  (D-327, 1.6.1c): a generic body may copy a `T` (`T:x = (<-l)[i];`) only under
  `T: Copy`; a bare `T` stays move-only (TYPE-046) and so does `Self`. Every
  scalar is `Copy` (the generated region), the prelude's `Ordering`, `Duration`,
  `Whence`, `Fcmd`, `Advice` and `LineEnding` are, a struct or enum of copyable
  members derives it (`#[derive(Copy)]`; a `string`, a `List`, a `dyn`, an
  `Optional` member refuses at the declaration, DERIVE-006), and an impl on a
  type that owns or whose member is not `Copy` is `NITPICK-TYPE-087` — the one
  judge for hand-written and derived impls. `list_get(@l, i)` reads a
  `List<T: Copy>` element out by value and traps `OutOfBounds` outside
  `[0, count)` (name the arm). A program cannot declare `Copy` or `list_get`
  (RESOLVE-001). A REFUSED impl still sits in the impl table: a rejection file
  that also tests the bound must use a type with no impl anywhere in the file.

- **A macro ARGUMENT is the caller's text and resolves at the invocation site** (D-340, landing 92): pass a local,
  a parameter, any expression; nothing the body declares captures it, and what stands under it (a `pick` arm's binding,
  an invocation) resolves with it. An argument a macro BODY writes for an inner invocation is the body's text. A
  DECLARATION macro's invocation stands at module level, so its argument resolves in the module's scope — naming the
  emitted function's own parameter is RESOLVE-002 (it compiled by capture before). A name inside a TYPE is the
  module's binding wherever written (D-222), an argument's included. `#caller` in an argument written outside a macro
  body is MACRO-008. Through an ALIAS (`macro:a = () { #b(); };`, D-125) the target's `#caller` reaches the alias's
  caller; through a body that merely contains the invocation it reaches that body's site, the module.
- **The hygiene marks are COUNTS in a node's flag word, and whatever replaces or copies a marked node owes them**
  (landing 92; `ast.npk`): `back` (steps out), `HYG_SITE`, `roots`. The clone carries them (`clone_expr`/`clone_stmt`
  are wrappers: a fresh node's word is empty, which is how `#caller` lost its mark, DEF-221); `instantiate_expr`
  COMPOSES the invocation's, its own root and the clone root's (a root then a step cancel); `instantiate_stmt` hands
  the invocation's to the block. A new expander path that writes over a node, or builds one from another, asks the
  same question: what did the old node carry? The resolver refuses a count that does not fit (RESOLVE-INTERNAL).
- **In the SMT encoder a spelling is not a binding** (DEF-223, landing 92): the bindings are kept by spelling,
  innermost first, so a site that turns an identifier NODE into a binding asks `ident_bind` (a module-scope symbol
  never reads a local), and a spelling that an identifier inside an expansion uses and the function declares twice is
  put in the escape set by `enc_prepare`. A NEW BINDING FORM in the encoder must be counted in the pre-walk
  (`collect_escaped_stmt`, `note_pattern_decls`), or the ambiguity rule misses it. The probe that found the hole asks
  the VERIFIED build, not the plain one: `python3 .internal/handoff_s11/vprog.py WT NPKC FILE` with the rows named —
  a wrong proof shows as "the verified binary exited N, expected M".
- **Read a finding against the reference's OTHER rules before calling it a defect** (DEF-220, withdrawn inside landing
  92): `#caller` reaching "two sites out" through `macro:w = () { #inner(); };` was D-125's alias rule, and the tree's
  own `macro_alias_sites.npk` said so the moment the chain ran. An inconsistency between two spellings is a defect
  only if no rule separates them; here one did.

- **A bare `#unreachable();` leaves, and a leaver is a row in BOTH lists** (DEF-225, landing 93): `stmt_exits` (which
  arm a merge drops: definite assignment, a `fixed` binding's one write, a move, the taint) and `stmt_completes`
  (FLOW-001) are twins with opposite conservatism, each a list of statement KINDS. A statement that never completes
  and is no kind of its own (the builtin is an expression statement) needs a helper read by both
  (`stmt_is_unreachable`). Under another expression (`int32:z = #unreachable();`) the statement completes, on
  purpose; the encoder's `stmt_falls_through` is a THIRD list and reads the builtin as falling through, on purpose
  (its claim becomes a hypothesis). One missing row showed as FIVE codes: when a finding names one refusal, probe
  every consumer of the predicate before sizing the fix. The early exit through `pick (r.is_error) { … }` is
  DEF-226, OPEN: write the `if` form.

- **A temporary is registered ONCE, by the one site that produces its value** (DEF-227, landing 94): `emit_fit`'s
  transfer registers the `dyn` cell (and the `Optional` wrap) it builds, so the expression kinds that lower THROUGH
  `emit_fit` -- the explicit `=> dyn` cast -- must not also be in `temp_producer`'s list: two entries under one name
  are taken once (`fnem_temp_take`) and dropped once, a use after free the binding then reads and a second free at
  the scope's exit. When a lowering hands a value to a path that registers it, ask whether the node's own kind
  registers it again; and when a floor call COPIES its operands (`npk_string_concat` frees neither), ask who frees
  them -- the template splice freed nothing (DEF-228) and handed a lone item's header through as its value (DEF-241).

- **A template's pieces are the statement's temporaries, and its value is its own body** (DEF-228, DEF-241; landing
  95): `emit_template` registers each `to_string` result and each replaced intermediate concatenation through
  `temp_register` once it has been copied past, never a string ITEM (a place's header is borrowed; a call's result is
  that call's temporary), and never the final value (the producer rule registers it once -- a second registration is
  F-037's double free). A lone string PLACE or CALL item is copied, so `\`&{ s }\`` is never `s`'s header; a lone
  LITERAL is not -- `\`plain\`` is the literal's own header, as `"plain"` is, at no cost (the landing's first form
  copied it too, and the sweep's one unexplained moved program, `op0391`, said so: read every moved program against
  the change). When a lowering hands an owned value to a floor call that COPIES (`npk_string_concat` frees neither
  operand), ask who frees the operand; and measure a lowering that allocates with a churn twin under
  `NPK_HEAP_STATS` (`tests/cost/template_splice.toml`, `template_literal.toml`).

- **A generic function may not instantiate itself at a type built from its own parameter** (DEF-229, landing
  96): `deep::<Box<T>>` inside `deep<T>` is TYPE-018 at the call; a cycle through another function that grows the
  argument is refused by the emitter's cap at 64 (the depth of the requested type), by the same code -- a backend
  refusal by name travels as `iv_refused`/`LL_REFUSED`, the third state beside the rung and the defect. A cycle
  that closes at a concrete type is fine. The two walks over a type's structure live in types.npk
  (`type_mentions_params_of`, `type_nest_depth`): a new type kind with an operand goes into `type_kind_has_elem`.

- **A toolchain pin is MOVED by measuring first, with the machine untouched** (D-349, landing 99): fetch the signed
  packages of the new release, verify them (`gpgv` on InRelease, the index's and each `.deb`'s sha256), `dpkg -x` them
  into a user prefix WITH `llvm-20-dev` -- the `usr/lib/llvm-20/lib/libLLVM.so.20.1` symlink ships in that package, and
  without it the tools' RUNPATH `$ORIGIN/../lib` falls through to the SYSTEM's library in silence (`ldd` on the prefix's
  `llc` is the proof; the version line is printed by the library that loaded) -- then run the ladder and the harness
  under `PATH=<prefix>/usr/lib/llvm-20/bin:$PATH` and compare object for object. Across 20.1.2 -> 20.1.8 every object and
  the emission were byte-identical and one `.comment` byte per linked binary moved. Every tool that links LLVM is
  rebuilt into a FRESH `ENGINES_ROOT` (never over the pinned install) and re-pinned -- Alive2 links `libLLVM.so.20.1`
  dynamically and would otherwise drift under its old digest. The machine's own move is the user's `sudo` at an hour
  when no runner is mid-run, told to the library seat in advance (their CI pins by the upstream tarball, their local
  workers by a private prefix), and every other worktree on the machine refuses by the pin until it is rebased. And a
  system-wide `apt` install of the release can be IMPOSSIBLE on a desktop: the 32-bit Mesa drivers hold `libllvm20:i386`,
  apt.llvm.org ships no i386, and a multi-arch library must stay at one version -- so the release goes in as a PREFIX (the
  same signed packages, `dpkg-deb -x`, the `~/.local/bin` links repointed, no sudo), which is how this machine moved; the
  engines then build against PATH's `llc` (engines.sh's default), never against `/usr/lib/llvm-20` by name.

- **A struct literal that writes several `sealed` fields from outside their module is ONE report** (D-337, landing 100):
  `type_struct_literal` collects the sealed fields the literal writes and reports once at the literal, naming them all, with
  the "declared `sealed` here" note under each (`note_sealed_decl`); a literal writing one keeps the per-field sentence, and
  every other write form is one report per write. When a rule is "one mistake, one report" (D-240), ask which NODE the mistake
  is: the literal, not its fields. And a rejection file's header is MEASURED after the file has its final line count -- a
  header written against a placeholder of another length, or shortened by a pair of lines, shifts every `expect-error-at`
  by its own change (found twice at this landing: a 4-line placeholder replaced by 14 lines moved five sites by ten, and two
  lines dropped from `list_fields.npk` moved eleven sites by two; D-332's count tool counts per CODE and cannot see it).

- **A refusal at the lexer's or the parser's seam is ONE report** (DEF-164, landing 101; D-240): a refused numeric literal
  stays a LITERAL token (`IntLit` with value 0, `FloatLit` with its text) so the parser has an operand and says nothing; a
  float's dangling exponent sign before `;` `)` `]` `}` `,` or the end is the refused literal's tail (`1.5e+`), while `3.5e-x`
  stays a literal, a minus and a name; a DECLARATION's recovery (`p_recover_decl`) skips to a sync point -- `;`, `}` or a
  declaration keyword -- never to an identifier the broken declaration left behind (`error:E(P);` was reported at the `(`
  and again at the `)`), where a statement's recovery (`p_recover`) still stops at anything that can start an item; and a
  trait-signature comparison says nothing about a signature holding a refused type (`sig_holds_invalid`: TYPE-001 was
  also TYPE-014). A parse refusal's count is tested in a frontend UNIT (`parse_one_report.npk`), never a rejection file
  (D-085: the grammar stage parses every tree file).
- **A declaration whose header fails is ONE report, and a declaration's recovery knows a bracket and a terminator**
  (DEF-245, DEF-260; landing 130): every declaration parser tests `p_in_panic` after its header and bails with an empty
  node of its own kind before the part that recovers on its own (the body, the member list, the initialiser;
  `p_parse_func`'s comment is the rule, `p_parse_mod` the exception for the loader's sake), and `p_recover_decl` stops at
  `is_decl_sync_point`'s set at bracket depth 0 only -- never a statement keyword, never inside `{ … }`, `( … )` or
  `[ … ]`; a `;` consumed, a `}` left standing; nothing skipped when the broken declaration ended at its own `;`. So a
  NEW declaration parser owes the same bail at the same point, a NEW keyword that can begin a declaration joins
  `is_decl_sync_point` beside `tok_starts_decl`, and a sub-parser that recovers on its own (a block, an item block, a
  member loop, an expression with `pick` arms) is never entered in panic. `parse_one_report.npk` is the unit, the fuzz
  corpus's seven named programs the measurement. THE STATEMENT LEVEL KNOWS A BRACE GROUP TOO since landing 139
  (DEF-265): `p_sync`, the no-progress belt every parser loop calls, counts `{`/`}` and stops at a sync point only
  outside every group (it took a struct literal's `}` after a failed expression for the body's end, and every
  statement after it was a module-level "expected a type"), and `p_recover` skips a `{` that follows a name or a `>`
  as the failed expression's literal; a `{` after `;`, `}`, `{`, `)` or a keyword is still a block. And a block
  without its `{` reads nothing (DEF-271): after a statement header fails, what follows is the enclosing block's,
  so the enclosing `}` stays the enclosing block's -- a new parser path that calls `p_parse_block` short of a `{`
  gets an empty block, never a loop that eats its caller's `}`. And a failed PARENTHESISED HEADER is
  skipped to its `)` (`p_recover_header`, parser.npk: `if`, `while`, `when`, `for`, a `pick`'s selector), a
  broken ARM's recovery is to the next arm (`p_recover_arm`, parse_pick.npk) and the arms without their
  `{` read nothing -- so a new statement with a parenthesised header, or a new arm-shaped loop, owes the
  same skip, or its header's rest is read as the enclosing block's statements (the chain's first reading
  of landing 139: six phantoms in three fuzz programs).
  And a `func:` written inside a body is ONE report at `func` naming D-018 (DEF-268, landing 141): `func` then
  `:` begins no statement, and it is skipped whole to its `;`.
- **A `fixed` parameter is written by its caller and never by its body** (DEF-248, landing 102): `n = …` and `n += …` on a
  `fixed int32:n` parameter are ASSIGN-002, as a `fixed` local's second write is; a plain or `move` parameter is re-assignable
  (DEF-124). The lesson is D-085's again: the arm's comment said "a parameter carries no `fixed`" and the bit had been there
  since 0.7.3 -- when a comment claims a flag is absent, grep the flag's readers before believing it. And a `fixed` VIEW's
  bytes travel by no qualifier until D-348 step (ii) lands (DEF-246: a view ranged from, copied out of, or assigned from
  `fixed` storage, or handed through a `Writer` impl that dropped the trait's `fixed`, writes it -- two of the six faces fault):
  until then write through no view of anything `fixed`, and spell `write`'s parameter as the trait does.
- **`fixed T[]` is a TYPE, the read-only view, and a view's rights travel by its type** (D-348 (ii), D-350, D-351;
  landing 103): `fixed uint8[]:v` means what it meant -- written once, read-only through it -- and `v`'s TYPE is
  `fixed uint8[]` wherever `v` goes: a plain `T[]` converts to it at any slot (an argument, a field, a return, a
  `List<fixed uint8[]>` element) and it converts back to nothing (TYPE-007 with the rule; `=>`/`=>!` TYPE-032), a
  write through it is TYPE-086 and an address of its element TYPE-071 in whatever slot it sits, a range of `fixed`
  storage and a slice read off a `fixed` struct are produced AS the view, `string_bytes` returns one (D-351: a
  reader of the bridge declares its slot `fixed uint8[]`; seven of the tree's own did), and an impl's slice
  parameter is `fixed` exactly where its trait's is (TYPE-014). In a bare type position -- a type argument, a function
  type's parameter, a return type -- `fixed` precedes a slice type only: `fixed int32` there, `stack` on a return and
  `fixed` on a cast target are PARSE-013 (DEF-247: they parsed and meant nothing). The type is `TY_SLICE` with its `b`
  slot set (`tt_slice_fixed`, `type_slice_fixed`): every kind-keyed walker keeps its answer, and interning one more
  type in the prelude shifts every type id after it -- the emission comparison renumbers (`tidnorm.py`). A
  re-pointable binding of a read-only view is not expressible (R1): walk a view by an offset, or `for`.
- **A write or an address into something that is not a place is ONE report** (landing 103; D-240): `require_place`
  and `require_place_operand` answer false after TYPE-024 and their callers stop -- `(raw f(v))[0i64] = b` over a
  read-only view was TYPE-024 and TYPE-086 for an hour. When a new rule asks a question of a target, ask whether the
  target's own refusal comes first.

- **A `frac`'s parts are its readable parts, and the prelude is regenerated after any edit** (D-347, DEF-231;
  landing 97): whole and num share a sign, |num| < denom, value = whole + num/denom always -- so −1 3/8 is
  {−1, −3, 8}, prints "-1 3/8", `.whole` is −1 and `=>! int` is the whole field; one value has one form, so a
  derived `Hash` and `==` agree. `src/prelude/prelude.npk` is EMBEDDED through the generated
  `src/frontend/prelude_source.npk`: run `python3 bootstrap/generator/gen_tables.py` (no arguments; `--check` only
  checks) before building after any prelude edit, or the compiler carries the old prelude in silence -- the
  landing's first build measured the old behaviour that way.
- **A harness and a manifest re-record in one worktree RACE; the harness starts after the record, on the committed
  final tree** (landing 95's first harness, 2026-10-08): the harness's `verify` stage compares the compiler's rows
  with `nitpick.obligations` AS IT READS IT, and `npkg verify --record` rewrites that file -- so a record step that
  lands under a running harness reds the stage on every re-keyed row (188 rows there: landing 94's rows re-keyed
  each way, which the record's own gate had already read as zero moved) and leaves `parity` with no verified
  compiler. A red of that shape is READ, not re-run blind: its rows are the gate's re-keyed rows exactly, or
  something else moved. Sequence a landing: chain, record, in-process checks, COMMIT, then the harness on that
  commit -- a docs-only amend under a running harness is harmless, a `src/` or manifest change is not.
- **A prelude edit moved EVERY program's emission through D-179's site table until landing 126** (landing 97; DEF-242,
  fixed at 126): each emission carried the prelude's WHOLE error-origin table (`@npk.sitep.N`, `@npk.site.paths`,
  `@npk.site.lines`; some 290 prelude sites, referenced or not), numbered before the program's own sites, so a guard
  added to the prelude shifted every program site's id and a line added shifted every prelude site's line below it --
  561 of 561 programs "different" at landing 97, whose frac section gained six guards and 27 lines. Since landing 126
  a program's table holds the sites its kept text references, numbered densely per program after D-262's trim
  (`irw_renumber_sites`), so a prelude edit moves only the programs that reach the edited body. A comparison ACROSS
  landing 126, or across a prelude edit that changes which bodies a program keeps, is read with the table
  canonicalised first (`python3 meta/roadmap/1.6/tools/sitenorm.py BASE_PRELUDE NEW_PRELUDE PAIRS_DIR .base.ll
  .new.ll`: ids to path and line, prelude lines mapped through the edit); the residue is what the landing changed. A
  prelude edit still moves every verify row's SITE KEY (node ids) while the problem hashes stand: compare `rows.txt`
  by its hash column, and an `.smt2` without its `;` comment lines.

- **A `fixed` slice is read-only through it** (D-348 (i), DEF-230; landing 98): `fixed uint8[]:v` cannot be
  written through (`v[i] = b`, `v[i] += 1u8`, `v[lo...hi][i] = b` are TYPE-086), its elements have no address
  (`@v[i]`, `$$m v[i]`, `v[i].bump()` are TYPE-071), and the same holds for a slice held in a `fixed` aggregate or
  declared a `fixed` field -- `place_fixed`'s walk goes through a slice base to the view's root (a pointer and a
  handle still stop it; `place_through_slice` picks the view's sentence). Read it, range it, pass it on; a callee
  that writes takes a plain `T[]`. Handing a `fixed` view to a plain parameter still compiles until D-348's step
  (ii) makes `fixed T[]` a type: do not rely on `fixed` at a call site to protect bytes a callee declares plain.

- **An `exit` operand is held to `int32` as any slot's value is** (DEF-249, landing 104): `exit may(x);` of a fallible
  `may`, `exit 5i64;` and `exit true;` are TYPE-007 at the operand, in `main` and `failsafe` alike -- unwrap first (`?!`, `?|`,
  a `pick`). Two lessons. SETTING `e.expected` types an operand under an expectation and holds it to nothing: every site that
  sets it owes a check after `type_of_expr`, or the emitter meets a type the statement never promised (here `llc` refused the
  `Result`'s pair where an `i32` stands -- loud, in the wrong phase); grep the `e.expected =` sites when adding a statement kind.
  And the check is `fits` ONLY where the emitter converts: `fits` admits the `Optional` wrap, the `dyn` coercion, the read-only
  view, and `check_slot_sites_agree` (harness.py's SLOT_SITE_PAIRS) refuses a `fits` site with no `emit_fit` partner -- a slot
  that admits no conversion compares exactly, as a condition's `!= t_bool` does. And D-240: an entry point whose signature is
  refused (TYPE-083/TYPE-044) keeps that one report; the operand that follows the wrong signature is not a second mistake.

- **An address-taken binding drops unconditionally at its scope exit** (DEF-159, landing 105): once `@x`, `$$i x`/`$$m x`
  or a pointer-receiver call has taken a flagged local's address, its scope-exit drop is the bare `call @"npk.drop.<tid>"`
  -- the flag a move, a pass or a destroy clears stays the fast path only for a binding nobody addressed. What makes the
  bare call sound is the slot's VACANCY (DEF-120, DEF-158): every site that clears a local's flag vacates the slot first
  (`clear_root_flag` has two callers, `emit_move_out` and `destroy_vacate`, and both do), so a NEW flag-clearing site owes
  the vacate too, and a new address-producing lowering owes a `mark_root_taken`. An emission comparison across a change to
  the drops is READ with `meta/roadmap/1.6/tools/dropnorm.py` (the flag tests stripped from both sides, temporaries and
  labels renumbered); a raw diff shows every later temporary renamed.

- **An enum values every variant or none, and a macro parameter cannot name what the body emits** (D-342 R1, D-343;
  landing 106): `{ A; B; Late = 9i32; }` is TYPE-093 at `Late` -- write `A = 0i32; B = 1i32; Late = 9i32;` or no values
  (an enum with a payload variant values none: a payload variant takes no value) -- and `macro:m = (N) { func:N = …; };`
  is MACRO-011 at the declaration (the argument would be dropped and the function literally called `N`). Two tooling
  lessons from the landing: a rejection file's header is REGENERATED from the checker's own report to a fixed point
  (`.internal/handoff_34/tools/header_from_check.py`: a header that grows moves every site's line, so one pass is never
  enough), and the tool must read the checker's STDERR -- its first run saw no sites and wrote an empty header.
  `tests/grammar/` is parse-only: a construct the CHECKER refuses may stay there as a grammar sample (`whole_grammar.npk`'s
  mixed `Net`), since the sweep's checker never reaches its enum.

- **A builtin is called, never named as a value** (DEF-243, landing 107): `func int64() never fails:f = mono_now;`,
  `call_it(mono_now)`, `Holder{ f: mono_now }`, `pass mono_now;` are TYPE-054 at the name -- write a function that calls the
  builtin and name that. In the checker, a type of 0 means "refused and already reported" (D-240), and `type_ident`
  answered 0 for a bare builtin's name with NOTHING reported: a slot's fit then asks nothing more, and the shape reaches the
  emitter. When a typer returns 0, ask who reported. And a call's callee must never reach `type_ident`: the first form of
  this rule refused every builtin CALL in the prelude, which the test's own `mono_now()` control and a prelude-heavy program
  showed before the chain ran -- a new rule about a NAME is measured on a program full of that name's ordinary uses first.

- **A module binding may be initialised from another `fixed` module binding of ANY type** (DEF-202, landing 108):
  `fixed Pt:Q = P;`, `fixed int32[3]:B = A;`, `fixed int32?:O2 = O;`, `Box{ at: P, dims: A }` and a reference to an
  inline module's `pub fixed` compile; the renderer copies the target's initialiser into the reader's constant (LLVM has
  no constant that is another global's VALUE). Two lessons. A walk that answers "what does this name stand for" belongs
  in ONE function every consumer reads (`fixed_global_sym`): the folder had it as a private step, and the gate and the
  renderer asked the folder for a VALUE instead -- which it holds for a scalar and a string only -- so the refusal's own
  sentence listed the form it refused, since D-165 landed (1.0.9d). And when a gate admits a reference, ask who reports the TARGET'S
  mistake: its own declaration does, so the reference answers "constant" and D-240's one report stands at the binding
  that holds the mistake -- following the reference into the gate would have reported one mistake twice.

- **`failsafe`'s pick may stand in a bare block, a statement macro's expansion among them** (DEF-224, D-352; landing 109):
  `func:failsafe = int32(Error:e) { #shared_failsafe(e); exit 9i32; };` compiles with the macro's pick, and so does
  `{ { pick (e) { … } } }`; a pick inside an `if`, a `when`, a loop or another pick's arm is REACH-001 as before, because
  the path that skips it reaches no arm. REACH-002 asks the arm contract of the pick where it stands, so a shared macro
  names what EVERY program using it reaches -- the union; MACRO-009's note points the report into the macro body. The
  lesson is D-085's cousin: "must contain" was implemented as "among the body's own statements", and the one shape the
  reference's own text invited (an expansion is a block) was refused from the day D-340 let a probe reach it (landing 92). When an analysis walks a body for ONE
  construct, ask what else the grammar lets stand between the body and the construct, and whether that thing changes
  the construct's meaning (a bare block does not; a conditional does).

- **A `pick (r.is_error)` whose error arm leaves checks the `Result` for what follows** (DEF-226, landing 110):
  `pick (r.is_error) { (true) { exit 10i32; }, (false) { } }` and then `r.value` compiles, with `!!! Unreachable;` or
  `#unreachable();` in the error arm alike -- as `if (r.is_error) { … }` always did. An error arm that falls through keeps
  the read tainted: the take is an intersection over the arms that fall through, never a union. The lesson: when a merge
  carries several definite facts, carry them in ONE function (`state_take_definite`), or the next fact added to the
  intersection (`checked` joined `state_intersect_must` long ago) is intersected and then dropped at the take, and the
  refinement holds inside the arm and nowhere after it.

- **A `give` lives in a pick EXPRESSION's arm and a `fall` in any pick arm, the INNERMOST pick governs, and a `defer`
  body leaves for nowhere; a `frac` member is never a place** (DEF-233, DEF-238; landing 111): `give x;` or `fall two;` in a
  plain body is TYPE-094 (the compiler exited 3 in silence before -- the emitter's slot lookup for a pick that was not
  there), a `give` in a statement pick's arm is TYPE-094 even when a pick expression's arm encloses the statement pick (it
  was EMIT-002: the statement pick's slot is empty), a `give` or a `fall` inside `defer { … }` is TYPE-040 (the body runs at
  the scope's exit, when the pick is no longer open: exit 3 in silence before), and `f.num = …`, `f.whole += …`, `@f.denom`,
  `$$m f.num`, `move(f.num)` are TYPE-024 by name (a frac is written whole). Three lessons. A statement kind "with nothing
  to type" still has a PLACE it is legal in: when the checker passes a kind through, ask what the emitter needs around it.
  A depth counter that says "inside an X" must be RESET by every construct that ends X's meaning (a statement pick's arm
  ends the give-ability a pick expression's arm began; a `defer` body ends every arm), and probed at every such construct
  -- the landing seat found both faces with twelve-line probes after the first form looked complete. And the three address
  typers ask the place question BEFORE typing their operand (`require_place_operand`, then `type_of_expr`), so a place rule
  that reads a recorded type must be asked again after the typing -- the first form of this landing missed `@g.denom` for
  that order, and only the test's address sites showed it.

- **A `NIL` member of an aggregate lowers as `{}` and is never touched** (DEF-232, landing 112): `struct:S = { int32:a; NIL:b; }`,
  `Box<NIL>`, `S[2]` and `enum:E = { A(NIL); B; }` compile and run; `#size_of<S>()` counts the other fields (D-084). `ll_type`'s
  `void` is a RETURN type and nothing else -- an aggregate lowering asks `ll_member_type`, which maps `NIL` to `{}`, and a NEW
  aggregate lowering or a new site that stores into, loads from or renders a member must ask the member's kind and skip a `NIL`
  (the literal, the read, the store, the module-constant renderer and the enum constructor do). Two lessons. A layout promise
  ("occupies nothing") is kept by EVERY consumer of the layout, and here the emitter had one answer for a kind in two positions.
  And a derivation that answers "bad" for a legal degenerate case (zero bytes past an enum's tag) must fail loudly: the enum's
  header was skipped in silence and `llc` reported an unsized type two steps later.

- **No keyword is a declared name, and the compiler says so where the name is written** (DEF-235, landing 113): `func:any`,
  `struct:any`, `mod:trit`, `func:acquire` in an impl, `int32:acquire` as a parameter, `wild int8->:buffer` as a local are
  PARSE-001 naming the keyword -- each compiled before and could never be named (`on`, `buffer`, `any`, `acquire` read like
  the most ordinary names, and the landing's own first build was refused for a local named `on`; its second found FOUR
  keyword-named FIELDS in the compiler's own source -- `FloatScan.end`, `FInstr.atomic`, the encoder's `result` term,
  `Counter.limit` -- that resolved only because a keyword token's name is 0 on both sides, a second such field in one
  struct would have aliased the first in silence). The dotted spellings
  `m.acquire()`, `v.any()`, `t.trit(i)`, `n.nit(i)` are the compiler's intercepts, declared nowhere: an exemption written for
  "method names" protected nothing, because a keyword token in declaration position carries no intern -- when a rule exempts
  a case, build the case and watch it work before keeping the exemption. Two readers, one rule: `p_declared_name` for every
  declaration kind, `p_binding_name` for every binding kind; a NEW name-reading site in the parser asks one of them, never
  `p_advance` -- the generic window's names, a variadic's and a macro parameter's joined them at landing 136
  (DEF-258), a local `T:keyword` is PARSE-001 at the name since the statement classifier reads a keyword followed
  by `=` or `;` as a declaration's name (DEF-259), and a function modifier stands before `func:` alone (DEF-261).
- **A `joins` deadline, an attribute's name and an `opaque struct`'s position are checked now** (DEF-236 (c)–(e), landing 113):
  `joins Duration{ ns: 1000000000i64 }`, `joins LIMIT` (a `fixed` module binding) and `joins comptime(…)` compile, a call or a
  parameter's value is TYPE-045 -- the module-constant gate's verdict (`const_init_verdict`) is the one notion of "constant
  expression", so a new "constant only" rule asks it rather than growing a list; `#[derive(…)]` is the one attribute, so
  `#[lexical_drop]`, `#[nosuch]` and ANY attribute on a local are DERIVE-007 (a window nobody reads is a window the language
  does not have: when a syntax is parsed and consumed by one reader, ask what that reader does with the rest); an `opaque
  struct` compiles nowhere today (RESOLVE-014 outside an `extern` block, EXTERN-001's reserved tier inside). A constant cycle is
  ONE RESOLVE-006 naming the chain (DEF-234 (c)): a walk that answers "does it reach itself" per member reports each member
  for one mistake; the chain is found once and the first member in source order carries it (D-240).

- **A module binding is `fixed`, with or without an initialiser** (DEF-250, landing 114): `int64:counter;` at module level is
  NITPICK-TYPE-055 as `int64:counter = 0i64;` always was -- write `fixed int64:COUNT = 0i64;` for a constant and keep mutable
  process state in `main`'s scope. Until this landing the bare binding lowered to a `constant` global: `counter = 5i64;` faulted at
  -O0 (`MachineFault`) and at -O2 `opt` deleted the store and read back ZERO, a wrong value from a program the checker had accepted
  (DEF-106's shape, two legs disagreeing). The lesson is D-085's cousin: a check written inside a branch is asked only of what
  enters the branch. When a rule says "every X", read where its check sits against every optional part of X -- here the
  initialiser -- and probe the shape WITHOUT the optional part; the library seat's plan did, the tree never had.

- **Every emitted `define` carries `null_pointer_is_valid`, and an emission comparison across landing 115 is read with it
  stripped** (DEF-251 (i), landing 115): `ll_fn_open` writes `) "split-stack" null_pointer_is_valid {`, so address zero is memory
  to every LLVM pass -- a store through `NULL` faults into `MachineFault` at -O2 as at -O0, where `opt` had read it as undefined,
  emptied `main` to `unreachable` and let the processor run into the next function (a reported SUCCESS in one shape). Compare
  emissions across the landing with `meta/roadmap/1.6/tools/attrnorm.py` (tidnorm's twin), and a NEW emitter site that opens a
  define writes `ll_fn_open`'s text, never its own. Two lessons. LLVM's attribute is the KEYWORD `null_pointer_is_valid`; the
  quoted string `"null-pointer-is-valid"` parses, is kept, and is read by no pass -- the first form of this landing changed
  nothing at -O2, which only the measurement showed. And the last net (D-307) is a property of the OBJECT, not the source: a
  fault the plain leg catches can be optimised into a fall-through, so a trap-route test is run at both legs (`null_store_legs.npk`'s
  helper is declared between `main` and `failsafe` on purpose, to be the function an empty `main` falls into).

- **A holder this frame owns is bound by its block, and a callee's store is asked the same question** (DEF-252, landing
  116; DEF-42's rule, D-004 rule 3): `c = Cursor{ src: string_bytes(t) }` AND `reset(@c, string_bytes(t))` -- a callee whose
  body stores its view parameter into the pointee -- are BORROW-002 when `c` is declared outside the block that declares `t`;
  so are `list_push(@outer, string_bytes(t))`, a method's receiver, a `dyn`'s method, a function value, an address stored
  through a callee, a `move` parameter holder with `t` in a nested block, and `@(<-p)` as a destination. Declare the holder
  inside the block, store a copy (`string_concat("", …)`, `.clone()`), or pass the borrow down. In escape.npk the question is
  ONE predicate, `holder_scope_violated`, asked by the direct store (`check_holder_scope`) and the call (`refuse_call_holder_scope`);
  a by-value parameter's bound is the body's block (`param_holder`), because the resolver declares a parameter in the FUNCTION
  scope, one above the body, and both die with the frame -- a new holder kind owes the same question and the right bound. Two
  lessons. A rule written for one spelling of a store (the assignment) was owed to the other (a callee's store) for a cycle, and
  the summaries that connected the store were read as the whole of rule 3. And "benign by construction" was a claim about an
  ADDRESS (a function-lifetime alloca) carried over to a VIEW of a block-scoped string's heap body, which the block's end frees:
  when a rule is called unchecked-but-benign, ask what else its subject can be, and run the shape at both legs.

- **A `comptime` value parameter is RESERVED: `NITPICK-TYPE-095` at the declaration, once, and nothing at the uses** (D-353,
  D-354; landing 117): `func:scale<comptime int32:K>`, `struct:Lock<T, comptime int32:LEVEL>`, a trait's, an enum's or an impl's
  value parameter refuses where it is declared; write the constant as a `fixed` module binding or a macro parameter. The
  compiler-known value arguments -- `Mutex<T, 2>`'s level, `simd<int32, 4>`'s lane count, `int32[N]`, a channel's constants --
  are not this form. In the checker the refusal lives in `collect_param_bounds` (type_trait.npk), the one pass every
  declaration's generic window goes through in the pipeline AND in `type_generic.npk`'s helper, which walks module functions
  itself and never reaches `check_decl`: a rule put in `check_decl` is invisible to that unit test, and routing the helper through
  `check_module` exposed impl-method bodies it had never checked (one refused under D-264). Two lessons. A refusal that must be
  asked once per DECLARATION KIND goes where every kind passes, not in a per-kind arm that one kind's test never runs. And a
  dedicated code for a reserved form is what lets the frozen corpus (D-336) hold "refused today, maybe accepted after the
  freeze" without a red run -- a reservation is a verdict with a direction, and TYPE-016 would have had none.

- **A method's symbol names the impl's TARGET, and a pointer receiver is peeled before every reader of the receiver's type decides
  anything** (DEF-253, DEF-254; landing 118; D-156): `func:drive<F: Src> = int32(F->:f)` calling `f.step()` links at `Count` now
  (the emitter's bound-call branch built `Count->:Src.step` and `llc` refused it), and so does a trait default `func:tock =
  int32(Self->:self) never fails { pass ((raw self.tick()) * 2i32); }` -- inside a default `self` is the POINTER to `Self`, and the
  `Self`-receiver test that picks the impl being emitted read the pointer's kind and left the bare `tick` on every compiler since
  D-185. In `emit_method_call`'s `msym == 0` branch and in `async_callee_name` the receiver's type is peeled ONCE, first; a new
  reader of a receiver's type (a symbol, an instance note, an impl lookup) asks the peeled type. Two lessons. The async twin had
  the peel since 1.1.12c and the sync branch did not: when one of two twin paths learns a rule, grep the other the same day (the
  rule this file states for `pick`'s two spellings). And a test that calls a method through a bound covers the shapes it
  spells: the tree's eight bound-call programs were all by-value or async, so the sync pointer form -- the shape a plugin
  interface is written around -- went unlinked for three cycles until a library's probe spelled it.

- **`NULL` is a `wild` pointer's value, and a plain `T->` that may be absent is `T->?`** (D-355, landing 119): `int64->:p = NULL;`,
  `p == NULL` on a plain pointer, `Holder{ inner: NULL }` over a plain field, `pass NULL` from a plain `T->()` and `int64->?:p =
  NULL;` are NITPICK-TYPE-096 -- write `int64->?:p = NIL;`, test `p != NIL`, read `p ?? @d` or `p?.f` (a field, still wrapped;
  `?.` reaches no method, so a `Self->` method on the pointee is called on `p ?? @d` until S-137 is answered), or declare the
  storage `wild` where it IS manual storage (`wild int8->:buf = NULL;`, `b.data = NULL;`, `w == NULL`, `dalloc(NULL)`,
  `fixed wild int64->:P = NULL;` all stand). In a test, a placeholder pointer "filled later" is an Optional pointer field with
  `NIL` (a borrow-free start) or a same-block dummy's address where the case reads through it afterwards -- never a `wild` field,
  which the borrow analysis does not track. Three lessons. A "maybe" reference is a TYPE, not a sentinel: the hazard leaves safe
  code by type, as D-010 removed the uninitialised read. A census counts qualifiers at the DECLARATION: the ruling's 22 "plain"
  compiler sites were `wild` fields read at their uses, and the plan's first measurement corrected it before a line was written.
  And a context the typer carries beside `expected` (`ExprTyper.null_ctx`) is SET by the declared slots and RESET by the
  dispatcher at every node that is not transparent (`null_ctx_passes`): a stale context reaching a nested slot is a hole, and the
  probes of the nested shapes (`wild int8->:w = raw f(NULL)` with `f` plain, a ternary arm into a plain slot) ran before the sweep.

- **`??!` is the Optional's unwrap-or-trap, and it is `?!`'s twin in every rule** (D-360, landing 120): `T->:q = p ??! Ident;`
  is the pointer, or a trap with the named identity -- a `Self->` method on a `T->?`'s pointee is `(p ??! Ident).m()`, no dummy
  object; `failsafe` names the identity (REACH-002 otherwise); the left operand is an `Optional` (a `Result` is `?!`/`?|`'s, a
  plain pointer is never absent, D-355), the right one `Error` identity; a contract refuses it (TYPE-060). Three lessons. A new
  TOKEN or NODE is written into the reference first -- LEXICAL_REFERENCE §5's production and AST_REFERENCE §3.4's table -- and
  `gen_tables.py` writes `token_kind.npk`, `operators.npk`, `token_name.npk` and `ast_kind.npk` from them (the in-process
  `check_generated_current` refused this landing's hand-edited `ast_kind.npk` within the minute; the kinds after a new row
  renumber, which nothing reads). A node of shape TWO in `ast.npk`'s table is walked by every shape-driven analysis for free and
  owes an arm only to the kind-keyed ones (the parser, the typer and its contract rules, REACH, the lock walk, the emitter's
  producer list and lowering, the encoder's `expr_children`, the harness's `KIND_STATUS`) -- grep the twin operator's kind to
  find them all. And a trap with no callee behind it is an ORIGIN: `npk_chain_reset` as `!!!`, never `npk_chain_push` as `?!`,
  which extends a chain the callee stamped.

- **A wild pointer enters a plain pointer slot only as `v =>! T->`** (D-361, landing 121): `int32->:q = w;` from a `wild`
  binding, `int32->:r = #ptr_add<int32>(w, 0i64);`, `raw f(w)` into a plain parameter, `H{ p: w }` over a plain field, `pass w`
  from a plain `T->()` and `uint8->:p = s.ptr` (a header's `ptr` is the allocator's block, wild by provenance since landing
  119) are NITPICK-TYPE-097 -- write the crossing (`w =>! int32->`, `s.ptr =>! uint8->`: the author's assertion that the
  pointer points at something readable, a same-type cast that lowers to nothing) or declare the slot `wild`. A dereference in
  place (`<-(#ptr_add<T>(p, k))`), a store through a `wild` pointer, a `wild` parameter or field and an `=>! wild` cast need
  nothing. In `src/`, a wild table handle (`LlCtx`'s `tr`/`obl`/`impls`) handed to a helper's plain parameter is written
  `x.fe.ctx.impls =>! ImplTable->`. Three lessons. A rule about a VALUE entering a SLOT lives at the slots that set the slot's
  context (119's `null_ctx` sites), asked after the value is typed and before the context is restored -- and a dereference
  target is a slot too, whose context is its pointer's provenance (the first form refused `<-slot = p` through a `wild
  T->->`; the fuzz corpus's `bi0123b` found it). The census is the CHECKER's: the textual count of `#ptr_add` (169 lines) said
  nothing about which results escaped, and the two-checker sweep said exactly (thirteen sites in the compiler -- the twelve table handles and `assoc_binding_of`'s one; this said twelve until landing 123 -- none in the
  libraries). And a confinement of one producer is read against the general rule it belongs to: making `#ptr_add` stricter
  than an `=>! wild` cast's result would have churned the Bridge for no safety.

- **A `comptime func:` is called inside `comptime(…)`, at a constant site, or from another `comptime func:`, and nowhere
  else** (D-357, landing 122): `int32:v = raw dbl(3i32);` in a run-time body, `fixed int32:k = raw dbl(3i32);` as a LOCAL,
  `(raw dbl(3i32)) + 1i32`, `raw inner.c_f(3i32)`, `raw dbl(x)` in a `requires` or a `prove`, and `g = dbl` as a function value
  are NITPICK-TYPE-098 -- write `comptime(dbl(3i32))` (the operand is the evaluator's alone), or declare an ordinary `func:`.
  A module binding's initialiser, an array size and an `assert_static` proposition are constant sites (each must fold and is
  emitted nowhere), so `fixed int32:K = raw dbl(21i32);` stands. A METHOD cannot be `comptime` (the evaluator folds a named
  call and never a method call): TYPE-098 at the declaration. The emitter never sees a `comptime func:`; if EMIT-time TYPE-098
  ever names one, a road reached the emitter that the checker did not ask -- find the road. Two lessons. A context flag the
  typer carries per function (`fn_pure`, `fn_comptime`, the call-graph edge's caller) is SET by every function and must be
  RESET at every declaration that is not one, or module-level typing inherits the last function's: a module binding after a
  `pure` function was refused TYPE-061 for a cycle (DEF-262), found by this landing's own program test -- when a test fails
  on a shape the rule under test does not touch, measure the shape on the BASE compiler before reading the rule. And a
  frontend unit that walks fixtures with its own loop models only what it sets: `type_stmt.npk`'s walk set two of the
  checker's eleven per-function fields, so a new rule read through a third saw no context there -- the unit's walk owes
  every field `check_function` sets that a rule reads.

- **A payload-less enum variant is a constant outside the evaluator** (D-356, landing 123): `fixed Ordering:O = Ordering.Less;`,
  a struct's or an array's member, a `comptime func:` that yields one and `comptime(Ordering.Less)` compile and are the variant's
  TAG -- the `i32`, or a tagged enum's aggregate with a zeroed slot -- where landings 90 through 122 refused them (TYPE-035;
  TYPE-004). A generic enum's variant is not folded (its instance is not in the expression) and stays TYPE-035. Two lessons.
  The evaluator's value is a POSITION (`CV_ENUM`'s `num`) and the emission wants the TAG: an enum with declared values
  (`{ A = 10i32; B = 20i32; }`, D-342) makes the two differ, so a renderer of an enum value asks `variant_tag_of` and never
  writes `num` -- `const_scalar_text`'s fallthrough would have written 1 for `Code.B` -- and a test of an enum constant reads
  the declared tag back through a `pick`'s arms, which compare tags. And the constant of an aggregate enum is written beside
  the body it must match (`ll_enum_tag_const` next to `ll_enum_body`): two texts of one shape in two files is how a layout
  drifts.

- **An impl names at least one type or trait of its own manifest's tree** (D-359, landing 124): `impl:bool:Ord`, `impl:int32:Ord`,
  `impl:List<int32>:ToString`, `impl:<T>:List<T>:ToString`, a blanket impl of a prelude trait and `impl:string = { … }` are
  NITPICK-TYPE-099 at the impl -- write your own trait for the type, or the prelude's trait for a type of your own; a type
  ARGUMENT counts (`impl:List<Pt>:ToString` names `Pt`). A refused impl is the one report: no TYPE-013 against the prelude's
  row, no TYPE-087, no completeness question -- its entry stays in the table so a call reports nothing more. The tree is the
  nearest `nitpick.toml` above the FILE (`srcmgr_tree_of`), else the program's root; the prelude and a derive's synthetic file
  are exempt. Two lessons. A plain pointer STORED into a `wild` field is BORROW-011 (D-223) while a `wild` PARAMETER takes a
  plain address at its call: a handle a table must hold is taken as `llctx_set_impls` takes its tables. And a frontend unit
  that calls the path helpers releases the frontend's wild blocks before its `exit` (`wild_release_all()`): `path_normalise`
  leaves two behind by design and the compiler bulk-releases them, so a unit without the release reads as a WildLeak.

- **A driver method may take `int8[]`/`uint8[]`, and `use a.b.name;` binds `name`** (DEF-234 (a) and (b), landing 125): the
  stub the compiler generates for a byte payload stated no `decreases` and was refused by the compiler's own loop rule
  (TYPE-072 at `<bridge-1>`) from 1.5.8c until this landing -- GENERATED source is checked like any other, so a generator
  owes every rule the checker asks, and a rule added later is owed to every generator (the derive's, the bridge's); and
  the parser's logical-path walk ate every `.ident` as a module segment, so the single-name form the reference lists
  never reached its branch and the loader said "not a module" -- when a form is documented and refused, read the PARSER's
  shape of it before the resolver's: the refusal's sentence named the wrong stage.

- **A program's site table holds the sites its kept text references, and a site table is DENSE against its chain calls**
  (DEF-242, landing 126): `emit_all` trims the items, `irw_renumber_sites` renumbers the two call shapes' ids from 1 in
  registration order and writes `site_keep`, and `emit_site_tables` writes the tables from that list -- so a NEW writer of a
  site reference must spell `call void @npk_chain_reset(i32 N)` or `call void @npk_chain_push(i32 N)` through `irw_line`
  (the scanner reads those two shapes and nothing else), and a new table or lookup that reads a site by id must read it
  through `site_keep`; both runners' belts (`check_site_table_dense`, `ir_site_table_dense`) fail a module whose table has
  a row no call references or whose call names no row, by name. A string constant that SPELLS the call (the compiler's own
  emitter text) is not a reference: the scanner matches at a line's start. **`site_path` answers a VIEW** (DEF-263, found by
  this landing's program test): the lookup's third word is `cap` 0 -- until 126 it was the length, an owned claim over the
  table's constant, and binding the answer to a local freed `.rodata` at the scope's exit (exit 70 inside `failsafe`). A
  generated lookup that builds a string over a constant writes `cap` 0; a `string` with `cap == len` is a claim the drop
  body will act on.
- **A `buffer` is indexed `b[i]` and ranged `b[lo...hi]`, and a `#wild_slice` over a header's `.ptr` is a VIEW of the header's
  owner** (D-341, landing 128): `b[i]` is the checked `uint8` place (`OutOfBounds` past `len`; `(<-p)[i]` behind a pointer --
  `p[i]` is TYPE-082), `b[lo...hi]` the checked `uint8[]` view, frozen like a `List`'s (D-325); access at another width stays
  `#ptr_add<T>` + `<-`. `#wild_slice<uint8>(x.ptr, n)` roots at `x` now, as `string_from_bytes(x.ptr, n)` always did: writing
  over `x`, moving out of it or handing `@x` to a writer while the view lives is BORROW-015, returning it from `x`'s frame
  BORROW-001 -- take the view in a block that ends before the write, or spell the view off a `wild` pointer when that is what
  it is. A `#`-sigil builtin's Views value is a `**Views: N**` marker in its reference row's Description (the sigil table has
  no Views column); `gen_tables.py` reads it into `builtin_views`, and `escape_expr`'s `#`-builtin arm asks `view_place` --
  a new viewing `#` form needs the marker and nothing else. A `fixed` buffer's bytes are still writable through `.ptr`
  (DEF-264, open): do not rely on `fixed` to protect a buffer's bytes reached by its pointer.
- **A file is ONE module whatever spelling names it, and the source manager's key is the file's identity** (DEF-244,
  landing 129): under a driver the key is the absolute normalised path (`srcmgr_key`, source.npk: the cwd D-236 recorded,
  joined and normalised), so a root named `a.npk` from its own directory and the same file imported back as
  `../b/../a/a.npk` are one id -- before this they were two keys, the file was read twice, and the symptoms were
  RESOLVE-010 ("an FNV collision") at its one error constant and RESOLVE-013 at its `main`. A unit's manager (no cwd) keys
  by the normalised spelling as before, which is why a unit that loads files through a bare `SourceManager` sees the old
  behaviour unless it calls `srcmgr_set_root`. The shown path is unchanged: it was rendered from the absolute path already,
  so an emission cannot tell the two keyings apart. A new reader of `SourceFile.path` reads a KEY, absolute under a driver.
- **`Trait.m(x)` reaches what `x.m()` reaches** (DEF-255, DEF-266; landing 131): a pointer receiver peels one level,
  a generic parameter is answered from its bounds by the trait the call NAMES (so it chooses among two bounds that
  supply one name), `Self` in a trait's body from that trait, a concrete family instance is recorded, and an `async`
  method needs `await` or `drop` (TYPE-043). The emitter names a bound receiver's symbol in ONE function both
  spellings ask (`bound_method_sym`, ir_expr.npk): a new road to a method's symbol asks it too. The awaited and the
  spawned forms reach since landing 137 (DEF-267): the await typer skips a trait-named operand, and the child-frame
  builder and `async_callee_name` read the receiver from argument 0 -- a new reader of an awaited or spawned call's
  receiver asks `emit_is_trait_qualified` before it reads `expr_operand`.
- **An explicit `<-` is a place step to both borrow analyses, as the auto-dereference is** (DEF-272,
  landing 138; the fuzzer's F-050): `(<-p).f` is `p.f`'s place and path, and `(<-lp)[i]` -- the one
  spelling of an element behind a pointer (TYPE-082) -- roots at `lp`, so a callee's store through one
  enters its store matrix, its write the mutation summary, its view the view summary, its move DEF-119's
  write, and the aliasing walk's parties see it; `pass @((<-a)[0i64])` from a pointer parameter is legal
  now (it was BORROW-001). In escape.npk the root walk is `esc_place_root`, never `place_root` directly:
  a new reader of a place's root asks it. `place_root` itself still stops at `<-` for the bindings
  analysis, the suspend walk and the emitter, which read `<-p` as the value it yields. A DESTINATION of
  a callee's store reached through a pointer (`@lp.f`, `@((<-lp)[i])`, `@(<-p)`) is BORROW-002
  (`place_through_pointer`): a holder's block bounds the pointer, not its pointee -- hand `@place` of a
  binding this frame owns, or store a copy. The pointer-root exemption of alias.npk (`b = other` leaves
  the viewed pointee alone) is for the BARE NAME written DIRECTLY: `<-b = …` is not it.
- **A pointer VALUE handed to a callee is checked against the views through it, as `@place` is** (DEF-275,
  landing 140): `Holder->:b = @hd; fixed uint8[]:v = string_bytes(b.items[0i64]); drop clobber(b);` is
  BORROW-015 at the call (it read the freed bytes, 170 at both legs); a callee that writes another field
  or reads only stands (its mutation summary says which). The limit the landing measured and KEPT: a view
  of a `List` element's string body across `list_push(@l, …)` is refused as it always was, because a
  callee's mutation path `items/[…]` cannot tell a push (which frees no body) from an overwrite or a
  `list_remove` (which does) -- copy the bytes or end the view's block first; the compiler's own import
  loader copies its path for this reason, and `fnem_cross_gep` answers a copy because the emitter STORES
  the text in its own fields (a self-wired view, found by `npkc FILE --dump-refs OUT`, the escape
  analysis's refs and summaries after its fixpoint -- the tool to read a BORROW-015 of the compiler's own
  source back to the call that recorded the view, never by guessing).
- **A `pick`'s value pattern is a constant of the selector's type** (DEF-178, DEF-187; landing 134): `(true)`, `('A')` or
  `(300i64)` over an `int32` is TYPE-007 (it matched by the literal's raw payload before, in silence), `(300)` over an
  `int8` is TYPE-031, a local `(n)` is TYPE-007 -- compare a run-time value in a guard, `(*) where (v == n) { … }` --
  and a `fixed` binding `(K)`, `comptime(…)` or constant arithmetic is a case. A range needs an ordered selector, and a
  `pick` over a float is TYPE-065 (an `if` chain is the spelling). The checker types a pattern and takes a LITERAL's
  record back (`exprtypes_unset`): the encoder reads a pattern's literal by its kind under the selector's type (L-3),
  and a recorded `char` would make every character `pick`'s conditions fresh symbols and move the manifest.
- **A numeral's digit run ends in a digit, and an attribute decorates only what keeps one** (DEF-190, DEF-182;
  landing 135): `10_`, `1_i32`, `0FF_hex` are LEX-003 and `1.5_f64` LEX-009 (they compiled as the digits before the
  `_`; a doubled `_` inside a run stands, as the production admits). An attribute before a statement, a module binding,
  a `use`, a `mod`, an `extern` block, a `Rules`, an `assoc`, an `opaque struct`, an `error`, a `unit` or a macro
  invocation is DERIVE-007 from the PARSER (`p_refuse_attrs`) -- it was dropped unread; a struct, an enum, a trait, an
  impl, a function and a macro keep their windows, and a local binding's is the checker's. Both are unit cases.
- **A macro takes no type arguments, its body declares only what an expansion can emit, and MACRO-009 notes the right
  invocation** (DEF-186, DEF-185, DEF-188; landing 144): `#one<int32>()` is MACRO-002 (it was ignored); an `error`, a
  `unit`, an `assoc`, a `use`, an `extern` block or an `opaque struct` in a body is MACRO-005 at the macro's declaration
  (it was MACRO-006 at the first instantiation); a note's range is the body's byte extent (`ast_macro_end`), never
  start lines plus slack. A generic `comptime func:` naming its `T` in a type under evaluation is DEF-270, open.
- **The obligation encoder's arms are frames of their own, and a new arm is a function and a test** (DEF-269, landing
  145): `enc_expr` (smt_encode.npk) computes the node's type once and tests the kinds in the order the one function
  tested them; an arm that may DECLINE its node answers an `ArmTerm { mine, t }` and the dispatch goes on where the
  old arm fell through -- a `term_none` is a legitimate answer (the `simd` constructor's), so `Term.ok` cannot carry
  "not mine". A declined builtin or `complex` node reaches the shared tail, the only place its arguments are recorded:
  answering `mine` true without walking them drops rows in silence. The statement walk (`walk_stmt`, 36 KB a frame) was
  the same class, DEF-277, closed at landing 149: eighteen `walk_k_*` arms behind a 216-byte dispatch, the Block arm
  `walk_block` itself (a wrapper would be a third frame on every level), and `stmt_carries_expr`'s arm AFTER the Give,
  Prove and AssertStatic tests the predicate also matches -- ahead of them, their rows vanish in silence.
- **The encoder's Int form of a mask computes on the mask's numeral in `int64`, so the width's two edges are answered,
  never computed** (DEF-279, landing 148): `int_form_and` (smt_encode.npk) reads a positive mask as `m + 1i64` and a
  negative one as `0i64 - m`, and at `2^63 - 1` and `-(2^63)` those sums do not fit -- the COMPILER trapped `IntOverflow`
  under `--obligations` on a legal program (the fuzz corpus's `lx0332b`; the plain build compiled it) until this landing.
  Each edge is the form it is, `(mod a 2^63)` and `(- a (mod a 2^63))`, written before the arithmetic. A helper that
  does arithmetic on a value a PROGRAM spells -- a mask, a shift amount, a literal's digits -- is asked what the width's
  extremes do to it, in the plain build and under `--obligations`, before it lands (DEF-207's lesson again).
- **A body nested in a body is walked ONCE per level by every front-half pass, and a `pick` used as a value gives on
  every path** (DEF-281, DEF-278, DEF-283, DEF-284; landing 152): `assign_loop_body` (bindings.npk) walked every loop
  body twice -- D-116's silent pre-pass, then the real pass -- at EVERY level, so a nest of n loops walked the innermost
  body 2^n times (twenty levels two seconds and 476 MB, twenty-four past 6 GB); a loop met inside a silent pre-pass is
  walked once now (the four unions a silent walk hands up gain nothing from a second), and the real pass still walks
  each loop it meets twice, so no report moves. A pick EXPRESSION's arms were walked twice per level by the escape
  analysis (`gives_borrowy` after `escape_stmt_window`) and typed twice by the checker (`arm_give_type` after
  `check_stmt`): both re-walks are gone -- the statement walks' `give` arms feed the open pick's FRAME (`Escape.givex`;
  `ExprTyper.givex_pick`/`givex_cur`, `type_give_operand`), one walk each. A new analysis that must know something
  about a construct's sub-statements asks it DURING its one walk of them, through a frame it pushes and pops, never by
  walking them again. Two holes the probes found closed with it: a `give` BELOW an arm's top level is held to the
  pick's type (TYPE-007 at the give; `give 7i64` into an `int32` pick compiled and stored the `i64`'s bits), and an arm
  that can reach its end without `give` is FLOW-001 at the arm (the pick yielded its slot's zero in silence) -- end
  every path of an expression pick's arm in `give`, or leave it; a give inside a counted loop or a `when` does not
  count, as D-323's conservatism says. `tests/backend/programs/deep_nests.npk` nests 100 `while`s, 60 `for`s, 60
  `loop`s and 40 pick expressions; the runners' compile timeouts make a walk that doubles again a red, not a hang.
- **A `pick` arm names several values with one body, and every reader of a pattern walks an arm's ALTERNATIVES**
  (D-363 R1, landing 151): `(1i32, 2i32, 5i32) { … }`, `(Color.Red, Color.Green) { … }`, and in `failsafe`
  `(ntime.ETimeValue, ntime.ETzMissing) { exit 87i32; }` -- a comma list of constants of the selector's type, or of
  bind-free paths (an enum's variants, error identities, bare or `file.Name`). A range, `(*)`, a struct destructure or
  a destructure that binds stands alone in its own arm (PARSE-005 in a list); a value repeated in one list, or already
  matched by an earlier unguarded arm, is PICK-004 at its second spelling -- the checker folds and compares the
  CONSTANTS (`refuse_repeated_value`), the exhaustiveness analysis the paths and bare names (`alt_matched_before`), so
  a repeated `(Boom)` arm is dead now, which no rule had said. The node is `PatValueList`, a window of the alternatives'
  pattern ids: a NEW reader of a pattern loops `pat_alt_count`/`pat_alt_at` (parse_pick.npk) instead of asking the arm's
  node its kind, or it sees a list where it expects a value and falls to its fail-closed arm. The emitter writes a list
  as several `switch` cases to one label and, in the chain, or-s `alt_hit`'s tests; the encoder's arm condition is the
  disjunction (the wildcard's negation covers it); the reach analysis reads every alternative as a named arm, so
  REACH-002 is unchanged. A path over a selector that is no enum -- `(m.K)` over an `int32` -- is TYPE-007 now where
  the emitter died: bring the constant into scope (`use m.{K};`) and write it bare. `src/` cannot spell a list until a
  snapshot that parses one (D-205).
- **The emitter's rest dispatch is two tables that must agree, its helpers write where their lines stood, and
  `emit_cast_values` never evaluates** (DEF-276, landing 150): `expr_kind_rest_group` maps a kind to its group and
  `emit_expr_kind_rest_1..4` hold the arms; a kind in one table and not the other fails CLOSED (EMIT-002 at the first
  program spelling it), and the harness's `check_rest_groups_agree` holds the two equal -- add an arm as a FUNCTION with a
  row in the map, never inline (about 650 bytes a level per inline arm; group 2, `#sqrt`'s and `??`'s, has some eight arms
  of headroom at the bound). A helper of `emit_coalesce` or of the ternary is called exactly where its lines stood
  (temporaries and labels are numbered in creation order, so a moved call shows only in an emission comparison), READS the
  `IrVal`s it is handed by address and writes none. `emit_cast_values` (28.5 KB) is a leaf: a conversion arm that one day
  evaluates a sub-expression recurses under it in silence. The committed snapshot predates DEF-124 -- re-assigning a `move`
  parameter after moving it is MOVE-001 to the builder -- which is why `emit_coalesce`'s fit stays in its own frame until a
  refresh (D-205's bridging rule). The thin margins left (`?.` 712 KiB, `#ptr_add` 900 KiB at the bound) are DEF-282.
- **A frame on a chain's path holds its recursion's operand and nothing after it** (DEF-282, landing 153; DEF-176's rule at the
  two chains landing 150 left thin): `emit_safe_nav` and `emit_builtin_ptr_add` evaluate the operand the chain nests through
  and hand every later line to a helper called where the lines stood, the values it reads by address -- so the `?.` chain at
  the bound keeps 2,776 KiB where it kept 712, and `#ptr_add`'s 1,284 where it kept 900. A lowering whose recursion comes FIRST
  takes one helper (`emit_cast`'s shape); one that recurses twice takes `emit_coalesce`'s two. `chain_deep_nav.npk` holds both
  chains at the bound but not the splits (the base passes it too): an emitter change on a chain's path is measured by its frame
  (allocas in the compiler's own emission; the prologue's `leaq -N(%rsp)` probe) and the stack's high-water mark (gdb at exit,
  `wt/43c/.internal/stackhw/`), never assumed. The signature question is MEASURED: passing the `ExprNode` by address saves ~2% a
  level (112 bytes per function), and the ~576 bytes a call site is the `IrVal` returned by value -- an out-pointer convention
  is the lever if a chain's margin ever needs more, not the node.
- **A field of a binding this frame owns is that binding's own storage** (DEF-287, landing 154; D-004 rule 3 relaxed with
  evidence): `n.next = @m;`, `arr[i].next = @m;`, `l[i].next = @m;` on same-block locals compile, as the literal always did; a
  holder declared outside the block that declares `m` is BORROW-002 at the statement, `pass n` after the store BORROW-001. The
  store also RECORDS what it stores: a view of a pointer parameter's pointee put into a local's field, an array's or a `List`'s
  element or a by-value parameter's field freezes its root now (BORROW-015 on a later write) -- the assignment form froze nothing
  before 154 and read freed memory at both legs. A slice's element is NOT the slice binding's storage (the store lands in the
  array viewed: a LOCAL slice's store is recorded on that array since landing 156, DEF-289's first face; a slice PARAMETER's
  lands in the caller's storage since landing 157, its second face: a store-matrix slot), and a borrow rooted at a `for` BINDING is refused in a field store whatever its block
  (DEF-288, landed at 155: a `for` binding is a block-scoped local to the analysis, so such a store is BORROW-002 by the
  holder-scope question, with a sentence naming the binding). A new place kind an assignment can target owes `field_store_root`
  its answer.
- **A `for` binding is a block-scoped local to the escape and aliasing analyses, and ONE predicate says what a parameter
  is** (DEF-288, DEF-290; landing 155): `symbol_is_param`/`symbol_is_loop_binding` (escape.npk, over `param_decl_index`) --
  a `DeclParamDecl` symbol is a parameter exactly when the function being walked declares it among its parameters, and
  anything else of that kind is a `for` binding (the parser makes a `DeclParamDecl` at two sites only). A new site that asks
  a symbol's kind asks one of the two, never `DeclKind.DeclParamDecl` alone: reading the kind made the binding a parameter
  for fifteen sites and a cycle. The binding is a bit copy of the element's header in a function-lifetime slot
  (`emit_for_indexed`): `string_bytes(s)` of `for (string:s in arr)` views the ELEMENT's bytes, `@s` is the slot; handed up
  BORROW-001, stored into a holder outside the loop BORROW-002 (bare local, field, callee's store alike), a body local a
  callee's store made the binding view is frozen over the body (BORROW-015). Bounded by the LOOP, not the collection: a view
  of a binding over a slice PARAMETER cannot be returned -- write `string_bytes(xs[i])`, which travels up. `holder_scope_root`
  answers the VIOLATING root so a sentence can name what it refuses; a new holder-scope caller asks it, not the bool. A
  self-wired holder (`t.p = @t.a`) is marked, so `pass t` is BORROW-001. STILL OPEN: writing the iterated array under its
  binding reads freed memory (DEF-293), `pass @a` of a pointer parameter's own slot compiles (DEF-292), a parameter's pointer
  carried out through a binding's value loses its provenance (DEF-294) -- do not write those shapes in `src/`.
- **A store through an indirection lands where the indirection points** (DEF-296, DEF-289's first face; landing 156):
  `hp.v = view`, `(<-hp).v = view`, `<-hp = Holder{ v: view }`, `(<-lp)[i].v = view`, `o.inner.v = view` with `inner: @h`,
  `sl[i].v = view` over a local array, and `put(hp, view)` to a callee that stores into its pointer parameter's pointee each
  record the stored view on the root of the storage the indirection names (`record_refs_through_indirection`, escape.npk) --
  so a view of a pointer parameter's pointee stored that way freezes the pointee, and its later write is BORROW-015 (it
  compiled and read freed memory at both legs). The targets are read off the root's chain BY TYPE (an address of the pointee's
  type, a parameter's pointer value, a slice's view of its element's array): a new way to make a pointer point at frame storage
  owes the chain an entry of that shape, or the store is recorded nowhere again. A slice PARAMETER's element is a
  store-matrix slot since landing 157 (DEF-289's second face; "A slice parameter's element is a store-matrix slot, and a slice
  argument is a destination").
- **An atomic's methods lower through a pointer, and `--reach` lists the arms** (DEF-274, D-363's R4; landing 146):
  `p.store(v)` with `atomic<int64>->:p` is D-098's one-level auto-dereference in the emitter too -- `emit_method_call`'s
  atomic intercept peels the pointer and `emit_atomic_method` reads the cell off the pointer's value; a kind-keyed
  intercept added there owes the same peel (the arenas and the condition variable have theirs, the atomic had none: the
  call fell to the ordinary method path and was EMIT-002 where `(<-p).store(v)` lowered). `npkc FILE --reach OUT` writes the
  identities a `failsafe` must name grouped by the arm's spelling -- the list to write arms from; like `--dump-refs`,
  no gate reads it.
- **A generic `comptime func:` is called with its type arguments written, and a comptime body may `move`** (DEF-270,
  DEF-280; landing 147): `comptime(width::<int32>())` binds `width`'s window to `int32` while the evaluator runs the body
  (`fold_call`, type_resolve.npk: the arguments resolved at the call in the caller's scope, the enclosing scope none), so
  `#size_of<T>()`, a local `T:y` and `T[3]` in the body are the argument's; a call with no turbofish binds nothing, and a
  body that names `T` in a type is TYPE-001 there. The evaluator folds `move(x)` as its operand's value since 147 -- the one
  spelling that binds a move-only `T` or a `string` to a local in a comptime body failed the fold before, with nothing but
  TYPE-004 at the `comptime(…)` to say so. A walker that evaluates or resolves INSIDE a generic body under a concrete call
  owes the window a binding (`bind_instance`'s shape); resolving in the declaration's scope alone is DEF-194's family.

- **A slice parameter's element is a store-matrix slot, and a slice argument is a destination** (DEF-289's second face;
  landing 157): a callee's store through its slice parameter's element (`sl[i].v = view`, `sl[i] = Holder{ v: view }`,
  `sl[1i64...2i64][0i64].v = view`, through a local copy `t = sl`, or `sl` handed on) enters its store matrix
  (`fold_flow_into_param` admits a `TY_SLICE` root beside a pointer's) and a write through it (an owning element overwritten,
  `move(sl[i].o)`, `@sl[i]` to a writer) its mutation summary (`record_mut_of_place`); at the call a non-`fixed` slice argument
  that is a place chain -- a range of named storage (`arr[lo...hi]`, a `List`'s range, `(<-ap)[lo...hi]`), a slice local, a
  slice field, the caller's own slice parameter -- is a destination (`dest_place`), so the stored view lands on the storage's
  root (the pointee's later write BORROW-015), and a callee's write under a live view of what a range views is BORROW-015 at
  the argument (`check_written_slice`, alias.npk). A RANGE argument composes with the callee's path AT the range step
  (`escape_slice_rest_path` drops the callee's slice-level steps, so its element index widens to the range): joined whole,
  every later step stands one position too far and `overlap_class` can read two overlapping places as disjoint -- a new
  composition of a callee's path with an argument's asks the same question. A FRAME view stored through a slice the call
  cannot name (`put(t, @ls)`, `put_ptr(@t[0i64], v)`, `put_ptr(@sl[0i64], v)`, `put(b.sl, …)`) is BORROW-009, conservatively
  (a same-block range through a slice local and `wild` storage through a slice are refused too): hand the callee
  `arr[lo...hi]` itself. STILL OPEN, each HIGH: a slice LOCAL handed to a writer under a live view of the array (DEF-297), a
  by-value struct parameter's slice field handed on (DEF-298), a slice field of a pointer parameter's pointee (DEF-299),
  a slice argument that is a call's result (DEF-301) -- do not write those shapes in `src/` (`arr[lo...hi][i].v = view`,
  DEF-300, is recorded since landing 158: "A range of named storage on a store's path is a step over that storage").
- **A range of named storage on a store's path is a step over that storage** (DEF-300; landing 158):
  `field_store_root` (escape.npk) answers `arr`'s symbol for `arr[lo...hi][i].v`, `arr[lo...hi][i] = …`, `l[lo...hi][i].v` and
  `arr[0...4][1...3][0].v` -- a range step whose base is a fixed array, a `List` or another such range continues to that base
  (`view_place` names a range's base; a range is TYPED a slice, so a walk that asks the base's type alone stops at it) -- so
  landing 154's field branch records the stored refs on `arr`'s chain (a pointee's view: BORROW-015 at the pointee's later
  write) and asks rule 3 of a frame borrow (a holder outside the borrowed local's block BORROW-002 at the statement; the same
  block admitted, as `arr[i].next = @x`). A range of a SLICE is an indirection (landing 156's walk), and a range of named storage
  is none (`path_indirection_base_type` skips it): a new walk over a store's place steps asks of a range which of the two it is,
  or the store lands nowhere. STILL OPEN, each HIGH: DEF-298, DEF-299 (both with POINTER twins: a by-value struct parameter's
  pointer field handed on or stored through, a pointee's pointer field stored through) -- do not write those shapes in
  `src/` (DEF-297 and DEF-301 are recorded since landing 159: "A slice's storage is read off its refs, and a path's text is
  LEVELLED").
- **A slice's storage is read off its refs, and a path's text is LEVELLED** (DEF-297, DEF-301, DEF-303; landing 159):
  a write through a slice no range of an array or a `List` names (a slice local, a slice field, a range of either, a call's
  result, a ternary) lands in the storage `escape_slice_targets` (escape.npk) reads off the slice's refs, and the aliasing walk
  asks the view parties THERE (`check_slice_targets`, `check_view_parties_at`), skipping every party the slice binding itself
  holds BY ITS HOLDER (`escape_slice_own_holder`). A match on the party's place and path as well missed the slice's own view
  whenever its chain carries a parameter's value too (`T[]:s = #wild_slice<T>(v.items, v.count); s[i] = x;` in a generic body:
  landing 159's first build refused nitpick-time's core, five sites). A new reader of a slice's storage asks
  `escape_slice_targets`, never the slice binding's own root. A path's text is LEVELLED (`level_path_text`): `arr[lo...hi][i]`
  is `i<lo+i>` and a range of a range one range; `e<lo>:<hi>` and `E` are an element somewhere in an extent, `P` a `pick`
  binding's opaque step (it was `R` until 159). A new producer of a path text goes through `path_join` or `path_rel_compute`,
  which level it; a new reader reads `P`, `e` and `E` (alias.npk's `decode_step`: `Step` kinds 3 and 4) and compares LEVELLED
  (`level_steps` before `overlap_class`), or a range's element stands one position too far and two overlapping places read
  disjoint. STILL OPEN, each HIGH: DEF-298, DEF-299 -- do not write those shapes in `src/` (DEF-302, the pointer-alias twin named
  here, is fixed at landing 160 -- the next gotcha).

- **A write through a pointer is a write to its TARGET** (DEF-302, landing 160; alias.npk). The aliasing walk resolves a NAMED
  pointer rooted at a local to the storage it points at (`pointer_targets_of`: off the root's chain -- a `REF_ADDR` of the
  pointee's type, a `REF_PASS` of the pointer's -- a two-level `bp.p` one hop at a time, fuel-bounded and OVER-APPROXIMATE, so a
  re-pointed pointer carries both targets, the one safe refusal k302a), and `check_access_reported` runs two readers for a WRITE
  or EXCLUSIVE access that is not a `$$`-claim (D-286's same-root rule kept for the claims, `mode != CHECK_CLAIMS`): the forward
  `check_pointer_targets` (the write re-rooted at each target via `ptr_write_base`, the write's steps BELOW the pointer composed
  onto the target's own, against the view parties there and the views through a pointer at the same target) and the reverse
  `check_pointer_view_parties` (a write by the pointee's own name against the views through a pointer at it). A pointer's target
  is its root COMPOSED WITH THE PATH BELOW IT -- `hp.o` targets `h.o`, not the whole of `h` -- so a write to a disjoint field
  reads disjoint; a new reader that forgets the composition reads two disjoint places as conflicting, or re-roots at the wrong
  level. A pointer rooted at a PARAMETER is DEF-275's store-matrix question (unchanged); a pointer with NO name (a call's result)
  or composed with a slice is DEF-305 (fixed at landing 161, below). A `Self->` pointer-value method receiver routes through `check_written`
  (`acc_call`'s `recv_ptr_param`). And `decreases fuel` on landing 159's two recursive slice-type helpers
  (`escape_type_may_hold_run`, `escape_view_carriers`) discharged their stack-depth rows: a new recursive escape or type-graph
  walk states `decreases <fuel>` or its `stack-depth` row stands `open`. STILL OPEN, each HIGH: DEF-298, DEF-299, DEF-304 (a
  plain pointer into a `List` element across a reallocating operation), DEF-306 -- do not write those shapes in `src/`
  (DEF-305, the pointer-with-no-name-or-slice twin named here, is fixed at landing 161 -- the next gotcha).

- **A call-result pointer has no place chain, and a slice through a pointer reads the pointee's chain** (DEF-305,
  landing 161; alias.npk, escape.npk). DEF-302's named-pointer walk finds the pointer an access goes through as the last
  pointer-typed base on its PLACE CHAIN (`ptr_write_base`), and `check_access_reported` keys a write's conflicts on the written
  place's ROOT symbol -- both answer nothing for a pointer that is a CALL'S RESULT (`place_root_symbol` 0, no place chain), so
  `drop wp(raw idp(@h));` and `(raw idp(@h)).set();` reached neither reader. Resolve a rootless pointer-typed write by its RETURN
  PROVENANCE: `escape_pointer_targets` (escape.npk, the twin of `escape_slice_targets`) reads the storage a pointer-valued result
  points at off the refs `collect_view_refs` carries (rule A, D-326 -- the pointer twin of DEF-301), and `check_pointer_targets`'s
  call-result arm (routed from `check_access_reported`'s rootless block and `acc_call`'s pointer-no-place branch, for a WRITE or
  EXCLUSIVE access that is not a `$$`-claim) checks it against the views there. And `escape_slice_targets` STOPS at a pointer -- a
  deref bails, a member recurses into the pointer LOCAL and reads ITS chain (the address of the pointee), not the runs the pointee
  views -- so a slice reached through a pointer (`bp.sl[i].o`, `(<-tp)[i].o`) had its targets read from the wrong chain;
  `escape_slice_targets_through_pointer` follows the first pointer on the base's path to its pointee(s) and reads each pointee's
  chain, and `escape_slice_own_holder` resolves a pointer root to its SINGLE pointee and tests THAT pointee's type, so the slice's
  own view is skipped by its holder (the pointee, not the pointer). A new reader of a pointer's or a slice's targets asks: is the
  root a call's result (resolve by provenance) or a pointer on a slice's path (follow it to the pointee's chain)? `decreases fuel`
  on `pointer_targets_of` discharged its stack-depth row. STILL OPEN, each HIGH: DEF-298, DEF-299 (the store-matrix faces, M3),
  DEF-304 (a plain pointer into a `List` element across a reallocating operation; Q75 with the author), DEF-306 (a lent crossing
  aliased through a pointer) -- do not write those shapes in `src/`.

  (DEF-304, listed still open here, is FIXED at landing 162 -- the next gotcha.)

- **A new aliasing party that must conflict with writes is VIEW-LIKE, not a standalone kind** (DEF-304, landing 162; alias.npk).
  A plain pointer into a `List` element held across an operation that reallocates, shrinks, reassigns, moves or drops the `List`
  (`list_push` past `cap`, `list_reserve`, `list_truncate`, `list_pop`, `list_clear`, an assignment over the `List`, a move out
  of it, a `List` field grown) read the FREED buffer on every compiler to date -- D-286's plain-address rule judged it by
  nothing. Option (a) FREEZE (the author's Q75, ≈19:55 2026-10-24 via `management_3`) freezes the `List` as D-325 freezes a
  view's root. The plan sketched the freeze as a STANDALONE party kind with a blanket `admit_written` of `ACC_WRITE`; the build
  made `PARTY_ELEM_ADDR` VIEW-LIKE and `summary_judged` instead, so it is judged by the mutation-summary path
  (`check_written`->CHECK_VIEWS) exactly as the view party is. More precise, and the reason to prefer it: a view-like party never
  fires on `@l` handed to a READ-ONLY callee (the summary carries no write of `l`), never conflicts with a `$$i`/`$$m` claim where
  a view would not (the claim keeps D-286's same-root rule), and leaves a claim-held hazard to the claim's own party (u304i stays
  `NITPICK-BORROW-013`). `place_is_list_element` recognises the element -- `l[i]` canonicalises to `l.items[…]` (D-314 §2), so the
  party overlaps the element-store path -- and is fuel-bounded with a measure (D-304). The by-scope false positive is ACCEPTED
  (k304b: the pointer dead before the op but still in scope, so the op is refused; narrow later with by-last-use liveness of the
  holder, as DEF-302's k302a). When adding a party that freezes a root against a write, model it on the VIEW party, not a new
  kind. STILL OPEN, each HIGH: DEF-298, DEF-299 (the store-matrix faces, M3),
  DEF-307 (a `List`-element pointer reached other than syntactically -- a call's result, or carried into a callee that
  reallocates through a pointer parameter; DEF-304's residue) -- do not write those shapes in `src/`.

- **A spawn's lend and a view of a pointee conflict with a write through an aliased pointer, the pointer resolved to its target**
  (DEF-306 + DEF-308, landing 163; alias.npk). A spawn LENDS its crossing until the block's join (`NITPICK-BORROW-016`, D-180),
  and a view freezes its root against a write (`NITPICK-BORROW-015`, D-325) -- both keyed on the crossing's or the view's OWN root.
  A write of the same storage reached by a POINTER under another name met neither: a crossing handed by a pointer value or an
  `@place` through a pointer, then reassigned, destroyed or moved by its own name (`drop filler(sp); s = …`), replaced the lent
  arena under its task; and a callee that replaces the whole pointee (`wr(hp)`, `(<-p) = …`), handed a named pointer, freed a
  string viewed by the pointee's own name -- use-after-frees / data races in safe code at both legs, on every compiler to date.
  The fix reuses DEF-302's and DEF-305's pointer resolvers: `ptr_place_written_whole` recognises a whole-pointee write through a
  named pointer, and `check_pointer_targets` routes it (under `CHECK_VIEWS`/`CHECK_LENT`) to `pe = place`, the pointer re-rooted at
  its target and checked against the VIEW and LENT parties there; `lend_through_pointer` (in `lend_spawn_args`) resolves a spawn's
  pointer argument to its target(s) -- `@place` by `ptr_last_base`+`pointer_targets_of`, a named pointer by `pointer_targets_of`, a
  call's result by `escape_pointer_targets` (D-326 rule A) -- and raises a LENT party there. A pointer PARAMETER has no target here
  (its pointee is the caller's, its lend the pointer's). When a check keys on a storage's own root, ask whether a POINTER reaches
  the same storage by another name and resolve it; the lend check needed the same resolution the view check got at landing 160.
  The class was closed via a kept MATRIX (holder × side × write × pointer-path, `.internal/def306_matrix_kept/`), the check exiting
  non-zero on any open cell; two over-refusals are ACCEPTED (c306f/c308f, a re-pointed pointer, flow-insensitive
  `pointer_targets_of`, D-286). STILL OPEN, each HIGH, KNOWN-LIMIT in the matrix by EXACT cell id: DEF-309 (a direct whole-pointee
  write or move through a CALL-RESULT pointer, four cells) and DEF-310 (`destroy` through a pointer FIELD escaping
  `NITPICK-TYPE-091`, one cell) -- they land FIRST next week, before M3; do not write those shapes in `src/`.

- **An escape reader asked from another pass needs the function it reads SET, and a place written at or below a dereference is
  resolved through the pointer below it** (DEF-309 + DEF-310 + DEF-311, landing 164; alias.npk, type_members.npk). Three gaps of
  one class, each measured before fixed. (1) DEF-309: `check_access_reported`'s root==0 arm (DEF-305) keyed on the written PLACE
  being pointer-typed, so a write AT or BELOW the dereference of a call-result pointer -- `(<-(raw idh(@h))) = …`,
  `move(<-(raw idp(@s)))`, `(<-(raw idh(@h))).o = …`, `(<-(raw idarr(@arr)))[0i64].o = …`, `wsw(@(<-(raw idp(@s))))` -- was
  judged by nothing (the place is the POINTEE; the pointer stands one level up): a viewed string and a lent arena freed under their
  holders at both legs. `rootless_deref_base` finds the first explicit dereference's rootless pointer operand on the place's path,
  `path_steps(x, place, stop, out)` stops at it, and `check_pointer_targets`'s call-result arm (rule A, D-326) is handed the
  pointer with the steps below composed onto each target -- a disjoint field stays disjoint; `acc_place` checks such a WRITE or
  EXCLUSIVE place ONCE and then walks its children, handing the dereference to `acc_expr`, so DEF-123's shared-holder refusal
  stays and no site reports twice (but v309g, a ternary of addresses, accepted). (2) DEF-311: rule A's readers know a parameter as
  a parameter only through `symbol_is_param` -> `param_decl_index` -> the escape state's `cur_fn`, which `escape_decl` restores to
  -1 after each function; asked from the ALIASING pass with it stale, a pointer PARAMETER forwarded through a call
  (`ws(raw ids(p))`, `drop replace(raw idp(sp))` under a lend) read as a `for` binding, a kind with no recorded chain -- no ref, so no target,
  silent -- while a local copy of the pointer was refused through its own chain. `alias_decl` sets `x.esc.cur_fn` for the function
  it walks and restores it. WHEN ONE PASS ASKS ANOTHER PASS'S READER, ASK WHAT STATE THAT READER ASSUMES ITS OWN PASS SET; a
  reader that answers "no refs" for a parameter is the first thing to suspect. (3) DEF-310: `destroy_receiver_owned` tested only
  the walk's root and its member steps, never each node's own type, so `pa.p.destroy()` (a pointer field), `l[0i64].destroy()`
  (a `List` of pointers) and their siblings consumed what they did not own (a double free, 107 or silent); now every node on the
  receiver's path is tested -- a pointer, a slice or a handle anywhere on it is `NITPICK-TYPE-091`. The class's matrix
  (`.internal/def306_matrix_kept/matrix306.py`, widened by the `param_call_result` and `two_level_call_result` paths) reads 1,080
  cells, 192 REFUSED, 2 KNOWN-LIMIT, 0 OPEN. STILL OPEN, HIGH, before M3: DEF-312 -- a pointer FIELD of a call-result pointer's
  pointee written through below the dereference (`(<-(raw idph(@ph))).p.o`), the composed steps never re-resolved at the field's
  hop (the two KNOWN-LIMIT cells; `rootless_deref_base`'s comment) -- and DEF-307 (landing 165); do not write those shapes in `src/`.
- **An element-address party is raised wherever a holder's refs carry one and judged by every pointer reader, as a view is** (DEF-307 +
  DEF-314, landing 165; alias.npk). DEF-304 (landing 162) froze a `List` against a by-name operation while a syntactic `@l[i]` was live,
  and two gaps stayed, each measured before fixed (`wt/51a/.internal/probes/t165/`, fifty-seven cells). (1) DEF-307: `party_kind_of`
  raised `PARTY_ELEM_ADDR` for a BARE `@place` alone, so an element address obtained as a call's result (`raw first(@l)`), through a
  `List` pointer (`@(<-lp0)[0i64]`), kept in a struct literal's field or handed to a call beside the `List` froze nothing; now
  `prescan_view_parties` raises it off the holder's recorded refs -- a REF_ADDR of a `List` element; for a POINTER holder a REF_ADDR of a
  whole `List` whose type is not the pointee's (rule A: the result points INTO the `List`); for an AGGREGATE holder the same when the
  holder's TYPE holds a pointer whose pointee is not the `List` (`type_holds_elem_pointer`; the closed direction for `dyn`, `any`, an
  enum with payloads, an unresolved type and fuel out) -- `acc_call` pushes a party of the `List` FOR THE CALL for an element-address
  argument, and `check_access_reported` skips a party at the address-taking that made it. (2) DEF-314: `check_view_parties_at`, the one
  body every pointer-resolved write reader ends in (DEF-302's named route, DEF-305's call-result arm, DEF-309's deref arm, the
  callee-summary route), admitted VIEW and LENT alone, and `check_pointer_view_parties` VIEW alone -- so a `List` assigned, moved,
  grown, reserved, cleared, popped or truncated THROUGH a pointer under a live element address freed the buffer at both legs; both
  admit the kind now, exempting a write through the address's own holder (`own`) and a write strictly below the elements
  (`write_into_elements` at `elem_party_level`: `(<-lp)[1i64].k` moves no buffer). WHEN A PARTY KIND IS VIEW-LIKE, ASK EVERY READER
  THAT ADMITS A VIEW WHETHER IT ADMITS THE NEW KIND, AND EVERY PLACE A VIEW IS RAISED WHETHER THE NEW KIND IS RAISED THERE TOO: landing
  162 did one of each (the same-root loop, `party_kind_of`) and left the pointer readers and `prescan_view_parties`. Accepted (D-286):
  k22 -- a struct holding a pointer TO the `List` beside an unrelated element-type pointer (`PX{ lp: @l, q: @g }`) is refused at the
  growth, because a `ViewRef` records the ADDRESSED place and not the field it lands in (0 tree sites); k6, DEF-304's by-order false
  positive, stands. Registered OPEN, HIGH: DEF-313 (M3), DEF-315, DEF-316, DEF-317 (landing 166), DEF-312 widened (167); do not write
  those shapes in `src/`.
  *Correction 2026-10-29 (landing 166, from management's review of 165, its F3 = DEF-320):* "a pointer whose pointee IS the `List`'s
  own type reaches the header, which never moves" (`type_holds_elem_pointer`'s `pt != pte`) is FALSE for a RECURSIVE element type --
  `struct:Node = { List<Node>:kids; int64:v; }`: a `List<Node>->` into an element's `kids` sits inside the buffer and moves with it.
  Landing 168 (DEF-318 to DEF-323). The digest line stands.
- **A rootless place is asked what its slice views, a carrier is read to the runs its chain holds, the walk fails closed past its fuel, and
  every kind-enumerating walk steps the unwrap operators and the casts** (DEF-315 + DEF-316 + DEF-317, landing 166; alias.npk, escape.npk).
  Three rootless shapes compiled and read freed memory at both legs on landing 165, each measured before fixed
  (`wt/51a/.internal/probes/t166/`, 127 cells; `MEASURED_165.md`, `MEASURED_166.md`). (1) DEF-315: `acc_call`'s receiver branch asked two
  shapes -- a pointer-typed place chain, a bare call result -- and a `Self->` method on the BARE DEREFERENCE of a call-result pointer,
  `(<-(raw ids(@h))).sset()`, was neither, so the method's summary write met nothing; the implicit `@recv` is a write at the dereference of a
  call-result pointer, DEF-309's shape, and takes DEF-309's road (`check_written` -> the root==0 block -> `rootless_deref_base` ->
  `check_pointer_targets`' call-result arm: a parameter root by DEF-311, two pointees, the LENT parties with the views). A member or an index
  BELOW the dereference was `acc_place`'s whole-place check already. (2) DEF-316: a write THROUGH A SLICE below such a dereference --
  `(<-(raw idsl(@t)))[0i64].o`, `(<-(raw idbs(@bs))).sl[0i64].o`, the element moved out, its address handed to a writer, a method on it -- was
  re-rooted at the POINTEE and never asked what the slice there views; the named path's slice dispatch (`escape_written_slice_base` ->
  `check_slice_targets`, DEF-297) runs in the root==0 block too, and the slice's OWN holder -- the view a write through the slice frees nothing
  of -- is read from the pointee or the carrier (`escape_slice_own_holder_rootless`: a SINGLE pointee or root, each pointer-typed ref resolved
  first, a slice or a one-view struct; two pointees or two roots skip none). (3) DEF-317: a call-result slice handed out of a struct CARRIER
  (`wsl(raw getsl(@b))`) borrows the carrier whole, a ref `escape_ref_is_run_of` counts as a run (a struct MAY hold one) at the carrier's own
  root, where no view of the array is rooted; now `escape_carrier_runs` follows a ref that is not a run ITSELF (`escape_ref_is_strict_run`), or
  is a run reached THROUGH a pointer, into its chain -- the carrier `b`, the pointer `bp`, the field `pb.bp`, the pointer-field links
  `c3 -> c2 -> c1 -> b`, the slice field copied by name `H2[]:s2 = bp.sl` (DEF-305 (b)'s through-pointer reader looked inward from the slice
  BASE only, and the refs loop's target was rooted at the pointer) -- and `escape_slice_targets` answers whether EVERY chain was read. THE FUEL
  FAILS CLOSED: three pointer links resolve; a chain still to follow at fuel 0 is UNRESOLVED and the write is refused, `NITPICK-BORROW-009` at
  the slice (`escape_unresolved_slice` where the escape pass records the write, the node remembered in `x.unresolved`; `check_slice_targets`
  adds no second line for a node the escape pass reported and reports itself where it did not) -- never read as "no targets". When you add a
  reader whose walk is fuel-bounded, decide what fuel-out MEANS before the first cell: here the refusal of a safe shape four links deep (d4n,
  0 tree sites) is the price of never admitting a deeper one unread. Parity with the NAMED road is the standard for the no-view twins: a
  pointer with TWO pointees (assigned two carriers in branches, re-pointed before the call) and a COPY of a carrier's slice are refused with no
  other view live because `wsl(bp.sl)` and `H2[]:s2 = b.sl; s2[0i64].o = …` are (the two-pointee, slice-copy and re-pointed classes of
  D-286's list), and a chain of two or more links refuses alike on both roads (DEF-325, LOW: the own-holder readers resolve only a
  pointer-ROOT link); the single-carrier no-view shapes (`wsl(raw getsl(@b))`, `getsl(bp)`, `getsl(pb.bp)`) stay clean. THE WALKS
  ENUMERATE NODE KINDS, AND A KIND LEFT OUT IS A VERDICT: the first build's own-holder walk stopped at the `raw` node over the call and FIVE
  controls refused against their own carrier's view (c10, c11, c12, c17n, c18n -- the fix of 04:12 steps `raw`, `relay`, `await` and the
  casts); with those named it still stopped at `?!`, whose node is `ExprEmphaticUnwrapExpr` (the value in slot a, the trap code in slot b),
  NOT `ExprRelayExpr` (the `relay` keyword's node), and `??!` is `ExprEmphaticCoalesceExpr` (D-360) -- so t1n/t3n refused against the carrier's
  own view and t1/t3 refused for the WRONG reason (that view, not `vw`'s); found by a diagnostic build that gave each `pass 0i32` of the two
  own-holder readers its own negative code and printed it through `alias_err` (an escape reader asked from the aliasing pass reports nothing
  itself: `escape_err` is silent when `!x.reporting`). Both erred CLOSED (a self-refusal, no hazard admitted); DEF-324's recorder, the same
  fault class, errs OPEN (`collect_view_refs` bails at a dereference reached as a value, so a getter `pass (<-q).sl` records no ref and the
  caller is silent) -- landing 167 makes every view-carrying walker fail closed on a kind it does not resolve and carries a mechanical table of
  expression kinds against each walker. Until then: before a kind-enumerating walk ships, write one cell per transparent kind on each road it
  can be spelled on (`raw f()`, `f() ?! E`, `g() ??! E`, `relay f()`, `await f()`, `=>`, `=>!`), a hazard that must refuse and its own-view
  control that must be clean (`cells4/`), and read the refusal's SENTENCE (which view it names), not only its code (`cells4_reason.sh`).
  Siblings measured REFUSED ALREADY (a04's ternary, b06's slice value, b08/b17's ranges, c05) narrow a registered gap and are recorded as such,
  not fixed twice.
- **A place reader's default answer is a verdict, and landing 167 made the dereference and the cast steps of every reader** (DEF-324, DEF-326,
  DEF-328 to DEF-332, DEF-338, DEF-341 to DEF-343, landing 167; escape.npk, alias.npk, type_expr.npk). Every kind-naming walker of the two
  analyses was asked, mechanically, what it does with each of the 49 expression kinds (`meta/roadmap/1.6/tools/kindtable167.py WORKTREE` ->
  `kindtable167.md`: `arm`, `shape` fallback, or `-` = the body's last `pass` decides), every accessor call was checked against the kinds that own
  its slot (`accessor_census167.py`: 373 ok, 2 MISMATCH = DEF-337's then-branch fault), and every default read by hand
  (`wt/51a/.internal/probes/t167/READING_167.md`); where a default admitted a hazard a cell was written and run on 165's and 166's binaries FIRST
  (`cells_b/`, `faces/`, `cells_c/` to `cells_g/`; `MATRIX_167.txt`). WHAT THE READING FOUND, by mechanism: (1) `collect_view_refs` answered NIL
  at a dereference read AS A VALUE, so a getter `pass (<-q).sl` recorded no ref and its caller's write was silent at every depth (DEF-324), a
  pointer copied through a dereference had no chain (its one-hop face), and -- the same line -- a view stored, handed on or moved through a
  dereference in a callee entered no store matrix and no rule B (DEF-331); the arm reads the pointee's chain now, one hop up when the
  dereference's own type is a pointer, and `arg_place_chain` steps the dereference. (2) `escape_expr`'s dereference arm asked for a pointer TYPE
  where `type_holds_pointer` is the question: `pass (<-hp).s;` escaped its frame (DEF-326). (3) The dereference road's slice reader read ONE level
  of the pointee's chain and fed no `resolved` (DEF-328; landing 166's "resolved through the carrier's chain" held on the named road only). (4) The
  own-holder readers keyed the one-view test on the FIELD's slice type, so `raw idsl(@b.sl)` over a struct holding a second view made `b` its own
  holder (DEF-329: the HOLDER's type, `escape_target_root_type`; and `@b.sl`, a slice-typed strict run, is a VIEW followed into `b`'s chain). (5) A
  getter `pass @q.sl;` is summarised as passing its parameter through -- its result a REF_ADDR of the struct with no path, a pointee type matching
  nothing -- so the call-result pointer had zero targets (DEF-330: a place whose type HOLDS the pointee type inside is a target WHOLE,
  `escape_type_holds`, fuel 8). (6) `escape_pointer_targets` typed a pointee by the CAST, so `(ap =>! H2->)` over an `any->` matched no ref
  (DEF-332: the cast is peeled, the pointee read off the OPERAND, an `any` pointee makes every ref a target, `cast_changes_pointee` drops the steps
  below a cross-type cast); the plain-to-plain retype `hp =>! S->` itself is `NITPICK-TYPE-032` by D-365 -- the author's ruling of 2026-10-29: safe
  code retypes a pointer only to its own type, to or from `any->`, or across the wild boundary. (7) The call's view party was raised for a bare
  view-maker alone (DEF-338: a ternary's arms and a cast's operand now). (8) Inside a callee `esc_place_root` stopped at a cast of the pointer
  parameter, so `(<-(q =>! H2->)).o = …` entered no summary (DEF-341: one line), a ref carried as an `any->` VALUE was no target of an `H2->` in
  THREE readers (`escape_any_pointer_link`, one predicate for the three), and a LOCAL COPY of the pointer parameter written through was recorded
  on no chain at all -- `record_mut_of_place` roots at the local and the store road's walk ran only with refs (DEF-342: `record_mut_through_indirection`,
  the whole extent, so a disjoint field written through the copy under a view of another field is an accepted over-refusal). (9) The ALIAS pass's
  place readers -- `path_steps`, `is_place_chain`, `place_root_symbol`, `root_ident_is_pointer`, `pointer_targets_of`, `party_kind_of`,
  `prescan_party`, `admit_value`, `admit_written`, `acc_call`'s element-address arm -- each stopped at a cast, so a view through `(<-(q =>! H2->)).o`
  had unreadable steps and BOTH judges skipped the party (`if (!party_steps(…)) continue;`, fail-OPEN), the reverse reader resolved a view through a
  local pointer's cast dereference to nothing, and `(@h) =>! any->` raised no plain party beside an exclusive claim (DEF-343; the hazard g6 read
  the freed bytes). THE RULES THAT CAME OUT OF IT. A reader that enumerates node kinds answers for EVERY kind -- its default `pass` is a verdict,
  and the kind table names which readers rely on theirs; read the table's new row before you add a reader or a kind, and `grep` the kind's twin
  spelling (`<-` beside the auto-dereference, `=>` beside `=>!`, a ternary beside a bare name). A judge FAILS CLOSED: a party whose steps cannot be
  read stands for its root WHOLE (`drop list_truncate(@pp, 0i64)` in place of `continue`, the three judges) [CORRECTED 2026-10-29, build 15: TWO judges -- `check_view_parties_at` and the same-root loop; `check_pointer_view_parties` truncates and then `continue`s on `pre.count > pp.count`, and `check_pointer_targets` and the root==0 block skip an unreadable step -- DEF-353, landing 168], and `check_access_reported` refuses a
  WRITE whose steps `path_steps` cannot spell (BORROW-009 -- the hardening row, with no legal control: it fires only where a walk has no arm). A fix
  to one pass is read against the OTHER: from build 8 on the escape side recorded the view through the cast dereference correctly and
  `npkc FILE --dump-refs OUT` showed the SAME refs for the silent cell and its refused plain twin -- the gap was the alias judges', and printing
  the refs found it faster than reading more code. Five builds in a row (9 to 13) each closed a face and exposed the next, each measured against
  the WHOLE matrix before the next: build 11 alone REGRESSED three cells to 113 (`rootless_deref_base` answered none for a root named through a
  cast while `is_place_chain` still said "no chain", and `acc_place`'s non-chain branch decomposed the write) -- a regression the full matrix
  caught and a partial run would not have; and a line that no hazard cell distinguishes is kept only when a CONTROL does (build 9's
  `root_ident_is_pointer` line: without it b12 refuses at the wrong site, so parity with the named road's b13 is the reason it stays). Parity with
  the named road is the standard for every no-view twin, as in 166: b11n, b12, f4g, f5f and DEF-342's whole extent are refused because their
  by-name or no-cast twins are. Before a kind-enumerating walk ships, write one cell per transparent kind on each road it can be spelled on --
  `raw`, `?!`, `??!`, `relay`, `await`, `=>`, `=>!`, `<-`, a ternary -- a hazard that must refuse and its own-view control that must be clean, and
  read the refusal's SENTENCE (which view it names), not only its code: f6a refused for the wrong view on build 2 and would have passed a
  code-only check. And the landing's own SWEEP is a measurement: build 13's found a fifteenth face — a ternary's arms' view parties,
  raised at the ARM's site, self-conflicted with the argument's own write on the rootless road (`check_slice_targets` ->
  `check_view_parties_at` skipped the access's own parties by holder alone), a NEW report on an existing rejection file
  (`slice_targets.npk`'s s14), fixed as build 14 by carrying the whole argument's site into both judges (`self_site`): a party raised
  for a nested expression carries the ARGUMENT's site, and every judge a rootless road reaches skips the access's own by it.
- **The two analyses each spell a PATH, and a step rule lands in both -- and a cast's TARGET TYPE is an axis of the summary road**
  (DEF-346, landing 167 build 15; escape.npk `path_rel_compute`, alias.npk `path_steps`). Build 14 taught alias.npk's `path_steps` to drop
  the steps below a pointee-changing cast (DEF-343) and left escape.npk's `path_rel_compute`, which spells a callee's SUMMARY path, keeping
  them: `(<-(q =>! S->)).x` under an `any->` parameter was summarised as `f<x>`, the caller composed `x` onto its `H2` and read the write as
  disjoint from a view of `h.o` -- and because landing 166 had refused the inline `wrs((@h) =>! any->)` WHOLE at the plain address, the
  narrower summary turned a refusal into a silence: the subcycle's first regression, caught by management's diff review and not by the
  401-cell matrix, whose callee cells all cast to the SAME struct type (f9j2, f9j4, f9j6: an `any` laundering back to `H2`, where the
  field names agree). When a reader learns a step rule, grep its twin in the other pass (`path_steps` and `path_rel_compute`;
  `collect_view_refs` and the party readers) and write the rule in both; and when a cell varies a cast, vary the cast's TARGET TYPE too --
  a summary is read by every caller under the caller's type, so the target type is an axis of the summary road that the place readers'
  matrix never had. Where the escape pass's CAST test (`escape_cast_changes_pointee`) meets an untyped cast layer it answers TRUE
  (the pointee whole), the closed direction: a summary that keeps steps it cannot vouch for admits a write (alias's
  `cast_changes_pointee` answers false for its own, non-summary reading; build 16's `casts_change_pointee`, the caller rule's test,
  and `escape_place_crosses_cast` answer TRUE for an untyped layer too). Narrowed in build 16 from "meets an untyped node on a
  summary path": the review of build 15 read `escape_type_holds` answering NO for an unresolved type and for a non-struct holder
  (DEF-353's open direction, landing 168), so the TRUE is the cast test's alone.
  BUILD 16 (the review of build 15): a summary has SEVERAL WRITERS, and a caller that consults it narrows what the plain-address
  party judged whole. Build 15 fixed `path_rel_compute` alone; the review of build 15 found the second-callee road
  (`record_mut_of_call` joined `wsx`'s `f<x>` back onto the truncated path), the value road (`dest_place` answered none for `q =>!
  S->`, so the write entered no summary) and the cast inside a getter's argument -- four INLINE cells REFUSED on 166 still silent on
  build 15 (r1f, r1h, r2b, r3b: 170 at both legs), and one over-refusal (r4, the D-021 round trip writing a disjoint field) the
  record had not listed. Build 16 does both: every writer of the summary truncates at a pointee-changing cast
  (`escape_place_crosses_cast`) [CORRECTED 2026-10-29, after the commit, from management's delta review of build 16 (its A): the MUTATION writers truncate -- `path_rel_compute`, `record_mut_of_call`, `record_mut_of_place`, `dest_place`, and `ref_full_path` for an ADDRESS argument -- and the VIEW and STORE writers do NOT when the cast pointer is handed on as a VALUE: `arg_place_chain` peels the cast, so `arg_refs_view`'s ref is the bare `q` with the callee's path, and `ref_full_path`'s `escape_place_crosses_cast(q)` is false. `getx3(any->:q) = fixed uint8[]` passing `raw gety(q =>! S->)`, `gety` viewing `p.x`, returns or stores a view summarised as `x` OF THE `any->` ARGUMENT, and `fixed uint8[]:vw = raw getx3((@h) =>! any->); h.o = …;` is SILENT -- inline and named, returned and stored -- the view reading the freed bytes (170) at both legs on landing 166 and on 167 alike (DEF-356, HIGH, landing 169; `wt/51a/.internal/probes/t167/rev167c/` pa_inline, pa_named, pa_store_inline, pa_store_inline_local; pa_store_named refused on 167 only, a gain). Not a regression, and not covered], and the CALLER judges a plain address under such a cast WHOLE without consulting the summary
  (`admit_written` -> `check_access_ex`, `CHECK_VIEWS`; `check_pointer_targets` composes no steps below an `any` pointee or a
  pointee-changing cast), as landing 166 did -- so the inline spelling no longer depends on any summary being right, and the named
  spelling depends on summaries that are true on every road. [CORRECTED 2026-10-29, the same review (its B and D): the named spelling is NOT covered where the pointer local is itself MADE by a pointee-changing cast -- `any->:ap = (@h) =>! any->; S->:sp = ap =>! S->;` (or `S->:sp = ((@h) =>! any->) =>! S->;`), then `wsx(sp)`, `sp.x = …` or `drop sp.sset()` under a view of `h.o`: `sp`'s chain holds `@h`, whose type `H2` is neither `S` nor `any`, so `ptr_ref_is_target` makes it no target, and `erase` reads the casts on the WRITTEN place and an `any` pointee, never the cast in the pointer's PROVENANCE -- SILENT, 170 at both legs on 166 and on 167 (DEF-357, HIGH, landing 169; the inline twin `wsx(((@h) =>! any->) =>! S->)` is refused on both, by 166's whole judgment and by 167's caller rule). The caller rule's legal half holds -- `wrs((@h) =>! any->)` with no view live, or with a view of ANOTHER struct, is clean, 0/0 (pe_inline_noview, pe_inline_otherview) -- and it has a SEVENTH accepted over-refusal, 166's own judgment kept: a READ-ONLY callee handed `(@h) =>! any->` under a view of `h.o` (`raw getx(…)`, a second view of the same bytes) is refused, `admit_written` never asking whether the callee writes (D-286's list; 0 tree sites); `(@buf) =>! wild int8->` handed to a reader under a slice of `buf` is refused on both too, at the plain address `@buf` (D-286's write-capable admission). The two sentences before this bracket are overstated as written; this bracket is their correction, F112 the library seat's (NOTICES)] THE RULES: when a fix moves a judgment from the caller to a callee's
  summary, keep the caller's whole judgment wherever a type boundary makes the summary's steps meaningless; before calling a summary
  road fixed, enumerate every writer of the summary (`grep -n 'mu_add\|path_join\|record_mut_of_\|dest_place\|ref_full_path'
  escape.npk`) and probe each with the SAME cell in its named and its inline spelling -- a fix that closes the named spelling and
  re-opens the inline one is a regression the matrix's cells, all named, cannot see; and an over-refusal a build introduces is
  written down in the build that introduces it (D-286's list), not found by the next review. The cost of the WHOLE readings (the
  summary's truncation at the cast, the caller's `any`-pointee erase): the round trip `any-> =>! H2->` writing a field disjoint from
  the view is refused (r4, the named `wk(ap)`, the sixth accepted over-refusal, 0 tree sites).
- **A pick EXPRESSION's arm is a statement list to every prescan, a `give` is collected from every statement kind of the arm, a spawned
  METHOD is a spawn with its receiver as the first argument, and a by-value owner crosses a spawn only by `move`** (DEF-327, DEF-337, DEF-339,
  DEF-340, landing 168; alias.npk `prescan_stmt`/`prescan_expr_picks`/`prescan_pick_window`, escape.npk `collect_gives_refs`/`collect_gives_roots`,
  `callee_is_async`, `escape_spawn_recv`, `spawn_param_is_move`, `escape_spawn_byval`, `escape_spawn_args`). Four readers each missed a SHAPE
  of a construct they already handled: the aliasing prescan walked statements and never entered an expression, so a holder declared in a pick
  EXPRESSION's arm had no party and a write of its root in the arm was unjudged (f3a: the arm's `give` read the freed bytes, 113 at both legs);
  the escape pass's give collectors entered a block, an `if` by the WRONG accessor (`stmt_body`, a `while`'s slot, where an `if` keeps its
  `then` in `stmt_then`) and a `pick`, and no loop at all; the spawn readers asked for an `ExprCallExpr` before `callee_is_async`, which read an
  identifier callee only, so `drop w.fill(@s)` -- and `drop tasks.fillq(@s)`, the module-qualified spelling -- was no spawn to either pass; and
  `escape_spawn_args` tested pointer arguments against D-180's five kinds and let a by-value OWNER through, whose header the child frame copies
  while the owner's binding stays live (114 at both legs: the task read the re-allocated block). THE LESSONS. (1) A walker that dispatches on
  `expr_shape` is not done at the shapes: `ExprCallExpr`, `ExprBuiltinExpr`, `ExprStructLiteralExpr` and `ExprPickExpr` are CUSTOM, and a
  `pass NIL` on CUSTOM is a hole -- the first build of this landing bailed there, and a pick in a CALL ARGUMENT (p4) and in a STRUCT LITERAL's
  field (p8) stayed silent 113/113 under it; write the four arms (a call's and a builtin's VALUE arguments stand after their generic ones, a
  struct literal's window alternates NAME and VALUE) as `acc_expr` has them, and make CUSTOM and NONE an INTERNAL refusal, as `acc_expr` and
  `escape_expr` do. (2) When a reader enumerates STATEMENT kinds, compare its list with `escape_stmt`'s and `prescan_stmt`'s: a `give` can stand
  under `if` (both branches), `while`, `for`, `loop`, `till`, a `when`'s body, `then` and `end`, a block and a pick arm; `defer` is TYPE-094.
  A `when`'s `then` and `end` contribute nothing to the flow analysis's `must` (D-027), so a test arm that gives inside a `when` ends in a
  trailing `give` or it is NITPICK-FLOW-001. AND A KIND IS NOT ITS SLOTS: the third build of this landing found `prescan_stmt` naming every
  statement kind and walking only the BODY slots of six of them -- a pick in a `for` iterable, a counted argument, a `when` condition, a
  pick's selector, an arm's `where` guard or an assignment's TARGET kept its arm's holders unregistered (q1-q8, 113/113) -- and the escape
  pass's `for` arm walking the iterable only under `if (for_binding_can_carry(…))`, a type question, so a pick in a range bound was never
  entered at all (q1 silent on 166 and 167 alike). The mechanical check is the SLOT TABLE of `meta/roadmap/1.6/tools/kindtable167.py`: per
  statement walker and kind, each expression accessor's value `walked`, `walked under if (…)`, or `READ` only -- run it on the tree before
  and after, and read every `under` and every `READ` of an expression slot by hand (a body slot is READ by design: it is walked as a
  statement). (3) Read a callee off the CALL GRAPH (`call_callee_fn`) before the callee expression's kind: the
  graph names a method, a qualified spelling and an identifier alike, and the identifier read is the fallback for a call the graph has no edge
  for. (4) The spawn rule's test is the PARAMETER's mode: a by-value owner crosses only into a `move` parameter (`drop takestr(move(s))`), a
  `Channel` by value is D-181's crossing, plain data copies harmlessly; whether or not the owner writes, the copy is refused (direction (A),
  management 2026-10-29 08:25:55; the permissive lend (B) the author's to grant). (5) A method's receiver is a crossing: a pointer receiver is
  BORROW-004 for every plain struct (no sanctioned kind takes a user method, TYPE-007), so DEF-149's legal spelling -- a NIL method on a pointer
  receiver spawned with `drop` -- stays legal at the type level and is BORROW-004 under D-180's crossing rule, as the free-function spelling
  already is (`async_method_bare.npk`'s control `tick` is a by-value inherent method now; a by-value `Self` in a TRAIT makes it TYPE-015 under
  `dyn`). (6) A spawn's site reports TWICE where two rules meet it at two lines (the receiver's crossing at the spawn, the lend at the destroy):
  D-332's one-report-per-site holds per LINE, and the header generator lists both. The cells: `wt/51a/.internal/probes/t168/`, `cells168/`, `cells168m/` and `cells168q/` (the
  spawn cells re-cut without the `dyn` trio, whose spawn inside `spawn_dyn` had contaminated every cell's verdict -- a shared preamble's own
  refusal is read as the cell's: keep a refusing helper in the one cell that needs it).
  *Dated note 2026-10-29 (after the commit; management's diff review of 1143343 (Opus, `credits/out/compiler-168-diff-review-2026-10-29.md`,
  in at 20:54)):* (4)'s rule misses a by-value `self` reached THROUGH A POINTER (`drop p.looks()`, `drop (@w).looks()`: `rxt` is the
  pointer's type -- DEF-360) and a generic `T` (no `TY_PARAM` case -- DEF-361); (3)'s call-graph read and (5)'s receiver rule miss the
  TRAIT-QUALIFIED spelling `drop Tr.m(w, n)` (DEF-362: the trait's name is read as the receiver and every argument shifts to the next
  parameter's mode -- the owner admitted, a legal `move` argument refused; a type-less method-call operand is a qualifier, the fix
  measured); and the escape pass never enters a pick expression standing as the receiver or inside a sanctioned pointer argument (DEF-359).
  The remedy "`.clone()` first" does not work alone -- a clone is a temporary owner, refused by (4) itself; `.clone()` INTO a `move`
  parameter. Landing 169 opens on the four. Twin-spelling rule: a reader written for `recv.m(args)` is owed to `Tr.m(recv, args)`.
- **A spawn's receiver and its sanctioned arguments are WALKED before their crossing is judged, a by-value receiver reached through a
  pointer crosses a spawn AS THAT POINTER, a generic `T` is an owner at a spawn, and a name before the dot with no type is a QUALIFIER** (DEF-359,
  DEF-360, DEF-361, DEF-362, landing 169; escape.npk `escape_spawn_recv`, `escape_spawn_args`, `escape_spawn_byval`, the spawn arm of
  `escape_expr`; DEF-363 registered). Four readers of landing 168's spawn rule each missed a face management's diff review found the same
  evening: `escape_spawn_recv` JUDGED the receiver's crossing and never WALKED it, and `escape_spawn_args` skipped `escape_expr` on its
  sanctioned `continue`, so a pick expression standing in either place declared views nobody recorded (113/114 at both legs); the receiver
  reader handed `rxt`, the POINTER's type, to the by-value rule, where a pointer drops nothing; `type_drops` has no `TY_PARAM` case, so a `T`
  at `string` crossed by value; and the trait-qualified `drop Tr.m(w, n)` put the trait's NAME where a receiver stands, shifting every
  argument to the next parameter's mode (the owner admitted, a legal `move` argument refused -- the one over-refusal 168 introduced). THE
  LESSONS. (1) A reader that CONSUMES an expression -- judges its crossing, reads its type -- without recursing into it is a hole for every
  construct that declares something inside an expression (a pick expression's arm, DEF-327's class): grep a walker for `continue` and `pass`
  that skip `escape_expr`/`acc_expr`, and `discard(raw escape_expr(…))` first (not `drop …`: TYPE-042, a value discarded). (2) THE EMITTER IS A
  PARTY TO THE ANALYSIS'S MODEL. The scratch fix peeled `rxt` to the pointee because D-098 says a by-value `self` through a pointer IS the
  pointee -- and the child-frame builder (`emit_child_frame`, shared by `await` and both spawns) stores the POINTER's words there: `drop
  pp.lookp()` and `await pp.lookp()` with NOTHING written read an address as `self.k`, 114 at both legs on 168 and 169 (r8, r10b, r16;
  DEF-134's load reached `emit_method_call` alone). A rule about what CROSSES is checked against what the emitter STORES in the frame --
  read the frame builder before writing a crossing rule -- and until the emitter is fixed (DEF-363, landing 170) the spelling is refused
  whatever the pointee, the closed direction. (3) Ask every generic predicate used as an EXEMPTION what it answers for a `T`: `type_drops`
  answers "no" by having no case, and "no" exempts. The closed reading is "an owner"; the bound (`T: Copy`) is the relaxation, and it
  needs the typer's BoundTable, which the escape pass does not hold -- say so in the row as a KNOWN over-refusal, not on D-286's list.
  (4) A method-call node's operand is not always a receiver: the trait- and type-qualified spellings (`Tr.m(w, n)`, `Tp.mp(@w)`) [CORRECTED 2026-10-30, the review of 321f31f (F3): BOTH are TRAIT-qualified -- `Tp` is a trait, and the typer has no type-qualified call (`type_method_call` takes a module, a trait or an enum constructor; `PD.m(x)` is NITPICK-TYPE-007), so the qualifier rule is reached by trait names only] carry a
  NAME there with no type (`exprtypes_get == 0`; the module-qualified one carries `ns_decl`), and a reader that takes the operand as
  parameter 0 shifts every argument by one -- the twin-spelling rule of the 168 gotcha, now a test (`spawn_review_faces.npk` sites 10-13).
  (5) A CELL THAT SWALLOWS THE FAILURE IT TESTS FOR PROVES NOTHING: r10 and r11 wrote `await pp.lookp() ?| NIL` and read 0/0 -- the task's
  `fail E1` was discarded -- and the legal half's first control 10 did the same while its comment said "right"; a hazard cell routes the
  wrong value to an exit (`?! E`, a channel, a compared result), and its control OBSERVES the right value, not the owner's write. (6) A face
  only the EMITTER reaches is censused through the CHECKER: a scratch-only refusal in a scratch worktree (`CENSUS363` in `escape_expr`'s
  await arm, wt/51n, never landed) swept over the corpus with `sweep.sh` and a parallel `census_one.sh` over the fuzz seat's full checkout --
  the command and the count in the record. The cells: `wt/51a/.internal/probes/t169/`, `cells_r/`, `cells_r2/`, `cells_r3/`, `cells_r3b/`, the review's
  `t168/rev168/cells_all4/`; the matrix `t169/MATRIX_169b.txt`.
  (6) [ADDED 2026-10-30, the review of 321f31f] A WALKED receiver's VERDICT is a judgment too: `escape_spawn_recv` runs `escape_expr` on the receiver (DEF-359)
  and drops the answer, where `escape_spawn_args` refuses an argument whose walk answers true ("a borrow cannot cross a spawn") -- so a by-value
  `self` HOLDING a borrow (`H{ p: @d }`) crosses where `drop rdf(h)` is refused, 114/114 at both legs (DEF-364, OPEN HIGH, landing 170); and a
  kind test on a TYPE PARAMETER must be recursive: `type_drops_recorded` has no `TY_PARAM`, `TY_SELF` or `TY_ASSOC` case, so `T?`, `T[n]`,
  `Result<T>` and a generic struct holding a `T` answer "owns nothing" and DEF-361's top-level test lets them cross (DEF-365, OPEN HIGH, landing
  170). Both probed silent on 168 and 169 alike, named and inline, 0 tree and library sites: `wt/51a/.internal/probes/t169/rev169/`.
- **A move slot asks where its value COMES FROM, not whether it has an address; a `move` receiver is a move slot; and a by-value
  receiver reached through a pointer is the POINTEE at an await and a spawn** (DEF-366, DEF-367, DEF-368, DEF-363, DEF-360 relaxed;
  landing 170; type_expr.npk `move_slot_source`, `require_move_if_owning`, `require_move_receiver`; type_members.npk `type_method_call`;
  type_stmt.npk `check_pass`; ir_expr.npk `emit_child_frame`; escape.npk `escape_spawn_recv`). TYPE-046 guarded the slots that take
  ownership with `expr_is_place` -- the ADDRESS question -- so an operator's result (`is (c) : a : b`, `a ?? d`, `r ?! E`, `relay r`, a
  cast) and a part of a temporary (`(raw mk()).s`) were "temporaries" and passed, each a second owner (95/95 at both legs on 168 and
  169); `type_method_call` handed the arguments to `check_args` with skip 1, so a `move Self:self` receiver was read by nobody at a call,
  an await, a spawn or UFCS; and `emit_child_frame` stored a pointer receiver's ADDRESS where a by-value `self` was expected. THE
  LESSONS. (1) THE ADDRESS QUESTION IS NOT THE OWNERSHIP QUESTION: a copy needs no address. A rule that guards a TRANSFER asks where the
  value comes from -- the root of the path and every operand by its own recorded type -- and answers "a copy" on exhaustion
  (`move_slot_source`: 0 fresh or plain data, 1 a place, 2 an operator over one, 3 a part of a temporary, 4 a part through `?.`).
  (2) A SLOT A HELPER SKIPS IS A SLOT NOBODY CHECKS: grep every `skip`/start offset handed to an argument walker and name who checks
  what it skips (`check_args` skip 1 left parameter 0's `move` unread since `move` parameters existed). (3) `exit` HIDES A DOUBLE FREE:
  the nine `Bridge:b = br ?! E9;` tests and the cells m1 and m1c read clean with two owners because `main` ends in `exit`, which runs no
  drops (D-183) -- the new rule's sweep found the nine, and the cells were re-cut in a function that returns (m1b, m1e: 95/95). (4) A
  RELAXATION IS RE-MEASURED AGAINST EVERY CELL THE CLOSED RULE REFUSED: build 7 relaxed 169's blanket pointer-receiver rule to the
  pointee's OWNERSHIP and re-opened an address-holding pointee (`H{ p: @d }` behind the pointer: x1, x2 114/114 at both legs, refused on
  169); the four-column table of the old refusals caught it, and the pointee's type is asked whether it holds an address too. (5) AN
  EMITTER FIX CAN MAKE A DOUBLE FREE REACHABLE: DEF-363's load turns `await pp.take()` -- a `move` receiver through a pointer, a wrong
  value before -- into the pointee's header freed twice, so DEF-366 lands first or with it; before an emission fix that makes code
  "work", ask what it makes reachable. (6) A SWEEP THAT FINDS REAL SITES STOPS THE EDIT: the ten DEF-367 sites (nine tree tests, the
  fuzz program cc0059) were reported before any respelling, and the respellings are the author's or management's to rule. The cells:
  `wt/51a/.internal/probes/t170/` (`pre/` to `pre8b/`, the matrix `gen_matrix170.py` -> `MATRIX_170i.txt`, DEF-368's
  `gen_def368.py` -> `DEF368_170i.txt`).
