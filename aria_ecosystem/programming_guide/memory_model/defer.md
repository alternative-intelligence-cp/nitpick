# The `defer` Keyword (RAII Cleanup)

**Category**: Memory Model → RAII  
**Keyword**: `defer`  
**Purpose**: Automatic resource cleanup at scope exit, preventing leaks

---

## Overview

The `defer` keyword ensures code executes when leaving the current scope, **no matter how you exit** (normal return, early return, error, panic). This implements RAII (Resource Acquisition Is Initialization) for Aria.

**Philosophy**: Allocate resource → immediately schedule cleanup → use safely

---

## Basic Syntax

```aria
defer <statement>;
```

The deferred statement executes when the enclosing scope exits, in **reverse order** of declaration.

---

## Simple Examples

### Example 1: File Cleanup

```aria
func:readConfig = Result<obj>(string:path) {
    wild File@:file = openFile(path);
    if (file == NULL) { fail(1); }
    
    // Schedule cleanup immediately after allocation
    defer file->close();
    
    // Safe to use file - cleanup guaranteed
    obj:config = file->readJSON();
    pass(config);
    
    // file->close() runs here automatically
};
```

### Example 2: Memory Deallocation

```aria
func:processData = Result<int64>(uint64:size) {
    wild int64@:buffer = aria.alloc<int64>(size);
    if (buffer == NULL) { fail(1); }
    
    // Guarantee free() even on early return
    defer aria.free(buffer);
    
    // Process data...
    if (someError) {
        fail(2);  // defer still runs!
    }
    
    buffer[0] = compute();
    pass(buffer[0]);
    
    // aria.free(buffer) runs here
};
```

---

## Execution Order

Defers execute in **LIFO** (Last-In-First-Out) order - reverse of declaration:

```aria
func:example = result() {
    defer print("First");   // Runs 3rd
    defer print("Second");  // Runs 2nd
    defer print("Third");   // Runs 1st
    
    print("Body");
    pass(NIL);
};

// Output:
// Body
// Third
// Second
// First
```

This matches resource cleanup order: clean up in reverse of acquisition.

---

## Common Patterns

### Pattern 1: Allocate → Defer → Use

```aria
func:pattern = result() {
    // 1. Allocate resource
    wild int64@:ptr = aria.alloc<int64>(1000);
    if (ptr == NULL) { fail(1); }
    
    // 2. Immediately defer cleanup
    defer aria.free(ptr);
    
    // 3. Use safely - cleanup guaranteed
    ptr[0] = 42;
    pass(NIL);
};
```

### Pattern 2: Multiple Resources

```aria
func:multipleResources = result() {
    wild File@:input = openFile("input.txt");
    if (input == NULL) { fail(1); }
    defer input->close();  // Cleanup 2nd
    
    wild File@:output = openFile("output.txt");
    if (output == NULL) { fail(2); }
    defer output->close();  // Cleanup 1st
    
    // Copy data...
    copyFile(input, output);
    
    pass(NIL);
    
    // Executes:
    // 1. output->close()
    // 2. input->close()
};
```

### Pattern 3: Lock/Unlock

```aria
func:criticalSection = result() {
    mutex.lock();
    defer mutex.unlock();  // Guaranteed unlock
    
    // Critical section code...
    if (condition) {
        fail(1);  // Unlocks before returning!
    }
    
    modifySharedData();
    pass(NIL);
    
    // mutex.unlock() runs here
};
```

### Pattern 4: Nested Resources with Error Handling

```aria
func:complexSetup = result() {
    wild Connection@:db = connectDB("localhost");
    if (db == NULL) { fail(1); }
    defer db->disconnect();
    
    wild Transaction@:tx = db->beginTransaction();
    if (tx == NULL) { fail(2); }  // db still disconnects!
    defer tx->rollback();
    
    // Do work...
    if (!processRecords(tx)) {
        fail(3);  // Both tx and db clean up!
    }
    
    tx->commit();
    pass(NIL);
    
    // Executes:
    // 1. tx->rollback()
    // 2. db->disconnect()
};
```

---

## Advanced Usage

### Defer with Closures

```aria
func:closureExample = result() {
    int64:counter = 0;
    
    // Defer can capture variables
    defer {
        counter++;
        print(`Final counter: &{counter}`);
    };
    
    counter = 100;
    pass(NIL);
    
    // Prints: Final counter: 101
};
```

### Conditional Defer

```aria
func:conditionalCleanup = result(bool:useResource) {
    wild int64@:ptr = NULL;
    
    if (useResource) {
        ptr = aria.alloc<int64>(100);
        if (ptr == NULL) { fail(1); }
        defer aria.free(ptr);  // Only defers if allocated
    }
    
    // Use ptr if allocated...
    pass(NIL);
};
```

### Defer in Loops

```aria
func:loopDefer = result() {
    // ⚠️ Be careful: defer in loop defers once per iteration!
    for (int64:i = 0; i < 10; i++) {
        wild int64@:temp = aria.alloc<int64>(1);
        defer aria.free(temp);  // Runs at end of EACH iteration
        
        temp[0] = i;
    }
    pass(NIL);
};
```

---

## Guaranteed Execution Paths

Defer runs on **ALL** exit paths:

### Path 1: Normal Return

```aria
func:normal = result() {
    defer print("Cleanup");
    pass(NIL);  // defer runs
};
```

### Path 2: Early Return

```aria
func:earlyReturn = result() {
    defer print("Cleanup");
    
    if (condition) {
        pass(42);  // defer runs before this return
    }
    
    pass(0);  // defer runs before this return too
};
```

### Path 3: Error Return (fail)

```aria
func:errorReturn = result() {
    defer print("Cleanup");
    
    if (error) {
        fail(1);  // defer runs before fail
    }
    
    pass(NIL);
};
```

### Path 4: Panic/Crash (Future)

```aria
func:panicPath = result() {
    defer print("Cleanup runs even on panic!");
    
    panic("Critical error");  // defer still runs
};
```

---

## Memory Management Patterns

### File Handles

```aria
func:processFile = Result<string>(string:path) {
    wild File@:file = openFile(path);
    if (file == NULL) { fail(1); }
    defer file->close();
    
    string:content = file->readAll();
    pass(content);
};
```

### Network Connections

```aria
func:fetchData = Result<obj>(string:url) {
    wild Socket@:conn = connect(url);
    if (conn == NULL) { fail(1); }
    defer conn->close();
    
    obj:data = conn->receive();
    pass(data);
};
```

### Temporary Files

```aria
func:processTemp = result() {
    string:tempPath = createTempFile();
    defer deleteFile(tempPath);
    
    writeData(tempPath, data);
    processData(tempPath);
    
    pass(NIL);
    // Temp file auto-deleted
};
```

### Locks and Mutexes

```aria
func:threadSafe = result() {
    globalMutex.lock();
    defer globalMutex.unlock();
    
    // Critical section - mutex always released
    sharedCounter++;
    
    pass(NIL);
};
```

---

## Comparison with Other Languages

### Aria (defer)

```aria
func:aria = result() {
    wild int64@:ptr = aria.alloc<int64>(100);
    defer aria.free(ptr);
    // Use ptr...
    pass(NIL);
};
```

### Go (defer)

```go
func aria() error {
    ptr := make([]int64, 100)
    defer cleanup(ptr)
    // Use ptr...
    return nil
}
```

### Rust (Drop trait)

```rust
fn aria() -> Result<()> {
    let ptr = Box::new([0; 100]);
    // Automatic drop at scope exit
    // Use ptr...
    Ok(())
}
```

### C++ (RAII destructor)

```cpp
int aria() {
    std::unique_ptr<int[]> ptr(new int[100]);
    // Automatic destructor at scope exit
    // Use ptr...
    return 0;
}
```

### Python (with statement)

```python
def aria():
    with open("file.txt") as f:
        # File auto-closed at block exit
        data = f.read()
    return data
```

---

## Best Practices

### ✅ Defer Immediately After Allocation

```aria
// GOOD: Defer right after acquire
wild File@:file = openFile(path);
defer file->close();
// Use file...
```

### ✅ Use defer for All Resources

```aria
// Files
defer file->close();

// Memory
defer aria.free(ptr);

// Locks
defer mutex.unlock();

// Connections
defer conn->disconnect();

// Transactions
defer tx->rollback();
```

### ✅ Reverse Order Matches Dependency

```aria
// Open dependencies in order
wild DB@:db = connectDB();
defer db->close();  // Close 2nd

wild Transaction@:tx = db->beginTx();
defer tx->end();  // Close 1st

// Cleanup order:
// 1. tx->end()    (child)
// 2. db->close()  (parent)
```

---

## Common Mistakes

### ❌ Forgetting defer

```aria
// WRONG: No cleanup on error path!
func:leak = result() {
    wild int64@:ptr = aria.alloc<int64>(100);
    
    if (error) {
        fail(1);  // Leaked ptr!
    }
    
    aria.free(ptr);  // Only runs on success path
    pass(NIL);
}

// CORRECT: defer guarantees cleanup
func:safe = result() {
    wild int64@:ptr = aria.alloc<int64>(100);
    defer aria.free(ptr);  // Runs on ALL paths
    
    if (error) {
        fail(1);  // ptr freed before fail
    }
    
    pass(NIL);
}
```

### ❌ Defer in Wrong Scope

```aria
// WRONG: defer in loop scope
func:wrong = result() {
    for (int64:i = 0; i < 10; i++) {
        wild File@:file = openFile(`file&{i}.txt`);
        defer file->close();  // Runs at end of EACH iteration
        // Might want to keep files open...
    }
    pass(NIL);
}

// CORRECT: defer in function scope if needed
func:correct = result() {
    wild File@[10]:files;
    
    for (int64:i = 0; i < 10; i++) {
        files[i] = openFile(`file&{i}.txt`);
        // Don't defer yet
    }
    
    // Defer all at function scope
    for (int64:i = 0; i < 10; i++) {
        defer files[i]->close();  // All close at function exit
    }
    
    // Use all files...
    pass(NIL);
}
```

### ❌ Deferring Before Null Check

```aria
// WRONG: defer before checking NULL
func:danger = result() {
    wild File@:file = openFile(path);
    defer file->close();  // ❌ What if file is NULL?
    
    if (file == NULL) { fail(1); }
    pass(NIL);
}

// CORRECT: check first, then defer
func:safe = result() {
    wild File@:file = openFile(path);
    
    if (file == NULL) { fail(1); }
    defer file->close();  // ✅ Only defer if valid
    
    pass(NIL);
}
```

---

## Performance Considerations

### Zero Overhead

Defer compiles to the same code as manual cleanup:

```aria
// This defer...
func:withDefer = result() {
    wild int64@:ptr = aria.alloc<int64>(100);
    defer aria.free(ptr);
    pass(NIL);
}

// ...compiles identically to:
func:manual = result() {
    wild int64@:ptr = aria.alloc<int64>(100);
    aria.free(ptr);
    pass(NIL);
}
```

The compiler inserts cleanup code at every exit point. **No runtime cost.**

### Multiple Exit Paths

Defer shines when you have many return paths:

```aria
// Without defer: Must repeat cleanup
func:manual = result() {
    wild int64@:ptr = aria.alloc<int64>(100);
    
    if (cond1) {
        aria.free(ptr);  // Must repeat
        fail(1);
    }
    
    if (cond2) {
        aria.free(ptr);  // Must repeat
        fail(2);
    }
    
    aria.free(ptr);  // Must repeat
    pass(NIL);
}

// With defer: Write once
func:deferred = result() {
    wild int64@:ptr = aria.alloc<int64>(100);
    defer aria.free(ptr);  // Single cleanup
    
    if (cond1) { fail(1); }
    if (cond2) { fail(2); }
    
    pass(NIL);
}
```

---

## Related Topics

- [wild Memory](allocation.md) - Manual memory management
- [result Type](../types/result.md) - Error handling with pass/fail
- [File I/O](../io_system/files.md) - File handle management
- [Mutexes](../advanced_features/concurrency.md) - Lock management
- [GC Memory](allocation.md) - Automatic memory management

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Line 144  
**Key Principle**: Defer = RAII without destructors, explicit and clear
