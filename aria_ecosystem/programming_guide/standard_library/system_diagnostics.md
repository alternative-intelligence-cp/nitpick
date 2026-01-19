# System Diagnostics

**Category**: Standard Library → System Diagnostics  
**Purpose**: Overview of system monitoring and diagnostics

---

## Overview

Aria provides system diagnostics for monitoring resource usage and performance.

---

## Available Functions

### Memory Diagnostics

```aria
import std.system;

// Get memory usage
mem: MemoryInfo = getMemoryUsage();

stdout << "Heap: $(mem.heap_used) / $(mem.heap_total)";
stdout << "RSS: $(mem.rss)";
```

### Network Diagnostics

```aria
// Get active connections
connections: []ConnectionInfo = getActiveConnections();

stdout << "Active connections: $(connections.length())";
```

---

## Common Patterns

### Health Check

```aria
fn health_check() -> HealthStatus {
    mem: MemoryInfo = getMemoryUsage();
    conns: []ConnectionInfo = getActiveConnections();
    
    status: HealthStatus = {
        memory_ok = mem.rss < MAX_MEMORY,
        connections_ok = conns.length() < MAX_CONNECTIONS,
        healthy = true
    };
    
    status.healthy = status.memory_ok and status.connections_ok;
    
    return status;
}
```

### Periodic Monitoring

```aria
fn monitor_system() {
    loop
        mem: MemoryInfo = getMemoryUsage();
        conns: []ConnectionInfo = getActiveConnections();
        
        logger.info("System stats", {
            heap_mb = mem.heap_used / (1024 * 1024),
            rss_mb = mem.rss / (1024 * 1024),
            connections = conns.length()
        });
        
        sleep(60);  // Every minute
    end
}
```

### Resource Alerts

```aria
fn check_resources() {
    mem: MemoryInfo = getMemoryUsage();
    
    // Memory threshold: 80% of total
    threshold: i64 = (mem.heap_total * 80) / 100;
    
    when mem.heap_used > threshold then
        logger.warn("High memory usage", {
            used_mb = mem.heap_used / (1024 * 1024),
            total_mb = mem.heap_total / (1024 * 1024),
            percent = (mem.heap_used * 100) / mem.heap_total
        });
    end
}
```

---

## Metrics Collection

```aria
struct SystemMetrics {
    timestamp: i64,
    memory_used: i64,
    connections: i32
}

fn collect_metrics() -> SystemMetrics {
    mem: MemoryInfo = getMemoryUsage();
    conns: []ConnectionInfo = getActiveConnections();
    
    return {
        timestamp = now(),
        memory_used = mem.heap_used,
        connections = conns.length()
    };
}

// Collect over time
metrics: []SystemMetrics = [];

loop
    metrics.append(collect_metrics());
    
    // Keep last 100 samples
    when metrics.length() > 100 then
        metrics = metrics[metrics.length()-100..];
    end
    
    sleep(5);
end
```

---

## Related

- [getMemoryUsage()](getMemoryUsage.md) - Memory statistics
- [getActiveConnections()](getActiveConnections.md) - Network connections

---

**Remember**: **Monitor system resources** in production!
