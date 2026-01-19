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
for variable in collection {
    // variable holds current item
}
```

---

## In For Loops

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

for num in numbers {
    // 'num' is the iteration variable
    stdout << num << "\n";
}
```

---

## Naming Iteration Variables

Choose **descriptive names**:

```aria
// ✅ Good: Descriptive
for user in users { }
for item in items { }
for record in records { }

// ❌ Poor: Generic
for x in users { }
for i in items { }  // i suggests index, not item
```

---

## Read-Only by Default

```aria
numbers: []i32 = [1, 2, 3];

for num in numbers {
    // num is read-only
    stdout << num;  // ✅ Can read
    num = 10;       // ❌ Can't modify
}
```

---

## Mutable with $

```aria
numbers: []i32 = [1, 2, 3];

for $num in numbers {
    // $num is mutable
    num = num * 2;  // ✅ Can modify
}
```

---

## With Index

```aria
items: []string = ["apple", "banana", "cherry"];

// Two iteration variables: item and index
for item, index in items {
    stdout << index << ": " << item << "\n";
}
```

---

## Index First or Second?

Aria convention: **item first, index second**:

```aria
// ✅ Aria convention
for item, index in items { }

// Compare: Other languages
// Python: for index, item in enumerate(items)
// Go: for index, item := range items
```

---

## Type is Inferred

```aria
numbers: []i32 = [1, 2, 3];

for num in numbers {
    // num is inferred as i32
}

users: []User = load_users();

for user in users {
    // user is inferred as User
}
```

---

## Can Declare Type Explicitly

```aria
items: []any = get_mixed_items();

for item: string in items {
    // item treated as string
}
```

---

## Scope

Iteration variable exists **only inside the loop**:

```aria
for item in items {
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

for item in items {
    // This 'item' shadows the outer one
    stdout << item;
}

stdout << item;  // Back to "outer"
```

---

## Multiple Iteration Variables

### Item and Index

```aria
for item, index in collection {
    // item: element
    // index: position (0-based)
}
```

### Key and Value

```aria
map: Map<string, i32> = load_map();

for key, value in map {
    stdout << key << ": " << value << "\n";
}
```

---

## Underscore for Unused

```aria
// Only need index
for _, index in items {
    stdout << "Position: " << index << "\n";
}

// Only need key
for key, _ in map {
    stdout << "Key: " << key << "\n";
}
```

---

## Best Practices

### ✅ DO: Use Descriptive Names

```aria
// Good: Clear intent
for user in users {
    send_email(user);
}

for transaction in transactions {
    process_transaction(transaction);
}
```

### ✅ DO: Use $ When Modifying

```aria
// Good: Explicit mutation
for $score in scores {
    score = score + bonus;
}
```

### ✅ DO: Use Singular for Collections

```aria
// Good: Singular item from plural collection
for user in users { }
for item in items { }
for record in records { }
```

### ❌ DON'T: Reuse Loop Variable

```aria
// Wrong: Confusing
for item in items {
    item = process(item);
    item = transform(item);
    item = validate(item);
    // Which item?
}

// Right: Different names for transformations
for original in items {
    processed: Item = process(original);
    transformed: Item = transform(processed);
    validated: Item = validate(transformed);
}
```

### ❌ DON'T: Use Abbreviated Names

```aria
// Wrong: Unclear
for u in users { }
for t in transactions { }

// Right: Full names
for user in users { }
for transaction in transactions { }
```

---

## Common Patterns

### Process Each Item

```aria
for user in users {
    user.notify();
}
```

### Filter and Collect

```aria
active: []User = [];

for user in users {
    if user.is_active {
        active.push(user);
    }
}
```

### Transform

```aria
for $value in values {
    value = value * 2;
}
```

### Find First

```aria
found: User? = nil;

for user in users {
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

for order in orders {
    // Clear: 'order' represents each order
    validate_order(order);
    process_payment(order);
    ship_order(order);
}
```

### Batch Update

```aria
products: []Product = get_products();

for $product in products {
    // $ indicates we're modifying
    product.price = product.price * 1.1;  // 10% increase
    product.updated_at = now();
}
```

### Report Generation

```aria
students: []Student = load_students();

for student, rank in students {
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
