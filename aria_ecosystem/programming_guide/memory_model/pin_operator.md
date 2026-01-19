# Pin Operator

**Category**: Memory Model → Operators  
**Operator**: `pin` / `unpin`  
**Purpose**: Control memory movement

---

## Syntax

```aria
pin <expression>;
unpin <expression>;
```

---

## Description

The `pin` operator prevents an object from being moved in memory by the garbage collector.

---

## Basic Usage

```aria
// Allocate with GC
data: Data = aria_gc_alloc(Data);

// Pin in place
pin data;

// Get stable pointer
ptr: *Data = &data;

// Unpin
unpin data;
```

---

## With defer

```aria
data: Data = aria_gc_alloc(Data);

pin data;
defer unpin data;  // Auto-unpin

// Use data...
```

---

## Examples

### C Interop

```aria
extern fn c_sort(arr: *i32, len: i32);

arr: []i32 = aria_gc_alloc_array(i32, 100);

pin arr;
defer unpin arr;

c_sort(&arr[0], arr.length());
```

### Async I/O

```aria
async fn save(data: Data) {
    pin data;
    defer unpin data;
    
    await file.write(&data);
}
```

---

## Best Practices

### ✅ DO: Use defer

```aria
pin data;
defer unpin data;
```

### ✅ DO: Pin Briefly

```aria
// Good: Short duration
pin data;
c_call(&data);
unpin data;

// Wrong: Forever pinned
pin data;
// Never unpinned!
```

### ❌ DON'T: Pin Stack Values

```aria
// Wrong
value: i32 = 42;
pin value;  // Unnecessary!
```

---

## Related

- [pinning](pinning.md) - Pinning concept
- [gc](gc.md) - Garbage collection
- [address_operator](address_operator.md) - Address operator

---

**Remember**: Always `unpin` what you `pin`!
