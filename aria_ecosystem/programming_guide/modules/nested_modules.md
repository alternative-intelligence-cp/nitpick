# Nested Modules

**Category**: Modules → Structure  
**Purpose**: Organizing modules hierarchically

---

## Overview

Modules can contain other modules, creating a **hierarchy** for better organization.

---

## Inline Nested Modules

```aria
mod outer {
    pub fn outer_function() {
        stdout << "Outer";
    }
    
    pub mod inner {
        pub fn inner_function() {
            stdout << "Inner";
        }
    }
}

// Access
outer.outer_function();
outer.inner.inner_function();
```

---

## File-Based Nested Modules

### Directory Structure

```
database/
├── mod.aria          # database module root
├── connection.aria   # database.connection
└── models/
    ├── mod.aria      # database.models
    ├── user.aria     # database.models.user
    └── post.aria     # database.models.post
```

### Module Declarations

```aria
// database/mod.aria
pub mod connection;
pub mod models;

pub fn init() {
    connection.connect()?;
}
```

```aria
// database/models/mod.aria
pub mod user;
pub mod post;
```

---

## Accessing Nested Modules

```aria
// Import parent
import database;

// Access nested
user: database.models.user.User = database.models.user.create();

// Or import nested directly
import database.models.user;

user: user.User = user.create();
```

---

## Visibility in Nested Modules

```aria
mod parent {
    fn private_parent() { }      // Private to parent
    pub fn public_parent() { }   // Public
    
    pub mod child {
        fn private_child() { }   // Private to child
        pub fn public_child() {  // Public
            // Can access parent's private items
            super.private_parent();
        }
    }
}

// From outside
parent.public_parent();           // ✅ OK
parent.child.public_child();      // ✅ OK
parent.private_parent();          // ❌ Error
parent.child.private_child();     // ❌ Error
```

---

## Common Patterns

### Feature Modules

```
app/
├── auth/
│   ├── mod.aria
│   ├── login.aria
│   ├── register.aria
│   └── tokens/
│       ├── mod.aria
│       ├── jwt.aria
│       └── refresh.aria
├── database/
│   ├── mod.aria
│   ├── connection.aria
│   └── models/
│       ├── mod.aria
│       ├── user.aria
│       └── post.aria
└── api/
    ├── mod.aria
    ├── routes.aria
    └── handlers/
        ├── mod.aria
        ├── users.aria
        └── posts.aria
```

---

### Library Organization

```aria
// lib.aria - Main entry point
pub mod core {
    pub mod types;
    pub mod traits;
}

pub mod utils {
    pub mod string;
    pub mod math;
}

pub mod io {
    pub mod file;
    pub mod network;
}

// Re-export commonly used items
pub use core.types.MainType;
pub use utils.string.format;
```

---

## Best Practices

### ✅ DO: Group Related Functionality

```
user_management/
  mod.aria
  create.aria       # Create user
  update.aria       # Update user
  delete.aria       # Delete user
  validation/       # Nested validation
    mod.aria
    email.aria
    password.aria
```

### ✅ DO: Keep Hierarchy Shallow

```
// Good - 2-3 levels
app.database.models.user  // ✅

// Too deep
app.modules.features.user.management.operations.create  // ❌
```

### ✅ DO: Use mod.aria for Public API

```aria
// database/mod.aria
mod connection;   // Private
mod pool;         // Private

pub mod models;   // Public nested module

// Re-export public API
pub use connection.connect;
pub use connection.disconnect;
```

### ❌ DON'T: Over-nest

```aria
mod level1 {
    mod level2 {
        mod level3 {
            mod level4 {
                mod level5 {  // ❌ Way too deep!
                }
            }
        }
    }
}
```

---

## Accessing Parent Items

```aria
mod parent {
    pub const CONFIG: string = "config";
    
    pub mod child {
        pub fn use_parent_config() {
            // Access parent's item
            config: string = super.CONFIG;
        }
        
        pub mod grandchild {
            pub fn use_grandparent_config() {
                // Access grandparent's item
                config: string = super.super.CONFIG;
            }
        }
    }
}
```

---

## Re-exporting from Nested Modules

```aria
// lib.aria
mod internal {
    pub mod deeply {
        pub mod nested {
            pub struct Important { }
        }
    }
}

// Flatten for users
pub use internal.deeply.nested.Important;

// Users can now do:
// import mylib;
// item: mylib.Important = ...
// Instead of: mylib.internal.deeply.nested.Important
```

---

## Related

- [mod](mod.md) - Modules overview
- [module_paths](module_paths.md) - Module path navigation
- [pub](pub.md) - Visibility control

---

**Remember**: Keep nesting **shallow** - aim for 2-3 levels max!
