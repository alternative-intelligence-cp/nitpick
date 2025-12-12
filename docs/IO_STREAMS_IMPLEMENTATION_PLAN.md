# Modern I/O Streams Implementation Plan
**Based on**: research_006_modern_streams.txt (388 lines)  
**Date**: December 11, 2025  
**Status**: Research complete, ready for Phase 6 implementation

---

## Executive Summary

The Aria runtime implements a **6-stream I/O model** that replaces the traditional Unix 3-stream (stdin/stdout/stderr) model. This design provides:

1. **Text/Binary Separation**: Dedicated channels prevent pipeline corruption
2. **Debug Isolation**: Observability without polluting output streams  
3. **Performance**: Zero-copy optimizations, no Base64 encoding overhead
4. **Safety**: TBB-aware serialization, UTF-8 enforcement, wildx protection

---

## Six-Stream Topology

| Stream   | FD | Type   | Buffering    | Purpose                                    |
|----------|----|---------|--------------|--------------------------------------------|
| stdin    | 0  | Text    | Line         | Human input, configuration                 |
| stdout   | 1  | Text    | Line         | Human-readable output, UI elements         |
| stderr   | 2  | Text    | Line         | Critical errors, panic dumps               |
| **stddbg** | 3  | Text/Structured | Async Ring | Diagnostic telemetry, trace logs |
| **stddati** | 4  | Binary  | Block        | Raw machine-readable input (tensors, etc.) |
| **stddato** | 5  | Binary  | Block        | Raw machine-readable output                |

---

## Key Design Decisions

### 1. Semantic Separation of Concerns

**Problem**: Traditional stdout conflates:
- Functional output (JSON, CSV, binaries)
- User interaction (prompts, progress bars)
- Debug logs (trace data)
- Binary payloads (raw bytes)

**Solution**: 
- **Control Plane** (stdin/stdout/stderr): Human interaction
- **Data Plane** (stddati/stddato): Binary pipelines
- **Observability Plane** (stddbg): Diagnostics

**Example Use Case**:
```aria
// ML training process
io.stdout.writeLine("Training epoch 5/10: 85% complete");  // User sees progress
io.stddbg.log("info", "Gradient", { loss: 0.032 });        // Logs to file
io.stddato.writeBytes(tensor_weights);                     // Binary to GPU worker
```

### 2. Platform-Specific Implementation

#### Linux/POSIX (File Descriptors)
- **Initialization**: Runtime checks/creates FDs 3-5 using `fcntl(F_GETFD)`
- **Default Provisioning**: 
  - stddbg → `/dev/null` or `$ARIA_LOG_FILE`
  - stddati/stddato → `/dev/null` (EOF/discard)
- **Process Spawning**: `dup2()` maps pipe ends to FDs 3-5 in child
- **Zero-Copy**: `splice()` syscall for stddati→stddato pipelines

#### Windows (HANDLEs)
- **Initialization**: No OS support for FDs 3-5
- **Handle Synthesis**: `CreatePipe()` for anonymous pipes
- **Passing**: Environment variable `__ARIA_STREAMS=DBG:<handle>;IN:<handle>;OUT:<handle>`
- **Runtime Reconstruction**: Parse env var, cast to HANDLEs
- **Named Pipes**: `\\.\pipe\aria-debug-*` for persistent IPC (ACLs, duplex)

#### Conditional Compilation
```aria
use cfg(target_os = "linux") std.os.linux.io;   // dup2/fcntl logic
use cfg(target_os = "windows") std.os.windows.io; // CreatePipe/STARTUPINFO
```

---

## Buffering Architecture

### Text Streams (stdin/stdout/stderr)
- **Strategy**: Line-buffered (1-4 KB)
- **Flush**: Automatic on newline `\n`
- **Purpose**: Responsive user interaction (no "hanging cursor")

### Binary Streams (stddati/stddato)  
- **Strategy**: Block-buffered (64 KB+, dynamic via `computeOptimalSize()`)
- **Flush**: On full or explicit `flush()`
- **Purpose**: Maximize throughput, minimize syscalls

### Debug Stream (stddbg)
- **Strategy**: Asynchronous SPSC ring buffer
- **Thread**: Dedicated "Logger Thread" drains buffer
- **Policy**: Drop-on-Full (prevent backpressure)
- **Purpose**: Zero-impact logging for production

### Thread Safety
- **Mutex Locking**: Each stream protected by mutex (atomic writes)
- **Buffer Pinning**: `#` operator prevents GC movement during syscalls
- **Wild Memory**: stddati/stddato support zero-copy via wild buffers (no GC overhead)

---

## API Design

### Stream Trait (Base Interface)
```aria
trait:Stream = {
    // Text I/O (UTF-8 enforced)
    func:write = result<int>(string:text);
    func:writeLine = result<int>(string:text);
    func:readLine = result<string>();
    
    // Binary I/O (raw bytes)
    func:writeBytes = result<int>(buffer:data);
    func:readBytes = result<buffer>(int:size);
    func:readInto = result<int>(wild buffer:dest); // Zero-copy
    
    // TBB-aware I/O (sentinel-safe)
    func:writeTBB8 = result<int>(tbb8:val);
    func:writeTBB64 = result<int>(tbb64:val);
    func:readTBB8 = result<tbb8>();
    
    // Utility
    func:flush = result<void>();
    func:close = result<void>();
    func:isatty = bool();
};
```

### Debug Stream Specialization
```aria
trait:DebugStream {
    func:log = result<void>(string:level, string:msg, obj:data);
    func:warn = result<void>(string:msg, obj:data);
    func:error = result<void>(string:msg, obj:data);
};
```

### Global Singleton Objects
```aria
mod std.io {
    pub const stdin: Stream;
    pub const stdout: Stream;
    pub const stderr: Stream;
    pub const stddbg: DebugStream;  // FD 3
    pub const stddati: Stream;      // FD 4
    pub const stddato: Stream;      // FD 5
}
```

---

## Process Management Integration

### Spawn with Stream Redirection
```aria
spawn("./processor", {
    stdin:  io.stdin,                    // Pass through
    stdout: io.stdout,                   // Pass through
    stderr: io.stderr,                   // Pass through
    stddbg: openFile("debug.log", "w"),  // Redirect to file
    stddati: pipe_reader,                // Pipe from previous process
    stddato: pipe_writer                 // Pipe to next process
});
```

### Complex Pipeline Example
```aria
// ML training pipeline: preprocessor → trainer → postprocessor
pipe:preprocess_out = createPipe();
pipe:train_out = createPipe();

spawn("./preprocess", {
    stddati: openFile("raw_data.bin", "r"),
    stddato: preprocess_out.write,
    stddbg: openFile("preprocess.log", "w")
});

spawn("./train", {
    stddati: preprocess_out.read,
    stddato: train_out.write,
    stddbg: openFile("train.log", "w"),
    stdout: io.stdout  // Progress bars visible to user
});

spawn("./postprocess", {
    stddati: train_out.read,
    stddato: openFile("model.bin", "w"),
    stddbg: openFile("postprocess.log", "w")
});
```

---

## TBB Integration

### Sentinel-Safe Serialization
```aria
// Writing TBB values to stddato
tbb64:value = 12345;
stddato.writeTBB64(value); // Runtime checks: if value == ERR, handle appropriately

// Two modes:
// 1. Raw Mode: Write exact bytes (0x8000 for ERR), receiver must understand TBB
// 2. Safe Mode: Check for ERR before write, emit error packet or halt
```

### Sticky Error Propagation Across Processes
```aria
// Process A calculates
tbb64:result = compute();  // May return ERR on overflow

// Write to pipe (ERR preserved)
stddato.writeTBB64(result);

// Process B reads
tbb64:received = stddati.readTBB64() ? ERR;  // Receives ERR if A computed ERR
if (received == ERR) {
    stderr.writeLine("Upstream computation failed");
}
```

---

## Async I/O Integration

### Event Loop Registration
- **Linux**: `epoll` for FDs 0-5
- **macOS**: `kqueue`
- **Windows**: `IOCP` for HANDLEs

### Async Read Pattern
```aria
async func:processInput = void() {
    wild buffer:chunk = aria.alloc_buffer(4096);
    defer aria.free(chunk);
    
    while (true) {
        // Non-blocking read (suspends coroutine if no data)
        result:res = await io.stddati.readAsync(4096);
        if (res.err != NULL) break;
        
        // Process data
        transform(res.val);
        
        // Non-blocking write
        await io.stddato.writeAsync(res.val);
    }
}
```

### Backpressure Management
- If stddati ring buffer fills, runtime stops reading from FD
- Kernel pipe/socket buffer fills → sender throttles (TCP window, pipe blocking)
- Automatic flow control prevents OOM

---

## Cross-Platform Normalization

### Line Ending Handling
| Stream Type | Input | Output | Behavior |
|-------------|-------|--------|----------|
| Text (0-3)  | `\r\n` → `\n` | `\n` → `\r\n` (Windows) | Normalized |
| Binary (4-5)| No change | No change | Raw bytes |

### Character Encoding
- **Text Streams**: UTF-8 enforced (validation on read/write)
- **Binary Streams**: No encoding, raw bytes
- **Invalid UTF-8**: Replacement character `` or error result (configurable)

---

## Security Considerations

### 1. Information Leaks via stddbg
**Risk**: Accidental logging of secrets (API keys, passwords, PII)

**Mitigations**:
- **Secure Mode**: `ARIA_SECURE_MODE=1` redirects stddbg to `/dev/null`
- **Compile-Time Stripping**: `cfg(release)` removes stddbg calls from binary

### 2. WildX Hazard (Executable Memory Exfiltration)
**Risk**: Writing JIT-compiled code to stdout/stddato leaks ROP gadgets

**Mitigation**:
- Type system forbids passing `wildx` pointers to stream write methods
- Compiler emits security error at compile time

---

## Implementation Phases

### Phase 6.1: Runtime Stream Infrastructure (Core)
**Estimated**: 2 weeks

1. **Stream Trait Definition** (`src/runtime/stream.cpp`)
   - Base Stream interface
   - DebugStream specialization
   - DataStream specialization

2. **Global Stream Objects** (`src/runtime/io_init.cpp`)
   - Initialize 6 singleton streams
   - Platform-specific FD/HANDLE allocation
   - Environment variable parsing (Windows)

3. **Buffering Implementations**
   - LineBuffer (text streams)
   - BlockBuffer (data streams)
   - AsyncRingBuffer (stddbg)

### Phase 6.2: Platform Backends
**Estimated**: 2 weeks

1. **Linux Backend** (`src/runtime/io_linux.cpp`)
   - `fcntl` FD validation
   - `dup2` for process spawning
   - `splice` for zero-copy
   - `epoll` integration

2. **Windows Backend** (`src/runtime/io_windows.cpp`)
   - `CreatePipe` for anonymous pipes
   - STARTUPINFOEX configuration
   - Named pipe support
   - IOCP integration

### Phase 6.3: High-Level API
**Estimated**: 1 week

1. **Text I/O Methods**
   - `writeLine`, `readLine`
   - UTF-8 validation
   - Line ending normalization

2. **Binary I/O Methods**
   - `writeBytes`, `readBytes`
   - `readInto` (zero-copy wild buffers)

3. **TBB-Aware Methods**
   - `writeTBB8/16/32/64`
   - `readTBB8/16/32/64`
   - Sentinel checking

### Phase 6.4: Process Integration
**Estimated**: 1 week

1. **Spawn Enhancement**
   - Stream redirection configuration
   - Pipe wiring for FDs 3-5
   - Handle passing on Windows

2. **IPC Utilities**
   - `createPipe()` function
   - Pipe reader/writer wrappers

### Phase 6.5: Async Integration
**Estimated**: 1 week

1. **Event Loop Integration**
   - Register FDs 3-5 with epoll/IOCP
   - Coroutine suspension on blocking I/O
   - Resumption on data available

2. **Async Methods**
   - `readAsync`, `writeAsync`
   - Backpressure handling

### Phase 6.6: Testing & Documentation
**Estimated**: 1 week

1. **Unit Tests**
   - Text I/O (line buffering, UTF-8)
   - Binary I/O (zero-copy, raw bytes)
   - TBB serialization
   - Process pipelines

2. **Integration Tests**
   - Multi-process pipelines
   - Async I/O stress tests
   - Cross-platform validation

3. **Documentation**
   - API reference
   - Usage examples
   - Migration guide (3-stream → 6-stream)

---

## Total Estimated Timeline

**Phase 6 (I/O Library)**: 8 weeks  
- Core infrastructure: 2 weeks
- Platform backends: 2 weeks  
- High-level API: 1 week
- Process integration: 1 week
- Async integration: 1 week
- Testing/docs: 1 week

**Dependencies**:
- Phase 4: File I/O (research_004) - provides `openFile()`
- Phase 5: Process Management (research_005) - provides `spawn()`
- Phase 7: Async runtime - provides event loop integration

**Parallel Work**:
- Platform backends can be developed concurrently (Linux/Windows)
- Tests can be written alongside implementation

---

## Success Criteria

1. ✅ All 6 streams functional on Linux and Windows
2. ✅ Zero-copy performance validated (splice on Linux)
3. ✅ Binary pipelines work without corruption
4. ✅ Debug logs don't interfere with stdout
5. ✅ TBB values serialize/deserialize correctly
6. ✅ Async I/O integrates with coroutine scheduler
7. ✅ Cross-platform tests passing (Ubuntu, Windows 11)
8. ✅ Documentation complete with examples

---

## Open Questions / Future Work

1. **stddbg Format**: JSON lines vs custom binary protocol?
2. **Network Streams**: Should sockets implement Stream trait?
3. **Compression**: Should stddato support transparent compression?
4. **Multiplexing**: Should single FD support multiple logical streams?
5. **Security**: Sandboxing for untrusted code (restrict FD access)?

---

## References

- **Research Document**: `/docs/gemini/responses/research_006_modern_streams.txt` (388 lines)
- **Aria Specification**: `/docs/info/aria_specs.txt` (stdio section)
- **Related Research**:
  - research_004: File I/O Library (933 lines)
  - research_005: Process Management (397 lines)
