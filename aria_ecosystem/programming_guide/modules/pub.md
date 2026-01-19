# `pub` Keyword

**Category**: Modules → Visibility  
**Syntax**: `pub <item>`  
**Purpose**: Make items publicly accessible

---

## Overview

`pub` makes items **visible** outside their defining module.

---

## Default Visibility

By default, items are **private**:

```aria
fn private_function() { }     // Private
struct PrivateStruct { }      // Private
const PRIVATE: i32 = 42;      // Private
```

---

## Public Items

```aria
pub fn public_function() { }     // Public
pub struct PublicStruct { }      // Public
pub const PUBLIC: i32 = 42;      // Public
```

---

## What Can Be Public?

```aria
pub fn function() { }           // Functions
pub struct Data { }             // Structs
pub enum Status { }             // Enums
pub const MAX: i32 = 100;       // Constants
pub type Alias = i32;           // Type aliases
pub mod submodule { }           // Modules
```

---

## Struct Field Visibility

```aria
pub struct User {
    pub name: string,     // Public field
    pub email: string,    // Public field
    age: i32,             // Private field
    password_hash: string // Private field
}
```

---

## Module Visibility

```aria
// Module is private
mod internal {
    pub fn helper() { }  // Function is public within module
}

// Module is public
pub mod api {
    pub fn endpoint() { }  // Public everywhere
    fn internal() { }      // Private to api module
}
```

---

## Public API Example

```aria
// lib.aria
mod internal {  // Private module
    pub fn helper() -> i32 {
        return 42;
    }
}

pub fn public_api() -> i32 {  // Public function
    return internal.helper();  // Can use internal items
}
```

---

## Nested Visibility

```aria
pub mod outer {
    pub fn outer_fn() { }
    fn outer_private() { }
    
    pub mod inner {
        pub fn inner_fn() { }
        fn inner_private() { }
    }
}

// Accessible
outer.outer_fn();
outer.inner.inner_fn();

// Not accessible
outer.outer_private();      // ❌ Private
outer.inner.inner_private(); // ❌ Private
```

---

## Re-exporting

```aria
mod internal {
    pub fn important() { }
}

// Re-export to make available at crate root
pub use internal.important;

// Users can now:
// import mycrate;
// mycrate.important();
```

---

## Common Patterns

### Public Interface, Private Implementation

```aria
// Public API
pub fn create_user(name: string) -> User {
    return User {
        name: name,
        id: generate_id(),  // Private helper
    };
}

// Private implementation
fn generate_id() -> i32 {
    // Implementation details
}
```

---

### Struct with Public Fields

```aria
pub struct Config {
    pub host: string,
    pub port: i32,
    pub timeout: i32,
}
```

---

### Struct with Private Fields

```aria
pub struct Connection {
    handle: *void,      // Private - internal detail
    is_open: bool,      // Private - internal state
}

impl Connection {
    pub fn new() -> Connection {  // Public constructor
        // Implementation
    }
    
    pub fn query() -> Result<Data> {  // Public method
        // Implementation
    }
}
```

---

## Best Practices

### ✅ DO: Expose Minimal API

```aria
pub fn create_user() -> User { }  // Public API
fn validate_email() -> bool { }   // Private helper
fn hash_password() -> string { }  // Private helper
```

### ✅ DO: Keep Implementation Private

```aria
pub struct Database {
    connection_pool: Pool,  // Private - implementation detail
}

impl Database {
    pub fn query() { }      // Public interface
    fn reconnect() { }      // Private helper
}
```

### ✅ DO: Public Struct, Private Fields with Methods

```aria
pub struct User {
    name: string,  // Private
    age: i32,      // Private
}

impl User {
    pub fn new(name: string, age: i32) -> User { }
    pub fn get_name() -> string { }
    pub fn get_age() -> i32 { }
}
```

### ❌ DON'T: Make Everything Public

```aria
pub fn helper1() { }  // ❌ If only used internally
pub fn helper2() { }  // ❌ Keep private
pub fn helper3() { }  // ❌ Exposes implementation
```

---

## Visibility Levels

```aria
// Private (default) - only in same module
fn private() { }

// Public - accessible from anywhere
pub fn public() { }

// Public in crate - accessible within crate only (not exported)
pub(crate) fn crate_public() { }
```

---

## Related

- [public_visibility](public_visibility.md) - Visibility rules
- [mod](mod.md) - Modules overview

---

**Remember**: **Minimize** public API - you can always make private items public later!
