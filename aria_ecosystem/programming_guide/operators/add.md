# Addition Operator (+)

**Category**: Operators → Arithmetic  
**Operator**: `+`  
**Purpose**: Add numbers or concatenate strings

---

## Syntax

```aria
<expression> + <expression>
```

---

## Description

The addition operator `+` adds two numbers or concatenates strings.

---

## Numeric Addition

```aria
// Integers
x: i32 = 5 + 3;  // 8

// Floats
y: f64 = 2.5 + 1.5;  // 4.0

// Mixed (requires casting)
z: f64 = 5.0 + (3 as f64);  // 8.0
```

---

## String Concatenation

```aria
// Concatenate strings
greeting: string = "Hello" + " " + "World";
stdout << greeting;  // "Hello World"

// String + number (requires conversion)
message: string = "Count: " + to_string(42);
```

---

## Type Requirements

```aria
// Same type required
a: i32 = 5;
b: i32 = 3;
sum: i32 = a + b;  // OK

// Type mismatch
x: i32 = 5;
y: f64 = 3.0;
result := x + y;  // ❌ Error!

// Must cast
result := (x as f64) + y;  // ✅ OK
```

---

## Overflow

```aria
// Integer overflow (wraps or errors depending on mode)
max: i32 = 2147483647;
overflow: i32 = max + 1;  // May wrap to -2147483648

// Use checked arithmetic
Result: i32 = pass checked_add(max, 1);  // Returns error
```

---

## Operator Precedence

```aria
// Multiplication before addition
Result: i32 = 2 + 3 * 4;  // 2 + 12 = 14

// Use parentheses
Result: i32 = (2 + 3) * 4;  // 5 * 4 = 20
```

---

## Best Practices

### ✅ DO: Use for Accumulation

```aria
sum: i32 = 0;
till(values.length - 1, 1) {
    sum += values[$];
}
```

### ✅ DO: Group Complex Expressions

```aria
total: f64 = (price * quantity) + tax + shipping;
```

### ❌ DON'T: Mix Types

```aria
// Wrong
result := 5 + 3.0;  // Type error

// Right
result := 5.0 + 3.0;
```

### ❌ DON'T: Ignore Overflow

```aria
// Dangerous
Result: i32 = huge_number + another_huge;

// Safe
Result: i32 = pass checked_add(huge_number, another_huge);
```

---

## Compound Assignment

```aria
x: i32 = 10;
x += 5;  // x = x + 5
stdout << x;  // 15
```

---

## Related

- [Subtract (-)](#subtract.md)
- [Add Assign (+=)](add_assign.md)
- [String Concatenation](../types/string_operations.md)

---

**Remember**: `+` requires **same types** - cast when needed!
