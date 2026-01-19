# aria_free

**Category**: Memory Model → Allocation Functions  
**Function**: `aria_free(ptr: *byte)`  
**Purpose**: Free allocated memory

---

## Signature

```aria
fn aria_free(ptr: *byte)
```

---

## Description

Frees memory previously allocated with `aria_alloc`, `aria_alloc_array`, `aria_alloc_string`, or `aria_alloc_buffer`.

---

## Parameters

- `ptr`: Pointer to memory to free (can be `nil`)

---

## Basic Usage

```aria
// Allocate
ptr: *byte = aria_alloc(1024);

// Use memory...

// Free
aria_free(ptr);
```

---

## With defer

```aria
fn process() {
    ptr: *byte = aria_alloc(1024);
    defer aria_free(ptr);  // Guaranteed to run
    
    // Use ptr...
    // Free happens automatically
}
```

---

## Safety Rules

### ✅ Safe

```aria
// Free once
ptr: *byte = aria_alloc(1024);
aria_free(ptr);

// Free nil is safe
ptr: *byte = nil;
aria_free(ptr);  // OK, does nothing
```

### ❌ Unsafe

```aria
// Double free - CRASH!
ptr: *byte = aria_alloc(1024);
aria_free(ptr);
aria_free(ptr);  // ❌ Error!

// Use after free - UNDEFINED!
ptr: *byte = aria_alloc(1024);
aria_free(ptr);
ptr[0] = 0xFF;  // ❌ Undefined behavior!
```

---

## Best Practices

### ✅ DO: Free Exactly Once

```aria
ptr: *byte = aria_alloc(1024);
aria_free(ptr);
ptr = nil;  // Prevent accidental reuse
```

### ✅ DO: Use defer

```aria
ptr: *byte = aria_alloc(1024);
defer aria_free(ptr);  // Can't forget
```

### ❌ DON'T: Free Twice

```aria
// Wrong
aria_free(ptr);
aria_free(ptr);  // Crash!
```

---

## Related

- [aria_alloc](aria_alloc.md) - Allocate memory
- [Allocators](allocators.md) - Allocation overview
- [defer](defer.md) - Automatic cleanup

---

**Remember**: Every `aria_alloc` needs exactly **one** `aria_free`!
