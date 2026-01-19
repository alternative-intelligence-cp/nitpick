# aria_gc_alloc

**Category**: Memory Model → Allocation Functions  
**Function**: `aria_gc_alloc(T) -> T`  
**Purpose**: Garbage-collected allocation

---

## Signature

```aria
fn aria_gc_alloc<T>() -> T
```

---

## Description

Allocates memory for type `T` that will be automatically freed by the garbage collector when no references exist.

---

## Parameters

- `T`: Type to allocate

---

## Returns

Instance of type `T`, managed by GC.

---

## Basic Usage

```aria
// Allocate with GC
data: Data = aria_gc_alloc(Data);

// Use data
data.value = 42;

// No manual free needed!
// GC collects when no references
```

---

## Advantages

### No Manual Free

```aria
// Heap allocation
ptr: *Data = aria_alloc(sizeof(Data));
defer aria_free(ptr);  // Must remember!

// GC allocation
data: Data = aria_gc_alloc(Data);
// Automatically freed!
```

### Shared Ownership

```aria
// Multiple references
data: Data = aria_gc_alloc(Data);

ref1: &Data = data;
ref2: &Data = data;
ref3: &Data = data;

// Freed when ALL references gone
```

### Complex Graphs

```aria
// Tree structure
root: Node = aria_gc_alloc(Node);
root.left = aria_gc_alloc(Node);
root.right = aria_gc_alloc(Node);

// All freed automatically
```

---

## Disadvantages

### Performance Overhead

```aria
// GC pauses for collection
// Non-deterministic cleanup time
```

### Memory Pressure

```aria
// May use more memory than manual
// Delayed collection
```

---

## When to Use

### ✅ Use GC When:

- Complex object graphs
- Shared ownership
- Don't want manual management
- Can accept GC pauses

### ❌ Don't Use GC When:

- Real-time requirements
- Tight memory constraints
- Predictable cleanup needed
- Performance-critical code

---

## Best Practices

### ✅ DO: Use for Complex Graphs

```aria
// Good: Tree/graph structures
tree: Tree = aria_gc_alloc(Tree);
tree.build_complex_structure();
```

### ✅ DO: Use for Shared Data

```aria
// Good: Multiple owners
shared: SharedData = aria_gc_alloc(SharedData);
module_a.use(shared);
module_b.use(shared);
```

### ❌ DON'T: Mix with Manual

```aria
// Wrong: Confusing
data: Data = aria_gc_alloc(Data);
aria_free(data);  // ❌ Don't manually free GC data!
```

### ❌ DON'T: Use for Everything

```aria
// Wrong: Unnecessary overhead
fn process() {
    value: i32 = aria_gc_alloc(i32);  // Overkill!
}

// Right: Use stack
fn process() {
    value: i32 = 42;
}
```

---

## Examples

### Linked List

```aria
struct Node {
    value: i32,
    next: Node?
}

head: Node = aria_gc_alloc(Node);
head.value = 1;
head.next = aria_gc_alloc(Node);
head.next.value = 2;

// All freed automatically
```

### Object Cache

```aria
cache: Map<string, Data> = Map::new();

for key in keys {
    data: Data = aria_gc_alloc(Data);
    data.load(key);
    cache.insert(key, data);
}

// Cache items freed when removed
```

---

## Related

- [gc](gc.md) - Garbage collection overview
- [Allocators](allocators.md) - Allocation strategies
- [aria_alloc](aria_alloc.md) - Manual allocation

---

**Remember**: GC = **automatic cleanup**, but with **performance overhead**!
