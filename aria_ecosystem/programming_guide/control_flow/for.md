# Till Loops

**Category**: Control Flow → Iteration  
**Keywords**: `till`  
**Philosophy**: Iterate with explicit bounds and index access

---

## What is a Till Loop?

**Till loops** iterate over a range of indices with clean, readable syntax using the `$` index variable.

---

## Basic Syntax

```aria
till(end_index, step) {
    // Use $ as index
}
```

---

## Iterate Over Array

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

till(numbers.length - 1, 1) {
    stdout << numbers[$] << "\n";
}

// Output:
// 1
// 2
// 3
// 4
// 5
```

---

## Iterate Over Range

```aria
// 0 to 9
till(9, 1) {
    stdout << $ << " ";
}
// Output: 0 1 2 3 4 5 6 7 8 9

// 1 to 10 (inclusive)
till(10, 1) {
    stdout << $ + 1 << " ";  // Index is 0-9, add 1 for 1-10
}
// Output: 1 2 3 4 5 6 7 8 9 10
```

---

## Iterate Over String

```aria
message: string = "Hello";

till(message.length - 1, 1) {
    stdout << message[$] << "\n";
}

// Output:
// H
// e
// l
// l
// o
```

---

## With Index

```aria
items: []string = ["apple", "banana", "cherry"];

till(items.length - 1, 1) {
    stdout << $ << ": " << items[$] << "\n";
}

// Output:
// 0: apple
// 1: banana
// 2: cherry
```

---

## Mutable Iteration

To modify items, use direct array indexing:

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

till(numbers.length - 1, 1) {
    numbers[$] = numbers[$] * 2;  // Modifies original array
}

stdout << numbers;  // [2, 4, 6, 8, 10]
```

---

## Read-Only vs Mutable Access

```aria
numbers: []i32 = [1, 2, 3];

till(numbers.length - 1, 1) {
    num: auto = numbers[$];  // Local copy
    num = num * 2;           // ✅ Modifies local variable
}

// ✅ Direct array access for mutation:
till(numbers.length - 1, 1) {
    numbers[$] = numbers[$] * 2;
}
```

---

## Break Statement

Exit the loop early:

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

till(numbers.length - 1, 1) {
    if numbers[$] == 3 {
        break;  // Exit loop
    }
    stdout << numbers[$] << " ";
}
// Output: 1 2
```

---

## Continue Statement

Skip to next iteration:

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

till(numbers.length - 1, 1) {
    if numbers[$] % 2 == 0 {
        continue;  // Skip even numbers
    }
    stdout << numbers[$] << " ";
}
// Output: 1 3 5
```

---

## Nested Till Loops

```aria
till(2, 1) {  // i: 0, 1, 2
    i: i32 = $ + 1;  // 1, 2, 3
    till(2, 1) {  // j: 0, 1, 2
        j: i32 = $ + 1;  // 1, 2, 3
        stdout << "(" << i << "," << j << ") ";
    }
    stdout << "\n";
}

// Output:
// (1,1) (1,2) (1,3) 
// (2,1) (2,2) (2,3) 
// (3,1) (3,2) (3,3)
```

---

## Iterate Over Map/Dictionary

```aria
scores: Map<string, i32> = Map::new();
scores.insert("Alice", 95);
scores.insert("Bob", 87);

keys: []string = scores.keys();
till(keys.length - 1, 1) {
    key: auto = keys[$];
    value: auto = scores[key];
    stdout << key << ": " << value << "\n";
}

// Output:
// Alice: 95
// Bob: 87
```

---

## Reverse Iteration

```aria
numbers: []i32 = [1, 2, 3, 4, 5];
reversed: []i32 = numbers.reverse();

till(reversed.length - 1, 1) {
    stdout << reversed[$] << " ";
}
// Output: 5 4 3 2 1
```

---

## Filter While Iterating

```aria
numbers: []i32 = [1, 2, 3, 4, 5, 6];

till(numbers.length - 1, 1) {
    if numbers[$] % 2 == 0 {  // Only process even numbers
        result: i32 = numbers[$] * numbers[$];
        stdout << result << " ";
    }
}
// Output: 4 16 36
```

---

## Common Patterns

### Sum Elements

```aria
numbers: []i32 = [1, 2, 3, 4, 5];
sum: i32 = 0;

till(numbers.length - 1, 1) {
    sum = sum + numbers[$];
}

stdout << "Sum: " << sum << "\n";  // Sum: 15
```

### Find Element

```aria
names: []string = ["Alice", "Bob", "Charlie"];
found: bool = false;

till(names.length - 1, 1) {
    if names[$] == "Bob" {
        found = true;
        break;
    }
}

if found {
    stdout << "Found Bob!\n";
}
```

### Transform Array

```aria
numbers: []i32 = [1, 2, 3, 4, 5];
squared: []i32 = [];

till(numbers.length - 1, 1) {
    squared.push(numbers[$] * numbers[$]);
}

stdout << squared;  // [1, 4, 9, 16, 25]
```

### Count Occurrences

```aria
items: []string = ["apple", "banana", "apple", "cherry", "apple"];
count: i32 = 0;

till(items.length - 1, 1) {
    if items[$] == "apple" {
        count = count + 1;
    }
}

stdout << "Apples: " << count << "\n";  // Apples: 3
```

---

## With Error Handling

```aria
files: []string = ["a.txt", "b.txt", "c.txt"];

till(files.length - 1, 1) {
    filename: auto = files[$];
    file: File = pass open(filename);
    content: string = pass file.read();
    
    stdout << "File " << filename << ": " << content.length() << " bytes\n";
}
```

---

## Enumerate Pattern

```aria
items: []string = ["first", "second", "third"];

till(items.length - 1, 1) {
    stdout << "Item " << ($ + 1) << ": " << items[$] << "\n";
}

// Output:
// Item 1: first
// Item 2: second
// Item 3: third
```

---

## Chunked Iteration

```aria
numbers: []i32 = [1, 2, 3, 4, 5, 6, 7, 8];
chunks: [][]i32 = numbers.chunks(3);

till(chunks.length - 1, 1) {
    stdout << "Chunk: " << chunks[$] << "\n";
}

// Output:
// Chunk: [1, 2, 3]
// Chunk: [4, 5, 6]
// Chunk: [7, 8]
```

---

## Best Practices

### ✅ DO: Use Till Loops for Arrays

```aria
// Good: Natural iteration
till(items.length - 1, 1) {
    process(items[$]);
}
```

### ✅ DO: Use Till for Counted Loops

```aria
// Good: Clear intent
till(9, 1) {
    stdout << $ << "\n";
}
```

### ✅ DO: Use Direct Indexing for Mutation

```aria
// Good: Explicit mutation
till(items.length - 1, 1) {
    items[$].update();
}
```

### ✅ DO: Break Early When Possible

```aria
// Good: Stop when found
till(large_list.length - 1, 1) {
    if large_list[$].matches(criteria) {
        result = large_list[$];
        break;
    }
}
```

### ❌ DON'T: Modify Collection While Iterating

```aria
// ❌ Wrong: Unsafe
till(items.length - 1, 1) {
    items.remove(items[$]);  // Undefined behavior!
}

// ✅ Right: Collect items to remove
to_remove: []Item = [];
till(items.length - 1, 1) {
    if should_remove(items[$]) {
        to_remove.push(items[$]);
    }
}

till(to_remove.length - 1, 1) {
    items.remove(to_remove[$]);
}
```

### ❌ DON'T: Use Index When Item Access is Simple

```aria
// Good: Direct access
till(items.length - 1, 1) {
    process(items[$]);
}
```

---

## Till vs Loop vs While

### Till Loop (Index-Based Iteration)

```aria
// Best for iterating arrays/collections
till(items.length - 1, 1) {
    process(items[$]);
}
```

### Loop (Infinite/Complex)

```aria
// Best for infinite or complex loops
loop {
    item: Item? = queue.pop();
    if item == nil {
        break;
    }
    process(item);
}
```

### While (Condition-Based)

```aria
// Best for condition-based loops
while !queue.is_empty() {
    item: Item = queue.pop();
    process(item);
}
```

---

## Real-World Examples

### Processing Records

```aria
records: []Record = load_records();

till(records.length - 1, 1) {
    record: auto = records[$];
    if !record.is_valid() {
        stderr << "Invalid record: " << record.id << "\n";
        continue;
    }
    
    result: bool = pass process_record(record);
    
    when result then
        stdout << "Processed: " << record.id << "\n";
    else
        stderr << "Failed: " << record.id << "\n";
    end
}
```

### Building Report

```aria
students: []Student = load_students();
report: string = "Student Grades:\n";

till(students.length - 1, 1) {
    student: auto = students[$];
    index: i32 = $;
    avg: f64 = student.calculate_average();
    grade: string = letter_grade(avg);
    
    report = report + format("{}. {} - {} ({})\n", 
        index + 1, student.name, avg, grade);
}

stdout << report;
```

### Matrix Operations

```aria
matrix: [][]i32 = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
];

till(matrix.length - 1, 1) {
    row: auto = matrix[$];
    till(row.length - 1, 1) {
        stdout << row[$] << " ";
    }
    stdout << "\n";
}
```

---

## Related Topics

- [For Syntax](for_syntax.md) - Complete syntax reference
- [Loop](loop.md) - Infinite loops
- [While](while.md) - Condition-based loops
- [Break](break.md) - Exit loops
- [Continue](continue.md) - Skip iterations
- [Dollar Variable](dollar_variable.md) - Index variable in loops

---

**Remember**: Till loops use **index-based iteration** - use `$` for index, direct array access for values, break/continue for control!
