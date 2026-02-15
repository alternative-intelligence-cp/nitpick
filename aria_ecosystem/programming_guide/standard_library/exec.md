# exec()

**Category**: Standard Library → Process Management  
**Syntax**: `exec(command: string, args: []string) -> Result<ProcessResult>`  
**Purpose**: Execute command and wait for completion

---

## Overview

`exec()` runs a command with arguments, **waits** for it to finish, and returns the result.

---

## Syntax

```aria
import std.process;

Result: Result<ProcessResult> = exec("ls", ["-la", "/home"]);
```

---

## Parameters

- **command** (`string`) - Command to execute
- **args** (`[]string`) - Command arguments

---

## Returns

- `Result<ProcessResult>` - Contains exit code, stdout, stderr

---

## ProcessResult Structure

```aria
struct ProcessResult {
    exit_code: i32,     // Exit code (0 = success)
    stdout: string,     // Standard output
    stderr: string      // Standard error
}
```

---

## Examples

### Basic Command

```aria
import std.process;

Result: ProcessResult = exec("echo", ["Hello, World!"])?;

stdout << "Output: $(result.stdout)";
stdout << "Exit code: $(result.exit_code)";
```

### Check Exit Code

```aria
Result<ProcessResult>:Result = exec("grep", ["pattern", "file.txt"])?;

when result.exit_code == 0 then
    stdout << "Found:\n$(result.stdout)";
else
    stderr << "Not found or error";
end
```

### Capture Errors

```aria
ProcessResult:Result = exec("git", ["status"])?;

when result.exit_code != 0 then
    stderr << "Git error: $(result.stderr)";
    fail("Git command failed");
end

stdout << result.stdout;
```

### Run Script

```aria
Result: ProcessResult = exec("python3", ["script.py", "arg1", "arg2"])?;

stdout << "Script output: $(result.stdout)";
```

---

## Common Commands

### List Files

```aria
Result: ProcessResult = exec("ls", ["-la"])?;
stdout << result.stdout;
```

### Git Operations

```aria
// Git status
status: ProcessResult = exec("git", ["status"])?;

// Git commit
commit: ProcessResult = exec("git", ["commit", "-m", "Update"])?;
```

### Build Commands

```aria
// Compile code
build: ProcessResult = exec("gcc", ["main.c", "-o", "program"])?;

when build.exit_code != 0 then
    stderr << "Build failed:\n$(build.stderr)";
end
```

---

## Error Cases

- Command not found
- Permission denied
- Invalid arguments
- Command execution failure

---

## Best Practices

### ✅ DO: Check Exit Codes

```aria
ProcessResult:Result = exec(command, args)?;

when result.exit_code != 0 then
    stderr << "Command failed with code $(result.exit_code)";
    stderr << "Error: $(result.stderr)";
    fail("Execution failed");
end
```

### ✅ DO: Handle Both Stdout and Stderr

```aria
Result: ProcessResult = exec("make", ["build"])?;

stdout << result.stdout;

when result.stderr.length() > 0 then
    stderr << "Warnings:\n$(result.stderr)";
end
```

### ❌ DON'T: Ignore Errors

```aria
Result<ProcessResult>:Result = exec("rm", ["-rf", important_dir])?;
// ❌ Didn't check if it succeeded!

// Better
when result.exit_code != 0 then
    fail("Failed to delete: $(result.stderr)");
end
```

### ⚠️ WARNING: Shell Injection

```aria
// DANGEROUS - user input could contain malicious commands!
user_input: string = get_user_input();
exec("sh", ["-c", "rm $user_input"])?;  // ❌ UNSAFE

// Safe - args are properly escaped
exec("rm", [user_input])?;  // ✅ Safe
```

---

## vs spawn()

```aria
// exec() - WAITS for completion
Result: ProcessResult = exec("sleep", ["5"])?;  // Blocks for 5 seconds
stdout << "Done sleeping";

// spawn() - Runs in background
process: Process = spawn("sleep", ["5"])?;  // Returns immediately
stdout << "Still running...";
process.wait()?;  // Wait when needed
```

---

## Related

- [spawn()](spawn.md) - Run process in background
- [fork()](fork.md) - Fork current process
- [wait()](wait.md) - Wait for child process
- [process_management](process_management.md) - Overview

---

**Remember**: `exec()` **blocks** until command completes!
