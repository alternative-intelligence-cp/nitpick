# Aria I/O System Overview

**Category**: I/O System  
**Status**: Core Language Feature  
**Philosophy**: Debugging is not an afterthought - it's a first-class citizen

---

## The Problem with Traditional I/O

Every language you've used has **three streams**:
- `stdin` - Input from user/system
- `stdout` - Output to user/system  
- `stderr` - Error messages

This model has a **fatal flaw**: **debugging competes with production output**.

When you add `print()` debugging statements, they pollute stdout. When you want to see debug info, it's mixed with actual program output. You end up with code like:

```python
# Other languages force this mess
if DEBUG:
    print(f"DEBUG: Processing {item}", file=sys.stderr)
print(result)  # Mixed with debug output!
```

**Result**: Developers remove debugging code before shipping, making production issues harder to diagnose.

---

## Aria's Solution: Six Streams (Hex-Stream)

Aria doesn't fight human nature - it redirects it. Instead of three streams, Aria provides **six**:

### Standard Streams (Traditional)
1. **stdin** - Standard input
2. **stdout** - Standard output
3. **stderr** - Standard error

### Debug Streams (Revolutionary)
4. **stddbg** - Debug output channel
5. **stddati** - Data input channel (structured data in)
6. **stddato** - Data output channel (structured data out)

---

## Why This Changes Everything

### Debug Output Never Pollutes Production

```aria
// Debug output goes to stddbg - NEVER stdout
stddbg << "Processing user: " << user.name;

// Production output is clean
stdout << result;
```

**In Production**:
- `stddbg` → `/dev/null` (or log file)
- `stdout` → User sees only clean results
- **No code changes needed**

### Structured Data is Separate from Human-Readable Text

```aria
// Human-readable output
stdout << "Balance: $" << balance;

// Machine-readable JSON to stddato
stddato << {"balance": balance, "currency": "USD"};
```

**Result**: Same program can serve both humans AND APIs without conditional logic.

### Control Plane vs Data Plane

Aria separates **control** (how the program runs) from **data** (what the program processes):

| Plane | Streams | Purpose |
|-------|---------|---------|
| **Control** | stdin, stdout, stderr, stddbg | Human interaction & debugging |
| **Data** | stddati, stddato | Structured I/O for processing |

This mirrors how **network routers** work - control traffic (routing tables, health checks) is separate from data traffic (actual packets).

---

## The Philosophy

**Traditional Approach**: "Remove debug code before shipping"  
**Aria's Approach**: "Leave debug code in, just redirect the stream"

This is **safety by construction**:
- Developers debug naturally with `stddbg`
- No temptation to use `stdout` for debugging
- Production deployments redirect `stddbg` to logs
- **Debug information is always available when needed**

It's the same principle as TBB sticky errors and the compiler-as-inspector model:

> **"Don't fight human nature - redirect it into safe patterns"**

---

## Real-World Impact

### Before Aria (Python example)
```python
def process_data(items):
    # print(f"DEBUG: Processing {len(items)} items")  # Commented out for production!
    results = []
    # Using till-style iteration
    for i in range(len(items)):
        # print(f"DEBUG: Item {items[i]}")  # Also commented out
        result = expensive_operation(items[i])
        results.append(result)
    return results
```

**Problem**: When production breaks, you uncomment debugging, redeploy, hope to reproduce the issue.

### With Aria
```aria
fn process_data(items: []Item) -> []Result {
    stddbg << "Processing " << items.len() << " items";
    
    results: []Result = [];
    till(items.length - 1, 1) {
        stddbg << "Processing item: " << items[$];
        Result: Result = expensive_operation(items[$]);
        results.push(result);
    }
    
    return results;
}
```

**In Production**:
```bash
# Normally: stddbg goes to /dev/null
./program < input.txt > output.txt

# When debugging: Redirect stddbg to see what happened
./program < input.txt > output.txt 6> debug.log  # fd 6 = stddbg
```

**Result**: Debug code is **always present**, **never pollutes output**, and **available when needed**.

---

## Stream Organization

```
┌─────────────────────────────────────┐
│         CONTROL PLANE               │
│  (Human Interaction & Debugging)    │
├─────────────────────────────────────┤
│  stdin  (0) ←  User input           │
│  stdout (1) →  User output          │
│  stderr (2) →  Error messages       │
│  stddbg (6) →  Debug output         │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│          DATA PLANE                 │
│   (Structured Data Processing)      │
├─────────────────────────────────────┤
│  stddati (4) ←  Structured input    │
│  stddato (5) →  Structured output   │
└─────────────────────────────────────┘
```

### File Descriptor Numbers
- **0**: stdin
- **1**: stdout
- **2**: stderr
- **4**: stddati (data input)
- **5**: stddato (data output)
- **6**: stddbg (debug)

**Note**: FD 3 is reserved for future use.

---

## Key Principles

1. **Debugging is First-Class**: `stddbg` is built into the language, not a library feature
2. **Separation of Concerns**: Human output ≠ Debug output ≠ Data output
3. **Production-Ready Debugging**: Leave debug statements in production code
4. **Stream Redirection**: Control at deployment, not in code
5. **No Conditional Debugging**: No `if DEBUG:` checks polluting the codebase

---

## Next Steps

- [Six-Stream Topology](six_stream_topology.md) - Deep dive into the architecture
- [stddbg](stddbg.md) - Debug stream details
- [stddati/stddato](stddati.md) - Data plane streams
- [Stream Separation](stream_separation.md) - Best practices

---

**Remember**: Every time you write `stddbg << "..."`, you're using a feature **no other language** provides as a core primitive. This is Aria's "Uno reverse" on debugging culture.
