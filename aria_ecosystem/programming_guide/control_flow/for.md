# For Loops

**Category**: Control Flow → Iteration  
**Keywords**: `for`, `in`  
**Philosophy**: Iterate over collections naturally

---

## What is a For Loop?

**For loops** iterate over collections (arrays, ranges, iterators) with clean, readable syntax.

---

## Basic Syntax

```aria
for item in collection {
    // Use item
}
```

---

## Iterate Over Array

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

for num in numbers {
    stdout << num << "\n";
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
for i in 0..10 {
    stdout << i << " ";
}
// Output: 0 1 2 3 4 5 6 7 8 9

// 1 to 10 (inclusive)
for i in 1..=10 {
    stdout << i << " ";
}
// Output: 1 2 3 4 5 6 7 8 9 10
```

---

## Iterate Over String

```aria
message: string = "Hello";

for char in message {
    stdout << char << "\n";
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

for item, index in items {
    stdout << index << ": " << item << "\n";
}

// Output:
// 0: apple
// 1: banana
// 2: cherry
```

---

## Mutable Iteration

To modify items, capture them mutably with `$`:

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

for $num in numbers {
    num = num * 2;  // Modifies original array
}

stdout << numbers;  // [2, 4, 6, 8, 10]
```

---

## Read-Only by Default

```aria
numbers: []i32 = [1, 2, 3];

for num in numbers {
    num = num * 2;  // ❌ Error: num is immutable
}

// ✅ Correct: Use $ for mutable access
for $num in numbers {
    num = num * 2;
}
```

---

## Break Statement

Exit the loop early:

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

for num in numbers {
    if num == 3 {
        break;  // Exit loop
    }
    stdout << num << " ";
}
// Output: 1 2
```

---

## Continue Statement

Skip to next iteration:

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

for num in numbers {
    if num % 2 == 0 {
        continue;  // Skip even numbers
    }
    stdout << num << " ";
}
// Output: 1 3 5
```

---

## Nested For Loops

```aria
for i in 1..=3 {
    for j in 1..=3 {
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

for key, value in scores {
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

for num in numbers.reverse() {
    stdout << num << " ";
}
// Output: 5 4 3 2 1
```

---

## Filter While Iterating

```aria
numbers: []i32 = [1, 2, 3, 4, 5, 6];

for num in numbers {
    if num % 2 == 0 {  // Only process even numbers
        Result: i32 = num * num;
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

for num in numbers {
    sum = sum + num;
}

stdout << "Sum: " << sum << "\n";  // Sum: 15
```

### Find Element

```aria
names: []string = ["Alice", "Bob", "Charlie"];
found: bool = false;

for name in names {
    if name == "Bob" {
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

for num in numbers {
    squared.push(num * num);
}

stdout << squared;  // [1, 4, 9, 16, 25]
```

### Count Occurrences

```aria
items: []string = ["apple", "banana", "apple", "cherry", "apple"];
count: i32 = 0;

for item in items {
    if item == "apple" {
        count = count + 1;
    }
}

stdout << "Apples: " << count << "\n";  // Apples: 3
```

---

## With Error Handling

```aria
files: []string = ["a.txt", "b.txt", "c.txt"];

for filename in files {
    file: File = pass open(filename);
    content: string = pass file.read();
    
    stdout << "File " << filename << ": " << content.length() << " bytes\n";
}
```

---

## Enumerate Pattern

```aria
items: []string = ["first", "second", "third"];

for item, i in items {
    stdout << "Item " << (i + 1) << ": " << item << "\n";
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

for chunk in numbers.chunks(3) {
    stdout << "Chunk: " << chunk << "\n";
}

// Output:
// Chunk: [1, 2, 3]
// Chunk: [4, 5, 6]
// Chunk: [7, 8]
```

---

## Best Practices

### ✅ DO: Use For Loops for Collections

```aria
// Good: Natural iteration
for item in items {
    process(item);
}
```

### ✅ DO: Use Ranges for Counted Loops

```aria
// Good: Clear intent
for i in 0..10 {
    stdout << i << "\n";
}
```

### ✅ DO: Use $ for Mutation

```aria
// Good: Explicit mutation
for $item in items {
    item.update();
}
```

### ✅ DO: Break Early When Possible

```aria
// Good: Stop when found
for item in large_list {
    if item.matches(criteria) {
        result = item;
        break;
    }
}
```

### ❌ DON'T: Modify Collection While Iterating

```aria
// ❌ Wrong: Unsafe
for item in items {
    items.remove(item);  // Undefined behavior!
}

// ✅ Right: Collect items to remove
to_remove: []Item = [];
for item in items {
    if should_remove(item) {
        to_remove.push(item);
    }
}

for item in to_remove {
    items.remove(item);
}
```

### ❌ DON'T: Use Index When Item is Enough

```aria
// Wrong: Unnecessary indexing
for i in 0..items.length() {
    process(items[i]);
}

// Right: Direct iteration
for item in items {
    process(item);
}
```

---

## For vs Loop vs While

### For Loop (Collections)

```aria
// Best for iterating collections
for item in items {
    process(item);
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

for record in records {
    if !record.is_valid() {
        stderr << "Invalid record: " << record.id << "\n";
        continue;
    }
    
    Result: bool = pass process_record(record);
    
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

for student, index in students {
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

for row in matrix {
    for cell in row {
        stdout << cell << " ";
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

---

**Remember**: For loops are **natural iteration** - use for collections, `$` for mutation, break/continue for control!
