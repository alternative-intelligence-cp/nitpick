# When-Then-End

**Category**: Control Flow → Conditionals  
**Keywords**: `when`, `then`, `end`  
**Philosophy**: Aria-style conditionals

---

## What is When-Then-End?

**When-then-end** is Aria's alternative conditional syntax to `if-else`, using natural language keywords.

---

## Basic Syntax

```aria
when condition then
    // code runs when condition is true
end
```

---

## Simple When

```aria
age: i32 = 20;

when age >= 18 then
    stdout << "Adult\n";
end
```

---

## When-Else

```aria
age: i32 = 16;

when age >= 18 then
    stdout << "Adult\n";
else
    stdout << "Minor\n";
end
```

---

## When-Else-When Chain

```aria
score: i32 = 85;

when score >= 90 then
    grade = "A";
else when score >= 80 then
    grade = "B";
else when score >= 70 then
    grade = "C";
else when score >= 60 then
    grade = "D";
else
    grade = "F";
end

stdout << "Grade: " << grade << "\n";
```

---

## Identical to If-Else

These are **functionally identical**:

```aria
// when-then-end
when condition then
    code();
else
    other_code();
end

// if-else
if condition {
    code();
} else {
    other_code();
}
```

---

## Multi-Line Bodies

```aria
when user.is_authenticated then
    validate_session();
    load_preferences();
    grant_access();
else
    show_login();
    log_attempt();
end
```

---

## Nested When

```aria
when user.is_authenticated then
    when user.is_admin then
        stdout << "Admin dashboard\n";
    else
        stdout << "User dashboard\n";
    end
else
    stdout << "Please log in\n";
end
```

---

## With TBB Error Handling

```aria
file: File = open("data.txt");

when file == ERR then
    stderr << "Failed to open file\n";
    return;
end

// Use file
content: string = pass file.read();
```

---

## Guard Clauses

```aria
fn process_user(user: User?) -> bool {
    when user == nil then
        return false;
    end
    
    when !user.is_valid() then
        return false;
    end
    
    // Main logic
    user.update();
    return true;
}
```

---

## Common Patterns

### Validation

```aria
when !is_valid_email(email) then
    fail "Invalid email format";
end
```

### Permission Checking

```aria
when !user.has_permission("delete") then
    fail "Permission denied";
end
```

### State Transitions

```aria
when state == "pending" then
    state = "processing";
    start_work();
else when state == "processing" then
    monitor_progress();
else when state == "done" then
    cleanup();
end
```

---

## When vs If

### Use When-Then When:

- ✅ Prefer natural language syntax
- ✅ Aria-style coding
- ✅ Team uses when-then
- ✅ Consistent with other when statements

### Use If-Else When:

- ✅ Prefer C-style syntax
- ✅ Team uses if-else
- ✅ Matching other languages
- ✅ Shorter for simple cases

---

## Best Practices

### ✅ DO: Use Explicit Comparisons

```aria
// Good
when count > 0 then
when name != nil then
```

### ✅ DO: Use Guard Clauses

```aria
// Good: Handle errors early
when input == nil then
    return;
end

when input.length() == 0 then
    return;
end
```

### ❌ DON'T: Mix When and If

```aria
// Wrong: Inconsistent
when condition then
    if other_condition {
        // ...
    }
end

// Right: Pick one style
when condition then
    when other_condition then
        // ...
    end
end
```

### ❌ DON'T: Deeply Nest

```aria
// Wrong: Too deep
when a then
    when b then
        when c then
            when d then
                // code
            end
        end
    end
end

// Right: Early returns
when !a then return; end
when !b then return; end
when !c then return; end
when !d then return; end

// code
```

---

## Real-World Examples

### Input Validation

```aria
fn validate_order(order: Order) {
    when order.items.length() == 0 then
        fail "Order must contain items";
    end
    
    when order.total < 0 then
        fail "Invalid order total";
    end
    
    when !order.customer.is_verified then
        fail "Customer not verified";
    end
}
```

### State Machine

```aria
when state == "idle" then
    state = "processing";
    start_work();
else when state == "processing" then
    when is_complete() then
        state = "done";
        finish_work();
    end
else when state == "done" then
    stdout << "Work already done\n";
end
```

### Permission System

```aria
when user.role == "admin" then
    when action == "delete" then
        allow_delete();
    else when action == "edit" then
        allow_edit();
    else when action == "view" then
        allow_view();
    end
else when user.role == "moderator" then
    when action == "edit" || action == "view" then
        allow_action();
    else
        deny_action();
    end
else
    when action == "view" then
        allow_view();
    else
        deny_action();
    end
end
```

---

## Related Topics

- [When Syntax](when_syntax.md) - Complete syntax reference
- [If-Else](if_else.md) - If-else syntax
- [Pick](pick.md) - Pattern matching
- [When-Then](when_then.md) - Concept overview

---

**Remember**: When-then-end is **identical to if-else** - just different syntax, choose based on preference!
