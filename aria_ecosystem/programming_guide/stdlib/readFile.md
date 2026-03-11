# The `readFile()` Function

**Category**: Standard Library → I/O  
**Syntax**: `result<string>:readFile(string:path)`  
**Purpose**: Read entire file contents as string with error handling

---

## Overview

The `readFile()` function reads the entire contents of a file and returns it as a string. It:

- Returns **`result<string>`** type (success or error)
- Reads **entire file** into memory
- Handles **text files** (for binary, use streams)
- Provides **automatic error handling**

**Philosophy**: Safe file I/O with explicit error handling via Result type.

---

## Basic Syntax

```aria
result<string>:result = readFile("path/to/file.txt");

if (!result.is_error) {
    print(`File content: &{raw(result)}`);
} else {
    print(`Error: &{result.err}`);
}
```

---

## Example 1: Basic File Reading

```aria
result<string>:result = readFile("config.txt");

if (!result.is_error) {
    string:content = raw(result);
    print(content);
} else {
    print(`Failed to read file: &{result.err}`);
}
```

---

## Example 2: Pipeline Style

```aria
// Process file content step by step
result<string>:raw_data = readFile("data.txt");

if (raw_data.is_error) {
    print(`Error: &{raw_data.err}`);
} else {
    string:upper = raw(raw_data).toUpperCase();
    string:processed = upper.replace("\n", " ");
    print(processed);
}
```

---

## Example 3: Early Return on Error

```aria
func:processConfigFile = result<NIL>(string:path) {
    // Read file, propagate error if fails
    string:content = readFile(path)?;

    // Process content
    parseConfig(content)?;

    pass(NIL);
};
```

---

## Error Handling Patterns

### Pattern 1: Unwrap with Default

```aria
// Use default if file doesn't exist
string:config = readFile("config.txt") ? "default config";

print(config);
```

### Pattern 2: Match on Result

```aria
result<string>:result = readFile("data.txt");

if (!result.is_error) {
    string:data = raw(result);
    processData(data);
} else {
    handleError(result.err);
}
```

### Pattern 3: Early Exit

```aria
func:loadAndProcess = result<NIL>() {
    // Read file, propagate error if fails
    result<string>:data = readFile("input.txt");
    if (data.is_error) {
        fail(`Cannot read input: &{data.err}`);
    }

    // Continue with valid data
    process(raw(data));
    pass(NIL);
};
```

---

## Common Use Cases

### Use Case 1: Configuration Files

```aria
func:loadConfig = result<Config>() {
    result<string>:content = readFile("app.config");

    if (content.is_error) {
        print("Using default config");
        pass(Config.default());
    }

    pass(parseConfig(raw(content))?);
};
```

### Use Case 2: Template Loading

```aria
func:loadTemplate = string(string:name) {
    string:path = `templates/&{name}.html`;

    pass(readFile(path) ? "<html>Not Found</html>");
};

string:homepage = loadTemplate("home");
```

### Use Case 3: Data Import

```aria
func:importData = result<int64[]>() {
    result<string>:raw = readFile("data.csv");
    if (raw.is_error) {
        fail("Cannot read data file");
    }

    // Parse CSV
    int64[]:values = parseCSV(raw(raw));
    pass(values);
};
```

### Use Case 4: File Existence Check

```aria
func:fileExists = bool(string:path) {
    result<string>:result = readFile(path);
    pass(!result.is_error);
};

if (fileExists("important.txt")) {
    print("File found!");
}
```

---

## File Size Considerations

### Small Files

```aria
// OK for small files (< 1 MB)
string:content = readFile("small.txt")?!;
```

### Large Files

```aria
// For large files, use streaming
func:processLargeFile = result<NIL>(string:path) {
    result<Stream>:stream = openFile(path, "r");
    if (stream.is_error) {
        fail("Cannot open file");
    }

    // Read line by line
    while (!raw(stream).eof()) {
        string:line = raw(stream).readLine()?!;
        processLine(line);
    }

    raw(stream).close();
    pass(NIL);
};
```

**Rule**: Use `readFile()` for small files, use `openFile()` + streaming for large files.

---

## Common Errors

### File Not Found

```aria
result<string>:result = readFile("nonexistent.txt");

if (result.is_error) {
    // err = "File not found: nonexistent.txt"
    print(`Error: &{result.err}`);
}
```

### Permission Denied

```aria
result<string>:result = readFile("/root/secret.txt");

if (result.is_error) {
    // err = "Permission denied: /root/secret.txt"
    print(`Error: &{result.err}`);
}
```

### File Too Large

```aria
result<string>:result = readFile("huge_file.bin");

if (result.is_error) {
    // err = "Out of memory"
    print(`Error: &{result.err}`);
}
```

---

## Path Specifications

### Relative Paths

```aria
// Relative to current working directory
readFile("data.txt");
readFile("./data.txt");
readFile("../shared/config.txt");
```

### Absolute Paths

```aria
// Absolute paths
readFile("/etc/hosts");
readFile("/home/user/documents/file.txt");
```

### Path Normalization

```aria
// Paths are normalized automatically
readFile("./data/../config.txt");
// Equivalent to: readFile("config.txt")
```

---

## Text Encoding

### Default: UTF-8

```aria
// Assumes UTF-8 encoding
result<string>:text = readFile("utf8.txt");
```

### Other Encodings (Future)

**Status**: Encoding support TBD

Possible future syntax:

```aria
// tentative
result<string>:text = readFile("latin1.txt", encoding: "ISO-8859-1");
```

---

## Best Practices

### ✅ Always Handle Errors

```aria
// GOOD: Check for errors
result<string>:result = readFile("data.txt");
if (result.is_error) {
    handleError(result.err);
} else {
    process(raw(result));
}
```

### ✅ Use Defaults for Optional Files

```aria
// GOOD: Graceful degradation
string:theme = readFile("theme.css") ? "/* default theme */";
```

### ✅ Validate Content After Reading

```aria
// GOOD: Validate after reading
result<string>:content = readFile("config.json");
if (!content.is_error) {
    if (isValidJSON(raw(content))) {
        parseJSON(raw(content));
    } else {
        print("Invalid JSON in config file");
    }
}
```

### ❌ Don't Ignore Errors

```aria
// BAD: Ignoring potential errors
string:data = readFile("file.txt")?!;
// Uses ?! which triggers failsafe - better than silently failing

// GOOD: Handle errors explicitly
result<string>:result = readFile("file.txt");
if (result.is_error) { fail(`Error: &{result.err}`); }
```

### ❌ Don't Read Large Files Entirely

```aria
// BAD: Reading 1GB file into memory
string:huge = readFile("gigabyte.log")?!;
// OOM (Out of Memory) error!

// GOOD: Stream large files
Stream:stream = openFile("gigabyte.log", "r")?!;
while (!stream.eof()) {
    processLine(stream.readLine()?!);
}
```

---

## Comparison with Other Languages

### Aria

```aria
result<string>:content = readFile("file.txt");
if (content.is_error) { print(content.err); }
```

### Python

```python
try:
    with open("file.txt", "r") as f:
        content = f.read()
except IOError as e:
    print(e)
```

### Node.js

```javascript
fs.readFile("file.txt", "utf8", (err, data) => {
    if (err) {
        console.log(err);
    } else {
        console.log(data);
    }
});
```

### Rust

```rust
match fs::read_to_string("file.txt") {
    Ok(content) => println!("{}", content),
    Err(e) => println!("Error: {}", e),
}
```

---

## Advanced Examples

### Example 1: Read Multiple Files

```aria
func:readAll = result<string[]>(string[]:paths) {
    string[]:contents = [];

    till(paths.length - 1, 1) {
        result<string>:result = readFile(paths[$]);

        if (result.is_error) {
            fail(`Failed to read &{paths[$]}`);
        }

        contents.append(raw(result));
    }

    pass(contents);
};

string[]:files = ["a.txt", "b.txt", "c.txt"];
result<string[]>:all = readAll(files);
```

### Example 2: Conditional Processing

```aria
func:processIfExists = void(string:path) {
    result<string>:data = readFile(path);
    if (data.is_error) {
        // File doesn't exist, skip silently
        print(`Skipping &{path}`);
    } else {
        // File exists, process it
        string[]:lines = raw(data).split("\n");
        till(lines.length - 1, 1) {
            processLine(lines[$]);
        }
    }
};
```

### Example 3: Retry on Failure

```aria
func:readWithRetry = result<string>(string:path, int64:maxRetries) {
    int64:attempt = 0;

    while (attempt < maxRetries) {
        result<string>:result = readFile(path);

        if (!result.is_error) {
            pass(raw(result));
        }

        attempt++;
        sleep(1000);  // Wait 1 second
    }

    fail(`Failed to read after &{maxRetries} attempts`);
};

result<string>:content = readWithRetry("network-file.txt", 3);
```

### Example 4: Cached Reading

```aria
%STRUCT FileCache {
    string@:path,
    string:content,
    int64:timestamp
}

gc FileCache[]:cache = [];

func:readCached = result<string>(string:path) {
    // Check cache first
    till(cache.length - 1, 1) {
        if (cache[$].path == path) {
            int64:age = getCurrentTime() - cache[$].timestamp;

            if (age < 60000) {  // Cache valid for 60 seconds
                pass(cache[$].content);
            }
        }
    }

    // Not in cache, read from disk
    result<string>:result = readFile(path);

    if (!result.is_error) {
        // Add to cache
        FileCache:entry = {
            path: path,
            content: raw(result),
            timestamp: getCurrentTime()
        };
        cache.append(entry);
    }

    pass(raw(result));
};
```

---

## Related Topics

- [writeFile()](writeFile.md) - Write files with Result handling
- [openFile()](openFile.md) - Stream-based file I/O
- [Result Type](../types/result.md) - Error handling with `result<T>`
- [String Type](../types/strings.md) - String operations
- [readJSON()](readJSON.md) - Parse JSON files
- [readCSV()](readCSV.md) - Parse CSV files

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Line 223  
**Unique Feature**: Built-in Result type for safe file operations
