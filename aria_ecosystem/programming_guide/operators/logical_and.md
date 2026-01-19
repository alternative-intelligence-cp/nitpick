# Logical AND Operator (and)

**Category**: Operators → Logical  
**Operator**: `and`  
**Purpose**: Logical conjunction

---

## Syntax

```aria
<expression> and <expression>
```

---

## Description

The logical AND operator `and` returns `true` only if **both** operands are `true`.

---

## Truth Table

```
a      b      | a and b
true   true   | true
true   false  | false
false  true   | false
false  false  | false
```

---

## Basic Usage

```aria
// Both true
when true and true then
    stdout << "Both true";  // Prints
end

// One false
when true and false then
    stdout << "Won't print";
end

// Variables
is_valid: bool = true;
is_ready: bool = true;

when is_valid and is_ready then
    process();
end
```

---

## Short-Circuit Evaluation

```aria
// Right side NOT evaluated if left is false
when false and expensive_check() then
    // expensive_check() never called
end

// Useful for null checks
when ptr != nil and ptr.is_valid() then
    // is_valid() only called if ptr not nil
end
```

---

## Multiple Conditions

```aria
// Chain multiple conditions
when age >= 18 and age <= 65 and is_citizen then
    stdout << "Can vote";
end

// Parentheses for clarity
when (age >= 18 and age <= 65) and is_citizen then
    stdout << "Can vote";
end
```

---

## Best Practices

### ✅ DO: Use for Multiple Requirements

```aria
when has_account and is_verified and balance > 0 then
    allow_withdrawal();
end
```

### ✅ DO: Use Short-Circuiting

```aria
// Check null first
when user != nil and user.is_active() then
    process(user);
end
```

### ✅ DO: Group Related Conditions

```aria
when (age >= 18 and age < 65) and (has_license and is_insured) then
    allow_driving();
end
```

### ❌ DON'T: Rely on Side Effects

```aria
// Wrong: Order matters!
when increment() > 5 and decrement() < 3 then
    // decrement() might not run
end

// Right: Separate side effects
count1 := increment();
count2 := decrement();
when count1 > 5 and count2 < 3 then
    ...
end
```

---

## Common Patterns

### Range Check

```aria
when x >= min and x <= max then
    stdout << "In range";
end
```

### Validation

```aria
when username != "" and password.length() >= 8 then
    create_account(username, password);
end
```

### Guard Clause

```aria
when !is_initialized and !is_shutting_down then
    fail "Invalid state";
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
    // Both a or b must be true, AND c must be true
end
```

---

## Related

- [Logical OR (or)](logical_or.md)
- [Logical NOT (!)](logical_not.md)
- [Bitwise AND (&)](bitwise_and.md)

---

**Remember**: `and` requires **both** to be true, **short-circuits** on false!
