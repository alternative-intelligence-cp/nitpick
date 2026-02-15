# Debug System (dbug)

**Built-in group-based debugging with assertions, conditional logging, and runtime control**

---

## Overview

Aria provides a **built-in debug system** (`dbug`) that combines:
- **Grouped logging** - Tag debug output for selective filtering
- **Conditional checks** - Assert-like functionality with custom actions
- **Format interpolation** - String formatting with type-safe substitution
- **Runtime control** - Enable/disable groups without recompilation

**Philosophy**: Debug instrumentation should be **first-class** in the language, not an afterthought. Safety-critical systems need robust debugging that stays in production code.

```aria
// Enable specific debug group
dbug.enable('sensors');

// Print debug message (only if group enabled)
dbug.print('sensors', "Temperature: {{fix256}}, Pressure: {{fix256}}", 
    [temp_reading, pressure_reading]);

// Conditional check with custom action
dbug.check('sensors', "Sensor out of range: {{fix256}}", [temp_reading],
    temp_reading < -40.0fx || temp_reading > 125.0fx,
    { calibrate_sensor(); });
```

---

## Core Functions

### dbug.print() - Grouped Logging

**Syntax**: `dbug.print(group, format_string, data_array)`

**Purpose**: Print debug messages tagged with a group label

**Parameters**:
- `group: string` - Debug group tag (e.g., "networking", "memory", "sensors")
- `format_string: string` - Format template with `{{type}}` placeholders
- `data_array: [dyn]` - Array of values to interpolate into format string

**Behavior**:
- Only prints if `group` is enabled via `dbug.enable(group)`
- Outputs to **hex stream** (separate from stdout/stderr)
- Format string uses `{{type}}` placeholders (e.g., `{{int32}}`, `{{fix256}}`)

**Examples**:

```aria
// Basic logging
dbug.print('network', "Packet received: {{uint32}} bytes from {{string}}", 
    [packet_size, source_ip]);

// Multiple values
dbug.print('memory', "Allocated {{uint64}} bytes at {{ptr}}, total: {{uint64}}", 
    [alloc_size, ptr_address, total_allocated]);

// Complex types
dbug.print('physics', "Position: {{vec3}}, velocity: {{vec3}}, t={{fix256}}", 
    [position, velocity, timestamp]);
```

**Group organization**:
```aria
// Hierarchical groups (by convention, use ':' separator)
dbug.enable('network:tcp');       // TCP-specific
dbug.enable('network:udp');       // UDP-specific
dbug.enable('memory:arena');      // Arena allocator
dbug.enable('memory:refcount');   // Reference counting
```

---

### dbug.check() - Conditional Assertions

**Syntax**: `dbug.check(group, format_string, data_array, condition, action?)`

**Purpose**: Assert-like checks with custom failure actions

**Parameters**:
- `group: string` - Debug group tag
- `format_string: string` - Error message template
- `data_array: [dyn]` - Values to interpolate
- `condition: bool` - Condition to check (triggers if **FALSE**)
- `action?: block` - Optional: Custom action to take on failure

**Behavior**:
- Evaluates `condition`
- If `condition == false`:
  1. Prints formatted message to hex stream
  2. Executes optional `action` block (if provided)
  3. If no action provided, calls `failsafe(ERR_DEBUG_CHECK_FAILED)`

**Examples**:

```aria
// Simple assertion (no custom action)
dbug.check('sensors', "Temperature out of range: {{fix256}}", [temp],
    temp >= -40.0fx && temp <= 125.0fx);
// If temp invalid: Prints message, calls failsafe()

// With custom recovery action
dbug.check('memory', "Buffer overflow detected: size={{uint64}}, max={{uint64}}", 
    [requested_size, buffer_max],
    requested_size <= buffer_max,
    {
        // Custom recovery
        resize_buffer(requested_size);
        log.error("Buffer resized to prevent overflow");
    });

// With state logging on failure
dbug.check('network', "Invalid packet checksum: expected={{uint32}}, got={{uint32}}", 
    [expected_checksum, actual_checksum],
    expected_checksum == actual_checksum,
    {
        log_packet_dump(packet);
        drop_packet(packet);
        stats.corrupted_packets += 1;
    });
```

**Condition logic**: Check triggers when condition is **FALSE**

```aria
// This checks that value IS in range (triggers if NOT)
dbug.check('range', "Value out of range: {{int32}}", [value],
    value >= 0 && value <= 100);  // Triggers if value < 0 OR value > 100

// Equivalent traditional assertion
assert(value >= 0 && value <= 100, "Value out of range");
```

---

## Format String Interpolation

**Syntax**: `{{type}}` placeholders in format string

**Supported types** (examples):
- `{{int32}}`, `{{uint64}}`, `{{int128}}`, etc. - Integer types
- `{{fix256}}`, `{{tfp64}}`, `{{float}}`, `{{double}}` - Numeric types
- `{{bool}}` - Boolean (prints "true" or "false")
- `{{string}}` - String values
- `{{ptr}}` - Pointer addresses (hex format)
- `{{vec3}}`, `{{vec9}}` - Vector types
- `{{trit}}`, `{{nit}}` - Balanced number types
- Generic: `{{T}}` for template types

**Order matching**: Placeholders filled in order from `data_array`

```aria
dbug.print('example', "a={{int32}}, b={{fix256}}, c={{string}}", 
    [42, 3.14fx, "hello"]);
// Output: "a=42, b=3.14, c=hello"
```

**Type safety**: Compiler verifies placeholder types match array element types

```aria
// ✓ GOOD: Types match
dbug.print('test', "Value: {{int32}}", [42]);

// ✗ ERROR: Type mismatch
dbug.print('test', "Value: {{int32}}", [3.14fx]);  // fix256 doesn't match int32!
```

**Multiple identical types**: Use positional ordering

```aria
dbug.print('coords', "Position ({{fix256}}, {{fix256}}, {{fix256}})", 
    [x, y, z]);
// First {{fix256}} = x, second = y, third = z
```

---

## Runtime Control

### Enabling/Disabling Groups

```aria
// Enable single group
dbug.enable('network');

// Enable multiple groups
dbug.enable('network');
dbug.enable('memory');
dbug.enable('sensors');

// Disable group
dbug.disable('network');

// Toggle group
dbug.toggle('debugging');

// Check if enabled
if dbug.is_enabled('sensors') {
    // Custom debug logic
}
```

### Global Control

```aria
// Disable ALL debug output
dbug.disable_all();

// Enable ALL groups
dbug.enable_all();

// List enabled groups
string[]:enabled = dbug.list_enabled();
till(enabled.length - 1, 1) {
    print("Debug enabled for: ", enabled[$]);
}
```

### Environment Variable Control

**Startup configuration**:
```bash
# Enable specific groups at startup
export ARIA_DEBUG="network,memory,sensors"
./my_aria_program

# Enable all groups
export ARIA_DEBUG="*"
./my_aria_program

# Disable all (default)
export ARIA_DEBUG=""
./my_aria_program
```

**Runtime override**:
```aria
// Code-based enable overrides environment
dbug.enable('custom_group');  // Works even if not in ARIA_DEBUG
```

---

## Output Channels

### Hex Stream (Default)

Debug output goes to **hex stream** (separate from normal I/O):

```aria
// Regular output (stdout)
stdout.print("Application started");

// Debug output (hex stream, doesn't mix with stdout)
dbug.print('startup', "Initializing modules: {{uint32}}", [module_count]);

// Error output (stderr)
stderr.print("Critical error occurred");
```

**Why separate stream?**
- **No pollution** of production stdout/stderr
- **Can be redirected** independently
- **Filtered at OS level** (pipe to file, grep, etc.)

**Redirection example**:
```bash
# Send hex stream to file, keep stdout separate
./my_aria_program 2>debug.log  # Assuming hex stream → stderr by default
```

### Custom Output Handler

```aria
// Override default hex stream handler
dbug.set_output_handler({handler_fn});

func:custom_debug_handler = void(string:group, string:message) {
    // Custom formatting
    timestamp:now = get_timestamp();
    log_file.write("[", now, "] [", group, "] ", message, "\n");
}

dbug.set_output_handler(custom_debug_handler);
```

---

## Use Cases

### 1. Sensor Debugging

```aria
dbug.enable('sensors');

func:read_temperature = fix256() {
    fix256:raw_reading = adc_read();
    
    dbug.print('sensors', "Raw ADC: {{fix256}}", [raw_reading]);
    
    fix256:calibrated = apply_calibration(raw_reading);
    
    dbug.check('sensors', "Temperature out of physical range: {{fix256}}", 
        [calibrated],
        calibrated >= -273.15fx && calibrated <= 1000.0fx,
        {
            // Recovery: Use last valid reading
            calibrated = last_valid_temperature;
            sensor_error_count += 1;
        });
    
    dbug.print('sensors', "Calibrated temp: {{fix256}}", [calibrated]);
    
    return calibrated;
}
```

### 2. Memory Allocation Tracking

```aria
dbug.enable('memory');

struct:ArenaAllocator {
    uint64:capacity,
    uint64:used,
    ptr:base_address,
}

func:arena_alloc = ptr(ArenaAllocator@:arena, uint64:size) {
    dbug.print('memory', "Alloc request: {{uint64}} bytes (used: {{uint64}}/{{uint64}})", 
        [size, arena.used, arena.capacity]);
    
    dbug.check('memory', "Arena overflow: size={{uint64}}, available={{uint64}}", 
        [size, arena.capacity - arena.used],
        arena.used + size <= arena.capacity,
        {
            // Auto-grow arena
            arena_grow(arena, size * 2);
            dbug.print('memory', "Arena grown to {{uint64}} bytes", [arena.capacity]);
        });
    
    ptr:allocated = arena.base_address + arena.used;
    arena.used += size;
    
    dbug.print('memory', "Allocated at {{ptr}}, new used: {{uint64}}", 
        [allocated, arena.used]);
    
    return allocated;
}
```

### 3. Network Protocol Debugging

```aria
dbug.enable('network:tcp');

func:handle_tcp_packet = void(Packet:pkt) {
    dbug.print('network:tcp', "Packet: seq={{uint32}}, ack={{uint32}}, flags={{uint8}}", 
        [pkt.seq_num, pkt.ack_num, pkt.flags]);
    
    dbug.check('network:tcp', "Invalid sequence number: {{uint32}}", [pkt.seq_num],
        pkt.seq_num >= expected_seq,
        {
            // Duplicate or out-of-order packet
            if pkt.seq_num < expected_seq {
                dbug.print('network:tcp', "Duplicate packet (seq={{uint32}})", 
                    [pkt.seq_num]);
                drop_packet(pkt);
            } else {
                dbug.print('network:tcp', "Out-of-order packet, buffering", []);
                buffer_packet(pkt);
            }
        });
    
    process_packet(pkt);
}
```

### 4. Physics Simulation Validation

```aria
dbug.enable('physics');

func:update_particle = void(Particle@:p, fix256:dt) {
    vec3:old_pos = p.position;
    
    // Update position
    p.position += p.velocity * dt;
    
    dbug.print('physics', "Particle {{uint32}}: pos={{vec3}}, vel={{vec3}}", 
        [p.id, p.position, p.velocity]);
    
    // Energy conservation check
    fix256:kinetic = 0.5fx * p.mass * p.velocity.length_squared();
    fix256:potential = p.mass * GRAVITY * p.position.y;
    fix256:total_energy = kinetic + potential;
    
    dbug.check('physics', "Energy drift detected: E={{fix256}}, expected={{fix256}}", 
        [total_energy, p.initial_energy],
        abs(total_energy - p.initial_energy) < 0.01fx,
        {
            // Log energy drift warning
            log.warn("Particle ", p.id, " energy drift: ", 
                total_energy - p.initial_energy);
        });
}
```

### 5. Nikola ATPM Phase Tracking

```aria
dbug.enable('nikola:atpm');

func:update_atpm_phase = void(ATPMState@:state, fix256:dt) {
    vec9:old_phase = state.phase_vector;
    
    // Update phase
    state.phase_vector += state.phase_velocity * dt;
    
    dbug.print('nikola:atpm', 
        "ATPM phase update: Δt={{fix256}}, |Δφ|={{fix256}}", 
        [dt, (state.phase_vector - old_phase).length()]);
    
    // Verify phase coherence
    fix256:coherence = state.phase_vector.dot(state.target_pattern);
    
    dbug.check('nikola:atpm', 
        "Phase coherence breakdown: coherence={{fix256}}", 
        [coherence],
        coherence >= MIN_COHERENCE,
        {
            // Emergency phase reset
            dbug.print('nikola:atpm', "Emergency phase reset triggered", []);
            reset_phase_to_stable_attractor(state);
        });
}
```

---

## Advanced Patterns

### Hierarchical Group Organization

```aria
// Top-level subsystems
dbug.enable('network');
dbug.enable('memory');
dbug.enable('physics');

// Fine-grained modules (use ':' separator by convention)
dbug.enable('network:tcp');
dbug.enable('network:udp');
dbug.enable('network:tls');

dbug.enable('memory:arena');
dbug.enable('memory:pool');
dbug.enable('memory:gc');

// Can enable entire hierarchy
dbug.enable('network');  // Enables all network:* groups
```

### Conditional Group Enabling

```aria
// Enable based on build configuration
#if DEBUG_BUILD
    dbug.enable_all();
#elif TESTING_BUILD
    dbug.enable('network');
    dbug.enable('memory');
#else
    // Production: Only critical groups
    dbug.enable('failsafe');
#endif

// Runtime conditional enabling
if performance_profiling_mode {
    dbug.enable('profiling');
    dbug.enable('timers');
}
```

### Performance Monitoring

```aria
dbug.enable('perf');

func:expensive_operation = void() {
    timestamp:start = get_timestamp();
    
    // Operation
    do_complex_computation();
    
    timestamp:end = get_timestamp();
    fix256:elapsed = (end - start) / 1000.0fx;  // Convert to ms
    
    dbug.print('perf', "Operation took {{fix256}}ms", [elapsed]);
    
    dbug.check('perf', "Performance degradation: {{fix256}}ms (limit: 10ms)", 
        [elapsed],
        elapsed < 10.0fx,
        {
            log.warn("Performance threshold exceeded!");
            trigger_profiler();
        });
}
```

### State Dumps on Failure

```aria
dbug.enable('state');

func:critical_transaction = Result<void>() {
    // Capture initial state
    State:initial_state = snapshot_state();
    
    dbug.print('state', "Transaction starting: state={{uint32}}", 
        [initial_state.checksum]);
    
    Result<void>:result = attempt_transaction();
    
    if result.is_error {
        // Dump full state on error
        dbug.check('state', "Transaction failed, dumping state", [],
            false,  // Always triggers
            {
                dump_state_to_file(initial_state, "error_state_dump.bin");
                dbug.print('state', "State dump written to error_state_dump.bin", []);
            });
    }
    
    return result;
}
```

---

## Comparison with Traditional Debugging

### vs printf Debugging

```aria
// Traditional printf (mixed with stdout, hard to filter)
printf("Debug: x=%d, y=%d\n", x, y);  // Always runs, pollutes output

// dbug (grouped, can be disabled, separate stream)
dbug.print('debug', "x={{int32}}, y={{int32}}", [x, y]);  // Clean, filterable
```

### vs Assertions

```aria
// Traditional assert (only boolean check, crashes on failure)
assert(value >= 0 && value <= 100);  // Abrupt termination

// dbug (custom actions, graceful degradation)
dbug.check('validation', "Value out of range: {{int32}}", [value],
    value >= 0 && value <= 100,
    {
        value = clamp(value, 0, 100);  // Recovery instead of crash
    });
```

### vs Logging Libraries

```aria
// Traditional logging (requires setup, external dependency)
logger.info("network", "Packet size: {}", packet_size);  // External lib

// dbug (built-in, first-class language support)
dbug.print('network', "Packet size: {{uint32}}", [packet_size]);  // Native
```

---

## Implementation Notes

### Group Storage

Groups stored as **hash set** of enabled group names:

```c
// Runtime implementation (C)
#include <stdhash_set.h>

static hash_set* enabled_groups = NULL;

bool dbug_is_enabled(const char* group) {
    if (!enabled_groups) return false;
    return hash_set_contains(enabled_groups, group);
}

void dbug_enable(const char* group) {
    if (!enabled_groups) {
        enabled_groups = hash_set_create();
    }
    hash_set_insert(enabled_groups, group);
}
```

### Format String Parsing

Compile-time format string validation:

```aria
// Compiler checks:
// 1. Placeholder count matches array length
// 2. Placeholder types match array element types
// 3. All placeholders are valid type names

// ✓ Valid
dbug.print('test', "{{int32}} {{string}}", [42, "hello"]);

// ✗ Error: Count mismatch (2 placeholders, 1 value)
dbug.print('test', "{{int32}} {{string}}", [42]);

// ✗ Error: Type mismatch (int32 placeholder, fix256 value)
dbug.print('test', "{{int32}}", [3.14fx]);
```

### Zero-Cost When Disabled

**Critical optimization**: `dbug.print()` and `dbug.check()` should compile to **no-op** when group is disabled:

```aria
// When 'network' group is DISABLED at compile-time:
dbug.print('network', "Message", [data]);  // ← Entire call removed by optimizer

// Equivalent to:
// (nothing - completely elided)
```

**Implementation approach**:
1. **Compile-time constant propagation** if groups known at compile time
2. **Branch prediction** hints for runtime checks (likely disabled)
3. **Lazy evaluation** of format strings (don't format if disabled)

---

## Best Practices

### 1. Use Descriptive Group Names

```aria
// ✓ GOOD: Clear, hierarchical
dbug.enable('network:tcp:handshake');
dbug.enable('memory:arena:allocation');
dbug.enable('physics:collision:broadphase');

// ✗ BAD: Vague, flat
dbug.enable('debug');
dbug.enable('stuff');
dbug.enable('test');
```

### 2. Include Context in Messages

```aria
// ✓ GOOD: Enough context to understand
dbug.print('network', "TCP handshake failed: client={{string}}, error={{int32}}", 
    [client_ip, error_code]);

// ✗ BAD: Not enough context
dbug.print('network', "Failed: {{int32}}", [error_code]);  // Failed what?
```

### 3. Use Custom Actions for Recovery

```aria
// ✓ GOOD: Attempt recovery
dbug.check('sensors', "Out of range: {{fix256}}", [reading],
    reading >= MIN && reading <= MAX,
    {
        reading = clamp(reading, MIN, MAX);
        sensor_error_count += 1;
    });

// ✗ BAD: Just crash
dbug.check('sensors', "Out of range: {{fix256}}", [reading],
    reading >= MIN && reading <= MAX);
// No action → calls failsafe → program terminates
```

### 4. Disable Groups in Production (Selectively)

```aria
// Production configuration
#if PRODUCTION_BUILD
    // Only safety-critical debug groups
    dbug.enable('failsafe');
    dbug.enable('safety_checks');
    // Disable verbose groups
    dbug.disable('network');
    dbug.disable('memory');
#else
    // Development: Enable everything
    dbug.enable_all();
#endif
```

### 5. Document Group Meanings

```aria
// ✓ GOOD: Document what each group tracks
/**
 * Debug groups:
 *   'network' - All network activity
 *   'network:tcp' - TCP protocol specifics
 *   'network:udp' - UDP protocol specifics
 *   'memory' - Memory allocation/deallocation
 *   'memory:arena' - Arena allocator internals
 *   'physics' - Physics simulation state
 *   'physics:forces' - Force calculations
 */
func:main = int() {
    // ...
}
```

---

## Integration with Safety Systems

### Complement to failsafe()

```aria
// dbug.check() with no action → calls failsafe
dbug.check('critical', "Invariant violated: {{int32}}", [state],
    state == VALID);
// If state != VALID: Prints message, then calls failsafe(ERR_DEBUG_CHECK_FAILED)

// Custom failsafe can log debug state
func:failsafe = void(int32:err_code) {
    if err_code == ERR_DEBUG_CHECK_FAILED {
        // Debug check failed: Dump full state
        dbug.print('failsafe', "Entering failsafe from debug check", []);
        dump_system_state();
    }
    
    // ... handle error ...
}
```

### Complement to unknown/Result

```aria
// Debug checks for early warning, before unknown propagates
func:divide_safe = fix256 | unknown(fix256:a, fix256:b) {
    dbug.check('math', "Division by near-zero: b={{fix256}}", [b],
        abs(b) > 0.0001fx,
        {
            log.warn("Near-zero division detected");
        });
    
    if b == 0.0fx {
        return unknown;  // Safety system handles this
    }
    
    return a / b;
}
```

---

## Planned Enhancements

### Future Additions

1. **Level-based filtering** (like traditional logging):
   ```aria
   dbug.set_level('network', LEVEL_INFO);   // Info and above
   dbug.set_level('memory', LEVEL_WARN);    // Only warnings/errors
   ```

2. **Sampling for high-frequency events**:
   ```aria
   dbug.print_sampled('physics', rate_hz: 100, "State update {{vec3}}", [pos]);
   // Only prints 100 times per second (even if called thousands of times)
   ```

3. **Conditional compilation**:
   ```aria
   #dbug('network') {
       // Code only compiled if 'network' group enabled at build time
       expensive_state_validation();
   }
   ```

4. **Structured data output** (JSON, etc.):
   ```aria
   dbug.print_json('api', {
       "endpoint": endpoint_name,
       "latency_ms": latency,
       "status": status_code
   });
   ```

---

## Related Features

- **[unknown](unknown.md)** - Soft error handling (complementary to dbug checks)
- **[Result<T>](result.md)** - Hard error handling (orthogonal to debug)
- **[failsafe()](failsafe.md)** - Final safety net (dbug can trigger)

---

## Implementation Status

| Feature | Parser | Compiler | Runtime | Status |
|---------|--------|----------|---------|--------|
| `dbug.print()` | ⏳ | ⏳ | ⏳ | Planned |
| `dbug.check()` | ⏳ | ⏳ | ⏳ | Planned |
| Group enable/disable | ⏳ | ⏳ | ⏳ | Planned |
| Format string interpolation | ⏳ | ⏳ | ⏳ | Planned |
| Hex stream output | ⏳ | ⏳ | ⏳ | Planned |
| Environment variable control | ⏳ | ⏳ | ⏳ | Planned |

---

## Summary

**dbug = First-class debug system for safety-critical applications**

### Quick Decision Guide

| Need | Use This | Why |
|------|----------|-----|
| Temporary debug print | `dbug.print()` | Can disable without removing code |
| Validate invariants | `dbug.check()` | Assertions with custom recovery |
| Filter debug output | Group tags | Enable/disable by subsystem |
| Production debugging | Selective group enabling | Keep instrumentation in production |
| Performance monitoring | `dbug.print()` + timestamps | Track timing without profiler |

### Key Principles

1. **Group-based filtering** - Control debug output granularity
2. **Built-in formatting** - Type-safe string interpolation
3. **Custom recovery** - Assertions don't have to crash
4. **Separate output** - Hex stream doesn't pollute stdout/stderr
5. **Zero-cost when disabled** - No runtime overhead for disabled groups
6. **First-class language feature** - Not an external library

**For safety-critical systems**: dbug provides production-grade debugging infrastructure that complements unknown/Result/failsafe without requiring external dependencies.

**Remember**: "Debug instrumentation isn't technical debt - it's operational visibility. Keep it in production, control it with groups."
