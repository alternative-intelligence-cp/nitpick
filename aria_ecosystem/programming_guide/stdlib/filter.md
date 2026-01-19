# The `filter()` Function

**Category**: Standard Library → Functional Programming  
**Syntax**: `T[]:filter(T[]:array, func:(bool)(T):predicate)`  
**Purpose**: Create new array containing only elements that pass a test

---

## Overview

The `filter()` function creates a **new array** containing only the elements from the original array that satisfy a predicate function. It:

- **Does not modify** the original array
- Returns a **new array** with filtered results
- Uses a **predicate function** that returns bool
- Supports **pipeline chaining**

**Philosophy**: Functional programming pattern for clean data filtering.

---

## Basic Syntax

```aria
// Filter array with predicate
int64[]:numbers = [1, 2, 3, 4, 5, 6];

int64[]:evens = filter(numbers, n => n % 2 == 0);
// evens = [2, 4, 6]
```

---

## Example 1: Filter Even Numbers

```aria
int64[]:numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

int64[]:evens = filter(numbers, n => n % 2 == 0);
// evens = [2, 4, 6, 8, 10]

int64[]:odds = filter(numbers, n => n % 2 != 0);
// odds = [1, 3, 5, 7, 9]
```

---

## Example 2: Filter by Threshold

```aria
int64[]:scores = [45, 67, 89, 34, 92, 78, 56];

// Get passing scores (>= 60)
int64[]:passing = filter(scores, score => score >= 60);
// passing = [67, 89, 92, 78]

// Get failing scores
int64[]:failing = filter(scores, score => score < 60);
// failing = [45, 34, 56]
```

---

## Example 3: Filter Strings

```aria
string[]:words = ["apple", "banana", "cherry", "date", "elderberry"];

// Get short words (length <= 5)
string[]:short = filter(words, w => w.length <= 5);
// short = ["apple", "date"]

// Get words starting with 'b'
string[]:startsWithB = filter(words, w => w.startsWith("b"));
// startsWithB = ["banana"]
```

---

## Example 4: Filter Objects

```aria
%STRUCT Person {
    string:name,
    int64:age
}

Person[]:people = [
    { "Alice", 30 },
    { "Bob", 17 },
    { "Charlie", 25 },
    { "Diana", 16 }
];

// Get adults (age >= 18)
Person[]:adults = filter(people, p => p.age >= 18);
// adults = [Alice(30), Charlie(25)]

// Get minors
Person[]:minors = filter(people, p => p.age < 18);
// minors = [Bob(17), Diana(16)]
```

---

## Predicate Functions

### Lambda Expression

```aria
// Inline lambda
int64[]:positive = filter(numbers, n => n > 0);
```

### Named Function

```aria
func:isEven = bool(int64:n) {
    pass(n % 2 == 0);
};

int64[]:evens = filter(numbers, isEven);
```

### Multi-Line Lambda

```aria
int64[]:filtered = filter(numbers, n => {
    if (n < 0) { pass(false); }
    if (n % 2 != 0) { pass(false); }
    pass(true);
});
```

---

## Common Patterns

### Pattern 1: Remove Nulls

```aria
string?[]:maybeStrings = ["hello", NIL, "world", NIL, "test"];

string[]:nonNull = filter(maybeStrings, s => s != NIL);
// nonNull = ["hello", "world", "test"]
```

### Pattern 2: Range Filter

```aria
int64[]:values = [5, 15, 25, 35, 45, 55];

// Get values in range [20, 40]
int64[]:inRange = filter(values, v => v >= 20 && v <= 40);
// inRange = [25, 35]
```

### Pattern 3: Complex Conditions

```aria
%STRUCT Product {
    string:name,
    float64:price,
    int64:stock
}

Product[]:products = [...];

// Available and affordable
Product[]:available = filter(products, p => {
    pass(p.stock > 0 && p.price < 100.0);
});
```

### Pattern 4: Filter by Property

```aria
%STRUCT User {
    string:username,
    bool:active,
    string:role
}

User[]:users = [...];

// Get active admins
User[]:activeAdmins = filter(users, u => {
    pass(u.active && u.role == "admin");
});
```

---

## Pipeline Chaining

### Chain with Other Operations

```aria
int64[]:numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

// Filter then transform
int64[]:result = numbers
    |> filter(n => n % 2 == 0)       // [2, 4, 6, 8, 10]
    |> transform(n => n * 2);         // [4, 8, 12, 16, 20]
```

### Multiple Filters

```aria
int64[]:data = [-5, -3, -1, 0, 1, 3, 5, 7, 9, 11];

int64[]:result = data
    |> filter(n => n > 0)     // Positive only
    |> filter(n => n % 2 != 0) // Odd only
    |> filter(n => n < 10);    // Less than 10
// result = [1, 3, 5, 7, 9]
```

### Filter and Reduce

```aria
int64[]:scores = [45, 67, 89, 34, 92, 78, 56];

// Average of passing scores
float64:avg = scores
    |> filter(s => s >= 60)
    |> reduce(0, (sum, s) => sum + s)
    / filter(scores, s => s >= 60).length;
```

---

## Performance Considerations

### Creates New Array

```aria
// Original unchanged
int64[]:original = [1, 2, 3, 4, 5];
int64[]:filtered = filter(original, n => n > 2);

// original still [1, 2, 3, 4, 5]
// filtered is [3, 4, 5]
```

### Memory Allocation

```aria
// Each filter creates new array
int64[]:step1 = filter(data, predicate1);  // Allocation 1
int64[]:step2 = filter(step1, predicate2); // Allocation 2
int64[]:step3 = filter(step2, predicate3); // Allocation 3

// Consider combining predicates
int64[]:result = filter(data, x => {
    pass(predicate1(x) && predicate2(x) && predicate3(x));
});  // Single allocation
```

---

## Best Practices

### ✅ Use for Declarative Filtering

```aria
// GOOD: Clear intent
int64[]:adults = filter(ages, age => age >= 18);
```

### ✅ Chain with Pipelines

```aria
// GOOD: Functional composition
data
    |> filter(isValid)
    |> transform(normalize)
    |> reduce(0, sum);
```

### ✅ Use Named Predicates for Reusability

```aria
// GOOD: Reusable predicate
func:isPositive = bool(int64:n) { pass(n > 0); };

int64[]:pos1 = filter(array1, isPositive);
int64[]:pos2 = filter(array2, isPositive);
```

### ❌ Don't Modify Array in Predicate

```aria
// BAD: Side effects in predicate
int64:count = 0;
filter(numbers, n => {
    count++;  // ⚠️ Side effect!
    pass(n > 0);
});

// GOOD: Pure predicate
filter(numbers, n => n > 0);
```

### ❌ Don't Overuse Chaining

```aria
// BAD: Too many allocations
result = data
    |> filter(p1)
    |> filter(p2)
    |> filter(p3)
    |> filter(p4);

// GOOD: Combine predicates
result = filter(data, x => p1(x) && p2(x) && p3(x) && p4(x));
```

---

## Comparison with Other Languages

### Aria

```aria
int64[]:evens = filter(numbers, n => n % 2 == 0);
```

### JavaScript

```javascript
const evens = numbers.filter(n => n % 2 === 0);
```

### Python

```python
evens = list(filter(lambda n: n % 2 == 0, numbers))
# or
evens = [n for n in numbers if n % 2 == 0]
```

### Rust

```rust
let evens: Vec<i64> = numbers.iter()
    .filter(|&&n| n % 2 == 0)
    .cloned()
    .collect();
```

---

## Advanced Examples

### Example 1: Multi-Criteria Filter

```aria
%STRUCT Employee {
    string:name,
    string:department,
    int64:salary,
    int64:experience
}

Employee[]:employees = [...];

// Senior engineers earning > 100k
Employee[]:seniorEngineers = filter(employees, e => {
    pass(
        e.department == "Engineering" &&
        e.experience >= 5 &&
        e.salary > 100000
    );
});
```

### Example 2: Dynamic Filtering

```aria
func:createFilter = func:(bool)(int64)(int64:threshold) {
    pass(n => n > threshold);
};

int64[]:numbers = [5, 10, 15, 20, 25];

func:(bool)(int64):filter10 = createFilter(10);
int64[]:above10 = filter(numbers, filter10);
// [15, 20, 25]

func:(bool)(int64):filter20 = createFilter(20);
int64[]:above20 = filter(numbers, filter20);
// [25]
```

### Example 3: Nested Filtering

```aria
%STRUCT Team {
    string:name,
    Person[]:members
}

Team[]:teams = [...];

// Teams with at least one adult member
Team[]:teamsWithAdults = filter(teams, team => {
    Person[]:adults = filter(team.members, p => p.age >= 18);
    pass(adults.length > 0);
});
```

### Example 4: Filter with Index (Custom)

```aria
func:filterWithIndex = T[](T[]:array, func:(bool)(T, int64):predicate) {
    T[]:result = [];
    
    till(array.length - 1, 1) {
        if (predicate(array[$], $)) {
            result.append(array[$]);
        }
    }
    
    pass(result);
};

// Get every other element
int64[]:everyOther = filterWithIndex(numbers, (n, i) => i % 2 == 0);
```

---

## Error Handling

### Empty Array Result

```aria
int64[]:numbers = [1, 3, 5, 7, 9];

int64[]:evens = filter(numbers, n => n % 2 == 0);
// evens = [] (empty array)

if (evens.length == 0) {
    print("No even numbers found");
}
```

### All Elements Match

```aria
int64[]:numbers = [2, 4, 6, 8];

int64[]:evens = filter(numbers, n => n % 2 == 0);
// evens = [2, 4, 6, 8] (all match)
```

---

## Related Topics

- [transform()](transform.md) - Map function for transforming elements
- [reduce()](reduce.md) - Fold/reduce operation
- [Pipeline Operator](../operators/pipeline.md) - |> and <| for chaining
- [Lambda Functions](../functions/lambdas.md) - Anonymous functions
- [Arrays](../types/arrays.md) - Array type documentation

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Line 236  
**Unique Feature**: Pure functional filtering with pipeline support
