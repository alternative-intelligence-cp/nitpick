# Pick Patterns

**Category**: Control Flow → Pick/Switch  
**Concept**: Pattern matching in pick statements  
**Philosophy**: Expressive, safe matching

---

## What are Pick Patterns?

**Pick patterns** define what values or structures to match in `pick` statements.

---

## Literal Patterns

```aria
pick value {
    case 1:
        stdout << "One\n";
    case 2:
        stdout << "Two\n";
    case 3:
        stdout << "Three\n";
    else:
        stdout << "Other\n";
}
```

---

## Multiple Values Per Case

```aria
pick day {
    case "Monday", "Tuesday", "Wednesday", "Thursday", "Friday":
        stdout << "Weekday\n";
    case "Saturday", "Sunday":
        stdout << "Weekend\n";
}
```

---

## String Patterns

```aria
pick command {
    case "start":
        start_server();
    case "stop":
        stop_server();
    case "restart":
        restart_server();
    else:
        stderr << "Unknown command\n";
}
```

---

## Range Patterns

```aria
pick score {
    case 90..=100:
        grade = "A";
    case 80..=89:
        grade = "B";
    case 70..=79:
        grade = "C";
    case 60..=69:
        grade = "D";
    else:
        grade = "F";
}
```

---

## Enum Patterns

```aria
enum Status {
    Pending,
    Processing,
    Done,
    Failed
}

pick status {
    case Status::Pending:
        wait();
    case Status::Processing:
        monitor();
    case Status::Done:
        finish();
    case Status::Failed:
        retry();
}
```

---

## Type Patterns (if supported)

```aria
pick value {
    case i32:
        stdout << "Integer\n";
    case string:
        stdout << "String\n";
    case bool:
        stdout << "Boolean\n";
    else:
        stdout << "Other type\n";
}
```

---

## Destructuring Patterns (if supported)

```aria
pick point {
    case (0, 0):
        stdout << "Origin\n";
    case (x, 0):
        stdout << "On X axis\n";
    case (0, y):
        stdout << "On Y axis\n";
    case (x, y):
        stdout << "Point at ({}, {})\n";
}
```

---

## Guard Clauses (if supported)

```aria
pick value {
    case x when x > 0:
        stdout << "Positive\n";
    case x when x < 0:
        stdout << "Negative\n";
    case 0:
        stdout << "Zero\n";
}
```

---

## Wildcard Pattern

```aria
pick value {
    case 1, 2, 3:
        process_low();
    case _:  // Matches anything
        process_other();
}
```

---

## Best Practices

### ✅ DO: Group Related Values

```aria
// Good
pick status_code {
    case 200, 201, 204:
        handle_success();
    case 400, 404, 422:
        handle_client_error();
    case 500, 503:
        handle_server_error();
}
```

### ✅ DO: Use Ranges for Continuous Values

```aria
// Good
pick age {
    case 0..=12:
        category = "child";
    case 13..=17:
        category = "teen";
    case 18..=64:
        category = "adult";
    else:
        category = "senior";
}
```

### ❌ DON'T: Duplicate Cases

```aria
// Wrong
pick value {
    case 1:
        a();
    case 1:  // Error: Duplicate!
        b();
}
```

### ❌ DON'T: Have Unreachable Cases

```aria
// Wrong
pick value {
    case _:  // Catches everything
        handle_all();
    case 1:  // Never reached!
        handle_one();
}
```

---

## Real-World Examples

### HTTP Status Codes

```aria
pick response.status {
    case 200:
        handle_ok();
    case 201:
        handle_created();
    case 400:
        handle_bad_request();
    case 401:
        handle_unauthorized();
    case 404:
        handle_not_found();
    case 500:
        handle_server_error();
    else:
        handle_unknown_status();
}
```

### Grade Calculation

```aria
pick percentage {
    case 90..=100:
        return "A";
    case 80..=89:
        return "B";
    case 70..=79:
        return "C";
    case 60..=69:
        return "D";
    else:
        return "F";
}
```

### Command Routing

```aria
pick command {
    case "create", "new", "add":
        create_item();
    case "delete", "remove", "rm":
        delete_item();
    case "update", "edit", "modify":
        update_item();
    case "list", "show", "ls":
        list_items();
    else:
        stderr << "Unknown command: " << command << "\n";
}
```

---

## Related Topics

- [Pick](pick.md) - Pick statement guide
- [Pick Syntax](pick_syntax.md) - Complete syntax
- [Fallthrough](fallthrough.md) - Case fallthrough

---

**Remember**: Pick patterns match **values, ranges, or structures** - group related values, use ranges for continuous data!
