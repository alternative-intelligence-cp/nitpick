# Six-Stream Topology (Hex-Stream)

**Category**: I/O System → Architecture  
**File Descriptors**: 0, 1, 2, 4, 5, 6  
**Philosophy**: Debugging is infrastructure, not instrumentation

---

## The Traditional Three-Stream Model (Broken)

Every Unix system and most programming languages inherit this model:

```
Process
  ├─ stdin  (fd 0) ← Input
  ├─ stdout (fd 1) → Output
  └─ stderr (fd 2) → Errors
```

**The Problem**: This model assumes programs are either:
1. **Interactive** (human I/O via stdin/stdout)
2. **Pipeline components** (data transformation)

It provides **no separation** between:
- Production output vs debug output
- Human-readable vs machine-readable data
- Control plane vs data plane

---

## Aria's Six-Stream Model (Hex-Stream)

```
Process (Hex-Stream Architecture)
  │
  ├─ CONTROL PLANE ────────────────────
  │  ├─ stdin  (fd 0) ← User input
  │  ├─ stdout (fd 1) → User output
  │  ├─ stderr (fd 2) → Error messages
  │  └─ stddbg (fd 6) → Debug output
  │
  └─ DATA PLANE ───────────────────────
     ├─ stddati (fd 4) ← Structured input
     └─ stddato (fd 5) → Structured output
```

### File Descriptor Allocation

| FD | Stream | Direction | Plane | Purpose |
|----|--------|-----------|-------|---------|
| **0** | stdin | Input | Control | User/interactive input |
| **1** | stdout | Output | Control | User/interactive output |
| **2** | stderr | Output | Control | Error messages |
| **3** | *(reserved)* | - | - | Future use |
| **4** | stddati | Input | Data | Structured data in (JSON, msgpack, etc.) |
| **5** | stddato | Output | Data | Structured data out |
| **6** | stddbg | Output | Control | Debug/diagnostic output |

**Design Decision**: FD 3 is intentionally skipped to maintain clean 0-1-2 for traditional streams and 4-5-6 for new streams.

---

## Control Plane vs Data Plane

This architecture is inspired by **network router design**, where control traffic (routing protocols, health checks) is separate from data traffic (actual packets).

### Control Plane (Human-Facing)

**Purpose**: Interaction with humans (developers, operators, users)

**Streams**:
- **stdin**: Commands, user input, interactive queries
- **stdout**: Results, status messages, human-readable output
- **stderr**: Error messages, warnings, human-readable diagnostics
- **stddbg**: Debug traces, performance metrics, diagnostic info

**Characteristics**:
- Text-oriented (though binary is allowed)
- Formatted for human readability
- Can be verbose
- May include ANSI color codes
- Line-buffered by default

### Data Plane (Machine-Facing)

**Purpose**: Structured data processing (APIs, pipelines, ETL)

**Streams**:
- **stddati**: Structured input (JSON, MessagePack, Protocol Buffers, etc.)
- **stddato**: Structured output (same formats)

**Characteristics**:
- Binary or structured text (JSON, etc.)
- Optimized for parsing/serialization
- Compact representation
- Schema-driven
- Buffered for throughput

---

## Why This Separation Matters

### 1. Clean Pipeline Composition

**Traditional Approach** (broken):
```bash
# stdout is polluted with debug info!
./step1 | ./step2 | ./step3
```

**Aria Approach**:
```bash
# Data flows through stddato → stddati
./step1 5>&1 | ./step2 6>step2.debug.log 5>&1 | ./step3

# Or more simply with Aria's pipeline operator
./step1 |> ./step2 |> ./step3  # Automatically uses data plane
```

### 2. Always-On Debugging

**Traditional**:
```python
# Must choose: debug OR clean output
if DEBUG:
    print(f"Processing {item}", file=sys.stderr)
results = process(item)
print(json.dumps(results))  # Mixed with debug output!
```

**Aria**:
```aria
// Debug output never interferes
stddbg << "Processing " << item;
Result: Result = process(item);
stddato << result;  // Clean structured output
```

### 3. Dual-Mode Programs

The same program can serve **both humans and machines**:

```aria
fn calculate_balance(account: Account) -> Balance {
    // Debug logging (for developers)
    stddbg << "Calculating balance for account " << account.id;
    
    balance: Balance = account.compute_balance();
    
    // Human-readable output
    stdout << "Account " << account.id << ": $" << balance;
    
    // Machine-readable output
    stddato << {"account_id": account.id, "balance": balance};
    
    return balance;
}
```

**Usage**:
```bash
# Human mode: See pretty output on stdout
./program

# API mode: Get JSON on stddato (fd 5)
./program 5>output.json

# Debug mode: See what's happening
./program 6>debug.log

# All modes at once!
./program >human.txt 5>api.json 6>debug.log
```

---

## Stream Redirection Examples

### Standard Usage (Human Mode)
```bash
./aria_program
# stdin (0) = terminal input
# stdout (1) = terminal output
# stderr (2) = terminal errors
# stddbg (6) = /dev/null (discarded)
# stddati (4) = /dev/null (no data input)
# stddato (5) = /dev/null (no data output)
```

### Data Processing Mode
```bash
cat data.json | ./aria_program 4>&0 5>&1
# stddati (4) reads from stdin
# stddato (5) writes to stdout
# Control plane still available for monitoring
```

### Debug Mode
```bash
./aria_program 6>&2
# Debug output appears on stderr (visible in terminal)
```

### Production with Logging
```bash
./aria_program 6>/var/log/myapp/debug.log
# Debug output captured to file
# Production output clean on stdout
```

### Full Separation
```bash
./aria_program \
  </input/user.txt \          # stdin from file
  >output/user.txt \          # stdout to file
  2>output/errors.txt \       # stderr to file
  4<data/input.json \         # stddati from file
  5>data/output.json \        # stddato to file
  6>logs/debug.log            # stddbg to file
```

---

## Implementation Notes

### Bootstrap Requirements

The Hex-Stream topology is established during process startup **before main()** executes:

1. **Kernel provides FDs 0, 1, 2** (standard Unix behavior)
2. **Aria runtime opens FDs 4, 5, 6**:
   - FD 4 (stddati): Defaults to `/dev/null` unless redirected
   - FD 5 (stddato): Defaults to `/dev/null` unless redirected
   - FD 6 (stddbg): Defaults to `/dev/null` in production, or inherited from environment

3. **Language provides handles**:
   - Global constants: `stdin`, `stdout`, `stderr`, `stddati`, `stddato`, `stddbg`
   - Type: `Stream` (language primitive)

### Error Handling

If FDs 4, 5, or 6 cannot be opened during bootstrap:
- **Fallback**: Map to `/dev/null`
- **Sticky Error**: Runtime sets `STREAM_INIT_FAILED` flag
- **Observable**: `stddbg.is_valid()` returns `false`

This ensures programs never crash due to I/O setup - they just silently discard output if streams are unavailable.

---

## Comparison to Other Languages

### Python
```python
import sys
sys.stdin   # FD 0
sys.stdout  # FD 1
sys.stderr  # FD 2
# No stddbg, stddati, stddato - must use libraries
```

### Go
```go
os.Stdin   // FD 0
os.Stdout  // FD 1
os.Stderr  // FD 2
// No built-in debug stream
```

### Rust
```rust
std::io::stdin()   // FD 0
std::io::stdout()  // FD 1
std::io::stderr()  // FD 2
// No debug stream - use log crate
```

### Aria
```aria
stdin    // FD 0
stdout   // FD 1
stderr   // FD 2
stddati  // FD 4 (NEW)
stddato  // FD 5 (NEW)
stddbg   // FD 6 (NEW)
```

**Only Aria** provides debug and data streams as **language primitives**.

---

## Design Principles

1. **Debug Output is Infrastructure**: Not a library concern, not a logging framework - it's built into the I/O model

2. **Data ≠ Output**: Structured data (JSON, etc.) should not compete with human-readable messages

3. **Production-Ready Debugging**: Developers shouldn't delete debug statements before shipping

4. **Stream Redirection > Conditional Logic**: Control behavior at deployment, not in code

5. **Separation of Concerns**:
   - Control plane = human interaction
   - Data plane = machine processing

---

## Hex-Stream Guarantees

Aria's compiler and runtime guarantee:

✅ **All six streams are always available** (even if redirected to `/dev/null`)  
✅ **Streams never alias** (stddbg ≠ stdout, stddato ≠ stdout)  
✅ **FD numbers are fixed** (stddbg is always FD 6)  
✅ **Buffering is appropriate** (line-buffered for control, block-buffered for data)  
✅ **No silent failures** (sticky errors if streams are broken)

---

## The "Aha!" Moment

Here's when you realize why this matters:

**Traditional debugging**:
1. Add `print("DEBUG: ...")` to find bug
2. Mix debug output with production output
3. Remove debug statements before commit
4. Bug appears in production
5. Add debug statements back
6. Redeploy
7. Repeat

**Aria debugging**:
1. Add `stddbg << "..."` to find bug
2. Debug output goes to stddbg (separate from stdout)
3. **Leave debug statements in code**
4. Commit and deploy
5. Bug appears in production
6. Redirect stddbg to log file: `./program 6>debug.log`
7. **Immediately see what's happening** - no code changes needed

**Result**: Debug code becomes **documentation** of the developer's thought process, **permanently embedded** in the codebase.

---

## Related Topics

- [I/O System Overview](io_overview.md)
- [stddbg](stddbg.md) - Debug stream details
- [stddati](stddati.md) / [stddato](stddato.md) - Data plane
- [Control Plane](control_plane.md) vs [Data Plane](data_plane.md)
- [Stream Separation Best Practices](stream_separation.md)

---

**The Six-Stream topology is Aria's "Uno reverse" on the Unix philosophy**: Instead of composing small programs with pipes, Aria lets you compose **multiple I/O channels** within a single program.
