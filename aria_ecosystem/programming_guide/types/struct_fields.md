# Struct Fields

**Category**: Types → Structs  
**Purpose**: Working with struct fields

---

## Accessing Fields

```aria
struct Person {
    name: string,
    age: i32
}

person: Person = { name = "Alice", age = 30 };

// Read
name: string = person.name;

// Write
person.age = 31;
```

---

## Field Types

```aria
struct AllTypes {
    integer: i32,
    floating: f32,
    text: string,
    flag: bool,
    array: []i32,
    nested: Point
}
```

---

## Optional Fields

```aria
struct Config {
    required: string,
    optional: ?string  // Can be nil
}

config: Config = {
    required = "value",
    optional = nil
};
```

---

## Default Values

```aria
// Use function for defaults
fn default_settings() -> Settings {
    return {
        timeout = 30,
        retries = 3,
        verbose = false
    };
}
```

---

## Related

- [Structs](struct.md)
- [Struct Declaration](struct_declaration.md)

---

**Remember**: Access fields with **dot notation**!
