# Loop Direction

**Category**: Control Flow → Till Loops  
**Concept**: Up or down iteration  
**Philosophy**: Flexible, readable counting

---

## What is Loop Direction?

**Loop direction** determines whether a `till` loop counts **up** or **down**.

---

## Up Direction (Default)

```aria
till i: 10 {
    stdout << i << " ";
}
// Output: 0 1 2 3 4 5 6 7 8 9
```

---

## Explicit Step Up

```aria
till i: 0 to 10 {
    // Counts from 0 up to 9
}
```

---

## Down Direction

```aria
till i: 10 down {
    stdout << i << " ";
}
// Output: 10 9 8 7 6 5 4 3 2 1
```

---

## Custom Start and End

```aria
// Count up from 5 to 14
till i: 5 to 15 {
    stdout << i << " ";
}
// Output: 5 6 7 8 9 10 11 12 13 14

// Count down from 10 to 1
till i: 10 down to 1 {
    stdout << i << " ";
}
// Output: 10 9 8 7 6 5 4 3 2 1
```

---

## Step Size

```aria
// Count by 2
till i: 0 to 10 by 2 {
    stdout << i << " ";
}
// Output: 0 2 4 6 8

// Count down by 2
till i: 10 down to 0 by 2 {
    stdout << i << " ";
}
// Output: 10 8 6 4 2 0
```

---

## Best Practices

### ✅ DO: Use `down` for Countdown

```aria
// Good: Clear intent
till i: 10 down {
    stdout << i << "...\n";
}
```

### ✅ DO: Specify Range for Clarity

```aria
// Good: Explicit range
till i: 1 to 100 {
    process(i);
}
```

### ❌ DON'T: Use Complex Logic for Simple Counts

```aria
// Wrong: Overcomplicated
i: i32 = 10;
while i > 0 {
    stdout << i;
    i = i - 1;
}

// Right: Use till
till i: 10 down {
    stdout << i;
}
```

---

## Examples

### Countdown

```aria
till seconds: 10 down {
    stdout << seconds << "...\n";
    sleep(1000);
}
stdout << "Liftoff!\n";
```

### Step Through Array Backwards

```aria
till i: items.length() down {
    stdout << items[i] << "\n";
}
```

### Skip Values

```aria
till i: 0 to 100 by 10 {
    stdout << i << " ";
}
// Output: 0 10 20 30 40 50 60 70 80 90
```

---

## Related Topics

- [Till](till.md) - Till loops
- [Till Syntax](till_syntax.md) - Complete syntax
- [For](for.md) - For loops
- [Loop](loop.md) - Infinite loops

---

**Remember**: Loop direction = **up (default) or down** - use `down` for countdown, `by` for custom steps!
