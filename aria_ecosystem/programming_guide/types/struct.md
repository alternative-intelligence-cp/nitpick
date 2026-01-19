# Structs

**Category**: Types → Composite  
**Syntax**: `struct { ... }`  
**Purpose**: Custom data structures

---

## Overview

Structs allow you to create **custom types** by grouping related data together.

---

## Declaration

```aria
// Define a struct
struct Person {
    name: string,
    age: i32,
    email: string
}

// Create instance
person: Person = {
    name = "Alice",
    age = 30,
    email = "alice@example.com"
};
```

---

## Accessing Fields

```aria
// Read fields
name: string = person.name;
age: i32 = person.age;

// Modify fields
person.age = 31;
person.email = "newemail@example.com";
```

---

## Methods

```aria
struct Rectangle {
    width: f32,
    height: f32
}

// Methods defined separately
fn area(self: &Rectangle) -> f32 {
    return self.width * self.height;
}

fn perimeter(self: &Rectangle) -> f32 {
    return 2.0 * (self.width + self.height);
}

// Use
rect: Rectangle = { width = 10.0, height = 5.0 };
a: f32 = rect.area();       // 50.0
p: f32 = rect.perimeter();  // 30.0
```

---

## Nested Structs

```aria
struct Address {
    street: string,
    city: string,
    zip: string
}

struct Employee {
    name: string,
    id: i32,
    address: Address
}

// Create
emp: Employee = {
    name = "Bob",
    id = 12345,
    address = {
        street = "123 Main St",
        city = "Boston",
        zip = "02101"
    }
};

// Access nested
city: string = emp.address.city;
```

---

## Best Practices

### ✅ DO: Group Related Data

```aria
// Good - cohesive struct
struct Point3D {
    x: f32,
    y: f32,
    z: f32
}
```

### ✅ DO: Use Clear Names

```aria
struct UserProfile {
    username: string,
    created_at: i64,
    is_active: bool
}
```

### ❌ DON'T: Make God Structs

```aria
// Bad - too many responsibilities
struct Everything {
    user_data: ...,
    settings: ...,
    cache: ...,
    network: ...
}
```

---

## Common Patterns

### Builder Pattern

```aria
struct Config {
    host: string,
    port: i32,
    timeout: i32
}

fn default_config() -> Config {
    return {
        host = "localhost",
        port = 8080,
        timeout = 30
    };
}

// Use
config: Config = default_config();
config.port = 9000;  // Override
```

### Data Transfer Objects

```aria
struct UserDTO {
    id: i32,
    name: string,
    email: string
}

fn get_user(id: i32) -> UserDTO {
    // Fetch and return
    ...
}
```

---

## Related

- [Struct Declaration](struct_declaration.md)
- [Struct Fields](struct_fields.md)
- [Struct Pointers](struct_pointers.md)
- [Generics](struct_generics.md)

---

**Remember**: Structs **group related data** into custom types!
