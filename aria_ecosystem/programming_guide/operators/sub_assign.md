# Subtract Assignment Operator (-=)

**Category**: Operators → Compound Assignment  
**Operator**: `-=`  
**Purpose**: Subtract and assign

---

## Syntax

```aria
<variable> -= <expression>
```

---

## Description

The subtract assignment operator `-=` subtracts the right operand from the variable and assigns the result back.

---

## Basic Usage

```aria
x: i32 = 20;
x -= 5;  // x = x - 5
stdout << x;  // 15
```

---

## Equivalent Forms

```aria
// These are equivalent
x -= 3;
x = x - 3;
```

---

## Common Uses

### Decrement Counter

```aria
remaining: i32 = 100;
remaining -= 10;

// Or use --
remaining--;
```

### Resource Tracking

```aria
credits: i32 = 1000;
cost: i32 = 150;
credits -= cost;
```

### Time Remaining

```aria
time_left: i32 = 60;
while time_left > 0 {
    process();
    time_left -= 1;
}
```

---

## Best Practices

### ✅ DO: Use for Consumption

```aria
inventory: i32 = 100;
sold: i32 = 25;
inventory -= sold;
```

### ✅ DO: Use for Countdown

```aria
retries: i32 = 3;
retries -= 1;
```

### ❌ DON'T: Use for Single Decrement

```aria
// Unclear
x -= 1;

// Better
x--;
```

---

## Related

- [Subtraction (-)](subtract.md)
- [Add Assign (+=)](add_assign.md)
- [Decrement (--)](decrement.md)

---

**Remember**: `-=` subtracts **in-place**!
