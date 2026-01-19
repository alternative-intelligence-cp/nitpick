# Less or Equal Operator (<=)

**Category**: Operators → Comparison  
**Operator**: `<=`  
**Purpose**: Test if left is less than or equal to right

---

## Syntax

```aria
<expression> <= <expression>
```

---

## Description

The less than or equal operator `<=` tests if the left value is less than **or equal to** the right value.

---

## Basic Usage

```aria
// Less than
when 5 <= 10 then
    stdout << "True";
end

// Equal
when 5 <= 5 then
    stdout << "Also true";
end

// Greater than
when 10 <= 5 then
    stdout << "Won't print";
end
```

---

## Includes Equality

```aria
max_age: i32 = 65;
age: i32 = 65;

when age <= max_age then
    stdout << "Allowed";  // Prints (65 <= 65)
end

// Contrast with <
when age < max_age then
    stdout << "Won't print";  // false
end
```

---

## Common Use Cases

### Maximum Bounds

```aria
when score <= 100 then
    process(score);
else
    fail "Score too high";
end
```

### Range Upper Bound

```aria
when age >= 18 and age <= 65 then
    stdout << "Working age";
end
```

### Array Bounds

```aria
when index >= 0 and index <= last_index then
    value: i32 = array[index];
end
```

---

## Best Practices

### ✅ DO: Use for Inclusive Bounds

```aria
// Inclusive range [min, max]
when value >= min and value <= max then
    stdout << "Valid";
end
```

### ✅ DO: Use for Limits

```aria
when count <= MAX_ITEMS then
    items.push(new_item);
end
```

### ❌ DON'T: Use When < is Clearer

```aria
// Confusing
when x <= y - 1 then
    ...
end

// Clearer
when x < y then
    ...
end
```

---

## Common Patterns

### Countdown

```aria
i: i32 = 10;
while i >= 0 {
    stdout << i;
    i--;
}
// Prints: 10 9 8 7 6 5 4 3 2 1 0
```

### Pagination

```aria
when page <= total_pages then
    show_page(page);
end
```

---

## Related

- [Less Than (<)](less_than.md)
- [Greater or Equal (>=)](greater_equal.md)
- [Equal (==)](equal.md)

---

**Remember**: `<=` is **inclusive** - includes equality!
