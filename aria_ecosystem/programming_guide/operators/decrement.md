# Decrement Operator (--)

**Category**: Operators → Unary  
**Operator**: `--`  
**Purpose**: Decrease value by 1

---

## Syntax

```aria
<variable>--  // Postfix
--<variable>  // Prefix (if supported)
```

---

## Description

The decrement operator `--` decreases a variable's value by 1.

---

## Basic Usage

```aria
x: i32 = 10;
x--;
stdout << x;  // 9
```

---

## Postfix vs Prefix

```aria
// Postfix: returns old value, then decrements
x: i32 = 10;
y: i32 = x--;  // y = 10, x = 9

// Prefix: decrements, then returns new value
x: i32 = 10;
y: i32 = --x;  // y = 9, x = 9
```

---

## In Loops

```aria
// Countdown
i: i32 = 10;
while i > 0 {
    stdout << i;
    i--;
}
// Prints: 10 9 8 7 6 5 4 3 2 1
```

---

## Equivalent Forms

```aria
// These are equivalent
x--;
x -= 1;
x = x - 1;
```

---

## Best Practices

### ✅ DO: Use for Countdown

```aria
count: i32 = 10;
while count > 0 {
    process(count);
    count--;
}
```

### ✅ DO: Use for Stack/Buffer Pointers

```aria
top: i32 = stack.length() - 1;
while top >= 0 {
    value: i32 = stack[top];
    top--;
}
```

### ❌ DON'T: Use in Complex Expressions

```aria
// Confusing
Result: i32 = --x + x--;

// Clear
x -= 1;
result = x + x;
x -= 1;
```

---

## Related

- [Increment (++)](increment.md)
- [Sub Assign (-=)](sub_assign.md)
- [while loops](../control_flow/while.md)

---

**Remember**: `--` is shorthand for `-= 1`!
