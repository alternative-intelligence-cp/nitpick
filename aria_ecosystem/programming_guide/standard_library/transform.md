# transform()

**Category**: Standard Library → Functional Programming  
**Syntax**: `transform(array: []T, transformer: fn(T) -> U) -> []U`  
**Purpose**: Transform array elements (alias for map)

---

## Overview

`transform()` applies a function to each element, producing a new array. **Same as `map()`**.

---

## Syntax

```aria
import std.functional;

doubled: []i32 = transform(numbers, fn(x: i32) -> i32 {
    return x * 2;
});
```

---

## Parameters

- **array** (`[]T`) - Input array
- **transformer** (`fn(T) -> U`) - Function transforming each element

---

## Returns

- `[]U` - New array with transformed elements

---

## Examples

### Double Numbers

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

doubled: []i32 = transform(numbers, fn(x: i32) -> i32 {
    return x * 2;
});
// [2, 4, 6, 8, 10]
```

### Convert Types

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

strings: []string = transform(numbers, fn(x: i32) -> string {
    return to_string(x);
});
// ["1", "2", "3", "4", "5"]
```

### Extract Fields

```aria
struct User {
    id: i32,
    name: string
}

users: []User = [...];

names: []string = transform(users, fn(u: User) -> string {
    return u.name;
});
```

### Nested Transformation

```aria
struct Point {
    x: i32,
    y: i32
}

points: []Point = [{x=1, y=2}, {x=3, y=4}];

x_coords: []i32 = transform(points, fn(p: Point) -> i32 {
    return p.x;
});
// [1, 3]
```

---

## vs map()

```aria
// These are identical:
result1: []i32 = transform(numbers, double);
result2: []i32 = map(numbers, double);
```

**Note**: `transform()` is just a **descriptive alias** for `map()`.

---

## Method Syntax

```aria
numbers: []i32 = [1, 2, 3];

// Using method
doubled: []i32 = numbers.transform(fn(x) { return x * 2; });

// Same as
doubled: []i32 = numbers.map(fn(x) { return x * 2; });
```

---

## Chaining

```aria
Result: []i32 = numbers
    .transform(fn(x) { return x + 1; })    // Add 1
    .transform(fn(x) { return x * 2; })    // Double
    .transform(fn(x) { return x - 5; });   // Subtract 5
```

---

## Related

- [map()](../array/map.md) - Same functionality (preferred name)
- [filter()](filter.md) - Filter elements
- [reduce()](reduce.md) - Reduce to single value

---

**Remember**: `transform()` is an **alias for map()**!
