# The `reduce()` Function

**Category**: Standard Library → Functional Programming  
**Syntax**: `U:reduce(T[]:array, U:initial, func:(U)(U, T):reducer)`  
**Purpose**: Reduce array to single value by accumulating results

---

## Overview

The `reduce()` function (also known as **fold**) reduces an array to a **single value** by applying an accumulator function. It:

- **Processes elements left-to-right**
- Uses an **initial accumulator value**
- Returns a **single result** (can be any type)
- Common for **sums, products, aggregations**

**Philosophy**: Functional programming pattern for aggregation and accumulation.

---

## Basic Syntax

```aria
// Reduce array to single value
int64[]:numbers = [1, 2, 3, 4, 5];

int64:sum = reduce(numbers, 0, (acc, n) => acc + n);
// sum = 15
```

---

## Example 1: Sum Array

```aria
int64[]:numbers = [1, 2, 3, 4, 5];

int64:sum = reduce(numbers, 0, (acc, n) => acc + n);
// sum = 1 + 2 + 3 + 4 + 5 = 15

// Step by step:
// acc=0,  n=1  →  0 + 1 = 1
// acc=1,  n=2  →  1 + 2 = 3
// acc=3,  n=3  →  3 + 3 = 6
// acc=6,  n=4  →  6 + 4 = 10
// acc=10, n=5  →  10 + 5 = 15
```

---

## Example 2: Product

```aria
int64[]:numbers = [2, 3, 4, 5];

int64:product = reduce(numbers, 1, (acc, n) => acc * n);
// product = 2 * 3 * 4 * 5 = 120
```

---

## Example 3: String Concatenation

```aria
string[]:words = ["Hello", "World", "Aria"];

string:sentence = reduce(words, "", (acc, w) => {
    pass(is acc == "" : w : `&{acc} &{w}`);
});
// sentence = "Hello World Aria"
```

---

## Example 4: Find Maximum

```aria
int64[]:numbers = [45, 67, 23, 89, 34];

int64:max = reduce(numbers, numbers[0], (acc, n) => {
    pass(is n > acc : n : acc);
});
// max = 89
```

---

## Reducer Functions

### Lambda Expression

```aria
// Inline lambda
int64:sum = reduce(numbers, 0, (acc, n) => acc + n);
```

### Named Function

```aria
func:add = int64(int64:acc, int64:n) {
    pass(acc + n);
};

int64:sum = reduce(numbers, 0, add);
```

### Multi-Line Lambda

```aria
int64:result = reduce(numbers, 0, (acc, n) => {
    if (n < 0) { pass(acc); }  // Skip negatives
    pass(acc + n);
});
```

---

## Common Patterns

### Pattern 1: Count Occurrences

```aria
string[]:words = ["apple", "banana", "apple", "cherry", "apple"];

int64:appleCount = reduce(words, 0, (count, w) => {
    pass(is w == "apple" : count + 1 : count);
});
// appleCount = 3
```

### Pattern 2: Build Object from Array

```aria
%STRUCT Stats {
    int64:count,
    int64:sum,
    float64:average
}

int64[]:numbers = [10, 20, 30, 40, 50];

Stats:stats = reduce(numbers, { 0, 0, 0.0 }, (s, n) => {
    int64:newCount = s.count + 1;
    int64:newSum = s.sum + n;
    float64:newAvg = newSum as float64 / newCount as float64;
    pass({ newCount, newSum, newAvg });
});
// stats = { count: 5, sum: 150, average: 30.0 }
```

### Pattern 3: Flatten Nested Arrays

```aria
int64[][]:nested = [[1, 2], [3, 4], [5, 6]];

int64[]:flat = reduce(nested, [], (acc, arr) => {
    till(arr.length - 1, 1) {
        acc.append(arr[$]);
    }
    pass(acc);
});
// flat = [1, 2, 3, 4, 5, 6]
```

### Pattern 4: Group By Property

```aria
%STRUCT Person {
    string:name,
    string:department
}

Person[]:people = [
    { "Alice", "Engineering" },
    { "Bob", "Sales" },
    { "Charlie", "Engineering" }
];

// Count by department
map[string, int64]:deptCounts = reduce(people, {}, (counts, p) => {
    int64:current = counts.get(p.department).unwrapOr(0);
    counts.set(p.department, current + 1);
    pass(counts);
});
// { "Engineering": 2, "Sales": 1 }
```

---

## Pipeline Chaining

### Chain with Filter and Transform

```aria
int64[]:numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

int64:sum = numbers
    |> filter(n => n % 2 == 0)       // [2, 4, 6, 8, 10]
    |> transform(n => n * n)         // [4, 16, 36, 64, 100]
    |> reduce(0, (acc, n) => acc + n); // 220
```

### Complex Pipeline

```aria
%STRUCT Product {
    string:name,
    float64:price,
    int64:quantity
}

Product[]:inventory = [...];

float64:totalValue = inventory
    |> filter(p => p.quantity > 0)           // In stock only
    |> transform(p => p.price * p.quantity)  // Calculate values
    |> reduce(0.0, (acc, val) => acc + val);  // Sum total
```

---

## Type Transformation

### Same Type (T → T)

```aria
// int64[] → int64
int64:sum = reduce(numbers, 0, (acc, n) => acc + n);
```

### Different Type (T → U)

```aria
// int64[] → string
string:concatenated = reduce(numbers, "", (acc, n) => {
    pass(`&{acc}&{n.toString()}`);
});

// Person[] → map[string, int64]
map[string, int64]:counts = reduce(people, {}, (m, p) => {
    // Build map from array
});
```

---

## Initial Value Importance

### Correct Initial Value

```aria
// Sum: use 0
int64:sum = reduce(numbers, 0, (acc, n) => acc + n);

// Product: use 1
int64:product = reduce(numbers, 1, (acc, n) => acc * n);

// String concat: use ""
string:text = reduce(words, "", (acc, w) => `&{acc} &{w}`);

// Array building: use []
int64[]:array = reduce(data, [], (acc, item) => {
    acc.append(item);
    pass(acc);
});
```

### Wrong Initial Value

```aria
// WRONG: Using 1 for sum
int64:sum = reduce(numbers, 1, (acc, n) => acc + n);
// Result off by 1!

// WRONG: Using 0 for product
int64:product = reduce(numbers, 0, (acc, n) => acc * n);
// Result is always 0!
```

---

## Best Practices

### ✅ Use for Aggregation

```aria
// GOOD: Accumulate values
int64:total = reduce(prices, 0, (sum, p) => sum + p);
```

### ✅ Choose Correct Initial Value

```aria
// GOOD: 0 for sum, 1 for product
int64:sum = reduce(numbers, 0, add);
int64:product = reduce(numbers, 1, multiply);
```

### ✅ Keep Reducer Pure

```aria
// GOOD: Pure function
reduce(numbers, 0, (acc, n) => acc + n);

// BAD: Side effects
int64:external = 0;
reduce(numbers, 0, (acc, n) => {
    external += n;  // ⚠️ Side effect!
    pass(acc + n);
});
```

### ❌ Don't Use for Simple Iteration

```aria
// BAD: Reduce for iteration only
reduce(numbers, NIL, (_, n) => {
    print(n);
    pass(NIL);
});

// GOOD: Use loop
till(numbers.length - 1, 1) {
    print(numbers[$]);
}
```

### ❌ Don't Mutate Accumulator (Usually)

```aria
// RISKY: Mutating accumulator
int64[]:result = reduce(arrays, [], (acc, arr) => {
    acc.append(arr);  // Mutation
    pass(acc);
});

// SAFER: Create new accumulator each time
int64[]:result = reduce(arrays, [], (acc, arr) => {
    int64[]:newAcc = acc.clone();
    newAcc.append(arr);
    pass(newAcc);
});
```

---

## Comparison with Other Languages

### Aria

```aria
int64:sum = reduce(numbers, 0, (acc, n) => acc + n);
```

### JavaScript

```javascript
const sum = numbers.reduce((acc, n) => acc + n, 0);
```

### Python

```python
from functools import reduce
sum = reduce(lambda acc, n: acc + n, numbers, 0)
# or
sum = sum(numbers)
```

### Rust

```rust
let sum: i64 = numbers.iter().fold(0, |acc, &n| acc + n);
```

---

## Advanced Examples

### Example 1: Running Statistics

```aria
%STRUCT RunningStats {
    int64:count,
    int64:sum,
    int64:min,
    int64:max
}

int64[]:data = [45, 67, 23, 89, 34, 12, 78];

RunningStats:stats = reduce(data, { 0, 0, INT64_MAX, INT64_MIN }, (s, n) => {
    pass({
        count: s.count + 1,
        sum: s.sum + n,
        min: is n < s.min : n : s.min,
        max: is n > s.max : n : s.max
    });
});

float64:average = stats.sum as float64 / stats.count as float64;
```

### Example 2: Build Histogram

```aria
int64[]:scores = [1, 2, 2, 3, 3, 3, 4, 4, 5];

map[int64, int64]:histogram = reduce(scores, {}, (hist, score) => {
    int64:count = hist.get(score).unwrapOr(0);
    hist.set(score, count + 1);
    pass(hist);
});
// { 1: 1, 2: 2, 3: 3, 4: 2, 5: 1 }
```

### Example 3: Reverse Array

```aria
int64[]:numbers = [1, 2, 3, 4, 5];

int64[]:reversed = reduce(numbers, [], (acc, n) => {
    int64[]:newAcc = [n];
    till(acc.length - 1, 1) {
        newAcc.append(acc[$]);
    }
    pass(newAcc);
});
// reversed = [5, 4, 3, 2, 1]
```

### Example 4: Compose Functions

```aria
func:(int64)(int64)[]:functions = [
    n => n + 1,
    n => n * 2,
    n => n * n
];

func:(int64)(int64):composed = reduce(functions, n => n, (f, g) => {
    pass(n => f(g(n)));
});

int64:result = composed(5);
// 5 → (5+1)=6 → (6*2)=12 → (12*12)=144
```

---

## Error Handling

### Empty Array

```aria
int64[]:empty = [];

// Need safe initial value
int64:sum = reduce(empty, 0, (acc, n) => acc + n);
// sum = 0 (safe with empty array)

// Dangerous with wrong initial
int64:max = reduce(empty, INT64_MIN, (acc, n) => {
    pass(is n > acc : n : acc);
});
// max = INT64_MIN (no elements to compare)
```

### Use with Result Type

```aria
Result[int64][]:results = [Result.ok(1), Result.ok(2), Result.err("oops")];

Result[int64]:sumResult = reduce(results, Result.ok(0), (accRes, r) => {
    if (accRes.isErr()) { pass(accRes); }
    if (r.isErr()) { pass(r); }
    
    int64:sum = accRes.unwrap() + r.unwrap();
    pass(Result.ok(sum));
});
```

---

## Performance

### Single Pass

```aria
// Efficient: Single pass through array
int64:total = reduce(numbers, 0, (acc, n) => acc + n);
```

### Avoid Expensive Operations in Reducer

```aria
// BAD: Expensive operation per element
string:result = reduce(words, "", (acc, w) => {
    pass(acc.clone() + w);  // Clone on each iteration!
});

// BETTER: Use mutable accumulator (if safe)
string:result = reduce(words, "", (acc, w) => {
    acc += w;  // In-place append
    pass(acc);
});
```

---

## Related Topics

- [filter()](filter.md) - Filter array elements
- [transform()](transform.md) - Map function for transforming
- [Pipeline Operator](../operators/pipeline.md) - |> and <| for chaining
- [Lambda Functions](../functions/lambdas.md) - Anonymous functions
- [Arrays](../types/arrays.md) - Array type documentation

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Line 238  
**Unique Feature**: Type-flexible reduction with pipeline support
