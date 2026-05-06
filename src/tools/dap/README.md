# adap - Aria Debug Adapter Protocol Library

A reusable C++17 library implementing the Debug Adapter Protocol (DAP) for building
debugger integrations.

## Features

- **Full JSON Implementation**: Built-in JSON parsing and serialization
- **DAP Message Types**: Request, Response, Event handling
- **Extensible Handler System**: Register custom command handlers
- **Protocol Tracing**: Debug logging to stddbg (FD 3)
- **TBB-Safe Counters**: Overflow-safe integer arithmetic
- **C FFI**: Call from Aria runtime or other languages

## Usage

### C++ API

```cpp
#include <adap/dap_protocol.hpp>

using namespace aria::adap;

int main() {
    DAPServer server;

    // Set capabilities
    Capabilities caps;
    caps.supportsConfigurationDoneRequest = true;
    caps.supportsEvaluateForHovers = true;
    server.set_capabilities(caps);

    // Register handlers
    server.register_handler("launch", [](const Request& req, Response& resp) {
        // Handle launch request
        auto program = req.arguments["program"].as_string();
        resp.success = true;
    });

    server.register_handler("setBreakpoints", [](const Request& req, Response& resp) {
        JsonArray breakpoints;
        // Process breakpoints...
        resp.body["breakpoints"] = breakpoints;
    });

    // Run the server (blocks until disconnect)
    return server.run();
}
```

### C FFI

```c
#include <adap/dap_protocol.hpp>

void handle_launch(const char* command,
                   const char* arguments_json,
                   char** response_json,
                   void* user_data) {
    // Parse arguments, handle launch
    *response_json = strdup("{\"success\": true}");
}

int main() {
    AriaDAPServerHandle server = aria_dap_new(0, 1);

    aria_dap_register_handler(server, "launch", handle_launch, NULL);

    int result = aria_dap_run(server);

    aria_dap_free(server);
    return result;
}
```

## DAP Types

### Capabilities

Configure debugger capabilities advertised to the client:

```cpp
Capabilities caps;
caps.supportsConfigurationDoneRequest = true;
caps.supportsFunctionBreakpoints = true;
caps.supportsConditionalBreakpoints = true;
caps.supportsEvaluateForHovers = true;
```

### Source

Represents a source file:

```cpp
Source src;
src.name = "main.aria";
src.path = "/project/src/main.aria";
```

### Breakpoint

Represents a breakpoint:

```cpp
Breakpoint bp;
bp.id = 1;
bp.verified = true;
bp.line = 42;
bp.source = src;
```

### StackFrame

Represents a stack frame:

```cpp
StackFrame frame;
frame.id = 1;
frame.name = "main";
frame.source = src;
frame.line = 10;
frame.column = 1;
```

### Variable

Represents a variable for inspection:

```cpp
Variable var;
var.name = "counter";
var.value = "42";
var.type = "int64";
var.variablesReference = 0;  // No children
```

## JSON API

Built-in JSON value type with parsing and serialization:

```cpp
// Create values
JsonValue null_val;
JsonValue bool_val(true);
JsonValue int_val(42);
JsonValue str_val("hello");
JsonValue arr_val(JsonArray{1, 2, 3});

// Objects
JsonObject obj;
obj["name"] = "test";
obj["value"] = 42;
JsonValue obj_val(obj);

// Parse JSON
auto parsed = JsonValue::parse("{\"key\": \"value\"}");
if (parsed) {
    std::string key = (*parsed)["key"].as_string();
}

// Serialize
std::string json = obj_val.to_string();
```

## Protocol Tracing

Enable protocol tracing for debugging:

```cpp
DAPServer server;
server.enable_tracing(true);  // Logs to FD 3 (stddbg)
```

Trace output (NDJSON format):

```
<- {"seq":1,"type":"request","command":"initialize",...}
-> {"seq":1,"type":"response","request_seq":1,"success":true,...}
```

## Hex-Stream Integration

The library uses Aria's Hex-Stream I/O model:

| FD | Name    | Usage                          |
|----|---------|--------------------------------|
| 0  | stdin   | DAP input (JSON-RPC)           |
| 1  | stdout  | DAP output (JSON-RPC)          |
| 2  | stderr  | Error messages                 |
| 3  | stddbg  | Protocol traces                |

## Building

```bash
mkdir build && cd build
cmake ..
make

# Run tests
./test_adap
```

## Integration with Nitpick Compiler

The adap library is used by the Nitpick debugger (`npkc debug`) to provide
IDE integration via DAP. See the Nitpick compiler documentation for details.

## License

Copyright (c) 2025 Nitpick Language Project
