# Multiplication Operator (*)

**Category**: Operators → Arithmetic  
**Operator**: `*`  
**Purpose**: Multiply numbers or dereference pointers

---

## Syntax

```aria
<expression> * <expression>  // Multiplication
*<pointer>                    // Dereference
```

---

## Description

The multiplication operator `*` multiplies two numbers or dereferences a pointer.

---

## Numeric Multiplication

```aria
// Integers
x: i32 = 5 * 3;  // 15

// Floats
y: f64 = 2.5 * 4.0;  // 10.0

// Mixed (requires cast)
z: f64 = (3 as f64) * 2.5;  // 7.5
```

---

## Variables

```aria
width: i32 = 10;
height: i32 = 5;
area: i32 = width * height;  // 50
```

---

## Overflow

```aria
// Integer overflow
large: i32 = 100000;
overflow: i32 = large * large;  // May overflow!

// Use checked arithmetic
Result: i32 = pass checked_mul(large, large);
```

---

## Pointer Dereference

```aria
// Get value from pointer
ptr: *i32 = &value;
x: i32 = *ptr;  // Dereference

// Modify through pointer
*ptr = 42;  // Set value
```

---

## Type Requirements

```aria
// Same type required
a: i32 = 5;
b: i32 = 3;
product: i32 = a * b;  // OK

// Must cast
x: i32 = 5;
y: f64 = 3.0;
result := (x as f64) * y;  // ✅ OK
```

---

## Best Practices

### ✅ DO: Use for Area/Volume

```aria
area: i32 = length * width;
volume: i32 = length * width * height;
```

### ✅ DO: Use for Scaling

```aria
scaled: f64 = value * scale_factor;
percentage: f64 = amount * 0.15;  // 15%
```

### ❌ DON'T: Ignore Overflow

```aria
// Dangerous
huge: i32 = big1 * big2;

// Safe
huge: i32 = pass checked_mul(big1, big2);
```

---

## Compound Assignment

```aria
x: i32 = 10;
x *= 5;  // x = x * 5
stdout << x;  // 50
```

---

## Operator Precedence

```aria
// Multiplication before addition
Result: i32 = 2 + 3 * 4;  // 2 + 12 = 14

// Use parentheses for clarity
Result: i32 = (2 + 3) * 4;  // 5 * 4 = 20
```

---

## Common Patterns

### Percentage

```aria
price: f64 = 100.0;
tax: f64 = price * 0.08;  // 8% tax
total: f64 = price + tax;
```

### Array Size

```aria
rows: i32 = 10;
cols: i32 = 20;
total_cells: i32 = rows * cols;  // 200
```

---

## Related

- [Division (/)](divide.md)
- [Mul Assign (*=)](mul_assign.md)
- [Dereference](../memory_model/pointer_syntax.md)

---

**Remember**: `*` multiplies **numbers** or **dereferences pointers**!
