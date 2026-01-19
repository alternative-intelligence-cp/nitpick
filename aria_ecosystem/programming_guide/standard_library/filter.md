# filter()

**Category**: Standard Library → Functional Programming  
**Syntax**: `filter(array: []T, predicate: fn(T) -> bool) -> []T`  
**Purpose**: Filter array elements by condition

---

## Overview

`filter()` creates a new array containing only elements that satisfy a condition.

---

## Syntax

```aria
import std.functional;

filtered: []i32 = filter(numbers, fn(x: i32) -> bool {
    return x > 10;
});
```

---

## Parameters

- **array** (`[]T`) - Input array
- **predicate** (`fn(T) -> bool`) - Function returning true to keep element

---

## Returns

- `[]T` - New array with elements where predicate returned true

---

## Examples

### Filter Numbers

```aria
numbers: []i32 = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

// Keep only even numbers
evens: []i32 = filter(numbers, fn(x: i32) -> bool {
    return x % 2 == 0;
});
// [2, 4, 6, 8, 10]
```

### Filter Strings

```aria
words: []string = ["apple", "banana", "apricot", "cherry", "avocado"];

// Keep words starting with 'a'
a_words: []string = filter(words, fn(w: string) -> bool {
    return w.starts_with("a");
});
// ["apple", "apricot", "avocado"]
```

### Filter by Length

```aria
names: []string = ["Alice", "Bob", "Charlie", "Dave", "Eve"];

// Keep names longer than 4 characters
long_names: []string = filter(names, fn(name: string) -> bool {
    return name.length() > 4;
});
// ["Alice", "Charlie"]
```

### Filter Structs

```aria
struct User {
    name: string,
    age: i32,
    active: bool
}

users: []User = [...];

// Keep only active users
active_users: []User = filter(users, fn(u: User) -> bool {
    return u.active;
});

// Keep users over 18
adults: []User = filter(users, fn(u: User) -> bool {
    return u.age >= 18;
});
```

---

## Method Syntax

```aria
numbers: []i32 = [1, 2, 3, 4, 5, 6];

// Using array method
evens: []i32 = numbers.filter(fn(x: i32) -> bool {
    return x % 2 == 0;
});
```

---

## Chaining

```aria
numbers: []i32 = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

Result: []i32 = numbers
    .filter(fn(x) -> bool { return x > 3; })      // [4,5,6,7,8,9,10]
    .filter(fn(x) -> bool { return x < 8; })      // [4,5,6,7]
    .filter(fn(x) -> bool { return x % 2 == 0; }); // [4,6]
```

---

## Best Practices

### ✅ DO: Use for Filtering Collections

```aria
// Perfect use case
active_items: []Item = filter(all_items, fn(item) -> bool {
    return item.active;
});
```

### ✅ DO: Chain with Other Operations

```aria
Result: []i32 = numbers
    .filter(fn(x) -> bool { return x > 0; })
    .map(fn(x) -> i32 { return x * 2; })
    .filter(fn(x) -> bool { return x < 100; });
```

### ❌ DON'T: Modify Array in Predicate

```aria
// Dangerous!
filtered: []i32 = filter(numbers, fn(x: i32) -> bool {
    numbers.append(x);  // ❌ Modifying source array!
    return x > 5;
});

// Safe
filtered: []i32 = filter(numbers, fn(x: i32) -> bool {
    return x > 5;  // ✅ Pure function
});
```

---

## Related

- [map()](map.md) - Transform elements
- [reduce()](reduce.md) - Reduce to single value
- [transform()](transform.md) - General transformation

---

**Remember**: `filter()` returns **new array** with matching elements!
