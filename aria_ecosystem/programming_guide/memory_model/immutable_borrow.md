# Immutable Borrow

**Category**: Memory Model → References  
**Operator**: `&`  
**Concept**: Read-only reference

---

## What is an Immutable Borrow?

An **immutable borrow** is a read-only reference created with `&` that allows viewing data without modifying it.

---

## Basic Syntax

```aria
value: i32 = 42;
ref: &i32 = &value;  // Immutable borrow

stdout << ref;  // ✅ Can read
ref = 100;      // ❌ Can't modify
```

---

## Multiple Immutable Borrows

You can have **unlimited** immutable borrows simultaneously:

```aria
value: i32 = 42;

ref1: &i32 = &value;  // ✅
ref2: &i32 = &value;  // ✅
ref3: &i32 = &value;  // ✅

stdout << ref1 << ref2 << ref3;  // All valid
```

---

## In Functions

```aria
fn print_value(v: &i32) {
    stdout << v << "\n";
}

value: i32 = 42;
print_value(&value);  // Borrow
stdout << value;      // Still usable
```

---

## Multiple Parameters

```aria
fn compare(a: &i32, b: &i32) -> bool {
    return a < b;
}

x: i32 = 10;
y: i32 = 20;

Result: bool = compare(&x, &y);  // Both borrowed
stdout << x << y;  // Both still usable
```

---

## With Collections

```aria
fn find(items: &[]string, target: string) -> i32? {
    till(items.length - 1, 1) {
        if items[$] == target {
            return $;
        }
    }
    return nil;
}

names: []string = ["Alice", "Bob", "Charlie"];
index: i32? = find(&names, "Bob");
// names still usable
```

---

## Rules

### ✅ CAN: Multiple Readers

```aria
value: i32 = 42;

ref1: &i32 = &value;
ref2: &i32 = &value;
ref3: &i32 = &value;
// All OK simultaneously
```

### ❌ CAN'T: Modify Through Reference

```aria
value: i32 = 42;
ref: &i32 = &value;

ref = 100;  // ❌ Error: immutable reference
```

### ❌ CAN'T: Mix with Mutable Borrow

```aria
value: i32 = 42;

ref: &i32 = &value;      // Immutable borrow
mut_ref: &i32 = $value;  // ❌ Error: can't borrow mutably
```

---

## Automatic Dereferencing

Aria automatically dereferences in most contexts:

```aria
value: i32 = 42;
ref: &i32 = &value;

// Automatic dereference
stdout << ref;          // Works like ref is i32
Result: i32 = ref + 1;  // Works like ref is i32
```

---

## Common Patterns

### Read Configuration

```aria
fn get_host(config: &Config) -> string {
    return config.host;  // Just reading
}
```

### Calculate from Data

```aria
fn calculate_average(numbers: &[]f64) -> f64 {
    sum: f64 = 0.0;
    till(numbers.length - 1, 1) {
        sum = sum + numbers[$];
    }
    return sum / numbers.length() as f64;
}
```

### Validation

```aria
fn is_valid(user: &User) -> bool {
    return user.name.length() > 0 && user.age >= 18;
}
```

---

## Best Practices

### ✅ DO: Use for Read-Only Access

```aria
// Good: Just reading
fn display(data: &Data) {
    stdout << data.format();
}
```

### ✅ DO: Default to Immutable

```aria
// Good: Immutable unless mutation needed
fn process(input: &string) {  // Read-only
    analyze(input);
}
```

### ❌ DON'T: Use When Mutation Needed

```aria
// Wrong: Need mutable for this
fn double(value: &i32) {
    value = value * 2;  // ❌ Error: immutable
}

// Right: Use mutable borrow
fn double(value: &i32) {
    value = value * 2;  // ✅ OK
}
```

---

## Examples

### String Operations

```aria
fn count_words(text: &string) -> i32 {
    return text.split(" ").length();
}

doc: string = "Hello world from Aria";
count: i32 = count_words(&doc);
```

### Comparison

```aria
fn max(a: &i32, b: &i32) -> i32 {
    if a > b {
        return a;
    } else {
        return b;
    }
}

x: i32 = 10;
y: i32 = 20;
Result: i32 = max(&x, &y);
```

---

## Related Topics

- [Borrowing](borrowing.md) - Borrowing overview
- [Mutable Borrow](mutable_borrow.md) - Mutable references
- [Borrow Operator](borrow_operator.md) - & operator

---

**Remember**: Immutable borrow = **read-only reference** - unlimited readers, no writers!
