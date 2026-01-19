# Stream I/O

**Category**: Standard Library → I/O  
**Purpose**: Streaming input/output patterns

---

## Overview

**Stream I/O** processes data in chunks rather than loading entire files into memory.

---

## Why Streaming?

### Memory Efficiency

```aria
// Bad - loads entire 10GB file
content: string = readFile("huge_log.txt")?;  // ❌ 10GB in RAM!

// Good - processes in chunks
file: File = openFile("huge_log.txt", "r")?;
defer file.close();

loop
    line: ?string = file.read_line();
    when line == nil then break; end
    process_line(line);
end  // ✅ Only one line in memory at a time
```

---

## Reading Streams

### Line by Line

```aria
file: File = openFile("data.txt", "r")?;
defer file.close();

loop
    line: ?string = file.read_line();
    when line == nil then break; end
    
    stdout << "Line: $line";
end
```

### Fixed-Size Chunks

```aria
file: File = openFile("binary.dat", "r")?;
defer file.close();

chunk_size: i32 = 4096;  // 4KB chunks

loop
    chunk: ?[]uint8 = file.read_bytes(chunk_size);
    when chunk == nil then break; end
    
    process_chunk(chunk);
end
```

---

## Writing Streams

### Write in Chunks

```aria
output: File = openFile("output.txt", "w")?;
defer output.close();

for i in 0..1000000 do
    output.write("Line $i\n")?;
    
    // Flush every 1000 lines
    when i % 1000 == 0 then
        output.flush()?;
    end
end
```

---

## Piping Streams

### File to File

```aria
input: File = openFile("source.txt", "r")?;
output: File = openFile("dest.txt", "w")?;

defer input.close();
defer output.close();

loop
    line: ?string = input.read_line();
    when line == nil then break; end
    
    // Transform line
    transformed: string = line.to_upper();
    
    output.write(transformed)?;
    output.write("\n")?;
end
```

### Process Pipeline

```aria
// Read from process output
proc: Process = spawn("generate_data")?;
output: File = proc.stdout();

loop
    line: ?string = output.read_line();
    when line == nil then break; end
    
    // Process streaming data
    data: Data = parse_line(line);
    process_data(data);
end

proc.wait()?;
```

---

## Buffering

### Buffer Writes

```aria
output: File = openFile("output.txt", "w")?;
defer output.close();

buffer: []string = [];

for item in large_dataset do
    buffer.append(format_item(item));
    
    // Flush when buffer reaches 1000 items
    when buffer.length() >= 1000 then
        output.write(buffer.join("\n"))?;
        output.write("\n")?;
        buffer.clear();
    end
end

// Flush remaining
when buffer.length() > 0 then
    output.write(buffer.join("\n"))?;
end
```

---

## Best Practices

### ✅ DO: Use Streaming for Large Files

```aria
// Perfect for huge files
fn process_log_file(path: string) -> Result<void> {
    file: File = openFile(path, "r")?;
    defer file.close();
    
    loop
        line: ?string = file.read_line();
        when line == nil then break; end
        analyze_log_line(line);
    end
    
    return Ok();
}
```

### ✅ DO: Process in Chunks

```aria
chunk_size: i32 = 8192;  // 8KB

file: File = openFile("data.bin", "r")?;
defer file.close();

loop
    chunk: ?[]uint8 = file.read_bytes(chunk_size);
    when chunk == nil then break; end
    
    process_binary_chunk(chunk);
end
```

### ❌ DON'T: Load Large Files Entirely

```aria
// Bad
content: string = readFile("10GB_file.txt")?;  // ❌ OOM!

// Good
file: File = openFile("10GB_file.txt", "r")?;
// Process in chunks  // ✅
```

---

## Related

- [openFile()](openFile.md) - File handles
- [readFile()](readFile.md) - Read entire file
- [createPipe()](createPipe.md) - Inter-process pipes

---

**Remember**: Use **streaming** for large files!
