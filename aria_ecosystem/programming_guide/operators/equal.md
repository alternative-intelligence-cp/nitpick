# Equality Operator (==)

**Category**: Operators → Comparison  
**Operator**: `==`  
**Purpose**: Test equality

---

## Syntax

```aria
<expression> == <expression>
```

---

## Description

The equality operator `==` tests if two values are equal, returning `true` or `false`.

---

## Basic Usage

```aria
// Numbers
when 5 == 5 then
    stdout << "Equal";
end

// Strings
when "hello" == "hello" then
    stdout << "Match";
end

// Variables
x: i32 = 42;
when x == 42 then
    stdout << "Found";
end
```

---

## Value Comparison

```aria
// Compares values, not identity
a: i32 = 100;
b: i32 = 100;
when a == b then  // true
    stdout << "Equal values";
end
```

---

## Type Requirements

```aria
// Must be same type
x: i32 = 42;
y: f64 = 42.0;

// Error: Can't compare i32 and f64
when x == y then  // ❌ Type error!
    ...
end

// Must cast
when x == (y as i32) then  // ✅ OK
    ...
end
```

---

## Struct Equality

```aria
struct Point {
    x: i32,
    y: i32
}

p1: Point = Point { x: 10, y: 20 };
p2: Point = Point { x: 10, y: 20 };

when p1 == p2 then  // true (field-wise comparison)
    stdout << "Same point";
end
```

---

## String Comparison

```aria
// Content comparison
s1: string = "hello";
s2: string = "hello";

when s1 == s2 then  // true
    stdout << "Same content";
end

// Case sensitive
when "Hello" == "hello" then  // false
    stdout << "Won't print";
end
```

---

## Floating Point

```aria
// Beware of precision
x: f64 = 0.1 + 0.2;
when x == 0.3 then  // May be false!
    stdout << "Might not print";
end

// Use epsilon comparison
epsilon: f64 = 0.000001;
when abs(x - 0.3) < epsilon then
    stdout << "Approximately equal";
end
```

---

## Nullable Comparison

```aria
x: i32? = 42;
y: i32? = 42;
z: i32? = nil;

when x == y then  // true
    stdout << "Both 42";
end

when z == nil then  // true
    stdout << "z is nil";
end
```

---

## Best Practices

### ✅ DO: Use for Value Comparison

```aria
when count == 0 then
    stdout << "Empty";
end
```

### ✅ DO: Compare Same Types

```aria
x: i32 = 42;
y: i32 = 42;
when x == y then  // OK
    ...
end
```

### ❌ DON'T: Confuse with Assignment

```aria
// Wrong: Assignment (=)
when x = 42 then  // Assigns, doesn't compare!
    ...
end

// Right: Comparison (==)
when x == 42 then
    ...
end
```

### ❌ DON'T: Compare Floats Directly

```aria
// Wrong: Precision issues
when float1 == float2 then
    ...
end

// Right: Use epsilon
when abs(float1 - float2) < epsilon then
    ...
end
```

---

## Operator Precedence

```aria
// == has lower precedence than arithmetic
when x + 1 == y * 2 then
    // Evaluated as: (x + 1) == (y * 2)
end
```

---

## Chaining Comparisons

```aria
// Not allowed
when x == y == z then  // ❌ Error
    ...
end

// Use logical AND
when x == y and y == z then
    stdout << "All equal";
end
```

---

## Related

- [Not Equal (!=)](not_equal.md)
- [Assignment (=)](assign.md)
- [Logical AND (and)](logical_and.md)

---

**Remember**: `==` compares **values**, not **identity**!
