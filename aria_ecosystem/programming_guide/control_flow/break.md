# Break Statement

**Category**: Control Flow → Loop Control  
**Keyword**: `break`  
**Philosophy**: Exit loops cleanly

---

## What is Break?

**Break** immediately exits the current loop.

---

## Basic Syntax

```aria
loop {
    if condition {
        break;  // Exit loop
    }
}
```

---

## With While Loop

```aria
count: i32 = 0;

while count < 100 {
    if count == 5 {
        break;  // Stop at 5
    }
    stdout << count << " ";
    count = count + 1;
}
// Output: 0 1 2 3 4
```

---

## With For Loop

```aria
numbers: []i32 = [1, 2, 3, 4, 5, 6, 7, 8];

for num in numbers {
    if num > 5 {
        break;  // Stop when we hit 6
    }
    stdout << num << " ";
}
// Output: 1 2 3 4 5
```

---

## With Infinite Loop

```aria
loop {
    input: string = read_input();
    
    if input == "quit" {
        break;  // Exit infinite loop
    }
    
    process(input);
}
```

---

## Finding First Match

```aria
names: []string = ["Alice", "Bob", "Charlie", "David"];
found: string? = nil;

for name in names {
    if name.starts_with("C") {
        found = name;
        break;  // Stop searching
    }
}

stdout << "Found: " << (found ?? "none") << "\n";
```

---

## Nested Loops

Break only exits the **innermost** loop:

```aria
for i in 0..5 {
    for j in 0..5 {
        if j == 3 {
            break;  // Only exits inner loop
        }
        stdout << "(" << i << "," << j << ") ";
    }
    stdout << "\n";
}

// Each row stops at j=3
```

---

## Breaking Outer Loops

Use a flag or labeled breaks (if supported):

```aria
// Using flag
done: bool = false;

for i in 0..5 {
    for j in 0..5 {
        if i == 2 && j == 2 {
            done = true;
            break;
        }
    }
    if done {
        break;
    }
}
```

---

## Common Patterns

### Search and Stop

```aria
for item in large_list {
    if item.matches(criteria) {
        result = item;
        break;
    }
}
```

### Limit Processing

```aria
processed: i32 = 0;

for item in items {
    process(item);
    processed = processed + 1;
    
    if processed >= max_items {
        break;
    }
}
```

### Validation

```aria
valid: bool = true;

for field in form_fields {
    if !field.is_valid() {
        valid = false;
        break;  // Stop checking
    }
}
```

---

## Best Practices

### ✅ DO: Break When Found

```aria
// Good: Stop once found
for user in users {
    if user.id == target_id {
        found_user = user;
        break;
    }
}
```

### ✅ DO: Break on Error

```aria
// Good: Stop on first error
for file in files {
    Result: bool = process_file(file);
    if !result {
        stderr << "Processing failed\n";
        break;
    }
}
```

### ❌ DON'T: Use Break for Normal Exit

```aria
// Wrong: Awkward
for i in 0..10 {
    process(i);
    if i == 9 {
        break;
    }
}

// Right: Let loop complete naturally
for i in 0..10 {
    process(i);
}
```

---

## Related Topics

- [Continue](continue.md) - Skip to next iteration
- [Return](return.md) - Exit function
- [Loop](loop.md) - Infinite loops
- [For](for.md) - For loops
- [While](while.md) - While loops

---

**Remember**: Break **exits the current loop** immediately - use for early exit, search-and-stop, error handling!
