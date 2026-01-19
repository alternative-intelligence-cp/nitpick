# Wild Pointers and References

**Category**: Memory Model → Safety  
**Concept**: Invalid memory access  
**Philosophy**: Prevent undefined behavior

---

## What are Wild Pointers?

**Wild pointers** are pointers or references that point to invalid memory:

1. **Uninitialized** - Never set to a valid address
2. **Dangling** - Point to freed memory
3. **Out of bounds** - Beyond valid range

---

## Types of Wild Pointers

### 1. Uninitialized Pointers

```aria
// Wrong: Uninitialized
ptr: *i32;
*ptr = 42;  // ❌ Wild pointer! Undefined behavior!

// Right: Initialize
ptr: *i32 = nil;
when ptr != nil then
    *ptr = 42;
end
```

### 2. Dangling Pointers

```aria
// Wrong: Use after free
ptr: *Data = aria_alloc(sizeof(Data));
aria_free(ptr);
ptr.value = 42;  // ❌ Dangling pointer!

// Right: Don't use after free
ptr: *Data = aria_alloc(sizeof(Data));
ptr.value = 42;
aria_free(ptr);
ptr = nil;  // Mark invalid
```

### 3. Out of Bounds

```aria
// Wrong: Beyond array
arr: []i32 = [1, 2, 3];
ptr: *i32 = &arr[10];  // ❌ Beyond array!

// Right: Check bounds
when index < arr.length() then
    ptr: *i32 = &arr[index];
end
```

---

## Why Wild Pointers are Dangerous

### Undefined Behavior

```aria
ptr: *i32;  // Uninitialized
*ptr = 42;  // Could:
            // - Crash
            // - Corrupt memory
            // - Appear to work!
```

### Data Corruption

```aria
// Freed memory reused
ptr1: *Data = aria_alloc(sizeof(Data));
aria_free(ptr1);

ptr2: *Other = aria_alloc(sizeof(Other));
// ptr2 might reuse ptr1's memory!

ptr1.value = 42;  // ❌ Corrupts ptr2's data!
```

### Security Vulnerabilities

```aria
// Wild pointers can be exploited
// To read sensitive data
// Or execute arbitrary code
```

---

## Prevention

### 1. Initialize Pointers

```aria
// Always initialize
ptr: *Data = nil;

when condition then
    ptr = aria_alloc(sizeof(Data));
end

when ptr != nil then
    // Safe to use
end
```

### 2. Null After Free

```aria
ptr: *Data = aria_alloc(sizeof(Data));
aria_free(ptr);
ptr = nil;  // Prevent accidental use
```

### 3. Use References Instead

```aria
// Pointers - can be wild
ptr: *Data = aria_alloc(sizeof(Data));

// References - safer
data: Data = Data::new();
ref: &Data = data;  // Can't be wild
```

---

## Aria's Safety Features

### 1. Borrow Checker

```aria
// Prevents dangling references
fn bad() -> &Data {
    data: Data = Data::new();
    return &data;  // ❌ Compile error!
}
```

### 2. Bounds Checking

```aria
arr: []i32 = [1, 2, 3];
value: i32 = arr[10];  // Runtime error (bounds check)
```

### 3. Null Checking

```aria
ptr: *Data? = get_optional_pointer();

// Must check before use
when ptr != nil then
    ptr.use();
else
    fail "Null pointer";
end
```

---

## Common Causes

### 1. Forgetting to Initialize

```aria
// Wrong
ptr: *i32;
*ptr = 42;

// Right
ptr: *i32 = nil;
```

### 2. Use After Free

```aria
// Wrong
aria_free(ptr);
ptr.use();

// Right
aria_free(ptr);
ptr = nil;
```

### 3. Returning Stack Addresses

```aria
// Wrong
fn bad() -> *i32 {
    value: i32 = 42;
    return &value;  // ❌ Dangling!
}

// Right
fn good() -> *i32 {
    return aria_alloc(sizeof(i32));
}
```

### 4. Iterator Invalidation

```aria
// Wrong
arr: []i32 = [1, 2, 3];
ptr: *i32 = &arr[0];
arr.push(4);  // May reallocate!
*ptr = 99;    // ❌ Might be wild!

// Right
arr: []i32 = [1, 2, 3];
arr.push(4);
arr[0] = 99;  // Safe
```

---

## Best Practices

### ✅ DO: Initialize Pointers

```aria
ptr: *Data = nil;
```

### ✅ DO: Use References

```aria
// Prefer references
ref: &Data = data;

// Over pointers
ptr: *Data = &data;
```

### ✅ DO: Null After Free

```aria
aria_free(ptr);
ptr = nil;
```

### ✅ DO: Validate Pointers

```aria
when ptr != nil and ptr.is_valid() then
    ptr.use();
end
```

### ❌ DON'T: Use Uninitialized

```aria
// Wrong
ptr: *i32;
*ptr = 42;
```

### ❌ DON'T: Use After Free

```aria
// Wrong
aria_free(ptr);
ptr.use();
```

---

## Detection Tools

### 1. Sanitizers

```bash
# Compile with sanitizers
aria build --sanitize=address
aria build --sanitize=memory
```

### 2. Static Analysis

```bash
# Run static analyzer
aria check --strict
```

### 3. Runtime Checks

```aria
// Enable runtime pointer validation
#[runtime_checks(pointers)]
```

---

## Examples

### Uninitialized

```aria
// ❌ Wrong
fn bad() {
    ptr: *i32;
    stdout << *ptr;  // Wild pointer!
}

// ✅ Right
fn good() {
    ptr: *i32 = nil;
    when ptr != nil then
        stdout << *ptr;
    end
}
```

### Dangling

```aria
// ❌ Wrong
fn bad() {
    ptr: *Data = aria_alloc(sizeof(Data));
    aria_free(ptr);
    return ptr;  // Dangling!
}

// ✅ Right
fn good() -> *Data {
    ptr: *Data = aria_alloc(sizeof(Data));
    return ptr;  // Caller must free
}
```

### Out of Bounds

```aria
// ❌ Wrong
arr: []i32 = [1, 2, 3];
ptr: *i32 = &arr[100];  // Beyond array!

// ✅ Right
when index < arr.length() then
    ptr: *i32 = &arr[index];
end
```

---

## Related

- [wildx](wildx.md) - Extended wild pointer concepts
- [pointer_syntax](pointer_syntax.md) - Pointer types
- [borrowing](borrowing.md) - Safe references

---

**Remember**: Wild pointers cause **undefined behavior** - always initialize and validate!
