# Struct Declaration

**Category**: Types → Structs  
**Purpose**: Defining struct types

---

## Basic Syntax

```aria
struct <Name> {
    <field>: <type>,
    <field>: <type>,
    ...
}
```

---

## Examples

### Simple Struct

```aria
struct Point {
    x: i32,
    y: i32
}
```

### With Multiple Types

```aria
struct User {
    id: i32,
    name: string,
    active: bool,
    score: f32
}
```

---

## Field Visibility

```aria
// Public by default
struct Public {
    visible: i32
}

// Private fields (with _ prefix, convention)
struct WithPrivate {
    _internal: i32,
    public: string
}
```

---

## Related

- [Structs](struct.md)
- [Struct Fields](struct_fields.md)

---

**Remember**: Use `struct` keyword to **define custom types**!
