# Ampersand Operator (&)

**Category**: Operators → Reference  
**Operator**: `&`  
**Purpose**: Create immutable reference or get address

---

## Syntax

```aria
&<expression>  // Borrow (reference)
&<variable>    // Address (pointer)
```

---

## Description

The ampersand `&` has two meanings in Aria:
1. **Borrow**: Create an immutable reference
2. **Address**: Get memory address (raw pointer)

---

## Borrowing (Reference)

```aria
// Create immutable reference
value: i32 = 42;
ref: &i32 = &value;

stdout << *ref;  // 42

// Can't modify through immutable reference
*ref = 99;  // ❌ Error!
```

---

## Function Parameters

```aria
fn print_value(x: &i32) {
    stdout << *x;
}

value: i32 = 42;
print_value(&value);  // Pass by reference
```

---

## Multiple References

```aria
data: i32 = 100;

ref1: &i32 = &data;
ref2: &i32 = &data;
ref3: &i32 = &data;

// All valid - multiple immutable borrows allowed
stdout << *ref1 << *ref2 << *ref3;
```

---

## Address Operator

```aria
// Get raw memory address
value: i32 = 42;
ptr: *i32 = &value;

// ptr is a raw pointer
*ptr = 99;  // Modifies value
```

---

## Borrowing Rules

```aria
// OK: Multiple immutable borrows
x: i32 = 42;
a: &i32 = &x;
b: &i32 = &x;

// Error: Can't mix immutable and mutable borrows
x: i32 = 42;
r: &i32 = &x;
w: $i32 = $x;  // ❌ Error!
```

---

## Best Practices

### ✅ DO: Use for Read-Only Access

```aria
fn calculate(data: &Data) -> i32 {
    return data.field1 + data.field2;
}
```

### ✅ DO: Borrow Large Structs

```aria
// Efficient - no copy
fn process(big_data: &LargeStruct) {
    ...
}

// Inefficient - copies entire struct
fn process(big_data: LargeStruct) {
    ...
}
```

### ❌ DON'T: Return Local References

```aria
// Wrong: Dangling reference
fn bad() -> &i32 {
    value: i32 = 42;
    return &value;  // ❌ value destroyed!
}
```

---

## Auto-Dereferencing

```aria
// References auto-dereference in most contexts
ref: &i32 = &42;

when ref == 42 then  // Auto-dereference
    stdout << "Equal";
end

// Manual dereference when needed
value: i32 = *ref;
```

---

## Related

- [Dollar ($)](dollar_operator.md) - Mutable borrow
- [Borrowing](../memory_model/borrowing.md)
- [Immutable Borrow](../memory_model/immutable_borrow.md)

---

**Remember**: `&` creates **read-only** references!
