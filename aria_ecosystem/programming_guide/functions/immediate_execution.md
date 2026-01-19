# Immediate Execution (IIFE)

**Category**: Functions → Advanced  
**Concept**: Define and call a function immediately  
**Also Known As**: IIFE (Immediately Invoked Function Expression)

---

## What is Immediate Execution?

**Immediate execution** means defining an anonymous function and **calling it right away** in the same expression.

---

## Basic Syntax

```aria
// Define and call immediately
Result: i32 = (|| {
    return 42;
})();  // Parentheses invoke the function

stdout << result;  // 42
```

---

## Why Use Immediate Execution?

### 1. Create Private Scope

```aria
// Variables inside don't leak out
value: i32 = (|| {
    temp: i32 = 42;
    other: i32 = temp * 2;
    return other;
})();

// 'temp' and 'other' don't exist here
stdout << value;  // 84
```

### 2. Complex Initialization

```aria
config: Config = (|| {
    base: Config = load_default_config();
    
    when environment == "production" then
        base.timeout = 5000;
        base.retry = true;
    else
        base.timeout = 1000;
        base.retry = false;
    end
    
    return base;
})();
```

### 3. One-Time Setup

```aria
initialized: bool = (|| {
    setup_logging();
    connect_database();
    load_plugins();
    return true;
})();
```

---

## With Parameters

You can pass parameters to immediately executed functions:

```aria
Result: i32 = (|x, y| {
    return x * y;
})(5, 10);  // 50
```

---

## Multi-Line Body

```aria
data: Data = (|| {
    // Complex logic
    raw: string = fetch_from_source();
    
    when raw == ERR then
        return Data::default();
    end
    
    parsed: Data = pass parse(raw);
    validated: Data = pass validate(parsed);
    
    return validated;
})();
```

---

## Async Immediate Execution

```aria
// Async IIFE
data: Data = await (async || {
    user: User = await fetch_user();
    orders: []Order = await fetch_orders(user.id);
    return process(user, orders);
})();
```

---

## Error Handling with IIFE

```aria
Result: i32? = (|| -> i32? {
    file: File = pass open_file("data.txt");
    content: string = pass file.read();
    number: i32 = pass content.parse();
    return number;
})();

when result == ERR then
    stderr << "Failed to load number\n";
end
```

---

## Comparison with Named Functions

### Named Function

```aria
fn calculate_tax(price: f64) -> f64 {
    rate: f64 = 0.08;
    return price * rate;
}

tax: f64 = calculate_tax(100.0);
```

### Immediate Execution

```aria
// Don't need to name it - used once
tax: f64 = (|price: f64| {
    rate: f64 = 0.08;
    return price * rate;
})(100.0);
```

---

## Common Patterns

### Conditional Initialization

```aria
port: i32 = (|| {
    env_port: string? = get_env("PORT");
    
    when env_port != nil then
        return parse(env_port) ?? 8080;
    else
        return 8080;
    end
})();
```

### Resource Cleanup

```aria
Result: Data = (|| {
    file: File = open("data.txt");
    defer file.close();  // Cleanup on exit
    
    content: string = file.read();
    return parse(content);
})();
```

### Lazy Evaluation

```aria
// Only calculate if needed
expensive_value: i32 = (|| {
    when cheap_check() then
        return 0;
    end
    
    return expensive_calculation();
})();
```

---

## JavaScript Comparison

### JavaScript IIFE

```javascript
// JavaScript
const result = (function() {
    const temp = 42;
    return temp * 2;
})();
```

### Aria IIFE

```aria
// Aria
Result: i32 = (|| {
    temp: i32 = 42;
    return temp * 2;
})();
```

---

## Best Practices

### ✅ DO: Use for Complex Initialization

```aria
// Good: Complex logic isolated
config: Config = (|| {
    base: Config = Config::default();
    
    // 10 lines of setup logic
    
    return base;
})();
```

### ✅ DO: Use for One-Time Setup

```aria
// Good: Setup runs once, result stored
logger: Logger = (|| {
    init_logging_system();
    return Logger::new();
})();
```

### ✅ DO: Use to Avoid Temporary Variables

```aria
// Good: No temp variables leak
user_count: i32 = (|| {
    users: []User = load_users();
    active: []User = users.filter(|u| u.active);
    return active.len();
})();
```

### ❌ DON'T: Overuse for Simple Cases

```aria
// Wrong: Too complex for simple value
x: i32 = (|| {
    return 42;
})();

// Right: Just assign directly
x: i32 = 42;
```

### ❌ DON'T: Make Them Too Large

```aria
// Wrong: 100 lines in IIFE
data: Data = (|| {
    // 100 lines of complex logic...
})();

// Right: Extract to named function
fn initialize_data() -> Data {
    // 100 lines of complex logic...
}

data: Data = initialize_data();
```

---

## Real-World Examples

### Configuration Loading

```aria
app_config: Config = (|| {
    // Load from multiple sources
    defaults: Config = Config::default();
    
    env_config: Config? = load_from_env();
    when env_config != nil then
        defaults = defaults.merge(env_config);
    end
    
    file_config: Config? = load_from_file("config.toml");
    when file_config != nil then
        defaults = defaults.merge(file_config);
    end
    
    return defaults;
})();
```

### Singleton Pattern

```aria
struct Database {
    connection: Connection
}

static DB: Database = (|| {
    conn: Connection = Connection::new("localhost:5432");
    conn.connect();
    return Database{connection: conn};
})();
```

### Feature Detection

```aria
has_feature: bool = (|| {
    when !check_basic_support() then
        return false;
    end
    
    when !check_advanced_support() then
        return false;
    end
    
    return true;
})();
```

### Data Pipeline

```aria
final_data: []User = (|| {
    raw: []User = load_users();
    
    filtered: []User = raw
        .filter(|u| u.age >= 18)
        .filter(|u| u.active);
    
    sorted: []User = filtered
        .sort(|a, b| a.name.compare(b.name));
    
    return sorted.take(100);
})();
```

---

## When to Use

### ✅ Use IIFE When:

- Need private scope for variables
- Complex initialization logic
- One-time setup code
- Conditional value assignment
- Want to avoid helper functions

### ❌ Don't Use IIFE When:

- Simple value assignment
- Logic will be reused
- Code is very long (>20 lines)
- Function should be named for clarity
- Testing the logic separately

---

## Alternative: Block Expressions

Some languages allow block expressions for similar effect:

```aria
// If Aria supports block expressions
value: i32 = {
    temp: i32 = 42;
    temp * 2
};

// Otherwise use IIFE
value: i32 = (|| {
    temp: i32 = 42;
    return temp * 2;
})();
```

---

## Related Topics

- [Lambda Expressions](lambda.md) - Anonymous functions
- [Anonymous Functions](anonymous_functions.md) - Nameless functions
- [Closures](closures.md) - Variable capture
- [Function Declaration](function_declaration.md) - Named functions

---

**Remember**: IIFE is **define and call immediately** - perfect for complex initialization, private scope, and one-time execution!
