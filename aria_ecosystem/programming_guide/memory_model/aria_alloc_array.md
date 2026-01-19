# aria_alloc_array

**Category**: Memory Model → Allocation Functions  
**Function**: `aria_alloc_array(T, count: i32) -> []T`  
**Purpose**: Allocate typed array

---

## Signature

```aria
fn aria_alloc_array<T>(count: i32) -> []T
```

---

## Description

Allocates an array of `count` elements of type `T` on the heap.

---

## Parameters

- `T`: Element type
- `count`: Number of elements

---

## Returns

Array of type `[]T`, or `nil` if allocation fails.

---

## Basic Usage

```aria
// Allocate array of 100 integers
arr: []i32 = aria_alloc_array(i32, 100);

when arr == nil then
    fail "Allocation failed";
end

// Use array
arr[0] = 42;
arr[99] = 100;

// Must free
aria_free(arr);
```

---

## Different Types

```aria
// Integers
numbers: []i32 = aria_alloc_array(i32, 50);

// Floats
values: []f64 = aria_alloc_array(f64, 100);

// Structs
users: []User = aria_alloc_array(User, 10);

// Free all
defer aria_free(numbers);
defer aria_free(values);
defer aria_free(users);
```

---

## Initialize After Allocation

```aria
arr: []i32 = aria_alloc_array(i32, 10);
defer aria_free(arr);

for i in 0..arr.length() {
    arr[i] = i * i;  // 0, 1, 4, 9, ...
}
```

---

## Best Practices

### ✅ DO: Use defer

```aria
arr: []i32 = aria_alloc_array(i32, 100);
defer aria_free(arr);
```

### ✅ DO: Check for Null

```aria
arr: []i32 = aria_alloc_array(i32, size);

when arr == nil then
    fail "Out of memory";
end
```

### ❌ DON'T: Forget to Free

```aria
// Wrong: Memory leak
arr: []i32 = aria_alloc_array(i32, 100);
// Never freed!
```

---

## Related

- [aria_alloc](aria_alloc.md) - Raw allocation
- [aria_free](aria_free.md) - Free memory
- [Allocators](allocators.md) - Allocation overview

---

**Remember**: Always **free** allocated arrays!
