# stddati (Data Input Stream)

**Category**: I/O System → Data Plane  
**File Descriptor**: 4  
**Direction**: Input only  
**Philosophy**: Structured data deserves its own channel

---

## What is stddati?

`stddati` (pronounced "standard data input") is Aria's **structured data input stream**. It's part of the **data plane** - a separate I/O channel dedicated to machine-readable data formats like JSON, MessagePack, Protocol Buffers, etc.

**The Key Insight**: Human interaction (`stdin`) and structured data processing (`stddati`) are fundamentally different operations and should not share the same stream.

---

## Why stddati Exists

### The Problem: stdin is Ambiguous

In traditional programs, `stdin` is overloaded:

```python
# Is this reading user input or structured data?
data = sys.stdin.read()

# Must guess based on context:
if is_interactive():
    # User typed something
    process_user_input(data)
else:
    # Piped data (maybe JSON?)
    process_data(json.loads(data))
```

**Problems**:
1. **Ambiguity**: Is stdin for humans or machines?
2. **No type safety**: Could be text, JSON, binary, who knows?
3. **Mixed concerns**: Interactive prompts vs batch data processing
4. **Poor UX**: Can't prompt user while also accepting piped data

### Aria's Solution: Separate Data Stream

```aria
// Human input from stdin
user_name: string = stdin.read_line();

// Structured data from stddati
data: JsonValue = stddati.read_json();
```

**Result**: **No ambiguity**. `stdin` is for humans, `stddati` is for machines.

---

## Basic Usage

### Reading JSON

```aria
use std::json;

fn main() {
    // Read JSON object from stddati
    config: json::Value = stddati.read_json()?;
    
    stddbg << "Loaded config: " << config;
    
    // Access fields
    port: u16 = config["port"].as_u16()?;
    host: string = config["host"].as_string()?;
    
    stdout << "Starting server on " << host << ":" << port;
}
```

**Usage**:
```bash
# Pass JSON via stddati (fd 4)
echo '{"port": 8080, "host": "localhost"}' | ./program 4>&0
```

### Reading Binary Data (MessagePack)

```aria
use std::msgpack;

fn main() {
    // Read MessagePack from stddati
    data: msgpack::Value = stddati.read_msgpack()?;
    
    // Process binary-encoded data
    process(data);
}
```

### Reading Line-Delimited JSON (NDJSON)

```aria
use std::json;

fn main() {
    // Read multiple JSON objects, one per line
    lines = stddati.lines().collect();
    till(lines.length - 1, 1) {
        record: json::Value = json::parse(lines[$])?;
        process_record(record);
    }
}
```

**Input** (NDJSON format):
```json
{"id": 1, "value": "foo"}
{"id": 2, "value": "bar"}
{"id": 3, "value": "baz"}
```

---

## Stream Properties

| Property | Value |
|----------|-------|
| **File Descriptor** | 4 |
| **Direction** | Input only |
| **Buffering** | Block-buffered (optimized for throughput) |
| **Default Source** | `/dev/null` (no input) |
| **Type** | `Stream` (language primitive) |
| **Typical Formats** | JSON, MessagePack, CBOR, Protocol Buffers, NDJSON |

---

## Use Cases

### 1. ETL Pipelines

```aria
// Extract data from database, transform, output to stddato
fn main() {
    records = stddati.read_ndjson().collect();
    till(records.length - 1, 1) {
        transformed: Record = transform(records[$]);
        stddato << transformed;
    }
}
```

**Usage**:
```bash
# Chain programs using data plane
./extract 5>&1 | ./transform 4>&0 5>&1 | ./load 4>&0
```

### 2. API Data Processing

```aria
use std::json;

fn main() {
    // Read API request from stddati
    request: json::Value = stddati.read_json()?;
    
    stddbg << "Processing API request: " << request["endpoint"];
    
    // Process
    response: json::Value = handle_api_request(request);
    
    // Write response to stddato
    stddato.write_json(response);
}
```

### 3. Configuration Loading

```aria
use std::toml;

fn main() {
    // Load config from stddati (TOML format)
    config: Config = stddati.read_toml()?;
    
    stddbg << "Loaded config: " << config;
    
    // Use config to initialize application
    app: Application = Application::new(config);
    app.run();
}
```

**Usage**:
```bash
./program 4<config.toml
```

### 4. Batch Processing

```aria
fn main() {
    // Read batch of items from stddati
    items: []Item = stddati.read_json()?;
    
    stddbg << "Processing batch of " << items.len() << " items";
    
    results: []Result = [];
    till(items.length - 1, 1) {
        results.push(process_item(items[$]));
    }
    
    // Output results to stddato
    stddato.write_json(results);
}
```

---

## Dual-Mode Programs: stdin AND stddati

One of Aria's superpowers: **the same program can handle both human interaction and structured data**.

```aria
fn main() {
    // Check if stddati has input
    when stddati.has_input() then
        // Data mode: Read from stddati
        data: JsonValue = stddati.read_json()?;
        process_data(data);
    else
        // Interactive mode: Prompt user via stdin
        stdout << "Enter your name: ";
        name: string = stdin.read_line();
        stdout << "Hello, " << name << "!";
    end
}
```

**Usage**:

Interactive mode:
```bash
./program
# Enter your name: Alice
# Hello, Alice!
```

Data mode:
```bash
echo '{"name": "Alice"}' | ./program 4>&0
# Processes JSON data
```

**No code changes needed** - the program adapts based on whether `stddati` has input.

---

## Redirection Patterns

### Read from File
```bash
./program 4<input.json
# stddati reads from input.json
```

### Read from Pipe (stdin → stddati)
```bash
cat data.json | ./program 4>&0
# Redirect stdin (fd 0) to stddati (fd 4)
```

### Read from Another Program
```bash
./producer 5>&1 | ./consumer 4>&0
# producer's stddato → consumer's stddati
```

### Read from Network Socket
```bash
nc -l 8080 | ./program 4>&0
# Network data → stddati
```

---

## API Design

### Reading Methods

```aria
// Read entire stream as bytes
bytes: []u8 = stddati.read_all()?;

// Read line by line
lines = stddati.lines().collect();
till(lines.length - 1, 1) {
    process_line(lines[$]);
}

// Read fixed number of bytes
buffer: [1024]u8;
n: usize = stddati.read(buffer)?;

// Read structured formats (via std library)
json_data: json::Value = stddati.read_json()?;
msgpack_data: msgpack::Value = stddati.read_msgpack()?;
```

### Checking Availability

```aria
// Is stddati connected to actual input?
when stddati.has_input() then
    data: JsonValue = stddati.read_json()?;
    process(data);
else
    stderr << "No data input available on stddati";
end

// Is stddati open and valid?
when stddati.is_valid() then
    // Stream is usable
else
    // Stream is broken/closed
end
```

---

## Error Handling

### Using TBB Result Types

```aria
// read_json returns Result<JsonValue, IoError>
Result: Result<JsonValue, IoError> = stddati.read_json();

match result {
    Ok(data) => {
        stddbg << "Loaded data: " << data;
        process(data);
    }
    Err(err) => {
        stderr << "Failed to read JSON: " << err;
        return ERR;
    }
}
```

### Sticky Errors with TBB

```aria
// With TBB types, errors propagate automatically
data: JsonValue? = stddati.read_json()?;  // '?' propagates ERR

// If read_json fails, data is ERR
// All subsequent operations with 'data' will also be ERR
```

---

## Best Practices

### ✅ DO: Use for Structured Data

```aria
// Correct: Machine-readable data goes to stddati
config: Config = stddati.read_toml()?;
api_request: JsonValue = stddati.read_json()?;
batch_data: []Record = stddati.read_msgpack()?;
```

### ✅ DO: Validate Input

```aria
data: JsonValue = stddati.read_json()?;

// Validate schema
when !validate_schema(data) then
    stderr << "Invalid data format";
    return ERR;
end

process(data);
```

### ✅ DO: Check for Input Before Reading

```aria
when stddati.has_input() then
    data: JsonValue = stddati.read_json()?;
    process_batch(data);
else
    // No batch data, fall back to interactive mode
    run_interactive();
end
```

### ❌ DON'T: Use for Human Input

```aria
// WRONG: Humans should use stdin, not stddati
stddati << "Enter your name: ";  // Won't work (input only!)
name: string = stddati.read_line();

// RIGHT: Use stdin for human interaction
stdout << "Enter your name: ";
name: string = stdin.read_line();
```

### ❌ DON'T: Mix with stdin

```aria
// WRONG: Ambiguous - which stream is which?
user_input: string = stdin.read_line();
data: JsonValue = stddati.read_json()?;

// RIGHT: Clearly separate concerns
when interactive_mode then
    user_input: string = stdin.read_line();
else
    data: JsonValue = stddati.read_json()?;
end
```

---

## Comparison to Other Languages

### Python
```python
import sys
import json

# stdin is overloaded - could be human or machine
data = json.load(sys.stdin)  # Assumes JSON, breaks if user input
```

### Go
```go
import "encoding/json"
import "os"

// Must use stdin for everything
var data map[string]interface{}
json.NewDecoder(os.Stdin).Decode(&data)
```

### Rust
```rust
use std::io;
use serde_json;

// stdin is the only option
let data: serde_json::Value = serde_json::from_reader(io::stdin())?;
```

### Aria
```aria
// Dedicated stream for structured data
data: JsonValue = stddati.read_json()?;

// stdin remains available for human interaction
user_input: string = stdin.read_line();
```

**Only Aria** separates human input from structured data input at the language level.

---

## Pipeline Composition

### Traditional Unix Pipes (Limited)
```bash
cat data.json | ./process | ./transform
# All data flows through stdout → stdin
# No separation of concerns
```

### Aria Data Plane Pipes
```bash
./extract 5>&1 | ./transform 4>&0 5>&1 | ./load 4>&0
# Data flows through stddato → stddati
# stdin/stdout remain free for control
```

### Aria Pipeline Operator (Sugar)
```aria
// Built-in operator for data plane composition
./extract |> ./transform |> ./load
// Automatically connects stddato → stddati
```

---

## Real-World Example: Log Processor

```aria
use std::json;

fn main() {
    stddbg << "Starting log processor";
    
    // Read NDJSON logs from stddati
    lines = stddati.lines().collect();
    till(lines.length - 1, 1) {
        log_entry: json::Value = json::parse(lines[$])?;
        
        // Extract fields
        timestamp: string = log_entry["timestamp"].as_string()?;
        level: string = log_entry["level"].as_string()?;
        message: string = log_entry["message"].as_string()?;
        
        // Filter: Only output errors
        when level == "ERROR" then
            // Output to stddato (for downstream processing)
            stddato.write_json(log_entry);
            
            // Also print to stdout for humans
            stdout << "[" << timestamp << "] ERROR: " << message;
        end
    }
    
    stddbg << "Log processing complete";
}
```

**Usage**:
```bash
# Process logs, output errors to file
cat /var/log/app.log | ./log_processor 4>&0 5>errors.json
```

**Result**:
- **stddati**: Reads logs (NDJSON)
- **stddato**: Outputs filtered errors (JSON)
- **stdout**: Human-readable error summary
- **stddbg**: Diagnostic info (start/complete)

All four streams working in harmony!

---

## The Philosophy

**Traditional Approach**: "stdin is the universal input - make it work for everything"

**Aria's Approach**: "Human input and structured data are fundamentally different - separate them"

This is the **control plane vs data plane** principle applied to input:
- **Control plane** (stdin): Humans, commands, interactive queries
- **Data plane** (stddati): JSON, MessagePack, database dumps, API payloads

---

## Related Topics

- [stddato](stddato.md) - Structured data output
- [Six-Stream Topology](six_stream_topology.md) - Full I/O architecture
- [Data Plane](data_plane.md) - Data plane design
- [stdin](stdin.md) - Comparison with standard input

---

**Remember**: When you see structured data (JSON, binary formats, etc.), think `stddati`. When you see human interaction (prompts, commands), think `stdin`. Keep them separate, keep them clean.
