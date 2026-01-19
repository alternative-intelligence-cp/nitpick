# When-Then Syntax Reference

**Category**: Control Flow → Syntax  
**Topic**: Complete when-then-end syntax

---

## Basic When-Then

```aria
when condition then
    // code
end
```

---

## When-Then-Else

```aria
when condition then
    // true branch
else
    // false branch
end
```

---

## When-Else-When Chain

```aria
when condition1 then
    // first branch
else when condition2 then
    // second branch
else when condition3 then
    // third branch
else
    // default branch
end
```

---

## Multi-Line Bodies

```aria
when is_valid then
    validate();
    process();
    commit();
else
    log_error();
    rollback();
end
```

---

## Condition Requirements

- Must be `bool` type
- No implicit truthiness
- Evaluated once before branching

```aria
// ✅ Valid
when x > 0 then
when is_ready then
when status == "active" then

// ❌ Invalid
when 1 then        // Not bool
when count then    // count must be bool
```

---

## Nesting

```aria
when outer then
    when inner then
        // nested
    end
end
```

---

## When vs If

Both are identical in behavior:

```aria
// when-then-end
when condition then
    code();
end

// if-else
if condition {
    code();
}
```

Choose based on preference or style guide.

---

## Examples

```aria
// Simple condition
when user.is_admin then
    grant_access();
end

// With else
when balance >= amount then
    withdraw(amount);
else
    fail "Insufficient funds";
end

// Chain
when score >= 90 then
    grade = "A";
else when score >= 80 then
    grade = "B";
else when score >= 70 then
    grade = "C";
else
    grade = "F";
end
```

---

## Related Topics

- [When-Then-End](when_then_end.md) - When-then guide
- [When-Then](when_then.md) - Concept
- [If Syntax](if_syntax.md) - If-else syntax

---

**Quick Reference**: `when bool then ... else ... end` - alternative to if-else
