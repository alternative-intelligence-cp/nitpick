# Pick Syntax Reference

**Category**: Control Flow → Syntax  
**Topic**: Complete pick statement syntax

---

## Basic Pick

```aria
pick value {
    case pattern:
        // code
    else:
        // default
}
```

---

## Multiple Cases

```aria
pick value {
    case 1:
        // code
    case 2:
        // code
    case 3:
        // code
    else:
        // default
}
```

---

## Multiple Values Per Case

```aria
case value1, value2, value3:
    // code
```

---

## Range Patterns

```aria
case 0..10:      // 0 to 9 (exclusive end)
    // code

case 0..=10:     // 0 to 10 (inclusive end)
    // code
```

---

## Fallthrough

```aria
case value1:
    // code
    fall;  // Continue to next case

case value2:
    // code
```

---

## Else Clause

```aria
pick value {
    case 1:
        // code
    else:
        // default (catches everything else)
}
```

---

## Complete Example

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

## With Strings

```aria
pick command {
    case "start":
        start();
    
    case "stop":
        stop();
    
    case "restart":
        restart();
    
    else:
        stderr << "Unknown command\n";
}
```

---

## With Enums

```aria
pick status {
    case Status::Pending:
        wait();
    
    case Status::Processing:
        monitor();
    
    case Status::Done:
        finish();
}
```

---

## Requirements

- All cases must have unique patterns
- `else` is optional but recommended
- Each case ends with `:` 
- No fallthrough by default (use `fall`)
- Break is implicit (no `break` keyword needed)

---

## Related Topics

- [Pick](pick.md) - Pick statement guide
- [Pick Patterns](pick_patterns.md) - Pattern matching
- [Fallthrough](fallthrough.md) - Case fallthrough

---

**Quick Reference**: `pick value { case pattern: ... else: ... }`
