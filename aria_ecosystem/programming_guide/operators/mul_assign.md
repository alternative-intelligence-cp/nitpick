# Multiply Assignment Operator (*=)

**Category**: Operators → Compound Assignment  
**Operator**: `*=`  
**Purpose**: Multiply and assign

---

## Syntax

```aria
<variable> *= <expression>
```

---

## Description

The multiply assignment operator `*=` multiplies the variable by the right operand and assigns the result back.

---

## Basic Usage

```aria
x: i32 = 5;
x *= 3;  // x = x * 3
stdout << x;  // 15
```

---

## Equivalent Forms

```aria
// These are equivalent
x *= 2;
x = x * 2;
```

---

## Common Uses

### Doubling

```aria
value: i32 = 10;
value *= 2;  // 20
```

### Scaling

```aria
price: f64 = 99.99;
price *= 1.08;  // Add 8% tax
```

### Exponential Growth

```aria
population: i32 = 100;
for year in 0..10 {
    population *= 2;  // Double each year
}
```

---

## Best Practices

### ✅ DO: Use for Scaling

```aria
distance: f64 = 100.0;
distance *= scale_factor;
```

### ✅ DO: Use for Accumulation

```aria
product: i32 = 1;
for value in values {
    product *= value;
}
```

### ❌ DON'T: Ignore Overflow

```aria
// Dangerous
big: i32 = 1000000;
big *= big;  // Overflow!

// Safe
big: i64 = 1000000;
big *= big;  // Uses larger type
```

---

## Related

- [Multiplication (*)](multiply.md)
- [Div Assign (/=)](div_assign.md)
- [Add Assign (+=)](add_assign.md)

---

**Remember**: `*=` multiplies **in-place**!
