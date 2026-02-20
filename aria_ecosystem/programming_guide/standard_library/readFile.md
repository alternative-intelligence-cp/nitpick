# readFile()

**Category**: Standard Library → I/O  
**Syntax**: `readFile(path: string) -> Result<string>`  
**Purpose**: Read entire file contents as string

---

## Overview

`readFile()` reads the complete contents of a file into a string.

---

## Syntax

```aria
import std.io;

content: Result<string> = readFile("data.txt");
```

---

## Parameters

- **path** (`string`) - File path to read

---

## Returns

- `Result<string>` - File contents on success, error on failure

---

## Examples

### Basic Usage

```aria
import std.io;

// Read file
Result: Result<string> = readFile("config.txt");

when result is Ok(content) then
    stdout << "File contents:\n$content";
elsif result is Err(msg) then
    stderr << "Error reading file: $msg";
end
```

### With Error Propagation

```aria
func:load_config = Result<Config, string>() {
    // ? propagates error if readFile fails
    string:content = readFile("config.json")?;
    Config:config = parse_json(content)?;
    pass(config);
}
```

### Read Multiple Files

```aria
files: []string = ["a.txt", "b.txt", "c.txt"];

till(files.length - 1, 1) {
    when readFile(files[$]) is Ok(content) then
        process(content);
    elsif is Err(msg) then
        stderr << "Failed to read $(files[$]): $msg";
    end
end
```

---

## Error Cases

- File doesn't exist
- No read permission
- Path is a directory
- I/O error

---

## Best Practices

### ✅ DO: Handle Errors

```aria
Result<string>:Result = readFile("important.txt");
when result is Err(msg) then
    log_error("Failed to read: $msg");
    fail("Cannot proceed");
end
```

### ✅ DO: Use for Text Files

```aria
// Perfect for config, text data
config: string = readFile("settings.json")?;
```

### ❌ DON'T: Use for Large Files

```aria
// Bad - loads entire 10GB file into memory!
huge: string = readFile("huge_log.txt")?;  // ❌

// Better - use streaming
stream: FileStream = openFile("huge_log.txt")?;
// Read in chunks
```

### ❌ DON'T: Use for Binary Files

```aria
// Bad - binary as string corrupts data
binary: string = readFile("image.png")?;  // ❌

// Better - use readBytes
data: []uint8 = readBytes("image.png")?;  // ✅
```

---

## Related

- [writeFile()](writeFile.md) - Write to file
- [openFile()](openFile.md) - Open file handle
- [readJSON()](readJSON.md) - Read and parse JSON
- [readCSV()](readCSV.md) - Read CSV files

---

**Remember**: `readFile()` loads **entire file** into memory!
