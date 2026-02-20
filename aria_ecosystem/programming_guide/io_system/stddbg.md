# stddbg (Debug Stream)

**Category**: I/O System → Control Plane  
**File Descriptor**: 6  
**Direction**: Output only  
**Philosophy**: Debugging is not optional instrumentation - it's core infrastructure

---

## What is stddbg?

`stddbg` is Aria's **dedicated debug output stream**. It's a language primitive (like `stdout` or `stderr`) that exists specifically for diagnostic and debug information.

**The Revolutionary Part**: In every other language, debug output either:
1. Pollutes `stdout` (breaking production output)
2. Pollutes `stderr` (mixing with actual errors)
3. Requires a logging library (third-party dependency)
4. Gets deleted before shipping (losing diagnostic capability)

**Aria's Approach**: Debug output has its **own stream** that never interferes with production I/O.

---

## Basic Usage

```aria
// Simple debug output
stddbg << "Entering calculate_balance()";

// Debug with values
stddbg << "Processing account: " << account_id;

// Multiple values
stddbg << "Balance: " << balance << ", Limit: " << credit_limit;

// Expressions
stddbg << "Array length: " << items.len();

// Conditional debugging (rare - usually just leave it in!)
when debug_mode then
    stddbg << "Detailed trace: " << trace_data;
end
```

---

## Why stddbg Exists

### The Problem: Debug Output Competes with Production Output

**Traditional approach** (Python example):
```python
def process_transaction(txn):
    print(f"DEBUG: Processing {txn.id}")  # POLLUTES STDOUT!
    result = apply_transaction(txn)
    print(result)  # Production output mixed with debug
    return result
```

**What happens**:
- Developers add debug prints to find bugs
- Debug output mixes with production output
- **Before shipping**: Delete debug statements
- **In production**: Bug appears, no way to see what's happening
- **Solution**: Add debug statements back, redeploy, hope to reproduce

**Result**: **Debug code is transient**, not permanent.

### Aria's Solution: Separate Stream

```aria
fn process_transaction(txn: Transaction) -> Result {
    stddbg << "Processing transaction: " << txn.id;
    
    Result: Result = apply_transaction(txn);
    
    stdout << result;  // Clean production output
    
    return result;
}
```

**What happens**:
- Debug output goes to `stddbg` (FD 6)
- Production output goes to `stdout` (FD 1)
- **Never interfere with each other**

**Deployment**:
```bash
# Development: See debug output
./program 6>&2  # stddbg to stderr (visible)

# Production: Discard debug output
./program  # stddbg to /dev/null (default)

# Debugging production: Capture to file
./program 6>/var/log/myapp/debug.log
```

**Result**: **Debug code is permanent**. It's always there when you need it.

---

## Stream Properties

| Property | Value |
|----------|-------|
| **File Descriptor** | 6 |
| **Direction** | Output only |
| **Buffering** | Line-buffered (flushes on `\n`) |
| **Default Target** | `/dev/null` (production mode) |
| **Type** | `Stream` (language primitive) |
| **Thread Safety** | Yes (atomic line writes) |

### Buffering Behavior

`stddbg` is **line-buffered**, meaning:
- Output is held in buffer until a newline (`\n`)
- Then flushed atomically to the underlying file descriptor
- This prevents interleaved output from multiple threads

**Force immediate flush**:
```aria
stddbg << "Critical debug point" << flush;
```

---

## Typical Use Cases

### 1. Function Entry/Exit Tracing

```aria
fn calculate_taxes(income: f64, deductions: f64) -> f64 {
    stddbg << "calculate_taxes(" << income << ", " << deductions << ")";
    
    taxable: f64 = income - deductions;
    tax: f64 = taxable * TAX_RATE;
    
    stddbg << "  -> tax = " << tax;
    return tax;
}
```

**In Production**: All `stddbg` output is discarded (no performance impact).  
**When Debugging**: Redirect `stddbg` to see the trace.

### 2. Loop Progress Monitoring

```aria
fn process_batch(items: []Item) -> []Result {
    stddbg << "Processing batch of " << items.len() << " items";
    
    results: []Result = [];
    till(items.len() - 1, 1) {
        when $ % 1000 == 0 then
            stddbg << "  Progress: " << $ << "/" << items.len();
        end
        
        results.push(process_item(items[$]));
    }
    
    stddbg << "Batch complete";
    return results;
}
```

### 3. State Transitions

```aria
fn state_machine(event: Event) -> State {
    old_state: State = current_state;
    
    current_state = match event {
        Event::Start => State::Running,
        Event::Pause => State::Paused,
        Event::Stop  => State::Stopped,
    };
    
    stddbg << "State: " << old_state << " -> " << current_state;
    
    return current_state;
}
```

### 4. Performance Timing

```aria
fn expensive_operation(data: []u8) -> Result {
    start: Time = Time::now();
    stddbg << "Starting expensive_operation, size=" << data.len();
    
    Result: Result = do_work(data);
    
    elapsed: Duration = Time::now() - start;
    stddbg << "Completed in " << elapsed.milliseconds() << "ms";
    
    return result;
}
```

### 5. Error Context (Pre-Panic)

```aria
fn critical_section(id: u32) {
    stddbg << "Entering critical section, id=" << id;
    
    when !validate(id) then
        stddbg << "VALIDATION FAILED for id=" << id;
        panic("Invalid ID");
    end
    
    perform_critical_work(id);
    
    stddbg << "Exiting critical section successfully";
}
```

**Advantage**: If the panic occurs, the last `stddbg` message before the crash is in the log (if redirected).

---

## Redirection Patterns

### Development Mode: See Everything
```bash
./program 6>&2
# stddbg output appears on stderr (visible in terminal)
```

### Production Mode: Silent
```bash
./program
# stddbg → /dev/null (default)
# No performance impact, no output clutter
```

### Production with Logging
```bash
./program 6>/var/log/myapp/debug.log
# stddbg captured to file for post-mortem analysis
```

### Separate Debug File in Dev
```bash
./program 2>errors.log 6>debug.log
# stderr → errors.log
# stddbg → debug.log
# Clear separation
```

### systemd Integration
```ini
[Service]
ExecStart=/usr/bin/myapp
StandardOutput=journal
StandardError=journal
# Map stddbg (fd 6) to journal with DEBUG priority
FileDescriptorStoreMax=1
FileDescriptorName=debug
```

---

## Best Practices

### ✅ DO: Leave Debug Statements In Code

```aria
fn calculate_balance(account: Account) -> Balance {
    stddbg << "calculate_balance: account=" << account.id;
    
    balance: Balance = account.transactions.sum();
    
    stddbg << "  balance=" << balance;
    return balance;
}
```

**Rationale**: These statements cost **nothing in production** (redirected to `/dev/null`) but are **invaluable when debugging**. They serve as documentation of the developer's thought process.

### ✅ DO: Use for Invariant Checking

```aria
fn add_item(list: []Item, item: Item) {
    before_len: usize = list.len();
    
    list.push(item);
    
    after_len: usize = list.len();
    stddbg << "add_item: len " << before_len << " -> " << after_len;
    
    assert(after_len == before_len + 1);  // This should always hold
}
```

### ✅ DO: Trace Complex Logic

```aria
fn resolve_dependency(pkg: Package) -> Maybe<Package> {
    stddbg << "Resolving " << pkg.name << "@" << pkg.version;
    
    match registry.find(pkg) {
        Some(found) => {
            stddbg << "  Found: " << found.name << "@" << found.version;
            return Some(found);
        }
        None => {
            stddbg << "  Not found, trying alternatives";
            return try_alternatives(pkg);
        }
    }
}
```

### ❌ DON'T: Use for User-Facing Messages

```aria
// WRONG: This should go to stdout, not stddbg
stddbg << "Welcome to MyApp!";

// RIGHT:
stdout << "Welcome to MyApp!";
```

**Rationale**: `stddbg` is for **developers**, not users.

### ❌ DON'T: Use for Error Messages

```aria
// WRONG: Errors should go to stderr
stddbg << "ERROR: File not found";

// RIGHT:
stderr << "ERROR: File not found";
```

**Rationale**: `stderr` is for errors, `stddbg` is for diagnostics.

### ❌ DON'T: Wrap in Conditional Checks (Usually)

```aria
// WRONG: Unnecessary - just leave it in!
when DEBUG_MODE then
    stddbg << "Debug info";
end

// RIGHT: Just write it!
stddbg << "Debug info";
```

**Rationale**: The runtime environment controls whether `stddbg` is visible. No need for conditional logic in most cases.

**Exception**: For **expensive** debug output (e.g., serializing large structures), you might use a conditional:

```aria
when debug_enabled then
    stddbg << "Full state: " << serialize_full_state();  // Expensive
end
```

---

## Performance Considerations

### Zero-Cost Abstraction (Almost)

When `stddbg` is redirected to `/dev/null`:
- **No I/O system calls** (write to null device is optimized away by kernel)
- **String formatting still happens** (this is the only cost)

**Estimated overhead**: ~10-50ns per `stddbg` statement (string concatenation + null write).

### When Debug Output is Expensive

If computing the debug message is expensive:

```aria
// Expensive: Always serializes, even if stddbg is /dev/null
stddbg << "State: " << expensive_serialize(state);

// Better: Only compute if debug is enabled
when stddbg.is_redirected() then
    stddbg << "State: " << expensive_serialize(state);
end
```

**Note**: `.is_redirected()` returns `true` if `stddbg` points to something other than `/dev/null`.

---

## Comparison to Logging Libraries

### Traditional Logging (Python, Java, etc.)

```python
import logging

logger = logging.getLogger(__name__)

def process_data(data):
    logger.debug(f"Processing {len(data)} items")  # Third-party library
    result = crunch(data)
    logger.debug(f"Result: {result}")
    return result
```

**Problems**:
1. **Third-party dependency** (not part of language)
2. **Configuration complexity** (log levels, handlers, formatters)
3. **Still pollutes namespace** (must import, configure)
4. **Not always available** (what if logging isn't set up?)

### Aria stddbg

```aria
fn process_data(data: []Item) -> Result {
    stddbg << "Processing " << data.len() << " items";
    Result: Result = crunch(data);
    stddbg << "Result: " << result;
    return result;
}
```

**Advantages**:
1. **Built into language** (no imports, no configuration)
2. **Always available** (guaranteed by runtime)
3. **Simple redirection** (shell-level control)
4. **No levels needed** (either on or off)

---

## The Philosophy Behind stddbg

### Traditional Mindset

"Debug output is temporary scaffolding. Remove it when the building is done."

**Result**: Production code lacks diagnostic capability.

### Aria Mindset

"Debug output is **permanent documentation** of how the code works. Leave it in."

**Result**: Production code is self-documenting and always debuggable.

---

## This is the "Uno Reverse"

Instead of fighting human nature (developers add print statements when debugging), Aria **redirects** that behavior:

1. **Human nature**: "I'll add `print()` to see what's happening"
2. **Traditional problem**: Print pollutes output, must be removed
3. **Aria solution**: Provide a **separate stream** for debug output
4. **Result**: Debug statements stay in code forever

**It's the same principle as**:
- TBB sticky errors (redirect error handling into the type system)
- Compiler-as-inspector (redirect enforcement to compile time)
- Six-stream topology (redirect I/O separation into infrastructure)

> **"Don't fight human nature - redirect it into safe patterns"**

---

## Real-World Example

Here's production code from an Aria HTTP server:

```aria
fn handle_request(req: HttpRequest) -> HttpResponse {
    stddbg << "Request: " << req.method << " " << req.path;
    stddbg << "  Headers: " << req.headers.len();
    
    response: HttpResponse = match req.path {
        "/api/users" => handle_users(req),
        "/api/posts" => handle_posts(req),
        _ => HttpResponse::not_found(),
    };
    
    stddbg << "Response: " << response.status << " (" << response.body.len() << " bytes)";
    
    return response;
}
```

**In Production**:
```bash
# Normal: stddbg → /dev/null (silent)
./http_server

# Debugging: stddbg → journald with DEBUG priority
./http_server 6> >(systemd-cat -t myapp -p debug)
```

**Result**: The same binary works in **both modes** - no recompilation, no code changes.

---

## Related Topics

- [Six-Stream Topology](six_stream_topology.md) - Full I/O architecture
- [I/O Overview](io_overview.md) - Why six streams?
- [Stream Separation](stream_separation.md) - Best practices
- [Control Plane](control_plane.md) - stddbg vs stdout/stderr

---

**Remember**: Every `stddbg` statement you write is **permanent documentation** that will help debug production issues. Don't delete it. Don't gate it behind flags. Just write it, ship it, and redirect it when you need it.

This is Aria's gift to frustrated developers everywhere: **debugging that doesn't disappear**.
