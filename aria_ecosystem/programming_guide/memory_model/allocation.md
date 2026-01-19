# Memory Allocation Strategies

**Category**: Memory Model → Allocation  
**Keywords**: `wild`, `gc`, `stack`, `wildx`  
**Purpose**: Understanding Aria's hybrid memory management system

---

## Overview

Aria provides **four distinct memory allocation strategies**, each optimized for different use cases:

| Strategy | Management | Speed | Safety | Use Case |
|----------|-----------|-------|--------|----------|
| **wild** | Manual | Fastest | Unsafe | Performance-critical, full control |
| **gc** | Automatic | Fast | Safe | General purpose, convenience |
| **stack** | Automatic | Fastest | Limited | Local variables, short-lived data |
| **wildx** | Manual | Fast | Unsafe | Executable memory (JIT compilers) |

**Philosophy**: You choose the right tool for each allocation. No one-size-fits-all.

---

## Stack Allocation (Default)

### Definition

**Stack allocation** is the default for local variables. Memory is automatically allocated when entering a scope and freed when exiting.

- **Management**: Automatic (compiler-managed)
- **Lifetime**: Scope-bound
- **Speed**: Fastest (just pointer increment)
- **Limitation**: Fixed size, short-lived

### Usage

```aria
func:example = result() {
    // Stack-allocated by default
    int64:x = 100;           // On stack
    string:name = "Alice";   // On stack
    int32[10]:buffer;        // 10 int32s on stack
    
    Point:p = {x: 10, y: 20}; // Struct on stack
    
    // All automatically freed when function returns
    pass(NIL);
};
```

### When to Use Stack

- ✅ Local variables with known, small size
- ✅ Short-lived data (function scope)
- ✅ Performance-critical hot paths
- ✅ When size is known at compile time
- ❌ Large allocations (risk stack overflow)
- ❌ Data that outlives function scope
- ❌ Dynamically sized data

---

## Wild Memory (Manual Management)

### Definition

**Wild memory** is manually managed heap allocation. You explicitly allocate and must explicitly free. No automatic cleanup.

- **Management**: Manual (you control everything)
- **Lifetime**: Explicit (alloc → free)
- **Speed**: Very fast (no GC overhead)
- **Risk**: Memory leaks if you forget to free

### Allocation and Deallocation

```aria
// Allocate wild memory
wild int64@:ptr = aria.alloc<int64>(100);  // 100 int64s

// Check for allocation failure
if (ptr == NULL) {
    stderr.write("Allocation failed");
    fail(1);
}

// Use the memory
ptr[0] = 42;
ptr[99] = 1337;

// MUST manually free
aria.free(ptr);
```

### The `defer` Pattern (RAII)

**Best practice**: Use `defer` to ensure memory is always freed, even on early returns:

```aria
func:processData = Result<int64>(uint64:size) {
    wild int64@:buffer = aria.alloc<int64>(size);
    
    if (buffer == NULL) {
        fail(1);  // Early return - no cleanup needed yet
    }
    
    // defer ensures free() runs at scope exit
    defer aria.free(buffer);
    
    // Process data...
    if (someCondition) {
        fail(2);  // Early return - defer still runs!
    }
    
    // More processing...
    buffer[0] = computeValue();
    
    pass(buffer[0]);  // defer runs after this return too
};
```

### Specialized Wild Allocators

```aria
// Buffer allocation (for I/O, networking)
wild uint8@:iobuf = aria.alloc_buffer(4096);
defer aria.free(iobuf);

// String allocation (for dynamic strings)
wild string@:str = aria.alloc_string(1024);
defer aria.free(str);

// Array allocation (for dynamic arrays)
wild int32@:arr = aria.alloc_array<int32>(1000);
defer aria.free(arr);
```

### When to Use Wild

- ✅ Performance-critical allocations (no GC overhead)
- ✅ Interop with C libraries (predictable layout)
- ✅ Large, long-lived data structures
- ✅ When you need full control of lifetime
- ✅ Embedded systems (deterministic memory)
- ❌ General-purpose code (prefer GC)
- ❌ When forgetting free() is likely
- ❌ Complex object graphs (hard to track)

---

## Garbage Collection (Automatic Management)

### Definition

**GC memory** is automatically managed. The garbage collector tracks references and frees memory when no longer reachable.

- **Management**: Automatic (GC-managed)
- **Lifetime**: Reference-counted or traced
- **Speed**: Fast (slight GC overhead)
- **Safety**: Memory-safe (no leaks)

### Allocation

```aria
// Allocate GC memory
gc obj@:user = aria.gc_alloc<obj>();

// No need to free - GC handles it
user->name = "Bob";
user->age = 30;

// When 'user' goes out of scope and no references remain,
// GC automatically frees the memory
```

### GC Initialization

The GC must be initialized at program start:

```aria
func:main = int64() {
    // Initialize GC (typically done automatically by runtime)
    aria.gc_init(0, 0);  // Default params
    
    // Now GC allocations work
    gc obj@:data = aria.gc_alloc<obj>();
    
    pass(0);
};
```

### Mixed Wild and GC

You can mix allocation strategies in the same program:

```aria
func:mixedAllocation = result() {
    // Stack allocation
    int64:counter = 0;
    
    // Wild allocation
    wild int64@:wildBuf = aria.alloc<int64>(1000);
    defer aria.free(wildBuf);
    
    // GC allocation
    gc obj@:gcData = aria.gc_alloc<obj>();
    
    // All three coexist peacefully
    counter++;
    wildBuf[0] = counter;
    gcData->value = counter;
    
    pass(NIL);
};
```

### When to Use GC

- ✅ General-purpose allocations
- ✅ Complex object graphs with shared ownership
- ✅ When forgetting to free would cause leaks
- ✅ Rapid prototyping and development
- ✅ High-level application code
- ❌ Real-time systems (GC pauses)
- ❌ Performance-critical inner loops
- ❌ C FFI interop (unpredictable addresses)

---

## Executable Memory (JIT Compilation)

### Definition

**Wildx memory** is executable wild memory for JIT (Just-In-Time) compilation. Allows runtime code generation.

- **Management**: Manual (like wild)
- **Lifetime**: Explicit (alloc → free)
- **Permission**: Executable (can run as code)
- **Use Case**: JIT compilers, dynamic code generation

### Allocation

```aria
// Allocate executable memory for JIT
wildx uint8@:codeBuf = aria.alloc_executable(4096);

if (codeBuf == NULL) {
    fail(1);
}
defer aria.free_executable(codeBuf);

// Write machine code to buffer
codeBuf[0] = 0x48;  // x86-64 instruction bytes
codeBuf[1] = 0xB8;
codeBuf[2] = 0x2A;
// ... (emit compiled code)

// Cast to function pointer and execute
func:generated = int64() {
    // Call generated code
};
```

### Security Considerations

Executable memory is **dangerous**:

- ⚠️ Must validate all generated code
- ⚠️ Vulnerable to code injection attacks
- ⚠️ OS may restrict executable allocations
- ⚠️ Use only when absolutely necessary

### When to Use Wildx

- ✅ JIT compiler implementation
- ✅ Dynamic code generation
- ✅ Runtime optimization engines
- ✅ Scripting language VMs
- ❌ General-purpose code
- ❌ User-controlled data
- ❌ Security-sensitive applications

---

## Comparison Matrix

| Feature | stack | wild | gc | wildx |
|---------|-------|------|-----|-------|
| **Allocation Speed** | Instant | Very Fast | Fast | Fast |
| **Deallocation** | Automatic | Manual | Automatic | Manual |
| **Lifetime Control** | Scope | Explicit | GC-decided | Explicit |
| **Memory Safety** | Safe | Unsafe | Safe | Unsafe |
| **GC Overhead** | None | None | Some | None |
| **Leak Risk** | None | High | None | High |
| **Executable** | No | No | No | Yes |
| **C FFI Compatible** | Limited | Yes | No | Yes |
| **Size Limit** | Small | Large | Large | Medium |

---

## Common Patterns

### Pattern 1: Stack + Defer for RAII

```aria
func:processFile = Result<obj>(string:path) {
    wild File@:file = openFile(path);
    if (file == NULL) { fail(1); }
    defer file->close();  // RAII cleanup
    
    wild uint8@:buffer = aria.alloc_buffer(4096);
    if (buffer == NULL) { fail(2); }
    defer aria.free(buffer);
    
    // Both cleaned up automatically on any return path
    obj:data = file->read(buffer, 4096);
    pass(data);
};
```

### Pattern 2: GC for Complex Data Structures

```aria
struct:Node = {
    int64:value;
    gc Node@:left;   // GC handles cycles
    gc Node@:right;
};

func:buildTree = Result<Node@>(int64:depth) {
    gc Node@:root = aria.gc_alloc<Node>();
    root->value = depth;
    
    if (depth > 0) {
        root->left = buildTree(depth - 1) ? NULL;
        root->right = buildTree(depth - 1) ? NULL;
    }
    
    pass(root);  // No need to free - GC handles tree
};
```

### Pattern 3: Wild for Performance

```aria
func:matrixMultiply = Result<flt64@>(flt64@:a, flt64@:b, int64:n) {
    wild flt64@:result = aria.alloc<flt64>(n * n);
    if (result == NULL) { fail(1); }
    defer aria.free(result);
    
    // Hot loop - no GC overhead
    for (int64:i = 0; i < n; i++) {
        for (int64:j = 0; j < n; j++) {
            flt64:sum = 0.0;
            for (int64:k = 0; k < n; k++) {
                sum += a[i * n + k] * b[k * n + j];
            }
            result[i * n + j] = sum;
        }
    }
    
    pass(result);  // Caller takes ownership
};
```

### Pattern 4: Wildx for JIT

```aria
func:compileFunction = Result<func>(obj:ast) {
    wildx uint8@:code = aria.alloc_executable(1024);
    if (code == NULL) { fail(1); }
    
    // Emit machine code
    emitPrologue(code);
    emitBody(code, ast);
    emitEpilogue(code);
    
    // Return function pointer
    func:compiled = code;
    pass(compiled);  // Caller must free with aria.free_executable
};
```

---

## Memory Pinning (GC Interaction)

### The `#` Operator

When passing GC memory to C functions, you must **pin** it to prevent the GC from moving it:

```aria
gc string@:str = aria.gc_alloc<string>();
str->data = "Hello from GC";

extern "mylib" {
    func:processCString = int32(char*:str);
}

// Pin GC memory for C FFI
#str;  // Prevent GC from moving str
int32:result = processCString(str->data);
// Automatically unpinned at scope exit
```

### Why Pinning is Necessary

GC may move objects during collection. C pointers must stay valid:

```aria
gc obj@:gcData = aria.gc_alloc<obj>();

// WRONG: Don't pass GC pointers to C without pinning
callCFunction(gcData);  // ❌ GC might move gcData!

// CORRECT: Pin first
#gcData;
callCFunction(gcData);  // ✅ Safe, gcData won't move
```

---

## Gotchas and Best Practices

### ❌ Forgetting to Free Wild Memory

```aria
// WRONG: Memory leak!
func:leak = result() {
    wild int64@:ptr = aria.alloc<int64>(1000);
    // ... use ptr ...
    pass(NIL);  // Forgot aria.free(ptr)! LEAK!
}

// CORRECT: Always use defer
func:correct = result() {
    wild int64@:ptr = aria.alloc<int64>(1000);
    defer aria.free(ptr);  // Guaranteed cleanup
    pass(NIL);
}
```

### ❌ Using Stack for Large Allocations

```aria
// WRONG: Stack overflow risk!
func:danger = result() {
    int64[1000000]:huge;  // 8 MB on stack! ⚠️
    pass(NIL);
}

// CORRECT: Use heap (wild or gc)
func:safe = result() {
    wild int64@:huge = aria.alloc<int64>(1000000);
    defer aria.free(huge);
    pass(NIL);
}
```

### ❌ Returning Pointers to Stack Memory

```aria
// WRONG: Dangling pointer!
func:danger = int64@() {
    int64:local = 42;
    pass(@local);  // ❌ local freed when function returns!
}

// CORRECT: Return heap-allocated memory
func:safe = int64@() {
    wild int64@:heap = aria.alloc<int64>(1);
    *heap = 42;
    pass(heap);  // Caller must free
}
```

### ✅ Check for Allocation Failure

```aria
// Always check wild allocations
wild int64@:ptr = aria.alloc<int64>(size);
if (ptr == NULL) {
    stderr.write("Allocation failed");
    fail(1);
}
defer aria.free(ptr);
```

### ✅ Use GC for Prototypes, Wild for Production

```aria
// During development: Use GC for safety
gc obj@:data = aria.gc_alloc<obj>();
// No defer needed, no leak risk

// For production hot paths: Switch to wild
wild obj@:data = aria.alloc<obj>(1);
defer aria.free(data);
// Faster, more predictable
```

---

## Related Topics

- [defer Keyword](defer.md) - RAII-style automatic cleanup
- [Pointers](../types/pointers.md) - Pointer syntax and semantics
- [Pinning Operator](../operators/pin.md) - `#` for GC pinning
- [Memory Safety](safety.md) - Borrowing and ownership
- [FFI System](../modules/extern.md) - C interop considerations

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Lines 143-145, 148-154  
**Key Insight**: No silver bullet - choose the right strategy for each allocation
