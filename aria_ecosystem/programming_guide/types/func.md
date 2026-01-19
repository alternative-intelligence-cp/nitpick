# Function Types

**Category**: Types → Functions  
**Syntax**: `fn(<params>) -> <return>`  
**Purpose**: First-class function types

---

## Overview

Functions are **first-class values** in Aria - they can be stored in variables, passed as arguments, and returned.

---

## Function Type Syntax

```aria
// Function type
type UnaryOp = fn(i32) -> i32;

// Function that takes two i32s, returns i32
type BinaryOp = fn(i32, i32) -> i32;

// Function that returns nothing
type Action = fn() -> void;
```

---

## Declaring Function Variables

```aria
// Store function in variable
add: fn(i32, i32) -> i32 = fn(a: i32, b: i32) -> i32 {
    return a + b;
};

// Use it
Result: i32 = add(5, 3);  // 8
```

---

## Passing Functions

```aria
fn apply(op: fn(i32) -> i32, value: i32) -> i32 {
    return op(value);
}

// Use
double := fn(x: i32) -> i32 { return x * 2; };
Result: i32 = apply(double, 5);  // 10
```

---

## Returning Functions

```aria
fn make_adder(n: i32) -> fn(i32) -> i32 {
    return fn(x: i32) -> i32 {
        return x + n;
    };
}

// Use
add_five := make_adder(5);
Result: i32 = add_five(10);  // 15
```

---

## Anonymous Functions (Lambdas)

```aria
// Anonymous function
square := fn(x: i32) -> i32 { return x * x; };

// Inline
numbers.map(fn(x: i32) -> i32 { return x * 2; });
```

---

## Closures

```aria
// Capture variables from outer scope
counter: i32 = 0;

increment := fn() -> i32 {
    counter += 1;  // Captures 'counter'
    return counter;
};

a: i32 = increment();  // 1
b: i32 = increment();  // 2
```

---

## Higher-Order Functions

```aria
// Takes function, returns function
fn compose(f: fn(i32) -> i32, g: fn(i32) -> i32) -> fn(i32) -> i32 {
    return fn(x: i32) -> i32 {
        return f(g(x));
    };
}

// Use
double := fn(x: i32) -> i32 { return x * 2; };
square := fn(x: i32) -> i32 { return x * x; };

double_then_square := compose(square, double);
Result: i32 = double_then_square(3);  // (3*2)^2 = 36
```

---

## Best Practices

### ✅ DO: Use for Callbacks

```aria
fn process_async(callback: fn(i32) -> void) {
    // Do async work
    Result: i32 = compute();
    callback(result);
}
```

### ✅ DO: Use for Strategy Pattern

```aria
fn sort(arr: []i32, compare: fn(i32, i32) -> i32) {
    // Sort using comparison function
    ...
}
```

---

## Related

- [Function Declaration](func_declaration.md)
- [Function Return](func_return.md)
- [Functions](../functions/README.md)

---

**Remember**: Functions are **first-class** in Aria!
