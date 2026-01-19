# openFile()

**Category**: Standard Library → I/O  
**Syntax**: `openFile(path: string, mode: string) -> Result<File>`  
**Purpose**: Open file handle for streaming I/O

---

## Overview

`openFile()` opens a file and returns a **file handle** for reading/writing in chunks.

---

## Syntax

```aria
import std.io;

file: Result<File> = openFile("data.txt", "r");
```

---

## Parameters

- **path** (`string`) - File path
- **mode** (`string`) - Open mode: `"r"`, `"w"`, `"a"`, `"r+"`, `"w+"`, `"a+"`

---

## Modes

| Mode | Description | Creates | Overwrites |
|------|-------------|---------|------------|
| `"r"` | Read only | No | - |
| `"w"` | Write only | Yes | Yes |
| `"a"` | Append only | Yes | No |
| `"r+"` | Read + Write | No | No |
| `"w+"` | Read + Write | Yes | Yes |
| `"a+"` | Read + Append | Yes | No |

---

## Returns

- `Result<File>` - File handle on success, error on failure

---

## Examples

### Read File in Chunks

```aria
import std.io;

file: File = openFile("large.txt", "r")?;

loop
    chunk: ?string = file.read_line();
    when chunk == nil then break; end
    
    stdout << chunk;
end

file.close();
```

### Write to File

```aria
file: File = openFile("output.txt", "w")?;

file.write("Line 1\n")?;
file.write("Line 2\n")?;
file.write("Line 3\n")?;

file.close();
```

### Append to File

```aria
// Append mode - doesn't overwrite
file: File = openFile("log.txt", "a")?;

file.write("[$timestamp] Event occurred\n")?;

file.close();
```

### Read and Write

```aria
file: File = openFile("data.txt", "r+")?;

// Read
content: string = file.read_all()?;

// Modify
modified: string = content.to_upper();

// Write back
file.seek(0)?;  // Go to start
file.write(modified)?;

file.close();
```

---

## File Methods

### Reading

```aria
file: File = openFile("data.txt", "r")?;

// Read line
line: ?string = file.read_line();

// Read all remaining
all: string = file.read_all()?;

// Read N bytes
chunk: string = file.read(1024)?;
```

### Writing

```aria
file: File = openFile("output.txt", "w")?;

// Write string
file.write("Hello\n")?;

// Write bytes
file.write_bytes([72, 101, 108, 108, 111])?;
```

### Position

```aria
// Get position
pos: i64 = file.tell()?;

// Seek to position
file.seek(0)?;  // Start
file.seek_end()?;  // End
```

---

## Best Practices

### ✅ DO: Always Close Files

```aria
file: File = openFile("data.txt", "r")?;
// ... use file ...
file.close();
```

### ✅ DO: Use defer for Cleanup

```aria
file: File = openFile("data.txt", "r")?;
defer file.close();  // Auto-close when scope ends

// Use file...
```

### ✅ DO: Use Append Mode for Logs

```aria
log: File = openFile("app.log", "a")?;
defer log.close();

log.write("[$time] Application started\n")?;
```

### ❌ DON'T: Forget to Close

```aria
file: File = openFile("data.txt", "r")?;
// Use file
// ❌ Never closed - resource leak!
```

---

## Related

- [readFile()](readFile.md) - Read entire file at once
- [writeFile()](writeFile.md) - Write entire file at once
- [stream_io](stream_io.md) - Streaming I/O patterns

---

**Remember**: Always **close** file handles when done!
