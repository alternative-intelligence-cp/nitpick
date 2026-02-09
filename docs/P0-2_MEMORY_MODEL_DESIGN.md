# P0-2: Memory Model Implementation Design

**Priority**: P0 (Safety Critical - Prevents "Confusion" failure mode)  
**Status**: Design Phase  
**Date**: 2026-02-07

---

## Overview

Implement language-level memory safety constructs for concurrent programming:
1. **atomic<T>** - Generic atomic types with explicit memory ordering
2. **Handle<T>** - Capability-based ownership preventing use-after-free
3. **Memory Ordering** - Explicit sequencing requirements
4. **Integration** - Work with hybrid GC/stack allocation model

---

## Design Goals

### 1. Zero-Cost Abstractions
- Compile to efficient runtime calls (already implemented in runtime/atomic.h)
- No overhead beyond what C++11 std::atomic provides
- Generic specialization via monomorphization

### 2. Safe by Default
- Prevent data races at compile time
- Explicit ordering prevents unintended optimizations
- Capability system prevents dangling pointers

### 3. Ergonomic
- Natural syntax for concurrent operations
- Type inference where possible
- Clear error messages for violations

---

## Part 1: atomic<T> Generic Type

### Language Syntax

```aria
// Basic declaration
atomic<int32>:counter;

// With initialization
atomic<int32>:counter = atomic<int32>::new(0);

// Load with explicit ordering
int32:value = counter.load(acquire);

// Store with explicit ordering
counter.store(42, release);

// Compare-and-swap
int32:expected = 10;
bool:success = counter.compare_exchange_strong(
    @expected,  // Reference to expected (may be updated)
    20,         // Desired value
    acq_rel,    // Success ordering
    acquire     // Failure ordering
);

// Fetch-and-add
int32:old_value = counter.fetch_add(1, release);
```

### Type Constraints

**Supported Types** (T must be):
- Arithmetic: `int8`, `int16`, `int32`, `int64`, `uint8`, `uint16`, `uint32`, `uint64`
- Boolean: `bool`
- Pointers: `@T` (fat pointers)
- TBB types: `tbb8`, `tbb16`, `tbb32`, `tbb64` (special handling for ERR)

**NOT Supported**:
- `string` (not lock-free)
- `struct` (use atomic<@T> with indirection)
- Arrays (use atomic pointers to array)

### Memory Ordering Constants

```aria
// Enumeration built into language
type MemoryOrder:
    relaxed,  // No synchronization, only atomicity
    acquire,  // Load barrier
    release,  // Store barrier
    acq_rel,  // Both (for RMW)
    seq_cst   // Sequential consistency (strongest, default)
end

// Usage
value: int32 = counter.load(acquire);  // Explicit
value: int32 = counter.load();        // Defaults to seq_cst
```

### Implementation Strategy

#### Compiler Frontend (Sema)
1. **Type Checking**: `atomic<T>` only for allowed types
2. **Method Resolution**: Map to runtime functions
3. **Ordering Validation**: Check ordering constraints

#### Monomorphization
```cpp
// Template generates specialized calls:
atomic<int32>:x;
x.store(42, release);  

// Becomes:
AriaAtomicInt32* _x = aria_atomic_int32_create(0);
aria_atomic_int32_store(_x, 42, ARIA_MEMORY_ORDER_RELEASE);
```

#### Runtime Mapping
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
atomic<@T>      → AriaAtomicPtr
atomic<tbb8>    → AriaAtomicTBB8
atomic<tbb16>   → AriaAtomicTBB16
atomic<tbb32>   → AriaAtomicTBB32
atomic<tbb64>   → AriaAtomicTBB64
```

---

## Part 2: Handle<T> Capability System

### The Problem: "Confusion"

**Use-After-Free Example**:
```aria
// ❌ DANGER: Pointer outlives object
func dangling_pointer() = @int32 {
    int32:x = 42;
    return @x;  // Returns pointer to stack variable!
}
// Caller gets dangling pointer - UNDEFINED BEHAVIOR
```

**Data Race Example**:
```aria
// ❌ DANGER: Shared mutable state
int32:shared = 0;

thread1: {
    shared = shared + 1;  // Read-modify-write
}

thread2: {
    shared = shared + 1;  // RACE CONDITION!
}
```

### The Solution: Capabilities

**Handle<T>** = Capability-based smart pointer
- Tracks object lifetime
- Prevents dangling references
- Enforces synchronization for shared access

### Language Syntax

```aria
// Heap allocation with capability
Handle<int32>:h = Handle<int32>::new(42);

// Access value (checked)
int32:value = h.get();

// Transfer ownership (move semantics)
func take_ownership(h: Handle<int32>) = int32 {
    return h.get();
}

Handle<int32>:h1 = Handle<int32>::new(42);
int32:result = take_ownership(pass h1);
// h1 is now invalid (moved)

// Shared ownership (atomic refcount)
SharedHandle<int32>:sh = SharedHandle<int32>::new(42);
SharedHandle<int32>:sh2 = sh.clone();  // Increment refcount
// Both valid, last one frees memory
```

### Safety Rules

#### 1. Lifetime Tracking
```aria
func safe_pointer() = Handle<int32> {
    Handle<int32>:h = Handle<int32>::new(42);
    return pass h;  // OK: ownership transferred
}

Handle<int32>:ptr = safe_pointer();
int32:value = ptr.get();  // ✅ Valid
```

#### 2. Move Semantics
```aria
Handle<int32>:h1 = Handle<int32>::new(42);
Handle<int32>:h2 = pass h1;  // h1 moved to h2

int32:x = h1.get();  // ❌ COMPILE ERROR: h1 invalidated
int32:y = h2.get();  // ✅ OK
```

#### 3. Borrowing (Temporary References)
```aria
func borrow_value(h: @Handle<int32>) = int32 {
    return h.get();  // ✅ Temporary borrow
}

Handle<int32>:h = Handle<int32>::new(42);
int32:value = borrow_value(@h);  // ✅ h still valid after call
```

#### 4. Thread Safety
```aria
// ❌ Can't share Handle across threads
Handle<int32>:h = Handle<int32>::new(42);
spawn_thread( {
    h.get();  // COMPILE ERROR: Handle not Send
});

// ✅ Use SharedHandle for shared ownership
SharedHandle<int32>:sh = SharedHandle<int32>::new(42);
spawn_thread( {
    sh.get();  // OK: atomic refcount
});

// ✅ Or use atomic<T> for shared mutable state
atomic<int32>:shared = atomic<int32>::new(0);
spawn_thread( {
    shared.fetch_add(1, release);  // OK: synchronized
});
```

### Implementation Strategy

#### Compiler Tracking
```cpp
struct HandleInfo {
    std::string varName;
    bool isValid;      // Has it been moved?
    bool isShared;     // SharedHandle vs Handle?
    TypeNode* innerType;
};

// In sema, track handle validity
std::unordered_map<std::string, HandleInfo> handleTracker;

// On move:
handleTracker[oldVar].isValid = false;

// On access:
if (!handleTracker[var].isValid) {
    error("Use of moved Handle");
}
```

#### Runtime Representation
```cpp
// Handle<T> = fat pointer + metadata
struct AriaHandle {
    void* data;           // Actual T* pointer
    uint64_t id;          // Unique allocation ID
    uint32_t generation;  // Detect ABA problem
    bool is_valid;        // Runtime check
};

// SharedHandle<T> = Handle + atomic refcount
struct AriaSharedHandle {
    AriaHandle handle;
    std::atomic<uint32_t>* refcount;
};
```

---

## Part 3: Hybrid Memory Model Integration

### Stack vs Heap

```aria
// Stack: Normal variables (scoped lifetime)
int32:x = 42;  // Automatically freed at scope end

// Heap: Handles required
Handle<int32>:h = Handle<int32>::new(42);  // Manual lifetime

// GC:  Managed references (future feature)
Gc<Tree>:tree = Gc<Tree>::new(...);  // Garbage collected
```

### Ownership Transfer

```aria
func stack_to_heap(x: int32) = Handle<int32> {
    // Copy value to heap
    return Handle<int32>::new(x);
}

func heap_to_stack(h: Handle<int32>) = int32 {
    // Extract value, free handle
    int32:value = h.get();
    h.drop();  // Explicit free
    return value;
}
```

### Preventing Leaks

```aria
// ❌ LEAK: Handle not dropped
func leak() {
    Handle<int32>:h = Handle<int32>::new(42);
    // Function ends without dropping h
}

// ✅ Compiler warning: "Handle 'h' never dropped"
// ✅ Add drop() or return it
```

---

## Implementation Phases

### Phase 1: atomic<T> (This Phase)
- [ ] Define `atomic<T>` generic struct in compiler
- [ ] Implement type constraints (only allowed types)
- [ ] Map methods to runtime calls
- [ ] Add `MemoryOrder` enum to language
- [ ] Write atomic operations tests

### Phase 2: Handle<T> Basics
- [ ] Define `Handle<T>` struct
- [ ] Implement move semantics tracking
- [ ] Runtime handle allocation/deallocation
- [ ] Lifetime validation in sema

### Phase 3: SharedHandle<T>
- [ ] Atomic refcounting implementation
- [ ] Thread-safety rules
- [ ] Integration with atomic<T>

### Phase 4: Integration Testing
- [ ] Concurrent data structures (lock-free queue)
- [ ] Thread pools with atomics
- [ ] Stress tests for race detection

---

## Example: Lock-Free Counter

```aria
// Shared counter across threads
struct Counter {
    atomic<int64>:value;
}

impl Counter {
    func<T>:new = Counter {
        return Counter {
            value: atomic<int64>::new(0)
        };
    }
    
    func increment() {
        self.value.fetch_add(1, release);
    }
    
    func get() = int64 {
        return self.value.load(acquire);
    }
}

// Usage across threads
SharedHandle<Counter>:counter = SharedHandle<Counter>::new(Counter::new());

spawn_thread( {
    counter.get().increment();  // Thread-safe
});

spawn_thread( {
    counter.get().increment();  // Thread-safe
});

join_all_threads();
int64:total = counter.get().get();  // Read final value
```

---

## Safety Properties Guaranteed

1. **No Data Races**: atomic<T> enforces synchronization
2. **No Use-After-Free**: Handle<T> tracks lifetime
3. **No Double Free**: Handle prevents multiple drops
4. **No Dangling Pointers**: Borrowing rules prevent outliving
5. **Explicit Ordering**: No hidden reorderings surprise developers

---

## Next Steps

1. Start with `atomic<T>` implementation
2. Test with simple counter examples
3. Add `Handle<T>` once atomics work
4. Build concurrent data structures
5. Document patterns for common use cases

---

**Status**: Ready to implement atomic<T> Phase 1
