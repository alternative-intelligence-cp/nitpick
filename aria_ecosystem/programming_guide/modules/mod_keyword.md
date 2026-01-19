# `mod` Keyword

**Category**: Modules → Declaration  
**Syntax**: `mod <name> { ... }` or `mod <name>;`  
**Purpose**: Declare or reference modules

---

## Overview

`mod` declares inline modules or references external module files.

---

## Inline Module

```aria
mod math {
    pub fn add(a: i32, b: i32) -> i32 {
        return a + b;
    }
    
    fn helper() -> i32 {  // Private to module
        return 42;
    }
}

// Use module
Result: i32 = math.add(5, 3);
```

---

## External Module File

```aria
// Reference external file math.aria
mod math;

// Use it
Result: i32 = math.add(5, 3);
```

---

## Module from Directory

```
utils/
├── mod.aria       # Module root
├── string.aria
└── array.aria
```

```aria
// Reference directory module
mod utils;

// Access submodules
utils.string.reverse("hello");
```

---

## Nested Modules

```aria
mod outer {
    pub mod inner {
        pub fn function() {
            stdout << "Inner function";
        }
    }
    
    pub fn outer_fn() {
        inner.function();  // Access nested
    }
}

// Use from outside
outer.inner.function();
outer.outer_fn();
```

---

## Visibility

```aria
mod private_mod {  // Module itself is private
    pub fn public_fn() { }  // Function is public within module
}

pub mod public_mod {  // Module is public
    pub fn public_fn() { }  // Accessible from outside
    fn private_fn() { }     // Not accessible
}
```

---

## Best Practices

### ✅ DO: One Module Per File

```aria
// math.aria
pub fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// main.aria
mod math;  // ✅ Clean
```

### ✅ DO: Use `mod.aria` for Directories

```
database/
  mod.aria        # Main module file
  connection.aria
  queries.aria
```

### ❌ DON'T: Nest Modules Too Deeply

```aria
mod level1 {
    mod level2 {
        mod level3 {
            mod level4 {  // ❌ Too deep!
            }
        }
    }
}
```

---

## Related

- [mod](mod.md) - Modules overview
- [import](import.md) - Importing modules
- [pub](pub.md) - Public visibility

---

**Remember**: `mod` **declares** modules, `import` **uses** them!
