# Process Management

**Category**: Standard Library → Process Management  
**Purpose**: Overview of process creation and management

---

## Overview

Aria provides comprehensive process management through `std.process`:

- **exec()** - Execute command and wait
- **spawn()** - Run process in background
- **fork()** - Create child process (Unix)
- **wait()** - Wait for child completion

---

## Quick Comparison

| Function | Blocks? | Returns | Use Case |
|----------|---------|---------|----------|
| `exec()` | ✅ Yes | ProcessResult | Run and wait for output |
| `spawn()` | ❌ No | Process | Background tasks |
| `fork()` | ❌ No | ForkResult | Duplicate current process |
| `wait()` | ✅ Yes | ProcessResult | Reap child process |

---

## When to Use What

### exec() - Simple Command Execution

```aria
// Run command, get output
Result: ProcessResult = exec("git", ["status"])?;
stdout << result.stdout;
```

**Use when**: Simple scripts, getting command output, synchronous execution

---

### spawn() - Background Tasks

```aria
// Start server in background
server: Process = spawn("python3", ["-m", "http.server"])?;
// Continue working...
server.kill()?;  // Stop when done
```

**Use when**: Servers, parallel tasks, long-running processes

---

### fork() - Process Duplication

```aria
// Create child process
Result: ForkResult = fork()?;

when result is Parent(child_pid) then
    wait(child_pid)?;
elsif result is Child then
    // Child does different work
    exit(0);
end
```

**Use when**: Unix-style process creation, custom process management

---

### wait() - Reap Children

```aria
// Wait for forked child
Result: ProcessResult = wait(child_pid)?;
```

**Use when**: After fork(), preventing zombies, synchronizing with children

---

## Common Patterns

### Run and Capture Output

```aria
Result: ProcessResult = exec("ls", ["-la", "/home"])?;

when result.exit_code == 0 then
    files: []string = result.stdout.split("\n");
    process_files(files);
else
    stderr << "Command failed: $(result.stderr)";
end
```

---

### Parallel Processing

```aria
// Spawn multiple tasks
tasks: []Process = [];

till(files.length - 1, 1) {
    p: Process = spawn("process_file.sh", [files[$]])?;
    tasks.append(p);
end

// Wait for all
till(tasks.length - 1, 1) {
    tasks[$].wait()?;
end
```

---

### Fork-Exec Pattern

```aria
fork_Result: ForkResult = fork()?;

when fork_result is Parent(child_pid) then
    Result: ProcessResult = wait(child_pid)?;
    stdout << "Child finished";
    
elsif fork_result is Child then
    // Replace child with different program
    exec("other_program", ["args"])?;
    exit(1);  // Only if exec fails
end
```

---

### Pipeline

```aria
// Create pipeline: cat file.txt | grep pattern | wc -l

// Method 1: Shell
Result: ProcessResult = exec("sh", ["-c", "cat file.txt | grep pattern | wc -l"])?;

// Method 2: Manual piping (more control)
cat: Process = spawn("cat", ["file.txt"])?;
grep: Process = spawn("grep", ["pattern"])?;
wc: Process = spawn("wc", ["-l"])?;

// Connect pipes (implementation details depend on system)
connect_stdout_to_stdin(cat, grep)?;
connect_stdout_to_stdin(grep, wc)?;

Result: ProcessResult = wc.wait()?;
```

---

## Process Lifecycle

```
┌─────────┐
│ Parent  │
└────┬────┘
     │ spawn() or fork()
     ├──────────┬────────────┐
     ▼          ▼            ▼
┌─────────┐ ┌─────────┐ ┌─────────┐
│ Child 1 │ │ Child 2 │ │ Child 3 │
└────┬────┘ └────┬────┘ └────┬────┘
     │           │            │
     │ exit(0)   │ exit(0)    │ exit(0)
     ▼           ▼            ▼
┌─────────────────────────────────┐
│ Parent wait()s for all children │
└─────────────────────────────────┘
```

---

## Best Practices

### ✅ DO: Check Exit Codes

```aria
ProcessResult:Result = exec(command, args)?;

when result.exit_code != 0 then
    stderr << "Command failed: $(result.stderr)";
    fail("Execution error");
end
```

### ✅ DO: Clean Up Processes

```aria
process: Process = spawn("task")?;
defer process.kill()?;  // Ensure cleanup

// Or wait for completion
defer process.wait()?;
```

### ✅ DO: Handle Errors

```aria
Result: Result<ProcessResult> = exec("risky_command", args);

when result is Err(msg) then
    log_error("Process error: $msg");
    // Fallback or recovery
end
```

### ❌ DON'T: Create Zombie Processes

```aria
// BAD
fork_Result: ForkResult = fork()?;
when fork_result is Parent(child_pid) then
    // Forgot to wait!  // ❌
end

// GOOD
when fork_result is Parent(child_pid) then
    wait(child_pid)?;  // ✅
end
```

---

## Security Considerations

### Avoid Shell Injection

```aria
// DANGEROUS
user_input: string = get_user_input();
exec("sh", ["-c", "rm $user_input"])?;  // ❌ UNSAFE!

// SAFE
exec("rm", [user_input])?;  // ✅ Arguments properly escaped
```

---

## Related

- [exec()](exec.md) - Execute and wait
- [spawn()](spawn.md) - Background processes
- [fork()](fork.md) - Fork process
- [wait()](wait.md) - Wait for children

---

**Remember**: Always **wait()** for children to prevent zombies!
