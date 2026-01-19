# aria_alloc

**Category**: Memory Model → Allocation Functions  
**Function**: `aria_alloc(size: i32) -> *byte`  
**Purpose**: Allocate raw memory

---

## Signature

```aria
fn aria_alloc(size: i32) -> *byte
```

---

## Description

Allocates `size` bytes of uninitialized memory on the heap.

---

## Parameters

- `size`: Number of bytes to allocate

---

## Returns

Pointer to allocated memory, or `nil` if allocation fails.

---

## Basic Usage

```aria
// Allocate 1024 bytes
ptr: *byte = aria_alloc(1024);

when ptr == nil then
    fail "Allocation failed";
end

// Use memory
ptr[0] = 0xFF;

// Must free
aria_free(ptr);
```

---

## Allocating Typed Data

```aria
// Allocate for i32
ptr: *i32 = aria_alloc(sizeof(i32)) as *i32;
*ptr = 42;

aria_free(ptr);
```

---

## Best Practices

### ✅ DO: Always Free

```aria
ptr: *byte = aria_alloc(1024);
defer aria_free(ptr);  // Guaranteed cleanup
```

### ✅ DO: Check for Null

```aria
ptr: *byte = aria_alloc(size);

when ptr == nil then
    fail "Out of memory";
end
```

### ❌ DON'T: Leak Memory

```aria
// Wrong
ptr: *byte = aria_alloc(1024);
// Never freed!

// Right
ptr: *byte = aria_alloc(1024);
defer aria_free(ptr);
```

---

## Related

- [aria_free](aria_free.md) - Free memory
- [aria_alloc_array](aria_alloc_array.md) - Allocate arrays
- [Allocators](allocators.md) - Allocation overview

---

**Remember**: Must **always free** with `aria_free`!
