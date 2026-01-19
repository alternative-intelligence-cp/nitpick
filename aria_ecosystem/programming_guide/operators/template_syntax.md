# Template Syntax

**Category**: Operators → String  
**Purpose**: String templates and interpolation

---

See [String Interpolation](interpolation.md) and [Template Literal](backtick.md) for complete documentation.

---

## Quick Reference

### Interpolation

```aria
// Embed variables
msg: string = "Hello, $name!";

// Embed expressions
text: string = "Total: ${price * quantity}";
```

### Raw Strings

```aria
// No escape processing
path: string = `C:\Windows\System32`;
```

### Multi-line

```aria
template: string = `
    Name: $name
    Age: $age
    Email: $email
`;
```

---

## Related

- [String Interpolation](interpolation.md)
- [Backtick (`)](backtick.md)
- [Strings](../types/string.md)

---

**Remember**: `$` for interpolation, `` ` `` for raw strings!
