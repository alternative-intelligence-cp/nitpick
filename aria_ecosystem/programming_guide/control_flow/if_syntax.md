# If Syntax Reference

**Category**: Control Flow → Syntax  
**Topic**: Complete if-else syntax

---

## Basic If

```aria
if condition {
    // code
}
```

---

## If-Else

```aria
if condition {
    // true branch
} else {
    // false branch
}
```

---

## If-Else-If

```aria
if condition1 {
    // first branch
} else if condition2 {
    // second branch
} else if condition3 {
    // third branch
} else {
    // default branch
}
```

---

## Condition Requirements

- Must be `bool` type
- No implicit truthiness
- Evaluated once before branching

```aria
// ✅ Valid conditions
if x > 0 { }
if is_valid { }
if name == "Alice" { }
if count != 0 && status == "ready" { }

// ❌ Invalid conditions
if 1 { }           // Not bool
if x { }           // x must be bool
if ptr { }         // No null checking
```

---

## Blocks

Curly braces required:

```aria
// ✅ Correct
if condition {
    statement;
}

// ❌ Wrong: No statement without braces
if condition
    statement;
```

---

## Nesting

```aria
if outer_condition {
    if inner_condition {
        // nested
    }
}
```

---

## Related Topics

- [If-Else](if_else.md) - If-else guide
- [When Syntax](when_syntax.md) - When-then-end syntax
- [Control Flow](../README.md) - All control flow

---

**Quick Reference**: `if bool { } else if bool { } else { }`
