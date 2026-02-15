# Stack Memory

**Category**: Memory Model → Memory Regions  
**Concept**: Automatic, fast allocation  
**Philosophy**: Predictable, efficient, safe

---

## What is Stack Memory?

The **stack** is a region of memory for automatic, short-lived allocations that are freed in reverse order of creation.

---

## Characteristics

### Automatic Management

```aria
fn calculate() -> i32 {
    x: i32 = 42;        // Allocated on stack
    y: i32 = x * 2;     // Allocated on stack
    return y;
}  // x and y automatically freed
```

### LIFO (Last In, First Out)

```aria
fn example() {
    a: i32 = 1;     // Pushed to stack
    b: i32 = 2;     // Pushed to stack
    c: i32 = 3;     // Pushed to stack
}  // Freed in reverse: c, b, a
```

### Fast Allocation

Stack allocation is **extremely fast** - just moving a pointer:

```aria
// Stack: Single pointer bump
value: i32 = 42;  // Microseconds

// vs Heap: Search free list, bookkeeping
value: Box<i32> = Box::new(42);  // Slower
```

---

## What Goes on the Stack?

### Local Variables

```aria
fn process() {
    count: i32 = 0;          // Stack
    name: string = "test";   // Stack (string header)
    active: bool = true;     // Stack
}
```

### Function Parameters

```aria
fn add(a: i32, b: i32) -> i32 {
    // a and b are on the stack
    return a + b;
}
```

### Return Values

```aria
fn create() -> Point {
    p: Point = Point{x: 10, y: 20};
    return p;  // Copied to caller's stack
}
```

### Small Structs

```aria
struct Point {
    x: i32,
    y: i32
}

point: Point = Point{x: 10, y: 20};  // Stack
```

---

## Stack Frames

Each function call creates a **stack frame**:

```aria
fn main() {
    // main's stack frame
    x: i32 = 10;
    
    Result: i32 = calculate(x);
}

fn calculate(value: i32) -> i32 {
    // calculate's stack frame (on top of main's)
    doubled: i32 = value * 2;
    return doubled;
}  // calculate's frame popped
// Back to main's frame
```

---

## Stack Size Limits

Stack space is **limited** (typically 1-8 MB):

```aria
// ❌ Stack overflow!
fn recursive() {
    large: [i32; 1000000] = [0; 1000000];  // Too big!
    recursive();  // Infinite recursion
}

// ✅ Use heap for large data
fn better() {
    large: Vec<i32> = Vec::with_capacity(1000000);
}
```

---

## Advantages of Stack

### 1. Speed

```aria
// Fast: Stack allocation
till(999999, 1) {
    value: i32 = $;  // Instant
}
```

### 2. Automatic Cleanup

```aria
fn example() {
    file: File = pass open("data.txt");
    // file automatically closed at end of scope
}  // Automatic!
```

### 3. Cache-Friendly

Stack data is **contiguous** and **local**, great for CPU cache:

```aria
// All on stack, cache-friendly
a: i32 = 1;
b: i32 = 2;
c: i32 = 3;
Result: i32 = a + b + c;
```

### 4. No Fragmentation

Stack allocations don't fragment memory:

```aria
// Stack pointer just moves up/down
// No fragmentation issues
```

---

## Stack vs Heap

### Stack

```aria
// Automatic, fast, limited size
value: i32 = 42;
array: [i32; 10] = [0; 10];
```

**Pros:**
- ✅ Extremely fast
- ✅ Automatic cleanup
- ✅ No fragmentation
- ✅ Cache-friendly

**Cons:**
- ❌ Limited size
- ❌ Fixed lifetime (scope-bound)
- ❌ Can't resize

### Heap

```aria
// Manual, slower, unlimited size
value: Box<i32> = Box::new(42);
vector: Vec<i32> = Vec::new();
```

**Pros:**
- ✅ Unlimited size (system memory)
- ✅ Flexible lifetime
- ✅ Can resize (Vec, etc.)

**Cons:**
- ❌ Slower allocation
- ❌ Manual management needed
- ❌ Can fragment
- ❌ Less cache-friendly

---

## Scope and Lifetime

Stack variables live until the end of their scope:

```aria
fn example() {
    {
        x: i32 = 42;
        stdout << x;  // OK
    }  // x freed here
    
    stdout << x;  // ❌ Error: x doesn't exist
}
```

---

## Stack Overflow

### Causes

1. **Deep recursion**

```aria
// ❌ Stack overflow
fn factorial(n: i32) -> i32 {
    if n == 0 { return 1; }
    return n * factorial(n - 1);  // 1000000 recursive calls!
}
```

2. **Large local arrays**

```aria
// ❌ Stack overflow
fn process() {
    huge: [i32; 10000000] = [0; 10000000];  // Too big!
}
```

### Solutions

```aria
// ✅ Use heap for large data
fn process() {
    huge: Vec<i32> = vec![0; 10000000];
}

// ✅ Use iteration instead of recursion
fn factorial(n: i32) -> i32 {
    result: i32 = 1;
    till(n, 1) {
        result = result * $;
    }
    return result;
}
```

---

## Best Practices

### ✅ DO: Use Stack by Default

```aria
// Good: Small, temporary data
count: i32 = 0;
point: Point = Point{x: 10, y: 20};
```

### ✅ DO: Keep Stack Allocations Small

```aria
// Good: Small arrays
buffer: [u8; 256] = [0; 256];

// Wrong: Large arrays
huge: [u8; 10000000] = [0; 10000000];  // Use heap!
```

### ✅ DO: Limit Recursion Depth

```aria
// Good: Bounded recursion
fn search(node: Node, depth: i32) -> bool {
    if depth > 100 {
        return false;  // Prevent overflow
    }
    // ...
}
```

### ❌ DON'T: Return Pointers to Stack

```aria
// ❌ Wrong: Dangling pointer!
fn get_pointer() -> &i32 {
    x: i32 = 42;
    return &x;  // Error: x freed at end of function!
}

// ✅ Right: Return value
fn get_value() -> i32 {
    x: i32 = 42;
    return x;  // Copied to caller
}
```

---

## Real-World Examples

### Simple Function

```aria
fn calculate_area(width: f64, height: f64) -> f64 {
    // All on stack
    area: f64 = width * height;
    return area;
}  // width, height, area all freed
```

### Processing Loop

```aria
fn process_items(items: []Item) {
    till(items.length - 1, 1) {
        // Loop variables on stack
        valid: bool = items[$].validate();
        
        when valid then
            result: Result = items[$].process();
            log_result(result);
        end
    }  // Loop variables freed each iteration
}
```

### Nested Scopes

```aria
fn example() {
    outer: i32 = 1;
    
    {
        inner: i32 = 2;
        stdout << outer + inner;  // 3
    }  // inner freed
    
    stdout << outer;  // OK
    stdout << inner;  // ❌ Error
}
```

---

## Stack Unwinding

When errors occur, stack frames are **unwound**:

```aria
fn main() {
    when process() == ERR then
        stderr << "Error occurred\n";
    end
}

fn process() -> Result {
    file: File = pass open("data.txt");
    defer file.close();  // Runs on unwind
    
    data: Data = pass parse(file);  // Error here
    return Ok(data);
}
// file.close() called during unwind
```

---

## Related Topics

- [Allocation](allocation.md) - Memory allocation
- [Defer](defer.md) - Cleanup on scope exit
- [RAII](raii.md) - Resource management
- [Borrowing](borrowing.md) - References to stack data

---

**Remember**: Stack is **automatic, fast, limited** - use for small, temporary data, heap for large or long-lived allocations!
