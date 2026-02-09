# Atomic<T> Implementation Plan

**Status**: Phase 1.1 - Language Interface Complete  
**Date**: 2026-02-07

---

## Overview

Implementation of atomic<T> generic type for lock-free concurrent programming.  
Runtime library already exists (runtime/atomic.h), need language integration.

---

## Phase 1: Language Interface ✅ COMPLETE

### Completed (2026-02-07):

1. **stdlib/atomic.aria** - Language interface
   - Generic atomic<T> class definition
   - MemoryOrder enumeration (relaxed, acquire, release, acq_rel, seq_cst)
   - Methods: new, load, store, exchange, compare_exchange_strong/weak, fetch_add, fetch_sub
   - Helper functions: atomic_increment, atomic_decrement, atomic_wait
   - Comprehensive documentation

2. **Test suite** - Feature validation
   - tests/feature_validation/atomic_test.aria - Full test suite (9 tests)
   - tests/feature_validation/atomic_minimal.aria - Minimal instantiation test
   - Tests: basic ops, exchange, CAS, fetch_add/sub, bool, multiple types, lock-free counter

### Next Steps:

See Phase 2 below.

---

## Phase 2: Compiler Integration (NEXT)

### 2.1: Grammar Changes
**Files**: `src/frontend/parser/grammar.y`, `src/frontend/lexer/lexer.l`

Add MemoryOrder to language:
```aria
type MemoryOrder:
    relaxed,
    acquire,
    release,
    acq_rel,
    seq_cst
end
```

Tasks:
- [ ] Add MemoryOrder enum to type system
- [ ] Support default parameter values (e.g., `order: MemoryOrder = seq_cst`)
- [ ] Lex/parse ordering keywords

Estimated time: 2 hours

### 2.2: Type Checker Constraints
**File**: `src/frontend/sema/type_checker.cpp`

Enforce atomic<T> type constraints:
- Only allow: int8, int16, int32, int64, uint8, uint16, uint32, uint64, bool, tbb8-64
- Reject: string, struct (unless @struct), arrays
- Compile-time error for unsupported types

Tasks:
- [ ] Add `isAtomicCompatible(Type*)` predicate
- [ ] Check generic instantiation: `atomic<SomeType>` → validate Type
- [ ] Enforce ordering constraints (e.g., relaxed not valid for load+store combo)

Estimated time: 3 hours

### 2.3: Monomorphization
**File**: `src/frontend/sema/generic_resolver.cpp`

Map atomic<T> methods to runtime calls:

```cpp
// Before monomorphization (Aria):
atomic<int32>:counter = atomic<int32>::new(0);
counter.store(42, release);
int32:value = counter.load(acquire);

// After monomorphization (C):
AriaAtomicInt32* counter = aria_atomic_int32_create(0);
aria_atomic_int32_store(counter, 42, ARIA_MEMORY_ORDER_RELEASE);
int32_t value = aria_atomic_int32_load(counter, ARIA_MEMORY_ORDER_ACQUIRE);
```

Type mapping:
```
atomic<int8>    → AriaAtomicInt8
atomic<int16>   → AriaAtomicInt16
atomic<int32>   → AriaAtomicInt32
atomic<int64>   → AriaAtomicInt64
atomic<uint8>   → AriaAtomicUint8
atomic<uint16>  → AriaAtomicUint16
atomic<uint32>  → AriaAtomicUint32
atomic<uint64>  → AriaAtomicUint64
atomic<bool>    → AriaAtomicBool
atomic<tbb8>    → AriaAtomicTBB8
atomic<tbb16>   → AriaAtomicTBB16
atomic<tbb32>   → AriaAtomicTBB32
atomic<tbb64>   → AriaAtomicTBB64
```

MemoryOrder mapping:
```
relaxed  → ARIA_MEMORY_ORDER_RELAXED
acquire  → ARIA_MEMORY_ORDER_ACQUIRE
release  → ARIA_MEMORY_ORDER_RELEASE
acq_rel  → ARIA_MEMORY_ORDER_ACQ_REL
seq_cst  → ARIA_MEMORY_ORDER_SEQ_CST
```

Tasks:
- [ ] Detect atomic<T> instantiation, map to runtime type
- [ ] Replace method calls with runtime function calls
- [ ] Handle default parameters (seq_cst if not specified)
- [ ] Generate #include "runtime/atomic.h" in CodeGen

Estimated time: 6 hours

### 2.4: Lifetime Management
**File**: `src/frontend/sema/scope.cpp`

Automatic cleanup for atomic variables:
- When `atomic<T>` goes out of scope → call `destroy()`
- Similar to RAII destructor pattern

Tasks:
- [ ] Track atomic variables in scope
- [ ] Insert destroy() calls at scope exit
- [ ] Handle early returns (destroy before return)

Estimated time: 2 hours

---

## Phase 3: Testing & Validation

### 3.1: Unit Tests
- [ ] Compile atomic_test.aria → verify C output
- [ ] Run atomic_test.aria → verify all 9 tests pass
- [ ] Test invalid types (should fail compilation)
- [ ] Test memory ordering violations (should warn/error)

### 3.2: Integration Tests
- [ ] Add to test suite batch runner
- [ ] Performance benchmarks (compare to raw C atomics)
- [ ] Multi-threaded stress test (spawn threads, race counters)

### 3.3: Documentation
- [ ] Update aria_specs.txt with atomic<T> section
- [ ] Add examples to programming_guide/concurrency.md
- [ ] Document memory model guarantees

Estimated time: 4 hours

---

## Phase 4: TBB-Specific Handling (Optional)

TBB types (tbb8, tbb16, tbb32, tbb64) have sticky error propagation.
Runtime already handles this via CAS loops.

Tasks:
- [ ] Verify TBB atomic tests compile
- [ ] Test ERR sentinel behavior in atomic context
- [ ] Document TBB atomic semantics

Estimated time: 2 hours

---

## Implementation Order

1. **Phase 2.1** - Grammar (MemoryOrder enum) - 2 hours
2. **Phase 2.2** - Type constraints - 3 hours  
3. **Phase 2.3** - Monomorphization - 6 hours  
4. **Phase 2.4** - Lifetime management - 2 hours  
5. **Phase 3.1** - Unit tests - 2 hours  
6. **Phase 3.2** - Integration - 2 hours  
7. **Phase 3.3** - Documentation - 1 hour

**Total estimated time**: ~18 hours of focused work

---

## Success Criteria

✅ **Phase 1**: Language interface defined (COMPLETE)
- [x] stdlib/atomic.aria created with full API
- [x] Test suite written

⏳ **Phase 2**: Compiler integration (NEXT)
- [ ] atomic_test.aria compiles to C
- [ ] Generated C calls runtime/atomic.h functions
- [ ] Type constraints enforced at compile time

⏳ **Phase 3**: Validation
- [ ] All 9 tests pass at runtime
- [ ] Invalid types rejected
- [ ] Performance matches C atomics

---

## Dependencies

**None** - Runtime library already complete:
- runtime/atomic.h - Full API (256 lines)
- runtime/atomic.cpp - Implementation with C11/C++11 atomics
- All types supported (int8-64, uint8-64, bool, tbb8-64, pointers)

---

## Future Work (Post-Phase 3)

1. **SharedAtomic<T>** - Atomic reference-counted smart pointers
2. **atomic<@T>** - Atomic fat pointers for GC integration
3. **Fence operations** - Standalone memory barriers (atomic_thread_fence)
4. **Wait/notify** - Efficient blocking (C++20 atomic_wait/atomic_notify)
5. **Hazard pointers** - Lock-free memory reclamation for concurrent data structures

---

## Notes

- Runtime library uses C11/C++11 atomics (portable)
- x86-64: Many orderings are free (TSO model)
- ARMv8: Uses LDAR/STLR instructions for acquire/release
- TBB types use CAS loops (no hardware sticky error support)
- Generic type instantiation already works (verified with complex<T>)

---

## References

- Design doc: `/home/randy/Workspace/REPOS/aria/docs/P0-2_MEMORY_MODEL_DESIGN.md`
- Runtime lib: `/home/randy/Workspace/REPOS/aria/include/runtime/atomic.h`
- Test suite: `/home/randy/Workspace/REPOS/aria/tests/feature_validation/atomic_test.aria`
- Complex<T> precedent: `/home/randy/Workspace/REPOS/aria/stdlib/complex.aria`
