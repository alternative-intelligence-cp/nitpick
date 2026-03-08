# Iteration Variable

**Category**: Control Flow → Iteration  
**Concept**: Loop variable that holds current item  
**Philosophy**: Clear, readable iteration

---

## What is an Iteration Variable?

The **iteration variable** is the variable that holds the current item in each loop iteration.

---

## Basic Syntax

```aria
till(collection.length - 1, 1) {
    // $ holds current index
    item: auto = collection[$];
}
```

---

## In Till Loops

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

till(numbers.length - 1, 1) {
    // $ is the index, numbers[$] is the current value
    stdout << numbers[$] << "\n";
}
```

---

## Naming Iteration Variables

Choose **descriptive names** for loop variables:

```aria
// ✅ Good: Descriptive
till(users.length - 1, 1) { user: auto = users[$]; }
till(items.length - 1, 1) { item: auto = items[$]; }
till(records.length - 1, 1) { record: auto = records[$]; }

// ❌ Poor: Generic
till(users.length - 1, 1) { x: auto = users[$]; }
till(items.length - 1, 1) { i: auto = items[$]; }  // i suggests index, not item
```

---

## Read-Only by Default

```aria
numbers: []i32 = [1, 2, 3];

till(numbers.length - 1, 1) {
    num: auto = numbers[$];  // num is local read-only copy
    stdout << num;           // ✅ Can read
    num = 10;                // ✅ Can modify local copy
    numbers[$] = 10;         // ❌ Can't modify array directly
}
```

---

## Mutable with Array Indexing

```aria
numbers: []i32 = [1, 2, 3];

till(numbers.length - 1, 1) {
    // $ is the index - can modify array directly
    numbers[$] = numbers[$] * 2;  // ✅ Can modify
}
```

---

## With Index

```aria
items: []string = ["apple", "banana", "cherry"];

// $ is the index - access both item and index
till(items.length - 1, 1) {
    item: auto = items[$];
    stdout << $ << ": " << item << "\n";
}
```

---

## Index is $

Aria uses **$** as the index variable in loops:

```aria
// $ is the current index (0-based)
till(items.length - 1, 1) {
    // $ varies from 0 to items.length-1
    stdout << "Index: " << $ << "\n";
}
```

---

## Type of `$` — Always `int64`

`$` has type **`int64`** in both `loop` and `till`. This is fixed, not inferred.

```aria
loop(0, 10, 1) {
    int64:i = $;   // ✅ correct — $ is int64
    int32:i = $;   // ❌ ERROR: cannot assign int64 to int32
}
```

When capturing `$` into a named variable for array indexing, always use `int64`:

```aria
int64[512]:grid;
loop(0, 512, 1) {
    int64:idx = $;
    grid[idx] = 0i64;
}
```

---

## Type of Iteration Variable — Inferred From Collection

The *element* variable you declare from the collection is inferred from the collection's element type — not from `$`:

```aria
numbers: []i32 = [1, 2, 3];

till(numbers.length - 1, 1) {
    num: auto = numbers[$];
    // num is inferred as i32 (from the array element type)
}

users: []User = load_users();

till(users.length - 1, 1) {
    user: auto = users[$];
    // user is inferred as User
}
```

---

## Can Declare Type Explicitly

```aria
items: []any = get_mixed_items();

till(items.length - 1, 1) {
    item: string = items[$] as string;
    // item explicitly typed as string
}
```

---

## Scope

Iteration variable exists **only inside the loop**:

```aria
till(items.length - 1, 1) {
    item: auto = items[$];
    stdout << item;
}

// item doesn't exist here
stdout << item;  // ❌ Error
```

---

## Shadowing

Can shadow outer variables:

```aria
item: string = "outer";

till(items.length - 1, 1) {
    item: auto = items[$];  // This 'item' shadows the outer one
    stdout << item;
}

stdout << item;  // Back to "outer"
```

---

## Accessing Index and Value

### Index via $

```aria
till(collection.length - 1, 1) {
    item: auto = collection[$];
    // $: index (position, 0-based)
    // item: element value
}
```

### Iterating Maps

For map iteration, use the map's keys or iteration methods:

```aria
map: Map<string, i32> = load_map();
keys: []string = map.keys();

till(keys.length - 1, 1) {
    key: auto = keys[$];
    value: auto = map[key];
    stdout << key << ": " << value << "\n";
}
```

---

## Only Using Index or Value

```aria
// Only need index - just use $
till(items.length - 1, 1) {
    stdout << "Position: " << $ << "\n";
}

// Only need value - declare variable
till(items.length - 1, 1) {
    item: auto = items[$];
    stdout << "Item: " << item << "\n";
}
```

---

## Best Practices

### ✅ DO: Use Descriptive Names

```aria
// Good: Clear intent
till(users.length - 1, 1) {
    user: auto = users[$];
    send_email(user);
}

till(transactions.length - 1, 1) {
    transaction: auto = transactions[$];
    process_transaction(transaction);
}
```

### ✅ DO: Use Direct Array Access When Modifying

```aria
// Good: Explicit mutation via array indexing
till(scores.length - 1, 1) {
    scores[$] = scores[$] + bonus;
}
```

### ✅ DO: Use Singular for Collections

```aria
// Good: Singular item from plural collection
till(users.length - 1, 1) { user: auto = users[$]; }
till(items.length - 1, 1) { item: auto = items[$]; }
till(records.length - 1, 1) { record: auto = records[$]; }
```

### ❌ DON'T: Reuse Loop Variable

```aria
// Wrong: Confusing
till(items.length - 1, 1) {
    item: auto = items[$];
    item = process(item);
    item = transform(item);
    item = validate(item);
    // Which item?
}

// Right: Different names for transformations
till(items.length - 1, 1) {
    original: auto = items[$];
    processed: Item = process(original);
    transformed: Item = transform(processed);
    validated: Item = validate(transformed);
}
```

### ❌ DON'T: Use Abbreviated Names

```aria
// Wrong: Unclear
till(users.length - 1, 1) { u: auto = users[$]; }
till(transactions.length - 1, 1) { t: auto = transactions[$]; }

// Right: Full names
till(users.length - 1, 1) { user: auto = users[$]; }
till(transactions.length - 1, 1) { transaction: auto = transactions[$]; }
```

---

## Common Patterns

### Process Each Item

```aria
till(users.length - 1, 1) {
    user: auto = users[$];
    user.notify();
}
```

### Filter and Collect

```aria
active: []User = [];

till(users.length - 1, 1) {
    user: auto = users[$];
    if user.is_active {
        active.push(user);
    }
}
```

### Transform

```aria
till(values.length - 1, 1) {
    values[$] = values[$] * 2;
}
```

### Find First

```aria
found: User? = nil;

till(users.length - 1, 1) {
    user: auto = users[$];
    if user.name == "Alice" {
        found = user;
        break;
    }
}
```

---

## Real-World Examples

### Processing Orders

```aria
orders: []Order = load_orders();

till(orders.length - 1, 1) {
    order: auto = orders[$];
    // Clear: 'order' represents current order
    validate_order(order);
    process_payment(order);
    ship_order(order);
}
```

### Batch Update

```aria
products: []Product = get_products();

till(products.length - 1, 1) {
    // Direct array access for modification
    products[$].price = products[$].price * 1.1;  // 10% increase
    products[$].updated_at = now();
}
```

### Report Generation

```aria
students: []Student = load_students();

till(students.length - 1, 1) {
    student: auto = students[$];
    rank: int64 = $;  // Index is the rank ($ is int64)
    // Both student and rank are clear
    stdout << format("{}. {} - GPA: {}", 
        rank + 1, student.name, student.gpa);
}
```

---

## Related Topics

- [For Loops](for.md) - For loop basics
- [Dollar Variable](dollar_variable.md) - Mutable iteration
- [Loop](loop.md) - Loop statement
- [While](while.md) - While loops

---

**Remember**: Iteration variable is **the current item** - use descriptive names, `$` for mutation, scope is loop-only!
