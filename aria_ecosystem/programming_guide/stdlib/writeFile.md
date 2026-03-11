# The `writeFile()` Function

**Category**: Standard Library → I/O  
**Syntax**: `result<NIL>:writeFile(string:path, string:content)`  
**Purpose**: Write string data to file with error handling

---

## Overview

The `writeFile()` function writes content to a file, creating it if it doesn't exist or overwriting it if it does. It:

- Returns **`result<NIL>`** type (success or error)
- **Creates or overwrites** the target file
- Writes **text data** as UTF-8 by default
- Provides **automatic error handling**

**Philosophy**: Safe file writing with explicit error handling via Result type.

---

## Basic Syntax

```aria
result<NIL>:result = writeFile("output.txt", "Hello, World!");

if (!result.is_error) {
    print("File written successfully");
} else {
    print(`Error: &{result.err}`);
}
```

---

## Example 1: Basic File Writing

```aria
string:data = "This is my file content\nWith multiple lines";

result<NIL>:result = writeFile("output.txt", data);

if (!result.is_error) {
    print("File saved!");
} else {
    print(`Failed to write: &{result.err}`);
}
```

---

## Example 2: Save Configuration

```aria
func:saveConfig = result<NIL>(Config:cfg) {
    string:json = cfg.toJSON();
    pass(writeFile("app.config", json));
};

Config:config = { theme: "dark", fontSize: 12 };
result<NIL>:r = saveConfig(config);
if (r.is_error) { print(`Failed: &{r.err}`); } else { print("Config saved"); }
```

---

## Example 3: Generate Report

```aria
func:generateReport = result<NIL>(int64[]:data) {
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
func:saveData = result<NIL>(string:data) {
    result<NIL>:result = writeFile("data.txt", data);

    if (result.is_error) {
        fail(`Cannot save data: &{result.err}`);
    }

    print("Data saved successfully");
    pass(NIL);
};
```

### Pattern 2: Retry on Failure

```aria
func:writeWithRetry = result<NIL>(string:path, string:content, int64:maxRetries) {
    int64:attempt = 0;

    while (attempt < maxRetries) {
        result<NIL>:result = writeFile(path, content);

        if (!result.is_error) {
            pass(NIL);
        }

        attempt++;
        sleep(1000);  // Wait before retry
    }

};
```

### Pattern 3: Backup Before Overwrite

```aria
func:safeWrite = result<NIL>(string:path, string:content) {
    // Check if file exists
    if (fileExists(path)) {
        // Create backup
        result<string>:backup = readFile(path);

        if (!backup.is_error) {
            writeFile(`&{path}.backup`, raw(backup));
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
func:appendLog = result<NIL>(string:message) {
    string:timestamp = getCurrentTime().toString();
    string:entry = `[&{timestamp}] &{message}\n`;
    
    // Read existing log
    string:existing = readFile("app.log") ? "";
    
    // Append new entry
    pass(writeFile("app.log", existing + entry));
};

appendLog("Application started");
appendLog("Processing data");
```

### Use Case 2: Export Data

```aria
func:exportCSV = result<NIL>(string[][]:data) {
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
func:renderTemplate = result<void>(string:template, string:output) {
    result<string>:tmpl = readFile(template);
    
    if (tmpl.is_error) {
        fail("Template not found");
    }
    
    string:rendered = raw(tmpl)
        .replace("{{title}}", "My Page")
        .replace("{{content}}", "Hello, World!");
    
    pass(writeFile(output, rendered));
};

renderTemplate("template.html", "index.html");
```

### Use Case 4: Temporary Files

```aria
func:createTempFile = result<string>() {
    string:path = `/tmp/aria_&{getCurrentTime()}.tmp`;

    result<NIL>:result = writeFile(path, "temporary data");

    if (result.is_error) {
        fail("Cannot create temp file");
    }

    pass(path);
};

result<string>:tempPath = createTempFile();
defer deleteFile(tempPath ? "/dev/null");
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
result<NIL>:result = writeFile("/root/protected.txt", "data");

if (result.is_error) {
    // err = "Permission denied: /root/protected.txt"
    print(`Error: &{result.err}`);
}
```

### Disk Full

```aria
result<NIL>:result = writeFile("huge.txt", veryLargeString);

if (result.is_error) {
    // err = "No space left on device"
    print(`Error: &{result.err}`);
}
```

### Invalid Path

```aria
result<NIL>:result = writeFile("/invalid/path/file.txt", "data");

if (result.is_error) {
    // err = "No such file or directory"
    print(`Error: &{result.err}`);
}
```

---

## Best Practices

### ✅ Always Check Result

```aria
// GOOD: Handle potential errors
result<NIL>:result = writeFile("important.txt", data);

if (result.is_error) {
    logError(result.err);
    handleFailure();
}
```

### ✅ Use Atomic Writes for Critical Data

```aria
// GOOD: Write to temp, then rename (atomic)
func:atomicWrite = result<NIL>(string:path, string:content) {
    string:temp = `&{path}.tmp`;

    result<NIL>:writeResult = writeFile(temp, content);
    if (writeResult.is_error) {
        pass(writeResult);
    }

    // Atomic rename
    pass(renameFile(temp, path));
};
```

### ✅ Validate Content Before Writing

```aria
// GOOD: Validate before writing
func:saveJSON = result<NIL>(string:path, string:json) {
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
// No error handling! Write failure silently discarded.

// GOOD: Check errors
result<NIL>:r = writeFile("log.txt", "data");
if (r.is_error) { logError(r.err); }
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
result<NIL>:result = writeFile("file.txt", "Hello");
if (result.is_error) { print(result.err); }
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
func:writeAll = result<NIL>(string@:path[], string[]:contents) {
    if (paths.length != contents.length) {
        fail("Mismatched arrays");
    }

    till(paths.length - 1, 1) {
        result<NIL>:result = writeFile(paths[$], contents[$]);

        if (result.is_error) {
            fail(`Failed to write &{paths[$]}`);
        }
    }

    pass(NIL);
};
```

### Example 2: Transaction-like Writes

```aria
func:writeTransaction = result<NIL>(string[]:paths, string[]:data) {
    string[]:tempPaths = [];

    // Write to temp files first
    till(paths.length - 1, 1) {
        string:temp = `&{paths[$]}.tmp`;
        tempPaths.append(temp);

        result<NIL>:result = writeFile(temp, data[$]);
        if (result.is_error) {
            // Rollback
            cleanupTemps(tempPaths);
            fail("Transaction failed");
        }
    }

    // All writes succeeded, rename atomically
    till(paths.length - 1, 1) {
        renameFile(tempPaths[$], paths[$]);
    }

    pass(NIL);
};
```

### Example 3: Structured Logging

```aria
%STRUCT LogEntry {
    int64:timestamp,
    string:level,
    string:message
}

func:writeLog = result<NIL>(LogEntry:entry) {
    string:line = `[&{entry.timestamp}] &{entry.level}: &{entry.message}\n`;

    string:existing = readFile("app.log") ? "";
    pass(writeFile("app.log", existing + line));
};

LogEntry:log = { getCurrentTime(), "INFO", "Server started" };
writeLog(log);
```

### Example 4: Content Pipeline

```aria
// Process input.txt, transform it, write output
result<string>:raw = readFile("input.txt");
if (raw.is_error) { fail(`Pipeline failed: &{raw.err}`); }

string:upper = raw(raw).toUpperCase();
string:processed = upper.replace(" ", "_");

result<NIL>:out = writeFile("output.txt", processed);
if (!out.is_error) { print("Pipeline complete"); } else { print(`Pipeline failed: &{out.err}`); }
```

---

## Related Topics

- [readFile()](readFile.md) - Read files with Result handling
- [openFile()](openFile.md) - Stream-based file I/O
- [Result Type](../types/result.md) - Error handling with `result<T>`
- [String Type](../types/strings.md) - String operations
- [File Permissions](../io_system/permissions.md) - Unix file permissions
- [Atomic Operations](../advanced/atomic_writes.md) - Safe file updates

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Line 224  
**Unique Feature**: Built-in Result type for safe file operations
