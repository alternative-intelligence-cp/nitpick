# Module Paths

**Category**: Modules → Navigation  
**Purpose**: Accessing items through module paths

---

## Overview

Module paths specify the **location** of items in the module hierarchy.

---

## Absolute Paths

Start from crate root or external crate:

```aria
// Access from root
Result: i32 = std.math.sqrt(16);

// Access from your crate root
user: User = myapp.models.User.new();
```

---

## Relative Paths

### From Current Module

```aria
// In module 'app.database'
use super.models.User;     // Parent module's 'models'
use self.connection.connect; // Current module's 'connection'
```

---

## Path Components

### `self` - Current Module

```aria
mod database {
    pub fn connect() { }
    
    pub fn init() {
        self.connect();  // Same as just connect()
    }
}
```

### `super` - Parent Module

```aria
mod parent {
    pub fn parent_fn() { }
    
    mod child {
        pub fn use_parent() {
            super.parent_fn();  // Call parent's function
        }
    }
}
```

### `crate` - Crate Root

```aria
// From anywhere in your crate
use crate.models.User;
use crate.utils.helpers.format;
```

---

## Examples

### Simple Path

```aria
use std.io.*;

file: File = openFile("data.txt")?;
```

### Nested Path

```aria
use myapp.database.models.*;

user: User = User.new();
```

### With `super`

```
app/
├── models/
│   └── user.aria
└── services/
    └── user_service.aria
```

```aria
// services/user_service.aria
use super.models.user.User;  // Go up to 'app', then to 'models.user'

pub fn create_user(name: string) -> User {
    return User.new(name);
}
```

---

## Full vs Shortened Paths

```aria
// Full path every time
result1: i32 = std.math.abs(-5);
result2: i32 = std.math.sqrt(16);
result3: i32 = std.math.pow(2, 10);

// Import and shorten
import std.math;
result1: i32 = math.abs(-5);
result2: i32 = math.sqrt(16);
result3: i32 = math.pow(2, 10);

// Import specific items
use std.math.abs;
use std.math.sqrt;
use std.math.pow;
result1: i32 = abs(-5);
result2: i32 = sqrt(16);
result3: i32 = pow(2, 10);
```

---

## Common Patterns

### Standard Library

```aria
import std.io;          // I/O operations
import std.collections; // Data structures
import std.http;        // HTTP client
```

### Internal Modules

```aria
use crate.models.User;
use crate.database.connect;
use crate.utils.validators.validate_email;
```

### Parent Module Items

```aria
mod parent {
    pub const CONFIG: Config = load_config();
    
    mod child {
        pub fn use_config() {
            config: Config = super.CONFIG;
        }
    }
}
```

---

## Path Resolution

```
myapp/
├── lib.aria
├── models/
│   ├── mod.aria
│   └── user.aria
└── services/
    ├── mod.aria
    └── user_service.aria
```

```aria
// From services/user_service.aria

// Absolute from crate root
use crate.models.user.User;

// Relative from parent
use super.super.models.user.User;  // services -> myapp -> models -> user

// Via module declaration
use myapp.models.user.User;
```

---

## Best Practices

### ✅ DO: Use Absolute Paths for Clarity

```aria
use crate.database.models.User;  // ✅ Clear
```

### ✅ DO: Use `super` for Sibling Modules

```aria
// In auth/tokens.aria
use super.login.verify_credentials;  // ✅ Sibling module
```

### ❌ DON'T: Over-use `super`

```aria
// Too many levels
use super.super.super.some_module;  // ❌ Hard to follow

// Better - use absolute
use crate.some_module;  // ✅ Clearer
```

---

## Related

- [import](import.md) - Importing modules
- [use](use.md) - Bringing items into scope
- [nested_modules](nested_modules.md) - Nested module structure

---

**Remember**: Use **absolute paths** for clarity, **relative** for nearby modules!
