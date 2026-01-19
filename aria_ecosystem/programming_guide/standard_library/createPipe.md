# createPipe()

**Category**: Standard Library → System I/O  
**Syntax**: `createPipe() -> Result<(File, File)>`  
**Purpose**: Create pipe for inter-process communication

---

## Overview

`createPipe()` creates a **unidirectional pipe** - data written to write end can be read from read end.

---

## Syntax

```aria
import std.io;

(read_end, write_end): (File, File) = createPipe()?;
```

---

## Returns

- `Result<(File, File)>` - Tuple of (read_end, write_end)

---

## Examples

### Basic Pipe

```aria
import std.io;

// Create pipe
(reader, writer): (File, File) = createPipe()?;

// Write to pipe
writer.write("Hello through pipe!")?;
writer.close();

// Read from pipe
message: string = reader.read_all()?;
stdout << message;  // "Hello through pipe!"

reader.close();
```

### Parent-Child Communication

```aria
(reader, writer): (File, File) = createPipe()?;

fork_Result: ForkResult = fork()?;

when fork_result is Parent(child_pid) then
    // Parent reads from pipe
    writer.close();  // Close write end
    
    message: string = reader.read_all()?;
    stdout << "Parent received: $message";
    
    reader.close();
    wait(child_pid)?;
    
elsif fork_result is Child then
    // Child writes to pipe
    reader.close();  // Close read end
    
    writer.write("Message from child")?;
    writer.close();
    
    exit(0);
end
```

### Pipeline Between Processes

```aria
// Create pipe for process pipeline
(reader, writer): (File, File) = createPipe()?;

fork_Result: ForkResult = fork()?;

when fork_result is Parent(child_pid) then
    writer.close();
    
    // Read output from child
    output: string = reader.read_all()?;
    stdout << "Result: $output";
    
    reader.close();
    wait(child_pid)?;
    
elsif fork_result is Child then
    reader.close();
    
    // Redirect stdout to pipe
    dup2(writer.fd(), stdout.fd())?;
    
    // Execute command - output goes to pipe
    exec("ls", ["-la"])?;
    exit(1);
end
```

---

## Best Practices

### ✅ DO: Close Unused Ends

```aria
(reader, writer): (File, File) = createPipe()?;

when fork_result is Parent then
    writer.close();  // ✅ Parent only reads
elsif fork_result is Child then
    reader.close();  // ✅ Child only writes
end
```

### ✅ DO: Use defer for Cleanup

```aria
(reader, writer): (File, File) = createPipe()?;
defer reader.close();
defer writer.close();

// Use pipe...
```

### ❌ DON'T: Forget to Close

```aria
(reader, writer): (File, File) = createPipe()?;

// Use pipe...

// ❌ Forgot to close - resource leak!
```

---

## Related

- [fork()](fork.md) - Create child process
- [openFile()](openFile.md) - File I/O
- [stream_io](stream_io.md) - Streaming I/O

---

**Remember**: Always **close both pipe ends** when done!
