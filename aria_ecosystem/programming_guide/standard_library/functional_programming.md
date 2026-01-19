# Functional Programming

**Category**: Standard Library → Functional Programming  
**Purpose**: Overview of functional programming utilities

---

## Overview

Aria provides comprehensive **functional programming** tools for working with collections:

- **filter()** - Select elements matching condition
- **map()/transform()** - Transform each element
- **reduce()** - Combine elements into single value
- **sort()** - Order elements
- **reverse()** - Reverse order
- **unique()** - Remove duplicates

---

## Core Concepts

### Higher-Order Functions

Functions that take or return other functions:

```aria
fn apply_twice(f: fn(i32) -> i32, x: i32) -> i32 {
    return f(f(x));
}

double := fn(x: i32) -> i32 { return x * 2; };

Result: i32 = apply_twice(double, 5);  // 20
```

---

### Pure Functions

Functions with no side effects:

```aria
// Pure - same input always gives same output
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// Impure - depends on external state
counter: i32 = 0;
fn increment() -> i32 {
    counter += 1;  // Side effect!
    return counter;
}
```

---

## Common Patterns

### Map-Filter-Reduce

```aria
numbers: []i32 = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

sum_of_evens_squared: i32 = numbers
    .filter(fn(x) { return x % 2 == 0; })      // [2,4,6,8,10]
    .map(fn(x) { return x * x; })               // [4,16,36,64,100]
    .reduce(0, fn(acc, x) { return acc + x; }); // 220
```

---

### Chaining Operations

```aria
struct User {
    name: string,
    age: i32,
    active: bool
}

users: []User = [...];

// Get names of active adult users, sorted
names: []string = users
    .filter(fn(u) { return u.active; })
    .filter(fn(u) { return u.age >= 18; })
    .map(fn(u) { return u.name; })
    .sort();
```

---

### Composition

```aria
// Compose functions
fn compose(f: fn(i32) -> i32, g: fn(i32) -> i32) -> fn(i32) -> i32 {
    return fn(x: i32) -> i32 {
        return f(g(x));
    };
}

double := fn(x: i32) -> i32 { return x * 2; };
add_one := fn(x: i32) -> i32 { return x + 1; };

double_then_add_one := compose(add_one, double);

Result: i32 = double_then_add_one(5);  // (5*2)+1 = 11
```

---

### Currying

```aria
// Currying - convert multi-argument function to nested single-argument functions
fn add(a: i32) -> fn(i32) -> i32 {
    return fn(b: i32) -> i32 {
        return a + b;
    };
}

add_five := add(5);
Result: i32 = add_five(10);  // 15
```

---

## Functional Array Operations

### Filter

```aria
// Keep elements matching condition
evens: []i32 = numbers.filter(fn(x) { return x % 2 == 0; });
```

### Map

```aria
// Transform each element
doubled: []i32 = numbers.map(fn(x) { return x * 2; });
```

### Reduce

```aria
// Combine all elements
sum: i32 = numbers.reduce(0, fn(acc, x) { return acc + x; });
```

### Sort

```aria
// Order elements
sorted: []i32 = numbers.sort();
```

### Reverse

```aria
// Reverse order
reversed: []i32 = numbers.reverse();
```

### Unique

```aria
// Remove duplicates
deduped: []i32 = numbers.unique();
```

---

## Pipelines

```aria
// Process data through multiple stages
data: []i32 = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

Result: []i32 = data
    .filter(fn(x) { return x > 3; })      // [4,5,6,7,8,9,10]
    .map(fn(x) { return x * x; })         // [16,25,36,49,64,81,100]
    .filter(fn(x) { return x < 70; })     // [16,25,36,49,64]
    .reverse();                            // [64,49,36,25,16]
```

---

## Lazy Evaluation

Some operations can be optimized with lazy evaluation:

```aria
// Find first even number > 100
numbers: []i32 = [1, 2, 3, ..., 200];

first_large_even: ?i32 = numbers
    .filter(fn(x) { return x % 2 == 0; })
    .filter(fn(x) { return x > 100; })
    .first();  // Stops after finding first match
```

---

## Best Practices

### ✅ DO: Use Pure Functions

```aria
// Good - pure function
fn square(x: i32) -> i32 {
    return x * x;
}

// Better than impure alternative
total: i32 = 0;  // Avoid global state
fn add_to_total(x: i32) {
    total += x;  // ❌ Side effect
}
```

### ✅ DO: Chain Operations

```aria
Result: []string = users
    .filter(is_active)
    .map(extract_name)
    .sort();
```

### ✅ DO: Use Descriptive Names

```aria
// Good
is_even := fn(x: i32) -> bool { return x % 2 == 0; };
evens: []i32 = numbers.filter(is_even);

// Not great
f := fn(x) { return x % 2 == 0; };
Result: []i32 = numbers.filter(f);
```

### ❌ DON'T: Over-Chain

```aria
// Too complex - hard to debug
Result: []i32 = data
    .filter(...)
    .map(...)
    .filter(...)
    .reduce(...)
    .map(...)
    .filter(...);  // ❌ Too many operations

// Better - break into steps
filtered: []i32 = data.filter(...);
transformed: []i32 = filtered.map(...);
Result: []i32 = transformed.filter(...);  // ✅ Clearer
```

---

## Related

- [filter()](filter.md) - Filter elements
- [reduce()](reduce.md) - Reduce to single value
- [sort()](sort.md) - Sort array
- [transform()](transform.md) - Transform elements

---

**Remember**: Functional programming emphasizes **pure functions** and **immutability**!
