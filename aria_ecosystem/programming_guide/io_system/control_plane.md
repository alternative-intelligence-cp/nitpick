# Control Plane

**Category**: I/O System → Architecture  
**Streams**: stdin (0), stdout (1), stderr (2), stddbg (6)  
**Purpose**: Human interaction, control messages, and debugging

---

## What is the Control Plane?

The **control plane** consists of the four streams dedicated to **human interaction** and **program control**:

- **stdin** - Interactive user input
- **stdout** - Normal human-readable output
- **stderr** - Error messages for humans
- **stddbg** - Debug/diagnostic information for developers

These streams are for **people**, not machines.

---

## Control Plane vs Data Plane

| Aspect | Control Plane | Data Plane |
|--------|---------------|------------|
| **Audience** | Humans (users, operators, developers) | Machines (APIs, pipelines, databases) |
| **Streams** | stdin, stdout, stderr, stddbg | stddati, stddato |
| **Format** | Human-readable text (mostly) | Structured data (JSON, binary) |
| **Purpose** | Interaction, monitoring, debugging | Data processing, ETL, APIs |
| **Buffering** | Line-buffered | Block-buffered |

---

## Why Separate Control from Data?

Inspired by **network router architecture**, where:
- **Control traffic**: Routing protocols, health checks, management
- **Data traffic**: Actual packets being forwarded

Similarly, in programs:
- **Control plane**: How the program runs, what the user sees, debugging
- **Data plane**: What the program processes (input → output transformation)

**Key insight**: These are fundamentally different concerns and should not share streams.

---

## Control Plane Streams

### stdin (FD 0)
**Interactive input from humans**

```aria
stdout << "Enter your name: ";
name: string = stdin.read_line();
```

### stdout (FD 1)
**Normal output for humans**

```aria
stdout << "Processing complete: " << count << " items\n";
```

### stderr (FD 2)
**Error messages for humans**

```aria
stderr << "ERROR: File not found\n";
```

### stddbg (FD 6)
**Debug/diagnostic output for developers**

```aria
stddbg << "Entering function process(), state=" << state;
```

---

## When to Use Control Plane

Use control plane streams when:
- ✅ Prompting the user for input
- ✅ Showing progress messages
- ✅ Reporting errors or warnings
- ✅ Debugging program behavior
- ✅ Displaying results in human-readable format

**Do NOT use control plane for**:
- ❌ JSON or binary data (use data plane)
- ❌ Data meant for other programs (use stddato)
- ❌ Structured logs for log aggregators (use stddato)

---

## Example: Control Plane in Action

```aria
fn main() {
    stddbg << "Program starting";
    
    // Interactive prompt (control plane)
    stdout << "Enter filename: ";
    filename: string = stdin.read_line();
    
    stddbg << "User entered: " << filename;
    
    // Validation
    when !file_exists(filename) then
        stderr << "ERROR: File not found: " << filename << "\n";
        return 1;
    end
    
    // Processing
    stdout << "Processing file...\n";
    Result: Result = process_file(filename);
    
    // Results (control plane - human-readable)
    stdout << "Complete! Processed " << result.lines << " lines\n";
    
    stddbg << "Program exiting";
    return 0;
}
```

**All control plane**:
- `stdin` for user input
- `stdout` for progress and results
- `stderr` for errors
- `stddbg` for debug traces

**No data plane needed** - this program is interactive, not part of a pipeline.

---

## Related Topics

- [Data Plane](data_plane.md) - Machine-facing streams
- [Six-Stream Topology](six_stream_topology.md) - Complete architecture
- [Stream Separation](stream_separation.md) - Best practices
- Individual streams: [stdin](stdin.md), [stdout](stdout.md), [stderr](stderr.md), [stddbg](stddbg.md)

---

**Remember**: Control plane is for **humans**. If a machine will read it, use the data plane instead.
