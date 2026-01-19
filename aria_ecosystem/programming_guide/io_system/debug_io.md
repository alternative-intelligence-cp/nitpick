# Debug I/O (stddbg)

**Category**: I/O System → Control Plane  
**Primary Stream**: stddbg (FD 6)  
**Philosophy**: Debugging is infrastructure, not an afterthought

---

## Overview

Debug I/O in Aria centers on the **stddbg stream** - a dedicated channel for diagnostic and debug information that never interferes with production output.

---

## Key Concept: Permanent Debug Code

**Traditional approach**:
```python
# print("DEBUG: ...") # Commented out before shipping
```

**Aria approach**:
```aria
stddbg << "Debug information";  // Always present, redirected in production
```

**Result**: Debug code **stays in** the codebase forever, always available when needed.

---

## Basic Usage

```aria
// Function entry/exit
stddbg << "Entering process_batch()";
Result: Result = process_batch(items);
stddbg << "Exiting process_batch, result=" << result.status;

// State transitions
stddbg << "State: " << old_state << " -> " << new_state;

// Performance timing
start: Time = Time::now();
expensive_operation();
stddbg << "Operation took " << (Time::now() - start).milliseconds() << "ms";

// Variable dumps
stddbg << "balance=" << balance << ", limit=" << limit;
```

---

## Deployment Modes

```bash
# Development: See debug output
./program 6>&2  # stddbg to stderr (visible)

# Production: Silent
./program  # stddbg to /dev/null (default)

# Production debugging: Capture to file
./program 6>/var/log/app/debug.log
```

---

## Why This Works

1. **Zero cost when disabled**: Output to `/dev/null` is optimized away
2. **No code changes**: Same binary in dev and production
3. **Always available**: Just redirect fd 6 to enable debugging
4. **No polluted output**: Never mixes with stdout or stddato

---

## Best Practices

### ✅ DO: Leave Debug Statements In

```aria
fn calculate_tax(income: f64) -> f64 {
    stddbg << "calculate_tax: income=" << income;
    
    tax: f64 = income * TAX_RATE;
    
    stddbg << "  -> tax=" << tax;
    return tax;
}
```

### ❌ DON'T: Use stdout for Debug

```aria
// WRONG: Pollutes production output
stdout << "DEBUG: Entering function\n";

// RIGHT: Dedicated stream
stddbg << "Entering function";
```

---

## See Also

- [stddbg](stddbg.md) - Full stddbg documentation
- [Six-Stream Topology](six_stream_topology.md) - Where stddbg fits
- [Stream Separation](stream_separation.md) - When to use each stream

---

**Debug I/O in Aria: Write it once, keep it forever, enable when needed.**
