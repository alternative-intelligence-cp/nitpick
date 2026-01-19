# The `readFile()` Function

**Category**: Standard Library → I/O  
**Syntax**: `Result[string]:readFile(string:path)`  
**Purpose**: Read entire file contents as string with error handling

---

## Overview

The `readFile()` function reads the entire contents of a file and returns it as a string. It:

- Returns **`Result[string]`** type (success or error)
- Reads **entire file** into memory
- Handles **text files** (for binary, use streams)
- Provides **automatic error handling**

**Philosophy**: Safe file I/O with explicit error handling via Result type.

---

## Basic Syntax

```aria
Result[string]:result = readFile("path/to/file.txt");

result?
    .onSuccess(content => {
        print(`File content: &{content}`);
    })
    .onError(err => {
        print(`Error: &{err}`);
    });
```

---

## Example 1: Basic File Reading

```aria
Result[string]:result = readFile("config.txt");

if (result.isOk()) {
    string:content = result.unwrap();
    print(content);
} else {
    string:error = result.err().unwrap();
    print(`Failed to read file: &{error}`);
}
```

---

## Example 2: Pipeline Style

```aria
// Chain operations with pipeline
readFile("data.txt")
    .map(content => content.toUpperCase())
    .map(upper => upper.replace("\n", " "))
    .onSuccess(processed => print(processed))
    .onError(err => print(`Error: &{err}`));
```

---

## Example 3: Early Return on Error

```aria
func:processConfigFile = Result[void](string:path) {
    // Read file, propagate error if fails
    Result[string]:result = readFile(path);
    if (result.isErr()) {
        pass(result.mapErr());  // Propagate error
    }
    
    string:content = result.unwrap();
    
    // Process content
    parseConfig(content);
    
    pass(Result.ok());
};
```

---

## Error Handling Patterns

### Pattern 1: Unwrap with Default

```aria
// Use default if file doesn't exist
string:config = readFile("config.txt")
    .unwrapOr("default config");

print(config);
```

### Pattern 2: Match on Result

```aria
Result[string]:result = readFile("data.txt");

pick(result.isOk()) {
    case true:
        string:data = result.unwrap();
        processData(data);
        
    case false:
        string:err = result.err().unwrap();
        handleError(err);
}
```

### Pattern 3: Early Exit

```aria
func:loadAndProcess = Result[void]() {
    // Early return on error
    Result[string]:data = readFile("input.txt");
    if (data.isErr()) {
        fail(`Cannot read input: &{data.err().unwrap()}`);
    }
    
    // Continue with valid data
    process(data.unwrap());
    pass(Result.ok());
};
```

---

## Common Use Cases

### Use Case 1: Configuration Files

```aria
func:loadConfig = Config() {
    Result[string]:content = readFile("app.config");
    
    if (content.isErr()) {
        print("Using default config");
        pass(Config.default());
    }
    
    pass(parseConfig(content.unwrap()));
};
```

### Use Case 2: Template Loading

```aria
func:loadTemplate = string(string:name) {
    string:path = `templates/&{name}.html`;
    
    pass(readFile(path).unwrapOr("<html>Not Found</html>"));
};

string:homepage = loadTemplate("home");
```

### Use Case 3: Data Import

```aria
func:importData = Result[int64[]]() {
    Result[string]:raw = readFile("data.csv");
    if (raw.isErr()) {
        fail("Cannot read data file");
    }
    
    // Parse CSV
    int64[]:values = parseCSV(raw.unwrap());
    pass(Result.ok(values));
};
```

### Use Case 4: File Existence Check

```aria
func:fileExists = bool(string:path) {
    Result[string]:result = readFile(path);
    pass(result.isOk());
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
string:content = readFile("small.txt").unwrap();
```

### Large Files

```aria
// For large files, use streaming
func:processLargeFile = Result[void](string:path) {
    Result[Stream]:stream = openFile(path, "r");
    if (stream.isErr()) {
        fail("Cannot open file");
    }
    
    // Read line by line
    while (!stream.eof()) {
        string:line = stream.readLine().unwrap();
        processLine(line);
    }
    
    stream.close();
    pass(Result.ok());
};
```

**Rule**: Use `readFile()` for small files, use `openFile()` + streaming for large files.

---

## Common Errors

### File Not Found

```aria
Result[string]:result = readFile("nonexistent.txt");

result.onError(err => {
    // err = "File not found: nonexistent.txt"
    print(`Error: &{err}`);
});
```

### Permission Denied

```aria
Result[string]:result = readFile("/root/secret.txt");

result.onError(err => {
    // err = "Permission denied: /root/secret.txt"
    print(`Error: &{err}`);
});
```

### File Too Large

```aria
// May run out of memory for very large files
Result[string]:result = readFile("huge_file.bin");

result.onError(err => {
    // err = "Out of memory"
    print(`Error: &{err}`);
});
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
Result[string]:text = readFile("utf8.txt");
```

### Other Encodings (Future)

**Status**: Encoding support TBD

Possible future syntax:

```aria
// Read with specific encoding (tentative)
Result[string]:text = readFile("latin1.txt", encoding: "ISO-8859-1");
```

---

## Best Practices

### ✅ Always Handle Errors

```aria
// GOOD: Check for errors
Result[string]:result = readFile("data.txt");
if (result.isErr()) {
    handleError(result.err().unwrap());
} else {
    process(result.unwrap());
}
```

### ✅ Use Defaults for Optional Files

```aria
// GOOD: Graceful degradation
string:theme = readFile("theme.css")
    .unwrapOr("/* default theme */");
```

### ✅ Validate Content After Reading

```aria
// GOOD: Validate after reading
Result[string]:content = readFile("config.json");
if (content.isOk()) {
    if (isValidJSON(content.unwrap())) {
        parseJSON(content.unwrap());
    } else {
        print("Invalid JSON in config file");
    }
}
```

### ❌ Don't Ignore Errors

```aria
// BAD: Ignoring potential errors
string:data = readFile("file.txt").unwrap();
// May panic if file doesn't exist!

// GOOD: Handle errors explicitly
Result[string]:result = readFile("file.txt");
result.onError(err => print(`Error: &{err}`));
```

### ❌ Don't Read Large Files Entirely

```aria
// BAD: Reading 1GB file into memory
string:huge = readFile("gigabyte.log").unwrap();
// OOM (Out of Memory) error!

// GOOD: Stream large files
Stream:stream = openFile("gigabyte.log", "r").unwrap();
while (!stream.eof()) {
    processLine(stream.readLine().unwrap());
}
```

---

## Comparison with Other Languages

### Aria

```aria
Result[string]:content = readFile("file.txt");
content.onError(err => print(err));
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
func:readAll = Result[string[]](string[]:paths) {
    string[]:contents = [];
    
    till(paths.length - 1, 1) {
        Result[string]:result = readFile(paths[$]);
        
        if (result.isErr()) {
            fail(`Failed to read &{paths[$]}`);
        }
        
        contents.append(result.unwrap());
    }
    
    pass(Result.ok(contents));
};

string[]:files = ["a.txt", "b.txt", "c.txt"];
Result[string[]]:all = readAll(files);
```

### Example 2: Conditional Processing

```aria
func:processIfExists = void(string:path) {
    readFile(path)
        .onSuccess(content => {
            // File exists, process it
            string[]:lines = content.split("\n");
            till(lines.length - 1, 1) {
                processLine(lines[$]);
            }
        })
        .onError(err => {
            // File doesn't exist, skip silently
            print(`Skipping &{path}`);
        });
};
```

### Example 3: Retry on Failure

```aria
func:readWithRetry = Result[string](string:path, int64:maxRetries) {
    int64:attempt = 0;
    
    while (attempt < maxRetries) {
        Result[string]:result = readFile(path);
        
        if (result.isOk()) {
            pass(result);
        }
        
        attempt++;
        sleep(1000);  // Wait 1 second
    }
    
    fail(`Failed to read after &{maxRetries} attempts`);
};

Result[string]:content = readWithRetry("network-file.txt", 3);
```

### Example 4: Cached Reading

```aria
%STRUCT FileCache {
    string@:path,
    string:content,
    int64:timestamp
}

gc FileCache[]:cache = [];

func:readCached = Result[string](string:path) {
    // Check cache first
    till(cache.length - 1, 1) {
        if (cache[$].path == path) {
            int64:age = getCurrentTime() - cache[$].timestamp;
            
            if (age < 60000) {  // Cache valid for 60 seconds
                pass(Result.ok(cache[$].content));
            }
        }
    }
    
    // Not in cache, read from disk
    Result[string]:result = readFile(path);
    
    if (result.isOk()) {
        // Add to cache
        FileCache:entry = {
            path: path,
            content: result.unwrap(),
            timestamp: getCurrentTime()
        };
        cache.append(entry);
    }
    
    pass(result);
};
```

---

## Related Topics

- [writeFile()](writeFile.md) - Write files with Result handling
- [openFile()](openFile.md) - Stream-based file I/O
- [Result Type](../types/result.md) - Error handling with Result[T]
- [String Type](../types/strings.md) - String operations
- [readJSON()](readJSON.md) - Parse JSON files
- [readCSV()](readCSV.md) - Parse CSV files

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Line 223  
**Unique Feature**: Built-in Result type for safe file operations
