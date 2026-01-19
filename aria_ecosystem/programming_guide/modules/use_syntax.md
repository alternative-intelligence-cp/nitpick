# `use` Syntax

**Category**: Modules → Importing  
**Purpose**: Detailed use statement syntax

---

## Overview

`use` has several syntax forms for importing items efficiently.

---

## Basic Syntax

```aria
use <path>::<item>;
```

```aria
use std.collections.HashMap;
use std.math.sqrt;
```

---

## Single Item

```aria
use std.io.openFile;

file: File = openFile("data.txt")?;
```

---

## Multiple Items (Separate)

```aria
use std.math.sqrt;
use std.math.abs;
use std.math.pow;
```

---

## Multiple Items (Grouped)

```aria
use std.math.{sqrt, abs, pow};

a: i32 = sqrt(16);
b: i32 = abs(-5);
c: i32 = pow(2, 10);
```

---

## Nested Groups

```aria
use std.collections.{
    HashMap,
    HashSet,
    Vec.{new as vec_new},  // Nested with alias
};
```

---

## Wildcard (All Items)

```aria
use std.math.*;

// All public items available
Result: i32 = sqrt(16);
```

---

## Aliasing

### Single Alias

```aria
use std.collections.HashMap as Map;

map: Map<string, i32> = Map.new();
```

### Multiple with Aliases

```aria
use std.collections.{
    HashMap as Map,
    HashSet as Set,
    Vec as Vector
};
```

---

## Self Keyword

```aria
use std.collections.HashMap.{self, Entry};

// 'self' imports HashMap itself
map: HashMap<string, i32> = HashMap.new();

// Also imports Entry
entry: Entry = ...;
```

---

## Module + Items

```aria
use std.io.{self, File, openFile};

// Can use module
file1: File = std.io.readFile("a.txt")?;

// Or items directly
file2: File = openFile("b.txt")?;
```

---

## Re-export with `pub use`

```aria
// In lib.aria
pub use internal.important_function;
pub use internal.ImportantType;

// External users can now use:
// import mylib;
// mylib.important_function()
// mylib.ImportantType
```

---

## Common Patterns

### Type Imports

```aria
use std.collections.{
    HashMap,
    HashSet,
    BTreeMap,
    Vec
};
```

### Function Imports

```aria
use std.io.{
    readFile,
    writeFile,
    openFile,
    deleteFile
};
```

### Mixed Imports

```aria
use std.http.{
    Client,          // Type
    get,             // Function
    post,            // Function
    StatusCode       // Enum
};
```

---

## Relative Paths

```aria
// In module 'app.services'
use super.models.User;          // Parent's module
use self.helpers.validate;      // Current module's item
use crate.utils.format;         // Crate root
```

---

## Conditional Use

```aria
#[cfg(os = "linux")]
use linux_module.function;

#[cfg(os = "windows")]
use windows_module.function;
```

---

## Best Practices

### ✅ DO: Group Related Imports

```aria
use std.collections.{
    HashMap,
    HashSet
};
```

### ✅ DO: Use Meaningful Aliases

```aria
use std.collections.HashMap as UserCache;  // ✅ Clear purpose
```

### ✅ DO: Explicit Over Wildcard

```aria
use std.math.{sqrt, abs, pow};  // ✅ Clear what's used
// Not: use std.math.*;
```

### ❌ DON'T: Over-alias

```aria
use HashMap as M;   // ❌ Cryptic
use Vec as V;       // ❌ Unclear
```

### ❌ DON'T: Deeply Nested Groups

```aria
use std.{
    io.{
        File.{
            self,
            open,
            read.{self, to_string}  // ❌ Too complex
        }
    }
};
```

---

## Syntax Reference

| Syntax | Meaning |
|--------|---------|
| `use std.io.File;` | Import single item |
| `use std.io.{File, openFile};` | Import multiple items |
| `use std.math.*;` | Import all public items |
| `use std.io.File as F;` | Import with alias |
| `use super.module.Item;` | Relative import |
| `pub use internal.Item;` | Re-export |
| `use std.io.{self, File};` | Import module + items |

---

## Related

- [use](use.md) - Use statement overview
- [import](import.md) - Importing modules
- [selective_imports](selective_imports.md) - Selective importing
- [module_aliases](module_aliases.md) - Module aliasing

---

**Remember**: Choose **clarity** over brevity!
