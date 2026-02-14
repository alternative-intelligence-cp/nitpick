# atomic<T> - Thread-Safe Generic Type

**Category**: Types → Concurrency  
**Purpose**: Lock-free atomic operations for multi-threaded safety-critical systems  
**Status**: ⚠️ PARTIAL IMPLEMENTATION (Basic ops complete, advanced orderings in progress)

---

## Overview

**atomic<T>** provides **thread-safe operations** on type T with configurable memory ordering. Essential for concurrent physics engines, parallel neurogenesis, and lock-free data structures.

---

## Key Characteristics

| Property | Value |
|----------|-------|
| **Generic** | Works with any type T (lock-free if ≤64 bits, locked otherwise) |
| **Default Ordering** | Sequential Consistency (SeqCst) - safest |
| **Memory Model** | Follows C++20 atomic semantics |
| **ERR Propagation** | ERR sentinel remains atomic |
| **Common Uses** | Counters, flags, shared state in parallel systems |

---

## Declaration

```aria
// Basic atomic types
atomic<int64>:counter = atomic_new(0);
atomic<tbb64>:error_code = atomic_new(0t64);
atomic<fix256>:energy_total = atomic_new(fix256(0));

// Atomic complex number (lock-based fallback)
atomic<complex<fix256>>:wavefunction = atomic_new({real: fix256(0), imag: fix256(0)});
```

---

## Memory Ordering Levels

### Sequential Consistency (SeqCst) - DEFAULT

```aria
// Strongest guarantee: all operations appear in single total order
// Matches programmer intuition - no surprises
// Performance cost: Memory barriers on most platforms

atomic<int64>:x = atomic_new(0);
atomic<int64>:y = atomic_new(0);

// Thread 1
x.store(1);          // SeqCst by default
y.store(2);

// Thread 2
if (y.load() == 2) {
    // x.load() GUARANTEED to be 1
    // Sequential consistency ensures ordering
}
```

### Acquire/Release (AcqRel) - UNSAFE

```aria
// Weaker but faster - one-way memory barriers
// MUST be marked unsafe - requires safety justification

unsafe {
    atomic<uint64>:seq_counter = atomic_new(0);
    
    // Acquire: Prevent later ops from moving before this load
    uint64:seq1 = seq_counter.load_acquire();
    fix256:data = tensor.read();  // Cannot move before seq1
    uint64:seq2 = seq_counter.load_acquire();
    
    // Release: Prevent earlier ops from moving after this store
    tensor.write(new_data);
    seq_counter.store_release(seq1 + 1);
}
```

### Relaxed - UNSAFE (EXPERT ONLY)

```aria
// No ordering guarantees except atomicity
// Fastest but dangerous - order doesn't matter

unsafe {
    atomic<int64>:stat_counter = atomic_new(0);
    
    // Statistics where order irrelevant
    stat_counter.fetch_add_relaxed(1);
}
```

---

## Atomic Operations

### Load (Read)

```aria
atomic<int64>:value = atomic_new(42);

// SeqCst load (default - safest)
int64:current = value.load();

// Acquire load (unsafe - weaker ordering)
unsafe {
    int64:current = value.load_acquire();
}

// Relaxed load (unsafe - no ordering)
unsafe {
    int64:current = value.load_relaxed();
}
```

### Store (Write)

```aria
atomic<int64>:value = atomic_new(0);

// SeqCst store (default)
value.store(100);

// Release store (unsafe)
unsafe {
    value.store_release(100);
}

// Relaxed store (unsafe)
unsafe {
    value.store_relaxed(100);
}
```

### Swap (Exchange)

```aria
atomic<int64>:value = atomic_new(10);

// Atomic exchange - returns old value
int64:old = value.swap(20);  // old == 10, value now 20

// AcqRel swap (unsafe)
unsafe {
    int64:old = value.swap_acqrel(30);
}
```

### Compare-Exchange (CAS - Compare-And-Swap)

```aria
atomic<int64>:counter = atomic_new(5);

// Expected value and desired value
int64:expected = 5;
int64:desired = 10;

// If counter == expected, set to desired and return true
// If counter != expected, set expected = counter and return false
bool:success = counter.compare_exchange(expected, desired);

if (success) {
    // counter now 10
} else {
    // counter wasn't 5, expected now contains actual value
}
```

#### Weak CAS (Unsafe - Spurious Failures Allowed)

```aria
unsafe {
    int64:expected = 5;
    int64:desired = 10;
    
    // May fail spuriously - use in retry loop
    while (!counter.compare_exchange_weak_acqrel(expected, desired)) {
        // Retry until success
        expected = 5;  // Reset expected value
    }
}
```

---

## Fetch-And-Modify Operations

### Arithmetic

```aria
atomic<int64>:counter = atomic_new(0);

// All return OLD value before modification
int64:old1 = counter.fetch_add(5);      // old1==0, counter now 5
int64:old2 = counter.fetch_sub(2);      // old2==5, counter now 3
int64:old3 = counter.fetch_and(0xFF);   // Bitwise AND
int64:old4 = counter.fetch_or(0x10);    // Bitwise OR
int64:old5 = counter.fetch_xor(0x01);   // Bitwise XOR

// Relaxed variants (unsafe)
unsafe {
    int64:old = counter.fetch_add_relaxed(1);
}
```

---

## ERR Propagation with Atomics

```aria
// atomic<tbb64> can hold ERR sentinel
atomic<tbb64>:status = atomic_new(0t64);

// ERR remains atomic
status.store(ERR);

// Load preserves ERR
tbb64:current = status.load();  // current == ERR

// Operations on ERR propagate
tbb64:result = current + 10t64;  // result == ERR (sticky!)
```

---

## Safety-Critical Use Case: Nikola Physics Engine

### Concurrent Metabolic Energy Tracking

```aria
// Multiple threads update total energy in parallel
atomic<fix256>:total_energy = atomic_new(fix256(0));

// Thread-safe energy accumulation
func:update_metabolism = void(fix256:delta_energy) {
    // Atomic read-modify-write
    fix256:old_energy = total_energy.load();
    fix256:new_energy = old_energy + delta_energy;
    
    // CAS loop for safe update
    while (!total_energy.compare_exchange(old_energy, new_energy)) {
        old_energy = total_energy.load();
        new_energy = old_energy + delta_energy;
    }
};

// Called from multiple physics threads simultaneously
// No data races - atomics ensure safety
```

### Lock-Free Queue (Advanced Pattern)

```aria
struct<T>:Node {
    *T:value;
    atomic<Handle<Node<*T>>>:next;  // Atomic handle, not pointer!
};

struct<T>:LockFreeQueue {
    atomic<Handle<Node<*T>>>:head;
    atomic<Handle<Node<*T>>>:tail;
    arena<Node<*T>>:node_arena;
};

func<T>:enqueue = Result<NIL>(LockFreeQueue<*T>@:queue, *T:value) {
    Handle<Node<*T>>:new_node = queue->node_arena.alloc({
        value: value,
        next: atomic_new(Handle<Node<*T>>{index: 0, generation: 0})
    });
    
    // CAS-based enqueue (lock-free)
    till MAX_RETRIES loop
        Handle<Node<*T>>:tail = queue->tail.load();
        Node<*T>:tail_node = queue->node_arena.get(tail) ?! default;
        
        Handle<Node<*T>>:next = tail_node.next.load();
        if (next.index == 0) {
            // Tail is actually last node
            if (tail_node.next.compare_exchange(next, new_node)) {
                queue->tail.compare_exchange(tail, new_node);
                pass(NIL);
            }
        } else {
            // Help other thread advance tail
            queue->tail.compare_exchange(tail, next);
        }
    end
    
    fail("Enqueue failed - too many retries");
};
```

---

## Memory Fences (Explicit Synchronization)

```aria
// Manual memory barriers for custom synchronization patterns

// Acquire fence - prevent loads/stores from moving before this point
atomic_fence_acquire();

// Release fence - prevent loads/stores from moving after this point
atomic_fence_release();

// SeqCst fence - full bidirectional barrier
atomic_fence_seqcst();

// Used with Relaxed operations to build custom sync
unsafe {
    atomic<int64>:data = atomic_new(0);
    atomic<bool>:ready = atomic_new(false);
    
    // Writer thread
    data.store_relaxed(42);
    atomic_fence_release();  // Ensure data write completes first
    ready.store_relaxed(true);
    
    // Reader thread
    while (!ready.load_relaxed()) { }
    atomic_fence_acquire();  // Ensure ready read completes first
    int64:value = data.load_relaxed();  // Guaranteed to see 42
}
```

---

## Lock-Free vs Lock-Based

### When Lock-Free

```aria
// Small types (≤64 bits) use hardware atomic instructions
atomic<int64>:counter;     // Lock-free (single instruction)
atomic<tbb32>:status;      // Lock-free
atomic<bool>:flag;         // Lock-free
```

### When Locked (Fallback)

```aria
// Large types use spinlock fallback
atomic<fix256>:big_value;              // Locked (256 bits)
atomic<complex<fix256>>:wavefunction;  // Locked (512 bits)
atomic<int1024>:rsa_key;               // Locked (1024 bits)

// Still thread-safe, just not lock-free
// Performance cost: spinlock acquisition
```

---

## Safety Guarantees

### Race Condition Prevention

```aria
// WITHOUT atomics - DATA RACE (undefined behavior)
int64:shared_counter = 0;

// Thread 1                  // Thread 2
shared_counter++;            shared_counter++;
// Final value unpredictable! Could be 1 or 2

// WITH atomics - SAFE
atomic<int64>:safe_counter = atomic_new(0);

// Thread 1                        // Thread 2
safe_counter.fetch_add(1);        safe_counter.fetch_add(1);
// Final value GUARANTEED to be 2
```

---

## Performance Characteristics

| Operation | SeqCst | AcqRel | Relaxed |
|-----------|--------|--------|---------|
| Load | ~10-30 cycles | ~3-10 cycles | 1 cycle |
| Store | ~10-30 cycles | ~3-10 cycles | 1 cycle |
| CAS | ~20-50 cycles | ~10-20 cycles | ~5-10 cycles |
| Fence | ~20-40 cycles | ~10-20 cycles | 0 cycles |

*Approximate - architecture dependent*

---

## Implementation Status

### ✅ Complete
- [x] Generic atomic<T> type
- [x] Load/store operations
- [x] Swap (exchange)
- [x] SeqCst (sequential consistency) default
- [x] ERR sentinel atomicity

### ⚠️ In Progress
- [ ] Unsafe orderings (Acquire/Release/Relaxed) need verification
- [ ] Compare-exchange (CAS) operations
- [ ] Fetch-and-modify (fetch_add, fetch_sub, etc.)
- [ ] Memory fences
- [ ] Lock-free size detection

---

## Related

- [fix256](fix256.md) - Often used with atomic for concurrent physics
- [complex<T>](complex.md) - atomic<complex<fix256>> for wavefunctions
- [simd<T,N>](simd.md) - Parallelism (data-level vs thread-level)
- [Handle<T>](handles.md) - Generational handles work with atomics

---

**Remember**: atomic<T> = **thread safety** + **lock-free performance** + **memory ordering**!

**Critical for**: Nikola AGI parallel physics, concurrent systems, lock-free data structures

**Default to SeqCst - only use weaker orderings in unsafe blocks with clear safety justification!**
