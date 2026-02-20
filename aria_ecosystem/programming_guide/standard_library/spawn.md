# spawn()

**Category**: Standard Library → Process Management  
**Syntax**: `spawn(command: string, args: []string) -> Result<Process>`  
**Purpose**: Start background process (non-blocking)

---

## Overview

`spawn()` starts a process in the **background** and returns immediately without waiting.

---

## Syntax

```aria
import std.process;

process: Result<Process> = spawn("long_running_task", ["arg1", "arg2"]);
```

---

## Parameters

- **command** (`string`) - Command to execute
- **args** (`[]string`) - Command arguments

---

## Returns

- `Result<Process>` - Process handle for managing the spawned process

---

## Process Structure

```aria
struct Process {
    pid: i32,                          // Process ID
    fn wait() -> Result<ProcessResult> // Wait for completion
    fn kill() -> Result<void>          // Terminate process
    fn is_alive() -> bool              // Check if running
}
```

---

## Examples

### Basic Spawn

```aria
import std.process;

// Start process in background
process: Process = spawn("sleep", ["10"])?;

stdout << "Process started with PID: $(process.pid)";
stdout << "Doing other work...";

// Wait for it to finish
Result: ProcessResult = process.wait()?;
stdout << "Process completed with exit code: $(result.exit_code)";
```

### Run Server

```aria
// Start server in background
server: Process = spawn("python3", ["-m", "http.server", "8000"])?;

stdout << "Server started on port 8000";

// Do other work...

// Shutdown server when done
server.kill()?;
```

### Multiple Parallel Processes

```aria
// Start multiple tasks in parallel
task1: Process = spawn("./process_file1.sh")?;
task2: Process = spawn("./process_file2.sh")?;
task3: Process = spawn("./process_file3.sh")?;

stdout << "All tasks running in parallel...";

// Wait for all to complete
result1: ProcessResult = task1.wait()?;
result2: ProcessResult = task2.wait()?;
result3: ProcessResult = task3.wait()?;

stdout << "All tasks completed";
```

### Check if Running

```aria
process: Process = spawn("long_task")?;

loop
    when not process.is_alive() then
        break;
    end
    
    stdout << "Still running...";
    sleep(1);
end

Result: ProcessResult = process.wait()?;
```

---

## Process Management

### Wait for Completion

```aria
process: Process = spawn("build.sh")?;

// Do other work...

// Wait when ready
Result: ProcessResult = process.wait()?;

when result.exit_code != 0 then
    stderr << "Build failed: $(result.stderr)";
end
```

### Kill Process

```aria
process: Process = spawn("infinite_loop.sh")?;

// After some time...
when timeout_occurred then
    process.kill()?;
    stdout << "Process terminated";
end
```

### Non-Blocking Check

```aria
process: Process = spawn("task.sh")?;

// Check without waiting
when process.is_alive() then
    stdout << "Task is still running";
else
    Result: ProcessResult = process.wait()?;
    stdout << "Task finished";
end
```

---

## Error Cases

- Command not found
- Permission denied
- Resource limits (too many processes)
- Invalid arguments

---

## Best Practices

### ✅ DO: Always Wait or Kill

```aria
process: Process = spawn("task")?;

// Either wait
Result: ProcessResult = process.wait()?;

// Or kill
process.kill()?;

// Don't leave orphan processes!
```

### ✅ DO: Use for Parallel Work

```aria
// Good - parallel processing
processes: []Process = [];

till(large_file_list.length - 1, 1) {
    p: Process = spawn("process_file.sh", [large_file_list[$]])?;
    processes.append(p);
end

// Wait for all
till(processes.length - 1, 1) {
    processes[$].wait()?;
end
```

### ❌ DON'T: Forget to Wait

```aria
process: Process = spawn("important_task")?;
// ❌ Program exits, task might not finish!

// Better
defer process.wait()?;  // ✅ Ensures completion
```

---

## vs exec()

```aria
// exec() - Blocks until complete
Result: ProcessResult = exec("task", args)?;  // Waits here
stdout << "Task done";

// spawn() - Returns immediately
process: Process = spawn("task", args)?;  // Continues immediately
stdout << "Task running in background";
process.wait()?;  // Wait when ready
```

---

## Related

- [exec()](exec.md) - Execute and wait for completion
- [fork()](fork.md) - Fork current process
- [wait()](wait.md) - Wait for child process
- [process_management](process_management.md) - Overview

---

**Remember**: `spawn()` returns **immediately** - don't forget to wait!
