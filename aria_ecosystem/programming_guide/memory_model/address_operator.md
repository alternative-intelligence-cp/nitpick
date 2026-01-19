# Address Operator

**Category**: Memory Model → Operators  
**Symbol**: `&`  
**Purpose**: Get memory address

---

## What is the Address Operator?

The **address operator** `&` gets the **memory address** of a value (creates a pointer).

---

## Basic Syntax

```aria
value: i32 = 42;
ptr: *i32 = &value;  // Get address of value

stdout << ptr;  // Prints memory address (e.g., 0x7ffc8b4d5a1c)
```

---

## Address vs Borrow

In Aria, `&` has **two meanings** depending on context:

### Borrow (Reference)

```aria
// Creating reference for borrowing
ref: &i32 = &value;  // Borrow
```

### Address (Pointer)

```aria
// Getting raw memory address
ptr: *i32 = &value;  // Pointer
```

---

## Getting Addresses

```aria
value: i32 = 42;
address: *i32 = &value;

stdout << "Value: " << value << "\n";        // 42
stdout << "Address: " << address << "\n";    // 0x...
```

---

## For All Types

```aria
number: i32 = 42;
text: string = "hello";
point: Point = Point{x: 10, y: 20};

num_ptr: *i32 = &number;
str_ptr: *string = &text;
point_ptr: *Point = &point;
```

---

## Dereferencing

Use `*` to access value at address:

```aria
value: i32 = 42;
ptr: *i32 = &value;

dereferenced: i32 = *ptr;  // Get value at address
stdout << dereferenced;     // 42
```

---

## Use Cases

### Interfacing with C

```aria
// C function expects pointer
extern fn c_function(ptr: *i32);

value: i32 = 42;
c_function(&value);  // Pass address
```

### Manual Memory

```aria
// Allocate raw memory
ptr: *i32 = aria_alloc(sizeof(i32));

// Use address
*ptr = 42;

// Free
aria_free(ptr);
```

### Arrays

```aria
arr: [i32; 5] = [1, 2, 3, 4, 5];
ptr: *i32 = &arr[0];  // Address of first element
```

---

## Safety

### ⚠️ Raw Pointers Are Unsafe

```aria
// Can be null
ptr: *i32 = nil;

// Can outlive data
fn bad() -> *i32 {
    x: i32 = 42;
    return &x;  // ⚠️ Dangling pointer!
}

// No bounds checking
ptr: *i32 = &arr[0];
value: i32 = *(ptr + 100);  // ⚠️ Out of bounds!
```

### ✅ Prefer References

```aria
// Safe: Borrow checking
ref: &i32 = &value;

// Unsafe: Raw pointer
ptr: *i32 = &value;
```

---

## Best Practices

### ✅ DO: Use References Instead

```aria
// Good: Safe borrowing
fn process(value: &i32) { }

// Avoid: Unsafe pointers
fn process(value: *i32) { }
```

### ✅ DO: Use for C Interop

```aria
// Good: Necessary for C
extern fn c_func(data: *byte, len: i32);

buffer: []byte = [1, 2, 3];
c_func(&buffer[0], buffer.length());
```

### ❌ DON'T: Use for Normal Code

```aria
// Wrong: Unnecessary and unsafe
fn add(a: *i32, b: *i32) -> i32 {
    return *a + *b;
}

// Right: Use references
fn add(a: &i32, b: &i32) -> i32 {
    return a + b;
}
```

---

## References vs Pointers

### References (Preferred)

```aria
ref: &i32 = &value;

// ✅ Always valid
// ✅ Can't be null
// ✅ Borrow checked
// ✅ Auto-dereferenced
```

### Pointers (Rare)

```aria
ptr: *i32 = &value;

// ⚠️ Can be invalid
// ⚠️ Can be null
// ⚠️ No borrow checking
// ⚠️ Must dereference manually
```

---

## Examples

### C Interop

```aria
extern fn memcpy(dest: *byte, src: *byte, n: i32);

src: []byte = [1, 2, 3, 4];
dst: []byte = [0; 4];

memcpy(&dst[0], &src[0], 4);
```

### Custom Allocator

```aria
fn allocate_array(size: i32) -> *i32 {
    return aria_alloc(size * sizeof(i32));
}

ptr: *i32 = allocate_array(10);
defer aria_free(ptr);

// Use pointer
for i in 0..10 {
    *(ptr + i) = i;
}
```

---

## Related Topics

- [Borrow Operator](borrow_operator.md) - Borrowing with &
- [Borrowing](borrowing.md) - Reference system
- [Pointer Syntax](pointer_syntax.md) - Pointer types

---

**Remember**: `&` for **address** (pointers) or **borrowing** (references) - prefer references for safety!
