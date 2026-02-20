# The `bool` Type

**Category**: Types → Primitives  
**Syntax**: `bool`  
**Purpose**: Boolean true/false values

---

## Overview

`bool` represents **logical values** - either `true` or `false`.

---

## Specifications

| Aspect | Value |
|--------|-------|
| **Values** | `true`, `false` |
| **Storage** | 1 byte (implementation-defined) |
| **Default** | `false` |

---

## Declaration

```aria
// Basic
is_ready: bool = true;
has_error: bool = false;

// From comparison
is_positive: bool = (value > 0);

// Type inference
active := true;  // Inferred as bool
```

---

## Literals

```aria
// True and false are keywords
x: bool = true;
y: bool = false;

// Not numbers!
// z: bool = 1;  // ❌ Error - not C!
```

---

## Logical Operations

### AND (`and`)

```aria
a: bool = true;
b: bool = false;

Result: bool = a and b;  // false

// Short-circuit evaluation
Result: bool = expensive_check() and another_check();
// another_check() not called if expensive_check() is false
```

### OR (`or`)

```aria
Result: bool = a or b;  // true

// Short-circuit
Result: bool = quick_check() or slow_check();
// slow_check() not called if quick_check() is true
```

### NOT (`!`)

```aria
is_disabled: bool = !is_enabled;

// Double negation
x: bool = !!value;  // Convert to bool
```

---

## Use Cases

### Conditionals

```aria
is_valid: bool = check_input(data);

when is_valid then
    process(data);
else
    report_error();
end
```

### Flags

```aria
struct Config {
    debug_mode: bool,
    verbose: bool,
    auto_save: bool
}

config: Config = {
    debug_mode = true,
    verbose = false,
    auto_save = true
};
```

### State Tracking

```aria
// Loop control
done: bool = false;

while not done {
    item := get_next_item();
    when item == nil then
        done = true;
    else
        process(item);
    end
}
```

---

## Best Practices

### ✅ DO: Use Descriptive Names

```aria
// Good - clear intent
is_connected: bool = check_connection();
has_permission: bool = user.can_write();
should_retry: bool = (attempts < max_attempts);
```

### ✅ DO: Use for Flags

```aria
// Perfect for options
enable_logging: bool = true;
skip_validation: bool = false;
```

### ❌ DON'T: Use Integers as Booleans

```aria
// Wrong - not C!
flag: i32 = 1;  // ❌

// Right
flag: bool = true;  // ✅
```

### ❌ DON'T: Compare with true/false

```aria
// Redundant
when is_ready == true then ...  // ❌

// Better
when is_ready then ...  // ✅

// For false
when not is_ready then ...  // ✅
```

---

## Conversions

### To/From Integers (Explicit)

```aria
// Bool to int
flag: bool = true;
value: i32 = when flag then 1 else 0 end;

// Int to bool (check for zero)
num: i32 = 42;
is_nonzero: bool = (num != 0);
```

### From Comparisons

```aria
// Automatically bool
is_positive: bool = (x > 0);
in_range: bool = (x >= min and x <= max);
```

---

## Common Patterns

### Validation

```aria
fn validate_user(user: User) -> bool {
    when user.name.is_empty() then
        return false;
    end
    
    when user.age < 18 then
        return false;
    end
    
    return true;
}
```

### State Machines

```aria
is_running: bool = false;
is_paused: bool = false;

fn start() {
    is_running = true;
    is_paused = false;
}

fn pause() {
    when is_running then
        is_paused = true;
    end
}
```

### Loop Control

```aria
// Early exit
found: bool = false;
till(items.length - 1, 1) {
    when matches(items[$]) then
        found = true;
        break;
    end
}

when found then
    stdout << "Found it!";
end
```

---

## Truth Tables

### AND

| A | B | A and B |
|---|---|---------|
| false | false | false |
| false | true | false |
| true | false | false |
| true | true | true |

### OR

| A | B | A or B |
|---|---|--------|
| false | false | false |
| false | true | true |
| true | false | true |
| true | true | true |

### NOT

| A | !A |
|---|-----|
| false | true |
| true | false |

---

## Related

- [Logical AND (and)](../operators/logical_and.md)
- [Logical OR (or)](../operators/logical_or.md)
- [Logical NOT (!)](../operators/logical_not.md)
- [When-Then-End](../control_flow/when_then_end.md)

---

**Remember**: `bool` is **true** or **false** - not `1` or `0`!
