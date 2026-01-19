# Fallthrough

**Category**: Control Flow → Pick/Switch  
**Keyword**: `fall`  
**Philosophy**: Explicit cascade between cases

---

## What is Fallthrough?

**Fallthrough** allows execution to continue into the next case in a `pick` statement, similar to `fallthrough` in Go or omitting `break` in C/switch.

---

## Basic Syntax

```aria
pick value {
    case 1:
        stdout << "One\n";
        fall;  // Continue to next case
    case 2:
        stdout << "Two\n";
    else:
        stdout << "Other\n";
}
```

---

## How It Works

```aria
value: i32 = 1;

pick value {
    case 1:
        stdout << "Matched 1\n";
        fall;  // Fall through to case 2
    case 2:
        stdout << "Matched 2\n";  // This also runs!
    else:
        stdout << "Other\n";
}

// Output:
// Matched 1
// Matched 2
```

---

## No Fallthrough by Default

Unlike C, Aria **doesn't fall through** by default:

```aria
value: i32 = 1;

pick value {
    case 1:
        stdout << "One\n";
        // No fall - stops here
    case 2:
        stdout << "Two\n";  // Doesn't run
}

// Output: One
```

---

## Explicit Fallthrough

```aria
day: string = "Saturday";

pick day {
    case "Saturday":
        stdout << "It's the weekend!\n";
        fall;  // Continue to Sunday
    case "Sunday":
        stdout << "No work today!\n";
    else:
        stdout << "Workday\n";
}

// Output when day is Saturday:
// It's the weekend!
// No work today!
```

---

## Multiple Falls

```aria
grade: string = "A";

pick grade {
    case "A":
        stdout << "Excellent\n";
        fall;
    case "B":
        stdout << "Good\n";
        fall;
    case "C":
        stdout << "Passing\n";
    else:
        stdout << "Needs improvement\n";
}

// Output when grade is "A":
// Excellent
// Good
// Passing
```

---

## Fall to Else

```aria
value: i32 = 5;

pick value {
    case 1:
        stdout << "One\n";
    case 5:
        stdout << "Five\n";
        fall;  // Falls to else
    else:
        stdout << "Done\n";
}

// Output:
// Five
// Done
```

---

## Common Patterns

### Grouped Cases

```aria
char: string = "a";

pick char {
    case "a":
        fall;
    case "e":
        fall;
    case "i":
        fall;
    case "o":
        fall;
    case "u":
        stdout << "Vowel\n";
    else:
        stdout << "Consonant\n";
}
```

### Progressive Actions

```aria
level: i32 = 3;

pick level {
    case 3:
        enable_advanced_features();
        fall;
    case 2:
        enable_intermediate_features();
        fall;
    case 1:
        enable_basic_features();
    else:
        disable_all_features();
}
```

### Cascading Defaults

```aria
priority: string = "high";

pick priority {
    case "critical":
        notify_everyone();
        fall;
    case "high":
        notify_team();
        fall;
    case "medium":
        notify_lead();
        fall;
    case "low":
        log_message();
}
```

---

## When to Use Fallthrough

### ✅ Use When:

- Cases share some logic
- Progressive/cumulative actions
- Grouping similar cases
- Intentional cascade

### ❌ Don't Use When:

- Cases are independent
- Each case is self-contained
- Logic would be clearer with helper functions

---

## Best Practices

### ✅ DO: Comment Intention

```aria
pick value {
    case 1:
        setup();
        fall;  // Intentionally fall to case 2
    case 2:
        execute();
}
```

### ✅ DO: Group Related Cases

```aria
pick status {
    case "pending":
        fall;
    case "processing":
        fall;
    case "waiting":
        handle_in_progress();
    else:
        handle_completed();
}
```

### ❌ DON'T: Overuse Fallthrough

```aria
// Wrong: Too complex
pick value {
    case 1:
        a();
        fall;
    case 2:
        b();
        fall;
    case 3:
        c();
        fall;
    case 4:
        d();
        fall;
    case 5:
        e();
}

// Right: Refactor
fn handle_value(value: i32) {
    if value >= 1 { a(); }
    if value >= 2 { b(); }
    if value >= 3 { c(); }
    if value >= 4 { d(); }
    if value >= 5 { e(); }
}
```

### ❌ DON'T: Use for Empty Cases

```aria
// Wrong: Fallthrough for empty case
pick value {
    case 1:
        fall;
    case 2:
        process();
}

// Right: Multiple values per case
pick value {
    case 1, 2:
        process();
}
```

---

## Comparison with Other Languages

### C/C++ (Implicit Fallthrough)

```c
switch (value) {
    case 1:
        printf("One\n");
        // Falls through by default!
    case 2:
        printf("Two\n");
        break;  // Explicit break needed
}
```

### Rust (No Fallthrough)

```rust
match value {
    1 => println!("One"),
    2 => println!("Two"),
    // No fallthrough - each arm is independent
}
```

### Aria (Explicit Fallthrough)

```aria
pick value {
    case 1:
        stdout << "One\n";
        fall;  // Explicit fall needed
    case 2:
        stdout << "Two\n";
}
```

---

## Real-World Examples

### Permission Levels

```aria
fn check_permission(level: i32) {
    pick level {
        case 3:  // Admin
            grant_admin_access();
            fall;
        case 2:  // Moderator
            grant_moderator_access();
            fall;
        case 1:  // User
            grant_user_access();
        else:
            deny_access();
    }
}
```

### HTTP Status Categories

```aria
status_code: i32 = 404;

pick status_code {
    case 200:
        fall;
    case 201:
        fall;
    case 204:
        handle_success();
    
    case 400:
        fall;
    case 404:
        handle_client_error();
    
    case 500:
        fall;
    case 503:
        handle_server_error();
    
    else:
        handle_unknown();
}
```

### Day Type Classification

```aria
pick day {
    case "Monday":
        fall;
    case "Tuesday":
        fall;
    case "Wednesday":
        fall;
    case "Thursday":
        fall;
    case "Friday":
        workday_routine();
    
    case "Saturday":
        fall;
    case "Sunday":
        weekend_routine();
}
```

---

## Related Topics

- [Pick](pick.md) - Pattern matching
- [Fall Keyword](fall.md) - Fall keyword reference
- [Pick Patterns](pick_patterns.md) - Case patterns
- [Pick Syntax](pick_syntax.md) - Complete syntax

---

**Remember**: Fallthrough is **explicit** in Aria - use `fall` to continue to next case, omit for normal break behavior!
