# While Loops

**Category**: Control Flow → Iteration  
**Keywords**: `while`  
**Philosophy**: Loop while condition is true

---

## What is a While Loop?

**While loops** repeat code as long as a condition remains true.

---

## Basic Syntax

```aria
while condition {
    // Code runs while condition is true
}
```

---

## Simple While Loop

```aria
count: i32 = 0;

while count < 5 {
    stdout << count << "\n";
    count = count + 1;
}

// Output:
// 0
// 1
// 2
// 3
// 4
```

---

## Condition Checked Before Each Iteration

```aria
// Condition is false from start - never runs
count: i32 = 10;

while count < 5 {
    stdout << "This never prints\n";
}
```

---

## Common Patterns

### Countdown

```aria
countdown: i32 = 10;

while countdown > 0 {
    stdout << countdown << "...\n";
    countdown = countdown - 1;
}

stdout << "Liftoff!\n";
```

### Process Until Empty

```aria
while !queue.is_empty() {
    item: Item = queue.pop();
    process(item);
}
```

### Read Until End

```aria
while !file.eof() {
    line: string = pass file.read_line();
    stdout << line << "\n";
}
```

### Retry Until Success

```aria
attempts: i32 = 0;
max_attempts: i32 = 3;
success: bool = false;

while !success && attempts < max_attempts {
    Result: bool = try_operation();
    success = result;
    attempts = attempts + 1;
}
```

---

## Break Statement

```aria
while true {
    input: string = read_input();
    
    if input == "quit" {
        break;  // Exit loop
    }
    
    process(input);
}
```

---

## Continue Statement

```aria
count: i32 = 0;

while count < 10 {
    count = count + 1;
    
    if count % 2 == 0 {
        continue;  // Skip even numbers
    }
    
    stdout << count << " ";
}
// Output: 1 3 5 7 9
```

---

## Infinite Loop with While

```aria
// ❌ Don't use while for infinite loops
while true {
    // code
}

// ✅ Use loop instead
loop {
    // code
}
```

---

## Best Practices

### ✅ DO: Ensure Condition Can Become False

```aria
// Good: count increases, will eventually reach 10
count: i32 = 0;
while count < 10 {
    process(count);
    count = count + 1;
}
```

### ✅ DO: Use While for Unknown Iterations

```aria
// Good: Don't know how many lines
while !file.eof() {
    line: string = file.read_line();
    process(line);
}
```

### ❌ DON'T: Create Accidental Infinite Loops

```aria
// ❌ Wrong: Forgot to increment
count: i32 = 0;
while count < 10 {
    stdout << count << "\n";
    // Missing: count = count + 1;
}
// Infinite loop!

// ✅ Right: Update condition variable
count: i32 = 0;
while count < 10 {
    stdout << count << "\n";
    count = count + 1;
}
```

### ❌ DON'T: Use While for Known Counts

```aria
// Wrong: Know we want exactly 10 iterations
i: i32 = 0;
while i < 10 {
    process(i);
    i = i + 1;
}

// Right: Use till loop
till(9, 1) {
    process($);
}
```

---

## Real-World Examples

### Input Loop

```aria
fn get_user_input() -> string {
    input: string = "";
    
    while input.length() == 0 {
        stdout << "Enter your name: ";
        input = read_line();
        
        if input.length() == 0 {
            stderr << "Name cannot be empty!\n";
        }
    }
    
    return input;
}
```

### Polling

```aria
while !server.is_ready() {
    stdout << "Waiting for server...\n";
    sleep(1000);  // Wait 1 second
}

stdout << "Server ready!\n";
```

### Buffer Processing

```aria
buffer: []byte = read_buffer();
position: i32 = 0;

while position < buffer.length() {
    chunk: []byte = buffer[position..position+1024];
    process_chunk(chunk);
    position = position + 1024;
}
```

---

## Related Topics

- [While Syntax](while_syntax.md) - Complete syntax
- [Loop](loop.md) - Infinite loops
- [For](for.md) - Collection iteration
- [Break](break.md) - Exit loops
- [Continue](continue.md) - Skip iterations

---

**Remember**: While loops run **while condition is true** - ensure condition can become false, prefer `for` for known iterations!
