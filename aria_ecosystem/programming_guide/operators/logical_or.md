# Logical OR Operator (or)

**Category**: Operators → Logical  
**Operator**: `or`  
**Purpose**: Logical disjunction

---

## Syntax

```aria
<expression> or <expression>
```

---

## Description

The logical OR operator `or` returns `true` if **either** operand is `true`.

---

## Truth Table

```
a      b      | a or b
true   true   | true
true   false  | true
false  true   | true
false  false  | false
```

---

## Basic Usage

```aria
// Both true
when true or true then
    stdout << "At least one true";  // Prints
end

// One true
when true or false then
    stdout << "Prints";  // Prints
end

// Both false
when false or false then
    stdout << "Won't print";
end
```

---

## Short-Circuit Evaluation

```aria
// Right side NOT evaluated if left is true
when true or expensive_check() then
    // expensive_check() never called
end

// Useful for defaults
value: i32 = get_cached() or compute_expensive();
```

---

## Multiple Conditions

```aria
// Accept multiple valid states
when status == "pending" or status == "active" or status == "review" then
    allow_edit();
end
```

---

## Best Practices

### ✅ DO: Use for Alternatives

```aria
when is_admin or is_owner then
    allow_access();
end
```

### ✅ DO: Use for Defaults

```aria
// Use default if primary fails
config := load_config() or get_default_config();
```

### ✅ DO: Use for Error Checking

```aria
when error1 or error2 or error3 then
    fail "Validation failed";
end
```

### ❌ DON'T: Rely on Evaluation Order

```aria
// Unclear which runs first
when side_effect1() or side_effect2() then
    // side_effect2() might not run
end

// Clear
result1 := side_effect1();
result2 := side_effect2();
when result1 or result2 then
    ...
end
```

---

## Common Patterns

### Permission Check

```aria
when user.is_admin() or user.is_owner(resource) then
    grant_access();
end
```

### Exit Conditions

```aria
when is_error or is_cancelled or is_complete then
    cleanup();
    return;
end
```

### Validation

```aria
when input == "" or input.length() > 100 then
    fail "Invalid input length";
end
```

---

## Operator Precedence

```aria
// and has higher precedence than or
when a or b and c then
    // Evaluated as: a or (b and c)
end

// Use parentheses for clarity
when (a or b) and c then
    // Either a or b, AND c must be true
end
```

---

## With Negation

```aria
// !(a or b) == (!a and !b)
when !(is_locked or is_deleted) then
    // Same as: !is_locked and !is_deleted
    allow_edit();
end
```

---

## Related

- [Logical AND (and)](logical_and.md)
- [Logical NOT (!)](logical_not.md)
- [Bitwise OR (|)](bitwise_or.md)

---

**Remember**: `or` requires **at least one** true, **short-circuits** on true!
