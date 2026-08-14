# Concurrency stdlib audit

A full read of `../nitpick/stdlib/`'s concurrency modules — `channel.npk` (350),
`actor.npk` (111), `thread_pool.npk` (157), `thread.npk` (111), `condvar.npk`
(85) — undertaken to recover their semantics before specifying the concurrency
surface. `CONCURRENCY_REFERENCE.md` §6 recorded that these have *"real
implementations and no specification"*, and the intent was to read the
implementation as the specification, the way D-057 recovered macro semantics from
the regression suite.

**That is not possible here.** The implementations do not agree with themselves,
and several of them do not work. This document records what was found, because
the findings determine the design rather than merely informing the port.

Findings are ordered by severity. Line numbers are as of the current prototype
tree.

---

## 1. Critical

### 1.1 `thread_pool` — two workers can dequeue the same slot, then call garbage as a function

`thread_pool.npk` maintains `pending` (offset 40), `head` (48), and `tail` (56)
as three independent fields. `submit` increments `pending` and advances `tail`
together under the lock. The worker advances `head` at **dequeue** but decrements
`pending` only **after the task has run** (`thread_pool.npk:39` vs `:47`).

The two therefore disagree for the entire duration of a task, and a second worker
that acquires the lock in that window reads a `pending` that is still positive:

| | Worker A | Worker B |
|---|---|---|
| | lock; `pending`=1; `head`=0 | |
| | take slot 0; `head`←1; **unlock** | |
| | *running the task* | lock; `pending`= **1** (not yet decremented) |
| | | `head`=1 → take slot **1**, which was never written |
| | | `head`←2; unlock; `called(arg)` |
| | `pending`←0 | |

Slot 1 holds whatever the mmap'd queue contained — zero on a fresh page, stale
task data after wraparound. `thread_pool.npk:42-43` loads a function pointer from
it and calls it.

**This is an arbitrary-jump defect reachable from ordinary use**, and it needs no
unusual timing: any two workers and one task suffice.

### 1.2 `thread_pool.shutdown` writes the stop flag to the wrong field, then reports success

`create` lays the pool out with **shutdown at offset 16** and `active_workers` at
32 (`thread_pool.npk:72-74`), and the worker loop tests offset 16
(`thread_pool.npk:51`, `:56`).

`shutdown` writes offset **32**:

```nitpick
<-cast_unchecked<int64->>(pool + 32i64) = 1i64; // Stop flag
```

So the flag the workers read is never set, the workers never exit, and
`shutdown` returns `0` — success. It corrupts `active_workers` on the way past,
which is why `active_tasks` reports 1 after a shutdown that did nothing.

The function also cannot join: `create` discards every thread id at spawn
(`drop(Thread.spawn(worker, pool))`, `:83`), and the code says so —
*"We don't have a way to join all yet natively since we didn't store PIDs, so
just yield briefly."* It yields twice and returns.

**A thread pool cannot be shut down, and the API reports that it was.**

### 1.3 `Channel.try_send` on an unbuffered channel delivers the value and reports failure

`channel.npk:204-219`:

```nitpick
state.array_ptr = value;                  // value is stored
state.count = 1i32;                       // marked present
drop CondVar.signal(state.cv_recv);       // a receiver is woken
drop Mutex.unlock(state.mtx);
pass 0i32 - 1i32;                         // reported as FAILURE
```

The comment above the return explains the *intent* — Go's `try_send` on an
unbuffered channel should succeed only if a receiver is already waiting, and the
author could not detect that — but the code returns the failure **after**
publishing the value rather than instead of publishing it.

Consequences for a caller that believes the return value: retrying sends the
message twice; freeing the value on the failure path hands the receiver a
dangling pointer.

### 1.4 `Actor`'s runner reads the argument block as the actor state

`Actor.spawn` allocates a 32-byte argument block and stores the state pointer at
offset 0 and the behaviour function at offset 24 (`actor.npk:36-38`). The runner
receives that block and does:

```nitpick
arena<ActorState>->:st = cast_unchecked<arena<ActorState>->>(arg);
```

`arg` **is** the argument block, so this casts the block itself to `ActorState`
rather than loading the pointer stored in it. `st.mailbox` therefore reads the
*state pointer* and passes it to `Channel.recv` as a channel handle, where it is
dereferenced as a `ChannelState` — a 20-byte allocation read as a 60-byte
structure, with `ChannelState.mtx` aliasing `ActorState.mailbox` and the rest
running off the end.

The dereference at `actor.npk:22` is present for the function pointer
(`<-cast_unchecked<...>(arg + 24i64)`) and absent for the state.

---

## 2. Silent no-ops

Functions that return success and do nothing. Each is worse than an absent
function, because the call site reads as though the work happened.

| Function | File | Behaviour |
|---|---|---|
| `Thread.sleep_ns` / `Thread.sleep_ms` | `thread.npk:101-102` | `pass NIL;` — **no syscall at all.** Any code that sleeps busy-spins. |
| `Thread.detach` | `thread.npk:94` | returns 0; leaks the 2 MiB stack and the context page |
| `Actor.reply` | `actor.npk:108` | always `-1` |
| `Actor.get_reply_channel` | `actor.npk:104` | always `0` |
| `Actor.set_reply_channel` | `actor.npk:100` | discards its argument |
| `Channel.select3` / `select4` | `channel.npk:343-349` | always `-1` — "nothing ready", forever |

`Thread.sleep_*` is the most damaging: a polling loop written against it becomes
a spin loop, on a system whose entire safety argument depends on bounded timing.

The three `Actor` reply functions are the whole request/reply half of the actor
model, and the source says as much: *"To truly fix this we should use TLS, but
for now … just return 0 to bypass for now as actors usually don't use it."*

---

## 3. Type erasure — the same defect this project keeps finding

Every one of these APIs is spelled in `int64`.

| Erased | Should carry |
|---|---|
| `Channel.send(int64:handle, int64:value)` | the element type |
| `Channel.recv → int64` | the element type |
| `Actor.send(int64, int64:message)` | the message type |
| `ThreadPool.submit(int64:pool, ?->:func, int64:arg)` | the job signature |
| `Mutex.create → int64` | the guarded data (already fixed by D-056) |

This is the same finding as `printf`'s `int64` arguments, `libn_ioctl`'s
unconstrained request code, `scanf`'s discarded length modifiers, and the `mutex`
handle D-056 removed. The fix is the same each time: **put the information in the
type.**

Two concrete failures follow directly from it here:

**`recv` cannot distinguish a received `0` from a closed channel.** `channel.npk`
returns `0i64` for a bad handle (`:140`), for a closed-and-empty channel (`:148`),
and for a successfully received value of zero (`:176`). `try_recv` does the same
for an empty channel (`:247`).

`Actor`'s runner is built on that ambiguity — it treats `msg == 0` as *possibly
closed* (`actor.npk:25-28`), so **a legitimate message whose value is zero is
silently discarded**.

**Unbuffered channels store the payload in the pointer field.** With capacity 0
no buffer is allocated, so `array_ptr` is reused to hold the value itself
(`channel.npk:106`, `:164`). It is consistent with itself and it only type-checks
because everything is `int64`.

---

## 4. Unbounded blocking

D-056 requires that no blocking operation waits forever.

- **`CondVar.wait` has no deadline** (`condvar.npk:37`) — a bare `FUTEX_WAIT`.
  `timedwait` exists beside it, so the unbounded form is a choice rather than a
  gap.
- **`Channel.send` / `recv` have no deadline** and loop on `CondVar.wait`.
- **`Channel.select2` ignores its timeout except as a zero test**
  (`channel.npk:337`): any non-zero value spins on `Thread.yield()` forever,
  never consulting the clock. Its `iters` counter is declared and never used.
- **`ThreadPool.wait_idle` busy-spins** on `Thread.yield()` with no deadline
  (`thread_pool.npk:142-152`).

### `select2` additionally reports closed as ready, and races

It returns a channel index when that channel is merely **closed**
(`channel.npk:327`, `:334`), so the caller proceeds to `recv` and — per §3 —
receives `0` indistinguishable from a value. And it releases each channel's lock
before returning the index, so another receiver may take the value first, leaving
the follow-up `recv` to block. A `select` whose purpose is to guarantee a
non-blocking follow-up does not.

---

## 5. Teardown races

`Channel.destroy` (`channel.npk:296-309`) frees the buffer and destroys the mutex
and both condition variables with **no check that nobody is waiting on them**. A
thread parked in `CondVar.wait(state.cv_recv, state.mtx)` wakes on freed memory.
`Actor.destroy` calls it (`actor.npk:92`), so the actor path inherits it.

Nothing in the module reference-counts endpoints or establishes quiescence.

---

## 6. Structural problems

**Hand-rolled structs as raw offsets.** `thread_pool.npk` addresses its state as
`pool + 16`, `pool + 40`, `pool + 64`, with the layout in a comment
(`thread_pool.npk:7-15`). §1.2 is precisely the failure this invites: a field
renumbered in one place and not another, with nothing to check it. `actor.npk`
does the same with its magic offset 24 (`:22`, `:38`), and `thread.npk` with its
context page (`:53-56`).

**Closure environments are silently discarded.** `ThreadPool.submit` comments
*"Deconstruct fat pointer"* and then sets `env_ptr = 0i64` unconditionally
(`thread_pool.npk:109-111`). Any submitted closure with captures runs with a null
environment.

**`Thread.spawn` reads locals in the child after a raw `clone`.**
`thread.npk:69-72` takes the `pid == 0` branch and reads `tls_base`, a local. The
child begins on a freshly mmap'd stack with no frame, so its copy of that local
does not exist; the value survives only if codegen happens to have kept it in a
register `clone` preserves. This is the standard reason raw `clone` needs an
assembly trampoline, and it is not something to leave to luck.

**`Thread.set_name` passes a `string` where a `char->` is expected.**
`cast_unchecked<int64>(name)` on a `{ptr, len, cap}` value (`thread.npk:105`);
`prctl(PR_SET_NAME)` receives the struct, not the text.

**Dead constructs.** `sys!!` throughout (removed by D-001); a `0xDEADBEEF` marker
written *"so GC safepoint ignores this thread"* (`thread.npk:39-40`, and D-003
removed the collector); `print_hex`, a debug helper that mmaps 4 KiB per call
(`thread.npk:5-31`); `hardware_concurrency` hardcoded to `4` (`:109`).

---

## 7. What this means for the port

The `CONCURRENCY_REFERENCE.md` §6 plan said `thread`, `thread_pool`, `channel`,
and `actor` are *"clean in themselves"* and blocked only on `atomic.npk` and
`core.npk`. **That assessment was based on an `extern` count, and it is wrong.**
They carry no C shims, which is what was measured, but three of the four contain
defects that cannot be carried across at any level of effort.

The correct conclusion is not a harder port but a **specification-first
rewrite**: none of these four modules is a suitable starting point, and the port
plan should say so rather than describing them as ready.

What the audit *does* provide is a precise list of the properties the
specification has to guarantee, and every one of them is now a rule in
`CONCURRENCY_REFERENCE.md` §§7–10:

| Defect | Prevented by |
|---|---|
| §1.1 dequeue race | one queue with one count under one lock — the pool is workers on a channel (D-073) |
| §1.2 wrong offset, no join | no hand-rolled offsets; pool lifetime is lexical and joins (D-062, D-073) |
| §1.3 deliver-then-fail | operations return `Result<T>`; publishing and reporting cannot disagree |
| §1.4 missing dereference | typed state, no `cast_unchecked` in the construction path |
| §2 silent no-ops | every operation returns `Result<T>`; there is nowhere to return a fake success |
| §3 erasure | `Channel<T, LEVEL, CAP>`, `Actor<M, R, LEVEL>` |
| §3 `0` means three things | `recv` returns `Result<T>`; closed is an error code, not a value |
| §4 unbounded waits | mandatory deadlines (D-056) |
| §4 `select` races | `select` does not exist (D-072) |
| §5 teardown races | channel storage is lexically scoped; endpoints cannot outlive it |
| §6 discarded closures | job type is checked, not a raw pointer |
| §6 raw `clone` in Nitpick | thread entry is specified, not open-coded per call site |
