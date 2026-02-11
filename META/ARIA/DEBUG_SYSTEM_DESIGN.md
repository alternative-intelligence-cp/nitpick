# Aria Debug System Design
**Status**: Planned  
**Priority**: P2 (Developer Experience)  
**Dependencies**: String interpolation/toString library  
**Target**: Pre-1.0 release  
**Date**: February 11, 2026

## Philosophy

Debugging should be:
1. **Type-safe** - Format strings validated at compile-time
2. **Zero-cost** - Disabled groups completely eliminated by optimizer
3. **Organized** - Group-based filtering for complex systems
4. **Safety-integrated** - Assertions hook into the 5-layer safety system
5. **Deterministic** - Reproducible debug output for embedded/real-time systems

## Core API

### `dbug.print(group, format_string, data)`

Simple logging with group-based filtering.

```aria
// Syntax
dbug.print("network", "Connected to {host:string} on port {port:int32}", [host, port]);

// Real-world examples
dbug.print("listeners", "Event received: {type:string} at {time:int64}", [event.type, timestamp]);
dbug.print("memory", "Allocated {bytes:uint64} bytes at {addr:uint64}", [size, address]);
dbug.print("parser", "Token: {token:string} line {line:int32} col {col:int32}", [tok, ln, col]);
```

**Format String Syntax**:
- `{name:type}` - Typed placeholder (compile-time validated)
- Type must match corresponding data array element
- Compiler error if types mismatch or count wrong

**Supported Types**:
- Primitives: `int8`, `int16`, `int32`, `int64`, `uint8`, `uint16`, `uint32`, `uint64`
- Floating: `flt32`, `flt64`
- Special: `bool`, `string`, `char`
- Pointers: Display as hex address `ptr`, `string->` (null-safe)
- Future: Custom types via toString trait

### `dbug.check(group, format_string, data, condition, action?)`

Type-safe assertions with optional custom actions.

```aria
// Basic assertion (triggers failsafe on failure)
dbug.check("memory", 
           "Buffer overflow: {size:int64} exceeds {max:int64}",
           [size, max],
           size <= max);
// On failure: prints message, calls failsafe(ERR_ASSERTION)

// Custom action
dbug.check("network",
           "Invalid port: {port:int32}",
           [port],
           port >= 1024 && port <= 65535,
           || { cleanup_socket(); pass(ERR_INVALID_PORT); });

// Complex conditions
dbug.check("state",
           "Invalid state transition: {from:int32} -> {to:int32}",
           [old_state, new_state],
           is_valid_transition(old_state, new_state),
           || { rollback_state(); });
```

**Behavior**:
- If `condition` evaluates to `true`: No action (assertion passed)
- If `condition` evaluates to `false`:
  1. Print formatted message to stderr
  2. If `action` provided: Execute action lambda
  3. Else: Call `failsafe(ERR_ASSERTION)`

### `dbug.when(condition, group, format_string, data)`

Conditional debug printing (more expressive than wrapping in `if`).

```aria
// Only log when condition is true
dbug.when(verbose_mode, "network", 
          "Headers: {count:int32} bytes", [header_size]);

// Complex conditions
dbug.when(packet.size > 1024, "network",
          "Large packet: {size:int64} from {src:string}",
          [packet.size, packet.source]);

// Multiple conditions
dbug.when(debug_level >= 2 && connection.active, "io",
          "Buffered {count:uint32} bytes for {conn_id:int64}",
          [buffer.count, connection.id]);
```

## Group Management

### Compile-Time Configuration (Production)

```aria
// In build configuration or compile flags
// --dbug-enable=network,memory
// --dbug-disable=verbose,trace

// In code (static initialization)
dbug.enable("network", "memory", "state");
dbug.disable("verbose", "trace", "perf");
```

**Optimizer Behavior**:
- Disabled groups: Entire function call eliminated (zero overhead)
- Enabled groups: Call inlined and optimized
- Format strings: Stored in `.rodata` with de-duplication

### Runtime Configuration (Development)

```aria
// Dynamic enable/disable (development builds only)
dbug.enable_runtime("listeners");
dbug.disable_runtime("verbose");

// Query state
if (dbug.is_enabled("network")) {
    // Expensive debug computation only if needed
    string:details = collect_network_stats();
    dbug.print("network", "Stats: {data:string}", [details]);
}
```

## Error Code Integration

```aria
// Define debug assertion error codes
builtin const ERR_ASSERTION: int32 = 100;
builtin const ERR_INVARIANT: int32 = 101;
builtin const ERR_PRECONDITION: int32 = 102;
builtin const ERR_POSTCONDITION: int32 = 103;

// Usage in assertions
dbug.check("memory", "Null pointer: {name:string}", [var_name], 
           ptr != NULL,
           || { failsafe(ERR_PRECONDITION); });
```

## Advanced Features (Phase 2+)

### Stack Traces on Assertion

```aria
dbug.check("state", 
           "Invalid state: {state:int32}",
           [current_state],
           is_valid_state(current_state));
// On failure:
// [ASSERTION FAILED] state: Invalid state: state=42
// Stack trace:
//   at process_event (event.aria:145)
//   at handle_input (input.aria:67)
//   at main (main.aria:23)
```

### Log Levels

```aria
dbug.debug("network", "Packet details: {info:string}", [info]);
dbug.info("startup", "Server started on port {port:int32}", [port]);
dbug.warn("memory", "High memory usage: {pct:flt32}%", [usage]);
dbug.error("critical", "Failed to connect: {reason:string}", [err]);

// Configure minimum level
dbug.set_level(DBUG_WARN);  // Only warn and error
```

### Output Redirection

```aria
// Default: stdout for print, stderr for check failures
dbug.set_output_stream(DBUG_STDOUT);
dbug.set_error_stream(DBUG_STDERR);

// File output (future)
dbug.set_output_file("debug.log");

// Hex stream I/O integration
dbug.set_output_hex_stream(debug_stream);
```

### Timing/Profiling

```aria
// Measure execution time
dbug.time_start("rendering");
render_frame();
dbug.time_end("rendering");
// Prints: [TIMING] rendering: 16.7ms

// Scoped timing
{
    dbug.scope_timer("database_query");
    result = query_database(sql);
} // Automatically prints timing on scope exit
```

## Implementation Architecture

### Compile-Time Components

1. **Lexer/Parser**: Recognize `dbug.` built-in module
2. **Semantic Analysis**:
   - Validate format string against data array types
   - Check group name is string literal (compile-time constant)
   - Verify lambda signature for custom actions
3. **Optimizer**:
   - Dead code elimination for disabled groups
   - Format string de-duplication
   - Inline enabled debug calls

### Runtime Components

1. **Group Registry**:
   - Hash map of group names → enabled state
   - Lock-free reads in production (const after init)
2. **Format Engine**:
   - Parse format strings (cached at compile-time)
   - Type-safe value substitution
   - Depends on toString library
3. **Output Sink**:
   - Buffered writes to configured streams
   - Thread-safe for concurrent debugging
   - Hex stream integration for embedded systems

### LLVM IR Generation

```llvm
; Disabled group (optimized away completely)
; dbug.print("disabled_group", "Message", []);
; → <no code generated>

; Enabled group
; dbug.print("network", "Port {port:int32}", [8080]);
define void @dbug_print_network() {
entry:
  %port = i32 8080
  %fmt = global constant [21 x i8] c"Port {port:int32}\00"
  call void @aria_dbug_print(i8* @.str.group.network, i8* %fmt, i32 %port)
  ret void
}
```

## Type System Integration

### Format String Validation

```aria
// Compile-time type checking
int32:port = 8080;
string:host = "localhost";

dbug.print("net", "Port {port:int32} Host {host:string}", [port, host]); // ✓

dbug.print("net", "Port {port:int64}", [port]); // ✗ Type mismatch: int32 vs int64
dbug.print("net", "Port {port:int32}", []);      // ✗ Not enough arguments
dbug.print("net", "Port {port:int32}", [port, host]); // ✗ Too many arguments
```

### Custom Type Support (via toString)

```aria
// User-defined type with toString
type:Point = struct {
    int32:x;
    int32:y;
    
    func:toString = string(Point->:self) {
        return "({self->x}, {self->y})";
    };
};

Point:p = {x: 10, y: 20};
dbug.print("geom", "Position: {pos:Point}", [p]);
// Output: Position: (10, 20)
```

## Integration with Safety System

```aria
// Layer 2: Emphatic unwrap with debug assertion
Result<User>:user_result = get_user(id);
User:user = user_result?! || {
    dbug.check("auth", "Failed to get user {id:int64}", [id], false);
    failsafe(ERR_USER_NOT_FOUND);
};

// Layer 3: Unknown detection with debug
if (!ok(value)) {
    dbug.print("state", "Encountered unknown value: {val:int32}", [value]);
    // Handle unknown state
}

// Layer 5: Failsafe with debug context
func:failsafe = void(int32:err_code) {
    dbug.error("critical", "Failsafe triggered: code {code:int32}", [err_code]);
    // Log to hex stream, cleanup, terminate safely
};
```

## Configuration Examples

### Development Build

```bash
# Enable verbose debugging
ariac --build=debug --dbug-enable=all my_app.aria

# Enable specific groups
ariac --build=debug --dbug-enable=network,memory,state my_app.aria
```

### Production Build

```bash
# Disable all debug output (zero overhead)
ariac --build=release --dbug-disable=all my_app.aria

# Keep critical assertions only
ariac --build=release --dbug-enable=assert,critical my_app.aria
```

### Embedded/Real-Time

```bash
# Hex stream output, timing disabled
ariac --target=embedded --dbug-output=hex --dbug-disable=timing my_app.aria
```

## Performance Characteristics

- **Disabled groups**: Zero runtime cost (no instructions generated)
- **Enabled groups**: ~10-50 cycles per call (mostly formatting)
- **Format strings**: Constant data (.rodata), zero heap allocation
- **Group check**: Single branch prediction (hot path: group enabled)
- **Memory overhead**: ~8 bytes per unique group (hash map entry)

## Comparison with Other Languages

| Feature | Aria `dbug` | Rust `dbg!()` | Zig `std.log` | C `assert()` |
|---------|-------------|---------------|---------------|--------------|
| Type-safe format | ✓ | ✗ (uses Debug trait) | ✓ | ✗ |
| Group filtering | ✓ | ✗ | ✓ (log levels) | ✗ |
| Zero-cost disabled | ✓ | ✓ | ✓ | ✓ (NDEBUG) |
| Custom actions | ✓ | ✗ | ✗ | ✗ |
| Safety integration | ✓ | Partial | ✗ | ✗ (aborts) |

## Implementation Phases

### Phase 1: Core Debug (Post-toString)
- [ ] `dbug.print()` basic implementation
- [ ] Format string parser and validator
- [ ] Compile-time group enable/disable
- [ ] Type checking for format strings
- [ ] Basic optimizer support (dead code elimination)

**Estimated effort**: 2-3 weeks  
**Depends on**: String interpolation, toString library

### Phase 2: Assertions (Post-Safety System)
- [ ] `dbug.check()` implementation
- [ ] Integration with `failsafe()`
- [ ] Custom action lambda support
- [ ] Error code definitions
- [ ] Stack trace capture on assertion failure

**Estimated effort**: 1-2 weeks  
**Depends on**: Phase 1, failsafe complete

### Phase 3: Advanced Features
- [ ] `dbug.when()` conditional printing
- [ ] Runtime group enable/disable
- [ ] Log levels (debug/info/warn/error)
- [ ] Output stream redirection
- [ ] Hex stream I/O integration
- [ ] Timing/profiling utilities

**Estimated effort**: 2 weeks  
**Depends on**: Phase 2, hex stream I/O

### Phase 4: Polish & Optimization
- [ ] LLVM optimization passes for debug calls
- [ ] Format string de-duplication
- [ ] Custom type toString integration
- [ ] Documentation and examples
- [ ] Performance benchmarking

**Estimated effort**: 1 week  
**Depends on**: Phase 3

## Open Questions

1. **Syntax bikeshedding**: Should we use `dbug` or `debug`? (Prefer `dbug` - shorter, unique)
2. **Group names**: String literals only, or allow const string variables?
3. **Thread safety**: Lock-free reads? Mutex for runtime enable/disable?
4. **Embedded constraints**: Minimum memory footprint? ROM vs RAM trade-offs?
5. **Format caching**: Pre-parse format strings at compile-time or runtime?

## Future Extensions

- **Conditional compilation**: `#[cfg(debug_assertions)]` style attributes
- **Debug macros**: Pre-processor style debug utilities
- **Interactive debugging**: GDB/LLDB integration for Aria types
- **Visual debugging**: Integration with IDE debug visualizers
- **Remote debugging**: Network protocol for embedded device debugging

## Related Work

See also:
- `META/ARIA/TYPE_SYSTEM_DESIGN.md` - toString trait
- `META/ARIA/SAFETY_REVIEW_TRIAGE.md` - failsafe integration
- `META/ARIA/THE_ARIA_PHILOSOPHY.md` - Zero-cost abstractions
- Future: String interpolation/formatting design doc

---

**Document Status**: Draft  
**Next Review**: After toString library implementation  
**Owner**: Randy (with Copilot assistance)
