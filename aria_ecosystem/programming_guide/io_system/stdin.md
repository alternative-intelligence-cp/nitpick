# stdin (Standard Input)

**Category**: I/O System → Control Plane  
**File Descriptor**: 0  
**Direction**: Input only  
**Philosophy**: Interactive user input, separate from structured data

---

## Overview

`stdin` is the traditional input stream for **human interaction**. In Aria's six-stream model, it's part of the **control plane** - dedicated to interactive prompts, commands, and user-typed input.

**Key Distinction**: For **structured data** (JSON, binary), use [stddati](stddati.md) instead.

---

## Basic Usage

```aria
// Read a line
name: string = stdin.read_line();

// Read all input
content: string = stdin.read_all();

// With prompt
stdout << "Enter name: ";
name: string = stdin.read_line();

// Check if interactive (TTY)
when stdin.is_tty() then
    stdout << "Interactive mode";
else
    stddbg << "Piped input";
end
```

---

## stdin vs stddati

| Aspect | stdin | stddati |
|--------|-------|---------|
| **For** | Humans typing | Machines sending data |
| **Format** | Text (usually) | JSON, binary, structured |
| **Use Case** | Interactive prompts | Data processing pipelines |
| **Example** | User entering password | API sending JSON payload |

---

## When to Use stdin

✅ **DO** use stdin for:
- Interactive user prompts
- Reading commands from terminal
- User typing filenames, options, etc.

❌ **DON'T** use stdin for:
- JSON or structured data (use `stddati`)
- Binary data streams (use `stddati`)
- Configuration files (use `stddati`)

---

## Example

```aria
fn main() {
    // Interactive mode
    when stdin.is_tty() then
        stdout << "Enter your name: ";
        name: string = stdin.read_line();
        stdout << "Hello, " << name << "!\n";
    else
        // Piped mode (read text)
        lines: []string = stdin.lines().collect();
        stddbg << "Read " << lines.len() << " lines from pipe";
    end
}
```

---

## See Also

- [stddati](stddati.md) - For structured/binary data input
- [stdout](stdout.md) - Output counterpart
- [Control Plane](control_plane.md) - stdin is part of control plane
- [Six-Stream Topology](six_stream_topology.md) - Full I/O architecture
