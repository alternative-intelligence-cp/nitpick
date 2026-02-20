# Function Parameters

**Category**: Functions → Syntax  
**Concept**: Inputs to functions  
**Philosophy**: Explicit types, clear intent

---

## Basic Parameter Syntax

```aria
fn function_name(param_name: Type) {
    // Use param_name here
}
```

---

## Single Parameter

```aria
fn greet(name: string) {
    stdout << "Hello, " << name << "\n";
}

// Call
greet("Alice");
```

---

## Multiple Parameters

```aria
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// Call with positional arguments
Result: i32 = add(5, 10);
```

---

## Parameter Types

### Primitives

```aria
fn process(
    id: i32,
    price: f64,
    active: bool,
    letter: char
) {
    // Implementation
}
```

### Complex Types

```aria
fn format_user(
    user: User,
    options: FormatOptions,
    template: string
) -> string {
    // Implementation
}
```

### References

```aria
fn modify(data: $Vec<i32>) {
    // data is mutable reference
    $data.push(42);
}

fn read(data: Vec<i32>) {
    // data is owned/copied
    stdout << data.len() << "\n";
}
```

---

## Optional Parameters (via Option Type)

Aria doesn't have default parameters, use `Option<T>`:

```aria
fn greet(name: string, title: string?) {
    when title != nil then
        stdout << title << " " << name << "\n";
    else
        stdout << name << "\n";
    end
}

// Call
greet("Alice", "Dr.");
greet("Bob", nil);
```

---

## Multiple Parameters of Same Type

```aria
fn calculate_area(width: i32, height: i32) -> i32 {
    return width * height;
}

// Must specify each type
// ❌ Wrong: fn calculate_area(width, height: i32)
// ✅ Right: fn calculate_area(width: i32, height: i32)
```

---

## Array Parameters

```aria
fn sum(numbers: []i32) -> i32 {
    total: i32 = 0;
    till(numbers.length - 1, 1) {
        num: i32 = numbers[$];
        total = total + num;
    }
    return total;
}

// Call
Result: i32 = sum([1, 2, 3, 4, 5]);
```

---

## Tuple Parameters

```aria
fn process_pair(pair: (i32, string)) {
    (num, text) = pair;
    stdout << num << ": " << text << "\n";
}

// Call
process_pair((42, "answer"));
```

---

## Struct Parameters

```aria
struct Point {
    x: i32,
    y: i32
}

fn distance(p: Point) -> f64 {
    return sqrt((p.x * p.x + p.y * p.y) as f64);
}

// Call
point: Point = Point{x: 3, y: 4};
dist: f64 = distance(point);
```

---

## Function Parameters (Higher-Order)

```aria
fn apply(value: i32, transform: fn(i32) -> i32) -> i32 {
    return transform(value);
}

// Call with lambda
Result: i32 = apply(5, |x| x * 2);

// Call with named function
fn double(x: i32) -> i32 { return x * 2; }
Result: i32 = apply(5, double);
```

---

## Generic Parameters

```aria
fn identity<T>(value: T) -> T {
    return value;
}

// T inferred from argument
num: i32 = identity(42);
text: string = identity("hello");
```

---

## Mutable Parameters

```aria
fn increment(count: $i32) {
    $count = $count + 1;
}

// Usage
value: i32 = 10;
increment($value);
stdout << value;  // 11
```

---

## Self Parameter (Methods)

```aria
struct Counter {
    count: i32
}

impl Counter {
    // Immutable self
    fn get() -> i32 {
        return self.count;
    }
    
    // Mutable self
    fn increment() {
        self.count = self.count + 1;
    }
    
    // Consuming self
    fn into_i32() -> i32 {
        return self.count;
    }
}
```

---

## Parameter Patterns

### Destructuring Tuple Parameter

```aria
fn process((x, y): (i32, i32)) -> i32 {
    return x + y;
}

// Call
Result: i32 = process((5, 10));  // 15
```

### Underscore for Unused

```aria
fn get_first((first, _): (i32, i32)) -> i32 {
    return first;
}
```

---

## Parameter Order Convention

```aria
// Good: Most important/required first
fn create_user(
    name: string,      // Required
    email: string,     // Required
    age: i32?,         // Optional
    bio: string?       // Optional
) -> User {
    // Implementation
}
```

---

## Variadic Parameters (Not Supported)

```aria
// ❌ Wrong: No variadic parameters
// fn sum(numbers: ...i32) -> i32 { }

// ✅ Right: Use array
fn sum(numbers: []i32) -> i32 {
    total: i32 = 0;
    till(numbers.length - 1, 1) {
        num: i32 = numbers[$];
        total = total + num;
    }
    return total;
}

// Call
Result: i32 = sum([1, 2, 3, 4, 5]);
```

---

## Named Arguments (Not Supported)

```aria
// ❌ Wrong: No named arguments
// user = create_user(name: "Alice", age: 30);

// ✅ Right: Use struct for clarity
struct UserParams {
    name: string,
    email: string,
    age: i32?
}

fn create_user(params: UserParams) -> User {
    // Implementation
}

// Call
user = create_user(UserParams{
    name: "Alice",
    email: "alice@example.com",
    age: 30
});
```

---

## Default Parameters (Not Supported)

```aria
// ❌ Wrong: No default parameters
// fn greet(name: string = "World") { }

// ✅ Right: Use separate functions or Option
fn greet() {
    greet_name("World");
}

fn greet_name(name: string) {
    stdout << "Hello, " << name << "\n";
}

// Or use Option
fn greet_optional(name: string?) {
    actual_name: string = name ?? "World";
    stdout << "Hello, " << actual_name << "\n";
}
```

---

## Best Practices

### ✅ DO: Use Descriptive Names

```aria
// Good: Clear parameter names
fn create_invoice(customer_id: i32, amount: f64, date: Date) { }
```

### ✅ DO: Keep Parameter Count Low

```aria
// Good: 3-4 parameters
fn format_address(street: string, city: string, zip: string) { }

// Better for many parameters: use struct
struct Address {
    street: string,
    city: string,
    state: string,
    zip: string,
    country: string
}

fn format_address(addr: Address) { }
```

### ✅ DO: Use Types That Prevent Errors

```aria
// Good: Type safety
fn set_age(age: u8) { }  // Can't be negative

// Instead of:
// fn set_age(age: i32) { }  // Could be negative
```

### ❌ DON'T: Use Too Many Parameters

```aria
// Wrong: Too many parameters
fn create_user(
    name: string,
    email: string,
    age: i32,
    address: string,
    phone: string,
    bio: string,
    avatar: string,
    role: string
) { }

// Right: Use struct
struct UserData {
    name: string,
    email: string,
    age: i32,
    // ... other fields
}

fn create_user(data: UserData) { }
```

### ❌ DON'T: Use Ambiguous Types

```aria
// Wrong: What do these booleans mean?
fn configure(true, false, true) { }

// Right: Use enum or named struct
enum Mode { Active, Inactive }
fn configure(mode: Mode, debug: bool, verbose: bool) { }
```

---

## Related Topics

- [Function Arguments](function_arguments.md) - Argument passing
- [Function Declaration](function_declaration.md) - Function basics
- [Function Syntax](function_syntax.md) - Complete syntax reference

---

**Remember**: Aria requires **explicit types** for all parameters - no inference at function boundaries!
