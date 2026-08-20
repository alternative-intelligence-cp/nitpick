# Cycle 1.1 — Async and concurrency

**Phase C.** Coroutine lowering, per-thread executors, channels, and the D-071
suspension model. This is the cycle the whole concurrency spec set (D-032/34/56/58/
62/63/71/72/73/82/83) has been written toward.

> Detailed **map**, not a full subcycle plan. Its subcycles are written when reached,
> once its gating decisions settle. It carries the largest decision load of any
> Phase-C cycle and a **hard dependency on cycle 0.10**.

## Hard dependency: cycle 0.10 must be done first

D-034: "each thread's executor owns an arena from which it allocates task frames."
1.1 **cannot build its executor without 0.10's frame allocator** (0.10.3, the one
distinct from surface `arena<T>` — see that subcycle for why the conflation would
otherwise block this cycle mid-flight). Channels, actors, and thread pools sit on
`atomic<T>` (minimally lowered at 0.10.4; generalized here under C-9) and the
arenas (0.10.2/0.10.4). If 1.1 is ever pulled ahead of
0.10, 0.10 moves with it.

## Decisions in (see `../OPEN_DECISIONS.md` §2)

- **B-2 — `Duration`, monotonic clock, executor timers.** *Blocks the cycle's start.*
  Safety, not comfort — deadlines are the language's entire residual-deadlock
  containment. Nothing in 1.1 can be built while the deadline type it all takes is
  undefined. This is the **first act** of the cycle.
- **C-7 — coroutine lowering** (coro ABI, suspend/resume, spawn/join bookkeeping,
  wind-up token) — depends on 0.10.3's frame allocator interface.
- **C-8 — narrow the borrow-across-await rule** (the current blanket rule makes the
  async I/O surface unwritable; narrow to no-borrow-across-**spawn**). Also closes the
  borrow-checker deep dive's obs. #1.
- **C-9 — construction & threading APIs** (channel/executor/thread/actor/pool
  creation, `Job`'s representation, CondVar handoff, async trait methods, `atomic`'s
  permitted-`T`/return-types).
- **B-3a — io_uring vs epoll** (decide the initial reactor; scope whether 1.1
  includes the file/socket reactor at all, or only futex-parking + timers + channels).

## Subcycle shape (to be filled when reached)

| # | Topic | Gated on |
|---|---|---|
| 1.1.0 | **`Duration` + the clock + timers** — the deadline substrate; `CLOCK_MONOTONIC` through the floor; a pinned `DEADLINE_EXCEEDED` | B-2 |
| 1.1.1 | **Coroutine lowering** — `@llvm.coro` state machines over 0.10.3's frame allocator; `await` yields `T`; `drop work()` spawns | C-7, 0.10.3 |
| 1.1.2 | **The executor** — run queue, futex park/wake, the D-063 stop-the-world trap hook, `async func:main` over the entry shim | C-7 |
| 1.1.3 | **Task lifetime & the join** — D-062 lexical join with the mandatory deadline; the wind-up token; the borrow-across-await narrowing (C-8) | C-8 |
| 1.1.4 | **Channels** — `Channel<T,LEVEL,CAP>` over §6; the construction API (C-9); no `select`, deadline-mandatory `recv` | C-9 |
| 1.1.5 | **Sync primitives** — `Mutex<T,LEVEL>`/`RwLock`/`CondVar` (timed only)/`Barrier` over the lock-level analysis (already built, 0.5.6) | C-9 |
| 1.1.6 | **Actors & thread pools** — tasks-with-mailboxes and N-workers-one-channel; the `Job` representation (C-9) | C-9 |
| 1.1.7 | **The reactor** — epoll+timerfd (or the B-3a choice); the in-flight-buffer ownership rule; file/socket streams over the IO_REFERENCE traits | B-3a |

## Watch for

- **The async I/O traits vs C-8.** `Reader`/`Writer` are `async` traits consumed as
  `dyn Writer` — so 1.1 needs 1.0's `dyn` *and* the C-8 borrow narrowing *and* a story
  for a virtually-dispatched coroutine's frame sizing (genuinely hard; part of C-9).
  This is the densest interaction in the plan; sequence the reactor (1.1.7) last so
  channels and the executor core do not wait on it.
- **The trap path grows a stop-the-world step** (D-063) ahead of `failsafe`. It uses
  `tkill`/`futex`/`set_robust_list` (the floor's syscall surface) and must be bounded
  (a thread not parked within the deadline is reported still-running, not waited on
  forever). This is safety-critical for the robotics path and is easy to under-build.
- **`failsafe` stays non-async** (D-063) and runs on the trapping thread with every
  executor already stopped — do not let the executor work tempt an `async failsafe`.
