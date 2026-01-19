# getMemoryUsage()

**Category**: Standard Library → System Diagnostics  
**Syntax**: `getMemoryUsage() -> MemoryInfo`  
**Purpose**: Get current memory usage statistics

---

## Overview

`getMemoryUsage()` returns information about the program's memory consumption.

---

## Syntax

```aria
import std.system;

mem: MemoryInfo = getMemoryUsage();
```

---

## Returns

- `MemoryInfo` - Memory usage statistics

---

## MemoryInfo Structure

```aria
struct MemoryInfo {
    heap_used: i64,       // Bytes allocated on heap
    heap_total: i64,      // Total heap size
    stack_used: i64,      // Stack memory used
    rss: i64,             // Resident set size (physical RAM)
    virtual_mem: i64      // Virtual memory size
}
```

---

## Examples

### Basic Usage

```aria
import std.system;

mem: MemoryInfo = getMemoryUsage();

stdout << "Heap used: $(mem.heap_used) bytes";
stdout << "RSS: $(mem.rss) bytes";
```

### Monitor Memory Growth

```aria
before: MemoryInfo = getMemoryUsage();

// Do memory-intensive work
large_array: []i32 = allocate_large_array();

after: MemoryInfo = getMemoryUsage();

increase: i64 = after.heap_used - before.heap_used;
stdout << "Memory increased by $increase bytes";
```

### Memory Leak Detection

```aria
fn detect_leak() {
    initial: MemoryInfo = getMemoryUsage();
    
    for i in 0..1000 do
        // Allocate without freeing
        data: *Data = allocate_data();
    end
    
    final: MemoryInfo = getMemoryUsage();
    
    when final.heap_used > initial.heap_used * 2 then
        stderr << "Possible memory leak detected!";
    end
}
```

### Format Memory Size

```aria
fn format_bytes(bytes: i64) -> string {
    when bytes < 1024 then
        return "$bytes B";
    elsif bytes < 1024 * 1024 then
        return "$(bytes / 1024) KB";
    elsif bytes < 1024 * 1024 * 1024 then
        return "$(bytes / (1024 * 1024)) MB";
    else
        return "$(bytes / (1024 * 1024 * 1024)) GB";
    end
}

mem: MemoryInfo = getMemoryUsage();
stdout << "Memory used: $(format_bytes(mem.heap_used))";
```

---

## Best Practices

### ✅ DO: Monitor in Long-Running Programs

```aria
loop
    // Do work...
    
    mem: MemoryInfo = getMemoryUsage();
    when mem.rss > MAX_MEMORY then
        logger.warn("High memory usage: $(mem.rss)");
        trigger_gc();
    end
    
    sleep(60);
end
```

### ✅ DO: Profile Memory-Intensive Operations

```aria
before: MemoryInfo = getMemoryUsage();
large_operation();
after: MemoryInfo = getMemoryUsage();

logger.info("Operation memory cost", {
    bytes = after.heap_used - before.heap_used
});
```

---

## Related

- [getActiveConnections()](getActiveConnections.md) - Network stats
- [system_diagnostics](system_diagnostics.md) - System overview

---

**Remember**: Monitor memory in **long-running processes**!
