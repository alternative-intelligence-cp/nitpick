# reverse()

**Category**: Standard Library → Functional Programming  
**Syntax**: `reverse(array: []T) -> []T`  
**Purpose**: Reverse array order

---

## Overview

`reverse()` creates a new array with elements in **reverse order**.

---

## Syntax

```aria
import std.functional;

reversed: []i32 = reverse(numbers);
```

---

## Parameters

- **array** (`[]T`) - Input array

---

## Returns

- `[]T` - New array with elements in reverse order

---

## Examples

### Reverse Numbers

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

reversed: []i32 = reverse(numbers);
// [5, 4, 3, 2, 1]

// Original unchanged
stdout << numbers;  // [1, 2, 3, 4, 5]
```

### Reverse Strings

```aria
words: []string = ["first", "second", "third"];

reversed: []string = reverse(words);
// ["third", "second", "first"]
```

### Reverse Characters in String

```aria
text: string = "Hello";
chars: []string = text.split("");

reversed_chars: []string = reverse(chars);
reversed_text: string = reversed_chars.join("");
// "olleH"
```

---

## Method Syntax

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

// Using method
reversed: []i32 = numbers.reverse();
```

---

## In-Place Reverse

```aria
// Reverse in-place (modifies original)
numbers: []i32 = [1, 2, 3, 4, 5];
numbers.reverse_in_place();
stdout << numbers;  // [5, 4, 3, 2, 1]
```

---

## With Chaining

```aria
Result: []i32 = numbers
    .filter(fn(x) { return x > 2; })  // [3, 4, 5]
    .reverse();                        // [5, 4, 3]
```

---

## Best Practices

### ✅ DO: Use for Reversing Lists

```aria
// Perfect use case
last_to_first: []Item = items.reverse();
```

### ❌ DON'T: Reverse Just to Access Last Element

```aria
// Bad
reversed: []i32 = numbers.reverse();
last: i32 = reversed[0];  // ❌ Inefficient

// Good
last: i32 = numbers[numbers.length() - 1];  // ✅ Direct access
```

---

## Related

- [sort()](sort.md) - Sort array
- [filter()](filter.md) - Filter elements

---

**Remember**: `reverse()` returns **new array**, original unchanged!
