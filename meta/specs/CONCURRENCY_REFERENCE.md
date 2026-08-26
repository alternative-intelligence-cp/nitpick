# Nitpick Concurrency and Threading

Adopted from `FORMAL_DRAFT/11_concurrency.md` with corrections and substantial
additions for D-003, D-004, D-016, D-017, and D-032…D-034. See
`GRAMMAR_ADOPTION_CONFLICTS.md` Parts I–L.

> Chapter 11 was 82 lines and predates D-003. Most of what follows is not
> correction but content that did not exist: the chapter was written when a
> garbage collector answered every ownership question, and it answers none of
> them now.

---

## 1. Two Paradigms

Concurrency splits strictly in two:

| | Mechanism | For |
|---|---|---|
| **Asynchronous execution** | native `async` / `await`, coroutines | cooperative, I/O-bound multitasking |
| **System threading** | standard library only, no language keywords | preemptive, CPU-bound parallelism |

Keeping the thread model out of the language core means bare-metal and embedded
targets are not forced to carry it.

**The connecting rule: threads supply parallelism, tasks supply concurrency, and
waiting is always a task-level event** (D-071). Every thread runs an executor, and
every blocking operation suspends the calling *task* rather than parking the
thread it is pinned to.

Without that rule the two halves collide. Tasks are pinned (D-032) and
communicate through channels (D-058), so a channel that parked the OS thread
would stall every sibling task on that executor — one task waiting on a sensor
queue silently stopping the control task beside it. There is consequently no
blocking-versus-async split in the API: `await ch.recv(deadline)` is written the
same way everywhere.

## 2. Asynchronous Execution

### 2.1 Declaring and awaiting

```nitpick
async func:fetch_data = string(string:url) {
    pass "Data payload";
};

async func:main = int32() {
    string:payload = relay await fetch_data("https://example.com");
    exit 0i32;
};
```

`async` functions return `Result<T>` like every other function, so the result
must be unwrapped — and since D-163, `raw` is NOT the unwrapper here: an
`async` callee can never be `never fails` (rule 2 — the executor can fail a
task independently of its body), so `raw await f(…)` is unlicensed by
construction. The honest spellings are the handling ones:

```nitpick
string:payload = relay await fetch_data(url);      // propagate
string:p2 = await fetch_data(url) ?| fallback;     // default
string:p3 = await fetch_data(url) ?! 9tbb32;       // trap
```

> **`await` is valid only inside an `async func`.** Using it in a synchronous
> function is a hard compile error, `NITPICK-040`.

### 2.2 Spawning a task

Calling an `async` function **without** `await` and discarding the result spawns
it on the executor:

```nitpick
drop work();        // runs concurrently; VALUE discarded, ERROR joined (D-163)
```

> **The error is not discarded (D-163, settled; the join lands via C-7/C-9).**
> `drop work()` keeps its spelling, but the spawned task's `Result` error reaches
> the enclosing scope's D-062 join, which relays the **first child error,
> verbatim (D-080), after every child has finished**, as the enclosing `async`
> function's own error; a task wound up by the join's deadline reports its wind-up
> code the same way. A spawned task's error is observable or the program does not
> compile — structured concurrency's rule, the natural completion of D-062's
> lexical task lifetime. (An `async` function can never be `never fails`, so the
> D-163 licence never applies to the spawn form.)

> Restored from the prototype's `concurrency_specs.txt` §1.3. Chapter 11 omitted
> it entirely, leaving no documented way to *start* a concurrent task — only to
> await one.

**A spawned task cannot outlive the scope that spawned it (D-062).** The task runs
concurrently, but the enclosing `async` function does not return until it has
finished. A task that must live for the whole program is spawned in `main`'s
scope.

Scope exit joins any unfinished task, under a **mandatory deadline** — the task is
asked to wind up and observes the request at its next `await`, taking a normal
error exit so `defer` runs. **The deadline is a property of the executor, fixed
where the executor is created** (D-083), so it sits in one greppable place per
thread rather than being repeated at every spawn. There is no unbounded join, and expiry **traps to
`failsafe`** rather than detaching the task or continuing silently.

This is the same two-layer shape as D-056: lexical lifetime makes "executor shuts
down with live frames" unreachable in a well-formed program, and the deadline
contains the residue. It is also the same rule borrows already follow (D-004) —
tasks nest inward and never escape outward.

> **There is no cancellation operation.** D-058 leaves no way to name a task, so
> there is nothing to cancel; the prototype's preemptive `Executor::cancel` is
> removed outright because it destroyed a live frame without running `defer`
> (D-062).

### 2.3 Tasks are pinned to threads

**A task resumes on the thread it suspended on.** The runtime does not migrate
tasks and does not work-steal (D-032).

This keeps `arena<T>`'s single-threaded guarantee (D-017) a **compile-time
structural property** rather than something depending on the scheduler
establishing happens-before edges correctly — the same reasoning that rejected a
tracing collector in D-003.

It costs less than it appears: work stealing benefits pools of CPU-bound
heterogeneous tasks, and Nitpick's async layer is I/O and coordination. Bulk
numeric work parallelizes by explicit partitioning across threads, which does not
run through the coroutine scheduler at all and is unaffected.

### 2.4 `Future<T>` and coroutine frames

`async` lowers to `@llvm.coro` state machines. `Future<T>` is the handle:

```llvm
%Future = type { ptr, ptr }   ; { coroutine_handle, result_slot }
```

**Each thread's executor owns an `arena<T>` from which task frames are
allocated**, released on task completion (D-034). Because tasks are pinned, that
arena is single-threaded — plain `arena<T>` at zero cost, not `shared_arena<T>`'s
atomic bump.

> **`Future<T>` is an internal lowering artifact, not surface syntax (D-058).**
> Nothing in the language produces one: `await f()` yields `T` directly and
> `drop work()` discards the result, so a user can neither name it nor hold it.
> The prototype already behaves this way — `type_checker.cpp`'s `AWAIT` case
> returns the operand's type with the `Future` unwrap sitting in a comment.
> Consequence to know: there is **no spawn-now-await-later**; fan-out and collect
> goes through `channel`.

## 3. System Threading

There is **no `spawn` or `go` keyword**, and **no `sync` keyword** — the compiler
rejects the latter outright. Threads, mutexes, condition variables, rwlocks, and
barriers are standard-library abstractions.

> Chapter 11 §11.3 says these interface with **`nitpick-libc`**. That is the
> wrong library and a stale reference: `nitpick-libc` was the *earliest*
> experiment, built on a musl tree while ideas were still being tested, and was
> all but deprecated even within the prototype. `libn` was written later
> specifically to remove that C dependency, and was then promoted into the
> prototype's standard library. The reference should be **`nlibc`**.

### 3.1 What already exists

Substantially more than chapter 11 describes — though less of it is C-free than
a direct `extern` count suggests. See the transitive note below.

**`libn` supplies the primitives.** Its syscall layer already wraps the Linux
threading calls — `futex` (12 uses), `clone` (5), `gettid`, `tkill`,
`set_robust_list` — so the foundation is present even though `libn` has no
thread *module* of its own.

**The promoted stdlib supplies the abstractions** (`../nitpick/stdlib/`):

| Module | Lines | C surface |
|---|---|---|
| `thread.npk` | 111 | **none** — raw syscalls only |
| `thread_pool.npk` | — | **none** |
| `mutex.npk` | 121 | **none** — futex-based |
| `rwlock.npk` | 126 | **none** |
| `condvar.npk` | 85 | **none** |
| `channel.npk` | 350 | **none** |
| `actor.npk` | 111 | **none** |
| `atomic.npk` | 181 | ⚠️ marked DEPRECATED — `extern "nitpick_runtime"` → `atomic_shim.cpp` |
| `barrier.npk` | 34 | ⚠️ marked DEPRECATED — `npk_shim_barrier_*` |
| `lockfree.npk` | — | ⚠️ marked DEPRECATED — `npk_shim_lfqueue_*` |

The three carrying **direct** C shims are already marked deprecated in their own
source, so the project had identified them independently.

> ### ⚠️ Four more are C-dependent *transitively*
>
> `thread`, `thread_pool`, `channel`, and `actor` carry no `extern` of their own
> but import `core.npk` (→ `nitpick_libc_string`) and `atomic.npk`
> (→ `nitpick_runtime`). **Only `mutex`, `rwlock`, and `condvar` are genuinely
> clean.**
>
> `atomic.npk` is superseded outright by the language-level `atomic<T>`, and
> `core.npk` / `string.npk` are superseded by `ARCHIVE/nstr`.

> ### ⛔ "Clean in themselves" was wrong — corrected
>
> An earlier revision of this section concluded that clearing those two roots
> would clear the whole stack *"without touching `thread`, `channel`, or `actor`
> themselves"*. **That assessment measured `extern` counts and nothing else, and
> it does not survive reading the code.**
>
> `meta/CONCURRENCY_STDLIB_AUDIT.md` records the full read. Three of those four
> modules carry defects that cannot be carried across at any level of effort — a
> thread-pool dequeue race that calls unwritten queue slots as functions, a
> `shutdown` that writes its stop flag to the wrong offset and reports success, a
> `try_send` that delivers the value and returns failure, an actor runner that
> reads its argument block as the actor state, and `Thread.sleep_*` implemented as
> `pass NIL;` so every sleeping loop is a spin loop.
>
> The correct conclusion is a **specification-first rewrite**, not a harder port.
> §§6–9 are that specification, and each rule there names the defect it exists to
> prevent.

`atomic.npk` in particular is superseded rather than merely stale: `atomic<T>` is
a **language type emitting native LLVM atomic IR with no shim**
(`TYPE_REFERENCE.md` §13), which is exactly what replaces it. `barrier` and
`lockfree` still need native reimplementation.

Also unassessed: `ARCHIVE/nthread` (154 lines) and `ARCHIVE/nsync`
(`nmutex` 47, `ncondvar` 47, `nsync` 205), which may duplicate or improve on the
stdlib versions.

### 3.2 What chapter 11 omits entirely

Channels, actors, thread pools, barriers, rwlocks, and lock-free queues are all
implemented, and **none of them appear in chapter 11**. It documents `async`,
`atomic<T>`, and a sentence saying threading lives in the standard library —
describing a far smaller surface than what exists. Specifying the concurrency
model properly means covering these, not just the two the chapter names.

## 4. Atomics

### 4.1 Obtaining an atomic

Atomics live in storage something else already owns. **There is no allocating
constructor** (D-033).

```nitpick
atomic<int32>:counter = 0i32;            // storage in the enclosing scope

struct:Stats = {
    atomic<int64>:hits;                  // or as a struct field
};

atomic<int32>:lk = atomic_from_ptr<int32>(hdr_ptr);   // alias existing memory
```

> `FORMAL_DRAFT` 11 §11.4.1's `atomic_new(0i32)` is **removed**. It heap-allocated
> with nothing stating who frees it — a question that did not exist when a
> collector answered it. Removing the form eliminates the question rather than
> answering it.
>
> Where an aliased address originates as an integer it must be converted with
> `#wild_ptr<T>(addr)` in `wild` context (D-019), and offsets go through
> `#ptr_add<T>(ptr, offset)` — not the raw `hdr_ptr + 24i64` §11.4.1 shows.

### 4.2 The method set

Exactly six, and nothing else:

`.load()` · `.store(v)` · `.swap(v)` · `.fetch_add(v)` · `.fetch_sub(v)` · `.compare_exchange(expected, desired)`

```nitpick
int32:prev = counter.fetch_add(1i32);
```

Methods dispatch via UFCS — another independent confirmation of D-006.

### 4.3 Strict sequential consistency

**All high-level `atomic<T>` methods enforce SeqCst** (D-016). Suffixed weaker
orderings such as `.load_acquire()` are rejected by the compiler. `relaxed`,
`acquire`, `release`, `acq_rel`, and `seq_cst` are reserved keywords but reachable
only through low-level compiler intrinsics intended for core framework
developers.

This is not conservatism for its own sake. Misused weak orderings do not fail
loudly or reproducibly — they produce intermittent corruption that surfaces on a
different CPU, under load, months later, and passes every test written for it.
That is the same failure class as numerical drift, and it is what the safety case
cannot tolerate. It also keeps `--verify-concurrency` tractable: under SeqCst,
data-race freedom is provable; under relaxed ordering the space of permitted
executions expands combinatorially.

## 5. Concurrency and the Memory Model

Chapter 11 says nothing about how threads interact with ownership. This is that
story, and it is where data-race freedom actually comes from.

### 5.1 Borrows cannot cross a concurrency boundary

Borrows are second-class (D-004): they pass down the call stack and never up. In
particular a borrow may not cross **a thread spawn** or **an `await` point**.

That eliminates shared stack references structurally, at compile time — no
runtime mechanism, no scheduler cooperation. It is the largest single
contributor to race freedom in the language, and it is enforced by the same
escape analysis that governs `stack`.

### 5.2 Arenas across threads

| | `arena<T>` | `shared_arena<T>` |
|---|---|---|
| Threading | single-threaded | multi-threaded |
| Operations | `alloc`, `get`, `free`, `reset`, `destroy` | **`alloc`, `get`, `destroy` only** |
| Per-slot `free` | yes | **no** |
| Storage | may reallocate on growth | **chunked, never moves** |
| Cost | zero | one atomic bump per allocation |

The decisive hazard is not the generation counter but that **growth moves
memory**: a pointer from `get()` dangles if another thread's `alloc()` triggers
reallocation, and no amount of atomic counting fixes that. `shared_arena<T>`
drops per-slot `free` so slots are never reused while the arena is live, which
removes the need for epochs, hazard pointers, or reference counting (D-017).

`destroy` on a shared arena requires that no thread still holds handles. That is
ownership, not synchronization: the owner destroys it after joining.

### 5.3 What this adds up to

Race freedom comes from three structural properties, none of them runtime checks:

1. borrows cannot cross a thread spawn or `await` (D-004);
2. tasks do not migrate between threads (D-032);
3. shared arenas never move memory and never reuse slots (D-017);
4. tasks cannot outlive the scope that spawned them (D-062);
5. **threads** cannot outlive the scope that spawned them either (D-083).

Properties 4 and 5 are the same rule as property 1, applied to tasks and threads
instead of borrows: all of them nest inward and none escapes outward. That is what
keeps a frame from referring to a scope that has already gone.

Note that 5 does **not** make property 1 redundant. Lexical thread lifetime closes
the *dangling* half — the spawning scope now strictly outlives the thread — while
the borrow ban closes the *aliasing* half, since two threads holding borrows of one
piece of storage is a data race regardless of how long either lives. Both are
needed.

## 6. Channels

`CONCURRENCY_REFERENCE` previously recorded channels as implemented-but-unspecified.
The implementation was read in full before this section was written; what it
actually does is catalogued in **`meta/CONCURRENCY_STDLIB_AUDIT.md`**, and several
rules below exist because of specific defects found there.

### 6.1 Shape

```nitpick
Channel<T, LEVEL, CAP>
```

| Parameter | Meaning |
|---|---|
| `T` | element type |
| `LEVEL` | D-056 lock level — a channel blocks, so it is a blocking primitive |
| `CAP` | `comptime int64` capacity. `0` is a rendezvous channel; `> 0` is buffered. |

**`CAP == 0` does not lower yet** *(1.1.10-B)*: it refuses with
`NITPICK-RUNG-001` naming stage C. A rendezvous is not a one-slot buffer — its
sender waits for a RECEIVER, and that hand-off is the synchronisation being
asked for, so it needs the waiter registration stage C builds. Lowering it onto
the buffered path meanwhile would give a send that returns the moment it
deposits: the same source, quietly not synchronising.

Capacity lives in the **type**, so each instantiation monomorphizes to one
behaviour with no runtime branch (D-064). The prototype dispatched on an `int32`
`mode` field at every operation, and its rendezvous path stored the payload in the
buffer *pointer* — which only type-checks because everything was `int64`.

There is no `oneshot` mode. It is a capacity-1 channel the sender closes.

### 6.2 Operations

```nitpick
await ch.send(move(v), deadline)   -> Result<NIL>
await ch.recv(deadline)            -> Result<T>
ch.close()                         -> Result<NIL>
```

**`len()` was struck** *(1.1.10-B)*. This sheet listed a fourth operation that
D-072 — the decision this section records — never settled; the implemented set
is the decided three. It is not a gap to fill later: a concurrent channel's
length is stale before the caller can read it, so `len()` is a hint that reads
like a fact, and every use that matters is a decision made on it a moment too
late. What the honest version of that question looks like is a `send` or `recv`
with a zero deadline, which asks and acts atomically.

- **`recv` returns `Result<T>`.** A closed channel is an **error code, never a
  value**. The prototype returned `0i64` for a bad handle, a closed channel, *and*
  a received zero — and its actor loop, built on that ambiguity, silently
  discarded any message whose value was zero.
- **The deadline substrate is D-176** (1.1.3): the parameter is a RELATIVE
  `Duration` (prelude `{ int64:ns }`) named `within` — the sheets here that
  write `deadline` predate the naming rule and read as `within` — converted
  ONCE to an absolute `CLOCK_MONOTONIC` timepoint at suspension entry
  (`mono_now()`, the floor's clock), so re-arms cannot drift. Expiry is
  `DEADLINE_EXCEEDED` (−4107): a catchable `Result` error at a `recv`/
  `acquire`, the JOIN's trap code when a task outlives its bound.
- **Deadlines are mandatory** (D-056). There is no unbounded `recv`, and a zero
  deadline expresses "do not wait" — which is why `try_send` and `try_recv` do not
  exist as separate operations.
- **`send` takes ownership**, written `move(v)` (D-065), so the transfer is visible
  at the call site.
- **Every operation suspends the task, never the thread** (D-071). What that
  rule governs is waiting for a PEER OPERATION — a full channel's sender, an
  empty channel's receiver — which is unbounded and must never cost a thread.
  It is not a claim that a channel operation is lock-free: the ring itself is
  protected by a per-channel futex mutex, a bounded critical section around a
  single element copy, held by no task across a suspension. *(1.1.10-C1 added
  that mutex; before it, two threads sending to one channel raced on `count`,
  `head` and `tail`, and 21 of 40 runs of a two-thread producer test silently
  lost messages.)*

### 6.3 What may be sent

**`T` may not contain a borrow.** Borrows are second-class and cannot cross a
thread spawn or an `await` (D-004), so a slice — which is a borrow (D-070) —
cannot be sent. Send an owning type.

### 6.4 There is no `select`

Waiting on N channels means acquiring N channel locks, and D-056 requires lock
acquisition to strictly **increase** in level, which two channels at the same
level cannot do. Making a general `select` sound would mean giving every channel a
distinct level, which does not compose, or exempting `select` from the level
discipline, which is the mechanism that makes lock-order freedom provable.

The common case is already covered: `select`'s usual job is *"receive work, but
also notice shutdown"*, and a `recv` that returns `DEADLINE_EXCEEDED` is exactly
that opportunity. Genuine fan-in uses several producers and **one** channel — one
lock, not N (D-072).

### 6.5 Lifetime

A channel's storage belongs to the scope that created it; endpoints are
**opaque generation-checked handles** into it (D-182) and cannot outlive that
scope — the same lexical rule tasks (D-062) and borrows (D-004) follow, held
by a different mechanism. They were specified as borrows, which made them
unable to cross a spawn (D-004/D-180) and so unusable for the thing channels
are for; as handles they may cross freely, and a stale one is
`StaleHandle` (−4106) rather than a dangling read.

A closed channel is **not** a reclaimed one, and the two must never be
confused: `close` ends the stream, leaving the slot, the buffer and everything
still in it exactly where they were so a receiver can drain them; reclamation
is what moves the generation and makes a surviving endpoint stale. An
implementation that bumped the generation on `close` reported `StaleHandle` —
"your handle is dangling" — for an orderly end of stream. **The reclaiming half
is not built** (D-182, and **B-6** in `meta/roadmap/OPEN_DECISIONS.md`): it is
the managed regime's RAII, which the backend does not have for any type yet, so
today a channel outlives its creating scope and `StaleHandle` cannot be
provoked from source.

There is therefore **no `destroy` and no endpoint reference counting**. It also
closes a teardown race: the prototype's `destroy` freed the mutex and both
condition variables with no check for parked waiters, and scope exit cannot run
while a task holding an endpoint is live, because D-062 joins those tasks first.

---

## 7. Actors

```nitpick
Actor<M, R, LEVEL>                      // message type, reply type, lock level

await actor.tell(move(m), deadline)     -> Result<NIL>
await actor.ask(move(m), deadline)      -> Result<R>
```

An actor is a **task with a mailbox**, not a thread with a mailbox. The prototype
spawns an OS thread per actor; under D-071 a waiting task suspends rather than
parking a thread, so the thread bought nothing and cost every defect in
`thread.npk`.

The mailbox is a `Channel<M, LEVEL, CAP>` — §6, not a second queue.

**`ask` is how a reply is obtained.** The prototype's `set_reply_channel`,
`get_reply_channel`, and `reply` are all stubs returning zero or failure. The
obvious alternative — putting a reply channel inside the message — **is
available since D-182**: an endpoint is a generation-checked handle rather
than a borrow, so it may ride in a message like any other value. `ask`
remains as convenience — it keeps the reply endpoint's lifetime obvious at
the call site — rather than as a workaround for a rule that no longer bites.

`R = NIL` for an actor that does not reply; `ask` is still useful there as an
acknowledgement, which is how backpressure is expressed.

**Lifetime is lexical** (D-062): an actor cannot outlive the scope that spawned
it, and scope exit closes the mailbox, drains it, and joins under a deadline.
`alive` is an `atomic<bool>` — the prototype writes a plain `int32` from the
stopping thread and reads it in the actor loop with no synchronization.

---

## 8. Thread pools

```nitpick
ThreadPool<LEVEL, CAP>:pool = ThreadPool.create(worker_count)?;
await pool.submit(move(job), deadline)?;
```

**A thread pool is N worker tasks receiving from one channel.** One count, one
lock, one implementation to verify.

That is not a simplification for its own sake. The prototype hand-rolls a second
queue with independent `pending`, `head`, and `tail` fields, advancing `head` at
dequeue but decrementing `pending` only after the task has run — so a second
worker reads a still-positive count, takes a slot that was never written, and
calls its contents as a function. Building the pool on §6 does not fix that race;
it removes the second counter that made it expressible.

- **Submitted work is lexically scoped** (D-062). The pool's owning scope does not
  exit until every submitted job has finished, under a deadline, and expiry traps.
  This is what replaces `shutdown`, which wrote its stop flag to the wrong field,
  could not join because the thread ids were discarded at spawn, and returned
  success regardless.
- **The job type is checked.** The prototype took a `?->` and an `int64`, and
  silently zeroed every closure's captured environment.
- **`wait_idle` does not exist** — it busy-spun with no deadline, and waiting for
  the work to finish is now what scope exit *does*.

---

## 9. Synchronization primitives

| Primitive | Form | Notes |
|---|---|---|
| Mutex | `Mutex<T, LEVEL>` | owns its data (D-056); no recursive variant |
| Read/write lock | `RwLock<T, LEVEL>` | owns its data |
| Condition variable | `CondVar<LEVEL>` | **`wait` is removed** — `timedwait` is the only form (D-056) |
| Barrier | `Barrier<comptime int32:N>` | reimplemented natively; the prototype wraps three C shims |

Every acquisition is **`async`**, deadline-bounded, and returns `Result`. There is
no infinitely blocking acquire anywhere in the concurrency surface.

```nitpick
{
    Guard<Config>:guard = relay await cfg_lock.acquire(deadline);
    guard.value.retries = 3i32;
}   // guard drops here; the lock is released
```

`await` is not optional here (D-071, D-082): an acquisition that parked the OS
thread would stall every sibling task pinned to that executor, and a mutex is the
likeliest place to hit that. The critical section is a **bare block** — there is
no `with` construct; `CONTROL_REFERENCE.md` §4.1's ordinary lexical scope already
releases the guard at the closing brace.

**There is no lock-free queue.** `lockfree.npk` is not ported: a lock-free MPMC
queue under SeqCst-only atomics (D-016) is hard to get right and expensive to
verify, a channel already provides the operation, and a second queue with a harder
proof obligation spends verification budget against the single Astrée run for a
case nothing has been shown to need (D-073).

---

## 10. Open items

- **Build the concurrency stdlib**, in dependency order — **build, not port.**
  `mutex`, `rwlock`, and `condvar` are genuinely C-free and are the only three
  that carry across as ports, and even they change shape: D-056 makes the mutex
  `Mutex<T, LEVEL>` owning its data, and D-056 removes `CondVar.wait` in favour of
  the timed form. `channel`, `actor`, `thread_pool`, and `thread` are **written
  against §§6–9 rather than ported** — `meta/CONCURRENCY_STDLIB_AUDIT.md` records
  why, defect by defect. `barrier` is reimplemented natively; `lockfree` and
  `atomic.npk` are not carried across at all (D-073). All go through the same
  D-012 signature classification as the rest of `nlibc`.
- ~~**Specify the omitted surface.**~~ — **done.** Channels, actors, thread pools,
  and the synchronization primitives are specified in §§6–9 (D-071, D-072, D-073).
- ~~**Deadlock freedom has no mechanism.**~~ — **settled by D-056.** Every
  blocking primitive carries a compile-time `LEVEL`; acquisition must strictly
  increase, making circular wait impossible by construction. What the static
  analysis cannot cover is contained by **mandatory deadlines** — every blocking
  operation returns `Result` and there is no infinitely blocking acquire. The
  flag now claims lock-order freedom rather than deadlock freedom. Note this
  changes the `mutex` API: `Mutex<T, LEVEL>` owns its data, `create_recursive` is
  removed, and the `int64` handle form is what made the old API unanalysable.
- ~~**Is `Future<T>` user-visible?**~~ — **settled by D-058: no.** It is a specified type
  (`TYPE_REFERENCE.md` §17) that no chapter uses. Either it is part of the
  surface language — in which case awaiting, composing, and cancelling futures
  need specifying — or it is an internal lowering artifact and should be marked
  as such.
- ~~**Task cancellation is unspecified.**~~ — **settled by D-062.** Task lifetime
  is **lexical**: a spawned task cannot outlive its spawning scope, which makes
  frame lifetimes nest and so makes D-034's arena correct rather than merely
  stated. Scope exit joins under a mandatory deadline; expiry traps. The
  prototype's *preemptive* cancellation is removed — it destroyed a live frame
  without running `defer`, left an admitted dangling handle, and after D-058 had
  nothing that could call it. The cooperative token survives as the join
  mechanism, not as surface syntax. On the K-semantics tie-in: `exit` already
  routes to `failsafe` when `<wild-live>` is non-empty (`nitpick.k:4215-4237`), so
  a live task set joins that same emptiness precondition.
- ~~**`async` + `failsafe`.**~~ — **settled by D-063.** A trap is a
  **whole-program event**. No coroutine is resumed on any thread, no `defer` runs
  anywhere, and no frame is destroyed — frames freeze as they are, because
  `coro.destroy()` would execute exactly the cleanup D-014 forbids. Other threads
  stop *before* `failsafe` gets control, so the handler cannot be racing a sibling
  task driving the same actuator. `failsafe` runs on the trapping thread as a
  plain call and **may not be `async`**. Async adds no new safing requirement: it
  makes D-014's existing one visible, since a synchronous function already could
  not rely on `defer` at trap time.
