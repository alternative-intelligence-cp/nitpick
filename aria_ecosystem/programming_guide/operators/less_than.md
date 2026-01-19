# Less Than Operator (<)

**Category**: Operators → Comparison  
**Operator**: `<`  
**Purpose**: Test if left is less than right

---

## Syntax

```aria
<expression> < <expression>
```

---

## Description

The less than operator `<` tests if the left value is strictly less than the right value.

---

## Basic Usage

```aria
// Numbers
when 5 < 10 then
    stdout << "5 is less";
end

when 10 < 5 then
    stdout << "Won't print";
end

// Variables
age: i32 = 15;
when age < 18 then
    stdout << "Minor";
end
```

---

## Strict Comparison

```aria
// Strict: not equal
when 5 < 5 then
    stdout << "Won't print";  // false
end

// Use <= for less or equal
when 5 <= 5 then
    stdout << "Will print";  // true
end
```

---

## Strings

```aria
// Lexicographic comparison
when "apple" < "banana" then
    stdout << "a comes before b";
end

when "cat" < "dog" then
    stdout << "c comes before d";
end

// Case sensitive
when "a" < "Z" then  // false ('a' = 97, 'Z' = 90)
    stdout << "Won't print";
end
```

---

## Range Checking

```aria
// Check if in range
when x < max and x > min then
    stdout << "In range";
end

// Chained (if supported)
when min < x < max then
    stdout << "In range";
end
```

---

## Best Practices

### ✅ DO: Use for Range Validation

```aria
when score < 0 or score > 100 then
    fail "Invalid score";
end
```

### ✅ DO: Use for Boundaries

```aria
when index < array.length() then
    value: i32 = array[index];
end
```

### ❌ DON'T: Mix Types

```aria
// Wrong
when 5 < 3.0 then  // Type error
    ...
end

// Right
when 5.0 < 3.0 then
    ...
end
```

---

## Common Patterns

### Minimum

```aria
fn min(a: i32, b: i32) -> i32 {
    when a < b then
        return a;
    else
        return b;
    end
}
```

### Bounds Check

```aria
when index < 0 or index >= length then
    fail "Index out of bounds";
end
```

### Sorting

```aria
when a < b then
    // a comes before b
    // Already in order
else
    // Swap needed
    temp: i32 = a;
    a = b;
    b = temp;
end
```

---

## Related

- [Less or Equal (<=)](less_equal.md)
- [Greater Than (>)](greater_than.md)
- [Equal (==)](equal.md)

---

**Remember**: `<` is **strict** - use `<=` for less **or equal**!
