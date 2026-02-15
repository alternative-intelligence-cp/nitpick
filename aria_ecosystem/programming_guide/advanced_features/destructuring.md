# Destructuring

**Category**: Advanced Features → Patterns  
**Purpose**: Extract values from complex data structures

---

## Overview

Destructuring **unpacks** data structures into individual variables.

---

## Tuple Destructuring

```aria
point: (i32, i32) = (10, 20);

// Destructure into variables
(x, y): (i32, i32) = point;

stdout << "$x, $y";  // 10, 20
```

---

## Struct Destructuring

```aria
struct Point {
    x: i32,
    y: i32,
}

p: Point = Point { x: 10, y: 20 };

// Destructure fields
Point { x, y } = p;

stdout << "$x, $y";  // 10, 20
```

---

## Partial Destructuring

```aria
struct User {
    name: string,
    age: i32,
    email: string,
}

user: User = User {
    name: "Alice",
    age: 30,
    email: "alice@example.com",
};

// Extract only needed fields
User { name, .. } = user;

stdout << name;  // "Alice"
```

---

## Renaming Fields

```aria
struct Config {
    host: string,
    port: i32,
}

config: Config = Config { host: "localhost", port: 8080 };

// Rename during destructuring
Config { host: server, port: server_port } = config;

stdout << "$server:$server_port";  // localhost:8080
```

---

## Array Destructuring

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

// First two elements
[first, second, ...rest] = numbers;

stdout << "First: $first";   // First: 1
stdout << "Second: $second"; // Second: 2
stdout << "Rest: $rest";     // Rest: [3, 4, 5]
```

---

## Nested Destructuring

```aria
struct Address {
    street: string,
    city: string,
}

struct Person {
    name: string,
    address: Address,
}

person: Person = Person {
    name: "Alice",
    address: Address {
        street: "123 Main St",
        city: "Springfield",
    },
};

// Nested destructuring
Person { name, address: Address { city, .. } } = person;

stdout << "$name from $city";  // Alice from Springfield
```

---

## Function Parameters

```aria
fn print_point((x, y): (i32, i32)) {
    stdout << "Point at ($x, $y)";
}

fn greet(User { name, age, .. }: User) {
    stdout << "Hello $name, age $age";
}

fn main() {
    print_point((10, 20));
    
    user: User = User { name: "Bob", age: 25, email: "bob@example.com" };
    greet(user);
}
```

---

## Match Destructuring

```aria
enum Message {
    Quit,
    Move(i32, i32),
    Write(string),
    ChangeColor(i32, i32, i32),
}

fn process(msg: Message) {
    match msg {
        Message.Quit => {
            stdout << "Quitting";
        }
        Message.Move(x, y) => {
            stdout << "Moving to ($x, $y)";
        }
        Message.Write(text) => {
            stdout << "Writing: $text";
        }
        Message.ChangeColor(r, g, b) => {
            stdout << "Color: RGB($r, $g, $b)";
        }
    }
}
```

---

## If-Let Destructuring

```aria
fn process_optional(opt: ?Point) {
    if let Some(Point { x, y }) = opt {
        stdout << "Point at ($x, $y)";
    } else {
        stdout << "No point";
    }
}

fn handle_result(Result: Result<User>) {
    if let Ok(User { name, age, .. }) = result {
        stdout << "$name is $age years old";
    } else {
        stderr << "Failed to get user";
    }
}
```

---

## While-Let Destructuring

```aria
fn process_iter(iter: Iterator<(string, i32)>) {
    while let Some((key, value)) = iter.next() {
        stdout << "$key = $value";
    }
}

fn process_results(results: []Result<Data>) {
    i: i32 = 0;
    while let Some(Ok(data)) = results[i] {
        process(data);
        i += 1;
    }
}
```

---

## Common Patterns

### Swapping Values

```aria
a: i32 = 5;
b: i32 = 10;

// Swap using tuple destructuring
(a, b) = (b, a);

stdout << "$a, $b";  // 10, 5
```

---

### Multiple Return Values

```aria
fn divide_with_remainder(a: i32, b: i32) -> (i32, i32) {
    quotient: i32 = a / b;
    remainder: i32 = a % b;
    return (quotient, remainder);
}

fn main() {
    (quot, rem) = divide_with_remainder(17, 5);
    stdout << "17 / 5 = $quot remainder $rem";  // 17 / 5 = 3 remainder 2
}
```

---

### Extracting Fields

```aria
struct Response {
    status: i32,
    body: string,
    headers: HashMap<string, string>,
}

fn process_response(resp: Response) {
    // Extract only what we need
    Response { status, body, .. } = resp;
    
    if status == 200 {
        stdout << body;
    } else {
        stderr << "Error: status $status";
    }
}
```

---

### Iterating with Destructuring

```aria
users: []User = get_users();

till(users.length - 1, 1) {
    User { name, age, .. } = users[$];
    if age >= 18 {
        stdout << "$name is an adult";
    }
}

// With tuples
points: [](i32, i32) = [(0, 0), (1, 1), (2, 4)];

till(points.length - 1, 1) {
    (x, y) = points[$];
    stdout << "Point at ($x, $y)";
}
```

---

### Destructuring in Closures

```aria
pairs: [](string, i32) = [("a", 1), ("b", 2), ("c", 3)];

// Destructure parameters
pairs.forEach(|(key, value)| {
    stdout << "$key = $value";
});

results: []Result<Data> = fetch_all();

valid_data: []Data = results
    .filterMap(|result| {
        if let Ok(data) = result {
            return Some(data);
        }
        return None;
    });
```

---

## Best Practices

### ✅ DO: Destructure to Improve Readability

```aria
// ❌ Repetitive
fn process(user: User) {
    stdout << user.name;
    stdout << user.email;
    stdout << user.age;
}

// ✅ Clean with destructuring
fn process(User { name, email, age, .. }: User) {
    stdout << name;
    stdout << email;
    stdout << age;
}
```

### ✅ DO: Use .. for Partial Extraction

```aria
struct Config {
    host: string,
    port: i32,
    timeout: i32,
    retries: i32,
    debug: bool,
}

fn connect(Config { host, port, .. }: Config) {
    // Only need host and port
    establish_connection(host, port);
}
```

### ✅ DO: Rename to Avoid Conflicts

```aria
fn merge_configs(config1: Config, config2: Config) {
    Config { host: host1, port: port1, .. } = config1;
    Config { host: host2, port: port2, .. } = config2;
    
    // Use renamed variables
    final_host: string = if host1 != "" { host1 } else { host2 };
}
```

### ⚠️ WARNING: Be Careful with Nested Destructuring

```aria
// ⚠️ Can become hard to read
Person { 
    name, 
    address: Address { 
        street, 
        city: City { 
            name: city_name, 
            state: State { abbreviation, .. }, 
            .. 
        }, 
        .. 
    }, 
    .. 
} = person;

// ✅ Better - destructure in steps
Person { name, address, .. } = person;
Address { city, .. } = address;
City { name: city_name, .. } = city;
```

### ❌ DON'T: Ignore All Fields Unnecessarily

```aria
// ❌ Bad - extracts nothing useful
fn process(User { .. }: User) {
    // Just use the whole user
}

// ✅ Good - extract what you need
fn process(User { name, age, .. }: User) {
    stdout << "$name is $age";
}

// ✅ Or don't destructure at all
fn process(user: User) {
    stdout << "${user.name} is ${user.age}";
}
```

---

## Related

- [pattern_matching](pattern_matching.md) - Pattern matching
- [error_propagation](error_propagation.md) - Error handling patterns

---

**Remember**: Destructuring makes code **cleaner** by extracting exactly what you need!
