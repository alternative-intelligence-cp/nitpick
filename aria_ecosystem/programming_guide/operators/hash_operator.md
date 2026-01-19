# The `#` Operator (Memory Pinning)

**Category**: Operators → Memory Management  
**Syntax**: `#variable` (pin in memory)  
**Purpose**: Prevent garbage collector from moving objects

---

## Overview

The `#` operator **pins** an object in memory, preventing the garbage collector from relocating it. This is critical when:

1. **Passing pointers to C/FFI functions** (they expect stable addresses)
2. **Implementing lock-free data structures** (address stability required)
3. **Memory-mapped I/O** (hardware expects fixed addresses)
4. **Performance-critical code** (avoid GC relocation overhead)

**Philosophy**: Explicit control over memory layout when needed, automatic otherwise.

---

## Basic Syntax

```aria
// Pin object in memory
gc SomeType:obj;
#obj;  // Object won't be moved by GC

// Get stable address
SomeType@:ptr = @obj;  // Address guaranteed stable
```

---

## Why Pinning is Needed

### Problem: GC Relocation

Garbage collectors may **move objects** to compact memory:

```aria
gc int64[1000]:array;
int64@:ptr = @array[0];  // Get pointer

// ⚠️ GC runs, might move array to new location
// ptr now points to old (invalid) location!

doSomething(ptr);  // ⚠️ DANGER: Use-after-move
```

### Solution: Pin Before Taking Address

```aria
gc int64[1000]:array;
#array;  // Pin in memory

int64@:ptr = @array[0];  // Safe: address won't change

// GC can run, but array stays at same address
doSomething(ptr);  // ✅ Safe: ptr still valid
```

---

## Use Case 1: C FFI with Pointers

### Passing GC Objects to C Functions

```aria
extern "libc" {
    func:qsort = void(void*:base, uint64:num, uint64:size, 
                      func:(int64)(void*, void*)@:compare);
}

gc int64[100]:array;
#array;  // MUST pin before passing to C

qsort(@array as void*, 100, sizeof(int64), @compareFunc);
// Array address stays stable during C function call
```

**Critical**: C functions expect **stable addresses** - they don't understand Aria's GC.

---

## Use Case 2: Memory-Mapped I/O

### Hardware Registers

```aria
// Memory-mapped hardware register
gc uint64@:register = 0x40000000 as uint64@;
#register;  // MUST pin: hardware expects fixed address

// Safe to write to hardware
*register = 0x1234;
```

### DMA Buffers

```aria
// DMA (Direct Memory Access) buffer
gc byte[4096]:dmaBuffer;
#dmaBuffer;  // Pin: DMA controller needs stable address

// Tell hardware where buffer is
hardware.setDmaAddress(@dmaBuffer);
hardware.startDma();
```

---

## Use Case 3: Lock-Free Data Structures

### Atomic Operations Require Stable Addresses

```aria
gc int64:counter = 0;
#counter;  // Pin: atomic ops need stable address

// Safe to use atomic operations
atomicIncrement(@counter);
```

### Lock-Free Queue

```aria
%STRUCT Node {
    int64:value,
    Node@:next
}

gc Node:head;
#head;  // Pin for lock-free operations

// CAS (Compare-And-Swap) needs stable address
compareAndSwap(@head.next, oldNext, newNext);
```

---

## Pinning Scope

### Pin Duration

Objects remain pinned until **explicitly unpinned** or go out of scope:

```aria
{
    gc int64[100]:array;
    #array;  // Pinned
    
    // ... use array ...
    
}  // array goes out of scope, automatically unpinned
```

### Manual Unpin (Future)

**Status**: Unpin syntax not yet specified.

Possible future syntax:

```aria
gc int64[]:array;
#array;  // Pin

// ... use array ...

~array;  // Unpin (tentative syntax)
```

---

## Performance Implications

### Benefits of Pinning

1. **Stable addresses**: Pointers remain valid across GC cycles
2. **No relocation overhead**: GC doesn't spend time moving pinned objects
3. **FFI safety**: C functions receive valid pointers

### Costs of Pinning

1. **Memory fragmentation**: Pinned objects create "holes" in heap
2. **GC efficiency**: Can't compact pinned objects, reducing GC effectiveness
3. **Memory overhead**: GC needs to track pinned objects separately

**Best Practice**: Pin only when necessary, unpin when done.

---

## Common Patterns

### Pattern 1: Pin for FFI Call

```aria
func:callCFunction = void(gc int64[]:data) {
    #data;  // Pin before FFI
    defer ~data;  // Unpin after (tentative)
    
    extern_c_function(@data);
}
```

### Pattern 2: Pin Long-Lived Objects

```aria
// Global state that C libraries reference
gc GameState:globalState;
#globalState;  // Pin for entire program lifetime

extern "game_engine" {
    func:init = void(GameState*:state);
}

init(@globalState as GameState*);
```

### Pattern 3: Temporary Pin

```aria
{
    gc byte[4096]:buffer;
    #buffer;  // Pin temporarily
    
    // Pass to C function
    readFile(fd, @buffer, 4096);
    
    // Process data
    processBuffer(buffer);
    
}  // buffer unpinned when scope ends
```

---

## Interaction with Memory Strategies

### `gc` Objects (Garbage Collected)

Pinning is **most relevant** for `gc` objects:

```aria
gc int64[]:array;
#array;  // Prevent GC from moving array
```

### `wild` Objects (Manual)

`wild` objects are **never moved** by GC, so pinning is unnecessary:

```aria
wild int64[]:array = aria.alloc(sizeof(int64) * 100);
// No need to pin - already has stable address
```

### `stack` Objects (Stack Allocated)

Stack objects have **stable addresses** for their lifetime:

```aria
int64[100]:array;  // Stack allocated
// No need to pin - address stable until scope ends
```

**Rule**: Only `gc` objects need pinning.

---

## Best Practices

### ✅ Pin Before Passing to FFI

```aria
// GOOD: Pin before C interop
gc byte[]:buffer;
#buffer;
c_function(@buffer);
```

### ✅ Pin for Hardware Access

```aria
// GOOD: Pin for memory-mapped I/O
gc uint32@:mmioReg = 0x40000000 as uint32@;
#mmioReg;
*mmioReg = value;
```

### ✅ Pin for Lock-Free Algorithms

```aria
// GOOD: Pin for atomic operations
gc int64:atomicCounter;
#atomicCounter;
atomicIncrement(@atomicCounter);
```

### ❌ Don't Pin Unnecessarily

```aria
// BAD: No need to pin for pure Aria code
gc int64[]:array;
#array;
processArray(array);  // Aria function handles GC objects fine
```

### ❌ Don't Pin `wild` or `stack` Objects

```aria
// WRONG: wild objects already have stable addresses
wild int64[]:array = aria.alloc(1024);
#array;  // ❌ Unnecessary, possibly error

// WRONG: stack objects don't move
int64[100]:stackArray;
#stackArray;  // ❌ Unnecessary
```

---

## Safety Considerations

### Dangling Pointers After Unpin

```aria
gc int64:value;
#value;

int64@:ptr = @value;

~value;  // Unpin (tentative)

// ⚠️ GC might now move value, ptr becomes invalid!
doSomething(ptr);  // DANGER
```

**Solution**: Keep object pinned as long as pointers to it exist.

### Fragmentation from Excessive Pinning

```aria
// BAD: Pin everything (causes fragmentation)
gc int64[]:a, b, c, d, e;
#a; #b; #c; #d; #e;  // Heap fragmented

// GOOD: Pin only what's needed for FFI
gc int64[]:data;
#data;
c_function(@data);
~data;
```

---

## Advanced Examples

### Example 1: JNI-Style FFI

```aria
extern "native_lib" {
    func:processArray = void(int64*:array, uint64:length);
}

func:callNative = void(gc int64[]:data) {
    #data;  // Pin for C call
    defer ~data;  // Unpin when done (tentative)
    
    processArray(@data as int64*, data.length);
}
```

### Example 2: Lock-Free Stack

```aria
%STRUCT StackNode {
    int64:value,
    StackNode@:next
}

gc StackNode@:head = NIL;
#head;  // Pin head pointer

func:push = void(int64:value) {
    gc StackNode@:node = aria.alloc(sizeof(StackNode));
    #node;  // Pin new node
    
    node->value = value;
    
    // Lock-free push with CAS
    while (true) {
        StackNode@:oldHead = head;
        node->next = oldHead;
        
        if (compareAndSwap(@head, oldHead, node)) {
            break;  // Success
        }
    }
};
```

### Example 3: GPU Buffer

```aria
extern "cuda" {
    func:cudaMalloc = int64(void**:ptr, uint64:size);
    func:cudaMemcpy = int64(void*:dst, void*:src, uint64:size, int64:kind);
}

gc byte[1024]:hostBuffer;
#hostBuffer;  // Pin for GPU transfer

void*:deviceBuffer;
cudaMalloc(@deviceBuffer, 1024);
cudaMemcpy(deviceBuffer, @hostBuffer as void*, 1024, CUDA_MEMCPY_HOST_TO_DEVICE);
```

---

## Comparison with Other Languages

### Aria `#` Operator

```aria
gc int64[]:array;
#array;  // Pin in memory
c_function(@array);
```

### C# `fixed` Statement

```csharp
int[] array = new int[100];
fixed (int* ptr = array) {
    CFunction(ptr);
}  // Automatically unpinned
```

### Java `Unsafe.monitorEnter`

```java
// No direct pinning, use Unsafe or JNI
Unsafe unsafe = getUnsafe();
long address = unsafe.allocateMemory(1024);
```

### Rust `Pin<T>`

```rust
let value = Box::new(42);
let pinned = Box::pin(value);
// Guaranteed not to move
```

---

## Related Topics

- [Garbage Collection](../memory_model/gc.md) - GC memory strategy
- [Memory Allocation](../memory_model/allocation.md) - wild, gc, stack, wildx
- [@ Operator](at_operator.md) - Address-of and pointers
- [extern Blocks](../modules/extern.md) - C FFI interop
- [Atomic Operations](../stdlib/atomic.md) - Lock-free primitives

---

**Status**: Comprehensive (unpin syntax TBD)  
**Specification**: aria_specs.txt Line 179  
**Unique Feature**: Explicit GC pinning for stable addresses
