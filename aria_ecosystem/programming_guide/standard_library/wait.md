# wait()

**Category**: Standard Library → Process Management  
**Syntax**: `wait(pid: i32) -> Result<ProcessResult>`  
**Purpose**: Wait for child process to complete

---

## Overview

`wait()` blocks until a child process finishes and returns its exit status.

---

## Syntax

```aria
import std.process;

Result: Result<ProcessResult> = wait(child_pid);
```

---

## Parameters

- **pid** (`i32`) - Child process ID to wait for

---

## Returns

- `Result<ProcessResult>` - Exit code, stdout, stderr

---

## Examples

### Wait for Forked Child

```aria
import std.process;

fork_Result: ForkResult = fork()?;

when fork_result is Parent(child_pid) then
    stdout << "Parent waiting for child $child_pid";
    
    // Wait for child to finish
    Result: ProcessResult = wait(child_pid)?;
    
    stdout << "Child exited with code: $(result.exit_code)";
    
elsif fork_result is Child then
    stdout << "Child doing work...";
    // Do work...
    exit(0);
end
```

### Check Exit Status

```aria
fork_Result: ForkResult = fork()?;

when fork_result is Parent(child_pid) then
    Result: ProcessResult = wait(child_pid)?;
    
    when result.exit_code == 0 then
        stdout << "Child succeeded";
    else
        stderr << "Child failed with code $(result.exit_code)";
    end
    
elsif fork_result is Child then
    success: bool = do_task();
    exit(when success then 0 else 1);
end
```

### Wait for Multiple Children

```aria
children: []i32 = [];

// Fork multiple children
till(4, 1) {
    fork_Result: ForkResult = fork()?;
    
    when fork_result is Parent(child_pid) then
        children.append(child_pid);
    elsif fork_result is Child then
        work($);
        exit(0);
    end
end

// Wait for all children
till(children.length - 1, 1) {
    Result: ProcessResult = wait(children[$])?;
    stdout << "Child $(children[$]) exited: $(result.exit_code)";
end

stdout << "All children completed";
```

---

## Preventing Zombie Processes

```aria
// Without wait - child becomes ZOMBIE!
fork_Result: ForkResult = fork()?;

when fork_result is Parent(child_pid) then
    // Parent continues without waiting
    return;  // ❌ Zombie process created!
elsif fork_result is Child then
    work();
    exit(0);
end

// With wait - properly cleaned up
when fork_result is Parent(child_pid) then
    wait(child_pid)?;  // ✅ Reaps child process
end
```

---

## Non-Blocking Wait

```aria
// Wait with timeout (if available)
fork_Result: ForkResult = fork()?;

when fork_result is Parent(child_pid) then
    // Try waiting with timeout
    Result: ?ProcessResult = wait_timeout(child_pid, 5)?;
    
    when result == nil then
        stderr << "Child didn't finish in 5 seconds";
        kill(child_pid)?;  // Force kill
    end
end
```

---

## Error Cases

- No child with given PID
- Child already reaped
- Permission denied
- Interrupted by signal

---

## Best Practices

### ✅ DO: Always Wait for Children

```aria
fork_Result: ForkResult = fork()?;

when fork_result is Parent(child_pid) then
    wait(child_pid)?;  // ✅ Prevents zombies
end
```

### ✅ DO: Check Exit Codes

```aria
ProcessResult:Result = wait(child_pid)?;

when result.exit_code != 0 then
    stderr << "Child process failed";
    fail("Child error");
end
```

### ✅ DO: Use defer for Cleanup

```aria
fork_Result: ForkResult = fork()?;

when fork_result is Parent(child_pid) then
    defer wait(child_pid)?;  // ✅ Ensures cleanup
    
    // Do other work...
end
```

### ❌ DON'T: Forget to Wait

```aria
fork_Result: ForkResult = fork()?;

when fork_result is Parent(child_pid) then
    // ❌ Forgot to wait - zombie process!
end
```

---

## Wait for Any Child

```aria
// Wait for any child (PID -1)
Result: ProcessResult = wait(-1)?;
stdout << "A child exited: $(result.exit_code)";
```

---

## Related

- [fork()](fork.md) - Create child process
- [exec()](exec.md) - Execute command
- [spawn()](spawn.md) - Spawn background process
- [process_management](process_management.md) - Overview

---

**Remember**: **Always wait()** to prevent zombie processes!
