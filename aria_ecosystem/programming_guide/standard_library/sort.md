# sort()

**Category**: Standard Library → Functional Programming  
**Syntax**: `sort(array: []T) -> []T` or `sort(array: []T, comparator: fn(T, T) -> i32) -> []T`  
**Purpose**: Sort array elements

---

## Overview

`sort()` returns a new sorted array. Can use natural ordering or custom comparator.

---

## Syntax

```aria
import std.functional;

// Natural sort
sorted: []i32 = sort(numbers);

// Custom sort
sorted: []i32 = sort(numbers, fn(a: i32, b: i32) -> i32 {
    return a - b;  // Ascending
});
```

---

## Parameters

- **array** (`[]T`) - Input array
- **comparator** (`fn(T, T) -> i32`) - Optional comparison function
  - Return negative if a < b
  - Return 0 if a == b
  - Return positive if a > b

---

## Returns

- `[]T` - New sorted array

---

## Examples

### Sort Numbers (Ascending)

```aria
numbers: []i32 = [5, 2, 8, 1, 9, 3];

sorted: []i32 = sort(numbers);
// [1, 2, 3, 5, 8, 9]
```

### Sort Numbers (Descending)

```aria
numbers: []i32 = [5, 2, 8, 1, 9, 3];

sorted: []i32 = sort(numbers, fn(a: i32, b: i32) -> i32 {
    return b - a;  // Descending
});
// [9, 8, 5, 3, 2, 1]
```

### Sort Strings

```aria
words: []string = ["banana", "apple", "cherry", "date"];

sorted: []string = sort(words);
// ["apple", "banana", "cherry", "date"]
```

### Sort by Length

```aria
words: []string = ["elephant", "cat", "dog", "butterfly"];

by_length: []string = sort(words, fn(a: string, b: string) -> i32 {
    return a.length() - b.length();
});
// ["cat", "dog", "elephant", "butterfly"]
```

### Sort Structs

```aria
struct Person {
    name: string,
    age: i32
}

people: []Person = [
    {name = "Alice", age = 30},
    {name = "Bob", age = 25},
    {name = "Charlie", age = 35}
];

// Sort by age
by_age: []Person = sort(people, fn(a: Person, b: Person) -> i32 {
    return a.age - b.age;
});
// [Bob(25), Alice(30), Charlie(35)]

// Sort by name
by_name: []Person = sort(people, fn(a: Person, b: Person) -> i32 {
    when a.name < b.name then return -1;
    when a.name > b.name then return 1;
    return 0;
});
// [Alice, Bob, Charlie]
```

---

## Method Syntax

```aria
numbers: []i32 = [5, 2, 8, 1, 9];

// Natural sort
sorted: []i32 = numbers.sort();

// Custom comparator
sorted: []i32 = numbers.sort(fn(a, b) { return b - a; });
```

---

## In-Place Sort

```aria
// Sort in-place (modifies original)
numbers: []i32 = [5, 2, 8, 1, 9];
numbers.sort_in_place();
stdout << numbers;  // [1, 2, 5, 8, 9]
```

---

## Multi-Level Sorting

```aria
struct User {
    name: string,
    age: i32,
    score: i32
}

users: []User = [...];

// Sort by score descending, then by age ascending
sorted: []User = sort(users, fn(a: User, b: User) -> i32 {
    // First compare scores (descending)
    when a.score != b.score then
        return b.score - a.score;
    end
    // If scores equal, compare ages (ascending)
    return a.age - b.age;
});
```

---

## Best Practices

### ✅ DO: Use Natural Sort When Possible

```aria
numbers: []i32 = [3, 1, 2];
sorted: []i32 = sort(numbers);  // ✅ Simple
```

### ✅ DO: Use Comparator for Complex Sorting

```aria
sorted: []Person = sort(people, fn(a, b) {
    return a.last_name.compare(b.last_name);
});
```

### ❌ DON'T: Mutate Elements in Comparator

```aria
// Dangerous!
sorted: []Item = sort(items, fn(a, b) {
    a.accessed = true;  // ❌ Side effects!
    return a.value - b.value;
});
```

---

## Stability

Aria's sort is **stable** - equal elements maintain their relative order:

```aria
struct Item {
    value: i32,
    id: string
}

items: []Item = [
    {value = 1, id = "A"},
    {value = 2, id = "B"},
    {value = 1, id = "C"}
];

sorted: []Item = sort(items, fn(a, b) { return a.value - b.value; });
// [{value=1, id="A"}, {value=1, id="C"}, {value=2, id="B"}]
// A comes before C (both have value=1) - order preserved
```

---

## Related

- [reverse()](reverse.md) - Reverse array
- [filter()](filter.md) - Filter elements
- [unique()](unique.md) - Remove duplicates

---

**Remember**: `sort()` returns **new array**, original unchanged!
