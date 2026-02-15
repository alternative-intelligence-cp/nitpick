# writeFile()

**Category**: Standard Library → I/O  
**Syntax**: `writeFile(path: string, content: string) -> Result<void>`  
**Purpose**: Write string to file (overwrites existing)

---

## Overview

`writeFile()` writes a string to a file, creating it if needed, **overwriting** if it exists.

---

## Syntax

```aria
import std.io;

Result: Result<void> = writeFile("output.txt", "Hello, World!");
```

---

## Parameters

- **path** (`string`) - File path to write
- **content** (`string`) - Data to write

---

## Returns

- `Result<void>` - Success or error

---

## Examples

### Basic Usage

```aria
import std.io;

// Write to file
Result: Result<void> = writeFile("greeting.txt", "Hello!");

when result is Ok() then
    stdout << "File written successfully";
elsif result is Err(msg) then
    stderr << "Write failed: $msg";
end
```

### With Error Propagation

```aria
func:save_config = Result<nil, string>(Config:config) {
    string:json = to_json(config);
    writeFile("config.json", json)?;
    pass(NULL);
}
```

### Generate Report

```aria
report: string = "System Report\n";
report += "=============\n";
report += "Status: OK\n";
report += "Uptime: $uptime seconds\n";

writeFile("report.txt", report)?;
```

---

## Overwrites Existing Files

```aria
// First write
writeFile("data.txt", "First content")?;

// Second write OVERWRITES
writeFile("data.txt", "New content")?;

// File now contains only "New content"
```

---

## Error Cases

- No write permission
- Disk full
- Path is a directory
- Invalid path

---

## Best Practices

### ✅ DO: Check for Errors

```aria
Result<nil>:Result = writeFile("output.txt", data);
when result is Err(msg) then
    log_error("Failed to write: $msg");
    fail("Save failed");
end
```

### ✅ DO: Create Backups for Important Files

```aria
// Backup before overwrite
when file_exists("config.json") then
    backup: string = readFile("config.json")?;
    writeFile("config.json.bak", backup)?;
end

writeFile("config.json", new_config)?;
```

### ❌ DON'T: Forget It Overwrites

```aria
// Existing file is LOST!
writeFile("important.txt", "new data")?;  // ⚠️ Original gone
```

### ⚠️ WARNING: Not Atomic

```aria
// If crash happens mid-write, file may be corrupted
writeFile("critical.dat", huge_data)?;

// Better - write to temp, then rename (atomic on most systems)
writeFile("critical.dat.tmp", huge_data)?;
rename("critical.dat.tmp", "critical.dat")?;
```

---

## Related

- [readFile()](readFile.md) - Read file contents
- [openFile()](openFile.md) - Open file handle (append mode)
- [appendFile()](appendFile.md) - Append without overwriting

---

**Remember**: `writeFile()` **OVERWRITES** existing files!
