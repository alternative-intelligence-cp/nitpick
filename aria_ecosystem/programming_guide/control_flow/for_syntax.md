# Till Loop Syntax Reference

**Category**: Control Flow → Syntax  
**Topic**: Complete till loop syntax

---

## Basic Till Loop

```aria
till(end_index, step) {
    // $ is the index variable
}
```

---

## With Index Access

```aria
till(collection.length - 1, 1) {
    // $ is the index (0, 1, 2, ...)
    // collection[$] is the current item
}
```

---

## Mutable Iteration

```aria
till(collection.length - 1, 1) {
    // Modify elements directly
    collection[$] = new_value;
}
```

---

## With Item and Index

```aria
till(collection.length - 1, 1) {
    item: auto = collection[$];
    index: i32 = $;
    // Both available
}
```

---

## Range Iteration

```aria
// 0 to 9
till(9, 1) {
    // $ is 0, 1, ..., 9
}

// 0 to 10 (inclusive)
till(10, 1) {
    // $ is 0, 1, ..., 10
}

// Reverse iteration
till(collection.length - 1, -1) {
    // $ goes from length-1 down to 0
}
```

---

## Loop Control

```aria
till(items.length - 1, 1) {
    if condition {
        break;     // Exit loop
    }
    
    if other_condition {
        continue;  // Skip to next iteration
    }
}
```

---

## Using $ Directly

```aria
// Just need index
till(items.length - 1, 1) {
    stdout << "Index: " << $ << "\n";
}

// Need both index and value
till(items.length - 1, 1) {
    stdout << $ << ": " << items[$] << "\n";
}
```

---

## Examples

```aria
// Array iteration
arr: []i32 = [1, 2, 3];
till(arr.length - 1, 1) { }

// String iteration
str: string = "hello";
till(str.length - 1, 1) { }

// Range
till(4, 1) { }  // 0 to 4

// Mutable update
till(items.length - 1, 1) {
    items[$].update();
}
```

---

## Related Topics

- [For Loops](for.md) - For loop guide
- [Iteration Variable](iteration_variable.md) - Loop variables
- [Dollar Variable](dollar_variable.md) - Mutable iteration

---

**Quick Reference**: `for var in collection { }` or `for $var, index in collection { }`
