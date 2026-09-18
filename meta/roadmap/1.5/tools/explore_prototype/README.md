# The schedule explorer's planning prototype — a BEHAVIOURAL REFERENCE, outside every gate (D-303, 1.5.7)

**What this is.** The throw-away prototype `nitpick-compiler_s7` built on 2026-09-17 to MEASURE 1.5.7's design
before it was a plan (`meta/roadmap/1.5/1.5.7.md` §1 carries the numbers): a text transform of the real floor
(`transform.py` — a scheduling point before every atomic step line, every `@npk_sys6(` call routed to a shim,
the thread lifecycle hooked) and a C shim (`npkx.c`) that runs one thread at a time under a baton, virtualizes
the futex, `epoll_pwait` and the clock, schedules by PCT with ordered priority bands and a fairness bound, replays
a schedule from its seed, and reads two quiescence oracles off the executor's real state. `build.sh` builds one
program against the transformed floor and the shim with clang; `sweep.sh` runs every `// stress:` program under N
seeds. It found, before a line of the real design was written, that a lost wakeup in this runtime is LATENESS an
exit code cannot see, that PCT needs a fairness rule and ORDERED bands, and that a model's control is not always a
floor bug.

**What it is for now.** The shim that ships with the tree is hand-written LLVM IR (`runtime/explore/npkx.ll`,
D-298; Nitpick cannot express it — no mutable module state under D-211, and code inside the floor's critical
sections may not allocate, trap or call the floor). Porting ~375 lines of C into IR by hand is a REVIEW unless
something holds the port to the original; this prototype is that something: at 1.5.7 step 1 the IR shim is held to
it SCHEDULE HASH FOR SCHEDULE HASH over the explored units — the same seed, the same step count, the same hash —
which turns the port into a measurement. It stays here afterwards as the reference any later change to the shim is
measured against. `transform.py` is superseded by the one transformer, `npkg/explore.npk` (both runners hold that
transformer's output to a literal in their self-checks); it is kept because `build.sh` and `sweep.sh` call it.

**What it is NOT.** It is not built by any gate, not linked into anything, not run by either runner, and not part
of the artifact or of the trusted computing base: the zero-dependency rule (CLAUDE.md, TCB.md) governs what SHIPS,
and this ships nowhere — the same standing as `tests/backend/fixtures/*.c`, the reference drivers of 1.1.13c
(D-149: test tooling outside the TCB). The user's condition on keeping it, 2026-09-17: "If it's just an extra
layer of verification for us that doesn't get shipped then i don't see a problem with it." Nothing under `src/`,
`runtime/`, `lib/`, `npkg/` or `tools/` references this directory.

**Amended once, at 1.5.7 step 1, and recorded here.** The port of this shim to IR found that the C did not replay
exactly under machine load: `npk_chunk_new` over-maps and trims, and whether its pre-trim `munmap` happens depends
on the alignment of `mmap`'s answer, so a step count -- and with it every PCT change point and the schedule hash --
depended on an ADDRESS. The plan's "exact replay" had been measured on one program whose mappings happened to
align. Both shims now place an anonymous mapping with no hint at a bump pointer (16 TiB, 64 KiB-aligned) with
`MAP_FIXED_NOREPLACE` (X-13 in the plan); the amendment is the one block marked in `npkx.c`. `hashcmp.sh` beside
it is the measurement the IR shim is held to.

**Amended a second time, at 1.5.7 step 4 (X-14), and recorded here.** Walking the models' controls found that both
shims BLOCKED a futex wait whose absolute deadline had already passed, until quiescence, where the kernel arms an
hrtimer that has already expired and returns `ETIMEDOUT` at once. The floor's `npk_sl_earliest` reads a due stamp
(`wake_at` = 1) as a deadline of 1 ns, so an executor whose second sweep is dropped (`park-unpark`'s
`drop-second-sweep`) sleeps for no time at all on the kernel and forever on the old shim -- where the LOST-WAKE
oracle then reported a lateness the kernel never has. Both shims return `-110` before parking when the deadline is
at or before virtual now (a relative timeout of zero included); the amendment is the second block marked in
`npkx.c`, and the sweep was re-run after it.

**Running it by hand** (a measurement, never a verdict):

```
python3 meta/roadmap/1.5/tools/explore_prototype/transform.py runtime/npkrt.ll /tmp/x/npkrt.explore.ll /tmp/x/sites.txt
bash meta/roadmap/1.5/tools/explore_prototype/build.sh "$PWD" tests/backend/programs/mutex_basic.npk /tmp/x
(ulimit -n 1024; NPKX_SEED=1 NPKX_TRACE=1 /tmp/x/mutex_basic.x < /dev/null; echo "exit $?")
bash meta/roadmap/1.5/tools/explore_prototype/sweep.sh "$PWD" /tmp/x 100
```

`build/npkc` must exist (`npkg build`). Environment: `NPKX_SEED` (the schedule), `NPKX_POLICY` (0 PCT, 1 random
walk), `NPKX_D` (PCT depth, 3), `NPKX_K` (the step estimate, 2000), `NPKX_FAIR` is the compiled-in 4,096,
`NPKX_BUDGET` (the step budget), `NPKX_TRACE` (1: the steps and the schedule hash at exit), `NPKX_ORACLE` (0 turns
the quiescence oracles off — for comparison only). The two `npkx_chk_*` hooks are the hand-written measurement of
1.5.7 §2.4 (the spec's hypotheses executed at `npk_small_free` and `npk_rq_push`); the generated checkers of step 5
replace them.
