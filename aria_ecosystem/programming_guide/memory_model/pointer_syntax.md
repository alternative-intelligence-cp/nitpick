# Pointer Syntax

**Category**: Memory Model → Pointers  
**Syntax**: `*T`  
**Concept**: Raw memory addresses

---

## Pointer Type Syntax

```aria
ptr: *i32        // Pointer to i32
ptr: *string     // Pointer to string
ptr: *Point      // Pointer to Point
```

---

## Creating Pointers

```aria
value: i32 = 42;
ptr: *i32 = &value;  // Get address
```

---

## Dereferencing

```aria
value: i32 = 42;
ptr: *i32 = &value;

dereferenced: i32 = *ptr;  // Get value at address
```

---

## Null Pointers

```aria
ptr: *i32 = nil;  // Null pointer

when ptr == nil then
    stderr << "Null pointer!\n";
end
```

---

## Pointer Arithmetic

```aria
arr: [i32; 5] = [1, 2, 3, 4, 5];
ptr: *i32 = &arr[0];

first: i32 = *ptr;        // 1
second: i32 = *(ptr + 1); // 2
third: i32 = *(ptr + 2);  // 3
```

---

## Function Pointers

```aria
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

func_ptr: fn(i32, i32) -> i32 = add;
Result: i32 = func_ptr(5, 3);  // 8
```

---

## Safety Notes

⚠️ **Raw pointers bypass safety checks:**

```aria
// Can be null
ptr: *i32 = nil;
value: i32 = *ptr;  // Crash!

// Can be invalid
ptr: *i32 = &local_var;
// local_var freed
value: i32 = *ptr;  // Undefined behavior!

// No bounds checking
ptr: *i32 = &arr[0];
value: i32 = *(ptr + 1000);  // Out of bounds!
```

---

## Prefer References

```aria
// ❌ Unsafe: Raw pointer
fn process(ptr: *i32) {
    value: i32 = *ptr;
}

// ✅ Safe: Reference
fn process(ref: &i32) {
    value: i32 = ref;
}
```

---

## Use Cases

### C Interop

```aria
extern fn c_function(data: *byte, len: i32);

buffer: []byte = [1, 2, 3];
c_function(&buffer[0], 3);
```

### Manual Allocation

```aria
ptr: *i32 = aria_alloc(sizeof(i32));
*ptr = 42;
aria_free(ptr);
```

### Low-Level Code

```aria
// Direct memory manipulation
ptr: *byte = get_hardware_address();
*ptr = 0xFF;
```

---

## Related Topics

- [Address Operator](address_operator.md) - Getting addresses
- [Borrowing](borrowing.md) - Safe references
- [Allocators](allocators.md) - Memory allocation

---

**Remember**: Pointers are **unsafe** - prefer references when possible!
