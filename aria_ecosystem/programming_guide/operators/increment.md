# Increment Operator (++)

**Category**: Operators → Unary  
**Operator**: `++`  
**Purpose**: Increase value by 1

---

## Syntax

```aria
<variable>++  // Postfix
++<variable>  // Prefix (if supported)
```

---

## Description

The increment operator `++` increases a variable's value by 1.

---

## Basic Usage

```aria
x: i32 = 5;
x++;
stdout << x;  // 6
```

---

## Postfix vs Prefix

```aria
// Postfix: returns old value, then increments
x: i32 = 5;
y: i32 = x++;  // y = 5, x = 6

// Prefix: increments, then returns new value
x: i32 = 5;
y: i32 = ++x;  // y = 6, x = 6
```

---

## In Loops

```aria
// Common in loops
i: i32 = 0;
while i < 10 {
    stdout << i;
    i++;
}
```

---

## Equivalent Forms

```aria
// These are equivalent
x++;
x += 1;
x = x + 1;
```

---

## Best Practices

### ✅ DO: Use in Simple Contexts

```aria
// Clear intent
counter++;
index++;
```

### ✅ DO: Prefer in Loops

```aria
for i in 0..10 {
    // Loop handles increment
}

// Or manual
i: i32 = 0;
while i < 10 {
    process(i);
    i++;
}
```

### ❌ DON'T: Use in Complex Expressions

```aria
// Confusing
arr[i++] = arr[i++];  // Undefined order!

// Clear
arr[i] = arr[i + 1];
i += 2;
```

### ❌ DON'T: Mix with Side Effects

```aria
// Unclear
x: i32 = ++i + i++;

// Clear
i += 1;
x = i + i;
i += 1;
```

---

## Related

- [Decrement (--)](decrement.md)
- [Add Assign (+=)](add_assign.md)
- [for loops](../control_flow/for.md)

---

**Remember**: `++` is shorthand for `+= 1`!
