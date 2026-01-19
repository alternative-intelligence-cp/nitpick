# The `pick` Construct (Pattern Matching)

**Category**: Control Flow → Pattern Matching  
**Syntax**: `pick(expression) { case: ... }`  
**Purpose**: Multi-way branching with pattern matching and fallthrough control

---

## Overview

The `pick` construct is Aria's pattern matching statement, similar to `switch` in C but with important differences:

- **No automatic fallthrough** (unlike C's `switch`)
- **Explicit `fall()` for intentional fallthrough**
- **Expression-based** matching with type safety
- **Range support** with `..` operator

**Philosophy**: Safe by default, explicit when you need fallthrough.

---

## Basic Syntax

```aria
pick(expression) {
    case value1:
        // Code for value1
        
    case value2:
        // Code for value2
        
    default:
        // Catch-all case
}
```

### Key Differences from C `switch`

| Feature | C `switch` | Aria `pick` |
|---------|------------|-------------|
| Fallthrough | Automatic (need `break`) | None (need `fall()`) |
| Type safety | Weak | Strong |
| Ranges | Not supported | `case 1..10:` |
| Expression types | Integers only | Integers, enums, chars |

---

## Example 1: Basic Pick

```aria
int64:day = 3;

pick(day) {
    case 1:
        print("Monday");
        
    case 2:
        print("Tuesday");
        
    case 3:
        print("Wednesday");
        
    case 4:
        print("Thursday");
        
    case 5:
        print("Friday");
        
    case 6:
    case 7:
        print("Weekend!");
        
    default:
        print("Invalid day");
}
```

**Output**: `Wednesday`

**Note**: Cases 6 and 7 both print "Weekend!" because case 6 falls through to case 7.

---

## Example 2: Range Matching

```aria
int64:score = 85;

pick(score) {
    case 90..100:
        print("Grade: A");
        
    case 80..89:
        print("Grade: B");
        
    case 70..79:
        print("Grade: C");
        
    case 60..69:
        print("Grade: D");
        
    case 0..59:
        print("Grade: F");
        
    default:
        print("Invalid score");
}
```

**Output**: `Grade: B`

---

## Explicit Fallthrough with `fall()`

### Without `fall()` - No Fallthrough

```aria
int64:x = 1;

pick(x) {
    case 1:
        print("One");
        // No fallthrough - execution stops here
        
    case 2:
        print("Two");
}

// Output: "One"
```

### With `fall()` - Intentional Fallthrough

```aria
int64:x = 1;

pick(x) {
    case 1:
        print("One");
        fall();  // Explicitly continue to next case
        
    case 2:
        print("Two");
}

// Output: "One" then "Two"
```

---

## Multiple Values per Case

### Comma-Separated Values

```aria
int64:month = 2;

pick(month) {
    case 12, 1, 2:
        print("Winter");
        
    case 3, 4, 5:
        print("Spring");
        
    case 6, 7, 8:
        print("Summer");
        
    case 9, 10, 11:
        print("Fall");
        
    default:
        print("Invalid month");
}
```

**Output**: `Winter`

### Alternative: Stacked Cases

```aria
pick(month) {
    case 12:
    case 1:
    case 2:
        print("Winter");
        
    case 3:
    case 4:
    case 5:
        print("Spring");
    
    // ... etc
}
```

**Both syntaxes are equivalent.**

---

## Default Case

### Catch-All Handler

```aria
int64:value = 999;

pick(value) {
    case 1:
        print("One");
        
    case 2:
        print("Two");
        
    default:
        print(`Unknown value: &{value}`);
}
```

**Output**: `Unknown value: 999`

### Default is Optional

```aria
// No default needed if all cases covered
pick(value) {
    case 1:
        handleOne();
    case 2:
        handleTwo();
    // No default - unmatched values do nothing
}
```

---

## Common Patterns

### Pattern 1: Command Dispatcher

```aria
str:command = readCommand();

pick(command) {
    case "help":
        printHelp();
        
    case "quit":
    case "exit":
        shutdown();
        
    case "save":
        saveData();
        
    case "load":
        loadData();
        
    default:
        print(`Unknown command: &{command}`);
}
```

### Pattern 2: State Machine

```aria
pick(currentState) {
    case STATE_INIT:
        initialize();
        currentState = STATE_RUNNING;
        
    case STATE_RUNNING:
        update();
        if (shouldPause) {
            currentState = STATE_PAUSED;
        }
        
    case STATE_PAUSED:
        if (shouldResume) {
            currentState = STATE_RUNNING;
        }
        
    case STATE_SHUTDOWN:
        cleanup();
}
```

### Pattern 3: Error Code Handling

```aria
int64:errorCode = performOperation();

pick(errorCode) {
    case 0:
        print("Success");
        
    case 1..99:
        print("Warning");
        logWarning(errorCode);
        
    case 100..199:
        print("Error");
        logError(errorCode);
        
    case 200..299:
        print("Critical");
        logCritical(errorCode);
        handleCritical();
        
    default:
        print("Unknown error code");
}
```

### Pattern 4: Keyboard Input

```aria
char:key = readKey();

pick(key) {
    case 'w':
    case 'W':
        moveUp();
        
    case 's':
    case 'S':
        moveDown();
        
    case 'a':
    case 'A':
        moveLeft();
        
    case 'd':
    case 'D':
        moveRight();
        
    case ' ':
        jump();
        
    case 27:  // ESC key
        pause();
        
    default:
        // Ignore other keys
}
```

---

## Range Syntax

### Inclusive Ranges

```aria
// Both endpoints included
pick(value) {
    case 1..10:
        print("Between 1 and 10 (inclusive)");
        // Matches: 1, 2, 3, ..., 9, 10
}
```

### Overlapping Ranges

```aria
// First match wins
pick(value) {
    case 1..50:
        print("Low");
        
    case 25..75:  // Won't match if value in 25-50 (matched above)
        print("Mid");
        
    case 50..100:
        print("High");
}
```

**⚠️ First matching case executes, even if ranges overlap.**

---

## Comparison with Other Constructs

### Pick vs If-Else Chain

```aria
// VERBOSE: if-else chain
if (value == 1) {
    handleOne();
} else if (value == 2) {
    handleTwo();
} else if (value == 3) {
    handleThree();
} else {
    handleDefault();
}

// CLEAN: pick
pick(value) {
    case 1: handleOne();
    case 2: handleTwo();
    case 3: handleThree();
    default: handleDefault();
}
```

### Pick vs C Switch

```c
// C: Need explicit breaks
switch (value) {
    case 1:
        handleOne();
        break;  // Required to prevent fallthrough
    case 2:
        handleTwo();
        break;
    default:
        handleDefault();
}
```

```aria
// Aria: No breaks needed (safe by default)
pick(value) {
    case 1:
        handleOne();
        // Automatically stops here
    case 2:
        handleTwo();
    default:
        handleDefault();
}
```

---

## Advanced Examples

### Example 1: Calculator

```aria
char:op = readOperator();
int64:a = 10;
int64:b = 5;

pick(op) {
    case '+':
        print(`&{a} + &{b} = &{a + b}`);
        
    case '-':
        print(`&{a} - &{b} = &{a - b}`);
        
    case '*':
        print(`&{a} * &{b} = &{a * b}`);
        
    case '/':
        if (b != 0) {
            print(`&{a} / &{b} = &{a / b}`);
        } else {
            print("Division by zero");
        }
        
    default:
        print("Unknown operator");
}
```

### Example 2: HTTP Status Codes

```aria
int64:statusCode = httpRequest(url);

pick(statusCode) {
    case 200:
        print("OK");
        processResponse();
        
    case 201:
        print("Created");
        
    case 204:
        print("No Content");
        
    case 400:
        print("Bad Request");
        logError("Client error");
        
    case 401:
        print("Unauthorized");
        requestAuth();
        
    case 404:
        print("Not Found");
        
    case 500..599:
        print("Server Error");
        retryRequest();
        
    default:
        print(`Unhandled status: &{statusCode}`);
}
```

### Example 3: Menu System

```aria
while (running) {
    print("\n1. New Game");
    print("2. Load Game");
    print("3. Settings");
    print("4. Quit");
    
    int64:choice = readInt();
    
    pick(choice) {
        case 1:
            startNewGame();
            
        case 2:
            loadGame();
            
        case 3:
            openSettings();
            
        case 4:
            running = false;
            
        default:
            print("Invalid choice");
    }
}
```

### Example 4: Intentional Fallthrough

```aria
// Logging levels with fallthrough
pick(logLevel) {
    case LOG_TRACE:
        logTrace(message);
        fall();
        
    case LOG_DEBUG:
        logDebug(message);
        fall();
        
    case LOG_INFO:
        logInfo(message);
        fall();
        
    case LOG_WARN:
        logWarn(message);
        fall();
        
    case LOG_ERROR:
        logError(message);
        // All levels >= ERROR get logged to error file
}
```

---

## Best Practices

### ✅ Use pick for Multiple Discrete Values

```aria
// GOOD: Clear value mapping
pick(command) {
    case "start": start();
    case "stop": stop();
    case "pause": pause();
}
```

### ✅ Use Ranges for Intervals

```aria
// GOOD: Range matching
pick(age) {
    case 0..12: category = "Child";
    case 13..19: category = "Teen";
    case 20..64: category = "Adult";
    case 65..120: category = "Senior";
}
```

### ✅ Always Include default for Safety

```aria
// GOOD: Handle unexpected values
pick(value) {
    case 1: handle1();
    case 2: handle2();
    default: handleUnexpected();
}
```

### ❌ Don't Use for Complex Conditions

```aria
// BAD: Complex logic doesn't belong in pick
pick(value) {
    case x if (x > 10 && x < 20):  // Not supported
        doSomething();
}

// GOOD: Use if-else for complex conditions
if (value > 10 && value < 20) {
    doSomething();
}
```

### ❌ Avoid Unnecessary fall()

```aria
// BAD: Overusing fall()
pick(x) {
    case 1:
        a();
        fall();
    case 2:
        b();
        fall();
    case 3:
        c();
}

// GOOD: Restructure logic
if (x >= 1 && x <= 3) {
    if (x <= 1) a();
    if (x <= 2) b();
    if (x <= 3) c();
}
```

---

## Comparison with Other Languages

### Aria `pick`

```aria
pick(value) {
    case 1: handleOne();
    case 2: handleTwo();
    default: handleDefault();
}
```

### C `switch`

```c
switch (value) {
    case 1: handleOne(); break;
    case 2: handleTwo(); break;
    default: handleDefault();
}
```

### Rust `match`

```rust
match value {
    1 => handleOne(),
    2 => handleTwo(),
    _ => handleDefault(),
}
```

### Python (match)

```python
match value:
    case 1: handleOne()
    case 2: handleTwo()
    case _: handleDefault()
```

---

## Related Topics

- [if/else Conditionals](if_else.md) - Basic conditionals
- [when/then/end](when_then.md) - Conditional loops with branching
- [Ternary Operator](../operators/is_ternary.md) - Inline conditionals
- [Ranges](../operators/range.md) - Range syntax with `..`
- [fall() Statement](fall.md) - Explicit fallthrough control

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Lines 457-478  
**Unique Feature**: Safe by default (no fallthrough), explicit `fall()` when needed
