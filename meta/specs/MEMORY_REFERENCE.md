# Nitpick Memory System Specification

Nitpick’s memory model enforces strict lifecycle constraints but offers flexible paradigms—from scope-managed RAII allocations to absolute raw manual management (`wild`) tailored for systems programming and JIT engines.

**There is no garbage collector** (D-003). Lifetimes are determined statically: by
scope for managed and `stack` bindings, by generation-counted `Handle<T>` for
arena contents, and manually for `wild`/`wildx`. Nothing relocates memory
implicitly, and there are no collection pauses.

## 1. Allocation Contexts & Modifiers

Variables in Nitpick exist in one of several allocation states. You can use contextual keywords (like `stack`, `wild`) immediately before the type declaration to explicitly control their residency.

### 1.1 Default Managed Memory (Implicit RAII/Scope-based)

> **In a generic body a bare `T` is treated as owning (D-264, 1.5.2f):** the
> body is checked once for every instantiation, so a copy of a `T` place is
> refused (TYPE-046) unless spelled `move(...)` or `.clone()`; a lending
> `pick` binds a `T` payload as a VIEW in place (D-266, 1.5.2h) and a consuming
> one moves it. The `move` of a scalar is its copy; the rule costs nothing
> where nothing owns.
If no keyword is provided, the allocation is tracked and managed. The compiler drops the binding when its scope exits — after the scope's joins and `defer`s, before its channel reclaims (D-183, D-207) — and at no earlier point; a value's last textual use does not shorten its life. *(The sentence here said "or at its NLL last-use point" until 1.5.1b; the compiler never did that.)*
```nitpick
int32:x = 42i32;           // Automatically managed on stack
```

### 1.1a Temporaries (D-246, 1.5.1b step 4)

An owning value that no place takes — a call's result passed straight to another
call, a literal built and bound to nothing, the operand of a comparison — is a
**temporary**, and it is dropped **when its statement ends**. The rule has one
shape everywhere: the value is registered where it is produced, taken by the
place that keeps it (a binding's initialiser, a `move` parameter, a struct or
array literal's slot, a `pass`), and dropped flag-guarded at the statement's end
if nothing took it. The same drop runs on every path out of the statement — a
`fail`, a `relay`, a `?!` trap, a `break` — and a temporary that feeds a loop's
or an `if`'s condition dies with the condition. Under `await` the temporaries
of the awaiting statement live in the frame, since the statement spans a
suspension. `tests/backend/programs/temp_*.npk` and the `cost` stage's
`temporaries` probe (the nested form held to 4× the bound form's peak,
measured ×3.0) pin it.

### 1.1b `List<T>` (D-247, 1.5.1b step 5)

> **Its buffer is managed storage (D-263, 1.5.2e).** `list_init` and
> `list_reserve` allocate through `alloc_managed`, the heap's UNTRACKED entry
> (the same one a `dyn` cell and a channel's ring use), and `ralloc` keeps a
> block's role, so a grown list stays managed and the generated drop frees it
> with `dalloc`. Until 1.5.2e the buffer was `alloc`'s, the tracked entry, and a
> `List` alive in `main` at `exit 0` — where `exit` runs joins and defers and no
> drops — was the one managed value D-151's exit check counted, exit 94
> `WildLeak`. `alloc_managed` is the PRELUDE's own (TYPE-054 elsewhere): a
> hand-written `wild` container relies on D-151's count as its enforcement of an
> unpaired free, and the count stays for every `wild` block.

`List<T>` — `{ items: wild T->; count; cap }`, the compiler's own growable
collection — is **compiler-known and owning**: its generated drop releases the
`count` elements through `T`'s drop where `T` owns and hands the block back, a
vacant List (`cap == 0`, D-225) owns nothing, and the type is move-only under
TYPE-046/047 like every owning type — pass it as a plain argument, consume it
with a `move T:p` parameter, never copy it binding to binding. It is declared
in the PRELUDE with its functions (`list_init`, `list_push`, `list_reserve`)
since 1.5.1b step 5b, through the bridging refresh the seed README describes;
until then it lived in `src/frontend/list.npk`.

### 1.1c What a `pass` or a `move` transfers (D-183 as corrected at 1.5.1b step 5)

`pass` moves the returned value out implicitly, and `move(place)` explicitly;
in both the ROOT binding of the place stops owning what it held (D-065's
whole-binding rule: `pass h.name` invalidates `h`). **A value whose type does
not drop transfers nothing**: `pass h.n` over an `int64` field, or `move(h.n)`,
copies the number out and leaves every owning sibling where it was, so `h`'s
drop still runs at scope exit. Until 1.5.1b the emitter cleared the root's
drop flag for the copyable case too, and an owning local returned by one of
its copyable fields leaked its owning fields on every call (DEF-8;
`pass_field.npk`). **One exception (D-251, 1.5.2)**: a `move` or `pass` out
of an OWNING field or element of a `limit<Rules>` binding refuses
(NITPICK-TYPE-063) — the vacant value it would leave (D-254) is a write no
rule can be asked to admit; move the whole binding, or copy the part.

### 1.2 `stack`
Forces explicit allocation onto the hardware call stack. Extremely fast (just a pointer bump), with memory reclaimed exactly at the scope's exit.
```nitpick
stack int32:counter = 0i32;
```

### 1.3 `wild` (Unmanaged / Manual Memory)
Declares the memory as completely unmanaged. `wild` pointers **must** be manually freed. They explicitly bypass RAII tracking. If you fail to free a `wild` pointer, the compiler will abort with a memory leak error on `exit`.
```nitpick
wild int8->:buffer = alloc(1024i64);
```

### 1.4 `wildx` (Executable Memory)
Reserved for JIT compilers. Provides executable memory adhering to W⊕X (Write XOR Execute) security protocols with ASLR and guard pages.
```nitpick
wildx uint8->:code = wildx_alloc(4096i64);
```

### 1.5 Constructing a pointer from an address

Casting an integer to a pointer is illegal in ordinary code. The prohibition is
suspended by exactly one construct, legal only in `wild` context (D-019):

```nitpick
wild int8->:page = #wild_ptr<int8->>(addr);
```

This exists because the allocator itself must turn an `mmap` result into a
`wild int8->`. Outside that layer it should essentially never appear.

## 2. Managing `wild` Memory

Because `wild` memory escapes automatic lifecycle tracking, it introduces the risk of leaks and use-after-free conditions. Nitpick uses static analysis to prevent leaks at compile time.

### 2.1 The `defer` Block
The canonical method for cleaning up `wild` memory is using a `defer` block immediately following the allocation. This guarantees the free runs on every normal exit path (early `return`, `pass`, `fail`, `relay`, or `exit`).

> **`defer` does not run on a trap.** `!!!` and `?!` transfer control directly to
> `failsafe` without unwinding (D-014). At trap time the state of the system is
> unknown, so no cleanup code runs before the handler that understands the
> situation gets control. `failsafe` receives the allocation registry intact and
> performs whatever cleanup is appropriate.

```nitpick
wild int8->:buf = alloc(16i64);
defer { dalloc(buf); }
```

### 2.2 `nodrop`
The `nodrop` keyword acts as a per-binding RAII opt-out. It prevents an outer initializer from being hooked into the auto-drop tracker. Useful if you want to allocate something normally tracked but explicitly assume manual `wild` lifecycle control over it.
```nitpick
wild int8->:manual_buf = nodrop alloc(16i64);
```

### 2.3 Transferring ownership — `move`

`move(place)` transfers ownership out of a binding and invalidates the source
(D-065). It is what makes single-owner discipline checkable on `wild` memory, and
so what prevents double-free:

```nitpick
wild int8->:buffer = malloc(100i64);
wild int8->:moved  = move(buffer);

free(buffer);   // NITPICK-019 — use after move, and separately
                // "cannot free moved variable"
```

- **Ownership moves only where `move` is written.** Passing an owning value to a
  function borrows it (D-004); there are no implicit moves, because an implicit
  one would change ownership with nothing visible at the point it happens.
- **A moved-from binding is invalid, not "valid but unspecified."** Any read is
  an error. It may be **reinitialized by assignment**, after which it is live
  again — ordinary definite-assignment analysis. A `fixed` binding cannot be,
  since it cannot be assigned at all.
- `move` is **not** a memory qualifier, despite older grammar listing it as one.
  It is a keyword operator with a parenthesized operand, the same shape as
  `comptime(expr)`.

## 3. Allocation Built-ins (NitpickAlloc)

Nitpick provides raw, slab-backed compiler intrinsics for dynamic sizing. All return `wild int8->` and must be handled appropriately.
*   **`alloc(size)`**: Allocate `size` uninitialized bytes.
*   **`calloc(count, size)`**: Allocate zero-initialized memory.
*   **`ralloc(ptr, new_size)`**: Resize the allocation. Old pointer becomes invalid.
*   **`dalloc(ptr)`**: Explicitly deallocate.

There are **no aliases**: `malloc`/`free`/`realloc` are C names, and since
D-149 there is no in-process C to reach them through. An earlier revision of
this list called `realloc` and `free` "retained legacy aliases" — that was
struck by D-119 in `BUILTIN_REFERENCE.md` and is corrected here too. The fifth
native, **`aalloc(size, align)`**, serves alignments above the default sixteen
(0.10.0).

Every heap allocation carries a hidden 16-byte header — **the block's size and
a secret-keyed, address-keyed magic word** — and the allocator detects
double-free and header corruption and routes them to `failsafe`
(`-4102`), out-of-memory to `failsafe` (`-4103`), and a malformed request
(negative size, `calloc` count×size overflow — the multiply is CHECKED —
`ralloc(p, 0)`, a non-power-of-two alignment) to `failsafe` (`-4104`).
*Not a CRC*: an earlier revision claimed "a hidden 8-byte CRC32 header", which
was wrong three ways (a CRC32 is 4 bytes; no allocator in the project's
history uses one; a CRC over payload is an O(size) cost per operation nobody
signed up for). The real scheme is canaries: in a slab the next block's header
is its neighbour's overrun canary and a tail guard closes the chunk; large
blocks carry their own footer guard; freed slots keep a distinct FREED magic
that is re-verified when the slot is handed out again. Double-free of a
**tracked** binding is additionally a compile-time error (D-119); the runtime
check covers pointers the static analysis cannot follow. Allocator control
state (bitmaps, the chunk and large-block tables) lives out of band where no
payload overrun can reach it, and `dalloc`/`ralloc` prove a pointer lies
inside allocator-owned memory before dereferencing anything — a garbage
pointer is a trap, never a wild load. The heap is single-threaded at this
rung; the lock discipline lands with 1.1's executor work.

### The `<wild-live>` registry and the exit-time check (0.10.1, D-151)

> **What the check counts, and what it does not (D-263, 1.5.2e).** Every block
> from `alloc`, `aalloc`, `calloc` and a `ralloc` of one — the `wild` regime,
> whose unpaired free is the author's — and never managed storage: a string's
> body, a `dyn` cell, a channel's ring, and since 1.5.2e a `List<T>`'s buffer
> (`alloc_managed`, the prelude's own entry). `exit` runs joins and defers and
> no drops (D-183's amendment), so an owning local of `main` is never dropped
> by a program that exits; managed storage is the kernel's at exit, and the
> check says nothing about it.

The allocator's own tables ARE the live-set — no second bookkeeping
structure exists to drift from the first. A **wild** allocation is what the
`alloc`/`calloc`/`ralloc`/`aalloc` builtins hand out, stamped with a
wild-role magic; **runtime-internal storage** (string bodies, argv, file
buffers) is managed-regime, stamped with the internal role, and is NOT in
`<wild-live>` — its RAII arrives with the managed lowering, and until then
it is reclaimed wholesale by `wild_release_all()` or process exit.

- **`wild_live_count()`** walks the chunk bitmaps (slots below each
  watermark whose free bit is clear, counting wild-role headers) and the
  large table. Allocation-free, preallocated state only: safe from
  `failsafe` in a degraded process.
- **The exit-time check**: a SUCCESSFUL exit (code 0 — CONTROL_REFERENCE
  §4.6's "successful" scoping) with a non-empty set routes to `failsafe`
  with `-4105`. A failure exit keeps its code — hijacking an error report
  with a leak trap would destroy the error, and error paths carry no
  cleanup obligation (the defer-does-not-run-on-trap reasoning, D-014).
  `failsafe` may call **`wild_release_all()`** — followed by `exit` and by
  nothing else (TYPE-062, 1.5.1b step 5: a `main` that released and then
  RETURNED ran its scope-exit drops over unmapped memory) — drops every chunk and
  large mapping, both regimes, leaving the allocator usable — and exit
  positive; its own exit passes because the in-failsafe flag is set. The
  same flag makes a trap RAISED INSIDE failsafe exit 70 directly instead
  of recursing.

**One registry mechanism, three clients** (the audit's unification ask):
the sorted fixed-stride table — mmap-grown, preallocated initial capacity,
binary-searched, allocation-free to read, `failsafe`-walkable — is the
mechanism, and the allocation tables (8-byte chunk entries, 32-byte large
entries) are its first client. The **stream registry** (IO_REFERENCE §10)
and the **driver registry** (D-149's Bridge) are specified as the next two
clients of the same shape, not re-inventions: fixed-stride entries, sorted
by key, walked by `failsafe` in registration order at shutdown.

## 4. `Handle<T>` and Arena Allocators

Arenas with `Handle<T>` are the primary mechanism for **graph-shaped and cyclic
data**, as well as for performance-critical structures (physics nodes, ECS). With
no collector, they are how Nitpick handles reference cycles: allocate the graph
in an arena and drop the arena wholesale, so individual nodes are never freed and
cycles among them are irrelevant.

When an object is allocated in an arena, the system issues a `Handle<T>` containing a generation counter. If the memory slot is reallocated or freed, the generation counter increments. Attempting to use the old handle immediately fails safely by returning an `ERR` through the `Result<T>` system, completely eliminating silent use-after-free catastrophes.

Handles are **indices, not pointers**, which is what makes them safe across arena
growth and what lets them express cyclic references without a collector.

### 4.1 `Handle<T>` Memory Layout
The `Handle<T>` arena pointer lowers into a 16-byte aligned struct (`%Handle = type { i64, i32 }` in LLVM IR):
- **Bytes [0-7]**: `uint64:index` (Slot Index in the Arena)
- **Bytes [8-11]**: `uint32:generation` (Generation Counter for stale detection)
- **Bytes [12-15]**: Padding (for 16-byte alignment)

### 4.2 The operation set, and chained member access

Creation is the **`arena_make(cap)`** builtin, type-directed by the
annotation the way an unsuffixed literal is (D-092). An earlier revision
wrote `arena<int64>.alloc(1000)` — one name for two different operations
(creating the arena on the type, allocating a slot on the value), which the
blueprint philosophy refuses; D-152 split them.

```nitpick
arena<int64>:my_arena = arena_make(1000i64);
Handle<int64>:h = my_arena.alloc();
drop my_arena.put(h, 41i64);             // write through the handle
int64:val = my_arena.get(h) ? 0i64;      // read a COPY, with a default
drop my_arena.free(h);
my_arena.destroy();
```

The set is `alloc() -> Handle<T>`, `get(h) -> Result<T>`,
`put(h, v) -> Result<NIL>`, `free(h) -> Result<NIL>`, `reset() -> NIL`,
`destroy() -> NIL` (D-017 as amended by D-152). `get` returns the element
**by value**: a borrow-returning `get` would be a returned borrow, which
D-004 refuses everywhere — mutation is spelled `put`. A stale handle fails
`get`/`put`/`free` with **`-4106` in `Result.err`**, never a trap.
`destroy` CONSUMES the arena (a compile-time move, like `dalloc`), and an
un-destroyed arena is a wild-role leak the exit-time check names (D-151).

> Note the operator: `?` takes a **fallback value**; `?!` takes a **failsafe error
> code** and traps (D-009). An earlier revision of this section wrote
> `my_arena.get(h) ?! 0i64`, which would call `failsafe(0)` on a stale handle
> rather than yielding `0`.

`.` handles all member access and auto-dereferences pointers (D-006). Chained
access through arenas embedded in structs works as ordinary member-place
addressing:

```nitpick
struct:App = { arena<int64>:my_arena; };
App:app = App{ my_arena: arena_make(16i64) };
Handle<int64>:h = app.my_arena.alloc();
```

### 4.2b The executor frame allocator is NOT `arena<T>` (D-153)

D-034's "each thread's executor owns an arena from which it allocates task
frames" names the arena *philosophy* — batch lifetime, drop-cheap — not the
surface type. The surface `arena<T>` is a **fixed-slot** allocator handing
out generation-checked **indices**; a coroutine frame is a **per-function,
variably-sized** block that `@llvm.coro.begin` needs as a **raw pointer**.
Conflating the two is the mistake the concurrency audit caught
(total_audit B-1), and this section exists so nobody repeats it.

The executor frame allocator (0.10.3) is runtime-internal — no keyword, no
builtin; its only caller is 1.1's coroutine lowering (C-7) — with a fixed
five-call interface: `npk_frame_exec_new` / `npk_frame_alloc(size, align)`
/ `npk_frame_free` / `npk_frame_drain` / `npk_frame_exec_destroy`. Tasks
are pinned (D-032), so the whole path is single-threaded and zero-atomic —
that is D-034's rationale for pinning. Chunked bump allocation; completed
frames return to a free list bucketed by **exact size**, which fits
coroutines precisely (one frame size per async function, recurring);
oversize frames take dedicated heap blocks; `drain` retires every frame at
once while keeping the chunks. The executor and its chunks are wild-role
heap blocks: an un-destroyed executor is a countable leak (D-151). Frame
alignment is capped at 16, the heap's own.

### 4.3 `shared_arena<T>` — arenas across threads

`arena<T>` is **single-threaded**. Sharing one across threads is unsound: growth
may relocate slots, so a pointer obtained from `get()` can dangle when another
thread's `alloc()` triggers reallocation — a hazard no amount of atomic
generation-counting fixes.

For concurrent use, `shared_arena<T>` is a distinct type with a deliberately
smaller contract (D-017):

| | `arena<T>` | `shared_arena<T>` |
|---|---|---|
| Threading | single-threaded | multi-threaded |
| Operations | `alloc`, `get`, `free`, `reset`, `destroy` | **`alloc`, `get`, `destroy` only** |
| Per-slot `free` | yes | **no** |
| Storage | may reallocate on growth | **chunked, never moves** |
| Cost | zero | one atomic bump per allocation |

Dropping per-slot `free` is what makes concurrency safe without epochs, hazard
pointers, or reference counting: slots are never reused while the arena is live,
so generations never increment during concurrent access and there is no freelist
to contend on. Allocation is a single atomic bump into non-moving chunked
storage.

`destroy` requires that no thread still holds handles — this is ownership, not
synchronization: the owner destroys the arena after joining.

**As built (0.10.4, D-154):** creation is `shared_arena_make(cap)`,
type-directed like `arena_make`; the surface value is ONE POINTER to the
runtime structure — shareable by reference is the contract, so the value is
the reference. **`alloc(v)` carries the value**, because there is no `put`:
the slot is written once, before its handle escapes, and is immutable after —
which is what makes concurrent `get` race-free with no per-slot machinery
(handle transfer between threads is the synchronizing edge; 1.1's channels
are SeqCst, D-016). `get` COPIES, exactly as `arena<T>`'s does and for the
same reason (a borrow return would violate D-004; D-152's argument applies
unchanged — the "likely yes" this section once carried is decided NO). Growth
RESERVES a capacity range with one atomic `fetch_add` — racing installers get
disjoint ranges and cannot collide — then publishes the chunk with a CAS
push; chunk sizes are geometric (each new chunk carries the arena's current
capacity in slots, capped at 65536), so a large arena is a few chunks, never
thousands. Shared handles carry generation ZERO constantly (nothing frees,
nothing increments), and `arena<T>` issues generations starting at 2 — so a
single-threaded arena's handle wandering into a shared `get` is refused as
stale (`-4106`), not read. `destroy` consumes the binding at compile time
(`MOVE-002`), and an un-destroyed shared arena is a wild-role leak the exit
check names (D-151).
