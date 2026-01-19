# Backtick Operator (`)

**Category**: Operators → String  
**Operator**: `` ` ``  
**Purpose**: Raw strings or identifiers

---

## Syntax

```aria
`raw string`
`identifier with spaces`
```

---

## Description

The backtick operator `` ` `` creates raw strings without escape sequences or allows special identifiers.

---

## Raw Strings

```aria
// Raw string - no escapes
path: string = `C:\Users\name\file.txt`;

// Regular string - need escapes
path2: string = "C:\\Users\\name\\file.txt";

// Multi-line raw string
sql: string = `
    SELECT *
    FROM users
    WHERE age > 18
`;
```

---

## Special Identifiers

```aria
// Identifier with spaces
`my variable`: i32 = 42;

// Reserved words as identifiers
`when`: i32 = 10;
`fn`: string = "function";
```

---

## Best Practices

### ✅ DO: Use for Paths

```aria
file_path: string = `C:\Windows\System32`;
```

### ✅ DO: Use for Regex

```aria
pattern: string = `\d+\.\d+\.\d+\.\d+`;
```

### ✅ DO: Use for SQL/Templates

```aria
query: string = `
    INSERT INTO users (name, email)
    VALUES ('$name', '$email')
`;
```

### ❌ DON'T: Use for Regular Strings

```aria
// Unnecessary
msg: string = `hello`;

// Better
msg: string = "hello";
```

---

## Related

- [String Interpolation](interpolation.md)
- [Strings](../types/string.md)
- [Escape Sequences](../types/string_escapes.md)

---

**Remember**: Backticks create **raw strings** - no escape processing!
