# Loop Syntax Reference

**Category**: Control Flow → Syntax  
**Topic**: Complete loop syntax

---

## Basic Loop (Infinite)

```aria
loop {
    // code repeats forever
}
```

---

## With Break

```aria
loop {
    if condition {
        break;  // Exit loop
    }
}
```

---

## With Continue

```aria
loop {
    if skip_condition {
        continue;  // Next iteration
    }
    
    // code
}
```

---

## Common Pattern

```aria
loop {
    item: Item? = queue.pop();
    
    if item == nil {
        break;
    }
    
    process(item);
}
```

---

## Examples

```aria
// Event loop
loop {
    event: Event = wait_for_event();
    handle(event);
}

// Server loop
loop {
    conn: Connection = accept();
    spawn(|| handle_connection(conn));
}

// Read until end
loop {
    line: string? = file.read_line();
    
    if line == nil {
        break;
    }
    
    process(line);
}
```

---

## Related Topics

- [Loop](loop.md) - Loop guide
- [Break](break.md) - Breaking loops
- [Continue](continue.md) - Skipping iterations

---

**Quick Reference**: `loop { }` - infinite loop, use `break` to exit
