# `use` Statement

**Category**: Modules → Importing  
**Syntax**: `use <module_path>;` or `use <module_path>.*;`  
**Purpose**: Import modules and bring items into scope

---

## Overview

`use` is Aria's only import keyword. It imports modules and their public items into the current scope.

---

## Basic Module Import

```aria
// Import entire module with wildcard
use std.math.*;
Result: i32 = sqrt(16);  // No prefix needed
```

---

## Selective Imports

```aria
// Import specific items
use std.math.{sqrt, abs, pow};

a: i32 = sqrt(16);
b: i32 = abs(-5);
c: i32 = pow(2, 10);
```

---

## Wildcard Use

```aria
use std.math.*;

// All public items available
result1: i32 = sqrt(16);
result2: i32 = abs(-5);
result3: i32 = pow(2, 10);
```

---

## Aliasing

```aria
use std.collections.HashMap as Map;

map: Map<string, i32> = Map.new();
```

---

## Re-exporting

```aria
// In lib.aria
pub use internal.module.PublicType;
pub use internal.helpers.public_function;

// Users can now import directly from your crate
// import mylib;
// mylib.PublicType
// mylib.public_function()
```

---

## Common Patterns

### Import Types

```aria
use std.collections.HashMap;
use std.collections.HashSet;
use std.collections.Vec;

map: HashMap<string, i32> = HashMap.new();
set: HashSet<string> = HashSet.new();
vec: Vec<i32> = Vec.new();
```

### Import Functions

```aria
use std.io.readFile;
use std.io.writeFile;

content: string = readFile("input.txt")?;
writeFile("output.txt", content)?;
```

### Import Constants

```aria
use std.math.PI;
use std.math.E;

circumference: flt64 = 2.0 * PI * radius;
```

---

## Nested Use

```aria
use std.collections.{
    HashMap,
    HashSet,
    Vec,
    LinkedList
};
```

---

## Best Practices

### ✅ DO: Use for Frequently Used Items

```aria
use std.io.openFile;
use std.io.readFile;

// Cleaner repeated use
file1: File = openFile("data1.txt")?;
file2: File = openFile("data2.txt")?;
content: string = readFile("data3.txt")?;
```

### ✅ DO: Group Related Uses

```aria
// I/O operations
use std.io.{openFile, readFile, writeFile};

// Collections
use std.collections.{HashMap, HashSet, Vec};
```

### ❌ DON'T: Wildcard Everything

```aria
use std.*;        // ❌ Unclear what's being used
use database.*;   // ❌ Possible name conflicts
```

### ❌ DON'T: Alias to Confusing Names

```aria
use HashMap as M;  // ❌ What is M?

// Better
use HashMap as UserMap;  // ✅ Clear context
```

---

## Selective Use

```aria
// Only bring in what you need
use std.math.sqrt;  // ✅ Specific

// Not
use std.math.*;     // ❌ Everything
```

---

## Use in Modules

```aria
mod database {
    use std.io.openFile;
    
    pub fn connect() -> Result<Connection> {
        // Can use openFile directly
        file: File = openFile("config.db")?;
    }
}
```

---

## Related

- [import](import.md) - Importing modules
- [use_syntax](use_syntax.md) - Use syntax details
- [selective_imports](selective_imports.md) - Selective importing
- [wildcard_imports](wildcard_imports.md) - Wildcard imports

---

**Remember**: `use` saves typing, but use it **sparingly** for clarity!
