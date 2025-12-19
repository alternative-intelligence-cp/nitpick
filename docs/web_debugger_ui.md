# Aria Debugger Web UI

## Overview

The Aria Debugger Web UI provides a browser-based debugging interface with real-time updates via WebSocket. It offers an alternative to VS Code integration for debugging Aria programs, with rich visualizations including memory maps and TBB type gauges.

## Architecture

```
┌──────────────┐           WebSocket            ┌──────────────┐
│              │  ←─────────────────────────→   │              │
│   Browser    │   JSON-RPC DAP Protocol        │  Web Server  │
│   (UI)       │                                 │  (C++)       │
│              │                                 │              │
└──────────────┘                                 └──────┬───────┘
                                                        │
                                                        │ Local
                                                        │ Bridge
                                                        ▼
                                                 ┌──────────────┐
                                                 │              │
                                                 │  DAP Server  │
                                                 │              │
                                                 └──────┬───────┘
                                                        │
                                                        │ LLDB API
                                                        ▼
                                                 ┌──────────────┐
                                                 │              │
                                                 │  LLDB/Target │
                                                 │              │
                                                 └──────────────┘
```

## Features

### Implemented (Phase 7.4.4)

- ✅ **Static HTTP Server**: Serves HTML/CSS/JS debugger UI
- ✅ **WebSocket Infrastructure**: Real-time communication foundation
- ✅ **UI Layout**: Source view, call stack, variables, breakpoints, console
- ✅ **Control Buttons**: Continue, pause, step over/into/out, restart, stop
- ✅ **Keyboard Shortcuts**: F5 (continue), F10 (step over), F11 (step into)
- ✅ **Console**: Expression evaluation with REPL
- ✅ **Dark Theme**: VS Code-inspired color scheme
- ✅ **Responsive Design**: Flexible panels and scrolling

### In Development

- 🚧 **Full HTTP Server**: Requires cpp-httplib integration
- 🚧 **WebSocket Server**: Requires ws library or IXWebSocket
- 🚧 **Source File Loading**: Fetch and display actual source files
- 🚧 **Syntax Highlighting**: Color-coded Aria syntax
- 🚧 **Interactive Breakpoints**: Click line numbers to toggle
- 🚧 **Variable Tree**: Expandable nested structures
- 🚧 **Memory Map Canvas**: Visual representation of GC/Wild/WildX regions
- 🚧 **TBB Gauges**: Graphical symmetric range visualization

## Building

### Dependencies

**Required:**
- LLDB (for DAP backend)
- nlohmann/json (for JSON parsing)

**Optional (for full HTTP/WebSocket support):**
- cpp-httplib (header-only HTTP server)
- IXWebSocket or websocketpp (WebSocket library)

### Build Commands

```bash
cd /path/to/aria

# Basic build (DAP only)
cmake -S . -B build
cmake --build build --target aria-dap

# With HTTP/WebSocket libraries (when available)
cmake -DENABLE_WEB_UI=ON -S . -B build
cmake --build build --target aria-dap
```

### Installing HTTP Libraries

**cpp-httplib:**
```bash
# Header-only, just clone into external/
git clone https://github.com/yhirose/cpp-httplib external/cpp-httplib

# Or install system-wide
sudo apt install libhttplib-dev  # Ubuntu
sudo pacman -S cpp-httplib        # Arch
brew install cpp-httplib          # macOS
```

**WebSocket Library:**
```bash
# IXWebSocket
git clone https://github.com/machinezone/IXWebSocket external/IXWebSocket

# Or websocketpp
sudo apt install libwebsocketpp-dev
```

## Usage

### Starting the Web UI

```bash
# Launch DAP server with web UI
./build/aria-dap --web-ui --port 8080

# Then open browser to:
http://localhost:8080
```

### Debugging Workflow

1. **Start Debugger**: Open web UI in browser
2. **Set Breakpoints**: Click line numbers in source view
3. **Launch Program**: Click "Continue" or press F5
4. **Inspect State**: View variables, call stack when stopped
5. **Step Through**: Use step buttons or keyboard shortcuts
6. **Evaluate Expressions**: Type in console and press Enter
7. **View Memory**: Click "Memory Map" button (coming soon)

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| F5 | Continue |
| Shift+F5 | Restart |
| F10 | Step Over |
| F11 | Step Into |
| Shift+F11 | Step Out |

### Console Commands

The debug console accepts Aria expressions:

```
> x                  // Print variable value
> x + y              // Evaluate expression
> obj.field          // Access struct fields
> array[0]           // Index into arrays
> foo(42)            // Call functions (future)
```

## UI Components

### Source View

- **Line Numbers**: Click to toggle breakpoints
- **Current Line**: Highlighted in teal
- **Breakpoints**: Red dot in gutter
- **Syntax Highlighting**: Color-coded tokens (when implemented)

### Call Stack Panel

- Shows function call hierarchy
- Click frame to inspect that scope
- Active frame highlighted in blue
- Shows source file and line number

### Variables Panel

- Lists variables in current scope
- Expandable tree for nested structures (coming soon)
- Shows value and type
- TBB types display semantic values

### Breakpoints Panel

- Lists all set breakpoints
- Shows file and line
- Click × to remove breakpoint
- Indicates verification status (green/red)

### Console

- Scroll back through messages
- Color-coded output:
  - **Blue**: Info messages
  - **Green**: Success/results
  - **Red**: Errors
- REPL for expression evaluation

## Implementation Details

### WebSocket Protocol

The web UI communicates with the DAP server using standard DAP JSON-RPC over WebSocket:

**Request Example:**
```json
{
  "seq": 1,
  "type": "request",
  "command": "continue",
  "arguments": {
    "threadId": 1
  }
}
```

**Response Example:**
```json
{
  "seq": 2,
  "type": "response",
  "request_seq": 1,
  "command": "continue",
  "success": true,
  "body": {
    "allThreadsContinued": true
  }
}
```

**Event Example:**
```json
{
  "seq": 10,
  "type": "event",
  "event": "stopped",
  "body": {
    "reason": "breakpoint",
    "threadId": 1,
    "allThreadsStopped": true
  }
}
```

### File Structure

```
tools/debugger/ui/
├── index.html      # Main UI structure
├── style.css       # VS Code-inspired styling
└── debugger.js     # Client-side logic

include/tools/debugger/
└── web_server.h    # WebServer class declaration

src/tools/debugger/
└── web_server.cpp  # HTTP/WebSocket server implementation
```

### Memory Map Visualization

The memory map canvas (coming in Phase 7.4.6) will display:

- **GC Heap**: Green regions for garbage-collected memory
- **Wild**: Orange regions for unmanaged memory
- **WildX**: Purple regions for executable memory
- **Stack**: Blue regions for stack frames

Each region is clickable to show hex dump and object headers.

### TBB Type Visualization

TBB types will be rendered with gauges showing:

- **Symmetric Range**: Visual bar from -127 to +127 (for tbb8)
- **Current Value**: Marker on the gauge
- **ERR Sentinel**: Red indicator when ERR
- **Overflow**: Warning when outside valid range

Example (HTML generation):

```html
<div class="tbb-gauge">
  <div class="gauge-bar">
    <div class="gauge-fill" style="width: 75%"></div>
    <div class="gauge-marker" style="left: 75%">42</div>
  </div>
  <div class="gauge-labels">
    <span>-127</span>
    <span>0</span>
    <span>+127</span>
  </div>
</div>
```

## Current Status

**Phase 7.4.4 Status**: Infrastructure Complete

The web UI infrastructure is in place with:
- Complete HTML/CSS/JS debugger interface
- WebSocket communication protocol defined
- UI component structure ready
- Event handling framework

**Next Steps**:
1. Integrate cpp-httplib for HTTP server
2. Add WebSocket library for real-time updates
3. Implement source file loading and display
4. Add syntax highlighting
5. Connect to DAP server backend
6. Test end-to-end debugging flow

## Integration with VS Code

The web UI complements VS Code debugging:

**Use Web UI When:**
- Need memory map visualization
- Want TBB gauge displays
- Prefer browser-based workflow
- Debugging on remote server (via SSH tunnel)

**Use VS Code When:**
- Prefer integrated IDE experience
- Need code editing while debugging
- Want IntelliSense and refactoring
- Using VS Code workspace already

Both interfaces connect to the same DAP server backend, so you can switch between them.

## Troubleshooting

### "Web UI Under Development" Page

**Problem**: Browser shows placeholder page instead of full UI

**Solution**: Full HTTP/WebSocket server requires additional libraries:
1. Install cpp-httplib and WebSocket library
2. Rebuild with `-DENABLE_WEB_UI=ON`
3. For now, use VS Code DAP integration instead

### WebSocket Connection Fails

**Problem**: Console shows "Disconnected from debugger"

**Solution**:
1. Ensure aria-dap is running with `--web-ui` flag
2. Check firewall allows port 8080
3. Verify WebSocket URL matches server address
4. Check aria-dap stderr logs for errors

### UI Not Loading

**Problem**: Blank page or 404 error

**Solution**:
1. Verify `tools/debugger/ui/` directory exists
2. Check `--static-dir` flag points to correct path
3. Ensure index.html has correct permissions
4. Check browser console for JavaScript errors

### Breakpoints Not Working

**Problem**: Click line numbers but breakpoints don't set

**Solution**:
1. Ensure WebSocket connected
2. Check source file path matches program
3. Verify program compiled with `-g` (debug info)
4. Try setting breakpoint via console: `setBreakpoints`

## Future Enhancements

### Phase 7.4.5: Async/Await Debugging
- Logical call stack for async functions
- Promise chain visualization
- Awaiter state inspection

### Phase 7.4.6: Memory Map Visualization
- Interactive canvas with zoomable regions
- Object header decoding on hover
- GC statistics overlay
- Heap fragmentation heatmap

### Phase 7.5: Editor Extensions
- VS Code extension marketplace package
- Vim/Neovim DAP configuration
- Emacs dap-mode integration

## Contributing

To add new web UI features:

1. Update `tools/debugger/ui/` HTML/CSS/JS
2. Add corresponding handlers in `web_server.cpp`
3. Extend DAP protocol if needed
4. Test in multiple browsers
5. Update this documentation

## References

- [Debug Adapter Protocol](https://microsoft.github.io/debug-adapter-protocol/)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib)
- [IXWebSocket](https://github.com/machinezone/IXWebSocket)
- Phase 7.4.3: DAP Server (`docs/dap_server.md`)
- Phase 7.4.2: LLDB Formatters (`docs/debugger_formatters.md`)
- Research: `docs/gemini/responses/request_036_debugger.txt`

## License

Same as Aria compiler (see main LICENSE file).
