# Borrow Operator

**Category**: Memory Model → Operators  
**Symbols**: `&`, `$`  
**Purpose**: Create references

---

## What is the Borrow Operator?

The **borrow operator** creates references to values:
- `&` creates **immutable** (read-only) references
- `$` creates **mutable** (read-write) references

---

## Immutable Borrow: &

```aria
value: i32 = 42;
ref: &i32 = &value;  // Immutable reference

stdout << ref;  // ✅ Can read
ref = 100;      // ❌ Can't modify
```

---

## Mutable Borrow: $

```aria
value: i32 = 42;
ref: &i32 = $value;  // Mutable reference

stdout << ref;  // ✅ Can read
ref = 100;      // ✅ Can modify
```

---

## In Function Calls

```aria
fn read(v: &i32) {
    stdout << v;
}

fn modify(v: &i32) {
    v = v * 2;
}

value: i32 = 42;

read(&value);    // Pass immutable reference
modify($value);  // Pass mutable reference
```

---

## Syntax Summary

### Create References

```aria
&value   // Immutable reference
$value   // Mutable reference
```

### Declare Reference Types

```aria
ref: &i32        // Immutable reference to i32
mut_ref: &i32    // Mutable reference to i32 (must use $ when creating)
```

---

## Multiple Uses

### Multiple Immutable

```aria
value: i32 = 42;

ref1: &i32 = &value;  // ✅
ref2: &i32 = &value;  // ✅
ref3: &i32 = &value;  // ✅
```

### One Mutable

```aria
value: i32 = 42;

ref1: &i32 = $value;  // ✅
ref2: &i32 = $value;  // ❌ Error: already borrowed
```

### Can't Mix

```aria
value: i32 = 42;

ref1: &i32 = &value;  // Immutable
ref2: &i32 = $value;  // ❌ Error: can't borrow mutably
```

---

## With Collections

```aria
numbers: []i32 = [1, 2, 3];

// Immutable borrow
fn sum(arr: &[]i32) -> i32 { }
total: i32 = sum(&numbers);

// Mutable borrow
fn double_all(arr: &[]i32) { }
double_all($numbers);
```

---

## In Loops

### Read-Only

```aria
for item in &collection {
    // item is &T (immutable reference)
}
```

### Mutable

```aria
for $item in collection {
    // item is mutable
}
```

---

## Best Practices

### ✅ DO: Use & by Default

```aria
// Good: Immutable unless needed
fn process(data: &Data) { }
```

### ✅ DO: Use $ When Modifying

```aria
// Good: Clear mutation intent
fn update(data: &Data) { }
update($my_data);
```

### ❌ DON'T: Confuse with Other Languages

```aria
// Aria: $ for mutable
ref: &i32 = $value;

// Rust: &mut
// let ref: &mut i32 = &mut value;

// C++: non-const reference
// int& ref = value;
```

---

## Quick Reference

| Operator | Meaning | Access |
|----------|---------|--------|
| `&value` | Borrow immutably | Read-only |
| `$value` | Borrow mutably | Read-write |

---

## Related Topics

- [Borrowing](borrowing.md) - Borrowing system
- [Immutable Borrow](immutable_borrow.md) - Read-only references
- [Mutable Borrow](mutable_borrow.md) - Mutable references

---

**Remember**: `&` = **read-only**, `$` = **read-write**!
