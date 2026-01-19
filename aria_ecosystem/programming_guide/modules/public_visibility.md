# Public Visibility

**Category**: Modules → Visibility  
**Purpose**: Understanding visibility and access control

---

## Overview

Visibility controls which items can be accessed from outside their defining module.

---

## Visibility Levels

### Private (Default)

```aria
fn function() { }           // Private to module
struct Data { }             // Private to module
const VALUE: i32 = 42;      // Private to module
```

### Public

```aria
pub fn function() { }       // Public everywhere
pub struct Data { }         // Public everywhere
pub const VALUE: i32 = 42;  // Public everywhere
```

### Public in Crate

```aria
pub(crate) fn function() { }  // Public within crate only
```

---

## Module Boundaries

```aria
mod database {
    fn private_helper() { }    // Only in 'database'
    pub fn connect() { }       // Available outside 'database'
}

// From outside
database.connect();      // ✅ OK - public
database.private_helper(); // ❌ Error - private
```

---

## Struct Visibility

### Public Struct, Public Fields

```aria
pub struct Config {
    pub host: string,
    pub port: i32,
}

config: Config = Config {
    host: "localhost",
    port: 8080
};
```

---

### Public Struct, Private Fields

```aria
pub struct User {
    name: string,     // Private
    age: i32,         // Private
}

impl User {
    pub fn new(name: string, age: i32) -> User {
        return User { name: name, age: age };
    }
    
    pub fn get_name() -> string {
        return self.name;
    }
}

// From outside
user: User = User.new("Alice", 30);  // ✅ Public constructor
name: string = user.get_name();      // ✅ Public method
direct: string = user.name;          // ❌ Error - private field
```

---

### Private Struct

```aria
struct InternalData {  // Private struct
    value: i32
}

pub fn create() -> InternalData {  // ❌ Error - can't expose private type
}
```

---

## Nested Module Visibility

```aria
pub mod outer {
    pub fn outer_public() { }
    fn outer_private() { }
    
    pub mod inner {
        pub fn inner_public() { }
        fn inner_private() { }
        
        pub fn use_outer() {
            super.outer_public();   // ✅ OK
            super.outer_private();  // ✅ OK - same parent module
        }
    }
}

// From outside
outer.outer_public();        // ✅ OK
outer.inner.inner_public();  // ✅ OK
outer.outer_private();       // ❌ Error
outer.inner.inner_private(); // ❌ Error
```

---

## Re-exporting

```aria
// internal.aria
pub fn helper() { }

// lib.aria
mod internal;

// Re-export to public API
pub use internal.helper;

// Users can now:
// import mylib;
// mylib.helper();
```

---

## Crate-Level Visibility

```aria
// Only public within your crate, not to external users
pub(crate) fn internal_helper() { }
pub(crate) struct InternalType { }
```

---

## Common Patterns

### Public API, Private Implementation

```aria
// lib.aria
mod database {
    struct Connection { }  // Private
    
    fn connect_internal() -> Connection { }  // Private
}

pub fn connect_to_database() -> Result<void> {
    database.connect_internal()?;  // Use private items
    return Ok();
}
```

---

### Encapsulation

```aria
pub struct Account {
    balance: i32,  // Private - can't be directly modified
}

impl Account {
    pub fn new() -> Account {
        return Account { balance: 0 };
    }
    
    pub fn deposit(amount: i32) {
        self.balance += amount;
    }
    
    pub fn get_balance() -> i32 {
        return self.balance;
    }
}

// Users can't directly modify balance
account: Account = Account.new();
account.deposit(100);           // ✅ OK
balance: i32 = account.get_balance();  // ✅ OK
account.balance = 1000;         // ❌ Error - private field
```

---

### Selective Exposure

```aria
mod utils {
    pub fn public_util() { }    // Public
    fn internal_util() { }      // Private
    fn helper() { }             // Private
}

// Only expose specific items
pub use utils.public_util;
```

---

## Visibility Rules

### Functions

```aria
fn private_fn() { }      // Private to module
pub fn public_fn() { }   // Public everywhere
```

### Structs

```aria
struct PrivateStruct { }        // Private struct
pub struct PublicStruct { }     // Public struct

pub struct MixedStruct {
    pub public_field: i32,      // Public field
    private_field: string,      // Private field
}
```

### Enums

```aria
enum PrivateEnum { A, B }       // Private enum
pub enum PublicEnum { A, B }    // Public enum (all variants public)
```

### Constants

```aria
const PRIVATE: i32 = 42;        // Private constant
pub const PUBLIC: i32 = 42;     // Public constant
```

---

## Best Practices

### ✅ DO: Minimize Public Surface

```aria
// Expose only what's needed
pub fn api_function() { }

// Keep helpers private
fn helper1() { }
fn helper2() { }
fn validate() { }
```

### ✅ DO: Encapsulate State

```aria
pub struct Server {
    socket: Socket,     // Private - implementation detail
    is_running: bool,   // Private - internal state
}

impl Server {
    pub fn start() { }  // Public interface
    pub fn stop() { }   // Public interface
    fn cleanup() { }    // Private helper
}
```

### ✅ DO: Use Re-exports for Clean API

```aria
// lib.aria
mod internal {
    pub fn important_function() { }
}

// Clean public API
pub use internal.important_function;
```

### ❌ DON'T: Expose Implementation Details

```aria
pub fn process_user(user: User) {
    internal_validate(user);  // ❌ Don't expose this
    internal_save(user);      // ❌ Or this
}

pub fn internal_validate(user: User) { }  // ❌ Should be private
pub fn internal_save(user: User) { }      // ❌ Should be private
```

---

## Visibility Checklist

- ✅ Is this item part of your public API?
- ✅ Do external users need direct access?
- ✅ Can you provide a better abstraction?
- ✅ Will making this public lock you into maintaining it?

**When in doubt, keep it private** - you can always make things public later!

---

## Related

- [pub](pub.md) - pub keyword
- [mod](mod.md) - Modules overview
- [use](use.md) - Importing items

---

**Remember**: **Private by default** - make things public only when necessary!
