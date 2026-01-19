# If-Else Statements

**Category**: Control Flow → Conditionals  
**Keywords**: `if`, `else`  
**Philosophy**: Simple, readable branching

---

## What is If-Else?

**If-else** is the basic conditional statement for choosing between two paths based on a boolean condition.

---

## Basic Syntax

```aria
if condition {
    // Runs when condition is true
} else {
    // Runs when condition is false
}
```

---

## Simple If

```aria
age: i32 = 20;

if age >= 18 {
    stdout << "Adult\n";
}

// No else - code continues
```

---

## If-Else

```aria
age: i32 = 16;

if age >= 18 {
    stdout << "Adult\n";
} else {
    stdout << "Minor\n";
}
```

---

## If-Else-If Chain

```aria
score: i32 = 85;

if score >= 90 {
    grade: string = "A";
} else if score >= 80 {
    grade: string = "B";
} else if score >= 70 {
    grade: string = "C";
} else if score >= 60 {
    grade: string = "D";
} else {
    grade: string = "F";
}

stdout << "Grade: " << grade << "\n";
```

---

## Condition Must Be Boolean

```aria
// ✅ Correct: Boolean condition
if x > 0 { }
if is_valid { }
if name == "Alice" { }

// ❌ Wrong: Non-boolean
if x { }         // x must be bool
if count { }     // count must be bool
```

---

## No Implicit Truthiness

Unlike some languages, Aria doesn't convert values to booleans:

```aria
// ❌ Wrong: Can't use numbers as conditions
count: i32 = 5;
if count { }  // Error!

// ✅ Correct: Explicit comparison
if count > 0 { }
if count != 0 { }

// ❌ Wrong: Can't use pointers/optionals directly
name: string? = get_name();
if name { }  // Error!

// ✅ Correct: Explicit check
if name != nil { }
```

---

## Comparison with When-Then-End

Aria has two conditional statements:

### If-Else (Traditional)

```aria
if condition {
    // code
} else {
    // code
}
```

### When-Then-End (Aria-style)

```aria
when condition then
    // code
else
    // code
end
```

**Both work identically** - choose based on preference or code style.

---

## Nested If-Else

```aria
if user.is_authenticated {
    if user.is_admin {
        stdout << "Admin dashboard\n";
    } else {
        stdout << "User dashboard\n";
    }
} else {
    stdout << "Please log in\n";
}
```

---

## With TBB Error Handling

```aria
Result: File = open("data.txt");

if result == ERR {
    stderr << "Failed to open file\n";
    return;
}

file: File = result;
// Use file
```

---

## If-Else with Pass/Fail

```aria
file: File = pass open("data.txt");

if file.size() > 1000000 {
    stdout << "Large file\n";
} else {
    stdout << "Small file\n";
}
```

---

## Logical Operators

```aria
// AND
if age >= 18 && has_license {
    stdout << "Can drive\n";
}

// OR
if is_admin || is_moderator {
    stdout << "Has elevated privileges\n";
}

// NOT
if !is_banned {
    stdout << "Welcome!\n";
}

// Complex conditions
if (age >= 18 || has_parent_permission) && !is_banned {
    stdout << "Access granted\n";
}
```

---

## Early Return Pattern

```aria
fn process(value: i32) {
    if value < 0 {
        stderr << "Invalid value\n";
        return;
    }
    
    if value == 0 {
        stdout << "Zero\n";
        return;
    }
    
    // Main logic for positive values
    Result: i32 = value * 2;
    stdout << "Result: " << result << "\n";
}
```

---

## Guard Clauses

```aria
fn withdraw(amount: f64) -> bool {
    // Guard clauses - check error conditions first
    if amount <= 0 {
        stderr << "Amount must be positive\n";
        return false;
    }
    
    if amount > balance {
        stderr << "Insufficient funds\n";
        return false;
    }
    
    if account_frozen {
        stderr << "Account is frozen\n";
        return false;
    }
    
    // Main logic - happy path
    balance = balance - amount;
    return true;
}
```

---

## Common Patterns

### Validation

```aria
if !is_valid_email(email) {
    fail "Invalid email format";
}
```

### Range Checking

```aria
if value < min || value > max {
    fail "Value out of range";
}
```

### Null/Nil Checking

```aria
user: User? = find_user(id);

if user == nil {
    fail "User not found";
}

// Safe to use user here
stdout << user.name << "\n";
```

### Flag Checking

```aria
if debug_mode {
    stdout << "Debug: Processing item " << id << "\n";
}
```

---

## Best Practices

### ✅ DO: Use Explicit Comparisons

```aria
// Good: Clear intent
if count > 0 { }
if name != nil { }
if status == "active" { }
```

### ✅ DO: Use Guard Clauses

```aria
// Good: Handle errors early
if input == nil {
    return;
}

if input.length() == 0 {
    return;
}

// Main logic here
```

### ✅ DO: Keep Conditions Simple

```aria
// Good: Simple, readable
has_permission: bool = is_admin || is_moderator;
if has_permission {
    grant_access();
}
```

### ❌ DON'T: Deeply Nest

```aria
// Wrong: Too many levels
if a {
    if b {
        if c {
            if d {
                // code
            }
        }
    }
}

// Right: Early returns or restructure
if !a { return; }
if !b { return; }
if !c { return; }
if !d { return; }

// code
```

### ❌ DON'T: Compare Booleans to True/False

```aria
// Wrong: Redundant
if is_valid == true { }
if is_active == false { }

// Right: Use directly
if is_valid { }
if !is_active { }
```

### ❌ DON'T: Use If for Assignment Only

```aria
// Wrong: Verbose
value: i32;
if condition {
    value = 10;
} else {
    value = 20;
}

// Right: Use ternary or when expression if available
value: i32 = condition ? 10 : 20;
```

---

## Real-World Examples

### Input Validation

```aria
fn process_order(order: Order) {
    if order.items.length() == 0 {
        fail "Order must contain items";
    }
    
    if order.total < 0 {
        fail "Invalid order total";
    }
    
    if !order.customer.is_verified {
        fail "Customer not verified";
    }
    
    // Process order
}
```

### Permission Checking

```aria
fn delete_file(user: User, file: File) {
    if !user.is_authenticated {
        fail "User not authenticated";
    }
    
    if file.owner != user.id && !user.is_admin {
        fail "Permission denied";
    }
    
    if file.is_locked {
        fail "File is locked";
    }
    
    // Delete file
    file.delete();
}
```

### State Machine

```aria
if state == "idle" {
    // Start processing
    state = "processing";
    start_work();
} else if state == "processing" {
    // Check progress
    if is_complete() {
        state = "done";
        finish_work();
    }
} else if state == "done" {
    // Already complete
    stdout << "Work already done\n";
}
```

---

## If vs When-Then

### Use If-Else When:

- ✅ Traditional C-style syntax preferred
- ✅ Team uses curly braces
- ✅ Short, simple conditions
- ✅ Matching other languages

### Use When-Then-End When:

- ✅ Prefer Aria-style syntax
- ✅ More readable for complex conditions
- ✅ Consistent with other when statements
- ✅ Team convention

---

## Related Topics

- [When-Then-End](when_then_end.md) - Aria-style conditionals
- [If Syntax](if_syntax.md) - Complete syntax reference
- [Pick](pick.md) - Pattern matching
- [Loop](loop.md) - Iteration

---

**Remember**: If-else is **simple branching** - condition must be boolean, use guard clauses for clarity, avoid deep nesting!
