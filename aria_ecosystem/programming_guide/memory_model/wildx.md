# Extended Wild Pointer Concepts

**Category**: Memory Model → Advanced Safety  
**Concept**: Advanced invalid pointer scenarios  
**Philosophy**: Deep understanding prevents bugs

---

## Advanced Wild Pointer Scenarios

Beyond basic uninitialized, dangling, and out-of-bounds pointers, there are more subtle ways pointers become wild.

---

## 1. Temporal Aliasing

### The Problem

```aria
// First allocation
ptr1: *Data = aria_alloc(sizeof(Data));
ptr1.id = 1;

// Save address
addr: i64 = ptr1 as i64;

// Free
aria_free(ptr1);

// Second allocation (might reuse memory!)
ptr2: *Other = aria_alloc(sizeof(Other));

// Convert back to original type
ptr3: *Data = addr as *Data;

// ptr3 now aliases ptr2!
ptr3.id = 99;  // ❌ Corrupts ptr2!
```

### The Fix

```aria
// Don't save raw addresses
// Use proper references/pointers
```

---

## 2. Type Confusion

### The Problem

```aria
// Allocate as one type
ptr1: *i32 = aria_alloc(sizeof(i32));
*ptr1 = 0x12345678;

// Cast to different type
ptr2: *byte = ptr1 as *byte;
ptr2[0] = 0xFF;  // Modifies part of i32!

// ptr1 now has unexpected value
stdout << *ptr1;  // Corrupted!
```

### The Fix

```aria
// Avoid type punning
// Use proper unions or conversions
```

---

## 3. Alignment Issues

### The Problem

```aria
// Allocate byte array
buffer: []byte = aria_alloc_buffer(100);

// Misaligned cast
ptr: *i64 = (&buffer[1]) as *i64;  // ❌ Misaligned!

// Accessing misaligned pointer
*ptr = 123;  // May crash on some platforms!
```

### The Fix

```aria
// Ensure proper alignment
ptr: *i64 = (&buffer[0]) as *i64;  // Aligned
```

---

## 4. Double Free

### The Problem

```aria
ptr1: *Data = aria_alloc(sizeof(Data));
ptr2: *Data = ptr1;  // Alias

aria_free(ptr1);
aria_free(ptr2);  // ❌ Double free!
```

### The Fix

```aria
ptr1: *Data = aria_alloc(sizeof(Data));
ptr2: *Data = ptr1;

aria_free(ptr1);
ptr1 = nil;
ptr2 = nil;  // Clear both
```

---

## 5. Shared Mutable State

### The Problem

```aria
// Shared pointer
global: *Data = aria_alloc(sizeof(Data));

// Thread 1
fn thread1() {
    aria_free(global);
    global = nil;
}

// Thread 2
fn thread2() {
    global.use();  // ❌ Might be freed!
}
```

### The Fix

```aria
// Use synchronization
mutex: Mutex = Mutex::new();

fn thread1() {
    lock mutex;
    defer unlock mutex;
    
    aria_free(global);
    global = nil;
}

fn thread2() {
    lock mutex;
    defer unlock mutex;
    
    when global != nil then
        global.use();
    end
}
```

---

## 6. Recursive Data Structures

### The Problem

```aria
// Linked list
struct Node {
    value: i32,
    next: *Node
}

head: *Node = aria_alloc(sizeof(Node));
head.next = head;  // Self-reference!

aria_free(head);
// head.next is now wild, but still accessible if we had another reference!
```

### The Fix

```aria
// Clear references before freeing
head.next = nil;
aria_free(head);
head = nil;
```

---

## 7. Buffer Reallocation

### The Problem

```aria
// Growable buffer
buffer: []byte = aria_alloc_buffer(10);
ptr: *byte = &buffer[5];

// Resize (reallocates!)
buffer = resize(buffer, 100);

// ptr now wild!
*ptr = 0xFF;  // ❌ Points to freed memory!
```

### The Fix

```aria
// Recalculate pointer after reallocation
buffer: []byte = aria_alloc_buffer(10);
index: i32 = 5;

buffer = resize(buffer, 100);
ptr: *byte = &buffer[index];  // Recalculate

*ptr = 0xFF;  // Safe
```

---

## 8. Callback Hell

### The Problem

```aria
// Register callback with pointer
data: *Data = aria_alloc(sizeof(Data));
register_callback(fn() {
    data.use();  // ❌ Might be freed!
});

// Free data
aria_free(data);

// Later: callback fires
// Uses wild pointer!
```

### The Fix

```aria
// Use weak references or validation
data: *Data = aria_alloc(sizeof(Data));
data_id: i64 = get_unique_id(data);

register_callback(fn() {
    when validate_id(data_id) then
        data.use();
    end
});
```

---

## 9. Stack Return

### The Problem

```aria
fn get_ptr() -> *i32 {
    value: i32 = 42;
    return &value;  // ❌ Returns stack address!
}

ptr: *i32 = get_ptr();
*ptr = 99;  // Wild! Stack frame gone!
```

### The Fix

```aria
fn get_ptr() -> *i32 {
    return aria_alloc(sizeof(i32));  // Heap allocated
}

// Or return by value
fn get_value() -> i32 {
    return 42;
}
```

---

## 10. Collection Invalidation

### The Problem

```aria
arr: []i32 = [1, 2, 3];
ptr: *i32 = &arr[0];

// Modify collection
arr.insert(0, 99);  // May reallocate!

// ptr is now wild
*ptr = 42;  // ❌ Points to old memory!
```

### The Fix

```aria
arr: []i32 = [1, 2, 3];
index: i32 = 0;

arr.insert(0, 99);

// Recalculate pointer
ptr: *i32 = &arr[index];
*ptr = 42;  // Safe
```

---

## Detection Strategies

### 1. Sanitizers

```bash
# Address sanitizer
aria build --sanitize=address

# Memory sanitizer
aria build --sanitize=memory

# Undefined behavior sanitizer
aria build --sanitize=undefined
```

### 2. Static Analysis

```bash
aria check --strict --wild-pointers
```

### 3. Runtime Validation

```aria
#[debug_pointers]
fn risky() {
    // Pointers validated at runtime
}
```

---

## Best Practices

### ✅ DO: Prefer References

```aria
// References are safer
ref: &Data = data;
```

### ✅ DO: Use Smart Pointers

```aria
// Smart pointer tracks lifetime
smart: Rc<Data> = Rc::new(Data::new());
```

### ✅ DO: Validate Before Use

```aria
when ptr != nil and is_valid(ptr) then
    ptr.use();
end
```

### ❌ DON'T: Save Raw Addresses

```aria
// Wrong
addr: i64 = ptr as i64;

// Right
// Use proper pointer types
```

### ❌ DON'T: Type Pun

```aria
// Wrong
int_ptr: *i32 = ...;
byte_ptr: *byte = int_ptr as *byte;

// Right
// Use proper conversions
```

---

## Summary

Wild pointers can occur through:

1. **Temporal aliasing** - Memory reused
2. **Type confusion** - Wrong type casts
3. **Alignment issues** - Misaligned access
4. **Double free** - Freeing twice
5. **Shared mutable state** - Concurrent access
6. **Recursive structures** - Self-references
7. **Buffer reallocation** - Growth invalidates pointers
8. **Callbacks** - Outlive data
9. **Stack return** - Returning local addresses
10. **Collection invalidation** - Modification invalidates iterators

---

## Related

- [wild](wild.md) - Basic wild pointer concepts
- [borrowing](borrowing.md) - Safe references
- [pointer_syntax](pointer_syntax.md) - Pointer types

---

**Remember**: Wild pointers are **subtle** - understand all scenarios to avoid them!
