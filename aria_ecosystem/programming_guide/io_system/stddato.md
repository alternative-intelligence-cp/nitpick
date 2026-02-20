# stddato (Data Output Stream)

**Category**: I/O System → Data Plane  
**File Descriptor**: 5  
**Direction**: Output only  
**Philosophy**: APIs and humans shouldn't fight over stdout

---

## What is stddato?

`stddato` (pronounced "standard data output") is Aria's **structured data output stream**. It's the output counterpart to `stddati`, dedicated to machine-readable formats like JSON, MessagePack, Protocol Buffers, and other structured data.

**The Revolutionary Part**: Your program can emit **both** human-readable output (via `stdout`) and machine-readable data (via `stddato`) **simultaneously**, without any conflicts.

---

## Why stddato Exists

### The Problem: stdout is Overloaded

Traditional programs must choose:

**Option 1**: Human-friendly output
```python
print(f"Processed {count} records")  # For humans
print(f"Total: ${total:.2f}")
```

**Option 2**: Machine-friendly output
```python
import json
print(json.dumps({"count": count, "total": total}))  # For APIs
```

**You can't have both!** If you print human-readable messages, you pollute the JSON output. If you only print JSON, humans can't read it.

**Common workaround** (terrible):
```python
if os.getenv("JSON_OUTPUT"):
    print(json.dumps({"count": count, "total": total}))
else:
    print(f"Processed {count} records")
```

**Problems**:
1. **Code duplication** - same data, two formats
2. **Conditional logic** - pollutes codebase
3. **Environment dependency** - fragile
4. **Still can't have both** - must choose at runtime

### Aria's Solution: Dual Output Streams

```aria
// Human-readable output to stdout
stdout << "Processed " << count << " records";
stdout << "Total: $" << total;

// Machine-readable output to stddato
stddato.write_json({"count": count, "total": total});
```

**Result**: **Both outputs simultaneously**. No conflicts, no conditionals, no compromises.

---

## Basic Usage

### Writing JSON

```aria
use std::json;

fn main() {
    // Process some data
    Result: Result = process_data();
    
    // Human output
    stdout << "Processing complete";
    stdout << "Records processed: " << result.count;
    
    // Machine output
    stddato.write_json({
        "status": "success",
        "count": result.count,
        "total": result.total
    });
}
```

**Usage**:
```bash
# See human output in terminal
./program

# Capture JSON to file
./program 5>output.json

# Both at once!
./program 5>output.json
# stdout shows progress in terminal
# output.json contains structured data
```

### Writing Binary Data (MessagePack)

```aria
use std::msgpack;

fn main() {
    data: LargeStruct = compute_results();
    
    // Human summary
    stdout << "Computation complete: " << data.size() << " elements";
    
    // Binary output (efficient)
    stddato.write_msgpack(data);
}
```

### Writing NDJSON (Newline-Delimited JSON)

```aria
use std::json;

fn main() {
    records = process_records().collect();
    till(records.length - 1, 1) {
        // Human progress
        stdout << "Processing record " << records[$].id;
        
        // Machine output (one JSON per line)
        stddato.write_json(records[$]);
        stddato << "\n";  // Newline separator
    }
}
```

**Output** (NDJSON):
```json
{"id": 1, "value": "foo"}
{"id": 2, "value": "bar"}
{"id": 3, "value": "baz"}
```

---

## Stream Properties

| Property | Value |
|----------|-------|
| **File Descriptor** | 5 |
| **Direction** | Output only |
| **Buffering** | Block-buffered (optimized for throughput) |
| **Default Target** | `/dev/null` (no output unless redirected) |
| **Type** | `Stream` (language primitive) |
| **Typical Formats** | JSON, MessagePack, CBOR, Protocol Buffers, NDJSON |

---

## Use Cases

### 1. Dual-Mode CLIs

```aria
fn main() {
    // Parse args
    args: Args = parse_args();
    
    // Perform operation
    Result: Result = execute_operation(args);
    
    // Human-friendly output (always shown)
    stdout << "Operation successful";
    stdout << "  Files processed: " << result.files_processed;
    stdout << "  Errors: " << result.errors;
    
    // Machine-readable output (captured if redirected)
    stddato.write_json({
        "status": "success",
        "files_processed": result.files_processed,
        "errors": result.errors,
        "timestamp": Time::now().iso8601()
    });
}
```

**Usage**:

Human mode:
```bash
./tool
# Operation successful
#   Files processed: 42
#   Errors: 0
```

API mode:
```bash
./tool 5>result.json
# Human output still appears on stdout
# JSON captured to result.json
```

Programmatic consumption:
```bash
result=$(./tool 5>&1 1>/dev/null)  # Get JSON only
# result contains: {"status": "success", ...}
```

### 2. ETL Pipelines

```aria
fn main() {
    stddbg << "Starting ETL pipeline";
    
    // Read from stddati
    input_data: []Record = stddati.read_json()?;
    
    // Transform
    output_data: []Record = [];
    till(input_data.length - 1, 1) {
        transformed: Record = transform(input_data[$]);
        output_data.push(transformed);
        
        // Progress to stdout
        stdout << "Processed record " << input_data[$].id;
    }
    
    // Write to stddato
    stddato.write_json(output_data);
    
    stddbg << "Pipeline complete: " << output_data.len() << " records";
}
```

**Pipeline composition**:
```bash
./extract 5>&1 | ./transform 4>&0 5>&1 | ./load 4>&0
# Data flows: extract.stddato → transform.stddati
#             transform.stddato → load.stddati
# stdout shows progress from all three programs
```

### 3. API Server Responses

```aria
fn handle_request(req: HttpRequest) -> HttpResponse {
    // Log request (human-readable)
    stdout << req.method << " " << req.path;
    
    // Debug trace
    stddbg << "Headers: " << req.headers.len();
    
    // Process request
    data: ResponseData = process_request(req);
    
    // Structured response
    stddato.write_json({
        "status": 200,
        "data": data,
        "timestamp": Time::now().unix()
    });
    
    return HttpResponse::ok(data);
}
```

### 4. Monitoring and Metrics

```aria
fn main() {
    start_time: Time = Time::now();
    
    Result: Result = run_job();
    
    elapsed: Duration = Time::now() - start_time;
    
    // Human output
    stdout << "Job completed in " << elapsed.seconds() << "s";
    
    // Metrics (for monitoring systems)
    stddato.write_json({
        "job": "data_processing",
        "duration_ms": elapsed.milliseconds(),
        "records_processed": result.count,
        "success": true
    });
}
```

**Monitoring integration**:
```bash
# Capture metrics to timeseries database
./job 5> >(curl -X POST http://metrics.local/ingest --data-binary @-)
```

---

## Redirection Patterns

### Write to File
```bash
./program 5>output.json
```

### Write to Pipe
```bash
./program 5>&1 | jq .
# Send stddato to stdout, pipe to jq for formatting
```

### Separate Files for Different Outputs
```bash
./program >human.txt 5>data.json 6>debug.log
# stdout → human.txt
# stddato → data.json  
# stddbg → debug.log
```

### Chain Programs (Data Plane)
```bash
./producer 5>&1 | ./consumer 4>&0
# producer.stddato → consumer.stddati
```

### Network Socket
```bash
./program 5> >(nc metrics.server.com 9000)
# Stream data to network
```

---

## API Design

### Writing Methods

```aria
// Write bytes
bytes: []u8 = get_data();
stddato.write(bytes)?;

// Write string
stddato << "Hello, world\n";

// Write structured data (via std library)
stddato.write_json({"key": "value"})?;
stddato.write_msgpack(data)?;

// Flush buffer
stddato.flush()?;
```

### Checking Availability

```aria
// Is stddato redirected to something other than /dev/null?
when stddato.is_redirected() then
    // Yes, write data
    stddato.write_json(expensive_data)?;
else
    // No, skip expensive serialization
    stddbg << "stddato not redirected, skipping output";
end

// Is stddato open and valid?
when stddato.is_valid() then
    stddato.write_json(data)?;
else
    stderr << "stddato is broken";
end
```

---

## Performance Optimization

### Avoid Expensive Serialization When Not Needed

```aria
// Only serialize if stddato is actually being used
when stddato.is_redirected() then
    // Expensive: Serialize full object graph
    full_data: JsonValue = serialize_everything(state);
    stddato.write_json(full_data)?;
else
    // Free: stddato is /dev/null, skip work
    stddbg << "Skipping stddato output (not redirected)";
end
```

**Why this matters**:
- Serializing large structures to JSON can be expensive
- If `stddato → /dev/null`, you're wasting CPU cycles
- Use `.is_redirected()` to conditionally serialize

### Buffering

```aria
// Block-buffered by default (good for throughput)
till(large_dataset.length - 1, 1) {
    stddato.write_json(large_dataset[$])?;  // Buffered
}

// Force flush when needed
stddato.flush()?;
```

---

## Error Handling

### Using TBB Results

```aria
Result: Result<(), IoError> = stddato.write_json(data);

match result {
    Ok(()) => {
        stddbg << "Data written successfully";
    }
    Err(err) => {
        stderr << "Failed to write data: " << err;
        return ERR;
    }
}
```

### Sticky Errors

```aria
// With '?' operator, errors propagate
stddato.write_json(data1)?;
stddato.write_json(data2)?;
stddato.write_json(data3)?;

// If any write fails, subsequent writes are skipped
// (TBB sticky error propagation)
```

---

## Best Practices

### ✅ DO: Emit Both Human and Machine Output

```aria
// Correct: Serve both audiences
fn main() {
    Result: Result = process();
    
    // Humans
    stdout << "Processing complete: " << result.count << " items";
    
    // Machines
    stddato.write_json({"count": result.count, "status": "ok"});
}
```

### ✅ DO: Use for Structured Data

```aria
// Metrics, logs, API responses - all to stddato
stddato.write_json({
    "metric": "response_time",
    "value": 127,
    "unit": "ms"
});
```

### ✅ DO: Check Redirection for Expensive Operations

```aria
when stddato.is_redirected() then
    // Only serialize if someone is listening
    huge_data: JsonValue = serialize_full_database();
    stddato.write_json(huge_data)?;
end
```

### ❌ DON'T: Use for Human-Readable Messages

```aria
// WRONG: Progress messages should go to stdout
stddato << "Processing item 5 of 100...";

// RIGHT:
stdout << "Processing item 5 of 100...";
```

### ❌ DON'T: Mix Formats

```aria
// WRONG: Inconsistent output format
stddato.write_json({"id": 1});
stddato << "Some random text";  // Breaks JSON parser!

// RIGHT: Consistent format
stddato.write_json({"id": 1});
stddato.write_json({"id": 2});
```

### ❌ DON'T: Use for Errors

```aria
// WRONG: Errors go to stderr
stddato << "ERROR: File not found";

// RIGHT:
stderr << "ERROR: File not found";
```

---

## Comparison to Other Languages

### Python
```python
# Must choose: human or machine output
if args.json:
    print(json.dumps({"result": data}))
else:
    print(f"Result: {data}")
# Can't have both!
```

### Go
```go
// Must write to stdout (no alternative)
if jsonOutput {
    json.NewEncoder(os.Stdout).Encode(data)
} else {
    fmt.Printf("Result: %v\n", data)
}
```

### Rust
```rust
// Same problem: stdout is the only output
if json_mode {
    serde_json::to_writer(io::stdout(), &data)?;
} else {
    println!("Result: {:?}", data);
}
```

### Aria
```aria
// Both outputs simultaneously
stdout << "Result: " << data;
stddato.write_json({"result": data});
// No conditionals, no conflicts
```

---

## Real-World Example: Database Query Tool

```aria
use std::json;
use std::sql;

fn main() {
    // Connect to database
    db: sql::Connection = sql::connect("postgres://...")?;
    
    stdout << "Connected to database";
    
    // Execute query
    query: string = "SELECT * FROM users WHERE active = true";
    rows: []sql::Row = db.query(query)?;
    
    // Human output (table format)
    stdout << "\nActive Users:";
    stdout << "ID    | Name          | Email";
    stdout << "------|---------------|------------------";
    till(rows.length - 1, 1) {
        stdout << rows[$]["id"] << " | " << rows[$]["name"] << " | " << rows[$]["email"];
    }
    stdout << "\nTotal: " << rows.len() << " users";
    
    // Machine output (JSON array)
    stddato.write_json(rows.map(|row| {
        return {
            "id": row["id"],
            "name": row["name"],
            "email": row["email"]
        };
    }));
    
    // Debug info
    stddbg << "Query executed in " << db.last_query_time() << "ms";
}
```

**Usage**:

Human mode (terminal):
```bash
./db_query
# Active Users:
# ID    | Name          | Email
# ------|---------------|------------------
# 1     | Alice         | alice@example.com
# 2     | Bob           | bob@example.com
# Total: 2 users
```

API mode (capture JSON):
```bash
./db_query 5>users.json
# Human output still shown in terminal
# users.json contains:
# [{"id": 1, "name": "Alice", "email": "alice@example.com"}, ...]
```

Scripting mode (JSON only):
```bash
./db_query 5>&1 1>/dev/null | jq '.[] | .email'
# alice@example.com
# bob@example.com
```

---

## The Philosophy

**Traditional Approach**: "stdout is for output - make it work for both humans and machines"

**Aria's Approach**: "Humans and machines consume output differently - give them separate streams"

This is the same **control plane vs data plane** principle:
- **Control plane** (stdout): Humans, progress messages, results
- **Data plane** (stddato): APIs, ETL, structured data

---

## The "Aha!" Moment

You're writing a CLI tool. Your boss says:

1. "Users need to see progress messages"
2. "The CI system needs JSON output for metrics"

**Traditional languages**: You're screwed. Add `--json` flag, duplicate all output logic, add conditionals everywhere.

**Aria**: You're done.

```aria
// Users see this
stdout << "Processing files...";

// CI sees this
stddato.write_json({"status": "processing"});

// Both happen at the same time
```

**No flags. No conditionals. No compromises.**

---

## Related Topics

- [stddati](stddati.md) - Structured data input
- [Six-Stream Topology](six_stream_topology.md) - Full architecture
- [Data Plane](data_plane.md) - Data plane design
- [stdout](stdout.md) - Comparison with standard output

---

**Remember**: When you have data that another program will consume (JSON, binary, etc.), write it to `stddato`. Your human users will thank you (they still get `stdout`), and your API consumers will thank you (they get clean structured data).
