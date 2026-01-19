# Is Operator (is)

**Category**: Operators → Type Checking  
**Operator**: `is`  
**Purpose**: Type checking and pattern matching

---

## Syntax

```aria
<value> is <type>
<value> is <pattern>
```

---

## Description

The `is` operator checks if a value matches a type or pattern.

---

## Type Checking

```aria
value: any = 42;

when value is i32 then
    stdout << "Integer";
end

when value is string then
    stdout << "String";
end
```

---

## Pattern Matching

```aria
Result: Result<i32, Error> = compute();

when result is Ok(value) then
    stdout << "Success: " << value;
elsif result is Err(error) then
    stdout << "Error: " << error;
end
```

---

## With Unions

```aria
union Value {
    Int(i32),
    Float(f64),
    Text(string)
}

value: Value = Value::Int(42);

when value is Int(n) then
    stdout << "Integer: " << n;
end
```

---

## Best Practices

### ✅ DO: Use for Type Checking

```aria
when response is Error then
    handle_error(response);
end
```

### ✅ DO: Use for Pattern Matching

```aria
when msg is Some(text) then
    process(text);
end
```

### ❌ DON'T: Use for Equality

```aria
// Wrong: Use == for equality
when x is 42 then ...  // Type check, not value!

// Right
when x == 42 then ...
```

---

## Related

- [Type Checking](../types/type_checking.md)
- [Pattern Matching](../control_flow/pick_patterns.md)
- [Union Types](../types/unions.md)

---

**Remember**: `is` checks **types and patterns**, not **values**!
