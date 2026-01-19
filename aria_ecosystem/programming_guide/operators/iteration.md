# Iteration Variable ($)

**Category**: Operators → Loops  
**Operator**: `$` (in loops)  
**Purpose**: Mutable iteration variable

---

See [Dollar Variable](../control_flow/dollar_variable.md) for complete documentation.

---

## Quick Reference

```aria
// Mutable iteration variable
for $i in 0..10 {
    $i += 1;  // Can modify!
    stdout << $i;
}

// Immutable (default)
for i in 0..10 {
    i += 1;  // ❌ Error!
}
```

---

## Use Cases

- Skipping iterations
- Conditional increment
- Custom step size
- Loop variable modification

---

**Remember**: `$` makes iteration variable **mutable**!
