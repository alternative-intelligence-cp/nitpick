# Iteration Variable ($)

**Category**: Operators → Loops  
**Operator**: `$` (in loops)  
**Purpose**: Mutable iteration variable

---

See [Dollar Variable](../control_flow/dollar_variable.md) for complete documentation.

---

## Quick Reference

**Note**: Aria uses `till` for loops, NOT `for-in`. The `$` variable is automatically provided by `till`.

```aria
// $ is the mutable iteration variable in till loops
till(9, 1) {
    process($);  // $ goes from 0 to 9
}

// ❌ WRONG: Aria doesn't use for-in loops
// for i in 0..10 { ... }  // This is Rust syntax!
```

---

## Use Cases

- Skipping iterations
- Conditional increment
- Custom step size
- Loop variable modification

---

**Remember**: `$` makes iteration variable **mutable**!
