# Modules

**Category**: Modules → Overview  
**Purpose**: Code organization and namespacing

---

## Overview

Modules organize code into logical units, provide **namespacing**, and control **visibility**.

---

## Module Basics

### File as Module

```aria
// math.aria - automatically becomes module 'math'

pub fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

pub fn multiply(a: i32, b: i32) -> i32 {
    return a * b;
}
```

### Using a Module

```aria
// main.aria
use math.*;

Result: i32 = add(5, 3);  // 8
```

---

## Module Declaration

### Inline Module

```aria
mod math {
    pub fn add(a: i32, b: i32) -> i32 {
        return a + b;
    }
}

// Use in same file
Result: i32 = math.add(5, 3);
```

### Module from File

```aria
// Reference external file
mod math;  // Loads math.aria

// Or from directory
mod utils;  // Loads utils/mod.aria
```

---

## Module Hierarchy

```
project/
├── main.aria
├── math.aria
└── utils/
    ├── mod.aria
    ├── string.aria
    └── array.aria
```

```aria
// main.aria
use math.*;
use utils.*;
use utils.string.*;

add(1, 2);
reverse("hello");
```

---

## Visibility

### Private by Default

```aria
// math.aria
fn helper() -> i32 {  // Private
    return 42;
}

pub fn public_fn() -> i32 {  // Public
    return helper();
}
```

### Public Items

```aria
pub fn function() { }       // Public function
pub struct Data { }         // Public struct
pub const MAX: i32 = 100;   // Public constant
```

---

## Re-exports

```aria
// lib.aria
mod internal;

// Re-export from internal module
pub use internal.important_function;
pub use internal.ImportantType;
```

---

## Namespace Path

```aria
// Full path
Result: i32 = std.math.sqrt(16);

// Import to shorten
import std.math;
Result: i32 = math.sqrt(16);

// Import specific function
use std.math.sqrt;
Result: i32 = sqrt(16);
```

---

## Common Patterns

### Library Structure

```
mylib/
├── lib.aria          # Main entry point
├── core/
│   ├── mod.aria
│   ├── types.aria
│   └── traits.aria
└── utils/
    ├── mod.aria
    └── helpers.aria
```

```aria
// lib.aria
pub mod core;
pub mod utils;

// Re-export commonly used items
pub use core.types.MainType;
pub use utils.helpers.helper_function;
```

### Feature Modules

```aria
mod database {
    pub fn connect() -> Connection { }
    pub fn query() -> Result<Data> { }
}

mod api {
    pub fn start_server() -> Result<void> { }
    pub fn handle_request() -> Response { }
}

mod auth {
    pub fn login() -> Result<User> { }
    pub fn verify_token() -> bool { }
}
```

---

## Best Practices

### ✅ DO: Organize by Feature

```aria
// Good structure
auth/
  login.aria
  register.aria
  tokens.aria
database/
  connection.aria
  queries.aria
api/
  routes.aria
  handlers.aria
```

### ✅ DO: Use Clear Names

```aria
mod user_management;  // ✅ Clear
mod um;               // ❌ Unclear
```

### ✅ DO: Control Visibility

```aria
// Public interface
pub fn create_user() -> User { }

// Private implementation
fn validate_email() -> bool { }  // Hidden
fn hash_password() -> string { } // Hidden
```

### ❌ DON'T: Deep Nesting

```aria
// Too deep
import app.modules.features.user.auth.login.handlers;  // ❌

// Better - flatten
import app.auth.login;  // ✅
```

---

## Related

- [mod keyword](mod_keyword.md) - Module declaration
- [import](import.md) - Importing modules
- [pub](pub.md) - Public visibility
- [use](use.md) - Bringing items into scope

---

**Remember**: Modules provide **organization** and **encapsulation**!
