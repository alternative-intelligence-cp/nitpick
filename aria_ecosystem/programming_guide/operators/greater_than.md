# Greater Than Operator (>)

**Category**: Operators → Comparison  
**Operator**: `>`  
**Purpose**: Test if left is greater than right

---

## Syntax

```aria
<expression> > <expression>
```

---

## Description

The greater than operator `>` tests if the left value is strictly greater than the right value.

---

## Basic Usage

```aria
// Numbers
when 10 > 5 then
    stdout << "10 is greater";
end

when 5 > 10 then
    stdout << "Won't print";
end

// Variables
age: i32 = 25;
when age > 18 then
    stdout << "Adult";
end
```

---

## Strict Comparison

```aria
// Strict: not equal
when 5 > 5 then
    stdout << "Won't print";  // false
end

// Use >= for greater or equal
when 5 >= 5 then
    stdout << "Will print";  // true
end
```

---

## Type Requirements

```aria
// Must be comparable types
x: i32 = 10;
y: i32 = 5;

when x > y then  // OK
    stdout << "x is greater";
end

// Can't compare incompatible types
when x > "hello" then  // ❌ Type error!
    ...
end
```

---

## Strings

```aria
// Lexicographic comparison
when "banana" > "apple" then
    stdout << "b comes after a";
end

when "dog" > "cat" then
    stdout << "d comes after c";
end

// Case sensitive
when "Z" > "a" then  // false ('Z' = 90, 'a' = 97 in ASCII)
    stdout << "Won't print";
end
```

---

## Floating Point

```aria
x: f64 = 10.5;
y: f64 = 10.3;

when x > y then
    stdout << "x is greater";
end

// Beware of precision
a: f64 = 0.1 + 0.2;
when a > 0.3 then
    // May or may not be true due to precision
end
```

---

## Chaining Comparisons

```aria
// Natural chaining (if supported)
when 0 < x < 10 then
    stdout << "x is between 0 and 10";
end

// Otherwise use AND
when x > 0 and x < 10 then
    stdout << "x is between 0 and 10";
end
```

---

## Best Practices

### ✅ DO: Use for Range Checks

```aria
when score > 90 then
    grade: string = "A";
elsif score > 80 then
    grade: string = "B";
elsif score > 70 then
    grade: string = "C";
end
```

### ✅ DO: Combine with AND/OR

```aria
when age > 18 and age < 65 then
    stdout << "Working age";
end
```

### ❌ DON'T: Mix Types Without Casting

```aria
x: i32 = 10;
y: f64 = 5.5;

when x > y then  // ❌ Type error
    ...
end

when (x as f64) > y then  // ✅ OK
    ...
end
```

---

## Common Patterns

### Maximum

```aria
fn max(a: i32, b: i32) -> i32 {
    when a > b then
        return a;
    else
        return b;
    end
}
```

### Validation

```aria
when age > 0 and age < 150 then
    // Valid age range
else
    fail "Invalid age";
end
```

### Sorting

```aria
when a > b then
    // Swap
    temp: i32 = a;
    a = b;
    b = temp;
end
```

---

## Operator Precedence

```aria
// > has lower precedence than arithmetic
when x + 5 > y * 2 then
    // Evaluated as: (x + 5) > (y * 2)
end
```

---

## Related

- [Greater or Equal (>=)](greater_equal.md)
- [Less Than (<)](less_than.md)
- [Equal (==)](equal.md)

---

**Remember**: `>` is **strict** - use `>=` for greater **or equal**!
