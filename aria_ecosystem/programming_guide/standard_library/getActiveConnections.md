# getActiveConnections()

**Category**: Standard Library → System Diagnostics  
**Syntax**: `getActiveConnections() -> []ConnectionInfo`  
**Purpose**: Get list of active network connections

---

## Overview

`getActiveConnections()` returns information about all active network connections in the program.

---

## Syntax

```aria
import std.system;

connections: []ConnectionInfo = getActiveConnections();
```

---

## Returns

- `[]ConnectionInfo` - Array of active connection information

---

## ConnectionInfo Structure

```aria
struct ConnectionInfo {
    local_addr: string,    // Local address
    local_port: i32,       // Local port
    remote_addr: string,   // Remote address
    remote_port: i32,      // Remote port
    state: string,         // Connection state (ESTABLISHED, LISTEN, etc.)
    protocol: string       // TCP, UDP, etc.
}
```

---

## Examples

### List Connections

```aria
import std.system;

connections: []ConnectionInfo = getActiveConnections();

for conn in connections do
    stdout << "$(conn.protocol) $(conn.local_addr):$(conn.local_port) -> $(conn.remote_addr):$(conn.remote_port) [$(conn.state)]";
end
```

### Count by State

```aria
connections: []ConnectionInfo = getActiveConnections();

established: i32 = 0;
listening: i32 = 0;

for conn in connections do
    when conn.state == "ESTABLISHED" then
        established += 1;
    elsif conn.state == "LISTEN" then
        listening += 1;
    end
end

stdout << "Established: $established, Listening: $listening";
```

### Find Connections to Specific Host

```aria
target_host: string = "192.168.1.100";

connections: []ConnectionInfo = getActiveConnections();

matching: []ConnectionInfo = connections.filter(fn(conn) -> bool {
    return conn.remote_addr == target_host;
});

stdout << "Connections to $target_host: $(matching.length())";
```

---

## Best Practices

### ✅ DO: Monitor Connection Leaks

```aria
fn check_connection_leak() {
    connections: []ConnectionInfo = getActiveConnections();
    
    when connections.length() > MAX_CONNECTIONS then
        logger.warn("Too many connections: $(connections.length())");
        close_idle_connections();
    end
}
```

### ✅ DO: Debug Network Issues

```aria
connections: []ConnectionInfo = getActiveConnections();

for conn in connections do
    when conn.state == "CLOSE_WAIT" then
        logger.warn("Stuck connection", {
            remote = "$(conn.remote_addr):$(conn.remote_port)"
        });
    end
end
```

---

## Related

- [getMemoryUsage()](getMemoryUsage.md) - Memory stats
- [system_diagnostics](system_diagnostics.md) - System overview

---

**Remember**: Monitor connections to prevent **resource leaks**!
