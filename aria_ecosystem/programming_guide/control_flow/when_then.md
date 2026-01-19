# The `when`/`then`/`end` Construct

**Category**: Control Flow → Conditional Loops  
**Syntax**: `when(condition) { body } then { success } end { failure }`  
**Purpose**: Loop with success/failure branching based on how loop terminated

---

## Overview

The `when`/`then`/`end` construct is a conditional loop that executes different code blocks depending on **how** the loop terminated:

- **`then` block**: Executes if loop completed normally (condition became false)
- **`end` block**: Executes if loop was broken early (via `break`)

**Philosophy**: Distinguish between "finished naturally" vs "interrupted early" without manual flags.

---

## Basic Syntax

```aria
when(condition) {
    // Loop body
    // Executes while condition is true
}
then {
    // Success path
    // Executes if loop completed normally
}
end {
    // Failure/early exit path
    // Executes if loop was broken via break
}
```

### Termination Scenarios

| Loop Exit | Block Executed | Use Case |
|-----------|----------------|----------|
| Condition becomes false | `then` | Normal completion |
| `break` statement | `end` | Early exit/interruption |

---

## Example 1: Search Array

```aria
int64[]:data = [10, 20, 30, 40, 50];
int64:target = 30;
int64:index = -1;

when(index < data.length - 1) {
    index++;
    if (data[index] == target) {
        break;  // Found it!
    }
}
then {
    // Loop completed without finding target
    print("Target not found");
}
end {
    // Loop broke early (found target)
    print(`Target found at index &{index}`);
}
```

**Output**: `Target found at index 2`

---

## Example 2: Input Validation Loop

```aria
int64:attempts = 0;
bool:valid = false;

when(attempts < 3 && !valid) {
    str:input = readLine("Enter password: ");
    
    if (validatePassword(input)) {
        valid = true;
        break;
    }
    
    attempts++;
}
then {
    // Ran out of attempts
    print("Too many failed attempts. Account locked.");
}
end {
    // Valid password entered
    print("Access granted!");
}
```

---

## Example 3: Resource Acquisition

```aria
bool:acquired = false;
int64:retries = 0;

when(retries < 5 && !acquired) {
    acquired = tryAcquireResource();
    
    if (acquired) {
        break;
    }
    
    retries++;
    sleep(1000);  // Wait before retry
}
then {
    // Failed to acquire after max retries
    print("Resource unavailable");
    handleFailure();
}
end {
    // Successfully acquired resource
    print("Resource acquired");
    useResource();
    releaseResource();
}
```

---

## When to Use Each Block

### Use `then` For:

- **Normal completion**: Loop ran until condition became false
- **No match found**: Searched entire dataset without finding target
- **Timeout/limit reached**: Tried maximum number of times without success
- **Exhaustion**: Processed all items without early exit

### Use `end` For:

- **Early success**: Found target before checking all items
- **Early termination**: Condition met before exhausting iterations
- **Break on error**: Detected error condition requiring immediate exit
- **User interruption**: Break triggered by external event

---

## Comparison with Traditional Loops

### Without `when`/`then`/`end`

```aria
// Need manual flag
int64:index = 0;
bool:found = false;

while (index < data.length && !found) {
    if (data[index] == target) {
        found = true;
    } else {
        index++;
    }
}

// Manual check
if (found) {
    print("Found!");
} else {
    print("Not found");
}
```

### With `when`/`then`/`end`

```aria
// No manual flag needed
int64:index = 0;

when(index < data.length) {
    if (data[index] == target) {
        break;  // Triggers end block
    }
    index++;
}
then {
    print("Not found");
}
end {
    print("Found!");
}
```

**Benefit**: Cleaner, more declarative, no manual state tracking.

---

## Common Patterns

### Pattern 1: Linear Search

```aria
int64:index = 0;

when(index < array.length) {
    if (array[index] == target) {
        break;
    }
    index++;
}
then {
    print("Target not found");
}
end {
    print(`Found at index &{index}`);
}
```

### Pattern 2: Retry Loop

```aria
int64:attempt = 0;

when(attempt < maxAttempts) {
    if (tryOperation()) {
        break;  // Success
    }
    attempt++;
    sleep(retryDelay);
}
then {
    print("Operation failed after max retries");
}
end {
    print("Operation succeeded");
}
```

### Pattern 3: Condition Polling

```aria
int64:elapsed = 0;

when(elapsed < timeout) {
    if (isReady()) {
        break;
    }
    sleep(100);
    elapsed += 100;
}
then {
    print("Timeout waiting for ready state");
}
end {
    print("Ready!");
}
```

### Pattern 4: User Input Validation

```aria
bool:valid = false;

when(!valid) {
    str:input = readLine("Enter value: ");
    
    if (isValid(input)) {
        processInput(input);
        break;
    }
    
    print("Invalid input, try again");
}
then {
    // Never happens (loop continues until valid)
    unreachable();
}
end {
    print("Input accepted");
}
```

---

## Optional Blocks

### `then` is Optional

```aria
// Only care about early exit case
when(condition) {
    if (found) {
        break;
    }
}
end {
    print("Found!");
}
// No then block needed
```

### `end` is Optional

```aria
// Only care about normal completion
when(condition) {
    process();
}
then {
    print("All done!");
}
// No end block needed
```

### Both Optional (Just a Loop)

```aria
// Equivalent to while loop
when(condition) {
    doWork();
}
```

---

## Nested `when` Constructs

### Nested Search

```aria
int64:row = 0;
bool:found = false;

when(row < matrix.rows && !found) {
    int64:col = 0;
    
    when(col < matrix.cols) {
        if (matrix[row][col] == target) {
            found = true;
            break;  // Breaks inner when
        }
        col++;
    }
    end {
        break;  // Found in this row, break outer
    }
    
    row++;
}
end {
    print(`Found at (&{row}, &{col})`);
}
then {
    print("Not found in matrix");
}
```

---

## Best Practices

### ✅ Use for Search Operations

```aria
// GOOD: Natural search pattern
when(searching) {
    if (found) { break; }
}
then {
    handleNotFound();
}
end {
    handleFound();
}
```

### ✅ Use for Retry Loops

```aria
// GOOD: Clear success/failure paths
when(attempts < max) {
    if (success) { break; }
    attempts++;
}
then {
    handleFailure();
}
end {
    handleSuccess();
}
```

### ✅ Prefer Over Manual Flags

```aria
// AVOID: Manual flag tracking
bool:found = false;
while (!found && condition) {
    if (match) { found = true; }
}
if (found) { ... }

// PREFER: when/then/end
when(condition) {
    if (match) { break; }
}
end { ... }
```

### ❌ Don't Overuse for Simple Loops

```aria
// BAD: Unnecessary for simple iteration
when(i < 10) {
    process(i);
    i++;
}
then {
    print("Done");
}

// GOOD: Use simple loop
loop(0, 9, 1) {
    process($);
}
print("Done");
```

---

## Error Handling Pattern

### Combining with Result Types

```aria
when(attempts < maxAttempts) {
    Result[Connection]:result = tryConnect();
    
    result?
        .onSuccess(conn => {
            useConnection(conn);
            break;  // Success
        })
        .onError(err => {
            logError(err);
            attempts++;
        });
}
then {
    fail("Failed to connect after &{maxAttempts} attempts");
}
end {
    pass(connection);
}
```

---

## Performance

### Zero-Cost Abstraction

The `when`/`then`/`end` construct compiles to efficient branching:

```aria
// This when/then/end...
when(condition) {
    if (found) { break; }
}
then {
    handleNotFound();
}
end {
    handleFound();
}

// ...compiles similarly to:
bool _early_exit = false;
while (condition) {
    if (found) {
        _early_exit = true;
        break;
    }
}
if (_early_exit) {
    handleFound();
} else {
    handleNotFound();
}
```

**No runtime overhead beyond a single boolean flag.**

---

## Use Cases

### Use Case 1: File Parsing

```aria
when(!eof(file)) {
    str:line = readLine(file);
    
    if (line.contains("ERROR")) {
        errorLine = line;
        break;
    }
}
then {
    print("No errors found in file");
}
end {
    print(`Error detected: &{errorLine}`);
}
```

### Use Case 2: Game Loop

```aria
when(player.isAlive() && !gameOver) {
    processInput();
    updateGame();
    render();
    
    if (player.reachedGoal()) {
        break;  // Win condition
    }
}
then {
    showGameOver("You died!");
}
end {
    showGameOver("You win!");
}
```

### Use Case 3: Network Request

```aria
int64:backoff = 100;

when(backoff < 10000) {
    Result[Response]:result = httpGet(url);
    
    if (result.isOk()) {
        response = result.unwrap();
        break;
    }
    
    sleep(backoff);
    backoff *= 2;  // Exponential backoff
}
then {
    fail("Request failed after max backoff");
}
end {
    pass(response);
}
```

---

## Related Topics

- [while Loop](while.md) - Basic conditional loop
- [break Statement](break_continue.md#break) - Loop early exit
- [continue Statement](break_continue.md#continue) - Skip iteration
- [Result Type](../types/result.md) - Error handling with Result[T]
- [loop Construct](loop.md) - Automatic iteration
- [till Loop](till.md) - Count-based iteration

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Lines 400-427  
**Unique Feature**: Automatic success/failure branching without manual flags
