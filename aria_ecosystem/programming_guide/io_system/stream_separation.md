# Stream Separation Best Practices

**Category**: I/O System → Best Practices  
**Philosophy**: The right data on the right stream at the right time

---

## The Golden Rule

**Each stream has ONE purpose. Never mix them.**

| Stream | Purpose | For |
|--------|---------|-----|
| **stdin** | User input | Humans typing commands |
| **stdout** | Normal output | Humans reading results |
| **stderr** | Error messages | Humans seeing what went wrong |
| **stddati** | Structured input | Machines sending data |
| **stddato** | Structured output | Machines receiving data |
| **stddbg** | Debug traces | Developers diagnosing issues |

**If you violate this rule, you're fighting the system instead of using it.**

---

## Decision Flow Chart

```
Need to output something?
├─ Is it an error message?
│  └─ YES → stderr
│
├─ Is it debug/diagnostic info?
│  └─ YES → stddbg
│
├─ Is it structured data (JSON, binary)?
│  └─ YES → stddato
│
└─ Is it normal human-readable output?
   └─ YES → stdout
```

For input:
```
Need to read something?
├─ Is it user typing interactively?
│  └─ YES → stdin
│
└─ Is it structured data (JSON, binary)?
   └─ YES → stddati
```

---

## Common Mistakes (and Fixes)

### ❌ Mistake 1: Debug Output on stdout

**WRONG**:
```aria
fn process(data: Data) -> Result {
    stdout << "DEBUG: Processing data\n";  // Pollutes output!
    
    Result: Result = transform(data);
    stdout << result;  // Mixed with debug info
    
    return result;
}
```

**RIGHT**:
```aria
fn process(data: Data) -> Result {
    stddbg << "Processing data";  // Separate stream
    
    Result: Result = transform(data);
    stdout << result;  // Clean output
    
    return result;
}
```

**Why it matters**:
```bash
# WRONG version: stdout is polluted
./program > output.txt
# output.txt contains: "DEBUG: Processing data\nActual Result"

# RIGHT version: stdout is clean
./program > output.txt 6>debug.log
# output.txt contains: "Actual Result"
# debug.log contains: "Processing data"
```

---

### ❌ Mistake 2: JSON on stdout

**WRONG**:
```aria
fn main() {
    Result: Result = compute();
    
    // Which format? Can't have both!
    when json_mode then
        stdout << json_dumps(result);  // Machines
    else
        stdout << "Result: " << result;  // Humans
    end
}
```

**RIGHT**:
```aria
fn main() {
    Result: Result = compute();
    
    // Both formats, always!
    stdout << "Result: " << result;      // Humans
    stddato.write_json(result);          // Machines
}
```

**Why it matters**:
- **WRONG**: Must choose at runtime (flags, env vars)
- **RIGHT**: Both outputs simultaneously, no choices needed

---

### ❌ Mistake 3: Errors on stdout

**WRONG**:
```aria
when file.open() == ERR then
    stdout << "ERROR: Cannot open file\n";  // Wrong stream!
    return ERR;
end
```

**RIGHT**:
```aria
when file.open() == ERR then
    stderr << "ERROR: Cannot open file\n";  // Correct stream
    return ERR;
end
```

**Why it matters**:
```bash
# Capture output to file
./program > results.txt

# WRONG: Error messages end up in results.txt (bad!)
# RIGHT: Error messages go to terminal (visible immediately)
```

---

### ❌ Mistake 4: Structured Data on stdin

**WRONG**:
```aria
// Ambiguous: Is this user input or JSON?
data: string = stdin.read_all();
json: JsonValue = parse_json(data);  // Might fail!
```

**RIGHT**:
```aria
// Clear: stddati is for structured data
json: JsonValue = stddati.read_json()?;  // Type-safe
```

**Why it matters**:
- **WRONG**: stdin could be interactive (user typing), breaking JSON parse
- **RIGHT**: stddati is explicitly for data, no ambiguity

---

## Real-World Example: Complete Separation

Here's a well-designed Aria program using all six streams correctly:

```aria
use std::json;

fn main() -> i32 {
    stddbg << "Program starting";
    
    // Read configuration (structured data)
    config: Config? = stddati.read_json()?;
    
    when config == ERR then
        stderr << "ERROR: Failed to read config\n";
        stderr << "  Provide config on fd 4\n";
        return 1;
    end
    
    stddbg << "Config loaded: " << config.mode;
    
    // Interactive prompt for user (if needed)
    when config.mode == "interactive" then
        stdout << "Enter filename: ";
        filename: string = stdin.read_line();
        
        stddbg << "User entered: " << filename;
    end
    
    // Process data
    stdout << "Processing...\n";
    
    Result: Result? = process(config);
    
    when result == ERR then
        stderr << "ERROR: Processing failed\n";
        stddbg << "Failure details: " << get_error_details();
        return 1;
    end
    
    // Output results
    stdout << "Processing complete!\n";
    stdout << "  Records processed: " << result.count << "\n";
    
    // Structured output for downstream tools
    stddato.write_json({
        "status": "success",
        "records": result.count,
        "timestamp": Time::now().unix()
    });
    
    stddbg << "Program exiting cleanly";
    
    return 0;
}
```

**Usage patterns**:

```bash
# Development: See everything
echo '{"mode": "batch"}' | ./program 4>&0 6>&2

# Production: Clean output, log debug
echo '{"mode": "batch"}' | ./program 4>&0 6>/var/log/app.log

# API mode: Capture structured output
echo '{"mode": "batch"}' | ./program 4>&0 5>result.json 1>/dev/null

# CI mode: Separate all streams
echo '{"mode": "batch"}' | ./program 4>&0 >out.txt 2>err.txt 5>data.json 6>debug.log
```

---

## Principles of Good Stream Usage

### 1. **Audience Separation**

- **Humans**: stdin, stdout, stderr
- **Machines**: stddati, stddato
- **Developers**: stddbg

**Never mix audiences on the same stream.**

### 2. **Persistence of Debug Code**

**Traditional approach**:
```python
# print("DEBUG: foo")  # Commented out for production
process(data)
```

**Aria approach**:
```aria
stddbg << "Processing foo";  // Always present, redirected in production
process(data);
```

**Benefit**: Debug code **never disappears** - it's always there when you need it.

### 3. **Dual-Mode Output**

Always provide **both** human and machine output:

```aria
// Don't make users choose
stdout << "Balance: $" << balance;     // Humans
stddato.write_json({"balance": balance});  // Machines
```

### 4. **Stream Redirection Over Conditionals**

**Bad** (traditional):
```aria
when verbose_mode then
    stdout << "Detailed info...";
end
```

**Good** (Aria):
```aria
stddbg << "Detailed info...";  // Always written, redirect controls visibility
```

**Why**: Configuration happens at **deployment time** (shell redirection), not **compile time** (flags and conditionals).

### 5. **Error Clarity**

Errors go to **stderr**, not stdout or stddbg:

```aria
// Clear error reporting
when validation_failed then
    stderr << "ERROR: Invalid input\n";
    stderr << "  Field: " << field_name << "\n";
    stderr << "  Reason: " << reason << "\n";
    return ERR;
end
```

---

## Testing Stream Separation

### Unit Test Example

```aria
#[test]
fn test_stream_separation() {
    // Capture all streams
    capture_streams();
    
    run_program();
    
    // Verify each stream has correct content
    assert!(stdout_contains("Processing complete"));
    assert!(stderr_contains("ERROR") == false);  // No errors
    assert!(stddbg_contains("Debug trace"));
    assert!(stddato_is_valid_json());
}
```

### Integration Test (Shell)

```bash
#!/bin/bash

# Run program with separated streams
echo '{"test": true}' | \
  ./program 4>&0 \
  >stdout.txt \
  2>stderr.txt \
  5>stddato.json \
  6>stddbg.txt

# Verify stdout
grep -q "Processing complete" stdout.txt || exit 1

# Verify stderr is empty (no errors)
[ ! -s stderr.txt ] || exit 1

# Verify stddato is valid JSON
jq . stddato.json >/dev/null || exit 1

# Verify stddbg has trace
grep -q "Program starting" stddbg.txt || exit 1

echo "All streams correctly separated!"
```

---

## Migration from Three-Stream Model

If you're coming from a traditional language:

### Before (Python)

```python
def process(data):
    if DEBUG:
        print(f"DEBUG: Processing {data}", file=sys.stderr)
    
    result = transform(data)
    
    if JSON_OUTPUT:
        print(json.dumps(result))
    else:
        print(f"Result: {result}")
    
    return result
```

**Problems**:
- Debug output mixed with errors (stderr)
- Must choose output format at runtime
- Conditional logic everywhere

### After (Aria)

```aria
fn process(data: Data) -> Result {
    stddbg << "Processing " << data;  // Debug stream
    
    Result: Result = transform(data);
    
    stdout << "Result: " << result;   // Human output
    stddato.write_json(result);       // Machine output
    
    return result;
}
```

**Benefits**:
- Each stream has one purpose
- Both output formats always available
- No conditionals, cleaner code

---

## Stream Hierarchy

Think of the six streams as organized by **concern**:

```
Control Plane (Human-Facing)
├─ stdin  → Interactive input
├─ stdout → Normal output
├─ stderr → Error messages
└─ stddbg → Debug traces

Data Plane (Machine-Facing)
├─ stddati → Structured input
└─ stddato → Structured output
```

**Rule**: Control plane streams are for humans, data plane streams are for machines. **Never cross them.**

---

## When to Break the Rules (Rarely)

### Exception 1: Logging Libraries

If you're using a logging library that **only** supports stdout/stderr, you might need to violate separation temporarily:

```aria
// Temporary: Log library doesn't know about stddbg
logger.debug("Message");  // Goes to stderr unfortunately

// Better: Wrap it
fn log_debug(msg: string) {
    stddbg << msg;  // Also send to stddbg
    logger.debug(msg);  // And to logger for compatibility
}
```

### Exception 2: Legacy System Integration

If integrating with a system that expects everything on stdout:

```aria
// Compatibility mode
when legacy_mode then
    stdout << debug_info;  // Violation, but necessary
else
    stddbg << debug_info;  // Preferred
end
```

**But**: Try to fix the legacy system instead of working around it.

---

## Summary: The Six-Stream Mindset

| Stream | Use When... |
|--------|-------------|
| **stdin** | User is typing commands or answering prompts |
| **stdout** | You want a human to see normal output |
| **stderr** | Something went wrong and a human needs to know |
| **stddati** | Reading JSON, binary data, or API payloads |
| **stddato** | Writing JSON, binary data, or API responses |
| **stddbg** | You're debugging and need to see what's happening |

**Mnemonic**: 
- **stdin/stdout/stderr** = **Control Plane** = **Humans**
- **stddati/stddato** = **Data Plane** = **Machines**
- **stddbg** = **Control Plane** = **Developers**

---

## Related Topics

- [Six-Stream Topology](six_stream_topology.md) - Architecture overview
- [I/O Overview](io_overview.md) - Why six streams?
- [Control Plane](control_plane.md) vs [Data Plane](data_plane.md)
- Individual stream docs: [stdin](stdin.md), [stdout](stdout.md), [stderr](stderr.md), [stddati](stddati.md), [stddato](stddato.md), [stddbg](stddbg.md)

---

**Remember**: Stream separation is **not** extra work - it's **less** work. No conditionals, no flags, no choosing. Just write the right data to the right stream, and let the deployment environment redirect as needed.

This is the Aria way.
