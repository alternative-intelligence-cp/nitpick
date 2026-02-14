# Debug Facility (`dbug`)

## Overview

The `dbug` facility provides built-in debugging and assertion capabilities with group-based filtering. Unlike traditional approaches that require littering code with temporary print statements that must be manually removed or preprocessor macros that complicate the build, Aria's `dbug` system integrates debugging into the language itself.

**Core Philosophy:**
- Debug instrumentation is legitimate permanent code, not temporary scaffolding
- Enable/disable debugging by group at compile time or runtime
- No need to strip debug code before release (it compiles to nothing when disabled)
- Simple, consistent interface across all code
- Integrates with Aria's hex stream I/O for clean separation

**Key Features:**
- **Grouped debug output**: Organize debug statements by logical groups
- **Conditional printing**: `dbug.print()` for simple output
- **Conditional assertions**: `dbug.check()` for validation with optional actions
- **Compile-time filtering**: Dead code elimination when groups disabled
- **Runtime filtering**: Dynamic enable/disable for interactive debugging
- **Type-safe formatting**: Integrates with Aria's string interpolation
- **Zero runtime cost when disabled**: Compiler eliminates disabled debug code

## Basic Interface

```aria
// Debug printing
func:dbug.print = void(
    str:group,           // Debug group name
    str:msg_fmt,         // Format string with interpolation
    any[]:data           // Data to interpolate into format
) {};

// Debug checking (assertion-style)
func:dbug.check = void(
    str:group,           // Debug group name
    str:msg_fmt,         // Format string with interpolation
    any[]:data,          // Data to interpolate into format
    bool:condition,      // Condition to check
    func:action?         // Optional action if condition fails
) {};

// Enable/disable groups
func:dbug.enable = void(str:group) {};
func:dbug.disable = void(str:group) {};

// Check if group is enabled
func:dbug.enabled = bool(str:group) {};
```

## Simple Examples

### Basic Debug Printing

```aria
func:processRequest = void(i32:requestId, str:endpoint) {
    dbug.print("network", "Processing request {i32} to {str}", [requestId, endpoint]);
    
    // ... actual processing logic ...
    
    dbug.print("network", "Request {i32} completed", [requestId]);
}
```

When the "network" group is enabled, output appears on the debug stream. When disabled, the compiler eliminates the calls entirely—zero runtime cost.

### Conditional Assertions

```aria
func:divideNumbers = f64(f64:numerator, f64:denominator) {
    dbug.check(
        "math",
        "Division by zero: {f64} / {f64}",
        [numerator, denominator],
        denominator != 0.0
    );
    
    return numerator / denominator;
}
```

The message only prints if the condition fails (denominator == 0.0). In debug builds with "math" enabled, you get the diagnostic. In release or with group disabled, the check disappears.

### Assertion with Action

```aria
func:allocateBuffer = u8[]?(usize:size) {
    dbug.check(
        "memory",
        "Excessive allocation requested: {usize} bytes",
        [size],
        size <= MAX_REASONABLE_ALLOCATION,
        func:action = void() {
            // Custom diagnostic action
            dbug.print("memory", "Stack trace:", []);
            printStackTrace();
        }
    );
    
    return malloc(size);
}
```

When the condition fails, both the message prints AND the custom action executes.

## String Interpolation Syntax

Aria uses a clear, type-explicit interpolation syntax in format strings:

**Format**: `{type}` where type is the Aria type name

```aria
i32:count = 42;
str:name = "items";
f64:ratio = 3.14159;

dbug.print("app", "Processed {i32} {str} with ratio {f64}", [count, name, ratio]);
// Output: Processed 42 items with ratio 3.14159
```

**Advantages over implicit syntax:**
- Type safety: Compiler verifies format string matches data types
- No runtime format parsing errors
- Clear intent: Reader sees expected types directly
- Supports all Aria types including custom ones
- No magic: Explicit is better than implicit

**Supported type specifiers:**
```aria
{i8} {i16} {i32} {i64}        // Signed integers
{u8} {u16} {u32} {u64}        // Unsigned integers
{f32} {f64}                    // Floating point
{bool}                         // Boolean (true/false)
{char}                         // Single character
{str}                          // String
{Q3<T>} {Q9<T>}               // Quantum types
{ptr<T>}                       // Pointers
{CustomType}                   // Any type implementing ToString
```

### Custom Format Options

Types can provide formatting hints:

```aria
{f64:2}      // 2 decimal places
{i32:hex}    // Hexadecimal output
{u64:bin}    // Binary output
{f64:sci}    // Scientific notation
{i64:size}   // Byte size formatting (1024 -> "1 KiB")
```

Example:
```aria
u64:bytes = 1048576;
dbug.print("perf", "Allocated {u64:size} memory", [bytes]);
// Output: Allocated 1 MiB memory
```

## Debug Groups

Groups organize debugging by logical concern, allowing fine-grained control without code modification.

### Common Group Patterns

**By Subsystem:**
```aria
dbug.print("network", ...);    // Network operations
dbug.print("database", ...);   // Database queries
dbug.print("cache", ...);      // Cache hit/miss
dbug.print("auth", ...);       // Authentication
```

**By Concern:**
```aria
dbug.print("performance", ...); // Timing information
dbug.print("memory", ...);      // Allocations
dbug.print("errors", ...);      // Error conditions
dbug.print("state", ...);       // State transitions
```

**By Component:**
```aria
dbug.print("parser", ...);      // Parser internals
dbug.print("compiler", ...);    // Compilation stages
dbug.print("runtime", ...);     // Runtime behavior
dbug.print("gc", ...);          // Garbage collection
```

### Hierarchical Groups

Use dot notation for hierarchical organization:

```aria
dbug.print("http.request", "Incoming {str} request", [method]);
dbug.print("http.response", "Status {i32}", [statusCode]);
dbug.print("http.parser.headers", "Parsed {i32} headers", [count]);
```

Enabling "http" enables all subgroups. Enabling "http.parser" enables only parser-related debug output.

### Compile-Time Group Configuration

In build configuration:
```toml
[debug]
enabled_groups = ["network", "database"]
disabled_groups = ["auth", "cache"]

# Or enable all except listed
default = "all"
disabled_groups = ["performance"]
```

Disabled groups compile to nothing—no runtime checks, no string constants, no data preparation.

### Runtime Group Control

```aria
func:main = i32() {
    // Enable debugging for specific groups
    dbug.enable("network");
    dbug.enable("database");
    
    if arguments.contains("--debug-cache") {
        dbug.enable("cache");
    }
    
    // ... run application ...
    
    // Disable for performance-critical section
    dbug.disable("network");
    processBulkData();
    dbug.enable("network");
    
    return 0;
}
```

Runtime control only works for groups compiled-in. If a group was compile-time disabled, runtime enable has no effect (the code doesn't exist).

## Integration with Hex Stream I/O

Aria separates concerns via distinct output streams:

- **Stream 0**: Standard output (normal program output)
- **Stream 1**: Standard error (error messages)
- **Stream 2**: Debug output (`dbug` facility)

This separation prevents debug output from polluting program output:

```bash
# Run program, ignore debug output
./myprogram 2>/dev/null

# Capture debug output only
./myprogram 2>&1 1>/dev/null

# Save debug to file while seeing normal output
./myprogram 2>debug.log

# Debug + errors to one file, output to another
./myprogram >output.txt 2>diagnostics.txt
```

Inside Aria:
```aria
// Goes to stream 0 (stdout)
print("User-visible result: {i32}\n", [result]);

// Goes to stream 1 (stderr)
error("Failed to connect: {str}\n", [reason]);

// Goes to stream 2 (debug stream)
dbug.print("network", "Retry attempt {i32}", [attempt]);
```

Shell redirection:
- `>` redirects stream 0 (stdout)
- `2>` redirects stream 1 (stderr)  
- `3>` redirects stream 2 (debug)

**Note**: On systems without stream 2 support, debug output falls back to stderr with a "[DEBUG]" prefix.

## Advanced Patterns

### Temporary Debug Scopes

```aria
func:complexCalculation = f64(f64[]:data) {
    bool:wasEnabled = dbug.enabled("math");
    dbug.enable("math");
    
    defer dbug.disable("math") if !wasEnabled;  // Restore state on exit
    
    dbug.print("math", "Starting calculation with {i32} points", [data.len]);
    
    // Deep calculation with lots of debug instrumentation
    f64:result = 0.0;
    for i32:i in 0..data.len {
        dbug.print("math", "Processing element {i32}: {f64}", [i, data[i]]);
        result += process(data[i]);
    }
    
    return result;
}
```

### Conditional Compilation

```aria
#if DEBUG_ENABLED
func:debugDumpState = void(State:state) {
    dbug.print("state", "Current state: {State}", [state]);
    for field in state.fields {
        dbug.print("state", "  {str} = {any}", [field.name, field.value]);
    }
}
#else
func:debugDumpState = void(State:state) {
    // Empty in release builds
}
#endif
```

### Performance Measurement

```aria
func:measureBlock = void(str:group, str:label, func:block) {
    if !dbug.enabled(group) {
        block();
        return;
    }
    
    u64:start = timeNanos();
    block();
    u64:elapsed = timeNanos() - start;
    
    dbug.print(group, "{str} took {u64:size}ns", [label, elapsed]);
}

// Usage:
measureBlock("performance", "Database query") {
    results = db.query("SELECT * FROM users WHERE active = true");
};
```

### Structured Logging Alternative

dbug can replace structured logging for internal debugging:

```aria
struct:LogEntry {
    str:level,
    str:component,
    str:message,
    any[]:data
}

func:logDebug = void(str:component, str:message, any[]:data) {
    dbug.print(component, "[DEBUG] {str}: {str}", [component, message]);
    for i32:i in 0..data.len {
        dbug.print(component, "  arg{i32} = {any}", [i, data[i]]);
    }
}
```

### State Machine Debugging

```aria
enum:State {
    Idle,
    Connecting,
    Connected,
    Disconnecting
}

struct:Connection {
    State:current,
    
    func:transition = void(State:next) {
        dbug.check(
            "statemachine",
            "Invalid transition: {State} -> {State}",
            [this.current, next],
            isValidTransition(this.current, next)
        );
        
        dbug.print("statemachine", "Transition: {State} -> {State}", 
                   [this.current, next]);
        
        this.current = next;
    }
}
```

## Common Use Cases

### Network Debugging

```aria
func:handleRequest = Result<Response, Error>(Request:req) {
    dbug.print("http.request", 
               "Received {str} {str} from {str}",
               [req.method, req.path, req.remoteAddr]);
    
    dbug.check("http.validation",
               "Invalid path: {str}",
               [req.path],
               req.path.startsWith("/"));
    
    Response:resp = processRequest(req)?;
    
    dbug.print("http.response",
               "Sending {i32} with {i32} bytes",
               [resp.statusCode, resp.body.len]);
    
    return Response.ok(resp);
}
```

### Memory Allocation Tracking

```aria
struct:Allocator {
    i64:allocatedBytes,
    i64:allocationCount,
    
    func:alloc = ptr<u8>(usize:size) {
        dbug.check("memory.limits",
                   "Allocation size exceeds limit: {usize}",
                   [size],
                   size <= MAX_ALLOCATION);
        
        ptr<u8>:memory = systemAlloc(size);
        
        dbug.check("memory.oom",
                   "Out of memory: requested {usize} bytes",
                   [size],
                   memory != null);
        
        this.allocatedBytes += size;
        this.allocationCount += 1;
        
        dbug.print("memory.alloc",
                   "Allocated {usize} bytes at {ptr} (total: {i64})",
                   [size, memory, this.allocatedBytes]);
        
        return memory;
    }
}
```

### Parser Development

```aria
func:parseExpression = Result<Expr, ParseError>(Tokenizer:tokens) {
    dbug.print("parser.expr", "Parsing expression, current token: {Token}", 
               [tokens.current()]);
    
    Expr:left = parsePrimary(tokens)?;
    dbug.print("parser.expr", "Got primary: {Expr}", [left]);
    
    while tokens.peek().isBinaryOp() {
        Token:op = tokens.consume();
        dbug.print("parser.expr", "Binary op: {Token}", [op]);
        
        Expr:right = parsePrimary(tokens)?;
        left = Expr.binary(op, left, right);
        
        dbug.print("parser.expr", "Built binary expr: {Expr}", [left]);
    }
    
    return Result.ok(left);
}
```

### Concurrent Access Debugging

```aria
struct:SharedResource {
    i32:value,
    Mutex:lock,
    i32:readers,
    
    func:read = i32() {
        dbug.print("concurrency.lock", "Attempting read lock", []);
        this.lock.acquire();
        
        this.readers += 1;
        dbug.check("concurrency.readers",
                   "Too many readers: {i32}",
                   [this.readers],
                   this.readers <= MAX_READERS);
        
        dbug.print("concurrency.lock", 
                   "Read locked (readers: {i32})",
                   [this.readers]);
        
        i32:val = this.value;
        
        this.readers -= 1;
        this.lock.release();
        dbug.print("concurrency.lock", "Read unlocked", []);
        
        return val;
    }
}
```

## Best Practices

### 1. Permanent Instrumentation

**Good**: Leave debug statements in code
```aria
func:processItem = void(Item:item) {
    dbug.print("processing", "Starting item {i32}", [item.id]);
    
    // process logic...
    
    dbug.print("processing", "Completed item {i32}", [item.id]);
}
```

**Bad**: Remove debug statements before commit
```aria
func:processItem = void(Item:item) {
    // TODO: removed debug prints, add back if bugs occur
    // process logic...
}
```

**Rationale**: When disabled, debug statements cost nothing. Keep them as documentation and future debugging aids.

### 2. Meaningful Group Names

**Good**: Descriptive, hierarchical groups
```aria
dbug.print("database.query.optimization", ...);
dbug.print("cache.redis.connection", ...);
```

**Bad**: Generic or ambiguous groups
```aria
dbug.print("debug", ...);
dbug.print("temp", ...);
```

**Rationale**: Good names make selective enabling useful. "debug" is not selective at all.

### 3. Type-Safe Formatting

**Good**: Match format placeholders to data types
```aria
i32:count = 42;
dbug.print("app", "Count: {i32}", [count]);
```

**Bad**: Mismatched types (will not compile)
```aria
i32:count = 42;
dbug.print("app", "Count: {str}", [count]);  // Compile error
```

**Rationale**: Compiler catches format errors at build time, not runtime.

### 4. Assertions for Invariants

**Good**: Use dbug.check for internal invariants
```aria
func:processArray = void(i32[]:arr) {
    dbug.check("validation",
               "Array length must be positive: {i32}",
               [arr.len],
               arr.len > 0);
    
    // ... process array knowing len > 0 ...
}
```

**Bad**: Silent assumptions
```aria
func:processArray = void(i32[]:arr) {
    // Assumes arr.len > 0, but doesn't verify
    i32:first = arr[0];  // Could crash
}
```

**Rationale**: Assertions catch bugs early in development without impacting release performance.

### 5. Separate Concerns from Errors

**Good**: Use dbug for debugging, proper errors for failures
```aria
func:connect = Result<Connection, Error>(str:host) {
    dbug.print("network.connect", "Attempting connection to {str}", [host]);
    
    Connection:conn = tryConnect(host);
    if conn.isErr() {
        error("Failed to connect to {str}: {str}\n", 
              [host, conn.error().message]);
        return Result.err(conn.error());
    }
    
    dbug.print("network.connect", "Connected successfully", []);
    return Result.ok(conn);
}
```

**Bad**: Using debug output as error reporting
```aria
func:connect = Result<Connection, Error>(str:host) {
    Connection:conn = tryConnect(host);
    if conn.isErr() {
        dbug.print("network", "Connection failed: {str}", 
                   [conn.error().message]);  // Wrong stream for errors
        return Result.err(conn.error());
    }
    return Result.ok(conn);
}
```

**Rationale**: Debug output disappears in production. Errors must always be reportable.

### 6. Performance-Critical Code

**Good**: Minimal debug in hot paths, use guards
```aria
func:processMillions = void(i32[]:data) {
    bool:debugging = dbug.enabled("hotpath");
    
    for i32:i in 0..data.len {
        if debugging && i % 10000 == 0 {
            dbug.print("hotpath", "Progress: {i32}/{i32}", [i, data.len]);
        }
        
        processElement(data[i]);
    }
}
```

**Bad**: Debug statement per iteration
```aria
func:processMillions = void(i32[]:data) {
    for i32:i in 0..data.len {
        dbug.print("hotpath", "Processing {i32}", [i]);  // Millions of prints
        processElement(data[i]);
    }
}
```

**Rationale**: Even checking if a group is enabled has cost. Check once outside loop.

### 7. Complex Data Structures

**Good**: Implement pretty-printing for custom types
```aria
struct:Request {
    str:method,
    str:path,
    Map<str,str>:headers,
    
    func:toString = str() {
        return "{method} {path} ({headers.len} headers)"
            .format([this.method, this.path, this.headers.len]);
    }
}

// Now works automatically:
dbug.print("http", "Request: {Request}", [req]);
```

**Bad**: Manual reconstruction in every debug call
```aria
dbug.print("http", 
           "Request: method={str}, path={str}, headers={i32}",
           [req.method, req.path, req.headers.len]);
```

**Rationale**: Define formatting once, use everywhere. Reduces repetition and errors.

## Integration with Testing

The dbug facility integrates naturally with Aria's testing framework:

```aria
test "cache hit updates stats" {
    // Enable specific debugging for this test
    dbug.enable("cache");
    dbug.enable("cache.stats");
    
    Cache:cache = Cache.new();
    cache.put("key", "value");
    
    // First access - should be cache hit
    str?:result = cache.get("key");
    
    dbug.check("test.assertion",
               "Cache should have returned value",
               [],
               result.isSome());
    
    // Verify internal state
    dbug.print("test.verification",
               "Cache stats: hits={i32}, misses={i32}",
               [cache.hits, cache.misses]);
    
    assert cache.hits == 1;
    assert cache.misses == 0;
}
```

Test runs can capture debug stream separately:
```bash
# Run tests with debug output to file
aria test --debug-output=test-debug.log

# Run specific test with debug enabled
aria test "cache hit" --enable-debug=cache
```

## Comparison with Alternatives

### vs. printf Debugging

**printf approach:**
```c
void processData(int* data, int len) {
    printf("DEBUG: Processing %d items\n", len);  // Forgot to remove
    for (int i = 0; i < len; i++) {
        printf("DEBUG: Item %d = %d\n", i, data[i]);  // Forgot to remove
        process(data[i]);
    }
    printf("DEBUG: Done\n");  // Forgot to remove
}
```

Problems:
- Must manually remove before release
- Pollutes stdout
- No way to selectively enable
- Easy to forget to remove some

**dbug approach:**
```aria
func:processData = void(i32[]:data) {
    dbug.print("processing", "Processing {i32} items", [data.len]);
    for i32:i in 0..data.len {
        dbug.print("processing", "Item {i32} = {i32}", [i, data[i]]);
        process(data[i]);
    }
    dbug.print("processing", "Done", []);
}
```

Advantages:
- Keep in code permanently
- Separate debug stream
- Enable/disable by group
- Zero cost when disabled

### vs. Assertions

**assert approach:**
```c
void divide(double a, double b) {
    assert(b != 0);  // Crude, no message
    return a / b;
}
```

**dbug.check approach:**
```aria
func:divide = f64(f64:a, f64:b) {
    dbug.check("math",
               "Division by zero: {f64} / {f64}",
               [a, b],
               b != 0.0);
    return a / b;
}
```

Advantages:
- Detailed context in message
- Group-based control (verify all math, or specific subsystems)
- Optional custom action
- Type-safe formatting

### vs. Logging Frameworks

**Traditional logging:**
```java
logger.debug("Processing request: id=" + requestId + ", path=" + path);
if (logger.isDebugEnabled("network")) {
    logger.debug("Headers: " + headers.toString());
}
```

Problems:
- Runtime enabled/disabled checks (cost even when disabled)
- String concatenation happens even if logging disabled
- Requires external configuration
- No compile-time elimination

**dbug approach:**
```aria
dbug.print("network", "Processing request: id={i32}, path={str}", 
           [requestId, path]);
dbug.print("network", "Headers: {Headers}", [headers]);
```

Advantages:
- Compile-time elimination when disabled
- No string operations if disabled
- Built into language
- Type-safe formatting

### vs. Preprocessor Macros

**C++ preprocessor approach:**
```cpp
#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

DEBUG_PRINT("Value: %d\n", value);
```

Problems:
- All or nothing (can't selectively enable)
- Macro hygiene issues
- No type safety
- Complicates build

**dbug approach:**
```aria
dbug.print("values", "Value: {i32}", [value]);
```

Advantages:
- Granular group control
- Type-safe
- Part of language, not build system
- Can be runtime controlled

## Error Handling Integration

The `dbug` facility integrates with Aria's error handling (`ok()`, `Result<T, E>`):

```aria
func:parseConfig = Result<Config, ParseError>(str:filename) {
    dbug.print("config.parse", "Loading config from {str}", [filename]);
    
    str:content = readFile(filename).ok() catch |err| {
        dbug.print("config.error", "Failed to read {str}: {Error}", 
                   [filename, err]);
        return Result.err(ParseError.fileNotFound(filename));
    };
    
    dbug.check("config.validation",
               "Config file empty: {str}",
               [filename],
               content.len > 0);
    
    Config:config = parse(content)?;
    
    dbug.print("config.parse", "Successfully parsed config", []);
    return Result.ok(config);
}
```

Debug output traces success and failure paths without impacting error handling logic.

## Implementation Notes

### Compiler Behavior

When a debug group is compile-time disabled, the compiler:

1. **Eliminates dead code**: Debug statements removed entirely
2. **Removes string constants**: Format strings not included in binary
3. **Optimizes data preparation**: No array construction for data parameter
4. **Inlines empty stubs**: No function call overhead

Example:
```aria
// Source:
dbug.print("disabled_group", "Value: {i32}", [expensiveComputation()]);

// Compiles to: (nothing)
// expensiveComputation() is not called
```

### Runtime Behavior

When a group is compile-time enabled but runtime disabled:

```aria
// Fast path - single boolean check
if (debugGroupEnabled["network"]) {
    formatAndPrint("network", format, data);
}
```

The check is a simple boolean array lookup, typically 1-2 CPU instructions.

### Memory Overhead

- **Per group**: 1 bit (enabled/disabled state)
- **Format strings**: Only for enabled groups
- **Debug stream buffer**: Shared across all groups (typically 64KB)

For a program with 100 debug groups, overhead is ~13 bytes plus format strings.

## Security Considerations

### Debug Information Leakage

Debug output may contain sensitive information:

```aria
// Potentially sensitive
dbug.print("auth", "Login attempt: user={str}, password={str}", 
           [username, password]);  // Don't log passwords!
```

**Best practices:**
- Never log passwords, tokens, or secrets
- Sanitize user input before debug printing
- Disable all debug groups in production builds
- Use separate debug builds for development

### Resource Exhaustion

Excessive debug output can consume resources:

```aria
// Dangerous in production
for i32:i in 0..1000000 {
    dbug.print("loop", "Iteration {i32}", [i]);  // Million debug prints
}
```

**Mitigation:**
- Compile with debug groups disabled for production
- Rate-limit debug output in hot paths
- Use conditional printing (every Nth iteration)

## Platform Differences

### Unix/Linux

Debug stream (stream 2) routes to file descriptor 3:
```bash
./program 3>debug.log
```

### Windows

Debug stream routes to `OutputDebugString()` API, visible in debuggers:
```
DebugView.exe  # See debug output
```

### Embedded Systems

Debug stream can route to:
- UART/serial port
- Memory buffer (for later extraction)
- Disabled entirely (no overhead)

Configuration via build flags:
```toml
[target.embedded]
debug_output = "uart0"
debug_buffer_size = 4096
```

## Future Enhancements

Potential future additions:

**Structured output:**
```aria
dbug.json("api", {
    "event": "request",
    "method": req.method,
    "path": req.path,
    "duration_ms": elapsed.ms()
});
```

**Conditional breakpoints:**
```aria
dbug.break("parser")  // Debugger break if group enabled
    .when(tokens.len == 0);
```

**Debug statistics:**
```aria
dbug.count("cache.hits");  // Increment counter
dbug.histogram("request.duration", duration);
dbug.gauge("active.connections", connectionCount);
```

**Trace visualization:**
```aria
dbug.span("database.query") {
    // Timed span for trace visualization
    results = db.execute(query);
}
```

## Summary

The `dbug` facility provides:

**For Development:**
- Quick debugging without temporary print statements
- Detailed diagnostics for complex issues
- Type-safe assertion checking
- Performance measurement

**For Production:**
- Zero runtime cost when disabled
- No binary bloat (code eliminated)
- Clean separation via hex stream I/O
- Optional runtime enabling for field diagnostics

**Key Principles:**
- **Simple**: Two functions cover most needs
- **Permanent**: Debug code stays in codebase
- **Selective**: Enable exactly what you need
- **Type-safe**: Compiler catches format errors
- **Zero-cost**: Disabled groups compile to nothing

The `dbug` facility embodies Aria's philosophy: Stop making this complicated. Debugging is legitimate permanent code. Make it first-class.
