# reduce()

**Category**: Standard Library → Functional Programming  
**Syntax**: `reduce(array: []T, initial: U, reducer: fn(U, T) -> U) -> U`  
**Purpose**: Reduce array to single value

---

## Overview

`reduce()` (also called fold) combines all array elements into a single value using a reducer function.

---

## Syntax

```aria
import std.functional;

sum: i32 = reduce(numbers, 0, fn(acc: i32, x: i32) -> i32 {
    return acc + x;
});
```

---

## Parameters

- **array** (`[]T`) - Input array
- **initial** (`U`) - Initial accumulator value
- **reducer** (`fn(U, T) -> U`) - Function combining accumulator with each element

---

## Returns

- `U` - Final accumulated value

---

## Examples

### Sum Numbers

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

sum: i32 = reduce(numbers, 0, fn(acc: i32, x: i32) -> i32 {
    return acc + x;
});
// 15
```

### Product

```aria
numbers: []i32 = [2, 3, 4, 5];

product: i32 = reduce(numbers, 1, fn(acc: i32, x: i32) -> i32 {
    return acc * x;
});
// 120
```

### Find Maximum

```aria
numbers: []i32 = [3, 7, 2, 9, 1, 5];

max: i32 = reduce(numbers, numbers[0], fn(acc: i32, x: i32) -> i32 {
    return when x > acc then x else acc;
});
// 9
```

### Concatenate Strings

```aria
words: []string = ["Hello", " ", "World", "!"];

sentence: string = reduce(words, "", fn(acc: string, word: string) -> string {
    return acc + word;
});
// "Hello World!"
```

### Count Occurrences

```aria
items: []string = ["apple", "banana", "apple", "cherry", "banana", "apple"];

counts: map<string, i32> = reduce(items, {}, fn(acc: map<string, i32>, item: string) -> map<string, i32> {
    acc[item] = acc.get(item, 0) + 1;
    return acc;
});
// {"apple": 3, "banana": 2, "cherry": 1}
```

### Group By

```aria
struct User {
    name: string,
    age: i32
}

users: []User = [
    {name = "Alice", age = 30},
    {name = "Bob", age = 25},
    {name = "Charlie", age = 30}
];

by_age: map<i32, []User> = reduce(users, {}, fn(acc, user) {
    when not acc.has_key(user.age) then
        acc[user.age] = [];
    end
    acc[user.age].append(user);
    return acc;
});
// {30: [Alice, Charlie], 25: [Bob]}
```

---

## Method Syntax

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

sum: i32 = numbers.reduce(0, fn(acc, x) -> i32 {
    return acc + x;
});
```

---

## Common Patterns

### Sum

```aria
sum: i32 = numbers.reduce(0, fn(acc, x) { return acc + x; });
```

### Average

```aria
sum: i32 = numbers.reduce(0, fn(acc, x) { return acc + x; });
average: flt64 = sum / numbers.length();
```

### Flatten Array

```aria
nested: [][]i32 = [[1, 2], [3, 4], [5, 6]];

flat: []i32 = nested.reduce([], fn(acc: []i32, arr: []i32) -> []i32 {
    return acc.concat(arr);
});
// [1, 2, 3, 4, 5, 6]
```

---

## Best Practices

### ✅ DO: Use for Aggregation

```aria
// Perfect use case
total: i32 = prices.reduce(0, fn(sum, price) {
    return sum + price;
});
```

### ✅ DO: Choose Good Initial Value

```aria
// Sum - start at 0
sum: i32 = numbers.reduce(0, add);

// Product - start at 1
product: i32 = numbers.reduce(1, multiply);

// Build object - start with empty
Result: obj = items.reduce({}, build_object);
```

### ❌ DON'T: Mutate Accumulator Unsafely

```aria
// Risky if accumulator is shared
Result: []i32 = numbers.reduce(shared_array, fn(acc, x) {
    acc.append(x);  // ⚠️ Mutating shared state
    return acc;
});

// Safer
Result: []i32 = numbers.reduce([], fn(acc, x) {
    new_acc: []i32 = acc.clone();
    new_acc.append(x);
    return new_acc;
});
```

---

## Related

- [filter()](filter.md) - Filter elements
- [map()](map.md) - Transform elements
- [functional_programming](functional_programming.md) - Overview

---

**Remember**: `reduce()` combines all elements into **one value**!
