# Greater or Equal Operator (>=)

**Category**: Operators → Comparison  
**Operator**: `>=`  
**Purpose**: Test if left is greater than or equal to right

---

## Syntax

```aria
<expression> >= <expression>
```

---

## Description

The greater than or equal operator `>=` tests if the left value is greater than **or equal to** the right value.

---

## Basic Usage

```aria
// Greater than
when 10 >= 5 then
    stdout << "True";
end

// Equal
when 5 >= 5 then
    stdout << "Also true";
end

// Less than
when 3 >= 5 then
    stdout << "Won't print";
end
```

---

## Includes Equality

```aria
min_age: i32 = 18;
age: i32 = 18;

when age >= min_age then
    stdout << "Allowed";  // Prints (18 >= 18)
end

// Contrast with >
when age > min_age then
    stdout << "Won't print";  // false (18 not > 18)
end
```

---

## Common Use Cases

### Minimum Values

```aria
when score >= 60 then
    stdout << "Pass";
else
    stdout << "Fail";
end
```

### Range Lower Bound

```aria
when age >= 18 and age <= 65 then
    stdout << "Working age";
end
```

### Loop Conditions

```aria
i: i32 = 10;
while i >= 0 {
    stdout << i;
    i -= 1;
}
// Prints: 10 9 8 7 6 5 4 3 2 1 0
```

---

## Best Practices

### ✅ DO: Use for Inclusive Bounds

```aria
// Inclusive range [min, max]
when value >= min and value <= max then
    stdout << "In range";
end
```

### ✅ DO: Use for Validation

```aria
when count >= 0 then
    process(count);
else
    fail "Count must be non-negative";
end
```

### ❌ DON'T: Use When > is Clearer

```aria
// Confusing
when x >= y + 1 then
    ...
end

// Clearer
when x > y then
    ...
end
```

---

## Related

- [Greater Than (>)](greater_than.md)
- [Less or Equal (<=)](less_equal.md)
- [Equal (==)](equal.md)

---

**Remember**: `>=` is **inclusive** - includes equality!
