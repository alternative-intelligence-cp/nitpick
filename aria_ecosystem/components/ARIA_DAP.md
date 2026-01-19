# Aria Debug Adapter Protocol (aria-dap)

**Component Type**: Debug Adapter / Debugger Integration  
**Language**: C++20 (planned)  
**Protocol**: DAP (Debug Adapter Protocol) v1.51  
**Debugger Backend**: LLDB (with GDB fallback)  
**Repository**: Design phase (not yet created)  
**Status**: Design / Specification Phase  
**Version**: v0.0.1-concept

---

## Table of Contents

1. [Overview](#overview)
2. [DAP Architecture](#dap-architecture)
3. [Debugger Backend Integration](#debugger-backend-integration)
4. [Core Features](#core-features)
5. [Custom Formatters for Aria Types](#custom-formatters-for-aria-types)
6. [Breakpoint Management](#breakpoint-management)
7. [6-Stream I/O Debugging](#6-stream-io-debugging)
8. [Integration Points](#integration-points)

---

## Overview

The Aria Debug Adapter (aria-dap) provides debugging capabilities for Aria programs. It implements the Debug Adapter Protocol (DAP), enabling debugging in any DAP-compatible IDE (VSCode, Vim, Emacs, etc.).

### Key Features

- **Standard Debugging**: Breakpoints, stepping, stack traces
- **Variable Inspection**: Watch expressions, hover evaluation
- **Custom Formatters**: Pretty-printing for Result<T>, TBB, streams
- **6-Stream Awareness**: Display stddbg, stddati, stddato separately
- **LLDB Backend**: Leverages LLVM debugger for native code
- **Source-Level Debugging**: Debug Aria source, not generated IR
- **Multi-Process**: Debug shell + child processes simultaneously

---

## DAP Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        ARIA-DAP                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ DAP Protocol Handler                                       ││
│  │  - JSON message parsing                                    ││
│  │  - Request routing (launch, attach, setBreakpoints, etc.)  ││
│  │  - Event generation (stopped, continued, exited)           ││
│  └────────────────────────────────────────────────────────────┘│
│                           ↓                                     │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ Debug Session Manager                                      ││
│  │  - Track debug sessions (one per process)                  ││
│  │  - Manage breakpoints (source → IR mapping)                ││
│  │  - Handle stepping (step over, into, out)                  ││
│  │  - Stack frame management                                  ││
│  └────────────────────────────────────────────────────────────┘│
│                           ↓                                     │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ LLDB Interface                                             ││
│  │  - Launch debuggee via LLDB                                ││
│  │  - Set breakpoints (LLDB commands)                         ││
│  │  - Step execution (next, step, continue)                   ││
│  │  - Evaluate expressions (LLDB eval)                        ││
│  │  - Query variables (frame variable, print)                 ││
│  └────────────────────────────────────────────────────────────┘│
│                           ↓                                     │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ Custom Formatters (Aria Types)                             ││
│  │  - Result<T> formatter (display Ok/Err)                    ││
│  │  - TBB formatter (show range, ERR sentinel)                ││
│  │  - Stream formatter (display FD, buffer state)             ││
│  └────────────────────────────────────────────────────────────┘│
│                           ↓                                     │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ Source Map Manager                                         ││
│  │  - Map Aria source → LLVM IR → machine code                ││
│  │  - Line number → instruction address                       ││
│  │  - Instruction → source location                           ││
│  └────────────────────────────────────────────────────────────┘│
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
                            ↕
                     JSON over stdio
                            ↕
┌─────────────────────────────────────────────────────────────────┐
│                    IDE / EDITOR                                 │
│              (VSCode, Vim, Emacs, etc.)                         │
└─────────────────────────────────────────────────────────────────┘
                            ↕
                         LLDB
                            ↕
┌─────────────────────────────────────────────────────────────────┐
│                 DEBUGGEE (Aria Program)                         │
└─────────────────────────────────────────────────────────────────┘
```

---

## Debugger Backend Integration

### Why LLDB?

**Advantages**:
1. **LLVM Integration**: Aria uses LLVM backend, LLDB understands LLVM IR
2. **Modern Design**: Better C++ API than GDB
3. **Extensibility**: Custom type formatters, Python scripting
4. **Cross-Platform**: Linux, macOS, Windows (with some limitations)

**Alternative**: GDB (fallback if LLDB unavailable)

### LLDB Command Interface

**aria-dap communicates with LLDB via C++ API**:

```cpp
#include <lldb/API/LLDB.h>

// Launch debuggee
lldb::SBDebugger debugger = lldb::SBDebugger::Create();
lldb::SBTarget target = debugger.CreateTarget("bin/my_app");
lldb::SBLaunchInfo launch_info(nullptr);
lldb::SBProcess process = target.Launch(launch_info, error);

// Set breakpoint
lldb::SBBreakpoint bp = target.BreakpointCreateByLocation("main.aria", 10);

// Continue execution
process.Continue();

// When stopped, query variables
lldb::SBFrame frame = process.GetSelectedThread().GetSelectedFrame();
lldb::SBValue var = frame.FindVariable("age");
std::string value = var.GetValue();
```

---

## Core Features

### 1. Launching & Attaching

**DAP Request**: `launch`

**VSCode launch.json**:
```json
{
  "type": "aria",
  "request": "launch",
  "name": "Debug Aria App",
  "program": "${workspaceFolder}/bin/my_app",
  "args": ["--input", "data.txt"],
  "cwd": "${workspaceFolder}",
  "stopOnEntry": false
}
```

**aria-dap Launch Sequence**:
1. Parse launch request
2. Invoke LLDB: `debugger.CreateTarget("bin/my_app")`
3. Set initial breakpoints (if any)
4. Launch: `target.Launch(args, env, ...)`
5. Send `initialized` event to IDE
6. IDE sends `setBreakpoints`, `setExceptionBreakpoints`
7. Send `configurationDone` response
8. Debuggee starts running

---

### 2. Breakpoints

**DAP Request**: `setBreakpoints`

**Example**:
```json
{
  "source": {"path": "/home/user/project/src/main.aria"},
  "breakpoints": [
    {"line": 10},
    {"line": 15, "condition": "age > 18"}
  ]
}
```

**aria-dap Processing**:
1. Map `main.aria:10` → `main.aria:10` (source line in DWARF debug info)
2. LLDB command: `breakpoint set --file main.aria --line 10`
3. Conditional breakpoint: `breakpoint modify --condition "age > 18"`
4. Return breakpoint IDs to IDE

**DWARF Debug Info**:
- Compiler generates DWARF with source line mappings
- LLDB reads DWARF to map Aria source → machine code
- Breakpoint on line 10 → instruction address in executable

---

### 3. Stepping

**DAP Requests**: `next`, `stepIn`, `stepOut`, `continue`

| Command | DAP Request | LLDB Command | Behavior |
|---------|-------------|--------------|----------|
| **Step Over** | `next` | `thread step-over` | Execute current line, don't enter functions |
| **Step Into** | `stepIn` | `thread step-in` | Enter function calls |
| **Step Out** | `stepOut` | `thread step-out` | Return from current function |
| **Continue** | `continue` | `process continue` | Run until next breakpoint or exit |

**Example**:
```aria
func:main = int64() {
    tbb8:age = 25;                // Line 10 (breakpoint here)
    result<string>:data = get();   // Line 11
    if (data.is_err()) {           // Line 12
        pass(1);                   // Line 13
    }
    pass(0);                       // Line 14
}
```

**Debugging Session**:
1. Breakpoint hits at line 10
2. IDE shows line 10 highlighted
3. User presses F10 (Step Over)
4. DAP sends `next` request
5. LLDB steps to line 11
6. DAP sends `stopped` event (reason: "step")
7. IDE highlights line 11

---

### 4. Variable Inspection

**DAP Request**: `scopes` (get variable scopes), `variables` (get variable values)

**Example**:
```aria
func:main = int64() {
    tbb8:age = 25;
    result<string>:data = io.read_file("test.txt");
    // Breakpoint here
    pass(0);
}
```

**IDE Variables Panel**:
```
Local Variables:
  age: 25 (tbb8)
  data: Ok("Hello, World!\n") (result<string>)
```

**DAP Sequence**:
1. IDE requests `scopes` (stackFrameId: 0)
2. DAP returns: `[{"name": "Local", "variablesReference": 1}]`
3. IDE requests `variables` (variablesReference: 1)
4. DAP queries LLDB: `frame variable`
5. LLDB returns: `age = 25`, `data = {...}`
6. DAP applies custom formatters (see below)
7. Returns formatted variables to IDE

---

### 5. Stack Traces

**DAP Request**: `stackTrace`

**Example**:
```aria
func:helper = int64(x: int64) {
    pass(x * 2);  // Breakpoint here
}

func:compute = int64() {
    helper(5);
}

func:main = int64() {
    compute();
    pass(0);
}
```

**Stack Trace**:
```
#0  helper (x=5)              main.aria:10
#1  compute ()                main.aria:14
#2  main ()                   main.aria:18
#3  __libc_start_main ()      (external)
```

**DAP Response**:
```json
{
  "stackFrames": [
    {
      "id": 0,
      "name": "helper",
      "source": {"path": "/path/to/main.aria"},
      "line": 10,
      "column": 4
    },
    {
      "id": 1,
      "name": "compute",
      "source": {"path": "/path/to/main.aria"},
      "line": 14,
      "column": 4
    },
    {
      "id": 2,
      "name": "main",
      "source": {"path": "/path/to/main.aria"},
      "line": 18,
      "column": 4
    }
  ]
}
```

---

### 6. Expression Evaluation

**DAP Request**: `evaluate`

**Use Cases**:
- **Watch Expressions**: Continuously evaluate `age * 2`
- **Hover Evaluation**: Mouse over `age` → show value
- **Debug Console**: Type expressions to evaluate

**Example**:
```json
{
  "expression": "age * 2",
  "frameId": 0,
  "context": "watch"
}
```

**DAP Processing**:
1. LLDB command: `expr age * 2`
2. LLDB returns: `50` (if age = 25)
3. DAP returns to IDE: `{"result": "50", "type": "int64"}`

**Aria-Specific Evaluation**:
```
Expression: data.ok()
Type: string
Value: "Hello, World!\n"

Expression: data.is_err()
Type: bool
Value: false
```

---

## Custom Formatters for Aria Types

### Result<T> Formatter

**LLDB Python Script** (`aria_formatters.py`):
```python
import lldb

def format_result(valobj, internal_dict):
    err = valobj.GetChildMemberWithName('err')
    val = valobj.GetChildMemberWithName('val')
    
    if err.GetValue() == '0x0':  # NULL pointer
        # Success case
        val_str = val.GetValue()
        return f'Ok({val_str})'
    else:
        # Error case
        err_str = err.GetSummary()
        return f'Err({err_str})'

# Register formatter
def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('type summary add -F aria_formatters.format_result AriaResult')
```

**Load in LLDB**:
```
command script import /path/to/aria_formatters.py
```

**Display in Debugger**:
```
Before: data = {err = 0x0, val = 0x7fff..., val_size = 14}
After:  data = Ok("Hello, World!\n")
```

---

### TBB Formatter

**Python Script**:
```python
def format_tbb8(valobj, internal_dict):
    value = valobj.GetValue()
    if value == '-128':
        return 'ERR (overflow/sentinel)'
    else:
        return f'{value} (tbb8, range: -128 to 127)'

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('type summary add -F aria_formatters.format_tbb8 tbb8')
```

**Display**:
```
Before: age = 25
After:  age = 25 (tbb8, range: -128 to 127)

Before: overflow = -128
After:  overflow = ERR (overflow/sentinel)
```

---

### Stream Formatter

**Python Script**:
```python
def format_stream(valobj, internal_dict):
    fd = valobj.GetChildMemberWithName('fd').GetValueAsUnsigned()
    stream_names = {
        0: 'stdin',
        1: 'stdout',
        2: 'stderr',
        3: 'stddbg',
        4: 'stddati',
        5: 'stddato'
    }
    name = stream_names.get(fd, f'FD {fd}')
    return f'Stream({name})'

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('type summary add -F aria_formatters.format_stream AriaTextStream')
```

**Display**:
```
Before: stream = {fd = 3, buffer = 0x..., ...}
After:  stream = Stream(stddbg)
```

---

## Breakpoint Management

### Source-Level Breakpoints

**Mapping**: Aria source line → Machine code address

**DWARF Debug Info**:
```
.debug_line section:
  main.aria:10 → 0x401234 (machine code address)
  main.aria:11 → 0x401240
  main.aria:12 → 0x401260
```

**aria-dap**:
1. User sets breakpoint on `main.aria:10`
2. DAP queries DWARF: line 10 → address 0x401234
3. LLDB sets breakpoint at 0x401234
4. When hit, LLDB reports address 0x401234
5. DAP maps back to `main.aria:10` for IDE display

---

### Conditional Breakpoints

**DAP Request**:
```json
{
  "breakpoints": [
    {
      "line": 10,
      "condition": "age > 18"
    }
  ]
}
```

**LLDB Command**:
```
breakpoint set --file main.aria --line 10
breakpoint modify 1 --condition "age > 18"
```

**Execution**:
- Breakpoint hits every time line 10 is reached
- LLDB evaluates `age > 18`
- If true: Pause execution, notify DAP
- If false: Continue execution

---

### Logpoints (Non-Breaking Breakpoints)

**DAP Request**:
```json
{
  "breakpoints": [
    {
      "line": 10,
      "logMessage": "Age is {age}"
    }
  ]
}
```

**LLDB Command**:
```
breakpoint set --file main.aria --line 10
breakpoint command add 1
  expr printf("Age is %d\n", age)
  continue
DONE
```

**Execution**:
- Breakpoint hits
- LLDB prints "Age is 25"
- Execution continues (no pause)

---

## 6-Stream I/O Debugging

### Challenge

Debugged process has 6 streams:
- FD 0-2: Standard (stdin/stdout/stderr) → Terminal
- FD 3-5: Aria-specific (stddbg/stddati/stddato) → Pipes or files

**Problem**: When debugging, how to see all 6 streams separately?

### Solution: Stream Capture

**aria-dap Setup**:
1. Redirect FD 3-5 to temporary files:
   - FD 3 (stddbg) → `/tmp/aria_debug_stddbg_<pid>`
   - FD 4 (stddati) → (keep as is, for input)
   - FD 5 (stddato) → `/tmp/aria_debug_stddato_<pid>`

2. Monitor these files in background threads

3. Send contents to IDE as custom events

**DAP Custom Event**: `aria/streamOutput`
```json
{
  "event": "aria/streamOutput",
  "body": {
    "stream": "stddbg",
    "output": "Processed 1024 bytes\n"
  }
}
```

**IDE Display**:
```
Debug Console:
  stdout: OK
  stderr: (no errors)
  stddbg: Processed 1024 bytes
```

---

### Multi-Process Debugging

**Scenario**: Debugging aria_shell spawning child process

**Challenge**: Track both shell and child simultaneously

**Solution**: LLDB multi-process support

**LLDB Settings**:
```
settings set target.process.follow-fork-mode child
settings set target.process.stop-on-fork true
```

**DAP Handling**:
1. Shell launches child via `aria_spawn()`
2. LLDB detects fork, pauses both processes
3. DAP sends `stopped` event for both (process IDs: parent, child)
4. IDE shows two debug sessions
5. User can switch between shell and child for debugging

---

## Integration Points

### With Aria Compiler

**Debug Symbols**:
- Compiler generates DWARF debug info (`-g` flag)
- Source line → machine code mappings
- Type information for variables

**Optimization**: 
- Debug builds use `-O0` (no optimization) for accurate stepping
- Release builds use `-O2` but still include debug symbols

---

### With Aria Runtime

**Runtime Symbols**:
- Runtime library compiled with debug symbols
- Can step into `aria_alloc()`, `aria_stdout_write()`, etc.
- Understand runtime internals during debugging

**Stream Capture**:
- Runtime cooperates with debugger for FD 3-5 redirection
- Optional environment variable: `ARIA_DEBUG_STREAMS=1`

---

### With VSCode Extension

**Installation**:
```bash
code --install-extension aria-lang.aria-vscode
```

**Extension provides DAP configuration**:
```json
{
  "debuggers": [
    {
      "type": "aria",
      "label": "Aria Debugger",
      "program": "aria-dap",
      "configurationAttributes": {
        "launch": {
          "required": ["program"],
          "properties": {
            "program": {"type": "string"},
            "args": {"type": "array"},
            "cwd": {"type": "string"},
            "env": {"type": "object"},
            "stopOnEntry": {"type": "boolean"}
          }
        }
      }
    }
  ]
}
```

---

## Configuration

### .ariadaprc (DAP Config)

```json
{
  "dap": {
    "backend": "lldb",  // or "gdb"
    "formatters": {
      "result": true,
      "tbb": true,
      "streams": true
    },
    "multiProcess": {
      "followFork": "child",  // "parent", "child", "both"
      "stopOnFork": true
    },
    "streams": {
      "captureStddbg": true,
      "captureStddato": true
    }
  }
}
```

---

## Related Components

- **[Aria Compiler](ARIA_COMPILER.md)**: Generates debug symbols (DWARF)
- **[aria-lsp](ARIA_LSP.md)**: Complementary IDE integration
- **[Aria Runtime](ARIA_RUNTIME.md)**: Provides symbols for runtime debugging
- **[aria_shell](ARIA_SHELL.md)**: Multi-process debugging with shell + children

---

**Document Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Design specification (implementation pending)
