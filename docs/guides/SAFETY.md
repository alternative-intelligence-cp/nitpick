# Aria Safety Guidelines

**Version**: 0.0.7-dev  
**Last Updated**: January 5, 2026  
**Audience**: All Aria Developers

## Table of Contents

1. [The Safety Philosophy](#the-safety-philosophy)
2. [Safety Keywords](#safety-keywords)
3. [Compiler Safety Levels](#compiler-safety-levels)
4. [Safety Acknowledgment System](#safety-acknowledgment-system)
5. [Memory Safety](#memory-safety)
6. [Concurrency Safety](#concurrency-safety)
7. [FFI Safety](#ffi-safety)
8. [TBB Safety](#tbb-safety)
9. [Runtime Safety Checks](#runtime-safety-checks)
10. [Production Guidelines](#production-guidelines)
11. [Common Unsafe Patterns](#common-unsafe-patterns)
12. [Best Practices](#best-practices)

---

## The Safety Philosophy

### Core Principle

**"You CANNOT create a mess accidentally - only explicitly."**

Aria makes this a **fundamental design tenet**: Safety requires correct usage, not magic compiler tricks.

### The Three Pillars

1. **Explicitness**: Dangerous operations require explicit keywords
2. **Acknowledgment**: Document WHY unsafe operations are necessary
3. **Minimization**: Keep unsafe code small and well-contained

### The Safety Contract

When you write Aria code, you enter into a contract:

**The Language Promises**:
- No hidden memory allocations
- No implicit type conversions
- No silent integer overflows (with TBB)
- No accidental pointer operations
- No undefined behavior without explicit unsafe features

**You Promise**:
- To acknowledge unsafe operations explicitly
- To document safety invariants
- To test unsafe code thoroughly
- To minimize the scope of unsafe operations

---

## Safety Keywords

### 1. `wild` - Manual Memory Management

**Meaning**: "I will manage this memory manually."

```aria
wild int64*:buffer = aria_alloc(1024);  // EXPLICIT: Manual allocation
defer aria_free(buffer);                // MUST clean up!
```

**What `wild` means**:
- ⚠️ You are responsible for deallocation
- ⚠️ No automatic memory management
- ⚠️ Risk of use-after-free, double-free, memory leaks
- ✅ You acknowledge these risks

**When to use**:
- Performance-critical code where GC overhead matters
- Low-level system interfaces (allocators, drivers)
- FFI with C libraries that manage memory

**When NOT to use**:
- Application logic (use GC)
- Short-lived objects (use stack or GC)
- Unless profiling shows GC is actually a problem

### 2. `exec` - Executable Memory

**Meaning**: "This memory will contain executable code."

```aria
exec void*:jit_buffer = aria_alloc_buffer(4096, 64, 1);
```

**What `exec` means**:
- ⚠️ Security implications (code injection risks)
- ⚠️ All risks of `wild` memory
- ⚠️ Platform-specific behavior (W^X policies)
- ⚠️ Potential for arbitrary code execution bugs
- ✅ You acknowledge serious security risks

**When to use**:
- JIT compilers
- Dynamic code generation
- Runtime assemblers

**Security note**: Operating systems enforce **W^X (Write XOR Execute)** - memory cannot be both writable and executable simultaneously. Use `aria_alloc_buffer` with proper flags.

### 3. `extern` - FFI Boundary

**Meaning**: "This calls C code - all Aria safety guarantees are off."

```aria
extern "libc" {
    func:malloc = void*(uint64:size);  // EXPLICIT: C function
}
```

**What `extern` means**:
- ⚠️ No Aria safety guarantees apply
- ⚠️ C functions can cause undefined behavior
- ⚠️ Memory management is manual
- ⚠️ Type system boundaries crossed
- ✅ You acknowledge FFI risks

**See**: [FFI Guide](FFI.md) for complete FFI safety guidelines.

### 4. `async` - Concurrency

**Meaning**: "This code runs concurrently - watch for data races."

```aria
async func:worker = void() {
    // EXPLICIT: Concurrent code
}
```

**What `async` means**:
- ⚠️ Potential for data races without synchronization
- ⚠️ Deadlock risks with improper locking
- ⚠️ Memory ordering concerns
- ✅ You acknowledge concurrency risks

**Status**: Not yet implemented in v0.0.7 - placeholder for future.

### 5. `@` Operator - Address-Of

**Meaning**: "I'm taking a raw pointer."

```aria
int32:x = 42;
int32*:ptr = @x;  // EXPLICIT: Taking address
```

**What `@` means**:
- ⚠️ Creating a raw pointer
- ⚠️ No automatic bounds checking
- ⚠️ Lifetime management is manual
- ✅ You acknowledge pointer risks

**Safe alternative**: Use references or GC pointers when possible.

---

## Compiler Safety Levels

### Level 0: Permissive (Default)

```bash
ariac file.aria  # Warnings shown but compilation proceeds
```

**Behavior**:
- Shows safety warnings for unsafe features
- Allows `wild`, `extern`, etc. with warnings
- Suitable for development and prototyping

**Example warning**:
```
warning: wild memory allocation without deferred cleanup
  --> src/buffer.aria:42:5
   |
42 |     wild int64*:ptr = aria_alloc(1024);
   |     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
   |
help: consider using 'defer aria_free(ptr)'
```

### Level 1: Strict

```bash
ariac --strict file.aria
```

**Behavior**:
- Safety warnings become **errors** unless acknowledged
- Requires safety comments for unsafe operations
- Forces developers to think about every unsafe op

**Example error**:
```
error: wild memory allocation requires safety acknowledgment
  --> src/buffer.aria:42:5
   |
42 |     wild int64*:ptr = aria_alloc(1024);
   |     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
   |
help: add safety comment:
      // SAFETY: <explanation>
```

### Level 2: Paranoid (Production Recommended)

```bash
ariac --paranoid file.aria
```

**Behavior**:
- Maximum safety checks enabled
- Requires explicit acknowledgments in code
- Suggests safe alternatives for every unsafe operation
- Fails on any unacknowledged unsafe pattern

**Use for**: Production builds, security-critical code, audited systems.

---

## Safety Acknowledgment System

### Inline Acknowledgments

For single operations:

```aria
// SAFETY: Manually managing memory for performance
// Arena allocator pattern - all allocations freed at scope end
wild int64*:buffer = aria_alloc(1024);
defer aria_free(buffer);
```

**Template**:
```aria
// SAFETY: <WHY this is necessary> 
//         <HOW you ensure safety>
<unsafe operation>
```

### Block Acknowledgments

For unsafe regions:

```aria
// SAFETY_BLOCK: FFI calls to legacy POSIX library
// - All pointers validated for null before passing to C
// - Return values checked for error codes
// - Memory ownership transferred per POSIX specification
// - errno checked after each call
extern "libc" {
    func:open = int32(int8*:path, int32:flags);
    func:close = int32(int32:fd);
    func:read = int64(int32:fd, void*:buf, uint64:count);
}
```

### Function-Level Acknowledgments

For functions with complex safety requirements:

```aria
// SAFETY_FUNC: arena_alloc
// INVARIANTS:
//   - arena.base is always valid and aligned to 8 bytes
//   - arena.offset + size never exceeds arena.capacity
//   - arena.offset is always aligned to 8 bytes
// CALLER MUST:
//   - Ensure size > 0
//   - Free entire arena with arena_destroy (not individual allocations)
func:arena_alloc = void*(Arena*:arena, uint64:size) {
    // ...
}
```

### Module-Level Acknowledgments

For entire modules with unsafe operations:

```aria
// SAFETY_MODULE: mem.aria - Low-level memory primitives
// 
// This module implements core memory allocation and management.
// All operations are carefully reviewed and tested with sanitizers.
// 
// Safety analysis: docs/safety/memory_allocator.md
// Test coverage: 98% (see tests/mem/)
// Fuzz testing: 1M iterations with no crashes
// 
// Unsafe operations:
//   - aria_alloc: Manual memory allocation
//   - aria_free: Manual deallocation
//   - aria_realloc: Pointer invalidation on resize
// 
// Safe wrappers available in std.alloc module.

// ... module implementation ...
```

---

## Memory Safety

### The Three Memory Models

Aria supports three memory management strategies:

1. **GC (Garbage Collected)** - Default, safe
2. **Wild (Manual)** - Explicit, performance-critical
3. **Exec (Executable)** - JIT compilation, highest risk

### GC Memory (Safe)

```aria
// Default: GC-managed, automatic cleanup
gc int64*:data = gc_alloc(8);
// No explicit free needed - GC handles it
```

**Advantages**:
- ✅ No memory leaks
- ✅ No use-after-free
- ✅ No double-free

**Disadvantages**:
- ❌ Unpredictable latency (GC pauses)
- ❌ Higher memory overhead
- ❌ Not suitable for real-time systems

### Wild Memory (Unsafe)

```aria
// SAFETY: Performance-critical tight loop, no GC pauses allowed
wild int64*:buffer = aria_alloc(1024);
defer aria_free(buffer);  // Automatic cleanup at scope end
```

**Best practices**:
1. **Always use `defer` for cleanup**
2. **Validate allocation success** (check for NULL)
3. **Document why GC isn't suitable**
4. **Keep scope small**

### Exec Memory (Highly Unsafe)

```aria
// SAFETY: JIT compiler - generating x86-64 machine code
// W^X compliance: buffer allocated RW, then marked RX before execution
exec void*:code = aria_alloc_buffer(4096, 64, 1);
```

**Security checklist**:
- [ ] Allocate with `aria_alloc_buffer` (W^X compliant)
- [ ] Generate code into buffer (writable phase)
- [ ] Mark executable before calling (read-execute phase)
- [ ] Validate generated code doesn't have exploitable bugs
- [ ] Free with `aria_free_buffer` (not regular `aria_free`)

**See**: `docs/jit_guide.md` for complete JIT safety guidelines.

### Memory Lifetime Rules

1. **Stack variables**: Lifetime = scope
   ```aria
   func:example = void() {
       int32:x = 42;  // Lives until end of function
   }
   ```

2. **GC allocations**: Lifetime = until no references exist
   ```aria
   gc int32*:ptr = gc_alloc(4);  // Lives until GC determines it's unreachable
   ```

3. **Wild allocations**: Lifetime = until explicit free
   ```aria
   wild int32*:ptr = aria_alloc(4);
   aria_free(ptr);  // MUST be called manually
   ```

### Use-After-Free Prevention

**Dangerous**:
```aria
wild int32*:ptr = aria_alloc(4);
aria_free(ptr);
@ptr;  // CRASH! Use-after-free
```

**Safe pattern**:
```aria
wild int32*:ptr = aria_alloc(4);
defer aria_free(ptr);  // Automatic at scope end
// Use ptr safely within scope
```

**Ultra-safe pattern** (paranoid mode):
```aria
wild int32*:ptr = aria_alloc(4);
defer {
    aria_free(ptr);
    ptr = NULL;  // Prevent accidental reuse
};
```

### Double-Free Prevention

**Dangerous**:
```aria
wild int32*:ptr = aria_alloc(4);
aria_free(ptr);
aria_free(ptr);  // CRASH! Double-free
```

**Safe pattern 1**: Use `defer` (only frees once)
```aria
wild int32*:ptr = aria_alloc(4);
defer aria_free(ptr);  // Guaranteed single free at scope end
```

**Safe pattern 2**: Nullify after free
```aria
wild int32*:ptr = aria_alloc(4);
// ... use ptr ...
aria_free(ptr);
ptr = NULL;  // Prevent double-free
```

---

## Concurrency Safety

**Status**: Concurrency primitives not yet implemented in v0.0.7.

**Future guidelines**:

### Data Race Prevention

```aria
// FUTURE: async keyword for concurrent code
struct SharedCounter {
    mutex:lock;
    int64:value;
}

async func:increment = void(SharedCounter*:counter) {
    // SAFETY: Mutex protects value from data races
    lock(counter->lock);
    defer unlock(counter->lock);
    
    counter->value += 1;  // Safe: protected by mutex
}
```

### Deadlock Prevention

```aria
// FUTURE: Lock ordering discipline
const LOCK_ORDER_DB = 1;
const LOCK_ORDER_CACHE = 2;

// Always acquire in order: DB → Cache
func:update_cache = void() {
    lock(db.lock);         // Order 1
    defer unlock(db.lock);
    
    lock(cache.lock);      // Order 2
    defer unlock(cache.lock);
    
    // Safe: consistent lock ordering prevents deadlocks
}
```

---

## FFI Safety

**See [FFI Guide](FFI.md) for complete documentation.**

### Quick Checklist

Before calling any C function:

- [ ] Declared in `extern` block?
- [ ] Type signatures match C exactly?
- [ ] Pointer parameters validated (non-null)?
- [ ] Return value checked for errors (NULL, -1, etc.)?
- [ ] Memory ownership documented (who frees)?
- [ ] TBB sentinels checked before passing?
- [ ] Struct layouts verified with `@sizeof`?

### Safe FFI Wrapper Pattern

```aria
extern "libc" {
    func:fopen = FILE*(int8*:path, int8*:mode);
    func:fclose = int32(FILE*:fp);
}

// Safe wrapper
func:open_file = result<FILE*>(int8*:path, int8*:mode) {
    // SAFETY: fopen may return NULL on failure
    FILE*:fp = fopen(path, mode);
    
    if (fp == NULL) {
        pass(ERR);  // Signal error
    } else {
        pass(fp);   // Return valid handle
    };
};

// Usage (safe)
func:read_config = void() {
    result<FILE*>:file_result = open_file("config.txt", "r");
    
    if (@is_err_result(file_result)) {
        print("Failed to open config");
        pass();
    } else {};
    
    FILE*:fp = file_result;
    // ... use fp safely ...
    fclose(fp);
}
```

---

## TBB Safety

**See [Type System Guide](TYPE_SYSTEM.md) for complete TBB documentation.**

### Overflow Detection

```aria
tbb32:a = 1000000000;  // 1 billion
tbb32:b = 3;
tbb32:c = a * b;  // Overflow! c = ERR32

if (@is_err_tbb32(c)) {
    print("Arithmetic overflow detected");
} else {
    print("Result: ");
    print_int(c);
};
```

### Error Propagation

```aria
// SAFETY: TBB sticky error semantics
// Any ERR in pipeline propagates automatically
tbb32:x = compute_step1();  // May return ERR
tbb32:y = x + 100;          // If x=ERR, y=ERR
tbb32:z = y * 2;            // If y=ERR, z=ERR

// Check once at the end
if (@is_err_tbb32(z)) {
    print("Computation failed at some step");
    pass();
} else {
    print("Final result: ");
    print_int(z);
};
```

### FFI Boundary

**Critical**: TBB sentinels are **lost** when passing to C:

```aria
// UNSAFE: Don't pass TBB to C without checking
tbb32:value = compute();  // May be ERR

extern "libc" {
    func:process = void(int32:x);
}

process(value);  // BUG! C sees ERR (-2147483648) as normal int!

// SAFE: Check before FFI
if (@is_err_tbb32(value)) {
    print("Cannot process error value");
} else {
    process(value);  // Safe: valid value only
};
```

---

## Runtime Safety Checks

### Development Builds

```bash
# Enable runtime bounds checking, null checks, etc.
ariac --runtime-checks src/main.aria
```

**Enables**:
- Bounds checking on array accesses
- Null pointer checks before dereferences
- Double-free detection
- Use-after-free detection (limited)
- Stack overflow detection

**Performance impact**: ~15-30% slowdown  
**Use case**: Development, testing, debugging

### Sanitizer Builds

```bash
# AddressSanitizer (memory errors)
ariac --sanitize=address src/main.aria

# ThreadSanitizer (data races)
ariac --sanitize=thread src/main.aria

# UndefinedBehaviorSanitizer
ariac --sanitize=undefined src/main.aria

# Combine multiple
ariac --sanitize=address,undefined src/main.aria
```

**Use case**: Pre-production validation, CI/CD pipelines.

### Production Builds

```bash
# No runtime checks (maximum performance)
ariac --release src/main.aria
```

**Behavior**:
- All runtime checks disabled
- Optimizations enabled
- Undefined behavior on violations

**Safety guarantee**: If code passes sanitizers and runtime checks in testing, production builds are safe.

---

## Production Guidelines

### 1. Testing Strategy

**Required for production code**:

1. **Unit tests** (test individual functions)
2. **Integration tests** (test FFI boundaries, system integration)
3. **Sanitizer runs** (ASan, TSan, UBSan)
4. **Fuzz testing** (for parsers, network code)
5. **Stress testing** (memory leaks, concurrency stress)

### 2. Code Review Checklist

Before merging code with unsafe operations:

- [ ] All unsafe operations have safety comments
- [ ] Safety invariants documented
- [ ] Alternative safe approaches considered and rejected
- [ ] Test coverage ≥ 95% for unsafe code
- [ ] Sanitizer clean (no warnings)
- [ ] Peer reviewed by senior developer

### 3. Unsafe Code Metrics

**Track these metrics** in production codebases:

| Metric | Target |
|--------|--------|
| % unsafe code | < 5% of codebase |
| Unsafe functions | < 10% of total functions |
| Lines with `wild` | < 2% of total lines |
| Files with `extern` | < 15% of total files |

If metrics exceed targets, **refactor** to reduce unsafe surface area.

### 4. Security Auditing

**Required for security-critical code**:

1. Manual code audit by security expert
2. Automated static analysis (if available)
3. Penetration testing
4. Memory safety analysis (all paths verified)

### 5. Dependency Management

**Third-party C libraries**:

- [ ] Audit C library code for security issues
- [ ] Wrap all C functions in safe Aria wrappers
- [ ] Document memory ownership for each FFI function
- [ ] Test with sanitizers enabled
- [ ] Pin versions (avoid surprise breaking changes)

---

## Common Unsafe Patterns

### Pattern 1: Manual Memory Management

**Use case**: Performance-critical allocations

```aria
// SAFETY: Arena allocator for parse tree (5000+ nodes)
// All nodes freed together at end of compilation pass
// Profiled: 40% faster than GC, no fragmentation
wild ArenaAllocator*:arena = arena_create(1024 * 1024);  // 1MB
defer arena_destroy(arena);

// Allocate from arena
ASTNode*:node = arena_alloc(arena, @sizeof(ASTNode));
```

### Pattern 2: FFI with C Libraries

**Use case**: System calls, legacy code integration

```aria
// SAFETY_BLOCK: POSIX file I/O
// - All file descriptors closed via defer
// - Error codes checked after each syscall
// - errno preserved across calls
extern "libc" {
    func:open = int32(int8*:path, int32:flags);
    func:read = int64(int32:fd, void*:buf, uint64:count);
    func:close = int32(int32:fd);
}

func:read_file = result<int8*>(int8*:path) {
    int32:fd = open(path, O_RDONLY);
    
    if (fd == -1) {
        pass(ERR);
    } else {};
    
    defer close(fd);
    
    // ... read operations ...
}
```

### Pattern 3: JIT Compilation

**Use case**: Runtime code generation

```aria
// SAFETY: x86-64 JIT compiler
// - Code buffer allocated with W^X compliance
// - Generated code validated for ROP gadgets
// - Buffer marked executable only after generation complete
exec void*:code = aria_alloc_buffer(4096, 64, 1);
defer aria_free_buffer(code);

// Generate machine code...
// (see docs/jit_guide.md for full JIT safety guidelines)
```

### Pattern 4: Performance-Critical Loop

**Use case**: Inner loop optimization

```aria
// SAFETY: Pointer arithmetic for vectorized scan
// - bounds checked before loop (0 <= idx < buffer_len)
// - alignment verified (buffer aligned to 16 bytes)
// - length multiple of 16 (SIMD requirement)
wild uint8*:ptr = @buffer[0];

loop (uint64:i = 0; i < len; i += 16) {
    // Process 16 bytes at once (SIMD)
    __m128i data = _mm_load_si128(ptr + i);
    // ...
}
```

---

## Best Practices

### 1. Minimize Unsafe Scope

**Bad** (large unsafe region):
```aria
wild int8*:buffer = aria_alloc(1024);

// 100 lines of code...

aria_free(buffer);
```

**Good** (small unsafe region):
```aria
func:process_data = result() {
    // SAFETY: Temporary buffer for batch processing
    wild int8*:buffer = aria_alloc(1024);
    defer aria_free(buffer);  // Cleanup right after allocation
    
    // Use buffer in small, controlled scope
    pass(NIL);
}
```

### 2. Use Type System for Safety

```aria
// Safe abstraction over unsafe internals
struct SafeBuffer {
    wild int8*:data;
    uint64:len;
    uint64:capacity;
}

// Unsafe operations hidden in module
func:sb_new = SafeBuffer(uint64:capacity) {
    // SAFETY: Internal allocation, managed by SafeBuffer
    wild int8*:data = aria_alloc(capacity);
    
    pass({
        data: data,
        len: 0,
        capacity: capacity
    });
};

func:sb_free = void(SafeBuffer:buf) {
    aria_free(buf.data);
}

// Users never see 'wild' - safe interface only
```

### 3. Document All Invariants

```aria
// INVARIANTS:
//   - arena.base is 8-byte aligned
//   - arena.offset ≤ arena.capacity
//   - arena.offset is always 8-byte aligned
//   - All allocations from arena are 8-byte aligned
struct Arena {
    wild void*:base;
    uint64:capacity;
    uint64:offset;
}
```

### 4. Test Unsafe Code Exhaustively

```aria
// Test file: tests/arena_test.aria

func:test_arena_alignment = void() {
    Arena*:arena = arena_create(1024);
    
    // Test alignment invariant
    void*:p1 = arena_alloc(arena, 1);
    void*:p2 = arena_alloc(arena, 1);
    
    // Verify 8-byte alignment
    if (((uint64)p1 % 8) != 0) { @panic("p1 misaligned"); } else {};
    if (((uint64)p2 % 8) != 0) { @panic("p2 misaligned"); } else {};
    
    arena_destroy(arena);
}

// Run with sanitizers:
// $ ariac --sanitize=address tests/arena_test.aria
// $ ./arena_test
```

### 5. Prefer Safe Alternatives

**Before writing unsafe code**, ask:

1. Can I use GC instead of `wild`?
2. Can I use a safe wrapper from stdlib?
3. Can I restructure to avoid pointers?
4. Can I use TBB types for overflow safety?
5. Can I isolate unsafe code to a small module?

**Example** - safe alternative:

```aria
// AVOID: Manual string building (unsafe)
wild int8*:buf = aria_alloc(1024);
// ... manual string manipulation ...
aria_free(buf);

// PREFER: Use stdlib string builder (safe)
import string;
StringBuilder:sb = sb_new();
sb_append(sb, "Hello");
sb_append(sb, " World");
int8*:result = sb_to_string(sb);  // Safe, GC-managed
```

---

## Summary

### The Aria Safety Guarantee

**IF** you avoid these keywords:
- `wild` (manual memory)
- `exec` (executable memory)
- `extern` (FFI)
- `@` (address-of)

**THEN** Aria guarantees:
- ✅ No memory leaks (GC handles it)
- ✅ No use-after-free
- ✅ No double-free
- ✅ No buffer overflows (bounds checked)
- ✅ No integer overflow bugs (with TBB)
- ✅ No null pointer dereferences (Optional types)

**WHEN** you use unsafe features:
- ⚠️ You explicitly acknowledge risks
- ⚠️ You document safety invariants
- ⚠️ You test thoroughly
- ⚠️ You take responsibility

### Final Checklist

Before shipping production code:

- [ ] All unsafe operations documented with `// SAFETY:` comments
- [ ] Safety invariants documented for all unsafe data structures
- [ ] Test coverage ≥ 95% for unsafe code paths
- [ ] Sanitizer clean (ASan, UBSan, TSan if concurrent)
- [ ] Code review by senior developer
- [ ] Metrics: < 5% unsafe code in codebase
- [ ] Alternative safe approaches considered and rejected
- [ ] Runtime checks enabled in development builds
- [ ] Paranoid mode compilation succeeds

---

## See Also

- [Type System Guide](TYPE_SYSTEM.md) - TBB safety, overflow detection
- [FFI Guide](FFI.md) - Complete FFI safety guidelines
- [Intrinsics Reference](../stdlib/INTRINSICS.md) - Safety-related intrinsics
- [JIT Guide](../jit_guide.md) - Executable memory safety

---

**Remember**: 

> "You cannot accidentally shoot yourself in the foot.  
> You must load the gun, aim it, and pull the trigger intentionally."

**If you find a way to cause undefined behavior without using unsafe keywords, that's a compiler bug. Please report it!**

---

**Maintained by**: Aria Compiler Team  
**License**: MIT  
**Contact**: https://ai-liberation-platform.org
