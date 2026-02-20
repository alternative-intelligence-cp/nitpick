# readJSON()

**Category**: Standard Library → I/O  
**Syntax**: `readJSON(path: string) -> Result<obj>`  
**Purpose**: Read and parse JSON file

---

## Overview

`readJSON()` reads a JSON file and returns a parsed object.

---

## Syntax

```aria
import std.json;

data: Result<obj> = readJSON("config.json");
```

---

## Parameters

- **path** (`string`) - Path to JSON file

---

## Returns

- `Result<obj>` - Parsed JSON object on success, error on failure

---

## Examples

### Basic Usage

```aria
import std.json;

// Read JSON file
Result: Result<obj> = readJSON("config.json");

when result is Ok(data) then
    host: string = data.server.host;
    port: i32 = data.server.port;
    stdout << "Server: $host:$port";
elsif result is Err(msg) then
    stderr << "Failed to read JSON: $msg";
end
```

### Parse into Struct

```aria
struct Config {
    host: string,
    port: i32,
    debug: bool
}

// Read and parse
obj_data: obj = readJSON("config.json")?;

// Map to struct
config: Config = {
    host = obj_data.host,
    port = obj_data.port,
    debug = obj_data.debug
};
```

### Handle Arrays

```aria
// JSON: { "items": [1, 2, 3, 4, 5] }
data: obj = readJSON("data.json")?;

items: []i32 = data.items;  // Array from JSON

till(items.length - 1, 1) {
    stdout << "Item: $(items[$])";
end
```

### Nested Objects

```aria
// JSON: { "user": { "name": "Alice", "age": 30 } }
data: obj = readJSON("user.json")?;

name: string = data.user.name;
age: i32 = data.user.age;

stdout << "$name is $age years old";
```

---

## Error Cases

- File doesn't exist
- Invalid JSON syntax
- No read permission
- I/O error

---

## Best Practices

### ✅ DO: Validate Structure

```aria
obj:data = readJSON("config.json")?;

// Check required fields exist
when not data.has_field("host") then
    fail("Missing 'host' field");
end
```

### ✅ DO: Handle Type Mismatches

```aria
data: obj = readJSON("data.json")?;

// Safe access with type checking
when data.port is i32(p) then
    port: i32 = p;
else
    stderr << "Port is not an integer";
end
```

### ❌ DON'T: Assume Structure

```aria
data: obj = readJSON("unknown.json")?;

// Dangerous - field might not exist!
value: string = data.some.nested.field;  // ❌ May crash

// Better - check first
when data.has_field("some") then
    // Safe to access
end
```

---

## Manual Parsing

```aria
// Equivalent to readJSON
content: string = readFile("data.json")?;
data: obj = parse_json(content)?;
```

---

## Related

- [readFile()](readFile.md) - Read raw file
- [writeJSON()](writeJSON.md) - Write JSON file
- [parse_json()](parse_json.md) - Parse JSON string
- [readCSV()](readCSV.md) - Read CSV files

---

**Remember**: JSON files are parsed into `obj` type!
