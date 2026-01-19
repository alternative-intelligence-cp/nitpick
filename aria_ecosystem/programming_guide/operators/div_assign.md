# Division Assignment Operator (/=)

**Category**: Operators → Compound Assignment  
**Operator**: `/=`  
**Purpose**: Divide and assign

---

## Syntax

```aria
<variable> /= <expression>
```

---

## Description

The division assignment operator `/=` divides the variable by the right operand and assigns the result back.

---

## Basic Usage

```aria
x: i32 = 100;
x /= 5;  // x = x / 5
stdout << x;  // 20
```

---

## Equivalent Forms

```aria
// These are equivalent
x /= 2;
x = x / 2;
```

---

## Integer Division

```aria
value: i32 = 10;
value /= 3;  // 3 (truncates)
stdout << value;
```

---

## Float Division

```aria
price: f64 = 99.99;
price /= 2.0;  // 49.995
stdout << price;
```

---

## Division by Zero

```aria
// Check before dividing
when divisor != 0 then
    total /= divisor;
else
    fail "Division by zero";
end
```

---

## Best Practices

### ✅ DO: Check for Zero

```aria
when count > 0 then
    average /= count;
end
```

### ✅ DO: Use for Averages

```aria
sum: i32 = 0;
for value in values {
    sum += value;
}
sum /= values.length();  // Average
```

### ❌ DON'T: Divide by Zero

```aria
// Wrong
x /= 0;  // Crash!

// Right
when divisor != 0 then
    x /= divisor;
end
```

---

## Related

- [Divide (/)](divide.md)
- [Add Assign (+=)](add_assign.md)
- [Modulo Assign (%=)](mod_assign.md)

---

**Remember**: `/=` divides **in-place**, check for **zero**!
