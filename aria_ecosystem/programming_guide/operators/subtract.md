# Subtraction Operator (-)

**Category**: Operators → Arithmetic  
**Operator**: `-`  
**Purpose**: Subtract numbers or negate

---

## Syntax

```aria
<expression> - <expression>  // Binary
-<expression>                 // Unary (negation)
```

---

## Description

The subtraction operator `-` subtracts the right operand from the left, or negates a value.

---

## Binary Subtraction

```aria
// Integers
x: i32 = 10 - 3;  // 7

// Floats
y: f64 = 5.5 - 2.3;  // 3.2

// Variables
a: i32 = 100;
b: i32 = 42;
diff: i32 = a - b;  // 58
```

---

## Unary Negation

```aria
// Negate positive
x: i32 = 5;
neg: i32 = -x;  // -5

// Negate negative
y: i32 = -10;
pos: i32 = -y;  // 10

// Direct literal
value: i32 = -42;
```

---

## Type Requirements

```aria
// Same type required
a: i32 = 10;
b: i32 = 3;
Result: i32 = a - b;  // OK

// Must cast different types
x: i32 = 10;
y: f64 = 3.0;
result := (x as f64) - y;  // ✅ OK
```

---

## Underflow

```aria
// Integer underflow
min: i32 = -2147483648;
underflow: i32 = min - 1;  // May wrap to 2147483647

// Use checked arithmetic
Result: i32 = pass checked_sub(min, 1);  // Returns error
```

---

## Unsigned Subtraction

```aria
// Careful with unsigned
a: u32 = 5;
b: u32 = 10;
Result: u32 = a - b;  // Wraps! Very large number

// Check before subtracting
when a >= b then
    diff: u32 = a - b;
else
    fail "Would underflow";
end
```

---

## Best Practices

### ✅ DO: Use for Differences

```aria
elapsed: i32 = end_time - start_time;
remaining: i32 = total - used;
```

### ✅ DO: Check Unsigned Bounds

```aria
when current >= previous then
    delta: u32 = current - previous;
end
```

### ❌ DON'T: Subtract Different Types

```aria
// Wrong
result := 10 - 5.0;  // Type error

// Right  
result := 10.0 - 5.0;
```

---

## Compound Assignment

```aria
x: i32 = 100;
x -= 25;  // x = x - 25
stdout << x;  // 75
```

---

## Operator Precedence

```aria
// Same as addition
Result: i32 = 10 - 2 * 3;  // 10 - 6 = 4

// Use parentheses
Result: i32 = (10 - 2) * 3;  // 8 * 3 = 24
```

---

## Related

- [Addition (+)](add.md)
- [Sub Assign (-=)](sub_assign.md)
- [Negation](../concepts/unary_operators.md)

---

**Remember**: `-` can be **binary** (subtract) or **unary** (negate)!
