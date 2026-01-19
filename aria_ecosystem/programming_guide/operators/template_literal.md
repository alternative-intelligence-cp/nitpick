# Template Literal (``)

**Category**: Operators → String  
**Operator**: `` ` ``  
**Purpose**: Raw strings

---

See [Backtick (`)](backtick.md) for complete documentation.

---

## Quick Reference

```aria
// Raw string - no escapes
path: string = `C:\Users\name\file.txt`;

// Multi-line
sql: string = `
    SELECT * FROM users
    WHERE age > 18
`;

// No processing
regex: string = `\d+\.\d+`;
```

---

**Remember**: Backticks create **raw strings** - no escape processing!
