# The explorer's negative controls (1.5.7 steps 3 and 4; X-10, X-15, D-301)

An instrument that claims to find lost wakeups is worth nothing until it is shown FINDING one. Each
`.ctl` here plants a defect in the floor by text substitution on `runtime/npkrt.ll` (one or more
`old:`/`new:` pairs, applied in order, each `old` block occurring exactly once in the text as it
stands), names a program, the VERDICT the explorer must reach and within how many seeds. Both runners
build the patched floor through the one transformer, run the program under the seeds — the schedule's
measured length as k, depth 3 — and REQUIRE the verdict: a control the explorer is blind to is a red run
by name (`explore-control-blind`), the models' rule (`floor-control-blind`) applied to the explorer. The
patched floor never leaves the run's build directory.

## The grammar

```
; comments begin with a semicolon
program: tests/backend/programs/<name>.npk       the program run against the patched floor
verdict: <word>                                  what must appear (below)
within: N                                        seeds 1..N, the first that meets the verdict ends the run
preempt-at: @function <instruction text prefix>  0..4 lines: a DIRECTED control (below)
old:                                             one or more pairs; each line indented by two spaces
  <lines of runtime/npkrt.ll, exactly once>
new:
  <their replacement>
spec-old:                                        a SPEC control's pairs (step 5, D-302): the same over
  <lines of runtime/npkrt.spec, exactly once>    runtime/npkrt.spec -- a FALSE caller hypothesis planted
spec-new:                                        where the floor is untouched
  <their replacement>
program-old:                                     a PROGRAM control's pairs (step 6, X-21): the same over
  <lines of the program's source, exactly once>  the source of the file `program:` names -- a defect in
program-new:                                     the program's own synchronization
  <their replacement>
```

A control holds floor pairs, spec pairs, program pairs, or any mix; the patched spec goes beside the
patched floor in the run's build directory, so the entry checkers (D-302) are generated from it as from the
real one, and the patched program source is written there too (the same basename, which its module's name
must match) and compiled from there. A block line is taken verbatim and must begin with two spaces, so a
program control plants indented source lines only; a program with a relative `use` cannot be planted (its
imports would not resolve from the build directory: a compile error, loud). Every unit's own IR, the
planted program's included, is transformed as the floor is (step 6, X-20), so a program's `atomic<T>` and
`sys` steps are points of the schedule.

The verdict is one of:

- a word of the shim — `DEADLOCK`, `STEP BUDGET`, `MMAP`, `LOST-FUTEX-WAKE`, `LOST-WAKE`, and `ASSUMPTION`
  (a caller hypothesis of the spec false at a call, reported by the generated entry checker: the spec
  control's verdict) — met when the shim prints it;
- `wrong-exit` — the program's own answer: any exit other than its `expect-exit:`, a signal included, or any
  shim verdict;
- `exit N` — that exit exactly;
- `late N` — LATENESS: the shim's virtual run time at exit (`vrun=` on the trace line, the clock's advance
  since the start) at or past N nanoseconds. This is the class D-301 names — a lost wakeup in this runtime
  degrades to a task sleeping to its deadline, which an exit code cannot see and virtual time hides — and it
  is the only lens for a mark that was OVERWRITTEN rather than left unread.

## Blind and directed

A **blind** control is found by PCT alone: it measures the instrument's POWER. A **directed** control
(X-15) names up to four atomic sites of the explored floor — `preempt-at: @function <prefix>`, resolved
by both runners against the patched floor's `sites.txt` to exactly one `atomic` row (else
`explore-control-site-unmatched` / `explore-control-site-kind`, by name) — at which the shim demotes the
arriving thread below every other, at every arrival: a change point that fires at a place instead of a
step count. It measures the instrument's SIGHT: told where to preempt, does the oracle or the program's
answer see the bad state? Three of the model controls plant interleavings whose windows are one
scheduling point wide inside runs of 100,000 steps; blind PCT's bound there is 1/(n·k^(d−1)) ≈ 10⁻⁸ per
seed, and 0 of 100 seeds found any of them, so they are directed. A directed site takes effect BEFORE
the named instruction (the scheduling point precedes the step), so a control names the first step AFTER
its window — the reserve, not the capacity read; the stop walk's first read, not the claim.

## The program control (step 6, 2026-09-18)

`atomic-lost-update.ctl` plants a lost update in `atomic_threads` itself: its `fetch_add` (one
read-modify-write, one step) becomes a `load` and a `store` (two steps with a window between them). Two
threads count to 400; a thread preempted between its load and its store writes back a stale value. The
explorer sees it only through the points step 6 puts in the PROGRAM's own IR, and that was MEASURED, 100
seeds each at the measured k: the planted source linked with its IR untransformed, 100 of 100 exit 0 (k 115:
each thread's loop runs from one floor step to the next with no point inside it, so no schedule can split
a load from its store); transformed, found at seed 1 and in 55 of 100 (exit 1, a total short of 400; k 917).

## The spec control (step 5, 2026-09-18)

`unconditional-apartness.ctl` plants 1.5.6's unconditional apartness clause for `npk_small_free`'s three
list neighbours back into the spec — the clause 1.5.6c found FALSE by reading (the ordinary free into a
chunk already on the partial list, whose head IS that chunk) — and requires `ASSUMPTION` within two
seeds: `drop_string` reports `ASSUMPTION @npk_small_free: (apart (partial-list head) (chunk))` on its
27th step, every seed. What reading found after a year, the entry checker finds in the first millisecond
of any program that frees a small block; the control is the mechanism's proof that it would.

## The nineteen model controls, walked (step 4, 2026-09-18)

Every control of `runtime/models/*.model` was applied to the floor's text and measured against an
explored program, with the shim of step 3 and then with the shim as amended by the walk (X-14, X-16).
Three findings about the INSTRUMENT came out of the walk before any control could be judged: a wait
whose absolute deadline had already passed blocked virtually until quiescence where the kernel returns
`ETIMEDOUT` at once (X-14 — the shim reported lost wakes that were its own); a fixed fairness bound
resonates with a periodic thread, which then rests at the same phase on every slice (X-16 — a control
was blind for the shim's reason); and windows one point wide need a directed change point (X-15).

Eleven are `.ctl`s in this directory. Eight are recorded below: five NOT A FLOOR BUG (the floor has a
second mechanism the model does not represent, or the bad state is unreachable by the language's
discipline), one NOT OBSERVABLE by any lens the explorer has (the model's), and two NOT EXPLORABLE (the
driver programs are real-child, `explore: no`, by the plan's own rule §2.8). Each record names the
measurement; each "not a floor bug" is also a dated note in the model's own file. The walk's harness then
found a floor defect none of the nineteen could name, DEF-57, and the `trap-route` model gained two
controls for it; both are recorded in the last section.

| model | control | outcome | measured |
|---|---|---|---|
| futex-mutex | store-release | **`.ctl`** (step 3) | LOST-FUTEX-WAKE, `mutex_basic`, 95 of 100 seeds, first at 2 |
| futex-mutex | wait-expecting-one | **`.ctl`** | LOST-FUTEX-WAKE, `mutex_basic`, 95 of 100, first at 2 — the first two-pair control |
| park-unpark | drop-second-sweep | not a floor bug | below |
| park-unpark | clear-after-sweep | not a floor bug | below |
| park-unpark | drop-keep-path | **`.ctl`** | `late 100000000`, `channel_threads`, 62 of 1000 (11 of 100 under the final shim), first at 5 |
| park-unpark | drop-epoll-recheck | not a floor bug | below |
| reactor-io | drop-due-stamp | **`.ctl`** | `wrong-exit`, `io_ready_basic`, 100 of 100, from seed 1 |
| reactor-io | free-before-unwatch | not observable | below |
| reactor-io | drop-duenow | **`.ctl`** | `wrong-exit`, `io_ready_declined` (new), 100 of 100, from seed 1 |
| channel-table | count-before-pointer | not a floor bug | below |
| channel-table | drop-recheck | not a floor bug | below |
| shared-arena | index-read-then-write | **`.ctl`** | `wrong-exit`, `shared_arena_race` (new), 51 of 500, first at 24 |
| shared-arena | link-without-cas | **`.ctl`**, directed | `wrong-exit` (STEP BUDGET), `shared_arena_race`, 27 of 100 directed, first at 4; 0 of 500 blind |
| driver-registry | plain-store-claim | not explorable | below |
| driver-registry | publish-before-prefill | not explorable | below |
| trap-route | old-arbitration | **`.ctl`**, directed | `wrong-exit` (exit 77 on every seed), `trap_one_failsafe` (new), 100 of 100 directed at the frozen store and the holder write, from seed 1; 0 of 100 blind — re-directed after DEF-57 (last section) |
| trap-route | no-stop | **`.ctl`** | `wrong-exit`, `trap_stops_runner`, 100 of 100, from seed 1 |
| trap-route | old-loser-exits | **`.ctl`**, directed | `wrong-exit` (exit 70 on every seed), `trap_two_threads`, 100 of 100 directed at the frozen store and the stop walk's first read, from seed 1; 0 of 100 blind — re-directed after DEF-57 (last section) |
| trap-route | heap-mutex-in-failsafe | **`.ctl`** | `wrong-exit` (DEADLOCK), `failsafe_alloc`, 36 of 100 from seed 1 — 0 of 100 before X-16 |

Four programs were written for the walk because no explored program had the shape a control needed,
and each is now an explored program of the suite in its own right: `io_ready_declined` (a watch the
kernel declines, `drop-duenow`), `shared_arena_race` (every slot re-read after both bumpers are done,
behind a two-party barrier — `shared_arena_spawn` checks each value right after its alloc and never sees
an overwrite), `trap_one_failsafe` (a `failsafe` that marks a pipe and finds the mark a second
`failsafe` left — two `failsafe`s exit 41 as one does), and `reactor_arm_race` (an executor arming its
reactor while a channel waker on another thread rouses it — the shape `drop-epoll-recheck` names, where
`no-rouse` is found 84 of 200 and the dropped re-check never absorbs a notification).

### Model controls measured and found NOT to be floor bugs

**`park-unpark` / `drop-second-sweep`** and **`clear-after-sweep`** — the executor's idle path clears
its park word, sweeps the sleeper list a second time, re-checks the ready queue, and only then waits;
the floor's comment (1.1.10-C2) calls the order "the whole of it". Dropping the second sweep, or
clearing the word after it, lets a waker's stamp land between the sweep and the wait with its rouse
erased — the model's wake-before-sleep, reachable there because the model has no DEADLINE. The floor
has one: `npk_sl_earliest` reads every sleeper's `wake_at`, a due stamp is the value 1, so the wait it
computes has an absolute deadline of 1 ns and the kernel returns from it at once (X-14); the next
`npk_step` sweeps the due task. The clear-then-recheck protocol is a first line of defence and the
stamp-as-deadline a second, and the model represents only the first. Measured on `channel_threads`
under the amended shim: `drop-second-sweep` 0 LOST-WAKE in 200 seeds and 0 `late 100000000` in 1000;
`clear-after-sweep` 0 in 100 and 0 in 1000; the virtual run time in the clean floor's own range (205 µs
to 606 µs) on every seed. (Under the step-3 shim `drop-second-sweep` showed 3 LOST-WAKEs in 100 seeds:
the SHIM's expired-deadline sleep, X-14, not the floor's.) Recorded in the model.

**`park-unpark` / `drop-epoll-recheck`** — an armed executor's epoll wait re-checks the park word by
hand before `epoll_pwait`, "or a rouse that landed between the caller's re-check and here — with the
ping skipped on a not-yet-visible evfd — sleeps through a due task". Under the explorer's sequentially
consistent interleaving the absorbed notification cannot happen: a rouser stores the park word BEFORE
it reads the eventfd word (`npk_ch_wake_one` and `npk_windup_all` both), and an executor arms (stores
the eventfd word) BEFORE it clears the park word, since the arming is a task's `io_register` and the
clear is the idle path after that task parked. For the notification to be absorbed with the re-check
gone, the rouser's store must follow the clear (else the clear erases it and the post-clear sweep
finds the stamp that preceded it) and its eventfd read must precede the arming (else it pings); with
arm → clear → store → read in one total order that is a contradiction. The visibility the comment
worries about is weak memory, which the shim does not explore (X-8) and which x86-64's `xchg` for a
`seq_cst` store orders anyway. Measured on `reactor_arm_race`, written for this shape: 0 LOST-WAKE in
1000 seeds and 0 `late 20000000` in 1000, where `no-rouse` on the same program is found 84 of 200.
Recorded in the model.

**`channel-table` / `count-before-pointer`** — publishing the count before the slot's pointer lets a
reader that saw the count read an unwritten slot, in the model, whose reader holds the handle of a slot
still being opened. In the floor a handle is MINTED by the open that publishes its slot: the fresh path
returns the packed (generation, index) after the count's release store, and the revive path reuses an
index under the channel's lock, moving its generation, with the slot pointer published long before. No
program can hold a fresh slot's index before its count is published, so no reader can read the torn
slot. Measured with the count published first on `chan_reclaim_race`, `channel_reclaim`, `chan_loop`
and `channel_threads`: 0 wrong exits in 300 seeds each. Recorded in the model.

**`channel-table` / `drop-recheck`** — two reclaimers of one handle, the second taking again because
its `ch_get` preceded the first's take; the re-check under the channel lock is what stops it. A
program cannot spell a reclaim: it is the creating function's scope exit (D-183), exactly once per
channel, or the caller's under `gives` (1.2.6) — still once. The stale-handle race a program CAN
reach (`chan_reclaim_race`: a send against a reclaim) goes through `ch_get`'s generation compare, not
through a second reclaim. Measured with the re-check dropped on the same four programs: 0 wrong exits
in 300 seeds each. Recorded in the model.

### A model control the explorer has no lens for

**`reactor-io` / `free-before-unwatch`** — the frame dies before its registration does, and a later
event stamps a due mark into freed memory (the model's `stamp-after-free`). The order is the
PRELUDE's (`io_ready`'s `defer { io_unwatch(iofd); }`, which every exit of the wait runs), not a line of
the floor; the nearest floor mutation, `npk_io_unwatch` made a no-op, leaves a fired one-shot disarmed
and an unfired one armed with a dead frame as payload, and its eventual stamp is a write into freed
memory that no exit code, no clock and no stamp oracle can see. Measured on `io_ready_basic`,
`text_pipe`, `streams_pipe` and `reactor_arm_race`: 0 wrong exits in 300 seeds each, the virtual run
times unmoved. The property is the model's (`stamp-after-free` unreachable, a floor-model row on every
run) and the correspondence belt's (the model's `unwatch` step anchored to the floor's blocks); the
explorer does not claim it.

### Model controls with no explorable program

**`driver-registry` / `plain-store-claim`** and **`publish-before-prefill`** — the registry is written
only by `npk_clone_exec`'s spawn of a driver, and every program that spawns one is a real-child program
(`// explore: no real child processes`, §2.8 of the plan: a virtual clock cannot share a real child's
real time). The two controls are the models' alone until a driver can be explored, which is not
planned: the ten driver programs run under `// stress:` on the real kernel, and the model's rows decide
the registry's claim and publish on every run.

### Two controls DEF-57 added to `trap-route` (step 4's own find, 2026-09-18)

The walk's first full harness found what no control had planted: `trap_one_failsafe` exited 96 at seed
371, where it expects 41. `npk_trap` publishes `@npk_frozen` before it claims the failsafe holder. An
executor that read the flag in that window answered it, in `npk_step`'s `frozen:` block, by trapping
`Unreachable` itself, and it could WIN the holder: `failsafe` ran with the wrong error. The model had no
error code, so every bad predicate it had stayed unreachable. The fix parks a watcher and keeps the
holder's re-entry exit 70 (OPEN_DECISIONS §2f has DEF-57 whole). The model now carries each thread's code
and two controls:

| model | control | outcome | measured |
|---|---|---|---|
| trap-route | frozen-traps | the unit's own seed (X-11), not a `.ctl` | `trap_one_failsafe`, seed 371, k 106,239 on both floors: exit 72 (the `Unreachable` arm) with the pre-fix block planted by hand on the fixed floor, 41 on the fixed floor; the harness's first run said 96, when that arm shared `IntOverflow`'s exit |
| trap-route | frozen-parks-holder | not observable | the model's (below) |

**`trap-route` / `frozen-traps`** — the old `frozen:` block: the executor that saw the flag enters the
arbitration with `Unreachable`, and `wrong-error` is reached at depth 6 in the model. On the floor the
window is ONE POINT WIDE (the point before the trapper's cmpxchg), and both parties' next step is the SAME
site, the claim. A directed site cannot order them: it demotes each arrival below every thread already
waiting, so the trapper, which arrived first, always claims first. That is a limit of X-15's sight, stated
here: a directed control can hold a thread at a place, but it cannot reorder two arrivals at one place.
Blind, the unit's own run met the window first at seed 371 of its 1,000, and that seed is now the unit's
`// explore-seed: 371`, run first on every run (X-11). The schedule replays up to the moment the watcher
reads the flag, which is exactly where the fixed block differs, and it exits 41 on the fixed floor. A
`.ctl` pinned to that seed would go blind at the first unrelated change to the route's step count, which
would be a red run that says nothing about DEF-57, so the model's control is the standing negative control:
both of the model's readings decide it on every run.

**`trap-route` / `frozen-parks-holder`** — the fix as first proposed at the hand-off: EVERY executor that
sees the flag parks, the holder included. `holder-parks` is reached at depth 6 in the model (the holder's own
`failsafe` meets the flag and parks, and nothing is left to end the process). No program can reach that
state: `failsafe` is never `async` (TYPE-043), so it cannot `await`, spawn or join, and nothing it calls
drives the executor. The explorer has no program for the control, and the model's rows are the evidence.

**Two of the walk's directed controls had been found THROUGH DEF-57.** On the fixed floor `old-arbitration` and
`old-loser-exits`, directed as the walk measured them (100 of 100 each), were BLIND: 10 seeds each, exit 41 on
every one. In their directed schedules the second claimant or the loser had never been a trapper. It was the
other thread before its task started: its executor saw the flag, trapped `Unreachable` on it, and so entered
the planted arbitration — DEF-57's window, which the fix closes. The two trap programs put both threads' traps
inside their tasks, so a second REAL trap needs the other task RUNNING when the first thread traps. Both
controls now also name the first trapper's frozen store (`preempt-at: @npk_trap store atomic i32 1, ptr
@npk_frozen`): it yields BEFORE publishing the flag, the other thread's task starts and runs to its own trap,
and each control's own window follows. Measured on the fixed floor, 100 seeds each: `old-arbitration` exit 77
on every seed (two `failsafe`s ran), `old-loser-exits` exit 70 on every seed (the planted re-entry arm), both
from seed 1. The other ten controls in this directory (the walk's nine, and step 3's `no-rouse`) were re-run on
the fixed floor through the harness's own code path and found at their recorded seeds. The lesson is the instrument's: a control proves it SEES the planted
defect only if the schedule that finds it goes through that defect. These two found the right verdict by a
route through a different defect, and nothing looked at the route.
