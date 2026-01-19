# While Syntax Reference

**Category**: Control Flow → Syntax  
**Topic**: Complete while loop syntax

---

## Basic While

```aria
while condition {
    // code runs while condition is true
}
```

---

## Condition Requirements

- Must be `bool` type
- Checked before each iteration
- No implicit truthiness

```aria
// ✅ Valid
while x > 0 { }
while is_ready { }
while !done { }

// ❌ Invalid
while 1 { }      // Not bool
while count { }  // count must be bool
```

---

## With Break

```aria
while condition {
    if exit_condition {
        break;  // Exit loop
    }
}
```

---

## With Continue

```aria
while condition {
    if skip_condition {
        continue;  // Next iteration
    }
    
    // code
}
```

---

## Examples

```aria
// Count down
count: i32 = 10;
while count > 0 {
    stdout << count;
    count = count - 1;
}

// Process until empty
while !queue.is_empty() {
    item: Item = queue.pop();
    process(item);
}

// Read file
while !file.eof() {
    line: string = file.read_line();
    stdout << line;
}
```

---

## Related Topics

- [While](while.md) - While loop guide
- [Loop](loop.md) - Infinite loops
- [For](for.md) - For loops

---

**Quick Reference**: `while bool_condition { }` - loops while true
