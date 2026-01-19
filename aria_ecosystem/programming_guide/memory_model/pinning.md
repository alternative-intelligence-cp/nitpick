# Pinning

**Category**: Memory Model → Advanced  
**Concept**: Prevent memory movement  
**Philosophy**: Control when needed

---

## What is Pinning?

**Pinning** prevents the garbage collector or memory manager from moving an object in memory.

---

## Why Pin?

### GC May Move Objects

```aria
// With GC, objects can move
data: Data = aria_gc_alloc(Data);
ptr: *Data = &data;  // Get address

// GC collection happens
gc_collect();

// ptr might now be INVALID!
// Object may have moved
```

### Pinning Prevents Movement

```aria
// Pin object
data: Data = aria_gc_alloc(Data);
pin data;

ptr: *Data = &data;  // Safe - won't move

// Even after GC
gc_collect();
// ptr still valid!

unpin data;  // Allow movement again
```

---

## When to Pin

### ✅ Pin When:

**1. Passing to C/Foreign Code**

```aria
// C function expects stable pointer
extern fn c_process(data: *Data);

data: Data = aria_gc_alloc(Data);
pin data;
c_process(&data);
unpin data;
```

**2. Long-Lived Raw Pointers**

```aria
// Keep pointer for extended time
data: Data = aria_gc_alloc(Data);
pin data;

ptr: *Data = &data;
// Use ptr over time
// ptr remains valid

unpin data;
```

**3. Hardware Access**

```aria
// DMA needs stable address
buffer: []byte = aria_gc_alloc(buffer_size);
pin buffer;

dma_start(&buffer);
// buffer won't move during DMA

unpin buffer;
```

---

## Pinning Syntax

### Pin Statement

```aria
data: Data = aria_gc_alloc(Data);

pin data;
// data is pinned

unpin data;
// data can move again
```

### Pin with defer

```aria
data: Data = aria_gc_alloc(Data);
pin data;
defer unpin data;  // Auto-unpin

// Use data...
// Automatic unpin on scope exit
```

---

## Costs of Pinning

### 1. Reduces GC Efficiency

```aria
// Pinned objects can't be compacted
// May lead to fragmentation
```

### 2. Memory Pressure

```aria
// Pinned objects stay in place
// Can block memory optimization
```

---

## Best Practices

### ✅ DO: Pin Minimally

```aria
// Good: Pin only when needed
pin data;
c_function(&data);
unpin data;

// Wrong: Pin forever
pin data;
// Never unpinned - bad!
```

### ✅ DO: Use defer

```aria
pin data;
defer unpin data;  // Can't forget

foreign_call(&data);
```

### ✅ DO: Prefer Borrowing

```aria
// Better: Use references
fn process(data: &Data) {
    // No pinning needed
}

// Only pin for raw pointers
fn process_raw(data: *Data) {
    // Needs pinning
}
```

### ❌ DON'T: Pin Stack Values

```aria
// Wrong: Stack values never move
value: i32 = 42;
pin value;  // ❌ Unnecessary!
```

### ❌ DON'T: Pin Everything

```aria
// Wrong: Defeats GC purpose
all_data: []Data = load_all();
for item in all_data {
    pin item;  // ❌ Too much!
}
```

---

## Examples

### C Interop

```aria
extern fn c_process(buffer: *byte, size: i32);

fn call_c() {
    buffer: []byte = aria_gc_alloc_buffer(1024);
    pin buffer;
    defer unpin buffer;
    
    c_process(&buffer[0], buffer.length());
}
```

### Async Operation

```aria
async fn write_async(data: Data) {
    pin data;
    defer unpin data;
    
    await disk.write(&data);
}
```

### Hardware DMA

```aria
fn dma_transfer(data: []byte) {
    pin data;
    defer unpin data;
    
    dma_controller.start(&data[0], data.length());
    await dma_controller.wait();
}
```

---

## Alternative: Stack Allocation

Instead of pinning GC data:

```aria
// GC + pin
data: Data = aria_gc_alloc(Data);
pin data;
c_function(&data);
unpin data;

// Better: Stack allocation
data: Data = Data::new();
c_function(&data);  // Already stable
```

---

## Related

- [pin_operator](pin_operator.md) - Pin operator syntax
- [gc](gc.md) - Garbage collection
- [address_operator](address_operator.md) - Getting addresses

---

**Remember**: Pin **only when necessary** - it limits GC efficiency!
