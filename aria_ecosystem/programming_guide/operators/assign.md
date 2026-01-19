# Assignment Operator (=)

**Category**: Operators → Assignment  
**Operator**: `=`  
**Purpose**: Bind value to variable

---

## Syntax

```aria
<variable> = <expression>
```

---

## Description

The assignment operator `=` binds a value to a variable name.

---

## Basic Usage

```aria
// Simple assignment
x: i32 = 42;

// Reassignment
x = 100;

// Expression
y: i32 = x + 10;
```

---

## Multiple Assignment

```aria
// Assign same value to multiple variables
a: i32 = b: i32 = c: i32 = 0;

// Better: Separate assignments
a: i32 = 0;
b: i32 = 0;
c: i32 = 0;
```

---

## Type Inference

```aria
// Explicit type
x: i32 = 42;

// Type inferred
y := 42;  // Type is i32
```

---

## Assignment vs Equality

```aria
// Assignment (single =)
x: i32 = 42;

// Comparison (double ==)
when x == 42 then
    stdout << "Equal";
end
```

---

## Compound Assignment

```aria
// Long form
x = x + 5;

// Compound form
x += 5;

// Available: +=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>=
```

---

## Assignment Returns Value

```aria
// Assignment returns the assigned value
x: i32;
y: i32 = (x = 42);  // x and y both 42

// Useful in conditionals
when (x = get_value()) > 0 then
    stdout << "Positive: " << x;
end
```

---

## Best Practices

### ✅ DO: Initialize When Declaring

```aria
x: i32 = 0;  // Clear initial value
```

### ✅ DO: Use Compound Operators

```aria
x += 5;  // Clearer than x = x + 5
```

### ❌ DON'T: Confuse = and ==

```aria
// Wrong: Assignment in condition (probably a bug)
when x = 42 then  // Assigns 42 to x, always true!
    stdout << "Bug!";
end

// Right: Comparison
when x == 42 then
    stdout << "Correct";
end
```

### ❌ DON'T: Chain Assignments Unnecessarily

```aria
// Confusing
a = b = c = 0;

// Clearer
a = 0;
b = 0;
c = 0;
```

---

## Move Semantics

```aria
// Assignment can move ownership
str1: string = "Hello";
str2: string = str1;  // str1 moved to str2

// str1 no longer valid
// str2 owns the string
```

---

## Copy vs Move

```aria
// Copy types (i32, f64, bool, etc.)
x: i32 = 42;
y: i32 = x;  // Copies
// Both x and y valid

// Move types (string, arrays, structs)
s1: string = "Hello";
s2: string = s1;  // Moves
// s1 no longer valid
```

---

## Related

- [Compound Assignment](add_assign.md) - +=, -=, etc.
- [Type Inference](../types/type_inference.md)
- [Move Semantics](../concepts/move_semantics.md)

---

**Remember**: `=` is assignment, `==` is comparison!
