# stderr (Standard Error)

**Category**: I/O System → Control Plane  
**File Descriptor**: 2  
**Direction**: Output only  
**Philosophy**: Errors are not data, not debug info - they're errors

---

## What is stderr?

`stderr` (standard error) is the traditional **error message stream**. It's file descriptor 2, used exclusively for error messages and warnings.

**In Aria's six-stream model**: `stderr` is part of the **control plane**, separate from both stdout and stddbg.

---

## Why stderr is Separate

The Unix tradition gave us **three streams** for a reason:
- **stdin (0)**: Input
- **stdout (1)**: Output
- **stderr (2)**: Errors

**Key insight**: Errors should **never mix with normal output**.

```bash
# This works because stderr is separate
./program > results.txt 2> errors.txt
# results.txt gets clean output
# errors.txt gets only errors
```

---

## Basic Usage

```aria
// Simple error message
stderr << "ERROR: File not found\n";

// Error with details
stderr << "ERROR: Cannot open '" << filename << "': " << error_msg << "\n";

// Warning message
stderr << "WARNING: Deprecated feature used\n";
```

---

## stderr vs stdout vs stddbg

**The Three Output Streams**:

| Stream | Purpose | Example |
|--------|---------|---------|
| **stdout** | Normal output for humans | `stdout << "Processing complete\n";` |
| **stderr** | Error messages | `stderr << "ERROR: Invalid input\n";` |
| **stddbg** | Debug/diagnostic info | `stddbg << "Entering function foo()";` |

**Why three?**

```aria
fn main() {
    stddbg << "Program starting";  // Debug trace
    
    Result: Result = process();
    
    match result {
        Ok(data) => {
            stdout << "Success: " << data;  // Normal output
        }
        Err(err) => {
            stderr << "ERROR: " << err;  // Error message
        }
    }
}
```

**Usage**:
```bash
# Production: Hide debug, show errors
./program 6>/dev/null 2>&1  # stderr to terminal, stddbg discarded

# Development: See everything
./program 6>&2  # stderr and stddbg both visible

# CI: Separate all three
./program >out.txt 2>err.txt 6>debug.log
```

---

## Error Reporting Patterns

### Simple Error

```aria
when !file.exists() then
    stderr << "ERROR: File not found: " << filename << "\n";
    return ERR;
end
```

### Error with Context

```aria
when balance < amount then
    stderr << "ERROR: Insufficient funds\n";
    stderr << "  Balance: $" << balance << "\n";
    stderr << "  Required: $" << amount << "\n";
    return ERR;
end
```

### Stack Trace on Panic

```aria
// On panic, runtime outputs to stderr
panic("Critical error occurred");
// stderr: PANIC: Critical error occurred
//         at main() (file.aria:42)
//         at _start() (runtime:15)
```

---

## Warnings vs Errors

### Errors (Fatal)

```aria
// Error: Stop execution
when !validate(data) then
    stderr << "ERROR: Invalid data format\n";
    return ERR;  // Stop
end
```

### Warnings (Non-Fatal)

```aria
// Warning: Continue execution
when version < MIN_VERSION then
    stderr << "WARNING: Old version detected, please upgrade\n";
    // But continue anyway
end
```

---

## Best Practices

### ✅ DO: Use for Actual Errors

```aria
stderr << "ERROR: Cannot connect to database\n";
stderr << "FATAL: Out of memory\n";
```

### ✅ DO: Prefix with Severity

```aria
stderr << "ERROR: " << message << "\n";
stderr << "WARNING: " << message << "\n";
stderr << "FATAL: " << message << "\n";
```

### ✅ DO: Include Context

```aria
stderr << "ERROR: Failed to parse JSON\n";
stderr << "  File: " << filename << "\n";
stderr << "  Line: " << line_num << "\n";
stderr << "  Reason: " << err.message << "\n";
```

### ❌ DON'T: Use for Debug Output

```aria
// WRONG: Debug info should go to stddbg
stderr << "DEBUG: Entering function\n";

// RIGHT:
stddbg << "Entering function";
```

### ❌ DON'T: Use for Normal Output

```aria
// WRONG: Normal output goes to stdout
stderr << "Processing complete\n";

// RIGHT:
stdout << "Processing complete\n";
```

### ❌ DON'T: Mix Errors with Data

```aria
// WRONG: JSON on stderr is weird
stderr << json_dumps(error_object);

// RIGHT: Errors are human-readable
stderr << "ERROR: " << error_object.message << "\n";
```

---

## Error Exit Codes

Combine `stderr` with proper exit codes:

```aria
fn main() -> i32 {
    Result: Result = process();
    
    match result {
        Ok(data) => {
            stdout << "Success\n";
            return 0;  // Success exit code
        }
        Err(err) => {
            stderr << "ERROR: " << err << "\n";
            return 1;  // Error exit code
        }
    }
}
```

**Shell usage**:
```bash
./program
if [ $? -eq 0 ]; then
    echo "Program succeeded"
else
    echo "Program failed (check stderr)"
fi
```

---

## Redirection Examples

```bash
# Errors to file, output to terminal
./program 2> errors.txt

# Both to same file
./program > combined.txt 2>&1

# Errors to terminal, output to file
./program > output.txt
# (stderr still goes to terminal)

# Discard errors (dangerous!)
./program 2> /dev/null

# Separate everything
./program >out.txt 2>err.txt 5>data.json 6>debug.log
```

---

## Related Topics

- [stdout](stdout.md) - Normal output
- [stddbg](stddbg.md) - Debug output (vs stderr)
- [Control Plane](control_plane.md) - stderr is part of control plane
- [Six-Stream Topology](six_stream_topology.md)

---

**Remember**: `stderr` is **only for errors**. Debug info goes to `stddbg`. Normal output goes to `stdout`. Keep them separate!
