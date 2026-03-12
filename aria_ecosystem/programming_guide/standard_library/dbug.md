# Debug Utilities (dbug)

**Module**: stdlib.dbug  
**Category**: Standard Library → Development Tools  
**Philosophy**: Debug code is permanent infrastructure, not temporary scaffolding

---

## Overview

The `dbug` module provides sophisticated debugging utilities that let you leave debug code in your production codebase. Unlike `print` debugging that you comment out before shipping, `dbug` uses **group-based filtering** to selectively enable debug output.

**Key Benefits**:
- Debug code stays in source (no commenting out/uncommenting cycle)
- Zero performance impact when disabled
- Group-based filtering (enable only what you need)
- Assert-like functionality for invariant checking
- Function entry/exit tracing
- Soft warnings that don't halt execution

---

## Quick Start

```aria
use stdlib.dbug;

func:main = int32() {
    // Enable the "network" debug group
    dbug_enable("network");
    
    // This prints (network group is enabled)
    dbug_print("network", "Connecting to server");
    
    // This doesn't print (parser group is disabled)
    dbug_print("parser", "Parsing token");
    
    // Assert that condition is true
    int32:port = 443;
    dbug_check("network", "Port must be valid", port > 0);
    
    pass(0);
}
```

---

## Core Concepts

### Debug Groups

Groups are string tags that categorize debug output. Use descriptive names:

```aria
"network"      // Networking operations
"parser"       // Parsing/lexing
"validation"   // Input validation
"database"     // Database queries
"auth"         // Authentication
"cache"        // Caching operations
"perf"         // Performance measurements
```

**Special group**: `"all"` enables/disables every group at once.

### Filtering Logic

- Messages print **only if** their group is enabled
- When disabled: zero overhead (condition check, no string formatting)
- When enabled: formatted output to stdout

---

## Group Management

### dbug_enable(group)

Enable debug output for a specific group.

```aria
dbug_enable("network");       // Enable network debugging
dbug_enable("parser");        // Also enable parser debugging
dbug_enable("all");           // Enable EVERYTHING
```

### dbug_disable(group)

Disable debug output for a group.

```aria
dbug_disable("network");      // Disable network debugging
dbug_disable("all");          // Disable EVERYTHING
```

### dbug_is_enabled(group) → bool

Check if a group is currently active.

```aria
if (dbug_is_enabled("network")) {
    // Expensive debug computation only when needed
    string:stats = calculate_network_stats() ? "";
    dbug_print("network", stats);
}
```

### dbug_list_enabled()

Show all currently enabled groups (useful for troubleshooting).

```aria
dbug_list_enabled();
// Output:
// [DBUG] Enabled groups:
//   - network
//   - parser
```

---

## Core Debug Functions

### dbug_print(group, message)

Print a message if the group is enabled.

**Signature**:
```aria
pub func:dbug_print = void(string:group, string:message);
```

**Output format**: `[DBUG:group] message`

**Examples**:
```aria
dbug_print("network", "Connection established");
// Output (if enabled): [DBUG:network] Connection established

dbug_print("parser", "Entering parse_expression");
// Output (if enabled): [DBUG:parser] Entering parse_expression

dbug_print("cache", "Cache miss");
// Output (if enabled): [DBUG:cache] Cache miss
```

**When to use**:
- Progress tracking ("Processing batch 3 of 10")
- State changes ("Status: CONNECTING → CONNECTED")
- Milestone events ("Loaded 1000 records")

### dbug_print_labeled(group, label, message)

Print a labeled message (variable name + value style).

**Signature**:
```aria
pub func:dbug_print_labeled = void(string:group, string:label, string:message);
```

**Output format**: `[DBUG:group] label: message`

**Examples**:
```aria
dbug_print_labeled("network", "Hostname", "api.example.com");
// Output: [DBUG:network] Hostname: api.example.com

dbug_print_labeled("auth", "User ID", "12345");
// Output: [DBUG:auth] User ID: 12345

dbug_print_labeled("parser", "Token type", "IDENTIFIER");
// Output: [DBUG:parser] Token type: IDENTIFIER
```

**When to use**:
- Variable inspection
- Parameter logging
- State dumps

### dbug_check(group, message, condition)

**Hard assertion** - halts execution if condition is false.

**Signature**:
```aria
pub func:dbug_check = void(string:group, string:message, bool:condition);
```

**Behavior**:
- If `condition` is `true`: No output, execution continues
- If `condition` is `false`:
  1. **Always prints** error message (ignores group enable status)
  2. Shows additional context if group is enabled
  3. **Halts execution** (infinite loop until manual intervention)

**Output format** (on failure):
```
✗ ASSERTION FAILED [group]: message
  (Enable more debug output in this group for details)
  [Program halted due to assertion failure]
```

**Examples**:
```aria
// Validate age is positive
int32:age = get_user_age();
dbug_check("validation", "Age must be positive", age > 0);

// Ensure buffer size is reasonable
int64:buffer_size = allocate_buffer();
dbug_check("memory", "Buffer size must be positive", buffer_size > 0);

// Verify port is in valid range
int32:port = parse_port();
dbug_check("network", "Port must be in range", port > 0 && port < 65536);

// Check pointer is not null
UserData->:user = find_user(id);
dbug_check("database", "User must exist", user != NULL);
```

**When to use**:
- Invariants that "should never" be violated
- Preconditions at function entry
- Invariants in algorithms ("array should never be empty here")
- Development-time safety checks

**Important**:
- Failed assertions **always** print (even if group disabled)
- Use for conditions that indicate **programmer error**
- Not for runtime errors (use `result` types for those)

### dbug_warn(group, message, condition)

**Soft assertion** - prints warning but continues execution.

**Signature**:
```aria
pub func:dbug_warn = void(string:group, string:message, bool:condition);
```

**Behavior**:
- If `condition` is `true`: No output
- If `condition` is `false`:
  1. Prints warning message
  2. Shows extra context if group is enabled
  3. **Execution continues**

**Output format** (on failure):
```
⚠ WARNING [group]: message
  (Enable debug group for more details)
```

**Examples**:
```aria
// Warn about performance issues
float64:cache_hit_rate = calculate_hit_rate();
dbug_warn("cache", "Cache hit rate is low", cache_hit_rate > 0.9);

// Warn about unusual but non-fatal conditions
int64:response_time_ms = measure_response();
dbug_warn("perf", "Response time exceeds threshold", response_time_ms < 100);

// Warn about deprecated usage
bool:using_old_api = check_api_version();
dbug_warn("compat", "Using deprecated API", !using_old_api);
```

**When to use**:
- Performance problems (not errors)
- Unusual but valid conditions
- Deprecation notices
- Heuristic violations

**Difference from dbug_check**:
- `dbug_check`: "This should **never** happen" → **halts**
- `dbug_warn`: "This is unusual but okay" → **continues**

---

## Convenience Functions

### dbug_separator(group)

Print a visual separator line for grouping output.

```aria
dbug_separator("network");
// Output: [DBUG:network] ========================================
```

**When to use**:
- Before/after major sections
- Visual grouping of related messages
- Making dense output more readable

### dbug_enter(group, function_name)

Log function entry (execution tracing).

```aria
func:process_request = void(int32:user_id) {
    dbug_enter("handlers", "process_request");
    // ... function body ...
}
// Output: [DBUG:handlers] → Entering: process_request
```

### dbug_exit(group, function_name)

Log function exit (execution tracing).

```aria
func:process_request = void(int32:user_id) {
    dbug_enter("handlers", "process_request");
    // ... function body ...
    dbug_exit("handlers", "process_request");
}
// Output: [DBUG:handlers] ← Exiting: process_request
```

**Entry/Exit Pattern**:
```aria
func:complex_operation = result<int32>(string:input) {
    dbug_enter("compute", "complex_operation");
    dbug_separator("compute");
    
    dbug_print("compute", "Step 1: Validate input");
    // ...
    
    dbug_print("compute", "Step 2: Process");
    // ...
    
    dbug_separator("compute");
    dbug_exit("compute", "complex_operation");
    pass(result_ok(42));
}
```

---

## Variable Watchpoints

**New in dbug**: Track variable changes automatically!

### Concept

Watchpoints let you monitor variables for changes and get alerts when they're modified. Perfect for:
- Tracking loop counters
- Monitoring state transitions
- Debugging unexpected mutations
- Finding "who changed this value?"

### How It Works

**Important**: Watchpoints are **not automatic** (would require compiler instrumentation). Instead:

1. You register a variable pointer: `dbug_watch_int64("group", "label", @var)`
2. You manually trigger checks: `dbug_watch_check_all()`
3. Dbug compares current value to last known value
4. If changed AND group enabled → prints alert
5. Updates last known value

**Think of it as**: Manual checkpoints where you want to know "did this variable change since last check?"

### dbug_watch_int64(group, label, ptr)

Register an int64 variable for change tracking.

**Signature**:
```aria
pub func:dbug_watch_int64 = bool(string:group, string:label, int64->:variable_ptr);
```

**Parameters**:
- `group`: Debug group for filtering
- `label`: Descriptive name (e.g., "counter", "balance")
- `variable_ptr`: Pointer to the variable (`@your_var`)

**Returns**: `true` if registered, `false` if max watches reached

**Example**:
```aria
int64:counter = 0i64;
dbug_watch_int64("loop", "counter", @counter);

while (counter < 100i64) {
    counter = counter + 1i64;
    dbug_watch_check_all();  // Alerts when counter changes
}
```

**Output**:
```
[DBUG:loop] 👁 Watching: counter
[DBUG:loop] ⚡ CHANGED: counter (was 0, now 1)
[DBUG:loop] ⚡ CHANGED: counter (was 1, now 2)
[DBUG:loop] ⚡ CHANGED: counter (was 2, now 3)
...
```

### dbug_watch_check_all()

Check all registered watchpoints for changes.

**When to call**:
- **End of loop iterations** (track iteration variables)
- **After function calls** that might modify state
- **Before/after critical sections**
- **Wherever you suspect changes** happening

**Example**:
```aria
int64:x = 0i64;
int64:y = 0i64;

dbug_watch_int64("math", "x", @x);
dbug_watch_int64("math", "y", @y);

x = 10i64;
y = 20i64;

dbug_watch_check_all();  // Shows both x and y changed
```

**Important**: Only alerts **on check call**, not continuously.

### dbug_watch_remove(ptr)

Stop watching a specific variable.

```aria
int64:temp = 0i64;
int64->:ptr = @temp;

dbug_watch_int64("debug", "temp", ptr);
// ... use temp ...
dbug_watch_remove(ptr);  // Stop tracking
```

### dbug_watch_clear_all()

Remove all watchpoints at once (cleanup).

```aria
// Set up multiple watches
dbug_watch_int64("test", "a", @a);
dbug_watch_int64("test", "b", @b);
dbug_watch_int64("test", "c", @c);

run_test();

// Clean up all at once
dbug_watch_clear_all();
```

### dbug_watch_list()

Show all currently active watchpoints.

```aria
dbug_watch_list();
// Output:
// [DBUG] Active watchpoints:
//   [loop] counter = 42
//   [state] current_state = 2
//   [balance] account_balance = 1500
```

### Practical Examples

#### Tracking Loop Counter

```aria
dbug_enable("loop");

int64:i = 0i64;
dbug_watch_int64("loop", "loop counter", @i);

while (i < 5i64) {
    // ... work ...
    i = i + 1i64;
    dbug_watch_check_all();  // See each increment
}

dbug_watch_clear_all();
```

#### State Machine Debugging

```aria
int64:STATE_IDLE = 0i64;
int64:STATE_PROCESSING = 1i64;
int64:STATE_DONE = 2i64;

int64:state = STATE_IDLE;
dbug_watch_int64("fsm", "state", @state);

state = STATE_PROCESSING;
dbug_watch_check_all();  // Alert: state changed 0 → 1

process_data();

state = STATE_DONE;
dbug_watch_check_all();  // Alert: state changed 1 → 2
```

#### Multiple Variable Tracking

```aria
int64:balance = 1000i64;
int64:transactions = 0i64;
int64:fees = 0i64;

dbug_watch_int64("account", "balance", @balance);
dbug_watch_int64("account", "transactions", @transactions);
dbug_watch_int64("account", "fees", @fees);

// Show what we're watching
dbug_watch_list();

// Do operations
balance = balance - 100i64;  // Withdrawal
transactions = transactions + 1i64;
fees = fees + 2i64;

dbug_watch_check_all();  // Shows all three changed
```

#### Strategic Checkpoints

```aria
func:complex_computation = void() {
    int64:intermediate_result = 0i64;
    dbug_watch_int64("compute", "result", @intermediate_result);
    
    // Checkpoint 1: After phase 1
    intermediate_result = phase1();
    dbug_watch_check_all();
    
    // Checkpoint 2: After phase 2
    intermediate_result = phase2(intermediate_result);
    dbug_watch_check_all();
    
    // Checkpoint 3: After phase 3
    intermediate_result = phase3(intermediate_result);
    dbug_watch_check_all();
    
    pass();
}
```

### Limitations & Design

**Why Manual Checks?**
- Automatic tracking needs compiler instrumentation
- Manual checkpoints give you control
- More efficient (check only when needed)
- Predictable behavior

**Current Support**:
- ✅ int64 variables
- ❌ int32, float64, etc. (TODO: easy to add)
- ❌ Strings, structs (TODO: needs comparison logic)
- ❌ Continuous monitoring (not possible without threading)

**Max Watchpoints**: 16 simultaneous (increase `DBUG_MAX_WATCHES` if needed)

**Performance**: 
- Each `dbug_watch_check_all()` loops through all watches
- O(n) where n = number of active watches
- Negligible for < 20 watches

**Best Practices**:
- ✅ Use for variables you suspect are changing unexpectedly
- ✅ Place checks at logical boundaries (loop ends, function returns)
- ✅ Clear watches when done (`dbug_watch_clear_all()`)
- ✅ Use group filtering to reduce noise
- ❌ Don't check every line (too verbose)
- ❌ Don't forget to clear (wastes watch slots)

---

## Performance Timing

Measure execution time for code sections without external profiling tools.

### dbug_time_start(group, label) → int64

Start a timing measurement. Returns current clock ticks.

**Parameters**:
- `group` - Debug group (must be enabled)
- `label` - Description of what's being timed

**Returns**: Clock ticks (or 0 if group disabled)

```aria
int64:start = dbug_time_start("perf", "Database query");
```

### dbug_time_end(group, label, start_time)

End a timing measurement and print elapsed ticks.

**Parameters**:
- `group` - Debug group (must be enabled)
- `label` - Description (should match start label)
- `start_time` - Value returned from `dbug_time_start`

```aria
dbug_time_end("perf", "Database query", start);
// Output: [DBUG:perf] TIMING END: Database query took 123456 ticks
```

### Timing Examples

#### Basic Timing

```aria
dbug_enable("perf");

int64:t = dbug_time_start("perf", "File processing");

// Do work
process_large_file("data.txt");

dbug_time_end("perf", "File processing", t);
// Output: [DBUG:perf] TIMING END: File processing took 987654 ticks
```

#### Multiple Sections

```aria
dbug_enable("perf");

int64:total = dbug_time_start("perf", "Batch job");

int64:t1 = dbug_time_start("perf", "Load data");
load_from_database();
dbug_time_end("perf", "Load data", t1);

int64:t2 = dbug_time_start("perf", "Transform data");
apply_transformations();
dbug_time_end("perf", "Transform data", t2);

int64:t3 = dbug_time_start("perf", "Save results");
write_to_disk();
dbug_time_end("perf", "Save results", t3);

dbug_time_end("perf", "Batch job", total);
```

#### Comparative Timing

```aria
dbug_enable("perf");

// Time algorithm A
int64:t_a = dbug_time_start("perf", "Algorithm A");
result_a = sort_bubble(data);
dbug_time_end("perf", "Algorithm A", t_a);

// Time algorithm B
int64:t_b = dbug_time_start("perf", "Algorithm B");
result_b = sort_quick(data);
dbug_time_end("perf", "Algorithm B", t_b);

// Compare which is faster
```

### Timing Notes

**Clock Source**: Uses C `clock()` function (CPU time, not wall time)

**Units**: Ticks are platform-dependent (typically microseconds)

**Overhead**: Minimal when group disabled (just a boolean check)

**Best Practices**:
- ✅ Use for quick performance checks
- ✅ Time inner loops separately from outer logic
- ✅ Compare relative timings (A vs B)
- ❌ Don't rely on absolute tick values (platform-dependent)
- ❌ Don't use for wall-clock measurements (use system time for that)

---

## Statistics Counters

Track occurrences without manual counting code.

### dbug_count(group, label)

Increment a counter by 1.

**Parameters**:
- `group` - Debug group (must be enabled)
- `label` - Counter name

```aria
while (process_next_item()) {
    dbug_count("stats", "items_processed");
}
```

### dbug_count_reset(group, label)

Reset a counter to zero.

```aria
dbug_count_reset("stats", "items_processed");
```

### dbug_count_show(group, label)

Print current value of a specific counter.

```aria
dbug_count_show("stats", "items_processed");
// Output: [DBUG:stats] items_processed: 42
```

### dbug_count_show_all()

Print all active counters.

```aria
dbug_count_show_all();
// Output:
// [DBUG] Active counters:
//   [stats] items_processed: 42
//   [stats] errors: 3
//   [network] requests: 127
```

### Counter Examples

#### Basic Counting

```aria
dbug_enable("stats");

int32:i = 0;
while (i < 100) {
    dbug_count("stats", "loop_iterations");
    i = i + 1;
}

dbug_count_show("stats", "loop_iterations");
// Output: [DBUG:stats] loop_iterations: 100
```

#### Success/Error Tracking

```aria
dbug_enable("stats");

int32:i = 0;
while (i < 50) {
    bool:success = attempt_operation() ? false;
    
    if (success) {
        dbug_count("stats", "successes");
    } else {
        dbug_count("stats", "failures");
    }
    
    dbug_count("stats", "total_attempts");
    i = i + 1;
}

drop(println("\nFinal statistics:"));
dbug_count_show("stats", "successes");
dbug_count_show("stats", "failures");
dbug_count_show("stats", "total_attempts");
```

#### Multiple Counters by Category

```aria
dbug_enable("stats");

while (process_events()) {
    string:event_type = get_next_event_type() ? "";
    
    if (event_type == "click") {
        dbug_count("stats", "clicks");
    } else if (event_type == "scroll") {
        dbug_count("stats", "scrolls");
    } else if (event_type == "keypress") {
        dbug_count("stats", "keypresses");
    }
    
    dbug_count("stats", "total_events");
}

dbug_count_show_all();
```

#### Reset Pattern

```aria
dbug_enable("stats");

// First batch
process_batch_1();
dbug_count_show("stats", "items");

// Reset for second batch
dbug_count_reset("stats", "items");
process_batch_2();
dbug_count_show("stats", "items");
```

### Counter Limitations

**Max Counters**: 32 simultaneous (increase `DBUG_MAX_COUNTERS` if needed)

**Storage**: Each counter uses ~40 bytes (group + label + count + active flag)

**Performance**: 
- Finding counter is O(n) where n = number of active counters
- Negligible for < 50 counters
- Increment is O(1) after first lookup (cached internally)

**Group Filtering**:
- `dbug_count()` still increments if group disabled
- `dbug_count_show()` respects group filtering (won't print if disabled)
- This lets you collect stats even when not displaying them

**Best Practices**:
- ✅ Use for tracking occurrences (loops, events, operations)
- ✅ Show all counters at program end (`dbug_count_show_all()`)
- ✅ Reset counters between logical sections
- ✅ Use descriptive labels ("http_requests" not "count1")
- ❌ Don't create hundreds of counters (use proper profiling)
- ❌ Don't use for timing (use `dbug_time_*` functions)

---

## Real-World Examples

### Network Connection Handler

```aria
use stdlib.dbug;

func:connect_to_server = bool(string:hostname, int32:port) {
    dbug_enter("network", "connect_to_server");
    
    dbug_print_labeled("network", "Target", hostname);
    dbug_check("network", "Port must be valid", port > 0 && port < 65536);
    
    dbug_print("network", "Resolving DNS");
    // ... resolve hostname ...
    
    dbug_print("network", "Opening TCP socket");
    // ... create socket ...
    
    dbug_print("network", "Sending handshake");
    // ... send initial data ...
    
    dbug_exit("network", "connect_to_server");
    pass(true);
}

// Usage:
// dbug_enable("network");  // See all network operations
// connect_to_server("api.example.com", 443);
```

### Parser with Assertions

```aria
use stdlib.dbug;

func:parse_expression = result<AST>(TokenStream:tokens) {
    dbug_enter("parser", "parse_expression");
    
    // Precondition check
    dbug_check("parser", "Token stream cannot be empty", tokens.length > 0);
    
    dbug_print("parser", "Building AST");
    AST:tree = build_tree(tokens);
    
    // Invariant check
    dbug_check("parser", "AST root must exist", tree.root != NULL);
    
    dbug_print("parser", "Type checking");
    bool:valid = type_check(tree) ? false;
    dbug_warn("parser", "Type check should succeed", valid);
    
    dbug_exit("parser", "parse_expression");
    pass(result_ok(tree));
}
```

### Cache System with Performance Warnings

```aria
use stdlib.dbug;

func:cache_lookup = result<string>(string:key) {
    dbug_enter("cache", "cache_lookup");
    dbug_print_labeled("cache", "Key", key);
    
    // Check for hit
    bool:hit = check_cache(key) ? false;
    
    if (hit) {
        dbug_print("cache", "✓ Cache HIT");
        string:value = get_from_cache(key);
        dbug_exit("cache", "cache_lookup");
        pass(result_ok(value));
    } else {
        dbug_print("cache", "✗ Cache MISS");
        
        // Warn if miss rate is getting high (but don't fail)
        float64:miss_rate = calculate_miss_rate();
        dbug_warn("cache", "High miss rate detected", miss_rate < 0.3);
        
        string:value = fetch_from_database(key);
        cache_store(key, value);
        
        dbug_exit("cache", "cache_lookup");
        pass(result_ok(value));
    }
}
```

### Batch Processing with Timing and Statistics

```aria
use stdlib.dbug;

func:process_file = bool(string:filename) {
    // Returns true on success, false on error
    // Simulated file processing
    pass(true);  // or false for errors
};

func:batch_processor = void() {
    dbug_enable("batch");
    dbug_enable("stats");
    
    drop(println("Starting batch file processor..."));
    
    // Time the entire batch
    int64:total_time = dbug_time_start("batch", "Batch processing") ? 0;
    
    // Process 100 files
    int32:i = 0;
    while (i < 100) {
        // Time individual file
        int64:file_time = dbug_time_start("batch", "Single file");
        
        bool:success = process_file("file.txt");
        
        dbug_time_end("batch", "Single file", file_time);
        
        // Track statistics
        if (success) {
            dbug_count("stats", "files_success");
        } else {
            dbug_count("stats", "files_failed");
        }
        dbug_count("stats", "files_total");
        
        // Checkpoint every 25 files
        if (i % 25 == 0 && i > 0) {
            dbug_separator("batch");
            drop(print("Checkpoint at file #"));
            printInt32(i);
            drop(println(""));
            dbug_count_show_all();
            dbug_separator("batch");
        }
        
        i = i + 1;
    }
    
    dbug_time_end("batch", "Batch processing", total_time);
    
    // Final statistics
    drop(println("\n=== FINAL STATISTICS ==="));
    dbug_count_show_all();
};

// Output example:
// [DBUG:batch] TIMING START: Batch processing
// [DBUG:batch] TIMING START: Single file
// [DBUG:batch] TIMING END: Single file took 1234 ticks
// ...
// ──────────────────────────────────────────────────
// Checkpoint at file #25
// [DBUG] Active counters:
//   [stats] files_success: 23
//   [stats] files_failed: 2
//   [stats] files_total: 25
// ──────────────────────────────────────────────────
// ...
// [DBUG:batch] TIMING END: Batch processing took 123456 ticks
//
// === FINAL STATISTICS ===
// [DBUG] Active counters:
//   [stats] files_success: 94
//   [stats] files_failed: 6
//   [stats] files_total: 100
```

---

## Workflow Tips

### Development Phase

```aria
// Enable ALL debug output while developing
dbug_enable("all");

// Your code with liberal dbug calls
// ...

// When feature works, disable
dbug_disable("all");
```

### Debugging Specific Issue

```aria
// Problem: Network code not working
// Solution: Enable just that group

dbug_enable("network");
connect_to_database();  // Now you see all network debug output
```

### Production Deployment

```aria
// All dbug calls stay in source code
// Simply don't enable any groups
// Zero performance impact

func:main = int32() {
    // No dbug_enable calls
    
    // All these compile to no-ops (condition check only)
    dbug_print("network", "...");
    dbug_print("parser", "...");
    
    pass(0);
}
```

### Temporary Investigation

```aria
func:main = int32() {
    // Enable for investigation
    dbug_enable("database");
    
    buggy_function();  // Debug output visible
    
    // Disable after investigation
    dbug_disable("database");
    
    rest_of_program();  // No debug output
    
    pass(0);
}
```

---

## Performance Characteristics

| Operation | Disabled Group | Enabled Group |
|-----------|----------------|---------------|
| `dbug_print` | One string comparison + branch | String concatenation + print |
| `dbug_check` | Condition eval + branch | Comparison + potential print/halt |
| `dbug_warn` | Condition eval + branch | Comparison + potential print |
| Group lookup | O(n) where n = enabled groups | O(n) (typically n < 10) |

**Memory**: Fixed `32 * string` array (group names), one boolean (all flag)

**Recommendations**:
- Keep number of enabled groups < 10 for best performance
- Use conditional computation for expensive debug info:
  ```aria
  if (dbug_is_enabled("stats")) {
      string:expensive_stats = calculate_all_stats();
      dbug_print("stats", expensive_stats);
  }
  ```

---

## Comparison with Other Approaches

| Approach | Pros | Cons |
|----------|------|------|
| **Manual commenting** | Simple | Error-prone, loses debug code |
| **Preprocessor #ifdef** | Zero runtime cost | Requires recompilation, brittle |
| **Logging libraries** | Feature-rich | Heavy dependencies, complex |
| **print() everywhere** | Built-in | Always outputs, no filtering |
| **dbug module** | Leave code in place, selective | Small runtime check when disabled |

---

## Future Enhancements

When Aria gains additional features, dbug will expand:

### Format Strings (Planned)
```aria
// TODO: After format strings land
dbug_printf("network", "Received {{}} bytes from {{}}", bytes, ip_addr);
dbug_checkf("val", "Expected {} but got {}", count == 5, 5, count);
```

### Struct Dumping (Planned)
```aria
// TODO: After reflection/metaprogramming
dbug_dump_struct("debug", "User data", user);
// Output:
// [DBUG:debug] User data:
//   id: 12345
//   name: "Alice"
//   age: 30
```

### Timing Measurements (Planned)
```aria
// TODO: After higher-order functions
dbug_measure_time("perf", "Database query", func() {
    query_database();
});
// Output: [DBUG:perf] Database query took 45ms
```

---

## Best Practices

### ✓ DO

- **Use descriptive group names**: `"network"`, `"parser"` not `"dbg1"`, `"test"`
- **Check expensive computations**: Use `dbug_is_enabled()` before costly work
- **Leave debug code in**: That's the whole point!
- **Use assertions liberally**: Catch bugs early in development
- **Group related code**: Same group for related components

### ✗ DON'T

- **Don't use for production errors**: Use `result` types for runtime errors
- **Don't enable > 20 groups**: Performance degrades with many groups
- **Don't rely on debug output for correctness**: It should be supplementary
- **Don't use as logging**: This is for development, not production monitoring
- **Don't forget to disable "all"**: Leave explicit enables for subsystems

---

## Integration with Other Tools

### With stddbg stream (I/O system)

`dbug` prints to stdout. For separate debug stream:

```aria
// TODO: When stddbg stream is implemented
// dbug will automatically use stddbg instead of stdout
```

### With GDB

When assertions fail, program halts (infinite loop). Attach GDB:

```bash
$ gdb -p <pid>
(gdb) bt  # Backtrace shows where assertion failed
```

### With Result Types

```aria
func:risky_operation = result<int32>() {
    dbug_enter("ops", "risky_operation");
    
    result<int32>:r = might_fail();
    if (r.is_error) {
        dbug_print("ops", "Operation failed as expected");
        dbug_exit("ops", "risky_operation");
        pass(r);  // Propagate error
    }
    
    // Continue with success path
    dbug_print("ops", "Operation succeeded");
    dbug_exit("ops", "risky_operation");
    pass(r);
}
```

---

## See Also

- [Debug I/O (stddbg)](debug_io.md) - Dedicated debug stream
- [Error Handling](../control_flow/error_handling.md) - Result types for runtime errors
- [Assertions](assertions.md) - Compile-time assertions (TODO)
- [Testing](../testing/unit_tests.md) - Test framework (TODO)
