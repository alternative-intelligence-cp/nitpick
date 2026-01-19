# Module Aliases

**Category**: Modules → Importing  
**Syntax**: `use <module> as <alias>;`  
**Purpose**: Rename modules and items for clarity or conflict resolution

---

## Overview

Aliases provide **alternative names** for imported modules or items.

---

## Basic Alias

```aria
use std.collections.HashMap as Map;

map: Map<string, i32> = Map.new();
```

---

## Module Alias

```aria
import very.long.module.path as short;

Result: i32 = short.function();
```

---

## When to Use Aliases

### ✅ Shorten Long Names

```aria
use std.collections.HashMap as Map;
use std.collections.HashSet as Set;
use std.collections.BTreeMap as Tree;

map: Map<string, i32> = Map.new();
set: Set<string> = Set.new();
tree: Tree<string, i32> = Tree.new();
```

### ✅ Resolve Conflicts

```aria
// Both modules have 'Client'
use http_client.Client as HttpClient;
use database.Client as DbClient;

http: HttpClient = HttpClient.new();
db: DbClient = DbClient.new();
```

### ✅ Clarify Purpose

```aria
use std.collections.HashMap as UserCache;
use std.collections.HashMap as ConfigMap;

users: UserCache<string, User> = UserCache.new();
config: ConfigMap<string, string> = ConfigMap.new();
```

---

## Alias Syntax

### Item Alias

```aria
use std.io.readFile as read;

content: string = read("data.txt")?;
```

### Type Alias

```aria
use std.collections.Vec as List;

items: List<i32> = List.new();
```

### Function Alias

```aria
use std.math.sqrt as squareRoot;

Result: i32 = squareRoot(16);
```

---

## Multiple Aliases

```aria
use std.collections.HashMap as Map;
use std.collections.HashSet as Set;
use std.collections.Vec as List;

map: Map<string, i32> = Map.new();
set: Set<string> = Set.new();
list: List<i32> = List.new();
```

---

## Grouped Aliases

```aria
use std.collections.{
    HashMap as Map,
    HashSet as Set,
    Vec as List
};
```

---

## Common Patterns

### Conflict Resolution

```aria
// Two different HTTP clients
use reqwest.Client as ReqwestClient;
use hyper.Client as HyperClient;

client1: ReqwestClient = ReqwestClient.new();
client2: HyperClient = HyperClient.new();
```

### Shortening

```aria
use deeply.nested.module.VeryLongTypeName as Short;

item: Short = Short.new();
```

### Contextual Clarity

```aria
use std.fs.File as FileHandle;
use database.Record as DatabaseRecord;

file: FileHandle = FileHandle.open("data.txt")?;
record: DatabaseRecord = DatabaseRecord.load()?;
```

---

## Best Practices

### ✅ DO: Use Descriptive Aliases

```aria
use std.collections.HashMap as UserCache;  // ✅ Clear purpose
```

### ✅ DO: Resolve Conflicts

```aria
use http.Response as HttpResponse;
use database.Response as DbResponse;  // ✅ Clear distinction
```

### ✅ DO: Shorten Long Names Reasonably

```aria
use very.long.nested.module.TypeName as TypeName;  // ✅ Remove path, keep name
```

### ❌ DON'T: Use Cryptic Aliases

```aria
use HashMap as M;   // ❌ What is M?
use Vec as V;       // ❌ Unclear
use String as S;    // ❌ Too short
```

### ❌ DON'T: Over-abbreviate

```aria
use createUser as cu;        // ❌ Not clear
use validateEmail as ve;     // ❌ Not clear
```

### ❌ DON'T: Alias Standard Names Unnecessarily

```aria
use Vec as Vector;   // ❌ Vec is already clear
use String as Str;   // ❌ String is clear
```

---

## Re-exports with Aliases

```aria
// In lib.aria
mod internal {
    pub struct ComplexInternalName { }
}

// Re-export with better name
pub use internal.ComplexInternalName as SimpleType;

// Users see:
// import mylib;
// item: mylib.SimpleType = ...
```

---

## Conditional Aliases

```aria
#[cfg(os = "linux")]
use linux_client.Client as PlatformClient;

#[cfg(os = "windows")]
use windows_client.Client as PlatformClient;

// Use PlatformClient regardless of OS
client: PlatformClient = PlatformClient.new();
```

---

## Alias Precedence

```aria
use std.collections.HashMap;
use std.collections.HashMap as Map;

// Both available
map1: HashMap<string, i32> = HashMap.new();
map2: Map<string, i32> = Map.new();
```

---

## Common Alias Examples

### Collections

```aria
use std.collections.{
    HashMap as Map,
    HashSet as Set,
    Vec as List,
    BTreeMap as OrderedMap
};
```

### Result Types

```aria
use std.result.Result as Res;
use std.option.Option as Opt;

fn process() -> Res<Data, Error> { }
fn find() -> Opt<Item> { }
```

### Platform-Specific

```aria
#[cfg(target = "wasm")]
use wasm_http.Client as HttpClient;

#[cfg(not(target = "wasm"))]
use std.http.Client as HttpClient;
```

---

## Related

- [use](use.md) - Use statement
- [import](import.md) - Importing modules
- [selective_imports](selective_imports.md) - Selective importing

---

**Remember**: Aliases should **clarify**, not **obscure**!
