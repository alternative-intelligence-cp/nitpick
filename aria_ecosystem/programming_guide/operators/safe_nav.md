# Safe Navigation Operator (?.)

**Category**: Operators → Safety  
**Operator**: `?.`  
**Purpose**: Safe member access on optional values

---

## Syntax

```aria
<optional>?.<member>
<optional>?.<method>()
```

---

## Description

The safe navigation operator `?.` safely accesses members on optional values, returning `nil` if the value is `nil`.

---

## Basic Usage

```aria
user: User? = find_user(id);

// Safe access - returns nil if user is nil
name: string? = user?.name;

// Chained access
email: string? = user?.profile?.email;
```

---

## Method Calls

```aria
Result: i32? = user?.calculate_score();

// Equivalent to
Result: i32? = when user != nil then
    user.calculate_score()
else
    nil
end
```

---

## Chaining

```aria
// All safe - returns nil if any step is nil
final: string? = object?.field1?.field2?.method()?.property;
```

---

## Best Practices

### ✅ DO: Use for Optional Chains

```aria
value: i32? = config?.database?.port;
```

### ✅ DO: Provide Defaults

```aria
port: i32 = config?.port or 8080;
```

### ❌ DON'T: Overuse Chaining

```aria
// Too long
result := a?.b?.c?.d?.e?.f?.g;

// Better: Break it up
intermediate := a?.b?.c;
when intermediate != nil then
    result := intermediate.d?.e?.f?.g;
end
```

---

## Related

- [Question Operator (?)](question_operator.md)
- [Null Coalescing (??)](null_coalescing.md)
- [Optional Types](../types/optional.md)

---

**Remember**: `?.` returns `nil` if **any step** is `nil`!
