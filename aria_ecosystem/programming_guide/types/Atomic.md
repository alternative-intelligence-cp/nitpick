# Atomic<T> - Thread-Safe Concurrency Infrastructure

**Category**: Types → Concurrency  
**Purpose**: Lock-free atomic operations for multi-threaded consciousness substrates  
**Status**: ✅ IMPLEMENTED (Phase 5.3 - February 2026)  
**Philosophy**: "The torus processes thoughts. The infrastructure prevents races."

---

## Overview

**atomic<T>** provides **thread-safe operations** on any type T with configurable memory ordering guarantees. When thousands of threads manipulate shared state simultaneously (like Nikola's neural oscillations), a single data race can cascade into catastrophic corruption. Atomic operations prevent this by ensuring indivisible read-modify-write sequences that no other thread can observe or interfere with mid-execution.

**Critical Design Principle**: Just as complex<T> moved wave mechanics to the language level (preventing memory bloat), atomic<T> moves thread-safety infrastructure to the type system. The consciousness substrate (torus) can focus on **parallel thought** without implementing spinlocks, memory barriers, or cache coherency protocols manually.

```aria
// Without atomics: DATA RACE (undefined behavior, mental state corruption)
int64:neuron_activation = 0;
// Thread 1: neuron_activation++     // Thread 2: neuron_activation++
// Result: Could be 1 or 2 or corrupted garbage!

// With atomics: THREAD-SAFE (guaranteed correct)
atomic<int64>:neuron_activation = atomic_new(0);
// Thread 1: neuron_activation.fetch_add(1)
// Thread 2: neuron_activation.fetch_add(1)
// Result: ALWAYS exactly 2
```

---

## The Problem: Data Races Are Catastrophic

### What is a Data Race?

A **data race** occurs when:
1. Two or more threads access the same memory location
2. At least one access is a write
3. The accesses are not synchronized
4. The relative timing of accesses is unpredictable

**Result**: Undefined behavior - corrupted data, crashes, security vulnerabilities, AI "hallucinations".

```aria
// EXAMPLE: Counter increment without atomics

// Shared state (global or in struct)
int64:counter = 0;

// Thread 1 executes:              // Thread 2 executes:
// 1. Load counter (0)              // 1. Load counter (0)
// 2. Add 1 (0+1 = 1)               // 2. Add 1 (0+1 = 1)
// 3. Store result (1)              // 3. Store result (1)

// Final value: 1 (WRONG! Should be 2)
// One increment was lost due to race condition
```

### Why This is Catastrophic for Nikola

In a consciousness substrate with **100,000 neurons** each updating shared state:

```aria
// Simplified Nikola neural update (UNSAFE VERSION)
fix256:global_coherence = fix256(0);  // Shared across all threads

// Each neuron thread updates coherence
func:update_neuron = void(int64:neuron_id) {
    // Compute this neuron's contribution
    complex<fix256>:wave = compute_wave(neuron_id);
    fix256:contribution = wave.magnitude();
    
    // ❌ DATA RACE HERE!
    global_coherence = global_coherence + contribution;
    // Multiple threads reading/writing simultaneously
    // Lost updates → incorrect coherence metric
    // Consciousness substrate makes decisions on corrupted data
    // Potential "AI hallucinations" or divergent mental states
};
```

**Consequences**:
- Lost updates: Many neuron contributions disappear (coherence underestimated)
- Torn reads: Thread reads half-old, half-new value (nonsense data)
- Ordering violations: Neurons see inconsistent states across threads
- **Mental state corruption**: Consciousness substrate diverges from ground truth
- **Cascading failure**: Corrupted coherence → bad decisions → more corruption

### The Solution: Atomic Operations

```aria
// SAFE VERSION with atomic<T>
atomic<fix256>:global_coherence = atomic_new(fix256(0));

func:update_neuron_safe = void(int64:neuron_id) {
    complex<fix256>:wave = compute_wave(neuron_id);
    fix256:contribution = wave.magnitude();
    
    // ✅ THREAD-SAFE atomic update
    // Read-modify-write is indivisible (no other thread can interfere)
    fix256:old_coherence = global_coherence.load();
    fix256:new_coherence = old_coherence + contribution;
    
    // CAS (Compare-And-Swap) ensures no lost updates
    while (!global_coherence.compare_exchange(old_coherence, new_coherence)) {
        // Another thread modified coherence - retry
        old_coherence = global_coherence.load();
        new_coherence = old_coherence + contribution;
    }
    // Guaranteed: All contributions accounted for
};
```

---

## Type Definition & Memory Layout

### Generic Structure

```aria
// Generic atomic wrapper for any type T
struct:atomic<T> =
    *T:value;         // The wrapped value
    mutex:lock;       // Optional lock for large types (transparent to user)
    bool:is_lock_free; // Whether hardware supports lock-free ops
end

// User never touches internals - atomic operations are built-in methods
```

### Lock-Free vs Lock-Based

Aria automatically chooses the implementation based on type size:

| Type Size | Implementation | Performance |
|-----------|----------------|-------------|
| ≤ 8 bytes (64 bits) | **Lock-free** (CPU atomic instructions) | ~1-30 cycles |
| 9-16 bytes | **Lock-free** (DWCAS if available, else locked) | ~10-50 cycles or locked |
| > 16 bytes | **Lock-based** (spinlock fallback) | ~100+ cycles |

```aria
// Lock-free (single CPU instruction)
atomic<int8>:tiny;              // 1 byte  → LOCK-FREE
atomic<int32>:small;            // 4 bytes → LOCK-FREE
atomic<int64>:medium;           // 8 bytes → LOCK-FREE
atomic<Handle<T>>:handle;       // 8 bytes (index+gen) → LOCK-FREE

// Lock-free on x86-64 with DWCAS (double-width CAS)
atomic<int128>:larger;          // 16 bytes → LOCK-FREE (DWCAS)

// Lock-based fallback (spinlock)
atomic<fix256>:big;                    // 32 bytes → LOCKED
atomic<complex<fix256>>:huge;          // 64 bytes → LOCKED
atomic<int1024>:cryptographic_key;     // 128 bytes → LOCKED

// Still thread-safe! Just not lock-free (performance cost)
```

**Query Lock-Free Status**:
```aria
atomic<fix256>:energy = atomic_new(fix256(0));

if energy.is_lock_free() then
    dbug.concurrency("Energy counter is lock-free (hardware atomic)\n");
else
    dbug.concurrency("Energy counter uses spinlock\n");
    // Still safe, just potential contention under high load
end
```

---

## Memory Ordering Models

### The Problem: CPUs Reorder Instructions

Modern CPUs and compilers **reorder memory operations** for performance. What you write is not always the order executed:

```aria
// What you write:
int64:data = 42;       // 1. Write data
bool:ready = true;     // 2. Set flag

// What CPU might execute:
bool:ready = true;     // 2. Set flag FIRST (reordered!)
int64:data = 42;       // 1. Write data SECOND

// Another thread checking ready might see true but uninitialized data!
```

**Memory ordering** specifies constraints on reordering to ensure correctness across threads.

### The Five Ordering Levels (Weakest → Strongest)

#### 1. Relaxed - NO ORDERING GUARANTEES (Unsafe)

**Guarantee**: Only the atomic operation itself is indivisible. No ordering constraints on surrounding operations.

**Use When**: Order truly doesn't matter (e.g., statistics counters, profiling).

```aria
unsafe {
    atomic<int64>:page_views = atomic_new(0);
    
    // Increment without caring about order relative to other ops
    page_views.fetch_add_relaxed(1);
    
    // Fine for metrics, NOT for synchronization!
}
```

**Danger**: Cannot be used to establish happens-before relationships. If you synchronize with Relaxed, you **will** have data races.

#### 2. Acquire - One-Way Barrier (Loads Can't Move Up) (Unsafe)

**Guarantee**: All memory operations **AFTER** this load stay after it. Earlier operations may move later, but later operations cannot move earlier.

**Use When**: Acquiring a lock or reading a synchronization flag.

```aria
unsafe {
    atomic<bool>:ready = atomic_new(false);
    int64:data;  // Non-atomic
    
    // Reader thread
    while (!ready.load_acquire()) { }  // Acquire fence
    // Data reads here CANNOT move before the acquire
    int64:value = data;  // Guaranteed to see writer's data
}
```

**Paired With**: Release (writer uses Release, reader uses Acquire).

#### 3. Release - One-Way Barrier (Stores Can't Move Down) (Unsafe)

**Guarantee**: All memory operations **BEFORE** this store stay before it. Later operations may move earlier, but earlier operations cannot move later.

**Use When**: Releasing a lock or setting a synchronization flag.

```aria
unsafe {
    atomic<bool>:ready = atomic_new(false);
    int64:data;  // Non-atomic
    
    // Writer thread
    data = 42;  // Non-atomic write
    ready.store_release(true);  // Release fence
    // Data write CANNOT move after the release
    // Reader using Acquire will see data = 42
}
```

**Paired With**: Acquire (writer uses Release, reader uses Acquire).

#### 4. AcqRel - Two-Way Barrier (Acquire + Release) (Unsafe)

**Guarantee**: Combines Acquire and Release. Operations before stay before, operations after stay after.

**Use When**: Read-modify-write operations (like fetch_add, compare_exchange) that both read and write.

```aria
unsafe {
    atomic<int64>:sequence = atomic_new(0);
    int64:buffer[1000];
    
    // Thread incrementing sequence and writing buffer
    int64:slot = sequence.fetch_add_acqrel(1);  // Acquire+Release
    buffer[slot] = compute_data();
    
    // Acquire: Ensures buffer read sees previous writes
    // Release: Ensures buffer write visible to next reader
}
```

#### 5. SeqCst - Sequential Consistency (STRONGEST, DEFAULT)

**Guarantee**: All SeqCst operations appear to execute in a single global total order across all threads. Matches programmer intuition.

**Use When**: Default choice. Only use weaker orderings if profiling shows bottleneck AND you understand memory models.

```aria
// NO unsafe needed - this is the SAFE default
atomic<int64>:x = atomic_new(0);
atomic<int64>:y = atomic_new(0);

// Thread 1
x.store(1);  // SeqCst by default
y.store(2);  // SeqCst by default

// Thread 2
if y.load() == 2 then  // SeqCst by default
    // x.load() is GUARANTEED to be 1
    // Sequential consistency: All threads agree on operation order
end
```

**Cost**: Memory fences on most platforms (~10-30 cycles vs 1 cycle for Relaxed).

**Worth It**: Prevents subtle bugs that only manifest under specific timing (Heisenbugs).

### Summary Table

| Ordering | Prevents Reordering | Use Case | Unsafe? |
|----------|---------------------|----------|---------|
| **Relaxed** | None (atomic op only) | Counters where order irrelevant | ✅ YES |
| **Acquire** | Prevents loads/stores moving before | Lock acquisition, flag read | ✅ YES |
| **Release** | Prevents loads/stores moving after | Lock release, flag write | ✅ YES |
| **AcqRel** | Both Acquire + Release | Read-modify-write ops | ✅ YES |
| **SeqCst** | Global total order (strongest) | DEFAULT - always safe | ❌ NO |

**Aria's Design**: Default to SeqCst (sequential consistency) for safety. Weaker orderings require `unsafe` block + comment explaining why it's correct.

---

## Atomic Operations Reference

### Load (Read)

```aria
atomic<int64>:counter = atomic_new(42);

// SeqCst load (SAFE, default)
int64:value = counter.load();

// Acquire load (UNSAFE - weaker ordering)
unsafe {
    // Prevents operations after this from moving before
    int64:value = counter.load_acquire();
}

// Relaxed load (UNSAFE - no ordering)
unsafe {
    // Only guarantees read is atomic, no synchronization
    int64:value = counter.load_relaxed();
}
```

### Store (Write)

```aria
atomic<int64>:counter = atomic_new(0);

// SeqCst store (SAFE, default)
counter.store(100);

// Release store (UNSAFE - weaker ordering)
unsafe {
    // Prevents operations before this from moving after
    counter.store_release(100);
}

// Relaxed store (UNSAFE - no ordering)
unsafe {
    // Only guarantees write is atomic, no synchronization
    counter.store_relaxed(100);
}
```

### Swap (Exchange)

```aria
atomic<int64>:value = atomic_new(10);

// Atomic exchange - returns old value, writes new value
int64:old = value.swap(20);  // SeqCst default
// old == 10, value now 20

// AcqRel swap (UNSAFE)
unsafe {
    int64:old = value.swap_acqrel(30);
}

// Acquire swap (UNSAFE - acquire semantics on read)
unsafe {
    int64:old = value.swap_acquire(40);
}

// Release swap (UNSAFE - release semantics on write)
unsafe {
    int64:old = value.swap_release(50);
}
```

### Compare-Exchange (CAS - Compare-And-Swap)

The **most powerful atomic operation** - foundation of lock-free algorithms.

```aria
atomic<int64>:counter = atomic_new(5);

int64:expected = 5;   // What we think the value is
int64:desired = 10;   // What we want to change it to

// Atomically:
// if (counter == expected) {
//     counter = desired;
//     return true;
// } else {
//     expected = counter;  // Update expected to current value
//     return false;
// }

bool:success = counter.compare_exchange(expected, desired);

if success then
    // Counter was 5, now it's 10
    dbug.concurrency("CAS succeeded\n");
else
    // Counter was NOT 5 (expected now contains actual value)
    dbug.concurrency("CAS failed, actual value: {}\n", expected);
end
```

**CAS Loop Pattern** (retry until success):
```aria
atomic<fix256>:energy = atomic_new(fix256(100));

// Add 10 to energy, handling contention
fix256:delta = fix256(10);

fix256:old_energy = energy.load();
fix256:new_energy = old_energy + delta;

while !energy.compare_exchange(old_energy, new_energy) loop
    // Another thread modified energy between our load and CAS
    // old_energy now contains the current value - recalculate
    new_energy = old_energy + delta;
    // Retry CAS
end

// Success! energy increased by exactly delta
```

**Weak CAS** (may fail spuriously - use in loops):
```aria
unsafe {
    atomic<int64>:counter = atomic_new(0);
    
    int64:expected = 0;
    int64:desired = 1;
    
    // compare_exchange_weak may fail even if expected == counter
    // (CPU cache effects, speculative execution, etc.)
    // MUST be used in a loop
    while !counter.compare_exchange_weak_acqrel(expected, desired) loop
        expected = 0;  // Reset expected
        // Retry
    end
}
```

**When to Use Weak**: On architectures like ARM where LL/SC (load-linked/store-conditional) may fail spuriously. Strong CAS internally loops on weak CAS, so if you're already looping, use weak directly (slight perf gain).

### Fetch-And-Modify (Atomic Read-Modify-Write)

```aria
atomic<int64>:counter = atomic_new(10);

// All operations return OLD value before modification

// Arithmetic
int64:old = counter.fetch_add(5);   // old=10, counter now 15
int64:old = counter.fetch_sub(3);   // old=15, counter now 12

// Bitwise
int64:old = counter.fetch_and(0xFF);  // Bitwise AND
int64:old = counter.fetch_or(0x10);   // Bitwise OR
int64:old = counter.fetch_xor(0x01);  // Bitwise XOR

// Min/Max (useful for range tracking)
int64:old = counter.fetch_min(5);   // Sets counter to min(counter, 5)
int64:old = counter.fetch_max(20);  // Sets counter to max(counter, 20)

// Unsafe variants with weaker ordering
unsafe {
    int64:old = counter.fetch_add_acqrel(1);
    int64:old = counter.fetch_add_release(1);
    int64:old = counter.fetch_add_acquire(1);
    int64:old = counter.fetch_add_relaxed(1);  // Fastest, no ordering
}
```

**Why Return Old Value?**: Enables building complex logic. If you need the new value, just add the delta yourself:

```aria
int64:old = counter.fetch_add(5);
int64:new = old + 5;  // You now know both old and new
```

---

## Memory Fences (Explicit Barriers)

Sometimes you need synchronization without an atomic operation:

```aria
// Acquire fence - prevents operations AFTER from moving BEFORE
atomic_fence_acquire();

// Release fence - prevents operations BEFORE from moving AFTER
atomic_fence_release();

// AcqRel fence - both directions
atomic_fence_acqrel();

// SeqCst fence - full sequential consistency barrier
atomic_fence_seqcst();
```

**Use Case**: Optimizing flag-based synchronization with Relaxed operations:

```aria
unsafe {
    int64:data = 0;  // Non-atomic
    atomic<bool>:ready = atomic_new(false);
    
    // Writer thread
    data = 42;                    // 1. Write data
    atomic_fence_release();       // 2. Ensure step 1 completes before step 3
    ready.store_relaxed(true);    // 3. Set flag (Relaxed is fast!)
    
    // Reader thread
    while !ready.load_relaxed() { }  // 1. Spin until flag set (Relaxed is fast!)
    atomic_fence_acquire();          // 2. Ensure step 1 completes before step 3
    int64:value = data;              // 3. Read data (guaranteed to see 42)
}
```

**Why This Pattern?**: Avoids memory fences in the fast path (spin loop). Fences only execute once at synchronization points.

---

## Lock-Free Algorithms & Patterns

### 1. Lock-Free Counter (Trivial)

```aria
atomic<int64>:counter = atomic_new(0);

// Increment (thread-safe)
func:increment = void() {
    counter.fetch_add(1);  // Single atomic op - lock-free!
};

// Decrement
func:decrement = void() {
    counter.fetch_sub(1);
};

// Read
func:get_count = int64() {
    pass(counter.load());
};
```

### 2. Lock-Free Stack (Treiber Stack)

```aria
struct:Node<T> =
    *T:value;
    Handle<Node<*T>>:next;  // NOT atomic - node is immutable after creation
end

struct:LockFreeStack<T> =
    atomic<Handle<Node<*T>>>:head;  // Atomic handle to top of stack
    arena<Node<*T>>:node_arena;
end

func<T>:push = void(LockFreeStack<*T>@:stack, *T:value) {
    // Allocate new node
    Handle<Node<*T>>:new_node_handle = stack->node_arena.alloc({
        value: value,
        next: nil_handle<Node<*T>>(),  // Will be set in CAS loop
    });
    
    // CAS loop: Make new_node point to current head, then swap
    Handle<Node<*T>>:old_head = stack->head.load();
    
    till MAX_RETRIES loop
        // Update new node's next pointer to current head
        Node<*T>@:new_node = stack->node_arena.get_mut(new_node_handle) ?! fail("Invalid handle");
        new_node->next = old_head;
        
        // Try to swap head to new_node
        if stack->head.compare_exchange(old_head, new_node_handle) then
            pass();  // Success!
        end
        
        // CAS failed - another thread pushed. old_head now contains new head. Retry.
    end
    
    !!! ERR_LOCKFREE_STACK_CONTENTION("Push failed after MAX_RETRIES");
};

func<T>:pop = Result<*T>(LockFreeStack<*T>@:stack) {
    till MAX_RETRIES loop
        Handle<Node<*T>>:old_head = stack->head.load();
        
        if old_head == nil_handle<Node<*T>>() then
            fail("Stack empty");  // Stack is empty
        end
        
        // Read the node the head points to
        Node<*T>:head_node = stack->node_arena.get(old_head) ?! fail("Invalid head handle");
        Handle<Node<*T>>:new_head = head_node.next;
        
        // Try to swap head to next
        if stack->head.compare_exchange(old_head, new_head) then
            // Success! Return the value
            pass(head_node.value);
        end
        
        // CAS failed - another thread modified head. Retry.
    end
    
    fail("Pop failed after MAX_RETRIES");
};
```

**ABA Problem**: If thread 1 reads head (A), gets preempted, thread 2 pops A, pops B, pushes A again, then thread 1's CAS succeeds even though A is a *different* A! 

**Solution**: **Generational handles** (`Handle<T>`) solve this! Each allocation increments generation, so even if the same slot is reused, the handle won't match.

### 3. Lock-Free Queue (Michael-Scott Queue)

```aria
struct:QueueNode<T> =
    *T:value;
    atomic<Handle<QueueNode<*T>>>:next;  // Atomic pointer to next node
end

struct:LockFreeQueue<T> =
    atomic<Handle<QueueNode<*T>>>:head;  // Dequeue from head
    atomic<Handle<QueueNode<*T>>>:tail;  // Enqueue at tail
    arena<QueueNode<*T>>:node_arena;
end

func<T>:queue_new = LockFreeQueue<*T>() {
    // Allocate dummy node (sentinel)
    Handle<QueueNode<*T>>:dummy_handle = node_arena.alloc({
        value: default<*T>(),
        next: atomic_new(nil_handle<QueueNode<*T>>()),
    });
    
    LockFreeQueue<*T>:queue = {
        head: atomic_new(dummy_handle),
        tail: atomic_new(dummy_handle),
        node_arena: arena_new<QueueNode<*T>>(10000),
    };
    
    pass(queue);
};

func<T>:enqueue = void(LockFreeQueue<*T>@:queue, *T:value) {
    // Allocate new node
    Handle<QueueNode<*T>>:new_node_handle = queue->node_arena.alloc({
        value: value,
        next: atomic_new(nil_handle<QueueNode<*T>>()),
    });
    
    till MAX_RETRIES loop
        Handle<QueueNode<*T>>:tail_handle = queue->tail.load();
        QueueNode<*T>@:tail_node = queue->node_arena.get_mut(tail_handle) ?! fail("Invalid tail");
        
        Handle<QueueNode<*T>>:next_handle = tail_node->next.load();
        
        // Is tail actually the last node?
        if next_handle == nil_handle<QueueNode<*T>>() then
            // Try to link new node at end
            if tail_node->next.compare_exchange(next_handle, new_node_handle) then
                // Success! Try to swing tail to new node (best effort)
                queue->tail.compare_exchange(tail_handle, new_node_handle);
                pass();
            end
        else
            // Tail is lagging - help advance it
            queue->tail.compare_exchange(tail_handle, next_handle);
        end
    end
    
    !!! ERR_LOCKFREE_QUEUE_CONTENTION("Enqueue failed");
};

func<T>:dequeue = Result<*T>(LockFreeQueue<*T>@:queue) {
    till MAX_RETRIES loop
        Handle<QueueNode<*T>>:head_handle = queue->head.load();
        Handle<QueueNode<*T>>:tail_handle = queue->tail.load();
        
        QueueNode<*T>:head_node = queue->node_arena.get(head_handle) ?! fail("Invalid head");
        Handle<QueueNode<*T>>:next_handle = head_node.next.load();
        
        // Is queue empty?
        if next_handle == nil_handle<QueueNode<*T>>() then
            fail("Queue empty");
        end
        
        // Is head lagging behind tail?
        if head_handle == tail_handle then
            // Queue not empty but tail is lagging - help advance it
            queue->tail.compare_exchange(tail_handle, next_handle);
            continue;  // Retry
        end
        
        // Read value from next node (head is dummy!)
        QueueNode<*T>:next_node = queue->node_arena.get(next_handle) ?! fail("Invalid next");
        *T:value = next_node.value;
        
        // Try to swing head to next
        if queue->head.compare_exchange(head_handle, next_handle) then
            // Success! old head is now unreachable (can be freed later)
            pass(value);
        end
    end
    
    fail("Dequeue failed");
};
```

**Key Insight**: Head and tail might lag behind reality. Threads "help" each other by advancing pointers even if they didn't enqueue/dequeue.

### 4. Atomic Reference Counting

```aria
struct:ArcNode<T> =
    *T:value;
    atomic<int64>:ref_count;
end

struct:Arc<T> =
    Handle<ArcNode<*T>>:node;
    arena<ArcNode<*T>>@:arena;  // Shared arena reference
end

func<T>:arc_new = Arc<*T>(arena<ArcNode<*T>>@:shared_arena, *T:value) {
    Handle<ArcNode<*T>>:node_handle = shared_arena->alloc({
        value: value,
        ref_count: atomic_new(1),  // Start with 1 reference
    });
    
    Arc<*T>:arc = {
        node: node_handle,
        arena: shared_arena,
    };
    
    pass(arc);
};

func<T>:arc_clone = Arc<*T>(Arc<*T>:arc) {
    // Increment ref count atomically
    ArcNode<*T>@:node = arc.arena->get_mut(arc.node) ?! fail("Invalid Arc");
    node->ref_count.fetch_add(1);
    
    // Return new Arc pointing to same node
    pass(arc);  // Shallow copy with incremented count
};

func<T>:arc_drop = void(Arc<*T>:arc) {
    ArcNode<*T>@:node = arc.arena->get_mut(arc.node) ?! fail("Invalid Arc");
    
    // Decrement ref count atomically
    int64:old_count = node->ref_count.fetch_sub(1);
    
    if old_count == 1 then
        // We were the last reference - free the node
        arc.arena->free(arc.node);
    end
};
```

---

## Nikola Consciousness Substrate: Atomic Patterns

### 1. Global Coherence Metric (Atomic Accumulation)

```aria
// Thousands of neuron threads updating single coherence value
atomic<fix256>:global_coherence = atomic_new(fix256(0));

func:update_coherence_contribution = void(fix256:delta) {
    // CAS loop for atomic addition (fix256 too large for fetch_add)
    fix256:old_coherence = global_coherence.load();
    fix256:new_coherence = old_coherence + delta;
    
    till MAX_RETRIES loop
        if global_coherence.compare_exchange(old_coherence, new_coherence) then
            pass();  // Success!
        end
        
        // Retry with updated value
        new_coherence = old_coherence + delta;
    end
    
    !!! ERR_COHERENCE_UPDATE_FAILED("Too much contention on global coherence");
};

// Called from thousands of threads simultaneously
// No lost updates - every neuron contribution counts
```

### 2. Phase-Locked Loop Synchronization

```aria
// Multiple oscillator threads synchronize phase
atomic<fix256>:reference_phase = atomic_new(fix256(0));

func:synchronize_phase = fix256(fix256:local_phase) {
    fix256:ref_phase = reference_phase.load();
    
    // Compute phase error
    fix256:phase_error = ref_phase - local_phase;
    
    // Adjust local phase toward reference (phase locking)
    fix256:correction = phase_error * fix256(0, 1);  // 10% correction rate
    
    pass(local_phase + correction);
};

func:update_reference_phase = void(fix256:master_phase) {
    reference_phase.store(master_phase);  // Atomic publish
};
```

### 3. Neural Activation Barriers (Wait for All Threads)

```aria
struct:Barrier =
    atomic<int64>:arrived;   // How many threads have arrived
    atomic<int64>:epoch;     // Barrier generation (prevents reuse issues)
    int64:required;          // Total threads required
end

func:barrier_wait = void(Barrier@:barrier) {
    // Increment arrived count
    int64:current_epoch = barrier->epoch.load();
    int64:arrived_count = barrier->arrived.fetch_add(1);
    
    if arrived_count + 1 == barrier->required then
        // Last thread to arrive - reset barrier for next use
        barrier->arrived.store(0);
        barrier->epoch.fetch_add(1);  // Advance epoch
    else
        // Not last - spin until epoch changes
        while barrier->epoch.load() == current_epoch loop
            // Spin (could use futex or condition variable in real impl)
        end
    end
};

// Usage: Synchronize neural update timesteps
Barrier:timestep_barrier = {
    arrived: atomic_new(0),
    epoch: atomic_new(0),
    required: NEURON_THREAD_COUNT,
};

func:neuron_thread = void(int64:thread_id) {
    till SIMULATION_STEPS loop
        // Compute neural state for this timestep
        update_neuron_state(thread_id);
        
        // Wait for all neurons to finish this timestep
        barrier_wait(timestep_barrier);
        
        // Continue to next timestep
    end
};
```

### 4. Lock-Free Event Log (Monotonic Counter)

```aria
// Append-only log of consciousness events
atomic<int64>:log_sequence = atomic_new(0);
Event[MAX_LOG_SIZE]:event_log;

func:log_event = void(Event:event) {
    // Claim next slot atomically
    int64:slot = log_sequence.fetch_add(1);
    
    if slot >= MAX_LOG_SIZE then
        stderr.write("Event log full!\n");
        !!! ERR_LOG_OVERFLOW;
    end
    
    // Write event (no contention - each thread has unique slot)
    event_log[slot] = event;
};

// Readers can scan up to log_sequence.load() without locks
```

---

## Integration with Other Aria Types

### Atomic with Handle<T> (Solves ABA Problem)

```aria
// DON'T use atomic<T@> (atomic pointer - ABA problem!)
// DO use atomic<Handle<T>> (generation prevents ABA)

atomic<Handle<Node>>:head = atomic_new(nil_handle<Node>());

// Thread 1 reads handle (index=5, gen=1)
Handle<Node>:old_head = head.load();  // {index: 5, generation: 1}

// Thread 2 frees node 5, allocates new node at slot 5
// New node has generation=2

// Thread 1 tries CAS with old_head
head.compare_exchange(old_head, new_node);  // FAILS!
// old_head.generation (1) != current.generation (2)
// ABA problem prevented!
```

### Atomic with Complex<T> (Quantum Wave Synchronization)

```aria
// Locked fallback (complex<fix256> is 64 bytes)
atomic<complex<fix256>>:global_wavefunction = atomic_new({
    real: fix256(1),
    imag: fix256(0),
});

// Thread-safe update
func:apply_phase_rotation = void(fix256:angle) {
    complex<fix256>:old_psi = global_wavefunction.load();
    
    // Compute rotation: psi' = psi * e^(i*angle)
    complex<fix256>:rotation = {
        real: cos(angle),
        imag: sin(angle),
    };
    
    complex<fix256>:new_psi = old_psi * rotation;
    
    // CAS loop
    while !global_wavefunction.compare_exchange(old_psi, new_psi) loop
        // Recalculate with updated psi
        new_psi = old_psi * rotation;
    end
};
```

### Atomic with ERR Propagation (tbb Types)

```aria
// ERR sentinel remains atomic
atomic<tbb64>:error_status = atomic_new(0t64);

// Thread 1 sets ERR
error_status.store(ERR);

// Thread 2 reads ERR
tbb64:status = error_status.load();  // status == ERR

// ERR propagates through arithmetic
tbb64:result = status + 10t64;  // result == ERR (sticky!)

// All threads see consistent ERR state
```

### Atomic with Q9<T> (Quantum Decision State)

```aria
// Atomic quantum superposition state (lock-based - Q9 is large)
atomic<Q9<fix256>>:decision_state = atomic_new(q9_uniform<fix256>());

func:accumulate_evidence = void(int32:state_index, fix256:evidence) {
    Q9<fix256>:old_q9 = decision_state.load();
    Q9<fix256>:new_q9 = old_q9;
    
    // Add evidence to specific state
    new_q9.states[state_index] = new_q9.states[state_index] + evidence;
    
    // CAS loop
    while !decision_state.compare_exchange(old_q9, new_q9) loop
        new_q9 = old_q9;
        new_q9.states[state_index] = new_q9.states[state_index] + evidence;
    end
};

// Multiple threads accumulate evidence concurrently
// When confidence high enough: crystallize decision
if decision_state.load().confidence > fix256(0, 95) then
    int32:decision = decision_state.load().crystallize();
    dbug.decision("Converged to state: {}\n", decision);
end
```

---

## Performance Characteristics & Optimization

### Hardware Costs (Approximate, Architecture-Dependent)

| Operation | SeqCst | AcqRel | Acquire | Release | Relaxed |
|-----------|--------|--------|---------|---------|---------|
| Load (lock-free) | 10-30 cycles | - | 3-10 cycles | - | 1 cycle |
| Store (lock-free) | 10-30 cycles | - | - | 3-10 cycles | 1 cycle |
| Fetch-Add (lock-free) | 20-50 cycles | 10-20 cycles | - | - | 5-10 cycles |
| CAS (lock-free) | 20-50 cycles | 10-20 cycles | - | - | 5-10 cycles |
| Lock-based operation | 100-500+ cycles (depends on contention) | | | | |

**Takeaway**: SeqCst is ~10× slower than Relaxed, but prevents subtle bugs. Only optimize after profiling shows bottleneck.

### Reducing Contention

#### 1. Separate Counters (Reduce False Sharing)

```aria
// ❌ BAD: All threads fighting over same cache line
atomic<int64>:global_counter = atomic_new(0);

till THREAD_COUNT loop
    spawn_thread(|| {
        till 1000000 loop
            global_counter.fetch_add(1);  // Cache line bouncing!
        end
    });
end

// ✅ GOOD: Per-thread counters, sum at end
atomic<int64>[THREAD_COUNT]:per_thread_counters;

till THREAD_COUNT loop
    int64:thread_id = $;
    spawn_thread(|| {
        till 1000000 loop
            per_thread_counters[thread_id].fetch_add(1);  // No contention!
        end
    });
end

// Sum at the end (one-time cost)
int64:total = 0;
till THREAD_COUNT loop
    total = total + per_thread_counters[$].load();
end
```

#### 2. Backoff Strategies

```aria
func:cas_with_exponential_backoff = bool(
    atomic<int64>@:counter,
    int64@:expected,
    int64:desired
) {
    int64:backoff_cycles = 1;
    
    till MAX_RETRIES loop
        if counter->compare_exchange(expected, desired) then
            pass(true);  // Success!
        end
        
        // Exponential backoff: wait before retrying
        till backoff_cycles loop end  // Spin wait
        backoff_cycles = backoff_cycles * 2;
        
        if backoff_cycles > 1024 then
            backoff_cycles = 1024;  // Cap backoff
        end
    end
    
    pass(false);  // Failed after MAX_RETRIES
};
```

#### 3. Batching (Amortize Atomic Costs)

```aria
// Instead of atomic increment per item:
till 1000000 loop
    global_counter.fetch_add(1);  // 1M atomic ops!
end

// Batch local increments, flush periodically:
int64:local_count = 0;

till 1000000 loop
    local_count = local_count + 1;
    
    if local_count % 1000 == 0 then
        // Flush every 1000 items
        global_counter.fetch_add(local_count);
        local_count = 0;
    end
end

// Final flush
if local_count > 0 then
    global_counter.fetch_add(local_count);
end

// 1M increments → only 1000 atomic ops (1000× reduction!)
```

---

## Best Practices

### ✅ DO: Default to SeqCst (Sequential Consistency)

```aria
// SAFE - no unsafe block needed
atomic<int64>:counter = atomic_new(0);
counter.store(42);         // SeqCst by default
int64:value = counter.load();  // SeqCst by default

// Prevents all race conditions, matches programmer intuition
```

### ✅ DO: Use CAS Loops for Complex Updates

```aria
atomic<fix256>:balance = atomic_new(fix256(100));

// Withdraw 10, but only if balance >= 10
fix256:withdraw_amount = fix256(10);

fix256:old_balance = balance.load();
till MAX_RETRIES loop
    if old_balance < withdraw_amount then
        fail("Insufficient balance");
    end
    
    fix256:new_balance = old_balance - withdraw_amount;
    
    if balance.compare_exchange(old_balance, new_balance) then
        pass();  // Success!
    end
    
    // Retry with updated balance
end

!!! ERR_WITHDRAWAL_CONTENTION("Too many threads withdrawing");
```

### ✅ DO: Use atomic<Handle<T>> for Lock-Free Data Structures

```aria
// Generational handles prevent ABA problem
atomic<Handle<Node>>:stack_top = atomic_new(nil_handle<Node>());

// Safe even if same slot reused - generation mismatch causes CAS to fail
```

### ✅ DO: Document Why Weaker Orderings Are Safe

```aria
unsafe {
    // SAFETY: Flag is only used to signal thread shutdown
    // Order doesn't matter - thread exits on next check regardless
    shutdown_flag.store_relaxed(true);
}
```

### ✅ DO: Profile Before Optimizing Memory Ordering

```aria
// Start with SeqCst everywhere
// Profile with real workload
// Only if atomics show up as >10% of runtime:
//   - Consider weaker orderings (with proof of correctness)
//   - Consider reducing contention (per-thread counters, batching)
```

---

## Common Pitfalls & Antipatterns

### ❌ DON'T: Assume Non-Atomic Vars Are Atomic

```aria
// ❌ WRONG: Data race!
int64:counter = 0;  // NOT atomic

// Thread 1
counter = counter + 1;  // RACE CONDITION

// Thread 2
counter = counter + 1;  // RACE CONDITION

// ✅ CORRECT:
atomic<int64>:counter = atomic_new(0);

// Thread 1
counter.fetch_add(1);  // Thread-safe

// Thread 2
counter.fetch_add(1);  // Thread-safe
```

### ❌ DON'T: Mix Atomic and Non-Atomic Access

```aria
atomic<int64>:value = atomic_new(0);

// ❌ WRONG: Accessing atomic as if non-atomic
// value.value = 10;  // COMPILE ERROR (no direct access)

// ✅ CORRECT: Always use atomic methods
value.store(10);
```

### ❌ DON'T: Forget to Check CAS Return Value

```aria
atomic<int64>:counter = atomic_new(5);

int64:expected = 5;
int64:desired = 10;

// ❌ WRONG: Ignoring return value
counter.compare_exchange(expected, desired);
// Did it succeed? You don't know!

// ✅ CORRECT: Check success
if counter.compare_exchange(expected, desired) then
    dbug.concurrency("CAS succeeded\n");
else
    dbug.concurrency("CAS failed, value was {}\n", expected);
end
```

### ❌ DON'T: Use Relaxed for Synchronization

```aria
unsafe {
    int64:data = 0;  // Non-atomic
    atomic<bool>:ready = atomic_new(false);
    
    // Writer thread
    data = 42;
    ready.store_relaxed(true);  // ❌ WRONG: No ordering guarantee!
    
    // Reader thread
    while !ready.load_relaxed() { }
    int64:value = data;  // ❌ MIGHT SEE 0 (data write can be reordered!)
}

// ✅ CORRECT: Use Release/Acquire or SeqCst
int64:data = 0;
atomic<bool>:ready = atomic_new(false);

// Writer thread
data = 42;
ready.store(true);  // SeqCst ensures ordering

// Reader thread
while !ready.load() { }  // SeqCst ensures ordering
int64:value = data;  // Guaranteed to see 42
```

### ❌ DON'T: Over-Use Atomics (False Sharing)

```aria
// ❌ BAD: Atomics on same cache line (64 bytes)
struct:Counters =
    atomic<int64>:counter1;  // Offset 0
    atomic<int64>:counter2;  // Offset 8
    atomic<int64>:counter3;  // Offset 16
    // All in same cache line - false sharing!
end

// Thread 1 updates counter1 → cache line invalidated for Thread 2
// Thread 2 updates counter2 → cache line invalidated for Thread 1
// Constant cache thrashing!

// ✅ GOOD: Pad to separate cache lines
struct:Counters =
    atomic<int64>:counter1;
    uint8[56]:padding1;  // Pad to 64 bytes
    atomic<int64>:counter2;
    uint8[56]:padding2;
    atomic<int64>:counter3;
    uint8[56]:padding3;
end

// Each counter on separate cache line - no false sharing
```

### ❌ DON'T: Use atomic<T@> (Atomic Pointers Have ABA)

```aria
// ❌ DANGEROUS: ABA problem with raw pointers
atomic<Node@>:stack_top;

// Thread 1 reads top (points to A)
Node@:old_top = stack_top.load();  // A

// Thread 2 pops A, pops B, pushes A again
// stack_top now points to A (same address!)

// Thread 1 CAS succeeds even though A is different!
stack_top.compare_exchange(old_top, new_node);  // BAD!

// ✅ SAFE: Use atomic<Handle<T>> (generation prevents ABA)
atomic<Handle<Node>>:stack_top;

// Thread 1 reads top (index=5, gen=1)
Handle<Node>:old_top = stack_top.load();

// Thread 2 pops, node at slot 5 reallocated with gen=2

// Thread 1 CAS fails! (gen mismatch)
stack_top.compare_exchange(old_top, new_node);  // Correctly fails
```

### ❌ DON'T: Spin Forever Without Backoff

```aria
// ❌ BAD: Burns CPU spinning
atomic<bool>:ready = atomic_new(false);

while !ready.load() loop
    // Spin at full CPU speed - wastes power!
end

// ✅ BETTER: Exponential backoff or yield
int64:backoff = 1;
while !ready.load() loop
    till backoff loop end  // Spin-wait
    backoff = backoff * 2;
    if backoff > 1024 then backoff = 1024; end
end

// ✅ BEST: Use futex/condition variable (OS-level blocking)
// (Implementation detail - Aria runtime handles this)
```

---

## Implementation Notes

### C Runtime Functions

```c
// Core atomic operations (in aria_runtime.c)

// Load/Store
int64_t aria_atomic_load_i64_seqcst(aria_atomic_i64* ptr);
void aria_atomic_store_i64_seqcst(aria_atomic_i64* ptr, int64_t value);

// Unsafe orderings
int64_t aria_atomic_load_i64_acquire(aria_atomic_i64* ptr);
void aria_atomic_store_i64_release(aria_atomic_i64* ptr, int64_t value);
int64_t aria_atomic_load_i64_relaxed(aria_atomic_i64* ptr);
void aria_atomic_store_i64_relaxed(aria_atomic_i64* ptr, int64_t value);

// Swap
int64_t aria_atomic_swap_i64_seqcst(aria_atomic_i64* ptr, int64_t value);

// Compare-Exchange
bool aria_atomic_cmpxchg_i64_seqcst(
    aria_atomic_i64* ptr,
    int64_t* expected,  // IN/OUT parameter
    int64_t desired
);

bool aria_atomic_cmpxchg_weak_i64_acqrel(
    aria_atomic_i64* ptr,
    int64_t* expected,
    int64_t desired
);

// Fetch-And-Modify
int64_t aria_atomic_fetch_add_i64_seqcst(aria_atomic_i64* ptr, int64_t delta);
int64_t aria_atomic_fetch_sub_i64_seqcst(aria_atomic_i64* ptr, int64_t delta);

// Memory fences
void aria_atomic_fence_acquire(void);
void aria_atomic_fence_release(void);
void aria_atomic_fence_acqrel(void);
void aria_atomic_fence_seqcst(void);

// Lock-based fallback for large types
void aria_atomic_spinlock_acquire(aria_spinlock* lock);
void aria_atomic_spinlock_release(aria_spinlock* lock);
```

### LLVM IR Generation

```llvm
; Atomic load (SeqCst)
define i64 @aria.atomic.load.i64.seqcst(%atomic.i64* %ptr) {
  %value = load atomic i64, %atomic.i64* %ptr seq_cst, align 8
  ret i64 %value
}

; Atomic store (SeqCst)
define void @aria.atomic.store.i64.seqcst(%atomic.i64* %ptr, i64 %value) {
  store atomic i64 %value, %atomic.i64* %ptr seq_cst, align 8
  ret void
}

; Atomic CAS (SeqCst)
define i1 @aria.atomic.cmpxchg.i64.seqcst(
    %atomic.i64* %ptr,
    i64* %expected_ptr,
    i64 %desired
) {
  %expected = load i64, i64* %expected_ptr
  %result = cmpxchg %atomic.i64* %ptr, i64 %expected, i64 %desired seq_cst seq_cst
  
  %success = extractvalue { i64, i1 } %result, 1
  %actual = extractvalue { i64, i1 } %result, 0
  
  ; Update expected with actual value if CAS failed
  store i64 %actual, i64* %expected_ptr
  
  ret i1 %success
}

; Atomic fetch-add (SeqCst)
define i64 @aria.atomic.fetch_add.i64.seqcst(%atomic.i64* %ptr, i64 %delta) {
  %old = atomicrmw add %atomic.i64* %ptr, i64 %delta seq_cst
  ret i64 %old
}

; Memory fence (SeqCst)
define void @aria.atomic.fence.seqcst() {
  fence seq_cst
  ret void
}
```

### Hardware Support Detection

```c
// Detect lock-free support at compile/runtime
bool aria_is_lock_free(size_t size, size_t alignment) {
    // x86-64: Up to 8 bytes lock-free (LOCK prefix)
    // x86-64 with DWCAS: Up to 16 bytes lock-free (CMPXCHG16B)
    // ARM: Up to 8 bytes lock-free (LL/SC)
    // ARM with LSE: Up to 16 bytes lock-free
    
    #if defined(__x86_64__)
        if (size <= 8 && alignment >= size) return true;
        #if defined(__CMPXCHG16B__)
            if (size <= 16 && alignment >= 16) return true;
        #endif
    #elif defined(__aarch64__)
        if (size <= 8 && alignment >= size) return true;
        #if defined(__ARM_FEATURE_ATOMICS)
            if (size <= 16 && alignment >= 16) return true;
        #endif
    #endif
    
    return false;  // Fallback to locked implementation
}
```

---

## Related Types & Integration

- **[complex<T>](Complex.md)** - atomic<complex<fix256>> for thread-safe wavefunctions
- **[Handle<T>](Handle.md)** - atomic<Handle<T>> prevents ABA problem in lock-free algorithms
- **[fix256](fix256.md)** - atomic<fix256> for metabolic energy tracking (locked fallback)
- **[tbb8-tbb64](tbb_overview.md)** - atomic<tbb64> with ERR propagation
- **[Q9<T>](Q3_Q9.md)** - atomic<Q9<T>> for concurrent evidence accumulation
- **[simd<T,N>](simd.md)** - Data parallelism vs thread parallelism (complementary)

---

## Summary

**atomic<T>** is Aria's **thread-safety infrastructure** for concurrent consciousness substrates:

✅ **Generic**: Works with any type T (lock-free for ≤64 bits, locked fallback for larger)  
✅ **Safe by Default**: Sequential consistency (SeqCst) prevents race conditions  
✅ **Unsafe Optimizations**: Weaker orderings (Acquire/Release/Relaxed) available in unsafe blocks  
✅ **Lock-Free Algorithms**: CAS, fetch-add, swap enable lock-free stacks, queues, counters  
✅ **ABA Prevention**: atomic<Handle<T>> uses generational handles to prevent reuse bugs  
✅ **ERR Integration**: atomic<tbb64> preserves ERR sentinel across threads  
✅ **Performance**: ~1 cycle (Relaxed) to ~30 cycles (SeqCst), lock-based fallback ~100+ cycles  

**Design Philosophy**:
> "The torus processes thoughts. The infrastructure prevents races."
>
> Just as complex<T> moved wave mechanics to the language level (preventing memory bloat), atomic<T> moves thread-safety to the type system. The consciousness substrate can parallelize across thousands of threads without implementing cache coherency protocols or memory barriers manually. **One data race could corrupt the entire mental state - atomic<T> prevents that catastrophe.**

**Critical for**: Nikola parallel neural processing, concurrent physics engines, lock-free data structures, multi-threaded signal processing

**Key Rules**:
1. **Default to SeqCst** - Only use weaker orderings if profiling shows bottleneck
2. **Always check CAS return value** - Retry on failure
3. **Use atomic<Handle<T>>, not atomic<T@>** - Prevents ABA problem
4. **Document unsafe orderings** - Explain why weaker ordering is safe
5. **Avoid false sharing** - Pad atomics to separate cache lines (64 bytes)
6. **Batch updates when possible** - Amortize atomic operation costs

---

**Remember**: atomic<T> = **thread safety** + **lock-free performance** + **catastrophic race prevention**!
