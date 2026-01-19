# Allocators

**Category**: Memory Model → Allocation  
**Concept**: Memory allocation strategies  
**Philosophy**: Choose the right tool for the job

---

## What are Allocators?

**Allocators** are systems for obtaining and releasing memory. Aria provides multiple allocators for different use cases.

---

## Types of Allocators

### 1. Stack Allocator (Default)

**Automatic, fast, limited:**

```aria
fn example() {
    value: i32 = 42;     // Stack allocated
    array: [i32; 10];    // Stack allocated
}  // Automatically freed
```

**When to use:**
- ✅ Small, temporary data
- ✅ Known size at compile time
- ✅ Short lifetime

### 2. Heap Allocator (Manual)

**Unlimited, slower, manual:**

```aria
// Allocate
ptr: *i32 = aria_alloc(sizeof(i32));
*ptr = 42;

// Must free
aria_free(ptr);
```

**When to use:**
- ✅ Large data
- ✅ Unknown size
- ✅ Long lifetime
- ✅ Needs to outlive scope

### 3. Garbage Collector (GC)

**Automatic cleanup, some overhead:**

```aria
// Allocate with GC
data: Data = aria_gc_alloc(Data);

// GC automatically frees when no references
```

**When to use:**
- ✅ Complex object graphs
- ✅ Don't want manual management
- ✅ Can accept GC pauses

---

## Allocation Functions

### aria_alloc

Allocate raw memory:

```aria
// Allocate bytes
ptr: *byte = aria_alloc(1024);  // 1KB

// Allocate typed
ptr: *i32 = aria_alloc(sizeof(i32));

// Must free
aria_free(ptr);
```

### aria_alloc_array

Allocate array:

```aria
// Allocate array of 100 i32s
arr: []i32 = aria_alloc_array(i32, 100);

// Use array
arr[0] = 42;

// Must free
aria_free(arr);
```

### aria_alloc_string

Allocate string:

```aria
// Allocate string
str: string = aria_alloc_string("Hello");

// Must free
aria_free(str);
```

### aria_alloc_buffer

Allocate buffer:

```aria
// Allocate buffer
buf: []byte = aria_alloc_buffer(1024);

// Use buffer
buf[0] = 0xFF;

// Must free
aria_free(buf);
```

### aria_gc_alloc

GC-managed allocation:

```aria
// Allocate with GC
data: Data = aria_gc_alloc(Data);

// No manual free needed
// GC collects when no references
```

### aria_free

Free allocated memory:

```aria
ptr: *i32 = aria_alloc(sizeof(i32));

// Use ptr...

aria_free(ptr);  // Release memory
```

---

## Choosing an Allocator

### Use Stack When:

```aria
// ✅ Small data
value: i32 = 42;

// ✅ Fixed size
buffer: [byte; 256];

// ✅ Short-lived
{
    temp: Data = Data::new();
}  // Freed automatically
```

### Use Heap When:

```aria
// ✅ Large data
large: []byte = aria_alloc_buffer(10000000);

// ✅ Dynamic size
size: i32 = user_input();
buffer: []byte = aria_alloc_buffer(size);

// ✅ Long-lived
global_cache: *Cache = aria_alloc(sizeof(Cache));
```

### Use GC When:

```aria
// ✅ Complex graphs
node: Node = aria_gc_alloc(Node);
node.left = aria_gc_alloc(Node);
node.right = aria_gc_alloc(Node);

// ✅ Shared ownership
data: Data = aria_gc_alloc(Data);
ref1: &Data = data;
ref2: &Data = data;
// Freed when all refs gone
```

---

## Custom Allocators

Can define custom allocation strategies:

```aria
struct PoolAllocator {
    pool: *byte,
    used: i32,
    capacity: i32
}

impl PoolAllocator {
    fn alloc(size: i32) -> *byte {
        when self.used + size > self.capacity then
            fail "Pool exhausted";
        end
        
        ptr: *byte = self.pool + self.used;
        self.used = self.used + size;
        return ptr;
    }
    
    fn reset() {
        self.used = 0;  // Reuse entire pool
    }
}
```

---

## Memory Safety

### Stack - Automatic

```aria
{
    value: i32 = 42;
}  // ✅ Automatically freed
```

### Heap - Manual (Careful!)

```aria
ptr: *i32 = aria_alloc(sizeof(i32));

aria_free(ptr);  // ✅ Must remember!

// ❌ Memory leak if forgotten
// ❌ Double free if called twice
// ❌ Use after free if accessed after
```

### GC - Automatic

```aria
data: Data = aria_gc_alloc(Data);

// ✅ Automatically freed when no references
```

---

## Best Practices

### ✅ DO: Use Stack by Default

```aria
// Good: Stack allocation
value: i32 = 42;
buffer: [byte; 256];
```

### ✅ DO: Use defer for Cleanup

```aria
// Good: Guaranteed cleanup
ptr: *byte = aria_alloc(1024);
defer aria_free(ptr);

// Use ptr...
```

### ✅ DO: Use RAII Wrappers

```aria
// Good: Automatic cleanup
vec: Vec<i32> = Vec::new();  // Manages allocation internally
// Freed automatically
```

### ❌ DON'T: Leak Memory

```aria
// Wrong: Memory leak
fn bad() {
    ptr: *i32 = aria_alloc(sizeof(i32));
    // Never freed!
}

// Right: Always free
fn good() {
    ptr: *i32 = aria_alloc(sizeof(i32));
    defer aria_free(ptr);
}
```

### ❌ DON'T: Double Free

```aria
// Wrong: Double free
ptr: *i32 = aria_alloc(sizeof(i32));
aria_free(ptr);
aria_free(ptr);  // ❌ Crash!

// Right: Free once
ptr: *i32 = aria_alloc(sizeof(i32));
aria_free(ptr);
ptr = nil;  // Prevent accidental reuse
```

---

## Real-World Examples

### Dynamic Array

```aria
fn create_array(size: i32) -> []i32 {
    arr: []i32 = aria_alloc_array(i32, size);
    
    for i in 0..size {
        arr[i] = i;
    }
    
    return arr;
}

// Caller must free
arr: []i32 = create_array(100);
defer aria_free(arr);
```

### String Builder

```aria
struct StringBuilder {
    buffer: *byte,
    length: i32,
    capacity: i32
}

impl StringBuilder {
    fn new() -> StringBuilder {
        capacity: i32 = 256;
        return StringBuilder{
            buffer: aria_alloc_buffer(capacity),
            length: 0,
            capacity: capacity
        };
    }
    
    fn append(text: string) {
        // Grow if needed
        when self.length + text.length() > self.capacity then
            self.grow();
        end
        
        // Copy text
        // ...
    }
}

impl Drop for StringBuilder {
    fn drop() {
        aria_free(self.buffer);
    }
}
```

### Object Pool

```aria
struct ObjectPool<T> {
    objects: []T,
    available: []bool
}

impl<T> ObjectPool<T> {
    fn new(capacity: i32) -> ObjectPool<T> {
        return ObjectPool{
            objects: aria_alloc_array(T, capacity),
            available: aria_alloc_array(bool, capacity)
        };
    }
    
    fn acquire() -> &T? {
        for i in 0..self.objects.length() {
            when self.available[i] then
                self.available[i] = false;
                return &self.objects[i];
            end
        }
        return nil;
    }
    
    fn release(obj: &T) {
        // Mark as available
    }
}
```

---

## Related Topics

- [aria_alloc](aria_alloc.md) - Basic allocation
- [aria_free](aria_free.md) - Freeing memory
- [aria_gc_alloc](aria_gc_alloc.md) - GC allocation
- [Stack](stack.md) - Stack allocation
- [RAII](raii.md) - Automatic cleanup

---

**Remember**: Stack = **automatic**, Heap = **manual**, GC = **automatic with overhead** - choose based on needs!
