# Null Coalescing Operator (??)

**Category**: Operators → Safety  
**Operator**: `??`  
**Purpose**: Provide default for nil values

---

## Syntax

```aria
<optional> ?? <default>
```

---

## Description

The null coalescing operator `??` returns the left value if not `nil`, otherwise returns the right value.

---

## Basic Usage

```aria
value: i32? = get_optional();

// Use value or default
Result: i32 = value ?? 0;

// Equivalent to
Result: i32 = when value != nil then value else 0 end;
```

---

## With Strings

```aria
name: string? = get_name();
display: string = name ?? "Unknown";
```

---

## Chaining

```aria
// Try multiple sources
config: Config = load_user_config() 
    ?? load_default_config() 
    ?? DEFAULT_BUILTIN_CONFIG;
```

---

## Best Practices

### ✅ DO: Provide Defaults

```aria
port: i32 = config.port ?? 8080;
timeout: i32 = options.timeout ?? 30;
```

### ✅ DO: Chain Fallbacks

```aria
value := primary ?? secondary ?? default;
```

### ❌ DON'T: Use for Zero Values

```aria
// Wrong: 0 is not nil!
count: i32 = value ?? 10;  // Doesn't help if value is 0

// Right: Check explicitly
count: i32 = when value > 0 then value else 10 end;
```

---

## Related

- [Question Operator (?)](question_operator.md)
- [Safe Navigation (?. )](safe_nav.md)
- [Optional Types](../types/optional.md)

---

**Remember**: `??` provides **defaults** for `nil` values!
