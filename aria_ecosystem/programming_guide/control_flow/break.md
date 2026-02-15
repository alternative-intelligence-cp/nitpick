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

## With Till Loop

```aria
numbers: []i32 = [1, 2, 3, 4, 5, 6, 7, 8];

till(numbers.length - 1, 1) {
    if numbers[$] > 5 {
        break;  // Stop when we hit 6
    }
    stdout << numbers[$] << " ";
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

till(names.length - 1, 1) {
    if names[$].starts_with("C") {
        found = names[$];
        break;  // Stop searching
    }
}

stdout << "Found: " << (found ?? "none") << "\n";
```

---

## Nested Loops

Break only exits the **innermost** loop:

```aria
till(4, 1) {
    i: i32 = $;
    till(4, 1) {
        j: i32 = $;
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

till(4, 1) {
    i: i32 = $;
    till(4, 1) {
        j: i32 = $;
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
till(large_list.length - 1, 1) {
    if large_list[$].matches(criteria) {
        result = large_list[$];
        break;
    }
}
```

### Limit Processing

```aria
processed: i32 = 0;

till(items.length - 1, 1) {
    process(items[$]);
    processed = processed + 1;
    
    if processed >= max_items {
        break;
    }
}
```

### Validation

```aria
valid: bool = true;

till(form_fields.length - 1, 1) {
    if !form_fields[$].is_valid() {
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
till(users.length - 1, 1) {
    if users[$].id == target_id {
        found_user = users[$];
        break;
    }
}
```

### ✅ DO: Break on Error

```aria
// Good: Stop on first error
till(files.length - 1, 1) {
    result: bool = process_file(files[$]);
    if !result {
        stderr << "Processing failed\n";
        break;
    }
}
```

### ❌ DON'T: Use Break for Normal Exit

```aria
// Wrong: Awkward
till(9, 1) {
    process($);
    if $ == 9 {
        break;
    }
}

// Right: Let loop complete naturally
till(9, 1) {
    process($);
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
