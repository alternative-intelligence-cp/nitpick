# Aria Debug Adapter Protocol (DAP) Server

## Overview

The Aria DAP server (`aria-dap`) implements the [Debug Adapter Protocol](https://microsoft.github.io/debug-adapter-protocol/) specification, enabling any DAP-compatible editor (VS Code, Vim, Emacs, etc.) to debug Aria programs through a standardized interface.

## Architecture

```
┌──────────────┐                    ┌──────────────┐                    ┌──────────────┐
│              │   JSON-RPC/DAP     │              │    LLDB C++ API    │              │
│   VS Code    │ ←────────────────→ │   aria-dap   │ ←────────────────→ │     LLDB     │
│              │   stdin/stdout     │              │    SBDebugger      │              │
└──────────────┘                    └──────────────┘                    └──────────────┘
                                           │
                                           │ Maps DAP requests to
                                           │ LLDB API calls
                                           ▼
                                    ┌──────────────┐
                                    │              │
                                    │ Aria Program │
                                    │              │
                                    └──────────────┘
```

### Components

1. **DAPServer**: Main server class handling protocol communication
   - Reads JSON-RPC messages from stdin
   - Dispatches to request handlers
   - Writes responses/events to stdout

2. **Request Handlers**: Map DAP commands to LLDB operations
   - `initialize`: Capability negotiation
   - `launch`/`attach`: Start debugging session
   - `setBreakpoints`: Manage breakpoints
   - `continue`/`next`/`stepIn`/`stepOut`: Control execution
   - `threads`/`stackTrace`: Inspect program state
   - `scopes`/`variables`: Variable inspection
   - `evaluate`: Expression evaluation (REPL)

3. **Event Thread**: Monitors LLDB events and sends DAP events
   - `stopped`: Process stopped (breakpoint, step, pause)
   - `exited`: Process exited with code
   - `terminated`: Debugging session ended
   - `breakpoint`: Breakpoint verified/changed

4. **LLDB Integration**: Uses LLDB C++ API
   - `SBDebugger`: Main debugger instance
   - `SBTarget`: Executable target
   - `SBProcess`: Running process
   - `SBThread`: Thread control and inspection
   - `SBFrame`: Stack frame inspection
   - `SBValue`: Variable values with formatter support

## Building

### Dependencies

- **LLDB**: Required for DAP server functionality
  - Ubuntu: `sudo apt install liblldb-dev`
  - Arch: `sudo pacman -S lldb`
  - macOS: `brew install llvm` (includes LLDB)

- **nlohmann/json**: JSON parsing library (header-only)
  - Ubuntu: `sudo apt install nlohmann-json3-dev`
  - Arch: `sudo pacman -S nlohmann-json`
  - macOS: `brew install nlohmann-json`

### Build Commands

```bash
cd /path/to/aria
cmake -S . -B build
cmake --build build --target aria-dap -j$(nproc)
```

If LLDB is not found:
```
-- LLDB library not found - DAP server will not be built
```

If LLDB is found:
```
-- Building DAP server: aria-dap
```

The executable will be at: `build/aria-dap`

## Usage

### VS Code Integration

Create `.vscode/launch.json`:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "aria",
      "request": "launch",
      "name": "Debug Aria Program",
      "program": "${workspaceFolder}/build/my_program",
      "args": [],
      "cwd": "${workspaceFolder}",
      "stopAtEntry": false,
      "preLaunchTask": "build"
    }
  ]
}
```

Create `.vscode/tasks.json`:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "build",
      "type": "shell",
      "command": "ariac",
      "args": ["${workspaceFolder}/src/main.aria", "-o", "build/my_program"],
      "group": {
        "kind": "build",
        "isDefault": true
      }
    }
  ]
}
```

Install the Aria VS Code extension (see `tools/vscode-extension/`).

### Manual Launch

Start DAP server manually (for testing or custom integration):

```bash
./build/aria-dap
```

The server reads DAP messages from stdin and writes responses/events to stdout. All logging goes to stderr.

### Protocol Example

**Initialize Request:**
```json
{
  "seq": 1,
  "type": "request",
  "command": "initialize",
  "arguments": {
    "clientID": "vscode",
    "adapterID": "aria",
    "pathFormat": "path",
    "linesStartAt1": true,
    "columnsStartAt1": true
  }
}
```

**Initialize Response:**
```json
{
  "seq": 1,
  "type": "response",
  "request_seq": 1,
  "command": "initialize",
  "success": true,
  "body": {
    "supportsConfigurationDoneRequest": true,
    "supportsEvaluateForHovers": true,
    "supportsValueFormattingOptions": true,
    "supportTerminateDebuggee": true
  }
}
```

**Initialized Event:**
```json
{
  "seq": 2,
  "type": "event",
  "event": "initialized"
}
```

## Features

### Supported DAP Requests

| Request | Status | Description |
|---------|--------|-------------|
| `initialize` | ✅ Complete | Capability negotiation |
| `launch` | ✅ Complete | Launch executable with arguments |
| `attach` | ✅ Complete | Attach to running process by PID |
| `configurationDone` | ✅ Complete | Configuration complete, resume if stopped |
| `disconnect` | ✅ Complete | End debugging session |
| `setBreakpoints` | ✅ Complete | Set/remove file:line breakpoints |
| `setExceptionBreakpoints` | 🚧 Stub | Exception handling (future) |
| `continue` | ✅ Complete | Resume execution |
| `next` | ✅ Complete | Step over (line) |
| `stepIn` | ✅ Complete | Step into function |
| `stepOut` | ✅ Complete | Step out of function |
| `pause` | ✅ Complete | Interrupt execution |
| `threads` | ✅ Complete | List threads |
| `stackTrace` | ✅ Complete | Get call stack |
| `scopes` | ✅ Complete | Get variable scopes (locals, args) |
| `variables` | ✅ Complete | Inspect variables |
| `evaluate` | ✅ Complete | REPL expression evaluation |

### Supported DAP Events

| Event | Status | Description |
|-------|--------|-------------|
| `initialized` | ✅ Complete | Sent after initialize response |
| `stopped` | ✅ Complete | Process stopped (breakpoint, step, etc.) |
| `exited` | ✅ Complete | Process exited with code |
| `terminated` | ✅ Complete | Debugging session terminated |
| `breakpoint` | ✅ Complete | Breakpoint verified/changed |

### Aria-Specific Features

- **TBB Type Formatters**: Automatically displays ERR sentinels semantically
  - `tbb8` ERR (-128) → "ERR"
  - `tbb32` overflow → "(OVERFLOW)"
  
- **GC Pointer Inspection**: Shows object headers as synthetic children
  - `type_id`, `size_class`, `mark_bit`, `pinned_bit`, etc.
  
- **Result<T> Formatting**: Pretty-prints Result types
  - `Ok(value)` when successful
  - `Error(code)` when failed

## Implementation Details

### Threading Model

1. **Main Thread**: Runs protocol loop
   - Reads DAP requests from stdin
   - Dispatches to handlers
   - Writes responses to stdout

2. **Event Thread**: Monitors LLDB events
   - Calls `SBListener::WaitForEvent()` with 1-second timeout
   - Converts LLDB events to DAP events
   - Sends events via stdout

3. **Synchronization**: Mutex protects LLDB operations
   - `std::lock_guard` in all handlers
   - Prevents race conditions between main and event threads

### Breakpoint Management

- **DAP ID Mapping**: Assigns unique IDs to breakpoints
- **File-Based Clearing**: When setting breakpoints, clears all existing breakpoints in that file
- **Verification**: Reports whether breakpoint resolved to actual code location
- **LLDB Integration**: Uses `SBTarget::BreakpointCreateByLocation()`

### Variable Inspection

- **Scopes**: Two scopes per frame
  - **Locals** (reference: `frame_id * 1000 + 1`)
  - **Arguments** (reference: `frame_id * 1000 + 2`)
  
- **Variable References**: Encoded as integers
  - `0` = no children (primitive value)
  - `> 0` = has children (struct, array, etc.)
  
- **Formatters**: Leverages LLDB formatters from Phase 7.4.2
  - TBB types show semantic values
  - GC pointers expose object headers
  - Result types show Ok/Error

### Error Handling

- **LLDB Errors**: Checked with `SBError::Fail()`
- **DAP Errors**: Set `success = false` and include error message
- **Graceful Degradation**: Handles missing threads, invalid frames, etc.

## Testing

### Unit Tests

Run protocol tests (message parsing, JSON serialization):

```bash
./build/tests/test_runner 2>&1 | grep dap_
```

Tests include:
- Message lifecycle and cleanup
- Breakpoint/StackFrame/Variable structures
- JSON serialization of capabilities
- Request/response/event formatting
- Protocol message parsing

### Integration Tests

Debug a simple Aria program:

```aria
// test.aria
fn main() {
    let x: tbb32 = 10;
    let y: tbb32 = 20;
    let z: tbb32 = x + y;
    print(z);
}
```

Compile with debug info:
```bash
ariac test.aria -g -o test_program
```

Create DAP launch request:
```json
{
  "seq": 1,
  "type": "request",
  "command": "launch",
  "arguments": {
    "program": "./test_program"
  }
}
```

Send via stdin and observe responses.

### VS Code Testing

1. Install Aria VS Code extension
2. Open Aria workspace
3. Set breakpoint in editor (F9)
4. Press F5 to start debugging
5. Observe:
   - Breakpoint hit
   - Variables panel shows locals
   - Call stack panel shows frames
   - Debug console allows evaluation

## Troubleshooting

### LLDB Not Found

**Problem**: CMake reports "LLDB library not found"

**Solution**: Install LLDB development package:
- Ubuntu: `sudo apt install liblldb-dev lldb`
- Arch: `sudo pacman -S lldb`
- macOS: `brew install llvm`

Verify installation:
```bash
find /usr -name "liblldb.so*" 2>/dev/null
ldconfig -p | grep lldb
```

### nlohmann/json Not Found

**Problem**: Compilation fails with "nlohmann/json.hpp: No such file"

**Solution**: Install JSON library:
- Ubuntu: `sudo apt install nlohmann-json3-dev`
- Arch: `sudo pacman -S nlohmann-json`
- macOS: `brew install nlohmann-json`

Or use as submodule:
```bash
cd /path/to/aria
git submodule add https://github.com/nlohmann/json external/json
```

Update CMakeLists.txt:
```cmake
include_directories(external/json/include)
```

### DAP Connection Issues

**Problem**: VS Code reports "Cannot connect to runtime process"

**Solution**:
1. Check aria-dap is in PATH or extension configured correctly
2. Verify executable has execute permission: `chmod +x build/aria-dap`
3. Check stderr logs: `aria-dap 2>dap.log`
4. Ensure program path is absolute in launch.json

### Breakpoints Not Verified

**Problem**: Breakpoints show gray circle (unverified)

**Solution**:
1. Ensure program compiled with `-g` flag (DWARF debug info)
2. Check source path matches exactly (case-sensitive)
3. Verify breakpoint on executable line (not comment/blank)
4. Check LLDB can resolve location:
   ```bash
   lldb ./program
   (lldb) b main.aria:10
   ```

### Variable Inspection Shows "<no value>"

**Problem**: Variables panel shows empty or "<no value>"

**Solution**:
1. Ensure stopped at valid frame (not optimized out)
2. Check variable is in scope at current line
3. Verify LLDB formatters loaded (see Phase 7.4.2 docs)
4. Try manual evaluation in debug console: `x`

## Future Enhancements

### Phase 7.4.4: Web UI (Weeks 13-14)
- Embedded HTTP/WebSocket server
- Browser-based debugger UI
- Memory map visualization canvas
- Source view with syntax highlighting

### Phase 7.4.5: Async/Await Debugging (Week 15)
- Logical call stack for async functions
- Promise chain visualization
- State machine frame reconstruction
- Awaiter variable inspection

### Phase 7.4.6: Advanced Features (Week 16)
- Memory map heatmap (GC regions, WildX, etc.)
- Data breakpoints (watch expressions)
- Reverse debugging (if LLDB supports)
- Remote debugging via GDB RSP

## References

- [Debug Adapter Protocol Specification](https://microsoft.github.io/debug-adapter-protocol/)
- [LLDB C++ API Documentation](https://lldb.llvm.org/cpp_reference/html/)
- [VS Code Debug Extension Guide](https://code.visualstudio.com/api/extension-guides/debugger-extension)
- Phase 7.4.2: TBB Type Formatters (`docs/debugger_formatters.md`)
- Phase 7.4.1: DWARF Emission (`docs/dwarf_emission.md`)
- Research: `docs/gemini/responses/request_036_debugger.txt`

## Contributing

When adding new DAP features:

1. Update `DAPServer::initializeHandlers()` with new command
2. Add handler method to `dap_server.h` and `dap_server.cpp`
3. Map DAP semantics to LLDB API calls
4. Add unit tests to `test_dap_server.cpp`
5. Update capabilities in `handleInitialize()`
6. Document in this file

Report issues: https://github.com/aria-lang/aria/issues
