# String Interpolation ($)

**Category**: Operators → String  
**Operator**: `$` in strings  
**Purpose**: Embed expressions

---

See [String Interpolation](interpolation.md) for complete documentation.

---

## Quick Reference

```aria
// Simple variable
name: string = "Alice";
msg: string = "Hello, $name!";

// Expression
x: i32 = 10;
text: string = "Value: ${x + 5}";

// Method calls
info: string = "Status: ${user.status()}";
```

---

**Remember**: `$variable` or `${expression}` in strings!
