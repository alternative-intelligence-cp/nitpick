# Function Arguments

**Category**: Functions → Calling  
**Concept**: Values passed to functions  
**Relation**: Arguments are the actual values; parameters are the placeholders

---

## Parameters vs Arguments

```aria
// Parameters: name and age
fn greet(name: string, age: i32) {
    stdout << "Hello, " << name << ", age " << age << "\n";
}

// Arguments: "Alice" and 30
greet("Alice", 30);
```

---

## Positional Arguments

Arguments must match parameter order:

```aria
fn divide(numerator: i32, denominator: i32) -> i32 {
    return numerator / denominator;
}

// Order matters!
Result: i32 = divide(10, 2);  // 5
Result: i32 = divide(2, 10);  // 0
```

---

## Type Matching

Arguments must match parameter types:

```aria
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// ✅ Correct: Types match
Result: i32 = add(5, 10);

// ❌ Error: Wrong types
// result = add(5.5, 10);     // f64, not i32
// result = add("5", "10");   // string, not i32
```

---

## Argument Count

Must match parameter count exactly:

```aria
fn calculate(a: i32, b: i32, c: i32) -> i32 {
    return a + b + c;
}

// ✅ Correct: 3 arguments
Result: i32 = calculate(1, 2, 3);

// ❌ Error: Wrong count
// result = calculate(1, 2);        // Too few
// result = calculate(1, 2, 3, 4);  // Too many
```

---

## Literal Arguments

```aria
fn process(id: i32, name: string, active: bool) {
    // Implementation
}

// Direct literals
process(123, "Alice", true);
```

---

## Variable Arguments

```aria
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

x: i32 = 5;
y: i32 = 10;

// Pass variables
Result: i32 = add(x, y);
```

---

## Expression Arguments

```aria
fn multiply(a: i32, b: i32) -> i32 {
    return a * b;
}

// Expressions as arguments
Result: i32 = multiply(2 + 3, 4 * 5);  // (5, 20) = 100
Result: i32 = multiply(get_x(), get_y());
```

---

## Array Arguments

```aria
fn sum(numbers: []i32) -> i32 {
    total: i32 = 0;
    till(numbers.length - 1, 1) {
        total = total + numbers[$];
    }
    return total;
}

// Array literal
Result: i32 = sum([1, 2, 3, 4, 5]);

// Array variable
numbers: []i32 = [10, 20, 30];
Result: i32 = sum(numbers);
```

---

## Tuple Arguments

```aria
fn process(pair: (i32, string)) {
    (num, text) = pair;
    stdout << num << ": " << text << "\n";
}

// Tuple literal
process((42, "answer"));

// Tuple variable
data: (i32, string) = (100, "hundred");
process(data);
```

---

## Struct Arguments

```aria
struct Point {
    x: i32,
    y: i32
}

fn print_point(p: Point) {
    stdout << "(" << p.x << ", " << p.y << ")\n";
}

// Struct literal
print_point(Point{x: 10, y: 20});

// Struct variable
point: Point = Point{x: 5, y: 15};
print_point(point);
```

---

## Function Arguments (Callbacks)

```aria
fn apply(value: i32, f: fn(i32) -> i32) -> i32 {
    return f(value);
}

// Lambda as argument
Result: i32 = apply(5, |x| x * 2);

// Named function as argument
fn double(x: i32) -> i32 { return x * 2; }
Result: i32 = apply(5, double);

// Variable holding function
doubler: fn(i32) -> i32 = |x| x * 2;
Result: i32 = apply(5, doubler);
```

---

## Optional Arguments (using Option)

```aria
fn greet(name: string, title: string?) {
    when title != nil then
        stdout << title << " " << name << "\n";
    else
        stdout << name << "\n";
    end
}

// With value
greet("Alice", "Dr.");

// Without value (nil)
greet("Bob", nil);
```

---

## Mutable Arguments

```aria
fn increment(count: $i32) {
    $count = $count + 1;
}

// Must pass with $
value: i32 = 10;
increment($value);  // Pass by reference
stdout << value;    // 11
```

---

## Generic Arguments

```aria
fn identity<T>(value: T) -> T {
    return value;
}

// Type inferred from argument
num: i32 = identity(42);        // T = i32
text: string = identity("hi");   // T = string

// Explicit type
num: i32 = identity::<i32>(42);
```

---

## Evaluation Order

Arguments are evaluated **left to right**:

```aria
fn test(a: i32, b: i32, c: i32) {
    stdout << a << ", " << b << ", " << c << "\n";
}

count: i32 = 0;

fn next() -> i32 {
    $count = $count + 1;
    return $count;
}

// Evaluates: next(), next(), next()
test(next(), next(), next());  // "1, 2, 3"
```

---

## Argument Passing Semantics

### Copy Types (Pass by Value)

```aria
fn modify(x: i32) {
    x = x + 10;  // Modifies local copy
}

value: i32 = 5;
modify(value);
stdout << value;  // Still 5 (not modified)
```

### Move Types (Ownership Transfer)

```aria
fn consume(data: Vec<i32>) {
    // data is now owned by this function
    stdout << data.len() << "\n";
}

vec: Vec<i32> = vec![1, 2, 3];
consume(vec);
// vec is no longer valid here - was moved
```

### Reference Types (Pass by Reference)

```aria
fn modify(data: $Vec<i32>) {
    $data.push(4);  // Modifies original
}

vec: Vec<i32> = vec![1, 2, 3];
modify($vec);  // Pass reference
stdout << vec.len();  // 4 (modified)
```

---

## Argument Patterns

### Spread Arguments (Not Supported)

```aria
// ❌ Wrong: No spread operator
// numbers: []i32 = [1, 2, 3];
// result = add(...numbers);

// ✅ Right: Pass array directly
fn sum(numbers: []i32) -> i32 { ... }
result = sum([1, 2, 3]);
```

### Named Arguments (Not Supported)

```aria
// ❌ Wrong: No named arguments
// user = create_user(name: "Alice", age: 30);

// ✅ Right: Positional only
user = create_user("Alice", 30);

// Or use struct for clarity
params = UserParams{name: "Alice", age: 30};
user = create_user(params);
```

---

## Method Call Arguments

```aria
struct Calculator;

impl Calculator {
    fn add(a: i32, b: i32) -> i32 {
        return a + b;
    }
}

calc: Calculator = Calculator;

// Method call - arguments after self
Result: i32 = calc.add(5, 10);
```

---

## Common Patterns

### Builder Pattern Arguments

```aria
user: User = UserBuilder::new()
    .name("Alice")
    .email("alice@example.com")
    .age(30)
    .build();
```

### Configuration Struct

```aria
struct Config {
    host: string,
    port: i32,
    timeout: i32,
    retry: bool
}

fn connect(config: Config) -> Connection {
    // Use config.host, config.port, etc.
}

// Clear what each argument means
connection = connect(Config{
    host: "localhost",
    port: 8080,
    timeout: 5000,
    retry: true
});
```

---

## Best Practices

### ✅ DO: Use Clear Variable Names

```aria
// Good: Clear what's being passed
user_id: i32 = 123;
user_name: string = "Alice";
create_user(user_id, user_name);
```

### ✅ DO: Use Structs for Many Arguments

```aria
// Good: Clear structure
request: HttpRequest = HttpRequest{
    method: "GET",
    url: "/api/users",
    headers: headers,
    body: nil
};
send_request(request);
```

### ✅ DO: Validate Before Passing

```aria
// Good: Validate first
age: i32 = parse_age(input);
when age >= 0 and age <= 120 then
    set_user_age(user, age);
else
    stderr << "Invalid age\n";
end
```

### ❌ DON'T: Use Magic Numbers

```aria
// Wrong: What does 5 mean?
process(data, 5, true);

// Right: Named constants
MAX_RETRIES: i32 = 5;
VERBOSE: bool = true;
process(data, MAX_RETRIES, VERBOSE);
```

### ❌ DON'T: Pass Wrong Order

```aria
// Wrong: Easy to mix up
fn create_rectangle(width: i32, height: i32) { }
create_rectangle(10, 20);  // Which is which?

// Better: Use struct or named types
struct Dimensions {
    width: i32,
    height: i32
}
fn create_rectangle(dims: Dimensions) { }
```

---

## Related Topics

- [Function Parameters](function_params.md) - Parameter definition
- [Function Declaration](function_declaration.md) - Function basics
- [Function Syntax](function_syntax.md) - Complete syntax

---

**Remember**: Arguments are the **actual values** you pass - they must match the parameter types and order exactly!
