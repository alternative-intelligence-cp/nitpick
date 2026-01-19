# Logical NOT Operator (!)

**Category**: Operators → Logical  
**Operator**: `!`  
**Purpose**: Negate boolean value

---

## Syntax

```aria
!<expression>
```

---

## Description

The logical NOT operator `!` inverts a boolean value: `true` becomes `false`, `false` becomes `true`.

---

## Basic Usage

```aria
// Negate true
Result: bool = !true;  // false

// Negate false
Result: bool = !false;  // true

// Negate variable
is_ready: bool = false;
is_not_ready: bool = !is_ready;  // true
```

---

## In Conditions

```aria
is_valid: bool = check_input();

when !is_valid then
    stdout << "Invalid input";
end

// Equivalent to
when is_valid == false then
    stdout << "Invalid input";
end
```

---

## Double Negation

```aria
// Double NOT returns original
x: bool = true;
Result: bool = !!x;  // true

// Generally avoid double negation
```

---

## With Comparisons

```aria
// Negate comparison
when !(x > 10) then
    stdout << "x is 10 or less";
end

// Better: Use opposite operator
when x <= 10 then
    stdout << "x is 10 or less";
end
```

---

## Best Practices

### ✅ DO: Use for Readability

```aria
when !is_empty then
    process_items();
end
```

### ✅ DO: Use for Opposite Condition

```aria
found: bool = search(item);

when !found then
    stdout << "Not found";
end
```

### ❌ DON'T: Double Negate

```aria
// Confusing
when !!value then
    ...
end

// Clear
when value then
    ...
end
```

### ❌ DON'T: Negate When Opposite Exists

```aria
// Unclear
when !(a < b) then
    ...
end

// Clear
when a >= b then
    ...
end
```

---

## Common Patterns

### Inverting Flags

```aria
enabled: bool = true;
disabled: bool = !enabled;
```

### Guard Clauses

```aria
when !is_authorized() then
    fail "Not authorized";
end

// Continue with authorized logic
```

### Toggle

```aria
// Toggle boolean
flag = !flag;
```

---

## With Logical Operators

```aria
// NOT has highest precedence
Result: bool = !a and b;  // (!a) and b

// Use parentheses for clarity
Result: bool = !(a and b);  // NOT (a and b)
```

---

## De Morgan's Laws

```aria
// !(a and b) == (!a or !b)
when !(x > 5 and y < 10) then
    // Same as: x <= 5 or y >= 10
end

// !(a or b) == (!a and !b)
when !(is_empty or is_null) then
    // Same as: !is_empty and !is_null
end
```

---

## Related

- [Logical AND (and)](logical_and.md)
- [Logical OR (or)](logical_or.md)
- [Not Equal (!=)](not_equal.md)

---

**Remember**: `!` inverts **boolean** values - use for clarity!
