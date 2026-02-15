# Continue Statement

**Category**: Control Flow → Loop Control  
**Keyword**: `continue`  
**Philosophy**: Skip to next iteration

---

## What is Continue?

**Continue** skips the rest of the current iteration and jumps to the next one.

---

## Basic Syntax

```aria
till(collection.length - 1, 1) {
    if should_skip(collection[$]) {
        continue;  // Skip to next item
    }
    
    // This runs only if we didn't continue
    process(collection[$]);
}
```

---

## Skip Even Numbers

```aria
till(9, 1) {
    if $ % 2 == 0 {
        continue;  // Skip even numbers
    }
    
    stdout << $ << " ";
}
// Output: 1 3 5 7 9
```

---

## Filter While Processing

```aria
numbers: []i32 = [1, -2, 3, -4, 5, -6];

till(numbers.length - 1, 1) {
    if numbers[$] < 0 {
        continue;  // Skip negative numbers
    }
    
    stdout << numbers[$] << " ";
}
// Output: 1 3 5
```

---

## With While Loop

```aria
count: i32 = 0;

while count < 10 {
    count = count + 1;
    
    if count == 5 {
        continue;  // Skip 5
    }
    
    stdout << count << " ";
}
// Output: 1 2 3 4 6 7 8 9 10
```

---

## Skip Invalid Items

```aria
records: []Record = load_records();

till(records.length - 1, 1) {
    record: auto = records[$];
    if !record.is_valid() {
        stderr << "Skipping invalid record: " << record.id << "\n";
        continue;
    }
    
    // Only process valid records
    process(record);
}
```

---

## Nested Loops

Continue affects only the **innermost** loop:

```aria
till(2, 1) {
    i: i32 = $;
    till(4, 1) {
        j: i32 = $;
        if j == 2 {
            continue;  // Skip j=2 in inner loop
        }
        stdout << "(" << i << "," << j << ") ";
    }
    stdout << "\n";
}

// Skips when j=2 in each row
```

---

## Common Patterns

### Filter and Process

```aria
till(users.length - 1, 1) {
    if !users[$].is_active {
        continue;  // Skip inactive users
    }
    
    send_notification(users[$]);
}
```

### Skip Based on Condition

```aria
till(files.length - 1, 1) {
    if files[$].extension() != ".txt" {
        continue;  // Only process .txt files
    }
    
    content: string = pass files[$].read();
    process(content);
}
```

### Error Handling

```aria
till(items.length - 1, 1) {
    result: Result = validate(items[$]);
    
    if result == ERR {
        stderr << "Invalid item, skipping\n";
        continue;
    }
    
    process(items[$]);
}
```

---

## Continue vs Break

### Continue - Skip This Iteration

```aria
till(4, 1) {
    if $ == 2 {
        continue;  // Skip 2, keep going
    }
    stdout << $ << " ";
}
// Output: 0 1 3 4
```

### Break - Exit Loop

```aria
till(4, 1) {
    if $ == 2 {
        break;  // Stop completely
    }
    stdout << $ << " ";
}
// Output: 0 1
```

---

## Best Practices

### ✅ DO: Use for Filtering

```aria
// Good: Skip unwanted items
till(items.length - 1, 1) {
    if !matches_criteria(items[$]) {
        continue;
    }
    
    process(items[$]);
}
```

### ✅ DO: Use for Early Validation

```aria
// Good: Skip invalid early
till(dataset.length - 1, 1) {
    if dataset[$].length() == 0 {
        continue;
    }
    
    // Complex processing here
}
```

### ❌ DON'T: Overuse Continue

```aria
// Wrong: Too many continues
till(items.length - 1, 1) {
    if condition1 { continue; }
    if condition2 { continue; }
    if condition3 { continue; }
    if condition4 { continue; }
    
    process(items[$]);
}

// Right: Combine conditions
till(items.length - 1, 1) {
    should_skip: bool = condition1 || condition2 || 
                        condition3 || condition4;
    
    if should_skip {
        continue;
    }
    
    process(items[$]);
}
```

### ❌ DON'T: Use Instead of If

```aria
// Wrong: Awkward
till(items.length - 1, 1) {
    continue;  // ???
    process(items[$]);  // Never runs
}

// Right: Use if
till(items.length - 1, 1) {
    if should_process(items[$]) {
        process(items[$]);
    }
}
```

---

## Real-World Examples

### Processing Log Files

```aria
till(log_file.length - 1, 1) {
    line: auto = log_file[$];
    // Skip empty lines
    if line.trim().length() == 0 {
        continue;
    }
    
    // Skip comments
    if line.starts_with("#") {
        continue;
    }
    
    // Parse and process
    entry: LogEntry = pass parse_log_entry(line);
    analyze(entry);
}
```

### Batch Processing

```aria
success_count: i32 = 0;
error_count: i32 = 0;

till(records.length - 1, 1) {
    record: auto = records[$];
    result: Result = process_record(record);
    
    if result == ERR {
        error_count = error_count + 1;
        continue;  // Skip to next record
    }
    
    success_count = success_count + 1;
    save_result(record, result);
}

stdout << "Processed: " << success_count << " successful, " 
       << error_count << " errors\n";
```

---

## Related Topics

- [Break](break.md) - Exit loops
- [Return](return.md) - Exit function
- [For](for.md) - For loops
- [While](while.md) - While loops
- [Loop](loop.md) - Infinite loops

---

**Remember**: Continue **skips current iteration** - use for filtering, validation, error handling in loops!
