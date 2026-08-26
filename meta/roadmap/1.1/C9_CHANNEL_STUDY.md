# C-9, the channel half — a design study

**Status: for ratification.** The thread half landed as D-181; this is what
remains of C-9, and with it of cycle 1.1's decision load.

Much of the channel SURFACE is already settled by **D-072** — the
`Channel<T, LEVEL, CAP>` shape, `send`/`recv`/`close` with mandatory
deadlines, no `select`, no `try_*`, capacity in the type. This study does not
revisit any of it. What is open is the part D-072 left as prose: **how a
channel comes into existence, how an endpoint reaches the task that uses it,
what a `Job` is, and what `atomic<T>` actually offers.**

---

## 1. The problem that has to be solved first

D-072 and `CONCURRENCY_REFERENCE.md` §6.4 say:

> A channel's storage belongs to the scope that created it, and **endpoints
> are second-class borrows of it.**

and §7 says, of putting a reply channel inside a message:

> not available: **an endpoint is a second-class borrow and borrows may not
> cross a thread spawn (D-004).**

Both are in the spec set today, and together they make channels **unusable
for the thing channels exist for**. The normal shape is:

```nitpick
Channel<Sample, 3i32, 64i64>:ch = …;
drop producer(ch);        // a spawn — and an endpoint may not cross one
drop consumer(ch);
```

D-180 narrowed the await half of D-004 rule 4 but deliberately KEPT the spawn
ban, on D-083's reasoning: two tasks holding borrows of one storage is a race
the holder cannot see. That reasoning is right and should not be weakened.
So the endpoint must stop being a borrow.

### The recommendation: an endpoint is a HANDLE, not a borrow

An endpoint is an opaque, copyable **value** — an index and a generation,
exactly the shape `Handle<T>` (D-152) and the kernel identifiers (D-042)
already have — naming a channel whose storage the runtime owns.

| Property | Why it follows |
|---|---|
| **Not an address** | so the aliasing hazard D-004/D-180 refuses does not exist, and an endpoint may cross a spawn, sit in a message, or be sent through another channel |
| **Generation-checked** | a stale endpoint is `StaleHandle` (−4106), a catchable error, not a dangling read — the same discipline arenas already have |
| **Not reference-counted** | D-072's "no refcounting on endpoints" survives intact; nothing is freed by an endpoint going away |
| **Lexically bounded** | the CREATING scope still owns the channel's life: scope exit closes and reclaims it, and D-062 has already joined every task that could hold an endpoint |
| **Copyable** | which is what makes multi-producer natural — D-072's fan-in answer ("several producers, one channel") needs N endpoints to exist |

This is one mechanism reused, not a new one: `fd` is already an opaque
handle that "compares but does not compute" (D-042), and a channel is the
same kind of thing — a runtime-owned resource named by a value.

**`CONCURRENCY_REFERENCE.md` §7's `ask` justification changes** as a
consequence: a reply endpoint CAN be put inside a message. `ask` stays as
convenience, not as a workaround for a rule that no longer bites.

---

## 2. Construction

```nitpick
Channel<Sample, 3i32, 64i64>:ch = channel()?;
```

**Recommendation: a bare-name builtin, `channel()`, typed from context** —
the same shape `arena_make()` already has (D-152), which reads its element
type from the annotation rather than from a turbofish. It returns
`Result<Channel<…>>`: creation allocates and can fail (OOM), and D-069 makes
that visible.

Rejected: `Channel.create(…)` (a static method on a builtin generic — no
other builtin type has one), and a declaration form like `channel:ch = …`
(a new binding shape for one type, which is the meaning-by-context the
blueprint philosophy refuses).

**Capacity and level come from the TYPE**, so the constructor takes no
arguments at all — which is the point of D-072 putting them there.

---

## 3. `Job`, and therefore pools

D-073 wants `ThreadPool` submitting jobs to N workers over one channel, and
D-018 removed closures, so "a job" cannot be a captured environment.

**Recommendation: a job is any value whose type implements a `Work` trait,
and the pool is GENERIC over that type.**

```nitpick
trait:Work = {
    func:run = NIL(Self:self);          // async: see §5
};

ThreadPool<J, LEVEL, CAP>               // J: Work
```

- **Nothing is erased.** D-073's condemnation of the prototype's
  `submit(int64, ?->, int64)` was precisely that it erased the job type and
  zeroed the captured environment; a generic parameter keeps the type.
- **The captured environment becomes the struct's fields** — explicit,
  typed, and owned, which is what D-018 said closures were hiding.
- **Monomorphization makes the frame size known**, which is what lets a
  pool's workers `await job.run()` at all (see §5).

A pool is then exactly D-073's sentence: N worker tasks receiving from one
`Channel<J, LEVEL, CAP>`. It is not a new primitive and needs no new
runtime.

---

## 4. `atomic<T>`

Storage-only, no allocating constructor (D-033), which
`CONCURRENCY_REFERENCE.md` §4 already fixes. What is open is the type set and
the return shapes.

**Permitted `T`: the integer widths up to 64 bits, `bool`, and `ptr`-shaped
values.** Not floats (no atomic FP ops on the target without a CAS loop the
user should write explicitly), not aggregates (a lock-free struct is a
different design and a bigger promise), not `tbb` (its ERR taint is a
computation discipline, and an atomic read-modify-write on a sticky sentinel
is a meaning nobody has specified).

**Operations** take an explicit ordering from the keywords the lexer already
has (`relaxed` / `acquire` / `release` / `acq_rel` / `seq_cst`):

```nitpick
counter.load(acquire)                 -> T          never fails
counter.store(v, release)             -> NIL        never fails
counter.exchange(v, acq_rel)          -> T          never fails
counter.fetch_add(n, relaxed)         -> T          never fails
counter.compare_exchange(exp, new, seq_cst) -> Cas<T>   never fails
```

**`compare_exchange` returns a small struct, not a bool**:

```nitpick
struct:Cas<T> = { bool:swapped; T:observed; };
```

A bare `bool` forces every CAS loop to re-read the location, which is both
slower and a second chance to observe a different value — the classic
mis-write. The observed value is what the loop needs, so the operation
returns it.

**All of them are `never fails`** (D-163): an atomic operation on valid
storage cannot fail, and wrapping it in a `Result` would make every counter
increment a decision point.

---

## 5. Async trait methods — the split I recommend

This is the sub-question C-9 called "genuinely hard", and it separates
cleanly:

- **Through a generic parameter (`J: Work`) — SUPPORTED.** Monomorphization
  gives each instantiation a concrete callee, so the coroutine frame's size
  is known at the call site and D-177's `await` lowering works unchanged.
- **Through `dyn` — REFUSED, by name, with the reason stated.** A `dyn
  Writer`'s `await w.write(…)` needs the callee's frame size at the call
  site, and the whole point of `dyn` is that the callee is not known there.
  The honest options are heap-allocating every dyn-async frame (an
  allocation per call, on the path D-153 exists to keep predictable) or
  storing a size in the vtable and allocating dynamically anyway. Neither is
  worth its cost for the uses in front of us; both remain possible later.

Pools and actors need only the generic half, which is why this split is
enough to finish cycle 1.1.

---

## 6. CondVar

D-073 already removed `wait`, leaving `timedwait`. Under D-071 the remaining
question is the handoff, and there is only one safe answer:

**`timedwait` releases the lock, suspends the TASK, and reacquires the lock
before returning** — with the reacquisition subject to the same D-056 level
discipline as any other acquisition. A CondVar therefore carries the level
of the lock it is paired with, and pairing is fixed at construction rather
than per call, so "which lock does this CondVar go with" is not a question
any call site can get wrong.

---

## 7. Actors

**Recommendation: no new syntax at all.** D-073 already says the mailbox is
a channel; with §1's endpoints and §3's `Work`, an actor is a struct plus an
async loop that receives from its own mailbox. `ask` is a helper over a
reply endpoint carried in the message — which §1 makes legal.

Adding actor syntax would be a third spawn-shaped construct beside `drop
f(x)` and `thread`, and the blueprint philosophy's answer to that is no.

---

## 8. What ratification means

If these stand, the channel half becomes **D-182**, covering: endpoints as
handles (with the `CONCURRENCY_REFERENCE` §6.4/§7 amendments that follow),
`channel()` as a context-typed builtin constructor, `Work` + generic pools,
`atomic<T>`'s type set and `Cas<T>`, the async-trait-method split, the
CondVar handoff, and actors as a pattern rather than a primitive.

**The one I would not build unratified** is §1: making an endpoint a handle
rather than a borrow contradicts two sentences currently in the spec set,
and while I believe those sentences are the error — they make channels
unusable with spawns, which is what channels are for — that is a call about
the language's shape, not an implementation detail.

**The one I would flag as cost** is §5's `dyn`-async refusal: it is a real
capability gap, stated rather than hidden, and it will be visible the first
time someone wants a `dyn Writer` in an async path.
