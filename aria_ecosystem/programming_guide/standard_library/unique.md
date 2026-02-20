# unique()

**Category**: Standard Library → Functional Programming  
**Syntax**: `unique(array: []T) -> []T`  
**Purpose**: Remove duplicate elements

---

## Overview

`unique()` returns a new array with duplicate elements removed, keeping first occurrence.

---

## Syntax

```aria
import std.functional;

deduped: []i32 = unique(numbers);
```

---

## Parameters

- **array** (`[]T`) - Input array with possible duplicates

---

## Returns

- `[]T` - New array with duplicates removed

---

## Examples

### Remove Duplicate Numbers

```aria
numbers: []i32 = [1, 2, 3, 2, 4, 1, 5, 3];

deduped: []i32 = unique(numbers);
// [1, 2, 3, 4, 5]
```

### Remove Duplicate Strings

```aria
words: []string = ["apple", "banana", "apple", "cherry", "banana"];

deduped: []string = unique(words);
// ["apple", "banana", "cherry"]
```

### Preserve Order

```aria
// First occurrence is kept
items: []i32 = [5, 2, 3, 2, 1, 3, 5];

deduped: []i32 = unique(items);
// [5, 2, 3, 1]  (order of first occurrences preserved)
```

---

## Method Syntax

```aria
numbers: []i32 = [1, 2, 2, 3, 3, 3];

deduped: []i32 = numbers.unique();
```

---

## With Chaining

```aria
Result: []i32 = numbers
    .filter(fn(x) { return x > 0; })
    .map(fn(x) { return x % 10; })
    .unique();  // Remove duplicates from transformed values
```

---

## Count Unique Elements

```aria
numbers: []i32 = [1, 2, 3, 2, 1, 4, 5, 4];

unique_count: i32 = unique(numbers).length();
// 5 (unique values: 1, 2, 3, 4, 5)
```

---

## Custom Equality

```aria
struct Person {
    id: i32,
    name: string
}

// Remove duplicates by ID
people: []Person = [...];

unique_people: []Person = unique_by(people, fn(p: Person) -> i32 {
    return p.id;  // Compare by ID
});
```

---

## Best Practices

### ✅ DO: Use for Deduplication

```aria
// Perfect use case
unique_tags: []string = unique(all_tags);
```

### ✅ DO: Combine with Sort

```aria
// Unique and sorted
Result: []i32 = unique(numbers).sort();
```

### ❌ DON'T: Assume Sorted Output

```aria
deduped: []i32 = unique([5, 2, 3, 2, 1]);
// [5, 2, 3, 1]  - NOT sorted!

// If you want sorted unique:
Result: []i32 = unique(numbers).sort();  // ✅
```

---

## Implementation Note

```aria
// Typical implementation keeps first occurrence
fn unique(arr: []T) -> []T {
    seen: set<T> = {};
    Result: []T = [];
    
    till(arr.length - 1, 1) {
        when not seen.contains(arr[$]) then
            result.append(arr[$]);
            seen.insert(arr[$]);
        end
    end
    
    return result;
}
```

---

## Related

- [filter()](filter.md) - Filter elements
- [sort()](sort.md) - Sort array
- [Set](../collections/set.md) - Unique collection type

---

**Remember**: `unique()` keeps **first occurrence** of each element!
