# Garbage Collection (GC)

**Category**: Memory Model → Allocation  
**Concept**: Automatic memory management  
**Philosophy**: Convenience with tradeoffs

---

## What is GC?

**Garbage Collection** automatically reclaims memory that is no longer referenced, eliminating manual `free` calls.

---

## How It Works

```aria
// Allocate with GC
data: Data = aria_gc_alloc(Data);

// Use data
data.value = 42;

// When data goes out of scope and no references exist
// GC automatically frees it
```

---

## Advantages

### 1. No Manual Free

```aria
// Manual allocation
ptr: *Data = aria_alloc(sizeof(Data));
defer aria_free(ptr);  // Must remember!

// GC allocation
data: Data = aria_gc_alloc(Data);
// Automatic!
```

### 2. No Dangling Pointers

```aria
// Manual - possible dangling pointer
fn bad() -> *Data {
    data: *Data = aria_alloc(sizeof(Data));
    aria_free(data);
    return data;  // Dangling!
}

// GC - safe
fn good() -> Data {
    data: Data = aria_gc_alloc(Data);
    return data;  // Valid until no references
}
```

### 3. Simplifies Complex Ownership

```aria
// Multiple references - GC handles it
shared: Data = aria_gc_alloc(Data);

ref1: Data = shared;
ref2: Data = shared;
ref3: Data = shared;

// Freed when all references gone
```

---

## Disadvantages

### 1. Performance Overhead

```aria
// GC must:
// - Track references
// - Scan memory
// - Pause to collect

// This adds overhead
```

### 2. Non-Deterministic

```aria
// Don't know WHEN collection happens
data: Data = aria_gc_alloc(Data);

// Could be freed anytime after last use
// Not predictable
```

### 3. Memory Pressure

```aria
// GC may delay collection
// Can use more memory than manual
```

---

## GC Strategies

### Mark and Sweep

1. Mark all reachable objects
2. Sweep and free unmarked objects

### Generational

- Young generation (frequent collection)
- Old generation (infrequent collection)

### Reference Counting

- Count references to each object
- Free when count reaches zero

---

## When to Use GC

### ✅ Use When:

```aria
// Complex object graphs
tree: Tree = aria_gc_alloc(Tree);

// Shared ownership
cache: Map<string, Data> = Map::new();
for key in keys {
    cache.insert(key, aria_gc_alloc(Data));
}

// Don't want manual management
objects: []Object = [];
for i in 0..100 {
    objects.push(aria_gc_alloc(Object));
}
```

### ❌ Don't Use When:

```aria
// Real-time systems
// Can't afford GC pauses

// Tight memory constraints
// GC uses extra memory

// Performance-critical
// Manual allocation faster
```

---

## GC Best Practices

### ✅ DO: Nullify References

```aria
// Help GC by clearing references
data: Data? = aria_gc_alloc(Data);

// When done
data = nil;  // Allow GC to collect
```

### ✅ DO: Limit Long-Lived Objects

```aria
// Good: Short-lived
fn process() {
    temp: Data = aria_gc_alloc(Data);
    // temp collected soon
}

// Avoid: Long-lived if possible
global: Data = aria_gc_alloc(Data);  // Lives forever
```

### ❌ DON'T: Create Excessive Garbage

```aria
// Wrong: Allocates in hot loop
for i in 0..1000000 {
    temp: Data = aria_gc_alloc(Data);  // Creates garbage!
}

// Right: Reuse or use stack
temp: Data = Data::new();  // Stack allocated
for i in 0..1000000 {
    temp.reset();  // Reuse same object
}
```

---

## Mixing Allocators

```aria
// Stack for short-lived
value: i32 = 42;

// Heap for large/long-lived
buffer: []byte = aria_alloc_buffer(1000000);
defer aria_free(buffer);

// GC for complex graphs
tree: Tree = aria_gc_alloc(Tree);
```

---

## Manual Collection

Force collection (if supported):

```aria
// Trigger GC collection
gc_collect();

// Or suggest collection
gc_suggest_collect();
```

---

## Related Topics

- [aria_gc_alloc](aria_gc_alloc.md) - GC allocation
- [Allocators](allocators.md) - Allocation strategies
- [RAII](raii.md) - Deterministic cleanup

---

**Remember**: GC = **convenient** but has **performance cost** - use appropriately!
