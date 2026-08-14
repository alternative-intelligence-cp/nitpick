# Nitpick Memory System Specification

Nitpick’s memory model enforces strict lifecycle constraints but offers flexible paradigms—from scope-managed RAII allocations to absolute raw manual management (`wild`) tailored for systems programming and JIT engines.

**There is no garbage collector** (D-003). Lifetimes are determined statically: by
scope for managed and `stack` bindings, by generation-counted `Handle<T>` for
arena contents, and manually for `wild`/`wildx`. Nothing relocates memory
implicitly, and there are no collection pauses.

## 1. Allocation Contexts & Modifiers

Variables in Nitpick exist in one of several allocation states. You can use contextual keywords (like `stack`, `wild`) immediately before the type declaration to explicitly control their residency.

### 1.1 Default Managed Memory (Implicit RAII/Scope-based)
If no keyword is provided, the allocation is tracked and managed. The compiler will automatically clean up the binding when it falls out of scope, or at its NLL (Non-Lexical Lifetime) last-use point — deterministic destruction in both cases.
```nitpick
int32:x = 42i32;           // Automatically managed on stack
```

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
The canonical method for cleaning up `wild` memory is using a `defer` block immediately following the allocation. This guarantees the free runs on every normal exit path (early `return`, `pass`, `fail`, or `exit`).

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

`realloc` and `free` are retained as legacy aliases for `ralloc` and `dalloc`;
the latter are the preferred Nitpick vernacular. *(`BUILTIN_REFERENCE.md` §1 is
the authority on the alias set; this list previously omitted them.)*

Every allocation carries a hidden 8-byte CRC32 header. Double-frees and
corruption trigger `failsafe`.

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

### 4.2 Arena UFCS Dispatch and Chained Member Access

Nitpick provides direct compiler support for managing arenas via Uniform Function Call Syntax (UFCS). You can invoke methods directly on the `arena<T>` type:

```nitpick
arena<int64>:my_arena = arena<int64>.alloc(1000);
Handle<int64>:h = my_arena.alloc();
int64:val = my_arena.get(h) ? 0i64;      // safe unwrap with a default
my_arena.free(h);
```

> Note the operator: `?` takes a **fallback value**; `?!` takes a **failsafe error
> code** and traps (D-009). An earlier revision of this section wrote
> `my_arena.get(h) ?! 0i64`, which would call `failsafe(0)` on a stale handle
> rather than yielding `0`.

`.` handles all member access and auto-dereferences pointers (D-006). Chained
access through arenas embedded in structs is supported, with the compiler
computing field offsets:

```nitpick
struct:App = { arena<int64>:my_arena; };
App:app = ...;
Handle<int64>:h = app.my_arena.alloc();
```

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
