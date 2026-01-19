# The `dyn` Type

**Category**: Types → Dynamic  
**Syntax**: `dyn`  
**Purpose**: Runtime-typed dynamic values

---

## Overview

`dyn` allows **runtime type flexibility** - value type is determined at runtime rather than compile time.

---

## Declaration

```aria
// Dynamic value
value: dyn = 42;

// Can hold different types
value = "hello";
value = [1, 2, 3];
value = { x = 10, y = 20 };
```

---

## Type Checking

```aria
value: dyn = 42;

// Check type at runtime
when value is i32 then
    stdout << "Integer: $(value as i32)";
elsif value is string then
    stdout << "String: $(value as string)";
end
```

---

## Type Casting

```aria
value: dyn = 42;

// Unsafe cast (panics if wrong type)
num: i32 = value as i32;

// Safe cast with pattern matching
when value is i32(n) then
    stdout << "Number: $n";
else
    stdout << "Not an integer";
end
```

---

## Use Cases

### Heterogeneous Collections

```aria
// Array of different types
items: []dyn = [42, "hello", 3.14, true];

for item in items do
    when item is i32(n) then
        stdout << "Integer: $n";
    elsif item is string(s) then
        stdout << "String: $s";
    end
end
```

### JSON-Like Data

```aria
// Dynamic object
data: dyn = {
    name = "Alice",
    age = 30,
    scores = [95, 87, 92]
};
```

---

## Best Practices

### ✅ DO: Use for Truly Dynamic Data

```aria
// JSON parsing
json_value: dyn = parse_json(text);
```

### ❌ DON'T: Overuse - Prefer Static Types

```aria
// Bad
value: dyn = 42;  // ❌ Use i32!

// Good
value: i32 = 42;  // ✅ Type-safe
```

### ⚠️ WARNING: Performance Cost

Dynamic types are **slower** than static types due to runtime type checking.

---

## Related

- [obj](obj.md) - Object type
- [Type Casting (as)](../operators/as.md)
- [Type Pattern Matching](../control_flow/when_is.md)

---

**Remember**: `dyn` = **runtime types** - use sparingly!
