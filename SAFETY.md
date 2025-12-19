# ⚠️ ARIA SAFETY CONTRACT ⚠️

## **NO LANGUAGE IS TRULY SAFE ON ITS OWN**

Aria makes this a **CORE TENET**: Safety requires correct usage. We make developers explicitly acknowledge this.

### The Aria Safety Philosophy

**You CAN create a mess with Aria if you're not careful.**  
**But you CANNOT do it accidentally.**  
**You must explicitly "sign the TOS" via language features.**

---

## Safety Categories & Explicit Opt-Ins

### 1. **Memory Safety** - The `wild` Keyword

By default, Aria manages memory safely via GC. To opt-out:

```aria
wild int64@:ptr = @value;  // EXPLICIT: You're managing this pointer yourself
```

**What `wild` means**:
- ⚠️ You are responsible for deallocation
- ⚠️ No automatic memory management
- ⚠️ Potential for use-after-free, double-free, memory leaks
- ✅ You acknowledge these risks by using `wild`

**Compiler behavior**:
- `wild` variables trigger safety warnings at compile time
- Must explicitly free wild allocations or use `defer`
- Mixing wild and GC pointers requires explicit annotations

### 2. **Concurrency Safety** - The `async` Keyword

Concurrent code introduces data races and deadlocks:

```aria
async func:process_data = void(data@:ptr) {
    // EXPLICIT: You're in concurrent territory
}
```

**What `async` means**:
- ⚠️ Potential for data races without proper synchronization
- ⚠️ Deadlock risks with improper locking
- ⚠️ Memory ordering concerns
- ✅ You acknowledge these by using `async`

**Compiler behavior**:
- Warns when accessing shared mutable state without synchronization
- Requires explicit thread-safety annotations
- Checks for common race condition patterns

### 3. **Unsafe Pointer Operations** - The `@` Operator

Raw pointer manipulation is dangerous:

```aria
int64@:ptr = @variable;     // EXPLICIT: Taking address
wild int64@:raw = cast(ptr);  // EXPLICIT: Raw pointer cast
```

**What pointer operations mean**:
- ⚠️ No bounds checking
- ⚠️ Potential for buffer overflows
- ⚠️ Type safety violations via casting
- ✅ You acknowledge these by using `@` and casts

**Compiler behavior**:
- Pointer arithmetic triggers warnings
- Casts require explicit type annotation
- Dereferencing unchecked pointers shows safety notices

### 4. **FFI (Foreign Function Interface)** - The `extern` Keyword

Calling C code bypasses Aria's safety guarantees:

```aria
extern "libc" {
    func:malloc = void@(uint64:size);  // EXPLICIT: C function, all bets are off
}
```

**What `extern` means**:
- ⚠️ No Aria safety guarantees apply
- ⚠️ C functions can cause undefined behavior
- ⚠️ Memory management is manual
- ⚠️ Type system boundaries are crossed
- ✅ You acknowledge these by using `extern`

**Compiler behavior**:
- All extern calls show safety warnings in strict mode
- Requires explicit type signatures
- Warns about pointer type mismatches at FFI boundaries

### 5. **Executable Memory** - The `wildx` Keyword

JIT compilation and self-modifying code:

```aria
wildx byte@:jit_buffer = aria.alloc_buffer(size);  // EXPLICIT: Executable memory
```

**What `wildx` means**:
- ⚠️ Security implications (code injection risks)
- ⚠️ All memory safety concerns of `wild`
- ⚠️ Platform-specific behavior
- ⚠️ Potential for arbitrary code execution bugs
- ✅ You acknowledge these by using `wildx`

**Compiler behavior**:
- Requires explicit platform security acknowledgment
- Shows security warnings at compile time
- Suggests safer alternatives when available

---

## Compiler Safety Levels

### Level 0: Permissive (Default)
```bash
ariac file.aria  # Warnings shown but compilation proceeds
```
- Shows safety warnings for all explicit unsafe features
- Allows wild, async, extern, etc. with warnings
- Suitable for development

### Level 1: Strict
```bash
ariac --strict file.aria
```
- All safety warnings become errors unless explicitly acknowledged
- Requires safety acknowledgment comments
- Forces developers to think about every unsafe operation

### Level 2: Paranoid (Recommended for Production)
```bash
ariac --paranoid file.aria
```
- Maximum safety checks
- Requires explicit safety acknowledgments in code
- Enables all static analysis warnings
- Suggests safe alternatives for every unsafe operation
- Fails on any unacknowledged unsafe pattern

---

## Safety Acknowledgment System

### Inline Acknowledgments

For single operations:

```aria
// SAFETY: Manually managing memory for performance
wild int64@:buffer = aria.alloc(1024);
defer aria.free(buffer);
```

### Block Acknowledgments

For unsafe regions:

```aria
// SAFETY_BLOCK: FFI calls to legacy C library
// - All pointers validated before passing to C
// - Return values checked for null
// - Memory ownership transferred per C API docs
extern "legacy_lib" {
    func:process = int32(void@:data);
}
```

### Module-Level Acknowledgments

For entire modules with unsafe operations:

```aria
// SAFETY_MODULE: Low-level memory allocator implementation
// This module implements the core memory allocation primitives.
// All operations are carefully reviewed and tested.
// See docs/memory/allocator.md for safety analysis.

mod allocator {
    // ... unsafe operations ...
}
```

---

## The Safety Contract

When you use explicit unsafe features in Aria, you are signing this contract:

### ✅ **I Understand:**
1. This operation can cause memory corruption, data races, or undefined behavior
2. The compiler cannot verify the safety of this code
3. I am responsible for ensuring correctness
4. Bugs here can compromise the entire program

### ✅ **I Commit To:**
1. Documenting WHY this unsafe operation is necessary
2. Explaining HOW I ensure safety
3. Minimizing the scope of unsafe code
4. Testing thoroughly with sanitizers and validation tools

### ✅ **I Acknowledge:**
1. No language can prevent all errors
2. Safety requires correct usage, not just compiler checks
3. Explicit > Implicit for dangerous operations
4. I cannot claim "I didn't know this was unsafe"

---

## Examples: Safe vs Unsafe

### ❌ Accidentally Unsafe (Not Possible in Aria)

```aria
// This DOES NOT COMPILE - no implicit pointer operations
int64:x = 42;
int64:y = x;  // Safe copy
// No way to accidentally get raw pointer without explicit @
```

### ✅ Explicitly Unsafe (Requires Acknowledgment)

```aria
// SAFETY: Performance-critical pointer arithmetic for buffer scan
// Bounds checked before this loop, length validated
wild byte@:ptr = @buffer[0];
for (int64:i = 0; i < length; i += 1) {
    process(ptr[i]);  // Explicit pointer arithmetic
}
```

---

## Compiler Warnings Examples

### Wild Memory Warning
```
warning: wild memory allocation without deferred cleanup
  --> src/allocator.aria:42:5
   |
42 |     wild int64@:ptr = aria.alloc(size);
   |     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
   |
   = note: wild memory must be manually freed
   = help: consider using 'defer aria.free(ptr)' for automatic cleanup
   = help: or use GC-managed memory unless performance is critical
```

### Async Data Race Warning
```
warning: potential data race on shared mutable state
  --> src/worker.aria:89:9
   |
89 |         shared_counter += 1;
   |         ^^^^^^^^^^^^^^^^^^
   |
   = note: 'shared_counter' is accessed from multiple async contexts
   = help: protect with mutex or use atomic operations
   = help: see docs/concurrency/synchronization.md
```

### Pointer Dereference Warning
```
warning: dereferencing unchecked pointer
  --> src/unsafe.aria:24:9
   |
24 |         val = @ptr;
   |               ^^^^
   |
   = note: pointer may be null or invalid
   = help: check pointer validity before dereferencing
   = help: consider using optional types for nullable pointers
```

---

## Runtime Safety Checks

Even with explicit unsafe code, Aria provides optional runtime checks:

### Compile with Safety Checks
```bash
ariac --runtime-checks file.aria
```

Enables:
- Bounds checking on pointer arithmetic
- Null pointer checks before dereferences
- Double-free detection
- Use-after-free detection (limited)
- Stack overflow detection

**Performance Impact**: ~15-30% overhead  
**Use Case**: Development, testing, debugging

### Debug Build with Sanitizers
```bash
ariac --debug --sanitize=address,thread,undefined file.aria
```

Integrates with:
- AddressSanitizer (ASan) for memory errors
- ThreadSanitizer (TSan) for data races
- UndefinedBehaviorSanitizer (UBSan) for UB detection

---

## Best Practices

### 1. Minimize Unsafe Scope
```aria
func:safe_wrapper = result(int64:size) {
    if (size <= 0) {
        pass({err: "Invalid size", val: NIL});
    }
    
    // SAFETY: Size validated above, cleanup via defer
    wild byte@:buffer = aria.alloc(size);
    defer aria.free(buffer);
    
    // ... safe operations on buffer ...
    
    pass({err: NULL, val: processed_data});
}
```

### 2. Use Type System for Safety
```aria
// Safe abstraction over unsafe operations
struct SafeBuffer {
    wild byte@:data;
    int64:size;
    int64:capacity;
}

func:new_buffer = result(SafeBuffer, int64:capacity) {
    // Unsafe internals, safe interface
}
```

### 3. Document Safety Invariants
```aria
// SAFETY INVARIANT: 
// - ptr is always valid if non-null
// - len always represents valid element count
// - data remains allocated while StrView exists
struct StrView {
    byte@:ptr;
    int64:len;
}
```

---

## Summary

### ⚠️ **REMEMBER:**

**Aria gives you the tools to write unsafe code when necessary.**  
**But it makes you acknowledge the risks explicitly.**  
**You cannot accidentally shoot yourself in the foot.**  
**You must load the gun, aim it, and pull the trigger intentionally.**

### The Three Rules:

1. **Explicit Over Implicit** - Dangerous operations require explicit keywords
2. **Acknowledge, Don't Hide** - Use safety comments to document risks
3. **Minimize and Isolate** - Keep unsafe code small and well-contained

---

**If you find a way to cause undefined behavior without using `wild`, `@`, `extern`, `async`, or `wildx`... that's a compiler bug. Please report it!**

*This is not just documentation. This is a contract between the language and its users.*  