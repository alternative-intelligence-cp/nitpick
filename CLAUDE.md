# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Status (short) — the full running record is `meta/STATUS_HISTORY.md`

- **Phase C. Cycle 1.6, the LLVM-native analyzer evidence (D-233), is underway**, in subcycle 1.6.1e: the landing queue of the library
  listener's and the fuzzer's findings, safety before correctness before performance. Cycles 0.0–1.5 are complete
  (`meta/roadmap/done/`); 1.4 made the compiler self-hosting (D-202) and 1.5 made the verification leg (`npkg verify`, the floor's
  specification and models, the schedule explorer).
- **main = landing 171** (2026-10-30; wt/50d, branch 50-def364, on landing 170's b122540) — DEF-371 + DEF-373 + DEF-372 + DEF-374 (nine
  `src/` files, +122/−37 code lines): `return Result{value: v};` is `pass v;` written long and takes `pass`'s road -- `check_leaving_value`
  (type_stmt.npk) and `emit_leaving_value` (ir_stmt.npk), one rule and one lowering for both spellings, so a named owner is MOVED at the
  `return` and every face `pass` refuses is refused with `pass`'s code --, and a `Result` literal's value anywhere else is a move slot,
  TYPE-046 (DEF-371); a `Result` literal writes ONE field, `NITPICK-TYPE-101` (DEF-373: the both form, retired by the author, leaked an
  owner on every caller error road, its value unreadable); a function with a `move` parameter named as a value is `NITPICK-TYPE-100` -- a
  function type carries no `move`, so a call through the value freed twice (DEF-372; a known over-refusal for a plain-data `move`);
  TAINT-001's message names its road (DEF-374) -- two double frees in safe code, 95/95 at both legs on 169 and 170 alike, closed. The twin
  table 83 pairs SAME (DEF-371's mechanical check), the sweep 54 appeared (the landing's own and TYPE-101 in five fuzz claim programs, named
  in F118) and 4 vanished (re-codings), the emission comparison 627 of 628 byte-identical (the one different, the long form's move-out), the
  manifest 7,653 obligations unmoved (120 rows re-keyed, 0 added). DEF-375 to DEF-378 registered; the spawn group built and taken out,
  landing 172's lead with DEF-378. Every landing's full paragraph — the defect, what was measured first, the rule now, the tests, the sweep,
  the emission comparison, the manifest — is at the top of `meta/STATUS_HISTORY.md`, newest first; the cycle history 0.0–1.5 follows it
  there. Read the newest few before planning work.
- **The queue** (management_7's rulings of 2026-10-30 -- 04:29, 06:03, 06:11, 06:47, 06:53, the spawn group out of 171 at 06:59 ((b)), the
  queue confirmed at 07:10, DEF-379 to DEF-384 at 08:39:44 and 09:08:19, DEF-385 and the queue past 172 keyed to the defects at 09:52:11 and 09:56:11 -- and management_6's of 2026-10-29 and 2026-10-30 for what follows): landing 171 LANDED (DEF-371 + DEF-373 +
  DEF-372 + DEF-374, the status bullet), then landing 172 = DEF-364 + DEF-365 + the qualifier hardening + DEF-378 + DEF-383, FIRST (HIGH but DEF-378
  MEDIUM: a by-value RECEIVER holding a borrow crosses a spawn where the argument twin `drop rdf(h)` is -004 -- `escape_spawn_recv` runs the
  receiver's walk and drops its verdict; its POINTER face is closed by 170's address rule (DEF-364); DEF-361 tests the top-level kind alone,
  so a `T` inside `T?`, `T[n]`, `Result<T>` or a generic struct crosses -- `type_drops_recorded` has no TY_PARAM case (DEF-365); an untyped
  method-call operand that is not a trait's name fails CLOSED; and `escape_expr`'s verdicts, which the receiver rule starts reading,
  over-refuse a read through a live view of a value whose type holds no address and take a borrowed POINTER for a pointee holding a borrow
  (DEF-378, both faces) -- with its own matrix, o1-o9 and their argument twins, u1-u6 and the legal half's controls 7, 8 and 28 in both
  spellings, the DEF-326 and DEF-364 hazard cells staying refused, and its own review, 170's owner message kept; build 2's hunks and cells
  in `t171/spawn172/`, the cells `t169/rev169/` and `t171/p364/`; and DEF-383, registered after 171: at a `return` the long form's value gets exactly `pass`'s escape summary -- `escape_return_value` hands `record_summary` the whole literal: a use-after-free 13/13 and two over-refusals, `t171/rev171/esc/`), then the Result-fields group's landing = DEF-380 + DEF-381 + DEF-382 + DEF-385 (registered after 171, HIGH but DEF-380 MEDIUM and DEF-382 LOW: `r.err = E1` over a value leaks it (DEF-380); a zero `Error` read off a success and failed makes a success holding the slot's zero, a NULL plain pointer (DEF-381); `r.is_error = false` reaches EMIT-002 (DEF-382); an exhaustive `pick` over a valued enum's vacant value takes no arm -- a fabricated success, an undefined read (DEF-385) -- its own landing, its emission comparison read alone, right after 172 if the author has answered its two questions by 172's LAND DONE ((a) the identity of D-069's trap at a computed taker; (b) `.err` TAINT-001 off the error road), otherwise after the old list's landing; (c) approved, `.err` and `.is_error` no places; the census `t171/rev171/census381/`), then the old list's landing = DEF-344 + DEF-347 + DEF-349 + DEF-353 + DEF-354 +
  DEF-355 + DEF-356 + DEF-357 (the review groups, HIGH but DEF-353 MEDIUM: an address carried into a struct field, a copy, a callee-built
  struct or a call result raises no plain party beside a claim (DEF-344); a copy of a parameter's pointer PART, `<-pp` or `hh.p`, enters no
  summary (DEF-347, DEF-342's residual); `push_call_view_party_in` fails OPEN at fuel 0 (DEF-349: a whole-argument party there); the pointer
  judges skip an unreadable step and `escape_type_holds` answers no for an unknown (DEF-353: a judge that meets "unknown" answers YES); a
  pointee-changing cast INSIDE a getter's argument, `(<-(raw ids(ap =>! S->))).x = …`, is no target of the result (DEF-354: a crossing flag
  out of `collect_view_refs`' cast arm); a view a getter returns through `(<-(q =>! S->)).x` from a NAMED `any->` local does not freeze the
  owner (DEF-355: the reverse reader through an `any` pointer, read before it is sized); a view handed on or STORED through a callee's CAST
  VALUE, `raw gety(q =>! S->)`, keeps the summary's field path, inline and named (DEF-356); a typed pointer local MADE by a pointee-changing
  cast, `S->:sp = ap =>! S->`, has no target and its steps are never erased (DEF-357) -- the last two from management's review of build 16,
  measured silent on 166 and 167 alike; the old list is measured and censused before it is cut, the 33 DEF-344 tree candidates read first (0
  sites, `t168/CENSUS_344_READ.md`) -- and DEF-345 (HIGH: `$$m` of a binding declared without an initialiser reads the previous frame's
  bytes when the callee leaves it unwritten, nitpick-libs_29's report of 2026-10-29, measured 85/85 at both legs; RULED by the author
  2026-10-29 17:29 -- (b), the middle road, D-366, two respellings: refused when the type does not drop, a dropping type keeps D-225's
  vacant value, C#-style `out` rules a later relaxation -- implemented in the old list's landing; its test items: `cast_place_readers.npk`'s site 22 re-cut so
  the write below the result's dereference is what is tested, inline legal-half controls of the caller rule with no view and with another
  struct's view, `q3 =>! wild int8->` as a control of `pointer_retype_plain_ok.npk`) --, then DEF-333's landing =
  DEF-333 + DEF-334 + DEF-335 + DEF-336 + DEF-384 (HIGH: a temporary argument under a transparent kind or a ternary, a by-value argument's view
  returned or carried out of the owning frame, a slice-storing callee's destination spelled through a pointer field, a dereference, a call
  result, a cast, a move, a ternary or an unwrap, and a builtin view-maker over `#ptr_add` recording no ref -- its six tree sites read, none
  a live hazard; and a view-maker over an element address, `string_from_bytes(@x[0i64], n)`, never frozen, 13/13 -- DEF-384, registered after 171, or in DEF-318's landing if its fix needs the element-address party), then DEF-312's landing = DEF-312 widened (HIGH: a pointer field of a call-result pointer's pointee written two levels down,
  F3a/F3b/a15) + DEF-348 + DEF-350 (HIGH, from the review of build 14: a write through a pointer FIELD of a pointer parameter's pointee in
  every spelling -- `hh.p.o`, its explicit dereferences, the pointee whole, a `Self->` method of a struct holding the pointer, plain or
  through a `dyn` -- and a pointer VALUE handed out of a holder's field by a getter; one fix with DEF-312's, a composed path re-resolved at
  each pointer-typed hop, not M3) + DEF-325 (LOW, while ≤ ~30 code lines), then DEF-318's landing = DEF-318 to DEF-323 (HIGH: the element-address
  party's six missed spellings; smallest first: 322, 323, 320, 319, 321, 318), all before M3 -- the fuzz seats' M16 targets the first binary
  after DEF-318 to DEF-323's landing --, and DEF-351 (the bare-builtin whole-write artefact behind probe10:151), DEF-352 (a ternary's wild
  provenance, TYPE-032) and DEF-358 (the emitter's refusal of a pick expression as a `for` iterable giving an array, registered at 168),
  LOW, after DEF-318's landing, and DEF-369 and DEF-370 (LOW, unplaced, registered at 170: a ternary's two arms moving two fields of one struct are
  MOVE-001 where a pick's arms are not; a pick expression standing as a method call's receiver is typed under the call's expected type,
  TYPE-007 at each `give`), DEF-376 (LOW, unplaced, registered at 171: `Self` by value in a trait's default body reaches EMIT-002), DEF-379 (LOW, unplaced, registered after 171: `return Result{…}` in a `comptime func:` does not fold) and
  DEF-375 and DEF-377 (MEDIUM, unplaced, registered at 171: a generic enum's instance inside a generic function has no type definition, and
  `llc` refuses it; a `T.Item` parameter cannot be instantiated at a concrete type -- each refused at build time); then DEF-298 + DEF-299
  (HIGH: DEF-289's class's last two faces, with their pointer twins, s302k/s302l, the fuzz seat's (A) face q05 and DEF-313 -- a store
  through a pointer PARAMETER into its pointee, recorded on no chain -- as M3, the store-matrix destination path, the census first), then
  DEF-286 (function types keyed by content; the libraries' next re-pin waits for it and carries every safety fix) -- management's order of
  2026-10-29 (07:15:11, 07:52:03, 11:05:40, 16:48, 21:09, 23:02; 2026-10-30 00:42, 00:54, 01:01, 01:37, 06:11, 06:47, 06:53, 06:59, 07:10, 08:39:44, 09:08:19, 09:52:11, 09:56:11),
  safety before performance -- then DEF-292, DEF-293, DEF-294, DEF-291, DEF-295, D-358 (Fable's), step 3d's rest (DEF-239), step 4. The plan
  of record is `meta/roadmap/` (start at `ROADMAP.md`; cycle 1.6's map is `meta/roadmap/1.6/README.md`, its subcycle record
  `meta/roadmap/1.6/1.6.1e.md`); every open defect and question is a row of `meta/roadmap/OPEN_DECISIONS.md`; the notice log to the library
  seats is `meta/NOTICES.md`.
- **The specification set is complete:** `meta/specs/` holds twenty-one documents and `DECISIONS.md` records 366 decisions, D-001
  through D-366. `meta/specs/` and `DECISIONS.md` are the authority on the language, never a summary of them.
- **The live hand-off state** (the RESUME-HERE entries, what is in flight) is the tail of `.internal/handoff_35/STATE.md`, and the seats'
  standing procedure is `.internal/handoff_40/HANDOFF_41.md` §7–§9 (both gitignored, on this machine).
- **When a landing lands:** insert its head paragraph at the top of `meta/STATUS_HISTORY.md` (the apply scripts' anchor: the previous
  landing's head opening, `IS UNDERWAY: **…`), change the `main =` and the queue lines above, put a new gotcha's full text at the end of
  the list in `meta/BUILDING_AND_TESTING.md` and one digest line for it below. Never grow this file's status again (Q70, 2026-10-23).

## Building and testing — read `meta/BUILDING_AND_TESTING.md` before any chain, record, harness or landing

- **The whole suite:** `python3 bootstrap/harness/harness.py` (about two to three hours; most of it prints nothing). It runs on cloud2
  through management (`cloud2: harness <landing> <sha>`); the workstation takes one only while cloud2 is busy. `--only X` is for
  iterating, never for concluding: nothing is committed on a filtered run.
- **The quick loops:** `python3 bootstrap/harness/quickemit.py [--keep] [--ir] FILE` (builds `npkc` when a source is newer, compiles,
  links and runs FILE), `python3 bootstrap/harness/quickcheck.py FILE` (the checker; rebuild it after every edit to `src/`).
- **npkg:** `quickemit.py --keep npkg/main.npk`, then `.internal/quickemit/p_main_npk build | test [--only S] | verify [--record]`.
- **The manifest:** ANY `src/` change can move `nitpick.obligations` (row texts name interned ids); run `npkg verify --record` in the
  commit that changes it and read the gate's two zeros (verdicts moved among shared rows; discharged counts fallen).
- **A landing, in order:** chain → record → rekey and mechanism read → texts → the F-notice to the library seat BEFORE the harness →
  post/apply → squash → in-process checks (foreground) → harness → `land.sh` on green → the notice verbatim with closing lines → STATE.
- **Never:** two harnesses on one machine; a record beside a harness's verify/parity; a chain beside a harness after its first hour;
  `pkill -f` a pattern that is in your own command line; a trailing `&` on an `&&` chain; a waiter on a fixed label (match
  `CHAIN[0-9]+ DONE`, end on the watched process's exit, cap it with `timeout`); a union merge of this file.
- **Run a program by hand under `ulimit -n 1024`** (the runners' descriptor limit); `grep` in the tool shell is ugrep with
  `--ignore-files` (`command grep` for a census).

## Gotchas — one line each; the full text is in meta/BUILDING_AND_TESTING.md
Three to know first:
- Never commit on a filtered `--only` run; it skips every whole-suite check — see "`--only` is for iterating, never for concluding."
- On `exited N, expected 0`, find `exit Ni32` in the test file to see which case broke — see "A test's expectation lives inside the test"
- Expect ~1 minute even for one test; that's the floor, don't optimise around it — see "Every test builds the whole frontend through the seed"
Five more, each a 1.4 debugging cycle:
- Never copy a string binding to another (TYPE-046); pass as plain arguments, consume with `move T:p` — see "Strings are move-only owners"
- On `check_type_walkers_total`, finish the change or give a TRUE excuse — see "The walkers-total instrument refuses a half-done type-kind change"
- Expect emitter fixes in `tools/` only after a snapshot refresh — see "A backend fix does not reach the tools until the snapshot carries it."
- Debug `npkc` traps as `src/` bugs: `gdb -ex "break npk_trap" -ex run -ex bt` — see "`src/`'s own code is checked like everyone else's since the switch"
- Never rewrite `done/` or settled DECISIONS; annotate with dated notes (D-085/D-202) — see "Never rewrite `done/` archives or settled DECISIONS text"
- Run programs by hand as `(ulimit -n 1024; ./prog)` — see "Both runners stand under `nitpick.toml`'s `[limits] nofile` (1024; 1.5.1b step 5)."
- Expect `pass h.n` to clear `h`'s flag only when its type drops (DEF-8) — see "`pass h.n` over a copyable field no longer clears `h`'s drop flag"
- Follow `wild_release_all()` with `exit`; measure inside `exit`'s operand — see "`wild_release_all()` is followed by `exit`, or TYPE-062"
- Expect the aggregate to stay live, dropping its siblings (S-26) — see "A `move` or `pass` out of a FIELD leaves the type's vacant value"
- Expect `Box<Point>.eq` to need `Point: Eq`, asked at the CALL (D-256) — see "A derived body reaches every member through the trait it derives"
- Gate builtin-kind methods in both via the one `types.npk` predicate — see "A builtin kind's fixed method set lives in the checker AND the emitter"
- Annotate: `Opt<int32>:o = Opt.None;` (TYPE-022); `pick` an `Optional` via `??` — see "A generic enum's payload-less variant needs the annotation"
- Grep the twin spelling (`=>`/`=>!`, `?!`/`?|`, both `pick`s) first — see "A rule written for one spelling of a construct is owed to the other."
- Use a prelude symbol before asserting on it in IR (D-262) — see "A prelude function is in an emitted module because something referenced it"
- Store a `T` as `move T:v` + `move(v)`, read `move(s.items[i])` (D-264); `move(x)` spends a scalar too — see "A `T` is move-only in a generic body"
- Quote `build/npkc.ll`'s sha256 to other machines; compare object sizes — see "The ladder prints its digests, and the emission's is the one that travels"
- Use `pick (move(v))` for `Self->`; no selector inside the arm (TYPE-067) — see "A lending `pick`'s binding is a VIEW, and a view has no address"
- Check a floor primitive's siblings for the same edge — see "A primitive's empty case is the runtime's to handle, and its neighbour is the model"
- Test drops in a function that RETURNS; `exit` never drops `main`'s owning locals (D-183) — see "`exit` runs joins and defers and no drops"
- Name only parameters in `old(...)`; repeat clause keywords, never a comma — see "A contract is a check in every build and a row in the manifest"
- Expect an undischarged `prove` to fail only `npkg verify` (`NITPICK-VERIFY-001`) — see "A `prove` is refused by the VERIFIED build only"
- Give a counted loop a positive literal step (TYPE-068); a computed step is a `loop-step` row — see "The counted loop's head is checked"
- In verify tests, name one `exhaustive checker` row per `pick`, both spellings, CODE lines only — see "The encoder is path-sensitive"
- Never use a rule added this cycle as a negative self-check case — see "A runner's negative self-check case runs under the SNAPSHOT"
- Import inline-module types (`use m.{T};`); private is RESOLVE-003 — see "A module member is called `m.f(x)`, read `m.MAX`, imported `use m.*;`"
- Use `@` elsewhere (BORROW-014) — see "A `$$i`/`$$m` claim is a whole call argument or a pointer local's whole value, and `@` claims nothing"
- Never `@`, `$$i`, `$$m` or pointer-receiver-call a `fixed` binding or field (TYPE-071) — see "A `fixed` binding has no address"
- Wait for the refresh; else RESOLVE-002 (D-205) — see "The compiler's own `failsafe` cannot name a NEW prelude identity until a snapshot refresh"
- Profile with callgrind over `npkg/main.npk` (~15 min), read the EXCLUSIVE top; `perf` can't run here — see "Measure before attributing a cost"
- Keep shifts in `0 ≤ n < width` (TYPE-070); a computed shift traps, so `failsafe` names `(ShiftRange)` — see "A shift's amount is checked"
- Write a mask wider than 64 bits as a numeral; a computed one stays `open` (D-280) — see "The bit-vector crossing stops at 64 bits"
- Run `--record` in the commit that adds twisted-typed code to `src/` or the prelude — see "Every twisted compare and cast out is an `err-exit` row"
- Treat a guard inside a clause or `prove` as part of the proposition (DEF-33) — see "A proposition holds only where its evaluation does not trap"
- Bound each float symbol both ways against literals (D-281); `prove(a + b > a)` alone stays `open` — see "A float row needs bounds for tier 2"
- Annotate a `simd(...)` context; integer lanes need `(IntOverflow)` — see "`simd` lanes are terms, and a `simd` local's lanes are its versions"
- Re-run `tcb_floor.py --write` and `verify --record` with any floor edit — see "The floor is specified now, and an `open` floor row is a red run"
- Count guards as `@npk_trap(i32 CODE)`; `?!`/`!!!` go via `npk_raise` — see "A program's raise is `npk_raise`; a guard's trap is `npk_trap`"
- Put floor allocas in the entry block, fully defined — see "Every alloca of the floor sits in its entry block and is fully DEFINED there"
- For a new floor syscall: row first, `gen_tables.py`, then a `kernel_effects.npk` EQUALITY case — see "The kernel-effect table is ONE authority"
- Read a floor model against its code CASE BY CASE; the belt proves only names (E-3) — see "A named block is not a modelled one"
- Never name a `func:` or callable after a builtin (RESOLVE-001); `gen_tables.py` writes unless `--check` — see "A builtin's name is the compiler's"
- Default to the safe reading (DEF-54) — see "When a decision ENUMERATES the cases it knows and defaults the rest, the default must be the safe reading"
- Keep the twins byte-identical (D-295) — see "The floor's models are read TWICE, and a probe with two cases in one file is read against the file"
- Iterate rejection files with `quickcheck.py` or the stage's tool; finish full — see "The harness's `--only` reaches the UNIT tests alone"
- Write each assumption's ARGUMENT beside it — see "A floor spec clause is a HYPOTHESIS ABOUT THE CALLER, and TCB.md §4d says who keeps it"
- Write each translator clause (`objects`, `frame`, …) once per section — see "What the translator reads once is written once"
- Time one z3 over the whole file, wall-clock beside rlimit; READ a hang-net red (D-297) — see "A solver cost is measured in the RUNNER'S OWN MODE"
- Mark every `// stress:` program `// explore: N` or `// explore: no <reason>` — see "A concurrency test is EXPLORED as well as stressed"
- Read a control's ROUTE (DEF-57) — see "A control proves sight of its OWN defect only when the schedule that finds it goes through that defect"
- Let only a real fault enter `npk_trap`; a watcher parks — see "An executor that sees `@npk_frozen` is watching a trap, not making one"
- Share an atomic via `wild` storage and `atomic_from_ptr` (`atomic_threads.npk`) — see "An `atomic<T>` cannot cross a spawn"
- Never put `"split-stack"` on a floor `define`; chain within a quarter of 64 KiB — see "Every emitted function checks its stack; the floor's do not"
- Name `(CastRange)` too if a float casts to an integer (D-306) — see "Every `failsafe` names `(StackExhausted)` and `(MachineFault)`"
- Choose a "none" the domain can't produce, and check something enforces it (DEF-69) — see "A sentinel is a value the domain never produces"
- Re-run explorer controls after any startup step — see "A floor global whose initial value is not zero gets a STATIC initializer, never a boot loop"
- Find one-point windows with HOLD (`hold-at:`, `NPKX_HOLD1..4`), not kept seeds (DEF-67) — see "A kept explorer seed goes stale with no signal"
- Write a new executor or TLS word at the rebirth in `npk_thread_start` — see "A spawned thread's floor state is a pool entry, reborn per thread"
- Give a kind a flags home before its first `pub` — see "A declaration's flags live where its kind says, and a slot read as a flag word must BE one"
- Put `// expect-error:` on its own line; a file with none is skipped silently as a fixture — see "An expectation is a `//` comment of its OWN LINE"
- Spell the pid with `sys_pid_tag()` (DEF-137); length moves the explored schedule — see "A per-process temp path spells its pid FIXED-WIDTH"
- Keep bounds `int64`-fitting integers (TYPE-068) — see "A counted loop's operands are widened by their own signedness, `till` never infers a direction, and a bound is an integer that fits the `int64` counter"
- Cast `=>! wild T->` for storage with no value yet — see "A store through a pointer drops the old pointee when the pointer names MANAGED storage"
- Expect `move(x)`/`pass x` to zero the slot (DEF-120); each one changes the emitted text — see "A moved-out whole binding is VACANT"
- Expect coroutine flags at frame role `50 + ordinal` (DEF-121) — see "A consuming `pick`'s arm is a SCOPE, and its bindings drop at the arm's end"
- Avoid TYPE-089 (`move`) and TYPE-090 (`fall`) — see "A `where` guard decides and does not consume; a `fall` lands on an arm that binds nothing"
- Treat a `pick (move(e))` binding as an owner: a second `move(x)` is MOVE-001 — see "A consuming `pick`'s binding is a binding to the move rules"
- Claim `$$m` to write; read with `$$i` or `@`; never pass `$$i` to a writing callee — see "A write THROUGH a `$$i` holder is BORROW-013"
- Rely on both (DEF-127, DEF-134) — see "A `for` binding ends with its loop, and a by-value receiver through a pointer is the pointee"
- When a rule lands, sweep under BOTH checkers and read every site — see "A rejection file's silent site is invisible to D-237's set rule"
- Run `verify --record`; quote `manifest_gate.py`'s zeros — see "ANY `src/` change moves `nitpick.obligations`, and the in-process checks cannot see it"
- Run `harness.py --only tests/backend/` before committing an emission change — see "A change to emitted TEXT breaks the emitter's unit tests"
- Link the test object against the previous floor too; passing on both tests nothing — see "A floor fix is measured against the floor before it"
- Behind a pointer write `(<-p)[i]`; `p[i]` is TYPE-082 — see "A `List` is indexed `l[i]`, and changed only through its operations"
- Make the rule hold of the field's vacant value (TYPE-077); a limited field has no address — see "A field may carry its own `limit<Rules>`"
- Spell `uint64` past 2^63−1 with bit ops (`~0u64`), else TYPE-076; FNV basis is deliberate — see "A constant means what the run time means"
- Re-record with new arithmetic in `src/`/`lib/`/prelude; never copy a row count — see "Every plain `+ - *` and negation has an `overflow` row"
- Expect `rows.txt` to have TWELVE fields; elision needs every context row — see "A guard inside a clause check has a row per context the check runs in"
- READ a `hung` red control before re-running; its net is 300 s, not 60 — see "An explorer control's finding seed may spin for half a minute"
- Use `+% -% *%` for wrapping (hashes, PRNGs); plain integers and `simd` lanes only (TYPE-078) — see "Modular arithmetic is spelled `+% -% *%`"
- Write no `sealed`, touch no `hidden` field outside its module; built-in headers are sealed — see "`sealed`/`hidden` draw the private-member line"
- Loop over a binding's `.len`/`.count` so `bounds` rows discharge — see "An index's proof is a length TERM, and a length term belongs to a binding"
- For a construct emitted more than once, check what the belts count (DEF-82) — see "A guard inside a `defer` body is one row and several traps"
- Keep 2^47's four spellings equal (`check_len_ceiling_agree`); never say "proven" — see "Every length is at or below 2^47, and the allocator is why"
- Name `(OutOfBounds)` in any program calling either — see "`string_from_bytes` and `#wild_slice` are guarded, and arm `OutOfBounds`"
- Name it in every pusher (D-308) — see "A `List`'s `count` and `cap` carry `limit<ListLen>`, so a program that pushes to one names `(LimitViolated)`"
- Bracket new per-module prelude output in `emit_program.npk` (`check_prelude_trimmed`) — see "A prelude RULE's predicate is an item"
- At a member read through a pointer, ask the pointee's kind (`b.cap` on `buffer->`) — see "The encoder's length fact must look THROUGH a pointer"
- Give it `decreases` (before `invariant`) or `unbounded` (TYPE-072); name `(DecreasesViolated)` — see "A `while`/`when` loop states why it ends"
- Check roles in use (0–7+, 20–25, 30+k, 41, 42–44, 50+k, 60+k) before adding one — see "The frame roles 42, 43 and 44 are the measures'"
- Put it before `never fails`, recursive groups only (TYPE-074/075) — see "A FUNCTION's `decreases` is checked at every call inside its recursive group"
- Write the measure with the loop; hoist a call bound into a local — see "Every `while`/`when` in the tree states its clause, and a new one must"
- Hoist a pointer or by-value-aggregate bound: `int64:n = x.count;` — see "A measure decides only over STABLE terms, so write the bound as a local"
- Expect a violation there as TYPE-069 at the loop (`fold_while`) — see "A `decreases` in a `comptime` body is checked by the evaluator"
- Add it to the runners' generated failsafes too; prelude loops have measures (REACH-002) — see "Every `failsafe` names `(DecreasesViolated)`"
- On RESOLVE-001 from two hoists, keep the first local and drop the second declaration — see "Two hoisted counts in one scope collide"
- Run `gen_tables.py` after it; `npkc` embeds `prelude_source.npk` — see "The generated prelude is regenerated after a sweep of `src/prelude/`"
- Set `x.fe.terminated` only at `unreachable` or `ret` — see "Emitting a branch to the label that follows does not mark the block terminated"
- Pick a deadline only a hang reaches (60 s, not 5); READ a red under load (DEF-85) — see "A test's join deadline is a HANG NET, never a verdict"
- Put the pid in `/tmp/npk_<name>` and unlink it at the end (`trap_stops_runner.npk`) — see "A program test's temp path carries its pid"
- Treat an `EMIT-002` in `tests/accept/` as a compiler defect — see "An accepted file is also EMITTED"
- Ask who frees a `_` payload; churn-twin it under `NPK_HEAP_STATS` — see "`_` in a `pick` pattern binds nothing and keeps its position"
- Sweep with full `npkc`, not `tools/check`, to ask "does it compile" — see "A whole-tree sweep with the CHECKER sees no emitter refusal"
- After encoder changes run `vprog.py` over `tests/verify/` first — see "A by-value struct's field is one term while nothing writes the binding"
- Match the trait's `move`s (TYPE-014); `.clone()` inside to own — see "An impl's parameter is `move` exactly where the trait's is"
- Use `ident_slot`; `fnem_lookup_idx` is for minted names only — see "The emitter resolves an identifier by the resolver's symbol, never by its name"
- Expect `'\u{…}'` and chars above U+00FF to be `char32`; a `char8` slot refuses (TYPE-007) — see "A character literal has a width"
- Never expect "the first to fail in time" (D-334) — see "The join relays the EARLIEST-SPAWNED child's error"
- Bound waiter loops and handle the error; `?! E` traps now (DEF-147) — see "An expired `timedwait` fails without re-acquiring, the guard spent"
- Never `destroy`, assign, `move` or `$$m` a crossing before the block ends (BORROW-016) — see "A spawn LENDS its crossing until the block's join"
- Read `argv` in place; no cstring `.clone()` in `src/`/`tools/` yet — see "A `cstring` is string-shaped and OWNS the copy `to_cstring` makes"
- Build dev by hand (`.internal/twofloor_dev.sh`); `quickemit.py` is WRONG — see "A floor ABI change refreshes the snapshot in TWO hops"
- Pass only an owning place, no pointer on its path (TYPE-091); avoid DEF-159's shape in `src/` — see "`destroy` takes the OWNING place"
- Use `a.cmp(b)` for strings; floats are TYPE-088 — see "`<=>` orders an integer, a `char`, a kernel identifier and the twisted kinds that order"
- Spell a minimum `(-127i8 - 1i8)`; `-128i8` is TYPE-031 — see "A certain constant division is refused wherever the folder decides the pair"
- Make type predicates answer `TY_ENUM` by its payloads (DEF-160) — see "An enum payload carries an address as a struct's field does"
- Expect a field view moved out to stay BORROW-001 (`collect_moved_place_refs`) — see "A moved-out owning value carries what its place holds"
- Give a new binding kind its own `assign_assign` branch, or it stays MOVE-001 — see "A `move` parameter's re-assignment is an assignment"
- Run `.internal/handoff_35/set_check.py` on a WIP — see "A `pick` does not select on a pointer, and a new TYPE rule can silence an ANALYSIS file whole"
- Write `f(x)`, never `x |> f` or `f <| x` (NITPICK-PARSE-014) — see "The pipe operators are struck"
- Name only the selector's variants (RESOLVE-002); `mod:error;` is PARSE-001 — see "An arm over an enum names one of ITS variants"
- Expect module-qualified names via `type_display_qualified` (DEF-126) — see "A mismatch between two same-named types names their modules"
- Expect `(-5i32..-2i32)` to fold (`pat_bound_value`); else `iv_broken` (DEF-132) — see "A range pattern's bound is read through the folder"
- Move arms with locals into own functions — see "A function the expression recursion passes through at every level holds a DISPATCH, not the arms"
- Have new recursive rules ask `p_too_deep`; members via `p_member_name` — see "The parser's descent has ONE bound, and a type is part of it"
- Use a variable — see "`pkill -f PATTERN` from a Bash tool call kills the call itself when the pattern's text appears anywhere in that call's command line"
- Put `cd` on its own line or in `( … )`; kill a wrapper first — see "`cd X && <command> &` runs its `cd` in the background job, not in the shell"
- Expect TYPE-043 otherwise, `dyn` methods included — see "An `async` METHOD is called under `await` or spawned with `drop`"
- Avoid MACRO-010 — see "A macro body is declarations or statements, not both; a broken body expands to nothing; a declaration's modifiers are read past"
- Write each site's line with its `expect-error-at`; a silent site fails (D-332) — see "A rejection file names ONE `expect-error` line PER SITE"
- Test PARSE-0NN and parser MACRO codes in `tests/frontend/parse_*.npk` — see "A refusal the PARSER makes is a unit case, never a rejection file"
- Make a new node constructor push height and log; add new kinds to `ast_depth.npk` (PARSE-012) — see "No tree nests deeper than 256 levels"
- Pass a rejection or accept file's flags in a `// npkc-flags:` header line — see "The build mode is the front half's"
- Never give one type arguments (TYPE-016); no `string<char16>`/`<char32>` until 1.6.1f — see "A builtin scalar takes no type arguments"
- Use literals, not expressions (TYPE-035) — see "A module constant of a float, fixed-point, `tbb` or ternary type is WRITTEN, not computed"
- Bound a type-resolving fold (`stamp_depth`); `comptime(…)` must fit its slot (TYPE-031) — see "A folded value is held to the type it lands in"
- Make a new literal reader ask the node's TYPE first — see "A literal's PAYLOAD is its value only for a plain integer, a `tbb` and a ternary"
- Never name anything `pid`, `tid`, `fd`, `uid` or `gid`; `tid` fails as PARSE-002 at first use — see "`tid` is a type keyword"
- Use `objcmp.sh`, `fnorm2.py` — see "Two emissions that differ only in how a constant is SPELLED are compared as ARTIFACTS and through a reference"
- Include texts long enough to split the roads (`float_literal_kat.npk`'s `roads`) — see "A test must hold the text that tells the roads apart"
- Gate statement arms on `fold_frames`; end arms in `give` — see "The compile-time evaluator runs statements only inside a `comptime func:` call"
- Write via `variant_tag_of`, not `num` — see "An enum value in the evaluator is its variant's POSITION, and since D-356 it leaves as the variant's TAG"
- Write a body needed at compile and run time twice (`c_f`/`r_f`); else `llc` sees it undefined — see "A `comptime func:` is emitted nowhere"
- When adding a bound, ask what RESETS it — see "The evaluator short-circuits, and a fold entered from inside an evaluation is part of it"
- Never repeat a value arm in a test of something else (DEF-212, OPEN) — see "Two value arms with one value reach `llc`"
- Record the user's words, date, session and questions in the queue-holder's NEXT commit — see "A relayed answer is recorded with where it was said"
- Use `float_const_text`; change it with the encoder's `fp_literal` — see "A float constant has ONE writer, and it writes BITS at both widths"
- Regenerate vectors via `float_vectors.py --write` — see "`float_round(text, width)` is the conversion, and its answers come from exact rationals"
- Ask where the evaluator reads a literal (DEF-205) — see "A `comptime(...)` operand is typed by NOBODY: the compile-time evaluator alone reads it"
- Compute an infinity, never write one; literals rounding to inf or 0 are TYPE-031 — see "A float literal FITS its type, and its length is free"
- Put new float-literal rules in `float_text_ok`, once; probe every position — see "A float literal's rules follow its WIDTH, whatever gave it one"
- State it as `ex <= text.len + 1000`; test a nine-digit exponent, plain and `--obligations` — see "A bound on a text is stated against the text"
- Say which input family went untried; LLVM caps the exponent at 24,000 — see "A measured claim has two halves, and a measurement has a domain"
- Test LEX-009 in `lexer_numeric.npk`, not a rejection file — see "A float literal is its production, and its tail is a suffix or nothing"
- Expect `2.5i32` to be TYPE-031 and `flt256`/`flt512` TYPE-001 — see "A literal with a fraction takes a float or fixed-point suffix"
- Use `suspend_until`/`suspend_io`/`io_watch` only in `async` (TYPE-043), parks never in `defer` — see "The task builtins are `async`'s"
- Put a channel's level ABOVE any mutex held while sending or receiving (LOCK-001) — see "A channel's LEVEL is a lock level"
- Never lock in a prelude-trait impl (LOCK-002); guard outside the call — see "An implementation of an undeclared trait method acquires nothing"
- Declare exact `acquires N` if called under a guard — see "A `<=`-bounded trait method is called with nothing held; a hold is visible or it is nothing"
- Write no cycle (MACRO-004) or nested `macro:` (MACRO-005) — see "A macro cycle is refused at its declaration, and the edge is what the body writes"
- Arm `expand_in_*`, `blank_*`, `clone_*` — see "The expansion walk reaches every expression, and every instantiation is its own copy of the body"
- Avoid MACRO-011 — see "A macro parameter is substituted wherever an expression stands, and cannot be a type's name or a name the body declares and uses"
- Tell instances apart by type arguments, never signature or terms — see "A generic function's instance is its TYPE ARGUMENTS, at every reader"
- Expect TYPE-093 otherwise — see "An enum variant's explicit value is a bare integer literal in `[0, 2^31 − 1]`, distinct from every other tag"
- Use the resolver's binding wherever a walker looks a NAME up (DEF-196) — see "A compile-time binding is the resolver's, never its spelling"
- Answer it only via `macro_may_bind`; invoker generics are TYPE-001 — see "A type's name in a macro body resolves where the macro was written"
- Do it first (DEF-191) — see "When a test answers wrongly, hand-write the shape without the feature under test before reading the feature's code"
- Expect `fold_chain` in the message, deduplicated without it by `fold_said` — see "A compile-time failure names its call chain"
- Read every agent's diff and report whole and ask how each mechanism fails; at most three agents at once — see "Read every delegated diff whole, and ask its author how each mechanism FAILS"
- Add the arms REACH-002 names; a `dyn` call reaches every impl — see "`failsafe` must name what the PRELUDE can raise on the program's behalf"
- Change the layout or triple in emitter, floor, shim AND pin in one commit — see "Every module of ours states the pinned layout and triple"
- Read `collect_call_refs` with `x.reporting` off; never on a bare builtin — see "A call's result holds a borrow by PROVENANCE, not by shape"
- Copy a `T` only under `T: Copy` (TYPE-046) — see "`Copy` is a prelude-owned MARKER, and the bound `T: Copy` licenses the copy"
- Never name the emitted function's parameter in an arg (RESOLVE-002) — see "A macro ARGUMENT is the caller's text and resolves at the invocation site"
- Carry them in new expander paths — see "The hygiene marks are COUNTS in a node's flag word, and whatever replaces or copies a marked node owes them"
- Bind via `ident_bind`, count new binding forms in the pre-walk, probe with `vprog.py` — see "In the SMT encoder a spelling is not a binding"
- Before calling an inconsistency a defect, check whether another rule separates the spellings (DEF-220) — see "Read a finding against the reference's OTHER rules before calling it a defect"
- Add leavers to `stmt_exits` and `stmt_completes` — see "A bare `#unreachable();` leaves, and a leaver is a row in BOTH lists"
- Keep `emit_fit`-lowered kinds out of `temp_producer` (DEF-227) — see "A temporary is registered ONCE, by the one site that produces its value"
- Never register the final value or an item (F-037) — see "A template's pieces are the statement's temporaries, and its value is its own body"
- Add operand kinds to `type_kind_has_elem` (TYPE-018) — see "A generic function may not instantiate itself at a type built from its own parameter"
- Extract with `llvm-20-dev`; rebuild engines in a fresh `ENGINES_ROOT` — see "A toolchain pin is MOVED by measuring first, with the machine untouched"
- Measure headers at the final line count — see "A struct literal that writes several `sealed` fields from outside their module is ONE report"
- Keep a refused literal a LITERAL token; test counts in `parse_one_report.npk` — see "A refusal at the lexer's or the parser's seam is ONE report"
- Bail on `p_in_panic` post-header — see "A declaration whose header fails is ONE report, and a declaration's recovery knows a bracket and a terminator"
- Never assign it (ASSIGN-002); write through no `fixed` view — see "A `fixed` parameter is written by its caller and never by its body"
- Declare `string_bytes` readers `fixed uint8[]` (TYPE-086) — see "`fixed T[]` is a TYPE, the read-only view, and a view's rights travel by its type"
- Check whether the target's own refusal (TYPE-024) comes first — see "A write or an address into something that is not a place is ONE report"
- Run `gen_tables.py` after any prelude edit — see "A `frac`'s parts are its readable parts, and the prelude is regenerated after any edit"
- Never overlap them — see "A harness and a manifest re-record in one worktree RACE; the harness starts after the record, on the committed final tree"
- Use `sitenorm.py`; diff `rows.txt` by hash — see "A prelude edit moved EVERY program's emission through D-179's site table until landing 126"
- Never write through it (TYPE-086) or take an element's address (TYPE-071); writers take `T[]` — see "A `fixed` slice is read-only through it"
- Unwrap an `exit` operand to `int32` first (TYPE-007); grep `e.expected =` sites — see "An `exit` operand is held to `int32` as any slot's value is"
- Vacate at every flag clear; call `mark_root_taken` per new address lowering — see "An address-taken binding drops unconditionally at its scope exit"
- Value all variants or none (TYPE-093) — see "An enum values every variant or none, and a macro parameter cannot name what the body emits"
- Wrap a builtin in a function to pass it (TYPE-054); when a typer returns 0, ask who reported — see "A builtin is called, never named as a value"
- Resolve names in one place (`fixed_global_sym`) — see "A module binding may be initialised from another `fixed` module binding of ANY type"
- Never inside `if`, `when`, a loop or an arm (REACH-001) — see "`failsafe`'s pick may stand in a bare block, a statement macro's expansion among them"
- Carry merge facts in one function (`state_take_definite`) — see "A `pick (r.is_error)` whose error arm leaves checks the `Result` for what follows"
- Expect TYPE-094, TYPE-040, TYPE-024 — see "A `give` lives in a pick EXPRESSION's arm and a `fall` in any pick arm, the INNERMOST pick governs, and a `defer` body leaves for nowhere; a `frac` member is never a place"
- Ask `ll_member_type` and skip `NIL` in new aggregate lowerings — see "A `NIL` member of an aggregate lowers as `{}` and is never touched"
- Never name anything `on`, `any`, `buffer` or `acquire` — see "No keyword is a declared name, and the compiler says so where the name is written"
- Ask `const_init_verdict` for constant-only rules — see "A `joins` deadline, an attribute's name and an `opaque struct`'s position are checked now"
- Keep mutable state in `main`'s scope (TYPE-055) — see "A module binding is `fixed`, with or without an initialiser"
- Use `ll_fn_open` — see "Every emitted `define` carries `null_pointer_is_valid`, and an emission comparison across landing 115 is read with it stripped"
- Declare it in the borrow's block (BORROW-002) — see "A holder this frame owns is bound by its block, and a callee's store is asked the same question"
- Use a `fixed` binding instead — see "A `comptime` value parameter is RESERVED: `NITPICK-TYPE-095` at the declaration, once, and nothing at the uses"
- Peel it first — see "A method's symbol names the impl's TARGET, and a pointer receiver is peeled before every reader of the receiver's type decides anything"
- Use `NIL` there (else TYPE-096) — see "`NULL` is a `wild` pointer's value, and a plain `T->` that may be absent is `T->?`"
- Write new tokens/nodes into the references; run `gen_tables.py` — see "`??!` is the Optional's unwrap-or-trap, and it is `?!`'s twin in every rule"
- Cast it or declare the slot `wild` (TYPE-097); census with the checker — see "A wild pointer enters a plain pointer slot only as `v =>! T->`"
- Expect TYPE-098 — see "A `comptime func:` is called inside `comptime(…)`, at a constant site, or from another `comptime func:`, and nowhere else"
- Render it via `variant_tag_of`, never `num` (D-356) — see "A payload-less enum variant is a constant outside the evaluator"
- Implement only your own trait or type (TYPE-099) — see "An impl names at least one type or trait of its own manifest's tree"
- Make every generator satisfy every checker rule — see "A driver method may take `int8[]`/`uint8[]`, and `use a.b.name;` binds `name`"
- Write via `irw_line` — see "A program's site table holds the sites its kept text references, and a site table is DENSE against its chain calls"
- Mind BORROW-015 — see "A `buffer` is indexed `b[i]` and ranged `b[lo...hi]`, and a `#wild_slice` over a header's `.ptr` is a VIEW of the header's owner"
- Read `SourceFile.path` as a KEY — see "A file is ONE module whatever spelling names it, and the source manager's key is the file's identity"
- Route method symbols via `bound_method_sym`; ask `emit_is_trait_qualified` first — see "`Trait.m(x)` reaches what `x.m()` reaches"
- In escape.npk ask `esc_place_root`, not `place_root` — see "An explicit `<-` is a place step to both borrow analyses, as the auto-dereference is"
- Trace BORROW-015 with `npkc FILE --dump-refs OUT` — see "A pointer VALUE handed to a callee is checked against the views through it, as `@place` is"
- Compare run-time values in a `where` guard; over floats use `if` (TYPE-065) — see "A `pick`'s value pattern is a constant of the selector's type"
- Never end digits in `_` (`10_` is LEX-003) — see "A numeral's digit run ends in a digit, and an attribute decorates only what keeps one"
- Avoid MACRO-002 — see "A macro takes no type arguments, its body declares only what an expansion can emit, and MACRO-009 notes the right invocation"
- Return `ArmTerm { mine, t }`; walk the arguments — see "The obligation encoder's arms are frames of their own, and a new arm is a function and a test"
- Test extremes — see "The encoder's Int form of a mask computes on the mask's numeral in `int64`, so the width's two edges are answered, never computed"
- Never re-walk — see "A body nested in a body is walked ONCE per level by every front-half pass, and a `pick` used as a value gives on every path"
- Loop `pat_alt_count`/`pat_alt_at` — see "A `pick` arm names several values with one body, and every reader of a pattern walks an arm's ALTERNATIVES"
- Add arms as functions — see "The emitter's rest dispatch is two tables that must agree, its helpers write where their lines stood, and `emit_cast_values` never evaluates"
- Measure frame allocas and stack high-water; never assume — see "A frame on a chain's path holds its recursion's operand and nothing after it"
- Give `field_store_root` an answer for every new place kind — see "A field of a binding this frame owns is that binding's own storage"
- Ask `symbol_is_param` — see "A `for` binding is a block-scoped local to the escape and aliasing analyses, and ONE predicate says what a parameter is"
- Extend `record_refs_through_indirection` for new frame pointers — see "A store through an indirection lands where the indirection points"
- Peel pointers in kind-keyed `emit_method_call` intercepts — see "An atomic's methods lower through a pointer, and `--reach` lists the arms"
- Write `comptime(width::<int32>())` — see "A generic `comptime func:` is called with its type arguments written, and a comptime body may `move`"
- Hand a storing callee `arr[lo...hi]` itself; a slice local or field is BORROW-009 — see "A slice parameter's element is a store-matrix slot, and a slice argument is a destination"
- Step a store walk over a range of an array or `List`; a slice's range is an indirection — see "A range of named storage on a store's path is a step over that storage"
- Compose a pointer's path below it onto its target (`hp.o` targets `h.o`, not `h`); give a new recursive escape walk `decreases <fuel>` — see "A write through a pointer is a write to its TARGET"
- Ask `escape_slice_targets` for a slice's storage; read `P`/`e`/`E` and compare paths levelled — see "A slice's storage is read off its refs, and a path's text is LEVELLED"
- Resolve a rootless (call-result) pointer by its provenance (`escape_pointer_targets`), not a place-chain reader; follow a pointer under a slice to the pointee's chain — see "A call-result pointer has no place chain, and a slice through a pointer reads the pointee's chain"
- Model a freeze-against-write party on the VIEW party (VIEW-like, `summary_judged`), never a standalone kind — see "A new aliasing party that must conflict with writes is VIEW-LIKE, not a standalone kind"
- Set `x.esc.cur_fn` when the aliasing pass asks an escape reader (DEF-311); resolve a deref write through `rootless_deref_base` (DEF-309); test every node of a `destroy` receiver's path (DEF-310) — see "An escape reader asked from another pass needs the function it reads SET, and a place written at or below a dereference is resolved through the pointer below it"
- Raise a new VIEW-like party wherever a view is raised (`prescan_view_parties`, `acc_call`) and admit it wherever a view is judged (`check_view_parties_at`, `check_pointer_view_parties`) — see "An element-address party is raised wherever a holder's refs carry one and judged by every pointer reader, as a view is"
- Route a rootless written place to the slice dispatch, resolve a carrier through its chain, REFUSE past the fuel (BORROW-009), and step `raw`, `?!` (`ExprEmphaticUnwrapExpr`, not `ExprRelayExpr`), `??!`, `relay`, `await` and the casts in every kind-enumerating walk — see "A rootless place is asked what its slice views, a carrier is read to the runs its chain holds, the walk fails closed past its fuel, and every kind-enumerating walk steps the unwrap operators and the casts"
- Step a dereference and a cast in every place reader of both analyses (`collect_view_refs`, `arg_place_chain`, `esc_place_root`; `path_steps`, `is_place_chain`, `place_root_symbol`, `peel_ptr_casts`), fail CLOSED where the steps cannot be read, never retype a plain pointer (TYPE-032, D-365), and run `kindtable167.py` before adding a reader or a kind — see "A place reader's default answer is a verdict, and landing 167 made the dereference and the cast steps of every reader"
- Resolve a spawn's pointer argument and a whole-pointee write's pointer to their target (`lend_through_pointer`, `ptr_place_written_whole`) — see "A spawn's lend and a view of a pointee conflict with a write through an aliased pointer, the pointer resolved to its target"
- Write a step rule in BOTH path readers (`path_steps`, `path_rel_compute`) and in EVERY writer of the callee summary (`record_mut_of_call`, `record_mut_of_place`, `ref_full_path`, `dest_place`); keep the caller's WHOLE judgment of a plain address across a type boundary (`admit_written`, build 16); vary a cast's TARGET TYPE and cut every cell NAMED and INLINE — see "The two analyses each spell a PATH, and a step rule lands in both"
- Write the four CUSTOM arms in every `expr_shape` walker and fail closed on the rest; walk every expression SLOT of a statement kind, unguarded (the slot table of `kindtable167.py`); collect a `give` from every statement kind; read a callee off the call graph; spawn an owner only by `move` — see "A pick EXPRESSION's arm is a statement list to every prescan, a `give` is collected from every statement kind of the arm, a spawned METHOD is a spawn with its receiver as the first argument, and a by-value owner crosses a spawn only by `move`"
- Walk a receiver or a sanctioned argument before judging its crossing; read `emit_child_frame` before writing a spawn rule (a by-value `self` through a pointer crosses as the POINTER, DEF-363); a `T` is an owner at a spawn; judge the receiver's walk VERDICT as an argument's (DEF-364) and test `T` recursively through `T?`, `T[n]`, `Result<T>` and a generic struct (DEF-365); a type-less method-call operand is a qualifier; a cell that `?| NIL`s its failure proves nothing — see "A spawn's receiver and its sanctioned arguments are WALKED before their crossing is judged, a by-value receiver reached through a pointer crosses a spawn AS THAT POINTER, a generic `T` is an owner at a spawn, and a name before the dot with no type is a QUALIFIER"
- Ask a move slot where its value COMES FROM (`move_slot_source`), not whether it has an address; a `move` receiver is a move slot; `exit` hides a double free; re-measure a relaxation against every cell the closed rule refused — see "A move slot asks where its value COMES FROM, not whether it has an address; a `move` receiver is a move slot; and a by-value receiver reached through a pointer is the POINTEE at an await and a spawn"
- Judge a shorthand and its long form by ONE function (`check_leaving_value`, `emit_leaving_value`) and check them with a generated twin table; a `Result` literal writes one field (TYPE-101); name a `move`-parameter function only as a callee (TYPE-100) — see "`pass v;` and `return Result{value: v};` are ONE road, a `Result` literal writes one field, and a function with a `move` parameter is not a value"

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
| `decreases`, `unbounded` | the termination clauses of D-304 (1.5.8c): `while (c) decreases n - i { … }` says why a loop ends, `while (c) unbounded { … }` that it may not. Keywords from 1.5.8c step 1 (the codes TYPE-072…075 are declared at step 0); until then `unbounded` was a function name in one test. Both read like ordinary names — a budget, a queue, a flag |
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
  `NITPICK-RESOLVE-012` at line 1 — "file `other.npk` declares `mod:hello;`
  first: a file's header names the file" (D-248). (This said RESOLVE-005, the
  missing-file code, until the install README's VM test measured it, 2026-10-08.)

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
- **LLVM 20.1.8** is the toolchain (D-349, 2026-10-08: the LLVM 20.1 release the
  distributions serve; 20.1.2, the version the prototype targeted, from 1.4.5 until
  then). ON THIS MACHINE IT IS A PREFIX, `~/.local/llvm-20.1.8/usr/lib/llvm-20`:
  apt.llvm.org's signed noble-20 packages (the ten `.deb`s and their index are
  kept in its `debs/`), extracted with `dpkg-deb -x`, because `apt` could not
  install them system-wide -- the 32-bit Mesa drivers hold `libllvm20:i386` at
  the archive's 20.1.2 and the suite ships no i386 -- so `/usr/lib/llvm-20` stays
  the distribution's 20.1.2 for Mesa and everything else. Ubuntu/Mint ship only
  versioned binaries (`llc-20`, `opt-20`, …) because LLVM 14, 18, and 20 coexist
  on this machine, so unversioned names are provided by symlinks in `~/.local/bin`,
  which point into the prefix's `bin` for every tool it has (`llc`, `opt`,
  `ld.lld`, `llvm-config`, `llvm-as`, `llvm-readelf`, …) and into
  `/usr/lib/llvm-20/bin` for the rest (`FileCheck`, `not`, `split-file`, `lldb`);
  `~/.local/llvm-20.1.8/links_before.txt` records where each pointed before. Available
  unversioned: `llc`, `opt`, `lli`, `llvm-as`, `llvm-dis`, `llvm-link`,
  `llvm-config`, `llvm-extract`, `llvm-reduce`, `bugpoint`, `llvm-jitlink`,
  `llvm-mc`, `llvm-objdump`, `llvm-readelf`, plus `FileCheck` / `not` /
  `split-file` for test harnesses. `clang` is on update-alternatives and already
  resolves to 20.
  - Verify with `llvm-config --version` (expect 20.1.8). If unversioned names
    stop resolving, the symlinks are the thing to check, not the packages.
  - `lld-20` is installed and symlinked; `ld.lld --version` reports 20.1.8, so
    the linker is version-matched with the rest of the toolchain.
- Note the naming migration in flight across the docs: older material uses
  earlier project names. Prefer current naming in new code.
