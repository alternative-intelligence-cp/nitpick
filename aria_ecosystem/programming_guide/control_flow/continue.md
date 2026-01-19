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
for item in collection {
    if should_skip(item) {
        continue;  // Skip to next item
    }
    
    // This runs only if we didn't continue
    process(item);
}
```

---

## Skip Even Numbers

```aria
for i in 0..10 {
    if i % 2 == 0 {
        continue;  // Skip even numbers
    }
    
    stdout << i << " ";
}
// Output: 1 3 5 7 9
```

---

## Filter While Processing

```aria
numbers: []i32 = [1, -2, 3, -4, 5, -6];

for num in numbers {
    if num < 0 {
        continue;  // Skip negative numbers
    }
    
    stdout << num << " ";
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

for record in records {
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
for i in 0..3 {
    for j in 0..5 {
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
for user in users {
    if !user.is_active {
        continue;  // Skip inactive users
    }
    
    send_notification(user);
}
```

### Skip Based on Condition

```aria
for file in files {
    if file.extension() != ".txt" {
        continue;  // Only process .txt files
    }
    
    content: string = pass file.read();
    process(content);
}
```

### Error Handling

```aria
for item in items {
    Result: Result = validate(item);
    
    if result == ERR {
        stderr << "Invalid item, skipping\n";
        continue;
    }
    
    process(item);
}
```

---

## Continue vs Break

### Continue - Skip This Iteration

```aria
for i in 0..5 {
    if i == 2 {
        continue;  // Skip 2, keep going
    }
    stdout << i << " ";
}
// Output: 0 1 3 4
```

### Break - Exit Loop

```aria
for i in 0..5 {
    if i == 2 {
        break;  // Stop completely
    }
    stdout << i << " ";
}
// Output: 0 1
```

---

## Best Practices

### ✅ DO: Use for Filtering

```aria
// Good: Skip unwanted items
for item in items {
    if !matches_criteria(item) {
        continue;
    }
    
    process(item);
}
```

### ✅ DO: Use for Early Validation

```aria
// Good: Skip invalid early
for data in dataset {
    if data.length() == 0 {
        continue;
    }
    
    // Complex processing here
}
```

### ❌ DON'T: Overuse Continue

```aria
// Wrong: Too many continues
for item in items {
    if condition1 { continue; }
    if condition2 { continue; }
    if condition3 { continue; }
    if condition4 { continue; }
    
    process(item);
}

// Right: Combine conditions
for item in items {
    should_skip: bool = condition1 || condition2 || 
                        condition3 || condition4;
    
    if should_skip {
        continue;
    }
    
    process(item);
}
```

### ❌ DON'T: Use Instead of If

```aria
// Wrong: Awkward
for item in items {
    continue;  // ???
    process(item);  // Never runs
}

// Right: Use if
for item in items {
    if should_process(item) {
        process(item);
    }
}
```

---

## Real-World Examples

### Processing Log Files

```aria
for line in log_file {
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

for record in records {
    Result: Result = process_record(record);
    
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
