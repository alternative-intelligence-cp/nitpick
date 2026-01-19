# The `obj` Type

**Category**: Types → Dynamic  
**Syntax**: `obj`  
**Purpose**: Generic object type for dynamic structures

---

## Overview

`obj` represents a **generic object** with dynamic fields - similar to JavaScript objects or Python dicts.

---

## Declaration

```aria
// Empty object
person: obj = {};

// With fields
person: obj = {
    name = "Alice",
    age = 30,
    active = true
};
```

---

## Field Access

```aria
person: obj = { name = "Alice", age = 30 };

// Access fields
name: string = person.name;
age: i32 = person.age;

// Add new fields
person.email = "alice@example.com";
```

---

## Dynamic Fields

```aria
person: obj = {};

// Add fields dynamically
person["name"] = "Bob";
person["age"] = 25;

// Access dynamically
key: string = "name";
value: dyn = person[key];  // "Bob"
```

---

## Nested Objects

```aria
user: obj = {
    name = "Alice",
    address = {
        street = "123 Main St",
        city = "Springfield"
    }
};

// Access nested
city: string = user.address.city;
```

---

## Use Cases

### Configuration

```aria
config: obj = {
    host = "localhost",
    port = 8080,
    debug = true
};
```

### JSON Parsing

```aria
// Parse JSON to obj
data: obj = parse_json(json_string);

// Access fields
name: string = data.user.name;
```

---

## Best Practices

### ✅ DO: Use for External Data

```aria
// Good for JSON, config files
config: obj = load_config("settings.json");
```

### ❌ DON'T: Use for Internal Structures

```aria
// Bad
user: obj = { name = "Alice", age = 30 };  // ❌

// Good - use struct instead
struct User {
    name: string,
    age: i32
}
user: User = { name = "Alice", age = 30 };  // ✅
```

### ⚠️ WARNING: No Compile-Time Type Checking

Objects don't have compile-time type safety!

---

## Related

- [dyn](dyn.md) - Dynamic type
- [struct](struct.md) - Type-safe structures
- [Dot (.) Operator](../operators/dot.md)

---

**Remember**: `obj` = **dynamic objects** - prefer structs when possible!
