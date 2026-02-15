# Division Operator (/)

**Category**: Operators → Arithmetic  
**Operator**: `/`  
**Purpose**: Divide numbers

---

## Syntax

```aria
<expression> / <expression>
```

---

## Description

The division operator `/` divides the left operand by the right operand.

---

## Basic Usage

```aria
// Integer division
Result: i32 = 10 / 3;  // 3 (truncates)

// Float division
Result: f64 = 10.0 / 3.0;  // 3.333...
```

---

## Integer Division

```aria
// Truncates toward zero
x: i32 = 10 / 3;   // 3
y: i32 = -10 / 3;  // -3

// No automatic rounding
z: i32 = 7 / 2;    // 3, not 4
```

---

## Floating Point Division

```aria
// Precise division
a: f64 = 10.0 / 3.0;  // 3.3333333...
b: f64 = 1.0 / 3.0;   // 0.3333333...

// Mixed types require casting
x: i32 = 10;
Result: f64 = (x as f64) / 3.0;
```

---

## Division by Zero

```aria
// Integer division by zero: CRASH!
x: i32 = 10 / 0;  // ❌ Runtime error!

// Float division by zero: Infinity
y: f64 = 10.0 / 0.0;  // Infinity

// Check before dividing
when divisor != 0 then
    Result: i32 = numerator / divisor;
else
    fail "Division by zero";
end
```

---

## Compound Assignment

```aria
x: i32 = 100;
x /= 5;  // x = x / 5
stdout << x;  // 20
```

---

## Type Mixing

```aria
// Same type required
i: i32 = 10;
f: f64 = 3.0;

// Error
result := i / f;  // ❌ Type mismatch!

// Must cast
result := (i as f64) / f;  // ✅ OK
```

---

## Best Practices

### ✅ DO: Check for Zero

```aria
when divisor != 0 then
    Result: i32 = numerator / divisor;
end
```

### ✅ DO: Use Floats for Precision

```aria
// Integer: Loses precision
avg: i32 = total / count;  // Truncated

// Float: Preserves precision
avg: f64 = (total as f64) / (count as f64);
```

### ✅ DO: Use pass for Checked Division

```aria
Result: i32 = pass checked_div(a, b);  // Returns Result
```

### ❌ DON'T: Divide by Zero

```aria
// Wrong: No check
x: i32 = value / 0;  // Crash!

// Right: Check first
when divisor != 0 then
    x: i32 = value / divisor;
end
```

### ❌ DON'T: Assume Rounding

```aria
// Wrong: Doesn't round
half: i32 = 10 / 2;  // 5 (OK)
third: i32 = 10 / 3;  // 3 (not 3.33, not 4)

// Right: Explicit rounding
third: i32 = round(10.0 / 3.0) as i32;  // 3
```

---

## Operator Precedence

```aria
// Division before addition
Result: i32 = 10 + 20 / 5;  // 10 + 4 = 14

// Use parentheses for clarity
Result: i32 = (10 + 20) / 5;  // 30 / 5 = 6
```

---

## Common Patterns

### Average

```aria
sum: i32 = 0;
count: i32 = values.length();

till(values.length - 1, 1) {
    sum += values[$];
}

avg: f64 = (sum as f64) / (count as f64);
```

### Percentage

```aria
part: i32 = 30;
total: i32 = 200;

percentage: f64 = ((part as f64) / (total as f64)) * 100.0;
stdout << percentage << "%";  // 15%
```

### Split Into Groups

```aria
items: i32 = 100;
group_size: i32 = 7;

groups: i32 = items / group_size;  // 14 full groups
remainder: i32 = items % group_size;  // 2 items left
```

---

## Related

- [Modulo (%)](modulo.md) - Remainder
- [Multiply (*)](multiply.md)
- [Div Assign (/=)](div_assign.md)

---

**Remember**: Integer division **truncates**, check for **zero**!
