# Dollar Variable ($)

**Category**: Control Flow → Iteration  
**Symbol**: `$`  
**Philosophy**: Automatic index variable in loops

---

## What is the Dollar Variable?

The **dollar sign (`$`)** is the **automatic index variable** in `till` loops, representing the current iteration index.

---

## Basic Syntax

```aria
till(array.length - 1, 1) {
    // $ is the current index (0, 1, 2, ...)
    stdout << $ << ": " << array[$] << "\n";
}
```

---

## Type of `$` — Always `int64`

`$` is always type **`int64`**, regardless of the collection element type. This is fixed, not inferred.

```aria
till(items.length - 1, 1) {
    int64:idx = $;   // ✅ correct
    int32:idx = $;   // ❌ ERROR: cannot assign int64 to int32
}
```

This matters when using `$` as an array subscript or passing it to a function that expects a specific integer type — always capture it as `int64`:

```aria
int64[256]:buf;
till(256, 1) {
    int64:i = $;
    buf[i] = 0i64;
}
```

---

## $ as Index

In `till` loops, `$` automatically holds the current index:

```aria
numbers: []i32 = [10, 20, 30];

till(numbers.length - 1, 1) {
    // $ is 0, then 1, then 2
    stdout << "Index " << $ << " = " << numbers[$] << "\n";
}

// Output:
// Index 0 = 10
// Index 1 = 20
// Index 2 = 30
```

---

## Reading Values with $

Use `$` to index into arrays:

```aria
users: []User = load_users();

till(users.length - 1, 1) {
    user: auto = users[$];  // $ is the index
    stdout << user.name << "\n";
}
```

---

## Modifying Array Elements

Use `$` to modify elements directly:

```aria
scores: []i32 = [85, 90, 78, 92];

// Double all scores
till(scores.length - 1, 1) {
    scores[$] = scores[$] * 2;
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
till(products.length - 1, 1) {
    products[$].price = products[$].price * 0.9;
}
```

---

## With Item and Index

```aria
items: []string = ["apple", "banana", "cherry"];

till(items.length - 1, 1) {
    item: auto = items[$];
    // $ is the index, item is the value
    items[$] = format("{}. {}", $ + 1, item);
}

// items is now ["1. apple", "2. banana", "3. cherry"]
```

---

## Using $ in Calculations

```aria
till(items.length - 1, 1) {
    // $ can be used in any expression
    rank: i32 = $ + 1;  // 1-based ranking
    stdout << rank << ". " << items[$] << "\n";
}
```

---

## Common Patterns

### Update All Elements

```aria
temperatures: []f64 = [32.0, 68.0, 86.0];

// Convert Fahrenheit to Celsius
till(temperatures.length - 1, 1) {
    temperatures[$] = (temperatures[$] - 32.0) * 5.0 / 9.0;
}
```

### Normalize Data

```aria
values: []i32 = [10, 20, 30, 40];
max: i32 = 40;

till(values.length - 1, 1) {
    values[$] = (values[$] * 100) / max;  // Convert to percentage
}

// values is now [25, 50, 75, 100]
```

### Apply Transformation

```aria
strings: []string = ["hello", "world"];

till(strings.length - 1, 1) {
    strings[$] = strings[$].to_uppercase();
}

// strings is now ["HELLO", "WORLD"]
```

---

## When to Use $

### ✅ Use $ When:

- Accessing array elements by index
- Modifying array elements in-place
- Need the numeric index value
- Performing index-based calculations

### ✅ $ is Always Available:

- `$` is automatically available in `till` loops
- Represents current iteration index
- No declaration needed

---

## Index Value

`$` holds the actual numeric index:

```aria
till(5, 1) {
    // $ is 0, 1, 2, 3, 4, 5
    stdout << "Iteration: " << $ << "\n";
}
```

---

## Best Practices

### ✅ DO: Use $ for Array Access

```aria
// Good: Clear index usage
till(users.length - 1, 1) {
    users[$].last_active = now();
}
```

### ✅ DO: Use $ for In-Place Updates

```aria
// Good: Modify originals
till(scores.length - 1, 1) {
    scores[$] = scores[$] + bonus;
}
```

### ✅ DO: Store $ if Needed Multiple Times

```aria
// Good: Store index for clarity (use int64, not int32)
till(items.length - 1, 1) {
    idx: int64 = $;
    stdout << idx << ": " << items[idx] << "\n";
}
```

### ❌ DON'T: Modify $ (It's Read-Only)

```aria
// Wrong: Can't modify $
till(10, 1) {
    $ = 5;  // ❌ Error: $ is read-only
}
```

### ❌ DON'T: Add/Remove During Iteration

```aria
// ❌ Wrong: Don't modify array size during iteration
till(items.length - 1, 1) {
    items.push(new_item);  // Unsafe!
}

// ✅ Right: Collect changes, apply after
to_add: []Item = [];
till(items.length - 1, 1) {
    to_add.push(create_related(items[$]));
}

till(to_add.length - 1, 1) {
    items.push(to_add[$]);
}
```

---

## Comparison with Other Languages

### C/C++

```c
// C: Explicit index variable
for (int i = 0; i < n; i++) {
    array[i] = array[i] * 2;
}
```

### Python

```python
# Python: enumerate for index
for i, item in enumerate(items):
    items[i] = item * 2
```

### Aria

```aria
// Aria: $ is automatic index
till(items.length - 1, 1) {
    items[$] = items[$] * 2;
}
```

---

## Real-World Examples

### Batch Update

```aria
records: []Record = load_records();

till(records.length - 1, 1) {
    records[$].updated_at = now();
    records[$].version = records[$].version + 1;
}

save_records(records);
```

### Data Cleaning

```aria
data: []string = load_data();

till(data.length - 1, 1) {
    data[$] = data[$].trim();
    data[$] = data[$].to_lowercase();
    data[$] = data[$].replace(",", "");
}
```

### Price Adjustment

```aria
items: []Item = get_inventory();

till(items.length - 1, 1) {
    if items[$].category == "sale" {
        items[$].price = items[$].price * 0.8;  // 20% off
    }
}
```

---

## Related Topics

- [Till Loops](till.md) - Till loop syntax
- [For Loops](for.md) - Loop basics
- [Iteration Variable](iteration_variable.md) - Loop variables
- [Loop](loop.md) - Loop statement

---

**Remember**: `$` is the **automatic index variable** - use it to access array elements, get the current index, and modify elements in-place!
