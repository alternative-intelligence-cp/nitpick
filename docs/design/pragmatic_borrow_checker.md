# Designing a Pragmatic Borrow Checker: Learning from Rust's Mistakes

## The Rust Problem

**The Core Issue:**
Rust's borrow checker is so restrictive that developers frequently resort to `unsafe` blocks, defeating the entire purpose. When correct, valid code can't be expressed in safe Rust, something is fundamentally broken.

**Examples of Valid Patterns Rust Makes Hard:**
1. Self-referential structs (absolutely valid in reality)
2. Multiple mutable references when you KNOW they don't alias
3. Interior mutability patterns (hence RefCell, Cell, etc.)
4. Graph data structures with cycles
5. Intrusive linked lists
6. Split borrows (borrowing different fields of a struct mutably)

**The Learning Curve Problem:**
When "steep learning curve" is the FIRST thing mentioned about a language, that's a design failure. The borrow checker should catch bugs, not require a PhD to use.

## Aria's Philosophical Advantage

### 1. GC is the Default
```aria
// This just works - GC handles it
struct Node {
    value: int32,
    next: Node?,  // Optional reference, GC tracked
};
```

**Implication:** The borrow checker only needs to enforce safety for:
- `wild` memory (explicit opt-out of GC)
- Data race prevention
- Explicit pinning with `#`

This is a MUCH smaller problem space than Rust.

### 2. Three-Tier Memory Model

```aria
int32:x = 42              // GC memory (safe, automatic)
wild int32@:ptr = ...     // Wild memory (manual, borrow checked)
int32$:ref = $x           // Borrow (prevents moves/mutations)
```

**Key Insight:** 99% of code uses GC and never touches the borrow checker. Only when you optimize or do low-level work do you need `wild` pointers, and THAT'S when the borrow checker activates.

### 3. The Pinning Escape Hatch

```aria
// I KNOW this is stable, let me pin it
wild int32@:stable_ptr = #x

// Now I can do things that would fight Rust's borrow checker
// Because I've explicitly told the compiler "this won't move"
```

**This is better than `unsafe`** because:
- It's explicit and localized (`#` operator)
- The type system knows it's pinned
- Still checked for use-after-free
- But allows patterns that need stable addresses

## Design Principles for Aria's Borrow Checker

### Principle 1: Trust the Programmer's Intent
```aria
// If I pin something, I'm telling you it's safe
wild int32@:ptr1 = #x
wild int32@:ptr2 = #x  // Should be allowed - I pinned it!
```

Rust would reject this. Aria says: "You pinned it, you know what you're doing."

### Principle 2: Scope-Based, Not Flow-Based
```aria
func:process = void() {
    wild int8@:data = malloc(1024)
    
    // Borrow checker just ensures:
    // 1. data is freed before scope ends
    // 2. Not used after free
    // 3. Not borrowed mutably while borrowed immutably
    
    free(data)
} // OK - was freed
```

**Don't track every single operation** like Rust. Track:
- Allocation and deallocation
- Active borrows at scope boundaries
- Use-after-free

### Principle 3: Mutable XOR Shared for WILD Only
```aria
// GC memory: multiple mutable references are fine (data race prevention happens elsewhere)
int32:x = 42
Node:n1 = get_node()
Node:n2 = get_node()
n1.next = n2  // Fine, GC tracks everything

// Wild memory: NOW we enforce 1 mutable XOR N immutable
wild int8@:buf = malloc(100)
int8$:r1 = $buf      // Immutable borrow
int8$:r2 = $buf      // OK - multiple immutable
int8$!:w1 = $!buf    // ERROR - can't mutably borrow while immutably borrowed
```

### Principle 4: Explicit is Better Than Implicit
```aria
// Want to do something complex? Be explicit:
wild Node@:node = malloc(sizeof(Node))

// Pin it if you need stable addresses
wild Node@:pinned = #node

// Or use borrow operators when you want checking
Node$:ref = $node

// Or just use it raw if you're doing low-level work
node.value = 42  // Direct access, you're in wild territory
```

## Concrete Differences from Rust

### Self-Referential Structs
**Rust:** Requires `Pin<Box<T>>` and unsafe code
**Aria:** 
```aria
struct SelfRef {
    data: int32,
    ptr: wild int32@,
};

func:create = SelfRef() {
    SelfRef:s = SelfRef(42, nil)
    s.ptr = #s.data  // Pin the field, get stable pointer
    return s
}
```

### Split Borrows
**Rust:** Hard to borrow different struct fields mutably
**Aria:**
```aria
struct Pair {
    a: int32,
    b: int32,
};

func:modify = void() {
    Pair:p = Pair(1, 2)
    // Just works - GC handles it
    wild int32@:ptr_a = #p.a
    wild int32@:ptr_b = #p.b
    // Both pinned independently
}
```

### Graph Structures
**Rust:** Rc<RefCell<T>> everywhere, runtime overhead
**Aria:**
```aria
struct Node {
    value: int32,
    neighbors: Node?[],  // GC handles cycles automatically
};

// Just works. GC finds cycles and collects them.
```

## The Borrow Checker's REAL Job in Aria

### 1. Prevent Use-After-Free (Wild Memory Only)
```aria
wild int8@:ptr = malloc(100)
free(ptr)
ptr[0] = 5  // ERROR: Use after free
```

### 2. Prevent Memory Leaks (Wild Memory Only)
```aria
func:leak = void() {
    wild int8@:ptr = malloc(100)
    // ERROR: ptr not freed before scope ends
}
```

### 3. Prevent Data Races on Wild Memory
```aria
wild Buffer@:buf = malloc(1024)
Buffer$!:w1 = $!buf  // Mutable borrow
Buffer$!:w2 = $!buf  // ERROR: Can't have two mutable borrows
```

### 4. Ensure Pinned Variables Stay Valid
```aria
int32:x = 42
wild int32@:p = #x
x = 100  // ERROR: Can't modify pinned variable
```

**That's it.** Four rules, not hundreds like Rust.

## Mutable Borrow Syntax Consideration

Given this philosophy, here's what makes sense:

```aria
// For GC memory - borrowing is optional, mainly for preventing mutations
int32:x = 42
int32$:ref = $x      // Immutable borrow (prevents x from changing)

// For wild memory - borrowing is safety-critical
wild int8@:buf = malloc(100)
int8$:r1 = $buf      // Immutable borrow
int8$:r2 = $buf      // OK - multiple immutable

// Mutable borrow - only for wild memory where it matters
int8$!:w = $!buf     // Mutable borrow (exclusive access)
```

**Why `$!` for mutable?**
1. Follows Aria's symbol philosophy
2. `!` already means "unwrap/assert/force" in the language
3. `$!` reads as "borrow forcefully" or "borrow exclusively"
4. Type can stay `int8$` (reference type) - mutability is an operation property
5. Shorter than `$mut`

## Implementation Strategy

### Phase 1: Current (Done)
- Basic borrow tracking
- Pin operator `#`
- Immutable borrows with `$`

### Phase 2: Mutable Borrows
- Add `$!` operator for mutable borrows
- Enforce "1 mutable XOR N immutable" for wild memory only
- Error messages that suggest solutions

### Phase 3: Smart Error Messages
```
error: Cannot mutably borrow 'buf' while immutably borrowed
  |
5 | int8$:r1 = $buf
  |            ---- immutable borrow here
6 | int8$!:w = $!buf
  |            ^^^^^ mutable borrow attempted
  |
help: Consider using the `#` operator to pin if you need stable access:
  |
  wild int8@:pinned = #buf
```

### Phase 4: Escape Hatches
- Document when to use `#` instead of fighting the borrow checker
- Provide patterns for common complex scenarios
- Make it EASY to do the right thing

## Success Criteria

A successful borrow checker should:

1. ✅ Catch REAL bugs (use-after-free, double-free, data races)
2. ✅ Allow VALID patterns (self-references, split borrows, graphs)
3. ✅ Have minimal learning curve (understand in < 1 hour)
4. ✅ Provide clear escape hatches (pin operator `#`, not `unsafe`)
5. ✅ Not fight the programmer on correct code
6. ✅ Generate helpful error messages with solutions

## The Bottom Line

**Rust's mistake:** Making the borrow checker enforce ALL memory safety, leading to:
- Complex lifetime annotations everywhere
- Fighting the compiler on valid code
- Resorting to `unsafe` frequently
- Steep learning curve

**Aria's approach:** GC handles most memory safety, borrow checker only enforces:
- Safety of `wild` (opt-in manual memory)
- Pinning guarantees
- Data race prevention on shared mutable state

**Result:** Simple rules, pragmatic escapes, catches real bugs, doesn't fight valid code.
