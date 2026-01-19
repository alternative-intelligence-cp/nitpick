# For Loop Syntax Reference

**Category**: Control Flow → Syntax  
**Topic**: Complete for loop syntax

---

## Basic For Loop

```aria
for variable in collection {
    // code
}
```

---

## With Index

```aria
for variable, index in collection {
    // variable: current item
    // index: position (0-based)
}
```

---

## Mutable Iteration

```aria
for $variable in collection {
    // $ allows modification
}
```

---

## Mutable with Index

```aria
for $variable, index in collection {
    // Only variable is mutable
}
```

---

## Range Iteration

```aria
// Exclusive end
for i in 0..10 {
    // 0 to 9
}

// Inclusive end
for i in 0..=10 {
    // 0 to 10
}

// Reverse
for i in numbers.reverse() {
    // ...
}
```

---

## Loop Control

```aria
for item in items {
    if condition {
        break;     // Exit loop
    }
    
    if other_condition {
        continue;  // Skip to next iteration
    }
}
```

---

## Underscore for Unused

```aria
// Ignore item
for _, index in items { }

// Ignore index  
for item, _ in items { }
```

---

## Examples

```aria
// Array iteration
for num in [1, 2, 3] { }

// String iteration
for char in "hello" { }

// Range
for i in 0..5 { }

// Map iteration
for key, value in map { }

// Mutable
for $item in items {
    item.update();
}
```

---

## Related Topics

- [For Loops](for.md) - For loop guide
- [Iteration Variable](iteration_variable.md) - Loop variables
- [Dollar Variable](dollar_variable.md) - Mutable iteration

---

**Quick Reference**: `for var in collection { }` or `for $var, index in collection { }`
