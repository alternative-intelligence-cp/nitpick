# fork()

**Category**: Standard Library → Process Management  
**Syntax**: `fork() -> Result<ForkResult>`  
**Purpose**: Create child process (Unix fork)

---

## Overview

`fork()` creates a **copy** of the current process. Returns different values in parent and child.

---

## Syntax

```aria
import std.process;

fork_Result: Result<ForkResult> = fork();
```

---

## Returns

- `Result<ForkResult>` - Contains pid information

---

## ForkResult

```aria
enum ForkResult {
    Parent(i32),  // In parent process, child's PID
    Child         // In child process
}
```

---

## Examples

### Basic Fork

```aria
import std.process;

Result: ForkResult = fork()?;

when result is Parent(child_pid) then
    stdout << "Parent: Child PID is $child_pid";
    // Parent code...
elsif result is Child then
    stdout << "Child: I am the child process";
    // Child code...
    exit(0);
end
```

### Fork and Execute

```aria
Result: ForkResult = fork()?;

when result is Parent(child_pid) then
    // Parent waits for child
    stdout << "Parent waiting for child $child_pid";
    wait(child_pid)?;
    stdout << "Child finished";
    
elsif result is Child then
    // Child executes different program
    exec("ls", ["-la"])?;
    exit(0);
end
```

### Multiple Children

```aria
// Fork multiple child processes
children: []i32 = [];

till(4, 1) {
    Result: ForkResult = fork()?;
    
    when result is Parent(child_pid) then
        children.append(child_pid);
    elsif result is Child then
        stdout << "Child $($) working...";
        // Do work...
        exit(0);
    end
end

// Parent waits for all children
till(children.length - 1, 1) {
    wait(children[$])?;
end

stdout << "All children completed";
```

---

## Common Patterns

### Fork-Exec Pattern

```aria
// Common Unix pattern: fork then exec
Result: ForkResult = fork()?;

when result is Parent(child_pid) then
    // Parent continues
    Result: ProcessResult = wait(child_pid)?;
    
elsif result is Child then
    // Child replaces itself with new program
    exec("different_program", ["args"])?;
    exit(1);  // Only reached if exec fails
end
```

### Background Task

```aria
Result: ForkResult = fork()?;

when result is Parent(child_pid) then
    stdout << "Background task started: $child_pid";
    // Parent continues immediately
    
elsif result is Child then
    // Child does background work
    do_long_task();
    exit(0);
end
```

---

## Copy-on-Write

After fork, child has **copy** of parent's memory:

```aria
value: i32 = 10;

Result: ForkResult = fork()?;

when result is Parent(child_pid) then
    value = 20;  // Parent's value
    stdout << "Parent value: $value";  // 20
    wait(child_pid)?;
    
elsif result is Child then
    value = 30;  // Child's value (doesn't affect parent)
    stdout << "Child value: $value";  // 30
    exit(0);
end
```

---

## Error Cases

- Resource limits (can't create more processes)
- Out of memory
- System restrictions

---

## Best Practices

### ✅ DO: Child Must Exit

```aria
Result: ForkResult = fork()?;

when result is Child then
    // Do child work
    process_data();
    exit(0);  // ✅ MUST exit!
end
// Otherwise child continues as second "parent"
```

### ✅ DO: Parent Should Wait

```aria
Result: ForkResult = fork()?;

when result is Parent(child_pid) then
    wait(child_pid)?;  // ✅ Prevent zombie processes
end
```

### ❌ DON'T: Fork in Loops Without Care

```aria
// Dangerous - exponential process explosion!
till(9, 1) {
    fork()?;  // ❌ Creates 2^10 = 1024 processes!
end

// Better
till(9, 1) {
    Result: ForkResult = fork()?;
    when result is Child then
        work();
        exit(0);  // ✅ Child exits
    end
    // Only parent continues loop
end
```

---

## vs spawn()

```aria
// fork() - Duplicates current process
Result: ForkResult = fork()?;
when result is Child then
    // Same program, different path
    exit(0);
end

// spawn() - Starts new program
process: Process = spawn("other_program", args)?;
```

---

## Zombie Processes

```aria
// BAD - creates zombie!
Result: ForkResult = fork()?;

when result is Parent(child_pid) then
    // Parent exits without waiting
    return;  // ❌ Child becomes zombie!
elsif result is Child then
    work();
    exit(0);
end

// GOOD - reap child
when result is Parent(child_pid) then
    wait(child_pid)?;  // ✅ Cleans up child
end
```

---

## Related

- [exec()](exec.md) - Execute command
- [spawn()](spawn.md) - Spawn background process
- [wait()](wait.md) - Wait for child
- [process_management](process_management.md) - Overview

---

**Remember**: Child **must exit()**, parent should **wait()**!
