# Aria Language Research Summary

**Date**: December 22, 2025  
**Status**: ✅ COMPLETE (14 topics researched)  
**Source Reports**: 
- `docs/gemini/responses/language_core_research_task.txt` (168 lines)
- `docs/gemini/responses/language_advanced_research_task.txt` (157 lines)

---

## Executive Summary

Comprehensive architectural analysis of Aria v0.0.7 covering core type system, hybrid memory model, concurrency primitives, FFI design, standard library, and compilation infrastructure. Research provides complete technical specifications ready for implementation of Phase 4+ features.

---

## Part 1: Core Language Features

### 1.1 TBB (Twisted Balanced Binary) Types

**Purpose**: Eliminate two's complement asymmetry problem (e.g., abs(INT_MIN) overflow)

**Architecture**:
- **Symmetric ranges**: tbb8 = [-127, +127], reserves -128 as ERR sentinel
- **Sentinel values**:
  * tbb8: -128 (0x80)
  * tbb16: -32,768 (0x8000)
  * tbb32: 0x80000000
  * tbb64: INT64_MIN
- **Sticky error propagation**: ERR + 1 = ERR, ERR * 5 = ERR
- **Error sinks**: Comparison operators (==, <, >) transition TBB→Boolean domain

**Implementation**:
- CodeGenContext maintains `exprTypeMap` to track TBB types
- TBBLowerer emits: Sentinel check → Arithmetic → Overflow check → Selection
- Storage identical to standard integers (efficient load/store)
- Operations inject check-and-propagate logic sequence

**Key Insight**: Errors propagate through calculation chains without intermediate checks. Validation happens once at end of pipeline.

---

### 1.2 Result<T> Type

**Purpose**: Explicit error handling, zero-cost abstraction

**Structure**:
```
result<T> {
    err: int8 | NULL,  // NULL = success, non-NULL = error code
    val: T | NULL      // Value if success, NULL if error
}
```

**States**:
- Success: `{ err: NULL, val: <value> }`
- Error: `{ err: <code>, val: NULL }`

**LLVM Representation**: Likely `struct { i8 err, [padding], T val }`
- Fits in registers (System V ABI) for simple types
- Zero-cost vs stack-based exceptions

**Unwrap Operator (`?`)**:
- Syntax: `value = func() ? default`
- Success: Returns `val`
- Error: Returns default value
- Panic mode: `value = func() ?` (panics on error)

**Implementation**:
- `currentFunctionReturnType` + `currentFunctionAutoWrap` for automated wrapping
- `pass(value)` creates success result
- `fail(code)` creates error result

---

### 1.3 Hybrid Memory Model

**Allocation Strategies** (AllocStrategy enum):
1. **STACK**: Automatic storage via LLVM `alloca`
2. **GC**: Garbage-collected heap (conservative mark-and-sweep)
3. **WILD**: Unmanaged heap (malloc/free, mimalloc)
4. **WILDX**: Executable memory (JIT, mmap with PROT_EXEC)
5. **VALUE**: Register storage (optimization)

**Appendage Theory** (Ownership Model):
- **Host (H)**: Data owner (GC-managed objects, strings, etc.)
- **Appendage (A)**: Reference to Host ($ or # operator)
- **Fundamental Rule**: Depth(H) ≤ Depth(A) (Host outlives Appendage)

**Safety Operators**:
- **Pinning operator (#)**: Pins GC object, prevents movement
  * Implementation: Sets bit in object header
  * Registers in Shadow Stack (1024 roots)
  * Safe for FFI calls and wild pointer arithmetic
  
- **Safe reference operator ($)**: Creates checked borrow
  * Borrow checker validates references don't escape owner scope
  * Enforces lifetime invariants without verbose syntax

**Defer Statement**:
- RAII-style cleanup for wild allocations
- CodeGenContext maintains `deferStacks` per scope
- `executeScopeDefers` triggers LIFO execution on scope exit
- Ensures wild allocations paired with free calls

**WildX and W^X Safety**:
- Memory cannot be writable AND executable simultaneously
- `aria_mem_protect_exec` transitions from Write to Execute state
- Supports self-hosting with runtime code generation

---

### 1.4 Type Inference & Generics

**Monomorphization**:
- Creates specialized copy for each concrete type (e.g., List<int8>, List<float>)
- Unlike Java (type erasure) or Go (dictionary passing)
- **Zero runtime overhead**: LLVM optimizer inlines per-type specializations

**Implementation**:
- `typeSubstitution` map: T → concrete type
- `currentMangledName`: Unique symbols (vector_int8_push vs vector_float_push)
- `getLLVMType` recursively substitutes generic parameters
- Name mangling prevents symbol collisions

**Type Tracking**:
- `exprTypeMap`: LLVM Value* → Aria type (critical for TBB safety)
- `scope_depth`: For lifetime analysis and borrow checking

**Fat Pointers** (Debug Mode):
```c
struct aria_fat_pointer {
   i8* ptr;      // Current pointer value
   i8* base;     // Allocation base address
   i64 size;     // Allocation size
   i64 alloc_id; // Unique allocation generation ID
};
```
- 32 bytes total
- Enables bounds checking: base ≤ ptr < base + size
- Use-after-free detection: alloc_id mismatch on freed/reallocated memory

---

### 1.5 Six-Stream I/O Topology

**Standard Streams** (FDs 0-2):
- `stdin` (FD 0): Standard Input
- `stdout` (FD 1): Standard Output
- `stderr` (FD 2): Error Reporting

**Extended Streams** (FDs 3-5):
- `stddbg` (FD 3): Debug/Telemetry (structured logging)
- `stddati` (FD 4): Binary Data Input (unbuffered)
- `stddato` (FD 5): Binary Data Output (unbuffered)

**Purpose**: Separate human I/O (text logs, UI) from machine I/O (binary data pipelines)

**Implementation**:
- Text streams: AriaTextStream with internal buffering
- Binary streams: AriaBinaryStream strictly unbuffered (minimal latency)
- Linux: pipe2, fork, dup2 to map parent pipes to child FDs 0-5
- Windows: Custom bootstrap via STARTUPINFOEX to inject handles

**Use Case**: Process can emit debug logs to stddbg without corrupting binary data on stddato

---

## Part 2: Advanced Features

### 2.1 Async/Await & Concurrency

**Stackless Coroutines**:
- Compiled into compact state machines (not Green Threads)
- Heap allocation only when suspended (not per-task stack)

**LLVM Coroutine Lowering**:
1. **State Reification**: Variables persisting across await points spilled to CoroutineFrame
2. **Intrinsic Injection**: `llvm.coro.id`, `llvm.coro.begin`, `llvm.coro.suspend`, `llvm.coro.end`
3. **Function Splitting**:
   - Ramp Function: Entry point, runs until first suspend
   - Resume Function: Continuation after suspend
   - Destroy Function: Cleanup and frame deallocation

**CoroutineFrame Structure**:
- State index (which suspend point)
- Promise object
- Spilled variables (captured across await)

**RAMP Optimization** (Resource Allocation for Minimal Pause):
- **Problem**: Standard async allocates heap frame immediately, even if operation completes synchronously
- **Solution**: Start execution optimistically on caller's stack
- **Fast path**: If function completes without suspending, return direct value (no heap allocation)
- **Slow path**: On true suspension, `__aria_ramp_promote` allocates CoroutineFrame, copies locals to heap
- **Impact**: Reduces allocator pressure for high-throughput networking (buffered socket reads)

**RampResult Union**:
- Fast path: Direct value
- Slow path: Coroutine handle

---

### 2.2 Work-Stealing Scheduler

**Architecture**: M:N threading (M user tasks on N OS threads)

**Chase-Lev Deque** (per worker thread):
- **Local operations**: LIFO (Last-In-First-Out)
  * Owner pushes/pops from bottom
  * Cache locality: Most recent task likely accesses hot data
- **Stealing operations**: FIFO (First-In-First-Out)
  * Thieves steal from top
  * Load balancing: Older tasks picked up by idle workers
  * Minimal contention: Owner and thieves access opposite ends

**Wild Affinity**:
- **Problem**: Task with wild memory on Thread A cannot safely resume on Thread B (thread-local storage)
- **Solution**: Tasks with wild memory pinned to originating worker thread
- **Implementation**: Scheduler checks `affinity_thread_id` in CoroutineFrame
- **Impact**: Prevents race conditions and allocator corruption without making all types Send+Sync

**Event Loop Integration**:
- Linux: epoll
- macOS: kqueue
- Windows: I/O Completion Ports (IOCP)
- I/O completion identifies Waker and pushes task to ready queue

**Runtime API**:
- `AriaFuture`: Opaque handle for pending computation
- `aria_async(task_fn, arg)`: Spawns new task, returns AriaFuture*
- `aria_await(future)`: Suspends caller until future resolves

---

### 2.3 Foreign Function Interface (FFI)

**C ABI Mapping**:

| Aria Type | LLVM IR | C ABI Type | Size | Notes |
|-----------|---------|------------|------|-------|
| int8, tbb8 | i8 | int8_t | 1 byte | Direct |
| int64 | i64 | int64_t | 8 bytes | Direct |
| int128 | i128 | __int128 | 16 bytes | x86-64 |
| flt32 | float | float | 4 bytes | IEEE 754 |
| wild byte* | i8* | uint8_t* | 8 bytes | Raw pointer |
| result<T> | {i8*, T} | AriaResult* | Pointer | Heap allocated |

**TBB Preservation**:
- C code must check for sentinel (e.g., -128 for tbb8)
- Runtime provides intrinsics: `tbb8_add`, `tbb8_mul` for C consumers
- Sticky error semantics must be preserved across boundary

**Memory Safety**:
- **Wild pointers**: Zero-cost, map directly to C pointers
- **GC pointers**: UNSAFE by default (moving collector)
- **Pinning requirement**: Use # operator before passing to C
  * Sets bit in object header
  * Prevents GC movement during C call
  * Borrow checker validates pointer derived from pinned reference

**Result<T> Pattern**:
- AriaResult struct allocated on heap
- Caller checks `err` field (NULL = success)
- **Caller responsibility**: Must call `aria_result_free()` to prevent leaks

**Extern Blocks**:
- Manual declaration of external symbols
- Resolved at link time against .so or .a libraries
- No automatic bindgen in v0.0.7 (noted gap)

---

### 2.4 Standard Library Architecture

**Memory Allocators** (std.mem):
- **Wild Allocator**: General-purpose (malloc/free, mimalloc)
  * Fast but manual management
  * Use with `defer` for cleanup
  
- **Arena Allocator**: Phase-oriented workloads
  * Block Chaining: Links memory segments
  * Pointer bump allocation: O(1)
  * Bulk deallocation: Reset or destroy entire arena
  * Eliminates fragmentation for short-lived objects
  
- **Pool Allocator**: Fixed-size blocks
  * Free list: O(1) alloc/dealloc
  * Uniform object sizes

**Collections**:
- **AriaString**: Immutable UTF-8
  * Small String Optimization (SSO): <23 bytes stored inline (24-byte struct)
  * Heap allocation only for large strings
  * Union of Heap view and Stack view
  * Discriminator byte for size/inline detection

**Time Module** (std.time):
- Uses tbb64 for time representation (prevents Year 2038 overflow)
- Hierarchical Timing Wheel for O(1) timeout management
- Similar to Linux kernel or Rust's Tokio

---

### 2.5 Compilation Model & AriaBuild

**AriaBuild Philosophy**: "Configuration as Data" (not Code)

**ABC Format** (Aria Build Configuration):
- Whitespace-insensitive superset of JSON
- C-style delimiters: `{}` for objects, `[]` for lists
- Trailing commas supported (cleaner diffs)
- Unquoted keys if valid identifier
- Comments: `//` single-line

**Variable Substitution**:
- Syntax: `&{variable}`
- Resolution order:
  1. Local scope (target-specific)
  2. Global scope (root variables block)
  3. Environment scope (ENV.HOME)
- No mutable global state

**Deterministic Globbing**:
- **Problem**: OS doesn't guarantee readdir order
- **Solution**: Alphabetically sort all glob results before populating source list
- **Impact**: Reproducible builds (same binary bit-for-bit)
- Supports: `**` (recursive), `*` (segment), `?` (char), `[...]` (class)

**Dependency Graph Engine**:
- Models build as Directed Acyclic Graph (DAG)
- **Explicit dependencies**: User-defined (depends_on field)
- **Implicit dependencies**: Parsed from `use` statements
- Scheduling: Kahn's Algorithm (topological sort)
- In-degree calculation: Nodes with 0 dependencies enqueued first
- Cycle detection: DFS, halts on circular imports
- Incremental compilation: Checks mtime, rebuilds only if dirty

**Toolchain Orchestration**:
- `-o`: Output path mapping
- `-I`: Include path management from dependencies
- `-E`: Macro debug mode (preprocessed.aria output)

**Macro Hygiene**:
- NASM-style preprocessor
- Automatic gensym (atomic counters) for unique identifiers
- Prevents variable capture and shadowing in metaprogramming

---

## Implementation Priorities

### Phase 4+ Roadmap (Based on Research)

**High Priority**:
1. ✅ Result<T> integration (implemented)
2. ✅ TBB types foundation (in progress)
3. ⏳ Borrow checker (Appendage Theory implementation)
4. ⏳ Async/await (LLVM coroutine intrinsics)
5. ⏳ Standard library completion (6-stream I/O, collections)

**Medium Priority**:
1. Fat pointers for debug mode
2. FFI safety enforcement (pinning validation)
3. Work-stealing scheduler
4. AriaBuild deterministic builds

**Future Work**:
1. Macro hygiene system
2. WILDX and JIT support
3. Advanced async features (RAMP optimization)
4. Automatic FFI bindgen

---

## Critical Implementation Notes

### CodeGenContext Key Fields
- `exprTypeMap`: LLVM Value* → Aria type (TBB safety)
- `typeSubstitution`: Generic parameter → concrete type
- `currentMangledName`: Unique symbol per specialization
- `scope_depth`: Lifetime analysis
- `deferStacks`: RAII cleanup blocks
- `pinnedVariables`: Pinned GC objects per scope

### Runtime Integration Points
- Shadow Stack: 1024 GC roots for pinned objects
- aria_alloc/aria_free: Wild allocator API
- aria_alloc_aligned: CoroutineFrame allocation
- aria_mem_protect_exec: W^X transitions for WILDX
- aria_result_free: FFI result cleanup
- tbb8_add/tbb8_mul: TBB intrinsics for C

### Safety Invariants
1. **TBB**: ERR propagation through arithmetic
2. **Borrow**: Depth(H) ≤ Depth(A)
3. **Pinning**: GC objects pinned before FFI
4. **W^X**: Memory not writable+executable
5. **Result**: Caller frees FFI results

---

## Research Gaps (Future Work)

1. **Borrow checker**: Theory defined, semantic analysis needs completion
2. **Exotic types**: nit (base-9) arithmetic optimization (lookup tables vs vectorized polynomial)
3. **FFI bindgen**: Automatic header generation not implemented in v0.0.7
4. **Package manager**: AriaX specifications pending

---

## References

1. Core research: `docs/gemini/responses/language_core_research_task.txt`
2. Advanced research: `docs/gemini/responses/language_advanced_research_task.txt`
3. Ecosystem docs: `/home/randy/._____RANDY_____/REPOS/aria_ecosystem/`
4. Updated specs: ASYNC_MODEL.md, FFI_DESIGN.md, TYPE_SYSTEM.md, ERROR_HANDLING.md

---

**Status**: ✅ Research complete, ready for implementation  
**Last Updated**: December 22, 2025
