# Text I/O

**Category**: I/O System → Formats  
**Streams**: All (primarily stdin, stdout, stderr, stddbg)  
**Philosophy**: Human-readable text is the control plane's native format

---

## Overview

**Text I/O** refers to reading and writing human-readable text data, typically using the **control plane** streams (stdin, stdout, stderr, stddbg).

---

## Text vs Binary vs Structured

| Type | Format | Streams | Purpose |
|------|--------|---------|---------|
| **Text** | UTF-8 strings, lines | stdin, stdout, stderr, stddbg | Human interaction |
| **Binary** | Raw bytes | stddati, stddato | Efficient data transfer |
| **Structured** | JSON, XML, etc. | stddati, stddato | Machine-readable data |

---

## Common Text I/O Operations

### Reading Text

```aria
// Read a line (stdin)
line: string = stdin.read_line();

// Read all text
content: string = stdin.read_all();

// Read line by line
for line in stdin.lines() {
    process_line(line);
}
```

### Writing Text

```aria
// Write to stdout
stdout << "Hello, world!\n";

// Formatted text
stdout << format("Value: {:.2f}", value);

// Multiple values
stdout << "Count: " << count << ", Total: " << total << "\n";
```

### Error Messages (Text)

```aria
// Error messages are human-readable text
stderr << "ERROR: File not found: " << filename << "\n";
```

### Debug Traces (Text)

```aria
// Debug output is also text
stddbg << "Processing item " << item.id << ", status=" << item.status;
```

---

## Encoding

Aria uses **UTF-8** encoding for all text by default:
- **stdin/stdout/stderr**: UTF-8 text
- **String type**: UTF-8 internally
- **Files**: UTF-8 unless specified otherwise

---

## When to Use Text I/O

✅ **Use text I/O for**:
- Interactive user prompts
- Progress messages
- Log messages
- Error reporting
- Debug traces

❌ **Don't use text I/O for**:
- Large binary data (use binary I/O on data plane)
- Structured data exchange (use JSON/MessagePack on data plane)
- High-performance data transfer (use binary on data plane)

---

## Example: Text-Based CLI

```aria
fn main() {
    stdout << "Welcome to MyApp\n";
    stdout << "================\n\n";
    
    loop {
        stdout << "Command (help, run, quit): ";
        command: string = stdin.read_line().trim();
        
        match command {
            "help" => show_help(),
            "run"  => run_task(),
            "quit" => break,
            _      => stderr << "Unknown command: " << command << "\n",
        }
    }
    
    stdout << "Goodbye!\n";
}
```

---

## See Also

- [Binary I/O](binary_io.md) - Binary data on data plane
- [stdin](stdin.md), [stdout](stdout.md), [stderr](stderr.md), [stddbg](stddbg.md)
- [Control Plane](control_plane.md) - Text I/O is primarily control plane
- [Data Plane](data_plane.md) - Structured/binary data
