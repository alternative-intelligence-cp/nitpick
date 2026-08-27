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

- **D-163 — `raw` and `drop` are licensed by `never fails`; a `Result` is never
  discarded without a keyword; `never fails` is checked.** Decided at 1.0's boundary
  (G-4); *implemented as this cycle's first three subcycles*, before any executor
  code exists to be swept. Its rule 4 fixes a requirement on C-7 / C-9: a spawned
  task's error reaches the D-062 join.
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
  **SETTLED at 1.1.12a as D-184** — epoll without timerfd; io_uring refused
  before Astrée by decision.

## Subcycle shape (to be filled when reached)

| # | Topic | Gated on |
|---|---|---|
| 1.1.0 | **`never fails` on every function, checked; the statement forms closed** — the grammar widened one production, the func-type flag, the body check, trait conformance, the `defer` rule, the builtin `fails` column, the seed taught the word, 188 bare statements keyworded, `check_raw_licensed` REPORTING | D-163 |
| 1.1.1 | **The `src/` sweep** — ~868 clauses, ~1,000 may-fail sites resolved at the root or rewritten (`relay` / `?!` / handled), ~90 `discard(raw …)`, the driver's stage calls relayed; instrument at zero for `src/`/`lib/`/`tools/` | 1.1.0 |
| 1.1.2 | **The `tests/` sweep, then the refusal** — the suites licensed, the flip to `TYPE_RAW_UNLICENSED` (with the spawn-form exemption), the instrument retired, fixpoint re-proven; D-163 SETTLED | 1.1.1 |
| 1.1.3 | **`Duration` + the clock + timers** — the deadline substrate; `CLOCK_MONOTONIC` through the floor; a pinned `DEADLINE_EXCEEDED` | B-2 |
| 1.1.4 | **Coroutine lowering** — `@llvm.coro` state machines over 0.10.3's frame allocator; `await` yields `T`; `drop work()` spawns | C-7, 0.10.3 |
| 1.1.5 | **The executor** — run queue, futex park/wake, the D-063 stop-the-world trap hook, `async func:main` over the entry shim | C-7 |
| 1.1.6 | **Task lifetime & the join** — D-062 lexical join with the mandatory deadline; the wind-up token; the borrow-across-await narrowing (C-8) | C-8 |
| ~~1.1.7~~ | *(planned as Channels; the numbering moved — see below)* | C-9 |
| 1.1.8 | **The executor** — DONE. Run queue, sleepers, futex park/wake, wind-up | C-7 |
| 1.1.9 | **Threads (D-181)** — DONE. `thread` functions, `joins`, `clone(2)`, per-thread executors | C-9 |
| 1.1.10 | **`atomic<T>`, channels, pools, actors** — DONE, four stages A–D. See `1.1.10.md` | C-9 |
| **1.1.11** | **Sync primitives** — `Mutex<T,LEVEL>`/`RwLock`/`CondVar` (timed only)/`Barrier` over the lock-level analysis (already built, 0.5.6) | C-9 |
| **1.1.12** | **The reactor** — **a: DONE (D-184, B-3a closed)**: epoll WITHOUT timerfd — `epoll_pwait` as the armed executor's idle wait, carrying the sleeper deadline; eventfd as the cross-thread wake channel; EPOLLONESHOT with the task frame as payload; `suspend_io` builtin + prelude `io_ready` + deferred `io_unwatch` (registration lives exactly as long as the wait); ctl-failure = due-now, the caller's retried syscall reports; **the task-identity rule** — every waiter registration (channels and locks included) resolves `cur_task`, closing a latent nested-wait lost-wakeup (`nested_wait.npk`); `sys` takes pointers (`ptrtoint` at the trampoline). io_uring refused before Astrée by decision. **Next — b:** owned-fd byte streams; **c:** text layer + std streams; **d:** async-methods-behind-dyn | B-3a |
| **1.1.13** | **The Bridge (D-149)** — `DeclExternBlock` lowers to driver stubs (marshal into the sealed ring, deadline dispatch, unmarshal-or-error); the interface hash and connect handshake; the `Driver` trait and the `failsafe`-reachable registry; the checker's refusal of D-002-era `fails on` contracts; the C SDK header and wire-conformance suite grown from the v3 POC (`../audit-0.8-close/driver_architecture_plan_v3.md`) | D-149 |

**THE NUMBERING MOVED, AND THIS TABLE NOW RECORDS WHAT HAPPENED** rather than
what was guessed. The plan above was written before the cycle started and put
channels at 1.1.7 with sync primitives, actors and pools after them. What the
work actually needed was different, and in an order the plan could not have
known: the typed-error system (D-179) was inserted at 1.1.5–1.1.7 so that the
executor's failures would be born named, which pushed everything down three;
and actors and pools turned out to be *tests* of the channel machinery rather
than a subcycle of their own — D-182's whole claim about them is "no new
primitive, no new runtime", and a subcycle that builds nothing is a subcycle
that should not exist. Sync primitives, the reactor and the Bridge are what
remain, renumbered 1.1.11–1.1.13.

The next one is **1.1.11, sync primitives**, and it starts closer to done than
it looks: 1.1.10-C1 built a three-state futex mutex in the runtime for the
channel's own ring, so the parking substrate a `Mutex<T, LEVEL>` needs already
exists and is exercised by a two-thread test. What 1.1.11 adds is the TYPE —
`Mutex<T, LEVEL>` owns its data (D-056), acquisition is `async` and
deadline-bounded, and the guard's scope is what releases it, which is the one
piece that leans on the managed lowering (B-6) and must be looked at before it
is built rather than after.

## Watch for

- **Every function this cycle writes declares its contract at birth.** An executor
  or channel method that cannot fail says `never fails`; one that can is `raw`ed and
  `drop`ped by nobody. The Bridge's generated stubs are may-fail by construction
  (D-149) and must not carry the clause. The D-062 join is designed against D-163
  rule 4: a spawned task's error is observable or the program does not compile.
- **The async I/O traits vs C-8.** `Reader`/`Writer` are `async` traits consumed as
  `dyn Writer` — so 1.1 needs 1.0's `dyn` *and* the C-8 borrow narrowing *and* a story
  for a virtually-dispatched coroutine's frame sizing (genuinely hard; part of C-9).
  This is the densest interaction in the plan; sequence the reactor (1.1.10) last so
  channels and the executor core do not wait on it — and the Bridge (1.1.11) after
  the reactor, since a dispatch await parks on the reactor's pollables and timers.
- **The Bridge is where D-149 stops being a rung message.** Until 1.1.11 lands,
  `extern` blocks refuse at the backend naming this cycle; after it, they are the
  ONLY foreign-code mechanism the language has ever shipped — there is no
  in-process era to migrate anyone off. The C reference driver and the
  wire-conformance suite are buildable out-of-tree before the cycle starts; the
  protocol is language-independent and POC-validated (18/18).
- **The trap path grows a stop-the-world step** (D-063) ahead of `failsafe`. It uses
  `tkill`/`futex`/`set_robust_list` (the floor's syscall surface) and must be bounded
  (a thread not parked within the deadline is reported still-running, not waited on
  forever). This is safety-critical for the robotics path and is easy to under-build.
- **`failsafe` stays non-async** (D-063) and runs on the trapping thread with every
  executor already stopped — do not let the executor work tempt an `async failsafe`.

## 1.1.11a — `Mutex<T, LEVEL>` and `Guard<T>`: the release IS the closing brace

Builtin type kinds like `Channel`: one managed cell per mutex, the value a
pointer, `acquire` the sole operation — `async`, deadline-mandatory,
`Result<Guard<T>>` — and the guard's scope-exit DROP is the release, the
first drop in the language that frees no memory and the thing cycle 1.2
existed to make possible. `guard.value` is the element's place through the
cell. The waiter protocol reuses the channel's; `mutex_basic.npk` runs two
real threads to an exact 200 under stress and watches a 1ms acquire on a
held lock answer DeadlineExceeded.

Landing it surfaced four buried edges, each a rule earning its keep: the
keyword table is LENGTH-BUCKETED (Mutex/Guard sat unreachable in the
7-bucket); `acquire` was already RESERVED for this operation and a keyword
token carries no intern, so the dot-name position now interns its spelling;
the await checker's intrinsic-suspension exemption (channels') extends to
the mutex; and D-180's borrow-across-spawn refusal exempts `Mutex<T,L>->` —
the hazard it names is mutation the holder cannot see at a suspension it did
not choose, which is answered by the lock itself, so the mutex borrow is the
sanctioned crossing while `Guard->` stays refused. The lock-level analysis
now reads `m.acquire(…)`'s level off the receiver's TYPE — the 0.5.6
doctrine's own fix arriving — and lock_levels.npk pins holding 5 while
acquiring 3 as LOCK-001 with no clause at the site.

RwLock, CondVar (timed only) and Barrier are 1.1.11's open half.

## 1.1.11b — RwLock, CondVar, Barrier: the table is complete

`RwLock<T, LEVEL>`: `read` returns `RGuard<T>` — shared, `.value`
read-only, its drop releases a reader — and `write` returns the same
`Guard<T>` a mutex does, because an exclusive hold is one meaning
everywhere; the cell carries a KIND so one `npk_guard_release` picks the
wake policy (mutex wakes one, writer release wakes the crowd of readers).
`CondVar<LEVEL>`'s `timedwait` LENDS the guard, releases link-first (no
lost signal), reacquires under the same absolute deadline — and on
DeadlineExceeded the guard is SPENT: holding a lock past an expired
deadline is the unbounded acquire D-056 forbids, so the lent guard's cell
is nulled and the caller re-acquires. POSIX reacquires unboundedly;
Nitpick deliberately does not. `Barrier<N, LEVEL>` gained the LEVEL the
spec table omitted (D-056's own sentence corrects it), and a timed-out or
wound-up party hands its slot back so the next round completes.

The ordering analysis split HOLDS from WAITS: binding a guard raises the
held level for the rest of the block; binding `arrive`/`timedwait`'s
`Result<NIL>` raises nothing — the first sync_prims draft was refused by
its own analysis for holding a level no binding held. sync_prims.npk runs
two threads reading concurrently (700 exact), the classic
predicate-loop wait/signal, and the barrier timeout-then-complete round,
under stress. Two more reserved words bit on the way (`gid`, and cat5's
arity twice); the reserved-word table keeps growing for a reason.
