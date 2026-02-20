# The `string` Type

**Category**: Types → Primitives  
**Syntax**: `string`  
**Purpose**: UTF-8 encoded text

---

## Overview

`string` represents **UTF-8 encoded text**. Strings are immutable and reference-counted.

---

## Specifications

| Aspect | Value |
|--------|-------|
| **Encoding** | UTF-8 |
| **Mutability** | Immutable |
| **Memory** | Reference-counted |
| **Null-terminated** | No (length-tracked) |

---

## Declaration

```aria
// String literals
name: string = "Alice";
message: string = "Hello, World!";

// Empty string
empty: string = "";

// Multi-line
poem: string = "Roses are red,
Violets are blue,
Aria is awesome,
And so are you!";
```

---

## String Literals

### Regular Strings

```aria
// Double quotes
text: string = "Hello";

// Escape sequences
escaped: string = "Line 1\nLine 2\tTabbed";
quote: string = "He said \"Hello\"";
```

### Raw Strings

```aria
// Backticks - no escape processing
path: string = `C:\Users\name\file.txt`;
regex: string = `\d+\.\d+`;
```

### Interpolation

```aria
// Embed variables
name: string = "Alice";
greeting: string = "Hello, $name!";

// Expressions
x: i32 = 10;
msg: string = "The value is ${x + 5}";
```

---

## Operations

### Concatenation

```aria
first: string = "Hello";
second: string = "World";

// Using +
combined: string = first + " " + second;  // "Hello World"

// Using interpolation (better)
combined: string = "$first $second";
```

### Comparison

```aria
a: string = "apple";
b: string = "banana";

// Equality
is_same: bool = (a == b);  // false

// Ordering
is_less: bool = (a < b);  // true (lexicographic)
```

### Length

```aria
text: string = "Hello";
len: i32 = text.length();  // 5

// UTF-8 aware
emoji: string = "👋🌍";
len: i32 = emoji.length();  // 2 (characters, not bytes)
byte_len: i32 = emoji.byte_length();  // 8 bytes
```

---

## Common Methods

### Case Conversion

```aria
text: string = "Hello World";

upper: string = text.to_uppercase();    // "HELLO WORLD"
lower: string = text.to_lowercase();    // "hello world"
```

### Trimming

```aria
text: string = "  hello  ";

trimmed: string = text.trim();         // "hello"
left: string = text.trim_left();       // "hello  "
right: string = text.trim_right();     // "  hello"
```

### Splitting

```aria
text: string = "apple,banana,cherry";

parts: []string = text.split(",");
// ["apple", "banana", "cherry"]
```

### Substring

```aria
text: string = "Hello World";

// Slice notation
sub: string = text[0..5];   // "Hello"
sub: string = text[6..11];  // "World"
```

### Contains/StartsWith/EndsWith

```aria
text: string = "Hello World";

has: bool = text.contains("World");      // true
starts: bool = text.starts_with("Hello"); // true
ends: bool = text.ends_with("World");    // true
```

---

## Use Cases

### Text Processing

```aria
fn process_log(line: string) {
    when line.starts_with("ERROR") then
        handle_error(line);
    elsif line.starts_with("WARN") then
        handle_warning(line);
    end
}
```

### Building Messages

```aria
fn format_user(name: string, age: i32) -> string {
    return "User: $name (age $age)";
}

msg: string = format_user("Alice", 30);
// "User: Alice (age 30)"
```

### Parsing

```aria
fn parse_config(line: string) -> Result<(string, string)> {
    parts: []string = line.split("=");
    
    when parts.length() != 2 then
        fail("Invalid format");
    end
    
    key: string = parts[0].trim();
    value: string = parts[1].trim();
    
    pass((key, value));
}
```

---

## Best Practices

### ✅ DO: Use Interpolation

```aria
// Clear and efficient
name: string = "Alice";
age: i32 = 30;
msg: string = "Name: $name, Age: $age";
```

### ✅ DO: Use Raw Strings for Paths

```aria
// No escaping needed
path: string = `C:\Users\alice\Documents`;
```

### ❌ DON'T: Concatenate in Loops

```aria
// Inefficient - creates many intermediate strings
Result: string = "";
till(999, 1) {
    result += "line\n";  // ❌ Slow!
}

// Better - use StringBuilder or collect
lines: []string = aria_alloc([]string, 1000);
till(999, 1) {
    lines[$] = "line";
}
Result: string = lines.join("\n");  // ✅
```

### ❌ DON'T: Assume ASCII

```aria
// Wrong for Unicode
text: string = "Hello 👋";
when text.length() == 6 then  // False! Length is 7
    ...
end

// Check byte length if needed
byte_len: i32 = text.byte_length();
```

---

## Escapes

```aria
// Common escape sequences
newline: string = "\n";
tab: string = "\t";
backslash: string = "\\";
quote: string = "\"";
unicode: string = "\u{1F44B}";  // 👋
```

---

## Immutability

```aria
// Strings are immutable
text: string = "Hello";
text[0] = 'h';  // ❌ Error! Cannot modify

// Create new string instead
text = text.to_lowercase();  // ✅ New string
```

---

## Memory Management

```aria
// Reference-counted - no manual free needed
fn create_message() -> string {
    msg: string = "Hello";
    return msg;  // Ref count incremented
}  // Original ref count decremented

Result: string = create_message();
// String lives until result goes out of scope
```

---

## Common Patterns

### Template Formatting

```aria
fn format_email(user: string, domain: string) -> string {
    return "${user}@${domain}";
}

email: string = format_email("alice", "example.com");
// "alice@example.com"
```

### Validation

```aria
fn is_valid_username(name: string) -> bool {
    when name.is_empty() or name.length() < 3 then
        return false;
    end
    
    when not name.chars().all(is_alphanumeric) then
        return false;
    end
    
    return true;
}
```

### Joining

```aria
// Join array of strings
names: []string = ["Alice", "Bob", "Charlie"];
Result: string = names.join(", ");
// "Alice, Bob, Charlie"
```

---

## Related

- [String Interpolation (${})](../operators/interpolation.md)
- [Raw Strings (`)](../operators/backtick.md)
- [Arrays ([])](array.md)

---

**Remember**: Strings are **UTF-8** and **immutable**!
