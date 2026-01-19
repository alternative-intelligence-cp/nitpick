# The `writeFile()` Function

**Category**: Standard Library → I/O  
**Syntax**: `Result[void]:writeFile(string:path, string:content)`  
**Purpose**: Write string data to file with error handling

---

## Overview

The `writeFile()` function writes content to a file, creating it if it doesn't exist or overwriting it if it does. It:

- Returns **`Result[void]`** type (success or error)
- **Creates or overwrites** the target file
- Writes **text data** as UTF-8 by default
- Provides **automatic error handling**

**Philosophy**: Safe file writing with explicit error handling via Result type.

---

## Basic Syntax

```aria
Result[void]:result = writeFile("output.txt", "Hello, World!");

result
    .onSuccess(_ => print("File written successfully"))
    .onError(err => print(`Error: &{err}`));
```

---

## Example 1: Basic File Writing

```aria
string:data = "This is my file content\nWith multiple lines";

Result[void]:result = writeFile("output.txt", data);

if (result.isOk()) {
    print("File saved!");
} else {
    string:error = result.err().unwrap();
    print(`Failed to write: &{error}`);
}
```

---

## Example 2: Save Configuration

```aria
func:saveConfig = Result[void](Config:cfg) {
    string:json = cfg.toJSON();
    
    Result[void]:result = writeFile("app.config", json);
    
    pass(result);
};

Config:config = { theme: "dark", fontSize: 12 };
saveConfig(config)
    .onSuccess(_ => print("Config saved"))
    .onError(err => print(`Failed: &{err}`));
```

---

## Example 3: Generate Report

```aria
func:generateReport = Result[void](int64[]:data) {
    string:report = "=== Report ===\n";
    
    till(data.length - 1, 1) {
        report += `Item &{$}: &{data[$]}\n`;
    }
    
    report += `Total: &{sum(data)}\n`;
    report += "==============\n";
    
    pass(writeFile("report.txt", report));
};

int64[]:sales = [100, 250, 175, 300];
generateReport(sales);
```

---

## Error Handling Patterns

### Pattern 1: Early Return on Error

```aria
func:saveData = Result[void](string:data) {
    Result[void]:result = writeFile("data.txt", data);
    
    if (result.isErr()) {
        fail(`Cannot save data: &{result.err().unwrap()}`);
    }
    
    print("Data saved successfully");
    pass(Result.ok());
};
```

### Pattern 2: Retry on Failure

```aria
func:writeWithRetry = Result[void](string:path, string:content, int64:maxRetries) {
    int64:attempt = 0;
    
    while (attempt < maxRetries) {
        Result[void]:result = writeFile(path, content);
        
        if (result.isOk()) {
            pass(result);
        }
        
        attempt++;
        sleep(1000);  // Wait before retry
    }
    
    fail(`Failed to write after &{maxRetries} attempts`);
};
```

### Pattern 3: Backup Before Overwrite

```aria
func:safeWrite = Result[void](string:path, string:content) {
    // Check if file exists
    if (fileExists(path)) {
        // Create backup
        Result[string]:backup = readFile(path);
        
        if (backup.isOk()) {
            writeFile(`&{path}.backup`, backup.unwrap());
        }
    }
    
    // Write new content
    pass(writeFile(path, content));
};
```

---

## Common Use Cases

### Use Case 1: Log File

```aria
func:appendLog = Result[void](string:message) {
    string:timestamp = getCurrentTime().toString();
    string:entry = `[&{timestamp}] &{message}\n`;
    
    // Read existing log
    string:existing = readFile("app.log").unwrapOr("");
    
    // Append new entry
    pass(writeFile("app.log", existing + entry));
};

appendLog("Application started");
appendLog("Processing data");
```

### Use Case 2: Export Data

```aria
func:exportCSV = Result[void](string[][]:data) {
    string:csv = "";
    
    till(data.length - 1, 1) {
        string[]:row = data[$];
        csv += row.join(",") + "\n";
    }
    
    pass(writeFile("export.csv", csv));
};

string[][]:table = [
    ["Name", "Age", "City"],
    ["Alice", "30", "NYC"],
    ["Bob", "25", "SF"]
];

exportCSV(table);
```

### Use Case 3: Template Rendering

```aria
func:renderTemplate = Result[void](string:template, string:output) {
    Result[string]:tmpl = readFile(template);
    
    if (tmpl.isErr()) {
        fail("Template not found");
    }
    
    string:rendered = tmpl.unwrap()
        .replace("{{title}}", "My Page")
        .replace("{{content}}", "Hello, World!");
    
    pass(writeFile(output, rendered));
};

renderTemplate("template.html", "index.html");
```

### Use Case 4: Temporary Files

```aria
func:createTempFile = Result[string]() {
    string:path = `/tmp/aria_&{getCurrentTime()}.tmp`;
    
    Result[void]:result = writeFile(path, "temporary data");
    
    if (result.isErr()) {
        fail("Cannot create temp file");
    }
    
    pass(Result.ok(path));
};

Result[string]:tempPath = createTempFile();
defer deleteFile(tempPath.unwrap());
```

---

## File Creation and Permissions

### Automatic Creation

```aria
// Creates file if it doesn't exist
writeFile("new_file.txt", "content");

// Creates intermediate directories (tentative)
writeFile("path/to/new/file.txt", "content");
```

### Overwrite Behavior

```aria
// First write
writeFile("data.txt", "Version 1");

// Overwrites previous content
writeFile("data.txt", "Version 2");
// File now contains "Version 2" only
```

### File Permissions (Unix)

```aria
// Default permissions: 0644 (rw-r--r--)
writeFile("file.txt", "data");

// Custom permissions (tentative syntax)
writeFile("script.sh", "#!/bin/bash\necho Hello", permissions: 0o755);
```

---

## Common Errors

### Permission Denied

```aria
Result[void]:result = writeFile("/root/protected.txt", "data");

result.onError(err => {
    // err = "Permission denied: /root/protected.txt"
    print(`Error: &{err}`);
});
```

### Disk Full

```aria
Result[void]:result = writeFile("huge.txt", veryLargeString);

result.onError(err => {
    // err = "No space left on device"
    print(`Error: &{err}`);
});
```

### Invalid Path

```aria
Result[void]:result = writeFile("/invalid/path/file.txt", "data");

result.onError(err => {
    // err = "No such file or directory"
    print(`Error: &{err}`);
});
```

---

## Best Practices

### ✅ Always Check Result

```aria
// GOOD: Handle potential errors
Result[void]:result = writeFile("important.txt", data);

if (result.isErr()) {
    logError(result.err().unwrap());
    handleFailure();
}
```

### ✅ Use Atomic Writes for Critical Data

```aria
// GOOD: Write to temp, then rename (atomic)
func:atomicWrite = Result[void](string:path, string:content) {
    string:temp = `&{path}.tmp`;
    
    Result[void]:writeResult = writeFile(temp, content);
    if (writeResult.isErr()) {
        pass(writeResult);
    }
    
    // Atomic rename
    Result[void]:renameResult = renameFile(temp, path);
    pass(renameResult);
};
```

### ✅ Validate Content Before Writing

```aria
// GOOD: Validate before writing
func:saveJSON = Result[void](string:path, string:json) {
    if (!isValidJSON(json)) {
        fail("Invalid JSON format");
    }
    
    pass(writeFile(path, json));
};
```

### ❌ Don't Ignore Write Errors

```aria
// BAD: Ignoring result
writeFile("log.txt", "data");
// No error handling!

// GOOD: Handle errors
writeFile("log.txt", "data")
    .onError(err => logError(err));
```

### ❌ Don't Write Sensitive Data Without Protection

```aria
// BAD: Plain text passwords
writeFile("passwords.txt", userPassword);

// GOOD: Encrypt before writing
string:encrypted = encrypt(userPassword);
writeFile("passwords.enc", encrypted);
```

---

## Comparison with Other Languages

### Aria

```aria
Result[void]:result = writeFile("file.txt", "Hello");
result.onError(err => print(err));
```

### Python

```python
try:
    with open("file.txt", "w") as f:
        f.write("Hello")
except IOError as e:
    print(e)
```

### Node.js

```javascript
fs.writeFile("file.txt", "Hello", (err) => {
    if (err) console.log(err);
});
```

### Rust

```rust
match fs::write("file.txt", "Hello") {
    Ok(_) => println!("Success"),
    Err(e) => println!("Error: {}", e),
}
```

---

## Advanced Examples

### Example 1: Batch File Writing

```aria
func:writeAll = Result[void](string@:path[], string[]:contents) {
    if (paths.length != contents.length) {
        fail("Mismatched arrays");
    }
    
    till(paths.length - 1, 1) {
        Result[void]:result = writeFile(paths[$], contents[$]);
        
        if (result.isErr()) {
            fail(`Failed to write &{paths[$]}`);
        }
    }
    
    pass(Result.ok());
};
```

### Example 2: Transaction-like Writes

```aria
func:writeTransaction = Result[void](string[]:paths, string[]:data) {
    string[]:tempPaths = [];
    
    // Write to temp files first
    till(paths.length - 1, 1) {
        string:temp = `&{paths[$]}.tmp`;
        tempPaths.append(temp);
        
        Result[void]:result = writeFile(temp, data[$]);
        if (result.isErr()) {
            // Rollback
            cleanupTemps(tempPaths);
            fail("Transaction failed");
        }
    }
    
    // All writes succeeded, rename atomically
    till(paths.length - 1, 1) {
        renameFile(tempPaths[$], paths[$]);
    }
    
    pass(Result.ok());
};
```

### Example 3: Structured Logging

```aria
%STRUCT LogEntry {
    int64:timestamp,
    string:level,
    string:message
}

func:writeLog = Result[void](LogEntry:entry) {
    string:line = `[&{entry.timestamp}] &{entry.level}: &{entry.message}\n`;
    
    string:existing = readFile("app.log").unwrapOr("");
    pass(writeFile("app.log", existing + line));
};

LogEntry:log = { getCurrentTime(), "INFO", "Server started" };
writeLog(log);
```

### Example 4: Content Pipeline

```aria
readFile("input.txt")
    .map(content => content.toUpperCase())
    .map(upper => upper.replace(" ", "_"))
    .andThen(processed => writeFile("output.txt", processed))
    .onSuccess(_ => print("Pipeline complete"))
    .onError(err => print(`Pipeline failed: &{err}`));
```

---

## Related Topics

- [readFile()](readFile.md) - Read files with Result handling
- [openFile()](openFile.md) - Stream-based file I/O
- [Result Type](../types/result.md) - Error handling with Result[T]
- [String Type](../types/strings.md) - String operations
- [File Permissions](../io_system/permissions.md) - Unix file permissions
- [Atomic Operations](../advanced/atomic_writes.md) - Safe file updates

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Line 224  
**Unique Feature**: Built-in Result type for safe file operations
