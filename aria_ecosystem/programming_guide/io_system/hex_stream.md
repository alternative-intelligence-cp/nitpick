# Hex-Stream (Six-Stream I/O System)

**Category**: I/O System → Architecture  
**Alternate Name**: Six-Stream Topology  
**Philosophy**: Debugging and data deserve first-class streams

---

## Overview

**Hex-Stream** (six streams) is Aria's revolutionary I/O architecture that extends the traditional Unix three-stream model (stdin/stdout/stderr) with three additional streams:

1. **stdin** (0) - User input
2. **stdout** (1) - Normal output
3. **stderr** (2) - Error messages
4. **stddati** (4) - **Structured data input** *(NEW)*
5. **stddato** (5) - **Structured data output** *(NEW)*
6. **stddbg** (6) - **Debug output** *(NEW)*

---

## Why "Hex-Stream"?

- **Hex** = Six (Greek: ἕξ)
- **Six streams** instead of three
- **Hexagonal architecture** concept: separate concerns by stream

---

## The Revolution

**Traditional Model** (3 streams):
- Programs must choose: serve humans OR machines
- Debug output pollutes production output
- No separation between control and data

**Hex-Stream Model** (6 streams):
- Programs serve humans AND machines simultaneously
- Debug output never pollutes anything
- Clear separation: Control plane vs Data plane

---

## Architecture

```
┌─────────────── CONTROL PLANE ───────────────┐
│  stdin (0)  →  User input                   │
│  stdout (1) →  Normal output                │
│  stderr (2) →  Error messages               │
│  stddbg (6) →  Debug traces                 │
└─────────────────────────────────────────────┘

┌─────────────── DATA PLANE ──────────────────┐
│  stddati (4) →  Structured input            │
│  stddato (5) →  Structured output           │
└─────────────────────────────────────────────┘
```

---

## Benefits

1. **Always-On Debugging**: Leave `stddbg` statements in production code
2. **Dual-Mode Output**: Serve humans (stdout) and APIs (stddato) simultaneously
3. **Clean Pipelines**: Data flows through stddato→stddati, control remains separate
4. **No Conditionals**: Stream redirection at deployment, not in code

---

## Example

```aria
fn main() {
    stddbg << "Program starting";  // Debug
    
    config: Config = stddati.read_json()?;  // Data input
    
    stdout << "Processing...";  // Human output
    
    Result: Result = process(config);
    
    stdout << "Done! Processed " << result.count << " items";  // Human
    stddato.write_json(result);  // Machine output
    
    stddbg << "Completed in " << elapsed << "ms";  // Debug
}
```

**Deployment**:
```bash
# Development: See everything
./program 4<config.json 5>out.json 6>&2

# Production: Silent debug
./program 4<config.json 5>out.json

# Debugging production: Capture debug
./program 4<config.json 5>out.json 6>debug.log
```

---

## See Also

- [Six-Stream Topology](six_stream_topology.md) - Detailed architecture
- [I/O Overview](io_overview.md) - Why six streams?
- [Control Plane](control_plane.md) vs [Data Plane](data_plane.md)
- [Stream Separation](stream_separation.md) - Best practices

---

**The Hex-Stream architecture is Aria's answer to 50 years of I/O limitations in Unix-derived systems.**
