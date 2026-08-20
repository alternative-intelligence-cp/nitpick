# Deep Dive: The Memory Allocator

Reviewed against nitpick-native at **0.8 closed** (commit `267c329`). Sources:
`bootstrap/runtime/npkrt.ll` (the floor, read in full), `MEMORY_REFERENCE.md`,
`BUILTIN_REFERENCE.md` §1, D-003, D-015, D-035, D-119, and the prototype's
`src/runtime/allocators/` (slab/pool/wild/wildx/arena/handle — the family this
must replace C-free).

**Bottom line:** the *only* allocator that exists today is the throwaway bump
allocator in the runtime floor. It is correct for what it is (D-015), but it means
**the real memory allocator — the load-bearing runtime TCB component — is entirely
unbuilt and, more importantly, entirely unplanned.** The roadmap stops at 0.9
"full type lowering" and 1.1 "async"; neither names the allocator, yet 1.1's async
executor *cannot* be built without arenas (D-034), and arenas cannot exist without
the allocator beneath them. This is the single largest unplanned dependency in the
build. The findings below are the specification and sequencing work owed before
that code is written, plus three spec/impl contradictions to fix now.

---

## 1. What exists today

`npkrt.ll` provides `npk_alloc` / `npk_calloc` / `npk_ralloc` / `npk_dalloc`:

- **Bump allocator, never frees.** `dalloc` is `ret void`. Correct per D-015 and
  SUBSET_1 §1.6 — the compiler runs once and exits, reclamation buys nothing, and
  an allocator is exactly the subtle code that must not live in the least-audited
  artifact. Our sources still write `defer { dalloc(p); }` so they stay correct
  when real freeing lands.
- **16-byte size header in front of every block** (0.7.3), which is what makes
  `ralloc` able to bound its copy — a hard-won fix (the commit history records a
  SIGBUS from copying the *new* size out of the *old* block when the header did
  not exist).
- **OOM calls `npk_exit(70)` directly**, with a comment noting the real runtime
  must route this through `failsafe` — correctly deferred, correctly flagged.

That is the whole of it. `arena`, `shared_arena`, `Handle`, `wild` (as distinct
from the bump floor), and `wildx` **parse as keywords but lower nowhere** — they
sit behind "a later rung" in `builtin_types.npk` with no rung named.

---

## 2. Spec ↔ implementation contradictions to fix NOW (cheap, and they mislead)

### C-1. The "8-byte CRC32 header" claim is wrong three ways — **fix the spec**

Both `MEMORY_REFERENCE.md` §111 and `BUILTIN_REFERENCE.md` §28 state verbatim:
*"Every allocation carries a hidden 8-byte CRC32 header. Double-frees and
corruption immediately trigger the failsafe."* Every clause is off:

1. **A CRC32 is 4 bytes, not 8.** "8-byte CRC32" is internally contradictory.
2. **No allocator in the project uses a CRC.** The floor uses a 16-byte *size*
   header (no integrity field at all). The prototype's `wild_alloc.cpp` uses an
   *atomic magic word* + a *footer magic* (`HEADER_MAGIC`/`FOOTER_MAGIC`), and
   `slab_alloc.cpp` uses a `pool_secret`-XOR'd free-list magic — pointer-poisoning
   canaries, not CRCs. A CRC over payload on every alloc/free would also be a
   per-operation O(size) cost nobody has signed up for.
3. **Double-free is primarily a *compile-time* error now** (D-119): a free is a
   move, and using a freed binding is `NITPICK-MOVE-001`. The runtime check is
   defense-in-depth for the cases static analysis cannot see (`wild` laundered
   through `extern`, an `any->` fabricated address), not the primary mechanism the
   spec sentence implies.

**Proposed correction (drafted into the mirrored specs):** replace with — *"Every
heap allocation carries a hidden header (size + a magic/canary word); the
allocator detects double-free and header corruption and routes them to `failsafe`.
Double-free of a tracked binding is additionally a compile-time error (D-119); the
runtime check covers pointers the static analysis cannot follow."* Leave the exact
canary scheme to the allocator's own subcycle (§4), but stop asserting a CRC32
that does not and should not exist.

### C-2. `MEMORY_REFERENCE.md` §107 still lists `free`/`realloc` as "legacy aliases" — already struck by D-119, not yet propagated

D-119 established `malloc`/`free`/`realloc` are **not builtins and not aliases** —
they are C functions reachable only through `extern`. `BUILTIN_REFERENCE.md` was
corrected (§42, the D-119 note). `MEMORY_REFERENCE.md` §3 still says *"`realloc`
and `free` are retained as legacy aliases for `ralloc` and `dalloc`."* Two docs
disagree; the memory doc is the stale one. One-line fix.

### C-3. The OOM→failsafe contract is unstated for the real allocator

The floor exits 70; the real allocator must trap. But `SAFETY_ARCHITECTURE.md`
lists OOM as a `failsafe` trigger *from the application's view* without stating the
**allocator's** obligation: OOM handling must itself be allocation-free (it runs
when allocation just failed), which means the failsafe path's memory is
preallocated — the same discipline D-014 states for `failsafe` generally, but it
needs saying at the allocator boundary specifically, because the allocator is
where the condition originates. Recorded as a design constraint for §4.

---

## 3. The unplanned critical path (the real finding)

The dependency chain nobody has drawn:

```
async/await (1.1)  ──needs──▶  per-thread executor arena for coroutine frames (D-034)
                                        │
arenas load-bearing (D-003) ───────────┤ arena<T> / Handle<T> generation counters
  replace GC for cyclic/graph data      │
                                        ▼
                              a real heap allocator (slab/pool) beneath arenas
                                        │
                                        ▼
                              free() that actually frees + the exit-time
                              <wild-live> leak check the K-semantics rule needs
```

Every arrow is a hard prerequisite, and **none of the lower boxes is on the
roadmap.** Consequences if this is not planned before 1.1 starts:

- **1.1 cannot build its executor.** D-034: "each thread's executor owns an
  `arena<T>` from which it allocates task frames." No arena → no frame allocation →
  no coroutines → no async. An agent reaching 1.1 against the current roadmap hits
  a wall with no plan to follow, which is exactly the "stop and ask" outcome the
  granular-planning goal exists to prevent.
- **The exit-time leak guarantee is presently vacuous.** The K-semantics `exit`
  rule (SAFETY_ARCHITECTURE, CONTROL_REFERENCE §4.6) traps if `<wild-live>` is
  non-empty at exit. With `dalloc` a no-op and no live-set tracked, *nothing is in
  the set and nothing is checked* — the guarantee the borrow/leak story rests on
  does not yet have an implementation to rest on. This is fine at the current rung
  (single-shot compiler) but is a load-bearing gap the moment programs run long.
- **`wildx` W^X state machine is specified (D-035) and implemented nowhere.**
  D-035's three deliverables — state the verification boundary, specify the
  lifecycle as verifiable obligations (seal-before-execute, no-write-after-seal,
  no-double-free, no-live-pages-at-exit), add `--extra-picky=no-wildx` — are all
  open. `bindings.npk` tracks move/free but has no notion of the seal transition.
  The prototype's `jit.npk` has a *found* use-after-free (D-035's own example:
  frees the page on write failure, then seals the freed page). That class is
  exactly what the state machine should reject statically, and there is no state
  machine.

---

## 4. Proposed allocator cycle (feeds the mirrored roadmap)

The allocator family is large enough to be its own cycle, and its sequencing is
forced by the dependency chain above. Drafted as **cycle 0.10** in the mirrored
roadmap (between 0.9 full-type-lowering and 1.1 async, because async needs it),
with these subcycles — each an independently testable rung, each with the
zero-dependency scan (D-011) and the three-way signature diff already in place:

| Sub | Builds | Why here / prereq |
|---|---|---|
| 0.10.0 | **The heap: a slab + large-block allocator, C-free**, replacing the bump floor for *run-once-and-keep-running* programs. Real `dalloc` with the header canary (C-1). Size classes from the prototype's `slab_alloc` design, rewritten (ideas, not code — D-015). | Everything below needs a real free. |
| 0.10.1 | **The `<wild-live>` registry + exit-time leak check** wired to real alloc/free, making the K-semantics `exit` rule non-vacuous. OOM→failsafe with a preallocated trap path (C-3). | The leak guarantee stops being theoretical. |
| 0.10.2 | **`arena<T>` + `Handle<T>`** (generation-counted, index-based per MEMORY_REFERENCE §4), single-threaded. Lowering for the keywords that currently only parse. | D-003's cycle/graph mechanism; **1.1 blocker**. |
| 0.10.3 | **`shared_arena<T>`** — chunked non-moving storage, atomic-bump alloc, no per-slot free (D-017). | Concurrency-safe arenas; **1.1 blocker**. |
| 0.10.4 | **`wildx` W^X state machine** (D-035): the alloc→write→seal→execute→free lifecycle as verifiable obligations in the analysis layer, `--extra-picky=no-wildx`, guard pages + ASLR in the runtime. Regression test: the `jit.npk` use-after-free D-035 found. | Closes D-035's three open deliverables. |

Ordering rationale, stated because the roadmap format asks for it: the heap is the
floor everything stands on; the leak registry makes the safety guarantee real
before any long-running program exists to violate it; the two arena types unblock
1.1 and must precede it; `wildx` is last because it is the most specialized and
has the fewest dependents. If 1.1 is scheduled before this cycle, 0.10.0–0.10.3
must move ahead of it regardless — the dependency is not negotiable, only the
cycle number is.

**Guard-page / canary details** (from the prototype, to rewrite not copy): magic
header word + footer word bracketing each `wild` block; `PROT_NONE` guard pages
around large allocations under `--guard-pages`; free-list nodes poisoned with a
per-pool secret XOR'd against the node address so a use-after-free that writes the
freelist is detected on the next pop. All of it is TCB code under the same
verification obligation as the rest (D-015), and all of it is *ideas* from
`../nitpick/src/runtime/allocators/` — which bottoms out in C and cannot be copied
through.

---

## 5. Smaller notes

1. **`calloc` overflow:** the floor's `npk_calloc` computes `count * size` with a
   plain `mul i64` — a classic multiply-overflow into an undersized allocation.
   Harmless in the compiler (sizes are known-small) but the real `calloc` must
   check the multiply, and the spec should say so. Recorded for 0.10.0.
2. **`ralloc(NULL, n)` is defined** (returns a fresh block) but `ralloc(p, 0)` is
   not — C's `realloc(p, 0)` is implementation-defined and a known footgun. Nail
   it down (proposed: `ralloc(p, 0)` frees and returns a non-dereferenceable
   non-NULL, or is simply refused — decide in 0.10.0).
3. **Alignment is fixed at 16** in the floor. `#[align(N)]` exists in the type
   system (D-020 examples); the real allocator needs an aligned-alloc path or
   over-alloc-and-round. Recorded.
4. **Arena `reset()`/`destroy()` and chained member access** were dropped from the
   carried-over spec (PROTOTYPE_DELTA §1) but are in MEMORY_REFERENCE §4.2 and
   D-017's operation tables. Reconcile before 0.10.2: the decision tables are the
   authority (they postdate the delta), so `reset`/`destroy` are IN for
   `arena<T>`, and `destroy` only for `shared_arena<T>`.
5. **No `pin` operator** — correctly removed (D-020), since nothing relocates
   implicitly. Worth an explicit line in the allocator spec that FFI handoff needs
   no pinning because `wild`/arena storage never moves under the program's feet
   (the driver architecture in this folder relies on exactly that).
