# stdout (Standard Output)

**Category**: I/O System → Control Plane  
**File Descriptor**: 1  
**Direction**: Output only  
**Philosophy**: Human-readable output, separate from structured data

---

## What is stdout?

`stdout` (standard output) is the traditional **output stream** for human-readable messages. It's file descriptor 1, the Unix standard for program output.

**In Aria's six-stream model**: `stdout` is part of the **control plane** - it's for humans, not machines.

---

## Basic Usage

```aria
// Simple output
stdout << "Hello, world!\n";

// Multiple values
stdout << "Result: " << result << "\n";

// Expressions
stdout << "Array has " << items.len() << " elements\n";

// Formatted output
stdout << format("Balance: ${:.2f}", balance);
```

---

## stdout vs stddato

**The Critical Distinction**: `stdout` is for **humans**, `stddato` is for **machines**.

### stdout (Human Output)

```aria
// User-facing messages
stdout << "Processing complete\n";
stdout << "Files processed: " << count << "\n";
```

**Purpose**: Messages that humans read in terminals

### stddato (Machine Output)

```aria
// Structured data
stddato.write_json({"count": count, "status": "ok"});
```

**Purpose**: Data that other programs consume

### The Power: Both at Once!

```aria
// Humans see progress
stdout << "Processing batch...\n";

// Machines get structured results
stddato.write_json({"batch": batch_id, "status": "done"});
```

**No conflicts!** stdout and stddato are completely separate.

---

## Common Patterns

### Progress Messages

```aria
fn process_batch(items: []Item) {
    total: usize = items.len();
    
    for i, item in items.enumerate() {
        // Human progress on stdout
        stdout << "Processing " << (i + 1) << "/" << total << "...\r";
        stdout.flush();  // Update immediately
        
        process_item(item);
    }
    
    stdout << "\n";  // Newline after progress
}
```

### Results Display

```aria
fn display_results(results: []Result) {
    stdout << "\nResults:\n";
    stdout << "========\n";
    
    for result in results {
        stdout << "  " << result.name << ": " << result.value << "\n";
    }
    
    stdout << "\nTotal: " << results.len() << " items\n";
}
```

---

## Best Practices

### ✅ DO: Use for User Messages

```aria
stdout << "Welcome to MyApp!\n";
stdout << "Operation completed successfully.\n";
```

### ✅ DO: Provide Both Human and Machine Output

```aria
// Humans
stdout << "Processed " << count << " records\n";

// Machines
stddato.write_json({"records_processed": count});
```

### ❌ DON'T: Use for Debug Output

```aria
// WRONG: Debug output pollutes stdout
stdout << "DEBUG: Entering function\n";

// RIGHT: Use stddbg
stddbg << "Entering function";
```

### ❌ DON'T: Use for Structured Data

```aria
// WRONG: JSON on stdout breaks human output
stdout << json_dumps(data);

// RIGHT: Use stddato
stddato.write_json(data);
```

---

## Related Topics

- [stddato](stddato.md) - Structured output (vs stdout)
- [stderr](stderr.md) - Error messages
- [stddbg](stddbg.md) - Debug output
- [Six-Stream Topology](six_stream_topology.md)

---

**Remember**: `stdout` is for humans. If you're emitting JSON or binary data, use `stddato`. If you're debugging, use `stddbg`. Keep them separate!
