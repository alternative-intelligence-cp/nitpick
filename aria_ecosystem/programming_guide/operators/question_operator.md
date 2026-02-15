# Question Operator (?)

**Category**: Operators → Error Handling  
**Operator**: `?`  
**Purpose**: Propagate errors or mark optional types

---

## Syntax

```aria
<result>?          // Error propagation
<type>?            // Optional type
```

---

## Description

The question operator `?` is used for error propagation and optional types.

---

## Error Propagation

```aria
func:read_file = Result<string, Error>(string:path) {
    File:file = open(path)?;  // Returns error if fails
    string:content = file.read()?;  // Returns error if fails
    pass(content);
}
```

---

## Optional Types

```aria
// Optional value
maybe: i32? = nil;
maybe = 42;

// Check before use
when maybe != nil then
    stdout << maybe;
end
```

---

## Unwrapping

```aria
value: i32? = get_optional();

// Unwrap with ?
Result: i32 = value?;  // Panics if nil

// Safe unwrap
when value != nil then
    Result: i32 = value;
end
```

---

## Best Practices

### ✅ DO: Use for Error Handling

```aria
func:process = Result<nil, Error>() {
    data := load()?;
    validate(data)?;
    save(data)?;
    pass(NULL);
}
```

### ✅ DO: Use for Optional Types

```aria
user: User? = find_user(id);
```

### ❌ DON'T: Overuse Unwrapping

```aria
// Dangerous
value: i32 = optional?;  // Panics if nil!

// Better
value: i32 = optional or 0;  // Default
```

---

## Related

- [Result Type](../types/result.md)
- [Optional Types](../types/optional.md)
- [Error Handling](../concepts/error_handling.md)

---

**Remember**: `?` **propagates errors** and marks **optional types**!
