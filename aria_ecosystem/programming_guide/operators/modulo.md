# Modulo Operator (%)

**Category**: Operators → Arithmetic  
**Operator**: `%`  
**Purpose**: Calculate remainder

---

## Syntax

```aria
<expression> % <expression>
```

---

## Description

The modulo operator `%` returns the remainder after division.

---

## Basic Usage

```aria
// 10 divided by 3 is 3 remainder 1
remainder: i32 = 10 % 3;  // 1

// 15 divided by 4 is 3 remainder 3
x: i32 = 15 % 4;  // 3

// Exact division has 0 remainder
y: i32 = 10 % 5;  // 0
```

---

## Even/Odd Test

```aria
fn is_even(n: i32) -> bool {
    return n % 2 == 0;
}

fn is_odd(n: i32) -> bool {
    return n % 2 != 0;
}

stdout << is_even(4);  // true
stdout << is_odd(7);   // true
```

---

## Wrap Around

```aria
// Circular buffer index
index: i32 = (index + 1) % buffer_size;

// Day of week (0-6)
day: i32 = (current_day + days_ahead) % 7;
```

---

## Division by Zero

```aria
// Division by zero: ERROR!
Result: i32 = 10 % 0;  // ❌ Crash!

// Check first
when divisor != 0 then
    remainder: i32 = value % divisor;
end
```

---

## Negative Numbers

```aria
// Result has sign of dividend
x: i32 = 10 % 3;    // 1
y: i32 = -10 % 3;   // -1
z: i32 = 10 % -3;   // 1
w: i32 = -10 % -3;  // -1
```

---

## Best Practices

### ✅ DO: Use for Cycling

```aria
// Rotate through array
current: i32 = (current + 1) % array.length();
```

### ✅ DO: Use for Even/Odd

```aria
when n % 2 == 0 then
    stdout << "Even";
end
```

### ❌ DON'T: Modulo by Zero

```aria
// Wrong
x: i32 = value % 0;  // Crash!

// Right
when divisor != 0 then
    x: i32 = value % divisor;
end
```

---

## Common Patterns

### Alternating Pattern

```aria
till(9, 1) {
    when $ % 2 == 0 then
        stdout << "Even: " << $;
    else
        stdout << "Odd: " << $;
    end
}
```

### Multiple of N

```aria
when value % 10 == 0 then
    stdout << "Multiple of 10";
end
```

### Extract Digit

```aria
// Get last digit
last_digit: i32 = number % 10;

// Get digit at position
digit: i32 = (number / power_of_10) % 10;
```

---

## Compound Assignment

```aria
x: i32 = 17;
x %= 5;  // x = x % 5
stdout << x;  // 2
```

---

## Related

- [Division (/)](divide.md)
- [Mod Assign (%=)](mod_assign.md)
- [Integer Division](../concepts/integer_arithmetic.md)

---

**Remember**: `%` gives the **remainder**, check for **zero**!
