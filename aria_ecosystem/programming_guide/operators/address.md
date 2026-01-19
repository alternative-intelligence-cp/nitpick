# Address Operator (&)

**Category**: Operators → Memory  
**Operator**: `&`  
**Purpose**: Get memory address

---

## Syntax

```aria
&<variable>
```

---

## Description

The address operator `&` gets the memory address of a variable, creating a raw pointer.

---

## Basic Usage

```aria
value: i32 = 42;
ptr: *i32 = &value;  // Get address

stdout << *ptr;  // Dereference: 42
```

---

## Difference from Borrowing

```aria
// Borrow (safe reference)
ref: &i32 = &value;  // Type is &i32

// Address (raw pointer)
ptr: *i32 = &value;  // Type is *i32
```

---

## With Arrays

```aria
arr: []i32 = [1, 2, 3];
ptr: *i32 = &arr[0];  // Address of first element
```

---

## Best Practices

### ✅ DO: Use for C Interop

```aria
extern fn c_function(data: *i32);

value: i32 = 42;
c_function(&value);
```

### ❌ DON'T: Use Instead of References

```aria
// Prefer safe references
fn process(data: &Data) { ... }

// Avoid raw pointers
fn process(data: *Data) { ... }
```

---

## Related

- [Ampersand (&)](ampersand.md) - Borrowing
- [Pointer Syntax](../memory_model/pointer_syntax.md)
- [Address Operator](../memory_model/address_operator.md)

---

**Remember**: `&` for **addresses** is **unsafe** - prefer **references**!
