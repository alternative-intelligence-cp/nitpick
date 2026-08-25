# C-9, the thread half — a design study

**Status: for ratification.** Nothing here is built. C-9 is the last decision
load in cycle 1.1, and it splits cleanly: this study covers **thread creation,
the per-thread executor, and the join**, which everything else (channels,
pools, actors, `atomic<T>`) sits on. The channel half follows once this
settles.

The question C-9 was raised for is blunt: **`Thread.spawn` appears nowhere in
the spec set** (D-083 says so in its own opening), and D-071 says every thread
runs an executor — so today the language has a rule about a construct it
cannot spell.

---

## 1. What is already decided, and therefore not open here

| Decision | What it fixes |
|---|---|
| **D-032** | Tasks are PINNED. No migration, no work-stealing — `arena<T>`'s single-threaded guarantee stays a compile-time structural property instead of runtime TCB. |
| **D-034** | Each thread's executor owns the arena its task frames come from. |
| **D-071** | Every thread runs an executor; all blocking is task suspension. **A thread's entry point is an `async func`.** |
| **D-083** | Thread lifetime is LEXICAL — scope exit joins, under a deadline, expiry traps. **There is no thread handle.** The join deadline is a property of the executor, fixed where the executor is created. |
| **D-073** | `Thread.detach` is removed. Pools and actors are channels, not new primitives. |
| **D-063** | A trap is a whole-program event: no task resumes on ANY thread, and `failsafe` runs on the trapping thread as a plain call. |
| **D-180** | A borrow may not cross a spawn — the aliasing half, which threads make literal. |

What is open is only: **how a thread is spelled, where its executor's state
lives, and what its failure does.**

---

## 2. The surface: `thread` is a function modifier

### The recommendation

```nitpick
thread async func:sensor_loop = NIL(fd:dev) {
    // ... its own executor, its own arena, its own run queue
};

async func:main = int32(cstring[]:_~argv) {
    drop sensor_loop(dev);      // starts the thread
    // ... scope exit joins it, under the deadline
};
```

**A thread body is a function declared `thread async`, and it is started by
the SAME spawn form a task uses — `drop f(args)`.** No new statement, no
handle, no ceremony.

### Why the spawn form rather than a new one

The alternative shapes were `Thread.spawn(f, arg)` (the prototype's, which
D-073 already condemns for erasing the job type and zeroing captured
environments), a `spawn` statement, or a handle-returning constructor (which
D-083 rules out).

`drop f(args)` already means *"start concurrent work whose value has nowhere
to go and whose error the enclosing scope will join"* — which is exactly what
starting a thread is. Reusing it buys every rule already built and checked:

- **D-179's spawn rule** — the callee's success type must be `NIL`, so a
  thread cannot pretend to return a value nobody can collect.
- **D-177's join** — the thread's frame goes on the same join list, is driven
  by the same deadline, and its first error relays into the spawner verbatim.
- **D-180's ban** — a borrow may not cross it, and for threads the aliasing
  hazard is a genuine data race rather than an interleaving one.
- **D-163's licensing** — the whole `drop`/`relay`/`?!`/`?|` discipline is
  unchanged.

A second spawn spelling would need every one of those rules restated, and
"which spelling does this rule apply to?" is precisely the meaning-by-context
the blueprint philosophy exists to prevent.

### Why the modifier is on the DECLARATION rather than the call

The cost is real and worth naming: reading `drop f(x)` does not tell you a
thread was created — you must look at `f`. The alternative (`drop thread
f(x)`) puts it at the call site.

The declaration wins on three grounds:

1. **It is a property of the function, not of the call.** A thread body owns
   an executor and an arena (D-034); a task does not. Under D-032's pinning
   those are different kinds of thing, and a function that is sometimes one
   and sometimes the other would have two different memory stories — the
   thing D-032 refused to buy.
2. **The audit question is answerable exactly.** `rg 'thread async func:'`
   lists every thread body in the program, and each one is a reviewable unit
   with its own deadline clause. Under the call-site form the same audit needs
   the call sites *and* their callees.
3. **`main` already works this way.** It is the one function whose executor is
   created for it, and nothing at its call site says so.

### The deadline clause

```nitpick
thread async func:sensor_loop = NIL(fd:dev) joins within(duration_secs(2i64)) {
```

D-083 requires the join deadline to be fixed where the executor is created
and to be *reviewable*. On the declaration it sits with the thread body it
governs, in one greppable place, and the program-level default (a stated
constant — currently 5s, in `npkrt`) applies where the clause is absent. This
is the one piece of new grammar the thread half needs.

**Open sub-question for ratification:** `joins within(…)` versus reusing the
existing contract position (`… never fails joins within(…)`). I recommend the
contract position — it is where per-function obligations already live, and
the parser already collects a clause list there.

---

## 3. Where the executor's state lives

Today 1.1.8's executor is **process-global**: ready queue, sleepers, park
request, the origin-chain ring, the freeze flag. On one thread that is exactly
right and the shape is honest. With threads it becomes wrong in three
different ways — two tasks on different threads would share one ready queue,
one park word, and one error chain.

### The recommendation: one thread-local pointer to an `Executor` struct

```
%npk.exec = type { ptr rq_head, ptr rq_tail, ptr sl_head,
                   i64 park_at, i32 park_pending, i32 park_word,
                   i64 join_deadline_ns,
                   [8 x i32] chain, i32 chain_n, i32 windup_seen,
                   ptr arena }          ; D-034's frame arena
```

and **one** thread-local slot holding `%npk.exec*`. Everything the executor
touches becomes a field read through that pointer.

Two mechanisms are available, and the choice is a real trade:

| | Mechanism | Cost |
|---|---|---|
| **A** | **LLVM `thread_local` globals** (initial-exec model) | The loader normally builds the TLS block from `PT_TLS`. We are static and freestanding — `_start` is ours — so the runtime must allocate and install a TLS block per thread itself, and must match the ABI's expectations for `%fs:0`. More machinery, but the access is a single `%fs`-relative load the optimiser understands. |
| **B** | **One `%fs`-relative word we own** — `CLONE_SETTLS` points `%fs` at a two-word block `[self_ptr][executor_ptr]`, and the runtime reads `%fs:8` through inline asm | Smaller: no TLS-block layout, no `PT_TLS` parsing. The ABI's `%fs:0 = self` convention is honoured so nothing else breaks. One inline-asm accessor, in one place. |

**Recommend B.** It is less machinery in the trusted computing base, it needs
nothing from the linker, and the one asm accessor is auditable at a glance —
which matters more than optimiser friendliness for a word read once per
executor operation. (A is a strictly larger surface for the same result, and
this project has one Astrée run to spend.)

### What must move, and what must not

| State | Per-thread? | Why |
|---|---|---|
| ready queue, sleepers, park word/request | **yes** | Two threads sharing a run queue would migrate tasks, which D-032 forbids outright. |
| join deadline | **yes** | D-083: fixed where the executor is created. |
| frame arena | **yes** | D-034, and it is what makes the arena single-threaded. |
| origin chain ring | **yes** | Two threads' in-flight errors would interleave into one history — the diagnostic would be fiction. |
| the freeze flag (D-063) | **NO — stays global** | "A trap is a WHOLE-PROGRAM event; no task is ever resumed after one." A per-thread freeze would let siblings keep running against unknown state, which is the exact thing D-063 refuses. |
| the heap | **NO — stays global** | It is already thread-safe by design (0.10's allocator), and per-thread heaps would break `wild` ownership transfer. |

---

## 4. Thread creation, concretely

- **`clone(2)`**, not `pthread_create` — zero dependency (the rule), and the
  runtime already owns `_start`, mmap-with-guard-page, and `exit`.
  Flags: `CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID`.
- **The stack** is one mmap with a `PROT_NONE` guard page below it — the same
  three-region shape `wildx` already uses, so the pattern is not new. Size is
  a **stated constant** (recommend 2 MiB, the prototype's, which is also what
  `Thread.detach` used to leak — D-073 §Thread).
- **`CHILD_CLEARTID` is the join.** The kernel clears a word and futex-wakes it
  when the thread exits; the joining scope waits on exactly that word, under
  the deadline. No handle is needed for this and none is exposed — which is
  what makes D-083's "no thread handle" implementable rather than aspirational.
- **`hardware_concurrency`** is `sched_getaffinity`, not the prototype's
  hardcoded `4` (D-073 lists it as a defect).

---

## 5. What a thread's failure does

A thread's root task is a task like any other, so its failure has an answer
already: **the frame carries the error, the join collects it, and the first
child error relays into the spawning scope verbatim** (D-080/D-177). Nothing
new is needed, and `failsafe` is not involved unless nobody handles it.

A **trap** inside a thread is D-063 unchanged: the global freeze is set, no
task resumes anywhere, `failsafe` runs on the trapping thread as a plain call,
and `exit_group` ends the process. The freeze staying global is what makes
this true rather than hopeful.

---

## 6. Two findings that fall out, and want ratifying with the rest

1. **`Thread.sleep_ns/ms` should be REMOVED, not reimplemented.** D-073 lists
   them as "reimplemented — the prototype's are `pass NIL;` with no syscall,
   so every sleeping loop is a spin loop". But D-071 postdates that: under
   "all blocking is task suspension", `await sleep(within)` is the one way to
   wait, and a thread-blocking sleep would be a second mechanism whose
   difference from the first is invisible at the call site — and which would
   stall every sibling task on that executor, the exact hazard D-071 was
   written against. **Recommend: struck, with D-073's row amended.**
2. **`atomic<T>` is needed by the thread half, not just the channel half** —
   the park word is written by one thread and read by another. The minimal
   set is a relaxed load/store and one compare-exchange; the full
   permitted-`T` question stays with the channel half.

---

## 7. What ratification means

If the recommendations stand, the thread half becomes a decision (D-181)
covering: the `thread async` modifier and its `joins within(…)` clause; the
spawn form reused verbatim; the `Executor` struct with a `%fs`-relative
pointer; `clone`/guard-page/CHILD_CLEARTID; the per-thread/global split of
runtime state with the freeze flag explicitly global; and the two amendments
above.

**The one thing I would not build without a decision** is the surface: the
modifier-on-declaration versus modifier-on-call choice changes what every
future concurrent program looks like, and it is a language-shape call rather
than an implementation detail.
