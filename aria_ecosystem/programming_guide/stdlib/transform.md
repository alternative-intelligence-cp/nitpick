# The `transform()` Function

**Category**: Standard Library → Functional Programming  
**Syntax**: `U[]:transform(T[]:array, func:(U)(T):mapper)`  
**Purpose**: Create new array by applying function to each element

---

## Overview

The `transform()` function (also known as **map**) creates a **new array** by applying a transformation function to each element. It:

- **Does not modify** the original array
- Returns a **new array** with transformed results
- Can **change element type** (T → U)
- Supports **pipeline chaining**

**Philosophy**: Functional programming pattern for clean data transformation.

---

## Basic Syntax

```aria
// Transform array with mapper function
int64[]:numbers = [1, 2, 3, 4, 5];

int64[]:doubled = transform(numbers, n => n * 2);
// doubled = [2, 4, 6, 8, 10]
```

---

## Example 1: Multiply Elements

```aria
int64[]:numbers = [1, 2, 3, 4, 5];

int64[]:doubled = transform(numbers, n => n * 2);
// [2, 4, 6, 8, 10]

int64[]:squared = transform(numbers, n => n * n);
// [1, 4, 9, 16, 25]

int64[]:plusTen = transform(numbers, n => n + 10);
// [11, 12, 13, 14, 15]
```

---

## Example 2: Type Conversion

```aria
// int64 to string
int64[]:numbers = [1, 2, 3];
string[]:strings = transform(numbers, n => n.toString());
// ["1", "2", "3"]

// string to int64
string[]:digits = ["10", "20", "30"];
int64[]:values = transform(digits, s => s.toInt64());
// [10, 20, 30]
```

---

## Example 3: String Operations

```aria
string[]:words = ["hello", "world", "aria"];

// To uppercase
string[]:upper = transform(words, w => w.toUpperCase());
// ["HELLO", "WORLD", "ARIA"]

// Get lengths
int64[]:lengths = transform(words, w => w.length);
// [5, 5, 4]

// Reverse strings
string[]:reversed = transform(words, w => w.reverse());
// ["olleh", "dlrow", "aira"]
```

---

## Example 4: Extract Properties

```aria
%STRUCT Person {
    string:name,
    int64:age
}

Person[]:people = [
    { "Alice", 30 },
    { "Bob", 25 },
    { "Charlie", 35 }
];

// Extract names
string[]:names = transform(people, p => p.name);
// ["Alice", "Bob", "Charlie"]

// Extract ages
int64[]:ages = transform(people, p => p.age);
// [30, 25, 35]
```

---

## Mapper Functions

### Lambda Expression

```aria
// Inline lambda
int64[]:doubled = transform(numbers, n => n * 2);
```

### Named Function

```aria
func:double = int64(int64:n) {
    pass(n * 2);
};

int64[]:result = transform(numbers, double);
```

### Multi-Line Lambda

```aria
string[]:formatted = transform(numbers, n => {
    string:prefix = is n < 0 : "NEG" : "POS";
    string:absStr = abs(n).toString();
    pass(`&{prefix}_&{absStr}`);
});
```

---

## Common Patterns

### Pattern 1: Normalize Data

```aria
float64[]:scores = [45.0, 67.0, 89.0, 92.0];

// Normalize to 0-1 range
float64:max = 100.0;
float64[]:normalized = transform(scores, s => s / max);
// [0.45, 0.67, 0.89, 0.92]
```

### Pattern 2: Format Output

```aria
int64[]:prices = [1000, 2500, 750];

// Format as currency
string[]:formatted = transform(prices, p => {
    pass(`$&{p / 100}.&{p % 100}`);
});
// ["$10.00", "$25.00", "$7.50"]
```

### Pattern 3: Compute Derived Values

```aria
int64[]:widths = [10, 20, 30];
int64[]:heights = [5, 10, 15];

// Compute areas (assuming same length)
int64[]:areas = transform(widths, w => {
    int64:index = indexOf(widths, w);
    pass(w * heights[index]);
});
```

### Pattern 4: Object Transformation

```aria
%STRUCT User {
    string:username,
    string:email
}

%STRUCT UserView {
    string:display,
    string:contact
}

User[]:users = [...];

UserView[]:views = transform(users, u => {
    pass({ u.username.toUpperCase(), u.email });
});
```

---

## Pipeline Chaining

### Chain with Other Operations

```aria
int64[]:numbers = [1, 2, 3, 4, 5];

// Transform then filter
int64[]:result = numbers
    |> transform(n => n * 2)        // [2, 4, 6, 8, 10]
    |> filter(n => n > 5);          // [6, 8, 10]
```

### Multiple Transformations

```aria
string[]:words = ["hello", "world"];

string[]:result = words
    |> transform(w => w.toUpperCase())  // ["HELLO", "WORLD"]
    |> transform(w => w.reverse())      // ["OLLEH", "DLROW"]
    |> transform(w => `<&{w}>`);        // ["<OLLEH>", "<DLROW>"]
```

### Filter, Transform, Reduce

```aria
int64[]:numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

int64:sum = numbers
    |> filter(n => n % 2 == 0)       // [2, 4, 6, 8, 10]
    |> transform(n => n * n)         // [4, 16, 36, 64, 100]
    |> reduce(0, (acc, n) => acc + n); // 220
```

---

## Type Transformation

### Same Type

```aria
// int64 → int64
int64[]:result = transform(numbers, n => n + 1);
```

### Different Type

```aria
// int64 → string
string[]:result = transform(numbers, n => n.toString());

// string → int64
int64[]:result = transform(strings, s => s.length);

// Person → string
string[]:result = transform(people, p => p.name);
```

---

## Performance Considerations

### Creates New Array

```aria
// Original unchanged
int64[]:original = [1, 2, 3];
int64[]:transformed = transform(original, n => n * 2);

// original still [1, 2, 3]
// transformed is [2, 4, 6]
```

### Memory Allocation

```aria
// Each transform creates new array
int64[]:step1 = transform(data, f1);  // Allocation 1
int64[]:step2 = transform(step1, f2); // Allocation 2
int64[]:step3 = transform(step2, f3); // Allocation 3

// Consider composing functions
func:composed = int64(int64:n) {
    pass(f3(f2(f1(n))));
};
int64[]:result = transform(data, composed);  // Single allocation
```

---

## Best Practices

### ✅ Use for Data Transformation

```aria
// GOOD: Clear transformation
string[]:names = transform(people, p => p.name);
```

### ✅ Chain with Pipelines

```aria
// GOOD: Functional composition
result = data
    |> filter(isValid)
    |> transform(normalize)
    |> transform(format);
```

### ✅ Keep Mappers Pure

```aria
// GOOD: Pure function, no side effects
transform(numbers, n => n * 2);

// BAD: Side effects
int64:total = 0;
transform(numbers, n => {
    total += n;  // ⚠️ Side effect!
    pass(n * 2);
});
```

### ❌ Don't Use for Iteration Only

```aria
// BAD: transform for side effects only
transform(users, u => {
    print(u.name);
    pass(u);  // Returning unchanged
});

// GOOD: Use loop for side effects
till(users.length - 1, 1) {
    print(users[$].name);
}
```

### ❌ Don't Create Unnecessary Copies

```aria
// BAD: Unnecessary transformation
int64[]:copy = transform(numbers, n => n);

// GOOD: Direct assignment or clone
int64[]:copy = numbers.clone();
```

---

## Comparison with Other Languages

### Aria

```aria
int64[]:doubled = transform(numbers, n => n * 2);
```

### JavaScript

```javascript
const doubled = numbers.map(n => n * 2);
```

### Python

```python
doubled = list(map(lambda n: n * 2, numbers))
# or
doubled = [n * 2 for n in numbers]
```

### Rust

```rust
let doubled: Vec<i64> = numbers.iter()
    .map(|&n| n * 2)
    .collect();
```

---

## Advanced Examples

### Example 1: Nested Transformation

```aria
int64[][]:matrix = [[1, 2], [3, 4], [5, 6]];

// Double all elements in 2D array
int64[][]:doubled = transform(matrix, row => {
    pass(transform(row, n => n * 2));
});
// [[2, 4], [6, 8], [10, 12]]
```

### Example 2: Conditional Transformation

```aria
int64[]:numbers = [1, 2, 3, 4, 5];

// Double even, triple odd
int64[]:result = transform(numbers, n => {
    pass(is n % 2 == 0 : n * 2 : n * 3);
});
// [3, 4, 9, 8, 15]
```

### Example 3: Index-Aware Transformation

```aria
func:transformWithIndex = U[](T[]:array, func:(U)(T, int64):mapper) {
    U[]:result = [];
    
    till(array.length - 1, 1) {
        result.append(mapper(array[$], $));
    }
    
    pass(result);
};

// Add index to each element
string[]:indexed = transformWithIndex(words, (w, i) => {
    pass(`&{i}: &{w}`);
});
// ["0: hello", "1: world", "2: aria"]
```

### Example 4: Async Transformation (Future)

```aria
// Transform with async operations (tentative)
async func:fetchUserData = UserData(int64:userId) {
    // ... async operation
};

int64[]:userIds = [1, 2, 3, 4, 5];

// Transform with async mapper
UserData[]:userData = await transformAsync(userIds, fetchUserData);
```

---

## Error Handling

### Transform with Result

```aria
func:parseInt = Result[int64](string:s) {
    // Returns Result type
};

string[]:strings = ["10", "20", "abc", "30"];

Result[int64][]:results = transform(strings, parseInt);

// Filter successful parses
int64[]:numbers = results
    |> filter(r => r.isOk())
    |> transform(r => r.unwrap());
// [10, 20, 30]
```

---

## Related Topics

- [filter()](filter.md) - Filter array elements
- [reduce()](reduce.md) - Fold/reduce operation
- [Pipeline Operator](../operators/pipeline.md) - |> and <| for chaining
- [Lambda Functions](../functions/lambdas.md) - Anonymous functions
- [Arrays](../types/arrays.md) - Array type documentation

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Line 237  
**Unique Feature**: Type-changing transformation with pipeline support
