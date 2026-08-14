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

## 2. Asynchronous Execution

### 2.1 Declaring and awaiting

```nitpick
async func:fetch_data = string(string:url) {
    pass "Data payload";
};

async func:main = int32() {
    string:payload = raw await fetch_data("https://example.com");
    exit 0i32;
};
```

`async` functions return `Result<T>` like every other function, so the result
must be unwrapped — `raw` precedes `await`.

> **`await` is valid only inside an `async func`.** Using it in a synchronous
> function is a hard compile error, `NITPICK-040`.

### 2.2 Spawning a task

Calling an `async` function **without** `await` and discarding the result spawns
it on the executor:

```nitpick
drop work();        // runs concurrently; result discarded
```

> Restored from the prototype's `concurrency_specs.txt` §1.3. Chapter 11 omitted
> it entirely, leaving no documented way to *start* a concurrent task — only to
> await one.

**A spawned task cannot outlive the scope that spawned it (D-062).** The task runs
concurrently, but the enclosing `async` function does not return until it has
finished. A task that must live for the whole program is spawned in `main`'s
scope.

Scope exit joins any unfinished task, under a **mandatory deadline** — the task is
asked to wind up and observes the request at its next `await`, taking a normal
error exit so `defer` runs. There is no unbounded join, and expiry **traps to
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
> The taint has few roots, so this is a targeted job rather than a rewrite:
> `atomic.npk` is superseded outright by the language-level `atomic<T>`, and
> `core.npk` / `string.npk` are superseded by `ARCHIVE/nstr`. Clearing those
> clears the entire concurrency stack **without touching `thread`, `channel`, or
> `actor` themselves**.

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
4. tasks cannot outlive the scope that spawned them (D-062).

Property 4 is the same rule as property 1, applied to tasks instead of borrows:
both nest inward and neither escapes outward. It is what keeps a task frame from
referring to a scope that has already gone.

## 6. Open items

- **Port the concurrency stdlib**, in dependency order. `mutex`, `rwlock`, and
  `condvar` are genuinely C-free and can go first. `thread`, `thread_pool`,
  `channel`, and `actor` are clean in themselves but must wait until `atomic.npk`
  is dropped and `core.npk`/`string.npk` are replaced by `nstr`. All go through
  the same D-012 signature classification as the rest of `nlibc`. `barrier` and `lockfree`
  need native reimplementation to drop their C shims; `atomic.npk` is superseded
  by the language-level `atomic<T>` and should not be ported at all.
- **Specify the omitted surface.** Channels, actors, and thread pools have real
  implementations and no specification. The concurrency model is not fully
  described until they are covered.
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
