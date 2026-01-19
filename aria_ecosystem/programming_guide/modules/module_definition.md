# Module Definition

**Category**: Modules → Structure  
**Purpose**: How to define and structure modules

---

## Overview

Modules are defined either as **files** or **inline declarations**.

---

## File-Based Modules

### Single File

```aria
// math.aria
pub fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

pub fn subtract(a: i32, b: i32) -> i32 {
    return a - b;
}
```

File `math.aria` automatically becomes module `math`.

---

### Directory Module

```
utils/
├── mod.aria       # Module entry point
├── string.aria    # Submodule
└── array.aria     # Submodule
```

```aria
// utils/mod.aria
pub mod string;
pub mod array;

pub fn common_util() {
    // Shared utility
}
```

```aria
// utils/string.aria
pub fn reverse(s: string) -> string {
    // Implementation
}
```

---

## Inline Modules

```aria
// main.aria
mod helpers {
    pub fn format_output(value: i32) -> string {
        return "Value: $value";
    }
}

fn main() {
    output: string = helpers.format_output(42);
    stdout << output;
}
```

---

## Module Structure

### Typical Module

```aria
// database.aria

// Private constants
const CONNECTION_TIMEOUT: i32 = 30;

// Private types
struct ConnectionPool {
    connections: []Connection
}

// Public API
pub fn connect(url: string) -> Result<Connection> {
    // Use private items
    pool: ConnectionPool = get_pool();
    return pool.acquire();
}

pub fn query(sql: string) -> Result<Data> {
    // Implementation
}

// Private helper
fn get_pool() -> ConnectionPool {
    // Implementation
}
```

---

## Module Organization

### Flat Structure

```
src/
├── main.aria
├── auth.aria
├── database.aria
└── api.aria
```

```aria
// main.aria
mod auth;
mod database;
mod api;
```

---

### Hierarchical Structure

```
src/
├── main.aria
├── auth/
│   ├── mod.aria
│   ├── login.aria
│   └── tokens.aria
└── database/
    ├── mod.aria
    ├── connection.aria
    └── queries.aria
```

```aria
// main.aria
mod auth;
mod database;

// auth/mod.aria
pub mod login;
pub mod tokens;
```

---

## Module Items

### What Can Be in a Module?

```aria
// All valid module items:

pub const MAX_SIZE: i32 = 1000;        // Constants

pub struct User {                      // Structs
    name: string,
    age: i32
}

pub enum Status {                      // Enums
    Active,
    Inactive
}

pub fn process() -> Result<void> {     // Functions
    // Implementation
}

pub mod submodule {                    // Nested modules
    pub fn helper() { }
}
```

---

## Best Practices

### ✅ DO: Group Related Functionality

```aria
// user.aria - everything user-related
pub struct User { }
pub fn create_user() -> User { }
pub fn delete_user() { }
pub fn find_user() -> ?User { }
```

### ✅ DO: Use mod.aria for Complex Modules

```
database/
  mod.aria          # Public API
  connection.aria   # Private implementation
  pool.aria         # Private implementation
  queries.aria      # Private implementation
```

```aria
// database/mod.aria
mod connection;
mod pool;
mod queries;

// Re-export public API
pub use connection.connect;
pub use queries.query;
```

### ❌ DON'T: Mix Unrelated Code

```aria
// bad_module.aria
pub fn user_login() { }
pub fn database_query() { }  // ❌ Different concerns
pub fn send_email() { }      // ❌ Different concerns
```

---

## Related

- [mod](mod.md) - Modules overview
- [mod_keyword](mod_keyword.md) - mod keyword
- [module_paths](module_paths.md) - Module paths

---

**Remember**: Keep modules **focused** on one responsibility!
