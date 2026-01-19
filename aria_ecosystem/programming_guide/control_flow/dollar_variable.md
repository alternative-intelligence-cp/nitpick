# Dollar Variable ($)

**Category**: Control Flow → Iteration  
**Symbol**: `$`  
**Philosophy**: Explicit mutation in loops

---

## What is the Dollar Variable?

The **dollar sign (`$`)** marks loop variables for **mutable access** to the original collection items.

---

## Basic Syntax

```aria
for $item in collection {
    item.modify();  // Can modify original
}
```

---

## Why Dollar Sign?

By default, loop variables are **read-only copies**:

```aria
numbers: []i32 = [1, 2, 3];

for num in numbers {
    num = num * 2;  // ❌ Error: num is immutable
}
```

Use `$` for **mutable access**:

```aria
numbers: []i32 = [1, 2, 3];

for $num in numbers {
    num = num * 2;  // ✅ Works: Modifies original array
}

stdout << numbers;  // [2, 4, 6]
```

---

## Read-Only by Default

```aria
users: []User = load_users();

// Read-only iteration
for user in users {
    stdout << user.name << "\n";  // ✅ Can read
    user.name = "New";            // ❌ Can't modify
}
```

---

## Mutable with $

```aria
users: []User = load_users();

// Mutable iteration
for $user in users {
    stdout << user.name << "\n";  // ✅ Can read
    user.name = "Updated";        // ✅ Can modify
}
```

---

## Modifying Array Elements

```aria
scores: []i32 = [85, 90, 78, 92];

// Double all scores
for $score in scores {
    score = score * 2;
}

stdout << scores;  // [170, 180, 156, 184]
```

---

## Modifying Struct Fields

```aria
struct Product {
    name: string,
    price: f64
}

products: []Product = load_products();

// Apply 10% discount
for $product in products {
    product.price = product.price * 0.9;
}
```

---

## With Index

```aria
items: []string = ["apple", "banana", "cherry"];

for $item, index in items {
    item = format("{}. {}", index + 1, item);
}

// items is now ["1. apple", "2. banana", "3. cherry"]
```

---

## Only $ Variable is Mutable

```aria
for $item, index in items {
    item = "new";    // ✅ Can modify item (marked with $)
    index = 999;     // ❌ Can't modify index (not marked with $)
}
```

---

## Common Patterns

### Update All Elements

```aria
temperatures: []f64 = [32.0, 68.0, 86.0];

// Convert Fahrenheit to Celsius
for $temp in temperatures {
    temp = (temp - 32.0) * 5.0 / 9.0;
}
```

### Normalize Data

```aria
values: []i32 = [10, 20, 30, 40];
max: i32 = 40;

for $value in values {
    value = (value * 100) / max;  // Convert to percentage
}

// values is now [25, 50, 75, 100]
```

### Apply Transformation

```aria
strings: []string = ["hello", "world"];

for $str in strings {
    str = str.to_uppercase();
}

// strings is now ["HELLO", "WORLD"]
```

---

## When to Use $

### ✅ Use $ When:

- Need to modify original collection
- Updating elements in-place
- Applying transformations
- Changing struct fields

### ❌ Don't Use $ When:

- Only reading values
- Creating new collection
- Don't need to modify originals

---

## Read-Only vs Mutable

### Read-Only (No $)

```aria
for item in items {
    // item is a copy
    // Can't modify original
    process(item);
}
```

### Mutable (With $)

```aria
for $item in items {
    // item references original
    // Can modify original
    item.update();
}
```

---

## Best Practices

### ✅ DO: Use $ for In-Place Updates

```aria
// Good: Modify originals
for $user in users {
    user.last_active = now();
}
```

### ✅ DO: Use $ When Intent is Clear

```aria
// Good: Obviously modifying
for $score in scores {
    score = score + bonus;
}
```

### ❌ DON'T: Use $ if Not Modifying

```aria
// Wrong: Unnecessary $
for $item in items {
    stdout << item << "\n";  // Just reading
}

// Right: No $
for item in items {
    stdout << item << "\n";
}
```

### ❌ DON'T: Modify Structure While Iterating

```aria
// ❌ Wrong: Don't add/remove during iteration
for $item in items {
    items.push(new_item);  // Unsafe!
}

// ✅ Right: Collect changes, apply after
to_add: []Item = [];
for item in items {
    to_add.push(create_related(item));
}

for item in to_add {
    items.push(item);
}
```

---

## Comparison with Other Languages

### Python

```python
# Python: Always mutable
for item in items:
    item.field = "new"  # Modifies original
```

### Rust

```rust
// Rust: Explicit mut
for item in &mut items {
    item.field = "new";  // Modifies original
}
```

### Aria

```aria
// Aria: Use $
for $item in items {
    item.field = "new";  // Modifies original
}
```

---

## Real-World Examples

### Batch Update

```aria
records: []Record = load_records();

for $record in records {
    record.updated_at = now();
    record.version = record.version + 1;
}

save_records(records);
```

### Data Cleaning

```aria
data: []string = load_data();

for $entry in data {
    entry = entry.trim();
    entry = entry.to_lowercase();
    entry = entry.replace(",", "");
}
```

### Price Adjustment

```aria
items: []Item = get_inventory();

for $item in items {
    if item.category == "sale" {
        item.price = item.price * 0.8;  // 20% off
    }
}
```

---

## Related Topics

- [For Loops](for.md) - For loop basics
- [Iteration Variable](iteration_variable.md) - Loop variables
- [Closures](../functions/closures.md) - $ in closures
- [Mutable References](../memory_model/mutable_borrow.md) - Mutable borrowing

---

**Remember**: `$` means **mutable access** - use for in-place updates, omit for read-only iteration!
