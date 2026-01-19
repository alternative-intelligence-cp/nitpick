# RAII (Resource Acquisition Is Initialization)

**Category**: Memory Model → Patterns  
**Concept**: Automatic resource management  
**Philosophy**: Resources tied to object lifetime

---

## What is RAII?

**RAII** (Resource Acquisition Is Initialization) is a pattern where resources are **acquired in constructors** and **released in destructors**, ensuring automatic cleanup.

---

## Core Principle

Resources are **bound to object lifetime**:

```aria
{
    file: File = File::open("data.txt");  // Acquire
    // Use file
}  // file.close() called automatically - Release
```

---

## Why RAII?

### Manual Management (Error-Prone)

```aria
// ❌ Easy to forget cleanup
file: File = open("data.txt");
process(file);
file.close();  // What if we forget this?

// ❌ What if error occurs?
file: File = open("data.txt");
process(file);  // Error here - file never closed!
file.close();
```

### RAII (Automatic)

```aria
// ✅ Automatic cleanup
{
    file: File = File::open("data.txt");
    process(file);
}  // Automatically closed, even on error!
```

---

## How RAII Works

### 1. Acquisition in Constructor

```aria
struct File {
    handle: FileHandle
}

impl File {
    fn open(path: string) -> File {
        handle: FileHandle = system_open(path);
        return File{handle: handle};  // Resource acquired
    }
}
```

### 2. Release in Destructor

```aria
impl Drop for File {
    fn drop() {
        system_close(self.handle);  // Resource released
    }
}
```

### 3. Automatic Cleanup

```aria
{
    file: File = File::open("data.txt");
    // Use file
}  // file.drop() called automatically
```

---

## Common RAII Resources

### File Handles

```aria
fn process_file() {
    file: File = pass File::open("data.txt");
    content: string = pass file.read();
    
    // Process content
}  // file closed automatically
```

### Database Connections

```aria
struct Connection {
    handle: DbHandle
}

impl Drop for Connection {
    fn drop() {
        self.handle.disconnect();
    }
}

fn query() {
    conn: Connection = Connection::connect();
    Result: Result = conn.query("SELECT * FROM users");
}  // Connection closed automatically
```

### Locks

```aria
struct MutexGuard<T> {
    lock: &Mutex<T>
}

impl<T> Drop for MutexGuard<T> {
    fn drop() {
        self.lock.unlock();  // Unlock automatically
    }
}

fn critical_section() {
    guard: MutexGuard = mutex.lock();
    // Critical section
}  // Automatically unlocked
```

### Memory Allocations

```aria
struct Vec<T> {
    data: *T,
    len: i32,
    capacity: i32
}

impl<T> Drop for Vec<T> {
    fn drop() {
        aria_free(self.data);  // Free memory automatically
    }
}

fn example() {
    vec: Vec<i32> = Vec::new();
    vec.push(1);
    vec.push(2);
}  // Memory freed automatically
```

---

## RAII with Defer

Aria's `defer` provides RAII-like behavior:

```aria
fn process() {
    file: File = pass open("data.txt");
    defer file.close();  // Guaranteed to run
    
    // Complex logic here
    // Even if error occurs, file.close() runs
}
```

---

## RAII Guarantees

### 1. No Resource Leaks

```aria
fn example() {
    file1: File = File::open("a.txt");
    file2: File = File::open("b.txt");
    file3: File = File::open("c.txt");
    
    // Even if we forget to close them
}  // All automatically closed
```

### 2. Exception Safety

```aria
fn process() {
    file: File = File::open("data.txt");
    
    Result: Data = pass parse(file);  // Error here
    
    // file.close() STILL called during stack unwinding
}
```

### 3. Deterministic Cleanup

```aria
{
    resource: Resource = acquire();
    // Use resource
}  // Cleanup happens HERE, not later
```

---

## Implementing RAII

### Basic Pattern

```aria
struct Resource {
    handle: Handle
}

impl Resource {
    // Acquire in constructor
    fn new() -> Resource {
        handle: Handle = acquire_handle();
        return Resource{handle: handle};
    }
}

impl Drop for Resource {
    // Release in destructor
    fn drop() {
        release_handle(self.handle);
    }
}
```

### With Error Handling

```aria
struct SafeFile {
    handle: FileHandle?
}

impl SafeFile {
    fn open(path: string) -> SafeFile? {
        handle: FileHandle? = try_open(path);
        
        when handle == nil then
            return nil;
        end
        
        return SafeFile{handle: handle};
    }
}

impl Drop for SafeFile {
    fn drop() {
        when self.handle != nil then
            close(self.handle);
        end
    }
}
```

---

## Common Patterns

### Scope Guard

```aria
struct ScopeGuard {
    cleanup: fn()
}

impl Drop for ScopeGuard {
    fn drop() {
        self.cleanup();
    }
}

fn example() {
    guard: ScopeGuard = ScopeGuard{
        cleanup: || {
            stdout << "Cleaning up\n";
        }
    };
    
    // Do work
}  // cleanup() called automatically
```

### Lock Guard

```aria
struct LockGuard {
    mutex: &Mutex
}

impl Drop for LockGuard {
    fn drop() {
        self.mutex.unlock();
    }
}

fn synchronized() {
    guard: LockGuard = mutex.lock();
    
    // Critical section - mutex locked
    
}  // Mutex automatically unlocked
```

### Transaction Guard

```aria
struct Transaction {
    committed: bool,
    connection: &Connection
}

impl Drop for Transaction {
    fn drop() {
        if !self.committed {
            self.connection.rollback();  // Auto-rollback if not committed
        }
    }
}

fn example() {
    tx: Transaction = Transaction::begin();
    
    tx.execute("INSERT ...");
    tx.commit();  // Sets committed = true
}  // If commit wasn't called, rollback happens
```

---

## Best Practices

### ✅ DO: Use RAII for All Resources

```aria
// Good: Automatic cleanup
file: File = File::open("data.txt");
conn: Connection = Connection::connect();
lock: Guard = mutex.lock();
// All cleaned up automatically
```

### ✅ DO: Make Resources Non-Copyable

```aria
// Good: Prevent double-free
struct File {
    handle: FileHandle
    // No Clone trait = can't copy
}
```

### ✅ DO: Use defer for Simple Cases

```aria
// Good: Simple cleanup
file: File = pass open("data.txt");
defer file.close();
```

### ❌ DON'T: Mix RAII and Manual Cleanup

```aria
// Wrong: Confusing
file: File = File::open("data.txt");
file.close();  // Manual close
// Destructor will close again! Double-close bug!

// Right: Let RAII handle it
file: File = File::open("data.txt");
// Automatically closed
```

### ❌ DON'T: Forget to Implement Drop

```aria
// Wrong: Resource leak
struct Connection {
    handle: DbHandle
}
// No Drop implementation - handle never closed!

// Right: Implement Drop
impl Drop for Connection {
    fn drop() {
        self.handle.close();
    }
}
```

---

## Real-World Examples

### Database Transaction

```aria
struct Transaction {
    conn: &Connection,
    active: bool
}

impl Transaction {
    fn begin(conn: &Connection) -> Transaction {
        conn.execute("BEGIN");
        return Transaction{conn: conn, active: true};
    }
    
    fn commit() {
        self.conn.execute("COMMIT");
        self.active = false;
    }
}

impl Drop for Transaction {
    fn drop() {
        when self.active then
            self.conn.execute("ROLLBACK");
        end
    }
}

// Usage
fn transfer_money(from: i32, to: i32, amount: f64) {
    tx: Transaction = Transaction::begin(&db);
    
    pass db.execute("UPDATE accounts SET balance = balance - ? WHERE id = ?", amount, from);
    pass db.execute("UPDATE accounts SET balance = balance + ? WHERE id = ?", amount, to);
    
    tx.commit();
}  // Auto-rollback if commit not reached
```

### Temporary Directory

```aria
struct TempDir {
    path: string
}

impl TempDir {
    fn create() -> TempDir {
        path: string = format("/tmp/aria-{}", random_id());
        create_directory(path);
        return TempDir{path: path};
    }
}

impl Drop for TempDir {
    fn drop() {
        remove_directory_recursive(self.path);
    }
}

fn test() {
    tmp: TempDir = TempDir::create();
    
    // Write test files to tmp.path
    // Run tests
}  // Directory automatically deleted
```

---

## RAII vs Manual Management

### C (Manual)

```c
FILE* f = fopen("data.txt", "r");
// ... use file ...
fclose(f);  // Must remember!
```

### C++ (RAII)

```cpp
{
    std::fstream f("data.txt");
    // ... use file ...
}  // Automatically closed
```

### Aria (RAII + Defer)

```aria
{
    file: File = File::open("data.txt");
    // ... use file ...
}  // Automatically closed

// Or with defer
file: File = pass open("data.txt");
defer file.close();
```

---

## Related Topics

- [Defer](defer.md) - Scope-based cleanup
- [Stack](stack.md) - Stack-based lifetimes
- [Allocation](allocation.md) - Memory management

---

**Remember**: RAII = **automatic resource management** - acquire in constructor, release in destructor, guaranteed cleanup!
