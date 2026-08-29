# Digest of r6.md — async-runtime defect classes and verification (source: meta/roadmap/research/r6.md)

> Extraction from "Architectural Resiliency in Cooperative Asynchronous
> Runtimes: A Defect Taxonomy and Verification Guide". Incident corpus is
> Tokio-heavy (no async-std/smol incidents despite the class names); the
> two formulas are embedded images in the source — decoded: the PCT bound
> is `1/(n·k^(d−1))`.

## 1. The defect taxonomy (with the report's incident citations)

| Class | Mechanism | Incident |
|---|---|---|
| Wake-before-sleep race | worker's final sweep sees empty queue, commits to park as another thread enqueues; enqueue not ordered before the sleep decision | tokio#1768 "Runtime gets stuck" |
| Lost wakeup via ordering | notifier's CAS fails against a concurrent SLEEP→IDLE and returns WITHOUT waking; missing explicit Acquire ("SeqCst was assumed sufficient") | tokio#525; rust-lang#53366 (std park/unpark ordering) |
| Absorbed notification | remote notify pushed the task but never woke the parked local-set driver ("assumed it was actively polling") | tokio#2057 |
| Waker contract violation | Pending returned without storing the waker; later completions notify no one — "complete and irrecoverable deadlock" | tokio#5835 (JoinSet::join_next on empty set) |
| Spurious-wake mishandling | the contract permits polls with no event; futures must conservatively re-check readiness or corrupt state | futex(2) semantics |
| Cancellation leak | waker CLONES hold strong refs on the whole task allocation; cancelled tasks not freed until every external clone drops — "tens of gigabytes daily" in production | tokio#5861; tokio#7884 (oneshot retains rx waker) |
| Inconsistent teardown | hierarchical cancellation observable mid-propagation (one child cancelled, sibling not) | tokio-util cancellation_token |
| Cooperative starvation / budget livelock | non-yielding combinator spins through a timer's wakeups forever ("CPU-pinning wake-up livelock"); per-task budgets (~256 ticks) can induce it | tokio#7883; discussion #6175 |
| Suspend clock drift | epoll/timers on CLOCK_MONOTONIC pause across device suspend; timeouts never fire after wake; fix (CLOCK_BOOTTIME) "a non-trivial architectural shift" | tokio#3185 |
| Channel ABA at generation wraparound | too-narrow generation tags wrap under throughput; a woken thread's CAS succeeds on a recycled slot, "deeply corrupting the channel state"; x86 CAS vulnerable, LL/SC natively immune | (mechanism sources only; no named incident) |

## 2. The lost-wakeup protocol literature

**The prepare-to-park / recheck / park discipline** (mandatory phases):
1. PREPARE: store the waker; transition state with **Release**.
2. RECHECK: re-validate the condition with **Acquire** (or SeqCst) so all
   concurrent notifiers' writes are visible.
3. PARK: only then `futex_wait` — the kernel's atomic compare of the futex
   word closes the final window (a change between recheck and park returns
   EAGAIN and aborts the sleep).

**Futex formalization**: Drepper's "Futexes Are Tricky" — the ternary state
machine (0 unlocked / 1 locked / 2 locked-with-waiters), the hard part
being non-owner mutation (1→2 visible mid-unlock). Modern kernels still
return EAGAIN/zero requiring unconditional re-evaluation on wake. eventfd
drain races: broadcast-woken workers race the read; losers must absorb the
spurious wake without assuming work exists.

**AtomicWaker — the named known-correct reference protocol** for
task-migrating consumers: WAITING(0)/REGISTERING(1)/WAKING(2);
`register` = CAS WAITING→REGISTERING (Acquire), write waker, back to
WAITING (Release); `wake` = fetch-or WAKING (AcqRel); **if a wake lands
during a register, the registering thread detects the WAKING bit and takes
responsibility for waking the task itself.** tokio#8240 is the wait-free
refinement. Reference implementations: futures::task::AtomicWaker, the
atomic-waker crate (1.1.2, audited), tokio oneshot.rs.

## 3. Model-checking precedents

- **Chase-Lev work-stealing deque**: TLA+ plus Iris/Coq separation logic
  "absolutely required" for the bottom/top interaction; the formal work
  isolated a real bug — `take` on empty decrements unsigned `bottom` →
  underflow → concurrent `steal` sees a gigantic deque and reads corrupted
  memory. Weak-memory re-derivations exist (C11 Acquire/Release).
- **The practical limit**: state-space explosion; DPOR is UNSOUND to
  terminate on cyclic state spaces (spin loops — ubiquitous in runtime
  code); the sound configuration is preemption-bounding (BPOR, OOPSLA'13).
- **The verdict**: TLA+-class checking is proven at the PRIMITIVE
  granularity (AtomicWaker, park/unpark, one channel); "exhaustively
  modeling an entire hand-written executor end-to-end remains
  computationally infeasible." Model the primitives, not the executor.

## 4. Tools comparison

| Tool | Exploration | Record | Applicability to a hand-written executor |
|---|---|---|---|
| loom | exhaustive, all weak-memory permutations | "100% detection for isolated primitives" | scales only to small primitives; needs mocked-primitive builds |
| shuttle (AWS) | randomized (PCT-class), deterministic replay | built for real codebases past loom's limits | same mocking requirement |
| Coyote (MS) | delay-bounding | (no separate record given) | technique portable |
| PCT | random priorities + d−1 change points | **guaranteed ≥ 1/(n·k^(d−1)) per run** for depth-d bugs | an algorithm — implementable in any harness that owns the scheduler |
| SURW (ASPLOS'25) | weighted walk, uniform interleaving coverage | ">70% over naive random" | needs per-thread event estimation; ref library exists |
| Fray (CMU, OOPSLA'25) | **shadow locking** — every concurrency event wrapped in a harness-dictated mutex, no primitive mocking | **77% more bugs than rr chaos on 53 known-bug benchmarks** | the escape hatch where mocking can't reach |
| rr chaos | random perturbation via ptrace | lowest yield (the Fray comparison) | zero code changes |
| Antithesis-class DST | deterministic hypervisor, all nondeterminism trapped, fault injection, virtual time | perfect replay | heaviest integration |

## 5. Stress-vs-systematic numbers

- **Raft/Q-learning study: out of 100 iterations — schedule exploration
  found the bug 95 times; naive random scheduler 4; OS-driven stress 0.**
- Fray: +77% over rr chaos on 53 known bugs.
- TaxDC: standard datacenter concurrency defects "routinely evade
  detection even after 7 continuous days of high-load stress testing."
- The report rates N-times stress "near 0% detection rate for specific
  interleavings" — 10,000 runs "execute the exact same handful of
  fast-path schedules 9,990 times."
- (Shallow races DO fall to stress — our two caught-at-~20-runs defects
  don't contradict the data; the claim is about deeper interleavings.)

## 6. The minimal deterministic harness (the report's recipe)

1. **Concurrency mocking / shadow locking**: test builds route every
   atomic/mutex/futex op through a mocked primitive that reports the
   intended action to a central harness scheduler and suspends the caller.
2. **Centralized scheduler**: all workers suspended at sync points; the
   scheduler (PCT or SURW driven) unparks exactly one, lets it execute
   exactly one synchronization operation, intercepts again — concurrency
   becomes a serialized, analyzable state machine.
3. **Reactor virtualization**: sever epoll from the kernel; synthetic
   EPOLLIN injected at chosen points (e.g., between budget exhaustion and
   the futex_wait park).
Operationally: a PRNG seed identifies every discovered schedule; failures
replay exactly from the seed.

## 7. Fit notes (the digest's, for the audit/1.5 planning)

- npkc owns EVERY primitive the mock layer must intercept (futex park,
  eventfd wake, channel CAS, waker state) — no third-party boundary, which
  is what makes loom-style interception expensive elsewhere. Reactor
  virtualization matches machinery the harness half-has (mock_driver.npk).
- For 1.5's executor modeling: model the primitives (park/unpark protocol,
  the channel, the waker state machine); BPOR-style preemption bounds if
  any model contains spin/retry loops.
