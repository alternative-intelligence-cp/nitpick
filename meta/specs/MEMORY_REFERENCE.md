# Nitpick Memory System Specification

Nitpick’s memory model enforces strict lifecycle constraints but offers flexible paradigms—from high-level, garbage-collected/RAII allocations to absolute raw manual management (`wild`) tailored for systems programming and JIT engines. 

## 1. Allocation Contexts & Modifiers

Variables in Nitpick exist in one of several allocation states. You can use contextual keywords (like `stack`, `gc`, `wild`) immediately before the type declaration to explicitly control their residency.

### 1.1 Default Managed Memory (Implicit RAII/Scope-based)
If no keyword is provided, the allocation is tracked and managed. The compiler will automatically clean up the binding when it falls out of scope (deterministic destruction).
```nitpick
int32:x = 42i32;           // Automatically managed on stack
```

### 1.2 `stack`
Forces explicit allocation onto the hardware call stack. Extremely fast (just a pointer bump), with memory reclaimed exactly at the scope's exit.
```nitpick
stack int32:counter = 0i32;
```

### 1.3 `gc`
Explicitly flags the object to be managed by the Garbage Collector, useful for overriding default heuristics on complex structs or when returning references.
```nitpick
gc MyStruct:obj = ...;
```

### 1.4 `wild` (Unmanaged / Manual Memory)
Declares the memory as completely unmanaged. `wild` pointers **must** be manually freed. They explicitly bypass RAII and GC tracking. If you fail to free a `wild` pointer, the compiler will abort with a memory leak error on `exit`.
```nitpick
wild int8->:buffer = alloc(1024i64);
```

### 1.5 `wildx` (Executable Memory)
Reserved for JIT compilers. Provides executable memory adhering to W⊕X (Write XOR Execute) security protocols with ASLR and guard pages.
```nitpick
wildx uint8->:code = wildx_alloc(4096i64);
```

## 2. Pinned Memory (`#`)

To make unmanaged manual memory and Garbage Collected memory interoperate safely, Nitpick provides the pin operator `#`. This tells the GC not to move or reclaim the memory until it is unpinned.

```nitpick
gc MyStruct:obj = ...;
#obj; // Pin the object so we can safely take a pointer to it for FFI
```

## 3. Managing `wild` Memory

Because `wild` memory escapes automatic lifecycle tracking, it introduces the risk of leaks and use-after-free conditions. Nitpick uses static analysis to prevent leaks at compile time.

### 3.1 The `defer` Block
The canonical method for cleaning up `wild` memory is using a `defer` block immediately following the allocation. This guarantees the free runs on every exit path (including early `return`, `pass`, `fail`, or `exit`).

```nitpick
wild int8->:buf = alloc(16i64);
defer { dalloc(buf); }
```

### 3.2 `nodrop`
The `nodrop` keyword acts as a per-binding RAII opt-out. It prevents an outer initializer from being hooked into the auto-drop tracker. Useful if you want to allocate something normally tracked but explicitly assume manual `wild` lifecycle control over it.
```nitpick
wild int8->:manual_buf = nodrop alloc(16i64);
```

## 4. Allocation Built-ins (NitpickAlloc)

Nitpick provides raw, slab-backed compiler intrinsics for dynamic sizing. All return `wild int8->` and must be handled appropriately.
*   **`alloc(size)`**: Allocate `size` uninitialized bytes.
*   **`calloc(count, size)`**: Allocate zero-initialized memory.
*   **`ralloc(ptr, new_size)`**: Resize the allocation. Old pointer becomes invalid.
*   **`dalloc(ptr)`**: Explicitly deallocate.

## 5. `Handle<T>` and Arena Allocators

For performance-critical code where you still want memory safety (e.g., Physics Nodes, ECS structures), use Arenas with `Handle<T>`.

When an object is allocated in an arena, the system issues a `Handle<T>` containing a generation counter. If the memory slot is reallocated or freed, the generation counter increments. Attempting to use the old handle immediately fails safely by returning an `ERR` through the `Result<T>` system, completely eliminating silent use-after-free catastrophes.

### 5.1 `Handle<T>` Memory Layout
The `Handle<T>` arena pointer lowers into a 16-byte aligned struct (`%Handle = type { i64, i32 }` in LLVM IR):
- **Bytes [0-7]**: `uint64:index` (Slot Index in the Arena)
- **Bytes [8-11]**: `uint32:generation` (Generation Counter for stale detection)
- **Bytes [12-15]**: Padding (for 16-byte alignment)

### 5.2 Arena UFCS Dispatch and Chained Member Access

Nitpick provides direct compiler support for managing arenas via Uniform Function Call Syntax (UFCS). You can invoke methods directly on the `arena<T>` type:

```nitpick
arena<int64>:my_arena = arena<int64>.alloc(1000);
Handle<int64>:h = my_arena.alloc();
int64:val = my_arena.get(h) ?! 0i64;
my_arena.free(h);
```
