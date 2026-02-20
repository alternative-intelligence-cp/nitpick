# DBUG Watchpoint Feature - Complete

**Date**: February 15, 2026  
**Enhancement**: Variable watching system for dbug module  
**Requested By**: User idea - "dbug.watch(var, condition, action)"

---

## What Was Added

Extended the dbug module with **variable watchpoint tracking** - a system to monitor variables for changes and alert when they're modified.

### User's Original Request

```
dbug.watch(var, condition, action) / dbug.unwatch(var);

that somehow checks if a var has been modified, if so check against condition 
and if condition met do action such as print or something.
```

### Challenge Identified

> "i'm not really sure how to keep it updated on when the var changes though, that 
> may be the tricky part, as it needs to do this check constantly until disabled."

**The Problem**: Automatic change detection requires:
- Compiler instrumentation (hook every assignment)
- OR background threading (continuously poll)
- OR language-level getters/setters

None of these exist in Aria yet.

### Solution: Manual Checkpoint System

Instead of automatic detection, implemented a **manual checkpoint pattern**:

1. Register variable pointer: `dbug_watch_int64("group", "label", @var)`
2. At strategic points, call: `dbug_watch_check_all()`
3. Compares current value vs last known value
4. Alerts if changed (and group enabled)

**Trade-off**: Not continuous, but gives developer control over when checks happen.

---

## API Added

### Core Functions

```aria
// Register a watchpoint
dbug_watch_int64(group, label, ptr) -> bool

// Check all watches (call manually)
dbug_watch_check_all() -> void

// Remove specific watch
dbug_watch_remove(ptr) -> void

// Clear all watches
dbug_watch_clear_all() -> void

// List active watches
dbug_watch_list() -> void
```

### Data Structures

```aria
const int32:DBUG_MAX_WATCHES = 16;

struct:WatchPoint {
    int64->:ptr;          // Pointer to watched variable
    int64:last_value;     // Last known value (for change detection)
    string:label;         // Description
    string:group;         // Debug group
    bool:active;          // Is watch active?
};

WatchPoint[DBUG_MAX_WATCHES]:_dbug_watches;
int32:_dbug_watch_count = 0;
```

---

## Usage Pattern

### Basic Example

```aria
dbug_enable("loop");

int64:counter = 0i64;
dbug_watch_int64("loop", "counter", @counter);

while (counter < 5i64) {
    counter = counter + 1i64;
    dbug_watch_check_all();  // ← Manual checkpoint
}

dbug_watch_clear_all();
```

**Output**:
```
[DBUG:loop] 👁 Watching: counter
[DBUG:loop] ⚡ CHANGED: counter (was 0, now 1)
[DBUG:loop] ⚡ CHANGED: counter (was 1, now 2)
[DBUG:loop] ⚡ CHANGED: counter (was 2, now 3)
[DBUG:loop] ⚡ CHANGED: counter (was 3, now 4)
[DBUG:loop] ⚡ CHANGED: counter (was 4, now 5)
```

### State Machine Example

```aria
int64:state = STATE_IDLE;
dbug_watch_int64("fsm", "state", @state);

state = STATE_CONNECTING;
dbug_watch_check_all();  // ← Checkpoint after state change

connect_to_server();

state = STATE_CONNECTED;
dbug_watch_check_all();  // ← Another checkpoint
```

### Multiple Variables

```aria
int64:x = 0i64;
int64:y = 0i64;
int64:sum = 0i64;

dbug_watch_int64("math", "x", @x);
dbug_watch_int64("math", "y", @y);
dbug_watch_int64("math", "sum", @sum);

x = 10i64;
y = 20i64;
sum = x + y;

dbug_watch_check_all();  // Shows all three changed
```

---

## Design Decisions

### Why Manual Checkpoints vs Automatic?

**Automatic (not implemented)**:
- ✅ Pro: Developer doesn't think about it
- ❌ Con: Needs compiler hooks for every assignment
- ❌ Con: Performance overhead (check on every write)
- ❌ Con: Not available in current Aria

**Manual (implemented)**:
- ✅ Pro: Works with current language features
- ✅ Pro: Developer controls when checks happen
- ✅ Pro: Efficient (only check when you care)
- ✅ Pro: Predictable behavior
- ⚠️ Con: Requires discipline (must remember to call check_all)

**Verdict**: Manual is pragmatic for now, with clear upgrade path when language gains capabilities.

### Why Only int64?

**Current**:
- `dbug_watch_int64()` for 64-bit integers

**Reason**:
- Proof of concept with most common debug type
- Easy to extend: copy function, change type signature
- Avoids premature generalization

**Future** (trivial to add):
```aria
dbug_watch_int32(group, label, int32->:ptr)
dbug_watch_float64(group, label, float64->:ptr)
dbug_watch_bool(group, label, bool->:ptr)
// etc.
```

### Condition & Action Parameters

**User request**: `dbug.watch(var, condition, action)`

**Current implementation**: No explicit condition/action parameters

**Rationale**:
- **Condition**: Implicit condition is "value changed" (always checked)
  - Custom conditions need: function pointers or callbacks (not in Aria yet)
  - Workaround: Check manually after `dbug_watch_check_all()` if needed
  
- **Action**: Implicit action is "print alert" (always done)
  - Custom actions need: function pointers or callbacks
  - Workaround: Listen for alerts in output, react accordingly

**Future**: When Aria gains function pointers:
```aria
// TODO: After function pointers
dbug_watch_int64_if(
    group, 
    label, 
    ptr, 
    condition_fn,  // func(int64) -> bool
    action_fn      // func(int64) -> void
)
```

---

## Files Modified/Created

### 1. `/stdlib/dbug.aria` - Extended

**Added** (200 lines):
- WatchPoint struct definition
- Global watchpoint registry
- 5 new public functions
- Full documentation in code

**Total**: 587 lines (was 387)

### 2. `/tests/test_dbug_watch.aria` - New

**Created** (270 lines):
- 5 comprehensive test cases
- Real-world usage patterns
- Multiple variable tracking demo
- State machine example
- Strategic checkpoint demo

### 3. `/programming_guide/standard_library/dbug.md` - Extended

**Added** (150 lines):
- Complete watchpoint documentation
- Concept explanation (manual checkpoints)
- All function signatures
- 5 practical examples
- Limitations & design rationale
- Best practices

**Total**: 783 lines (was 633)

---

## Testing Strategy

```aria
// Test 1: Basic single variable
int64:counter = 0i64;
dbug_watch_int64("test", "counter", @counter);
while (counter < 5i64) {
    counter = counter + 1i64;
    dbug_watch_check_all();
}
// Expected: 5 change alerts

// Test 2: Multiple variables
dbug_watch_int64("test", "x", @x);
dbug_watch_int64("test", "y", @y);
x = 1i64; y = 2i64;
dbug_watch_check_all();
// Expected: 2 change alerts

// Test 3: Group filtering
dbug_watch_int64("important", "critical", @critical);
dbug_watch_int64("verbose", "detail", @detail);
dbug_enable("important");
dbug_disable("verbose");
critical = 42i64; detail = 99i64;
dbug_watch_check_all();
// Expected: Only "critical" alert shown

// Test 4: State machine
int64:state = 0i64;
dbug_watch_int64("fsm", "state", @state);
state = 1i64; dbug_watch_check_all();
state = 2i64; dbug_watch_check_all();
// Expected: 2 state transition alerts

// Test 5: Cleanup
dbug_watch_clear_all();
dbug_watch_list();
// Expected: "No active watchpoints"
```

---

## Use Cases

### 1. Loop Counter Debugging

**Problem**: "Why is this loop iterating 101 times instead of 100?"

**Solution**:
```aria
int64:i = 0i64;
dbug_watch_int64("loop", "i", @i);

while (i < 100i64) {  // Bug: should be i <= 100i64
    i = i + 1i64;
    dbug_watch_check_all();
}
// See every value of i, spot the off-by-one
```

### 2. State Corruption Detection

**Problem**: "Something is modifying my state variable unexpectedly"

**Solution**:
```aria
int64:connection_state = STATE_DISCONNECTED;
dbug_watch_int64("state", "connection", @connection_state);

// Various operations
operation_a();
dbug_watch_check_all();  // Checkpoint

operation_b();
dbug_watch_check_all();  // Checkpoint

// If state changes unexpectedly, you know it happened between checkpoints
```

### 3. Race Condition Debugging

**Problem**: "Value changes between check and use"

**Solution**:
```aria
int64:shared_counter = 0i64;
dbug_watch_int64("race", "counter", @shared_counter);

// Thread A (simulated)
shared_counter = shared_counter + 1i64;
dbug_watch_check_all();

// Thread B (simulated)
shared_counter = shared_counter + 1i64;
dbug_watch_check_all();

// See interleaved modifications
```

### 4. Algorithm Invariant Checking

**Problem**: "This value should never exceed 1000"

**Solution**:
```aria
int64:accumulator = 0i64;
dbug_watch_int64("algo", "accumulator", @accumulator);

while (has_more_data()) {
    accumulator = accumulator + next_value();
    dbug_watch_check_all();
    
    // Manual invariant (could add to dbug in future)
    if (accumulator > 1000i64) {
        dbug_check("algo", "Accumulator exceeded limit!", false);
    }
}
```

---

## Comparison to Other Languages

| Language | Feature | Notes |
|----------|---------|-------|
| **GDB** | `watch` command | Hardware watchpoint, automatic |
| **LLDB** | `watchpoint set` | Hardware watchpoint, automatic |
| **Python** | `trace()` or `__setattr__` | Hook each assignment |
| **JavaScript** | `Object.defineProperty()` getter/setter | Property-level hooks |
| **Aria dbug** | Manual checkpoints | No runtime or compiler support needed |

**Aria's approach**:
- Closest to: Manual breakpoints with value inspection
- Trade-off: Not automatic, but works today with current tooling
- Upgrade path: Could become automatic with compiler support later

---

## Future Enhancements

### Phase 1 (Easy - Just Add Types)

```aria
dbug_watch_int32(group, label, int32->:ptr)
dbug_watch_int16(group, label, int16->:ptr)
dbug_watch_float64(group, label, float64->:ptr)
dbug_watch_bool(group, label, bool->:ptr)
```

### Phase 2 (Needs Language Features)

```aria
// After function pointers
dbug_watch_int64_if(
    group, 
    label, 
    ptr,
    condition: func(int64) -> bool,
    action: func(int64) -> void
)

// Example:
dbug_watch_int64_if(
    "validation",
    "age",
    @age,
    func(val) { pass(val >= 0 && val < 150); },  // Condition
    func(val) {                                   // Action
        dbug_print("validation", "Age out of range!");
    }
);
```

### Phase 3 (Needs Compiler Support)

```aria
// Compiler-instrumented automatic watchpoints
#[watch("group", "label")]
int64:my_var = 0i64;

// Compiler generates:
// - Hook on every assignment to my_var
// - Auto-call check function
// - No manual checkpoints needed
```

### Phase 4 (Needs Threading)

```aria
// Background monitoring thread
dbug_watch_async_int64(group, label, ptr, interval_ms);

// Polls variable every N milliseconds in background
// Alerts on change without manual checkpoints
```

---

## Summary

### What User Asked For

> "dbug.watch(var, condition, action) that somehow checks if a var has been modified"

### What Was Delivered

✅ **Core functionality**: Variable change detection  
✅ **Pointer-based**: Monitors via `@variable` addresses  
✅ **Group filtering**: Respects existing dbug group system  
✅ **Multiple watches**: Track up to 16 variables simultaneously  
✅ **Clean API**: Register, check, remove, clear, list  

⚠️ **Manual checkpoints**: Not automatic (language limitation)  
⚠️ **int64 only**: Easy to extend to other types  
⚠️ **No custom conditions/actions**: Needs function pointers (future)  

### Philosophy Maintained

The watchpoint system follows dbug's core philosophy:
- **Leave it in code**: Watches stay in source, controlled by groups
- **Zero cost when disabled**: Group filtering applies to watchpoints too
- **Explicit over implicit**: You control when checks happen
- **Progressive enhancement**: Works today, clear upgrade path for future

### Key Innovation

**Manual checkpoints** as a practical solution to automatic detection:
- Works with current language capabilities
- Gives developers control
- Efficient (check only when needed)
- Predictable behavior
- Clear upgrade path when compiler gains instrumentation

---

## User Feedback Point

The implementation addresses the user's core need (track variable changes) while being honest about the limitation (manual checks vs automatic). The documentation clearly explains:

1. **Why it's manual**: Language constraints
2. **Where to place checks**: Strategic points (loops, after operations)
3. **What's coming**: Automatic detection when compiler supports it

This is better than over-promising and under-delivering. The manual checkpoint pattern is actually quite powerful for debugging state machines, loop variables, and algorithm invariants.

**Question for user**: Does the manual checkpoint approach meet your needs, or would you prefer to wait for automatic detection via compiler support? My thinking is: manual is useful today, automatic can come later as an enhancement.
