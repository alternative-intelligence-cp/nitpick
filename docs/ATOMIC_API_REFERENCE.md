# atomic<T> API Quick Reference

**P0-2 Memory Model - Part 1: Lock-Free Synchronization**

---

## Type Signature

```aria
class atomic<*T>
```

**Supported Types**:
- int8, int16, int32, int64
- uint8, uint16, uint32, uint64
- bool
- tbb8, tbb16, tbb32, tbb64

---

## Memory Ordering

```aria
type MemoryOrder:
    relaxed,  # No sync, only atomicity
    acquire,  # Load barrier
    release,  # Store barrier
    acq_rel,  # Both (RMW)
    seq_cst   # Total order (default)
end
```

**When to Use**:
- `relaxed`: Counter, flags (single variable, no other sync)
- `acquire`: Read after another thread's release
- `release`: Write before another thread's acquire
- `acq_rel`: Read-modify-write (CAS, fetch_add)
- `seq_cst`: Default (safest, slight overhead)

---

## Core Operations

### Creation
```aria
atomic<int32>:counter = atomic<int32>::new(0);
```

### Load
```aria
int32:value = counter.load(acquire);
int32:value = counter.load();  # defaults to seq_cst
```

### Store
```aria
counter.store(42, release);
counter.store(42);  # defaults to seq_cst
```

### Exchange
```aria
int32:old = counter.exchange(42, acq_rel);
```

### Cleanup
```aria
counter.destroy();
# Auto-called at scope exit (RAII)
```

---

## Compare-and-Swap (CAS)

### Strong (no spurious failure)
```aria
int32:expected = 10;
bool:success = counter.compare_exchange_strong(
    @expected,  # May be updated!
    20,         # New value
    acq_rel,    # Success ordering
    acquire     # Failure ordering
);

if success {
    # Exchange happened: 10 → 20
} else {
    # Failed: expected now holds current value
}
```

### Weak (may spuriously fail)
```aria
# Use in loop for better performance
loop {
    int32:current = counter.load(relaxed);
    int32:new_val = compute(current);
    
    if counter.compare_exchange_weak(
        @current, new_val, release, relaxed
    ) {
        break;  # Success
    }
    # Retry on failure or spurious failure
}
```

---

## Arithmetic Operations

### Fetch-and-Add
```aria
int32:old = counter.fetch_add(1, release);
# old = previous value
# counter = old + 1
```

### Fetch-and-Subtract
```aria
int32:old = counter.fetch_sub(1, release);
# old = previous value
# counter = old - 1
```

---

## Helper Functions

### Increment
```aria
int32:old = atomic_increment(@counter);
# Equivalent to: counter.fetch_add(1, seq_cst)
```

### Decrement
```aria
int32:old = atomic_decrement(@counter);
# Equivalent to: counter.fetch_sub(1, seq_cst)
```

### Wait (busy-wait until condition)
```aria
atomic_wait(@flag, true);
# Spins until flag == true
```

---

## Common Patterns

### Thread-Safe Counter
```aria
atomic<int32>:counter = atomic<int32>::new(0);

# Thread 1, 2, 3, ... N:
int32:ticket = counter.fetch_add(1, relaxed);
# Each thread gets unique ticket number
```

### Lock-Free Increment (CAS Loop)
```aria
loop {
    int32:current = counter.load(relaxed);
    int32:new_val = current + 1;
    
    if counter.compare_exchange_weak(
        @current, new_val, release, relaxed
    ) {
        break;
    }
}
```

### Flag Synchronization
```aria
# Thread 1 (producer):
data.store(compute(), relaxed);
ready.store(true, release);  # Synchronize

# Thread 2 (consumer):
atomic_wait(@ready, true);
int32:value = data.load(acquire);  # Synchronized
```

### Shared Reference Count
```aria
atomic<int32>:refcount = atomic<int32>::new(1);

# Clone:
refcount.fetch_add(1, relaxed);

# Drop:
int32:old = refcount.fetch_sub(1, release);
if old == 1 {
    # Last reference, acquire to sync
    refcount.load(acquire);
    free_resource();
}
```

---

## Type Instantiation Examples

```aria
# Integers
atomic<int8>:tiny = atomic<int8>::new(0);
atomic<int32>:medium = atomic<int32>::new(0);
atomic<int64>:large = atomic<int64>::new(0);

# Unsigned
atomic<uint32>:flags = atomic<uint32>::new(0);

# Boolean
atomic<bool>:ready = atomic<bool>::new(false);

# TBB (with sticky error)
atomic<tbb32>:counter = atomic<tbb32>::new(0);
```

---

## Memory Ordering Cheat Sheet

| Operation | Default | Common | Relaxed OK? |
|-----------|---------|---------|-------------|
| load() | seq_cst | acquire | Counter only |
| store() | seq_cst | release | Counter only |
| exchange() | seq_cst | acq_rel | Rarely |
| CAS strong | seq_cst | acq_rel | No |
| CAS weak | seq_cst | release/relaxed | Loop only |
| fetch_add | seq_cst | relaxed | Counter only |
| fetch_sub | seq_cst | release | Refcount |

---

## Platform Notes

**x86-64 (TSO)**:
- acquire/release often "free" (hardware provides TSO)
- seq_cst requires MFENCE

**ARMv8 (Weak)**:
- acquire → LDAR instruction
- release → STLR instruction
- More expensive than x86

**Lock-Free Guarantee**:
- All primitive types are lock-free
- CAS may spuriously fail on ARM (use weak in loop)

---

## Safety Rules

✅ **DO**:
- Use acquire for loads after another thread's release
- Use release for stores before another thread's acquire
- Use acq_rel for read-modify-write
- Use CAS weak in loops (better performance)
- Default to seq_cst when unsure

❌ **DON'T**:
- Mix atomic and non-atomic access (undefined!)
- Use relaxed for synchronization (data race!)
- Forget to destroy (memory leak!)
- Use strong CAS in tight loop (spurious retry wastes cycles)

---

## Error Handling

**TBB Types**:
```aria
atomic<tbb32>:counter = atomic<tbb32>::new(0);
tbb32:value = counter.load(acquire);

if value == ERR {
    # Sticky error propagated
    handle_error();
}
```

**Invalid Types** (compile error):
```aria
atomic<string>:s;  # ERROR: string not atomic-compatible
atomic<SomeStruct>:x;  # ERROR: use atomic<@SomeStruct>
```

---

## Performance Tips

1. **Use relaxed for counters**: No sync needed, just atomicity
2. **Use weak CAS in loops**: Fewer spurious retries on ARM
3. **Batch operations**: Minimize atomic ops per transaction
4. **Profile ordering**: seq_cst has overhead on ARM

---

## Compile-Time vs Runtime

**Compile-Time**:
- Type checking (atomic<T> only for lock-free types)
- Memory ordering validation
- Monomorphization to runtime calls

**Runtime**:
- Hardware atomic instructions (LOCK CMPXCHG, LDAR, STLR)
- Memory barriers (MFENCE, DMB, DSB)
- Lock-free algorithms (no OS scheduler involvement)

---

## Migration from C/C++

**C11 `_Atomic`**:
```c
_Atomic int counter;
atomic_store(&counter, 42);
int value = atomic_load(&counter);
```

**Aria `atomic<T>`**:
```aria
atomic<int32>:counter = atomic<int32>::new(0);
counter.store(42, seq_cst);
int32:value = counter.load(seq_cst);
```

**Advantages**:
- Type safety (generic T checked at compile time)
- RAII cleanup (no manual destroy)
- Explicit ordering (no hidden seq_cst cost)

---

## Future Extensions

**Phase 2: Handle<T>** (Capability ownership)
**Phase 3: SharedHandle<T>** (Atomic refcount)
**Phase 4: atomic<@T>** (Atomic pointers with GC)
**Phase 5: Fence operations** (Standalone barriers)
**Phase 6: Wait/notify** (C++20 atomic_wait)

---

**See Also**:
- `docs/P0-2_MEMORY_MODEL_DESIGN.md` - Complete design
- `docs/ATOMIC_IMPLEMENTATION_PLAN.md` - Implementation roadmap
- `tests/feature_validation/atomic_test.aria` - Test examples
- `include/runtime/atomic.h` - Runtime library

**Version**: Phase 1 (Language Interface)  
**Status**: Ready for compiler integration  
**Date**: February 7, 2026
