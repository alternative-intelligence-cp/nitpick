# Data Plane

**Category**: I/O System → Architecture  
**Streams**: stddati (4), stddato (5)  
**Purpose**: Structured data processing, APIs, ETL pipelines

---

## What is the Data Plane?

The **data plane** consists of two streams dedicated to **structured data** exchange between programs:

- **stddati** (FD 4) - Structured data input (JSON, MessagePack, etc.)
- **stddato** (FD 5) - Structured data output

These streams are for **machines**, not people.

---

## Data Plane vs Control Plane

| Aspect | Data Plane | Control Plane |
|--------|------------|---------------|
| **Audience** | Machines (APIs, pipelines, databases) | Humans (users, operators, developers) |
| **Streams** | stddati, stddato | stdin, stdout, stderr, stddbg |
| **Format** | Structured data (JSON, binary) | Human-readable text (mostly) |
| **Purpose** | Data processing, ETL, APIs | Interaction, monitoring, debugging |
| **Buffering** | Block-buffered (throughput) | Line-buffered (responsiveness) |

---

## Why Separate Data from Control?

### The Problem with Traditional stdin/stdout

In traditional programs, `stdin` and `stdout` are overloaded:

```python
# Is this for humans or machines?
data = sys.stdin.read()  # Could be user typing OR piped JSON
print(result)  # Could be for terminal OR next program in pipeline
```

**Result**: Programs must choose to serve **either** humans **or** machines, not both.

### Aria's Solution: Dedicated Data Streams

```aria
// Humans use control plane
stdout << "Processing...";

// Machines use data plane
data: JsonValue = stddati.read_json()?;
stddato.write_json(result);
```

**Result**: Same program serves **both** humans and machines simultaneously.

---

## Data Plane Streams

### stddati (FD 4)
**Structured data input**

```aria
// Read JSON configuration
config: Config = stddati.read_json()?;

// Read binary data
data: []u8 = stddati.read_all()?;
```

### stddato (FD 5)
**Structured data output**

```aria
// Write JSON results
stddato.write_json({"status": "ok", "count": 42});

// Write binary data
stddato.write_msgpack(large_dataset);
```

---

## When to Use Data Plane

Use data plane streams when:
- ✅ Reading/writing JSON, XML, YAML
- ✅ Processing binary formats (MessagePack, Protocol Buffers)
- ✅ Building ETL pipelines
- ✅ Implementing APIs
- ✅ Exporting data for other tools
- ✅ Batch processing

**Do NOT use data plane for**:
- ❌ User prompts or interactive input (use stdin)
- ❌ Progress messages (use stdout)
- ❌ Error messages (use stderr)
- ❌ Debug traces (use stddbg)

---

## Pipeline Composition

### Traditional Unix Pipe (Limited)

```bash
./extract | ./transform | ./load
# All data flows through stdout → stdin
# No room for progress messages (would break pipeline)
```

### Aria Data Plane Pipe

```bash
./extract 5>&1 | ./transform 4>&0 5>&1 | ./load 4>&0
# Data flows: stddato → stddati
# stdout/stderr still available for monitoring!
```

Or using Aria's pipeline operator:
```bash
./extract |> ./transform |> ./load
# Automatically connects stddato → stddati
```

---

## Example: Data Plane ETL Pipeline

```aria
fn main() {
    stddbg << "ETL pipeline starting";
    
    // Read structured data from stddati
    records: []Record = stddati.read_ndjson()?;
    
    stdout << "Processing " << records.len() << " records\n";
    
    // Transform
    transformed: []Record = [];
    till(records.len() - 1, 1) {
        // Progress on control plane
        when $ % 1000 == 0 then
            stdout << "  Progress: " << $ << "/" << records.len() << "\n";
        end
        
        // Debug trace
        stddbg << "Transforming record " << records[$].id;
        
        transformed.push(transform(records[$]));
    }
    
    // Write transformed data to stddato
    stddato.write_json(transformed);
    
    stdout << "Pipeline complete!\n";
    stddbg << "Pipeline finished in " << elapsed << "ms";
}
```

**Usage**:
```bash
cat input.ndjson | ./pipeline 4>&0 5>output.json
# Data plane: input.ndjson → stddati, stddato → output.json
# Control plane: stdout shows progress, stddbg can be logged
```

---

## Dual-Mode Programs

The same program can serve **both** APIs and humans:

```aria
fn main() {
    Result: Result = process();
    
    // Human output (control plane)
    stdout << "Processing complete\n";
    stdout << "  Records: " << result.count << "\n";
    stdout << "  Errors: " << result.errors << "\n";
    
    // API output (data plane)
    stddato.write_json({
        "status": "success",
        "records": result.count,
        "errors": result.errors,
        "timestamp": Time::now().unix()
    });
}
```

**Usage**:

Human mode:
```bash
./program
# Sees friendly output on stdout
```

API mode:
```bash
./program 5>result.json 1>/dev/null
# Gets JSON on fd 5, stdout discarded
```

Both modes:
```bash
./program 5>result.json
# Humans see stdout, machines get JSON
```

---

## Data Formats

Common formats on the data plane:

### JSON
```aria
data: JsonValue = stddati.read_json()?;
stddato.write_json(result);
```

### NDJSON (Newline-Delimited JSON)
```aria
lines = stddati.lines().collect();
till(lines.length - 1, 1) {
    record: JsonValue = json::parse(lines[$])?;
    process(record);
}
```

### MessagePack (Binary)
```aria
data: msgpack::Value = stddati.read_msgpack()?;
stddato.write_msgpack(result);
```

### Protocol Buffers
```aria
data: MyProto = stddati.read_protobuf()?;
stddato.write_protobuf(result);
```

---

## Best Practices

### ✅ DO: Use for Structured Data

```aria
// JSON, binary formats, serialized objects
stddato.write_json({"key": "value"});
```

### ✅ DO: Keep Control and Data Separate

```aria
// Humans see progress
stdout << "Processing batch " << batch_id << "\n";

// Machines get data
stddato.write_json(batch_result);
```

### ✅ DO: Check if Data Stream is Available

```aria
when stddati.has_input() then
    // Batch mode: process data
    data: JsonValue = stddati.read_json()?;
    process_batch(data);
else
    // Interactive mode: prompt user
    run_interactive();
end
```

### ❌ DON'T: Mix Human and Machine Data

```aria
// WRONG: Mixing progress with JSON
stddato << "Processing...\n";  // Breaks JSON parser!
stddato.write_json(result);

// RIGHT: Separate streams
stdout << "Processing...\n";   // Human
stddato.write_json(result);    // Machine
```

---

## Related Topics

- [Control Plane](control_plane.md) - Human-facing streams
- [Six-Stream Topology](six_stream_topology.md) - Complete architecture
- [Stream Separation](stream_separation.md) - Best practices
- [stddati](stddati.md), [stddato](stddato.md) - Detailed stream docs

---

**Remember**: Data plane is for **machines**. If a human will read it, use the control plane instead.
