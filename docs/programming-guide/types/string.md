# string: String Type

## Overview

**Purpose**: Text data, character sequences, and string manipulation with UTF-8 encoding.

**Characteristics**:
- UTF-8 encoded Unicode text
- Immutable by design (new strings on modification)
- Explicit length tracking (not null-terminated)
- Heap-allocated via GC
- Zero-copy substring views (future optimization)
- Efficient concatenation with `concat_n`

**When to Use**:
- Text data and messages
- File paths and URLs
- User input/output
- Configuration values
- Log messages
- String processing and parsing

**When NOT to Use**:
- Binary data (use `byte` array or `buffer`)
- Single characters (use `char`)
- Fixed-size text (consider `array<char, N>` for stack allocation)
- Performance-critical tight loops (consider `StringBuilder` or preallocated buffers)

---

## string vs char: Sequences vs Singles

### The Critical Distinction

**string**: Sequence of characters
- Purpose: Represent text (words, sentences, names)
- Encoding: UTF-8 (variable-length)
- Storage: Heap-allocated pointer + length
- Mutable: No (returns new string)
- Operations: Concatenation, searching, splitting
- Intent: "What does this text say?"

**char**: Single character
- Purpose: Represent one character/code point
- Encoding: UTF-8 (1-4 bytes)
- Storage: Value type (stack)
- Mutable: Yes (can reassign)
- Operations: Classification, comparison
- Intent: "What is this letter?"

### Examples of the Difference

```aria
// String: Text sequence
string:greeting = "Hello, World!";
string:name = "Alice";
string:message = greeting + " My name is " + name;

i64:length = greeting.length();  // 13 bytes
bool:has_world = greeting.contains("World");

// Char: Single character
char:first = greeting[0];  // 'H'
char:letter = 'A';
char:space = ' ';

if (letter.is_upper()) {
    log.write("Uppercase letter\n");
}
```

### Building Strings from Characters

```aria
// Inefficient: Repeated concatenation (O(n²))
string:result = "";
for (i: i32 = 0; i < 1000; i++) {
    result = result + "x";  // Creates new string each iteration!
}

// Efficient: Preallocate or use StringBuilder (future)
char:chars[1000];
for (i: i32 = 0; i < 1000; i++) {
    chars[i] = 'x';
}
string:result = string.from_chars(chars);
```

---

## String Literals

### Standard String Literals

```aria
string:simple = "Hello";
string:sentence = "The quick brown fox.";
string:empty = "";
```

### Escape Sequences

```aria
string:newline = "Line 1\nLine 2";
string:tab = "Column1\tColumn2";
string:quote = "He said \"Hello\"";
string:backslash = "Path: C:\\Users\\";
string:unicode = "Snowman: \u2603";  // ☃
```

### Raw Strings (Backtick)

```aria
// No escape processing
string:raw = `C:\Users\Documents\file.txt`;
string:regex = `\d+\.\d+`;  // Regex pattern (backslashes literal)
string:multiline = `Line 1
Line 2
Line 3`;

// Great for:
// - Windows paths (no need to escape \)
// - Regular expressions (literal backslashes)
// - SQL queries
// - Multiline text blocks
```

---

## String Interpolation

### Basic Interpolation

```aria
string:name = "Alice";
i32:age = 25;

// Using $ for variables
string:msg = "Hello, $name!";
// "Hello, Alice!"

// Using ${} for expressions
string:info = "Age next year: ${age + 1}";
// "Age next year: 26"
```

### Expression Interpolation

```aria
i32:x = 10;
i32:y = 20;

string:result = "${x} + ${y} = ${x + y}";
// "10 + 20 = 30"

f64:price = 19.99;
string:display = "Total: $${price * 1.08}";
// "Total: $21.59" (note: $$ to escape literal $)
```

### Method Call Interpolation

```aria
User:user = get_current_user();

string:status = "User: ${user.name()}, Balance: $${user.balance()}";
```

### Complex Expressions

```aria
// Function calls
string:time = "Current time: ${get_time().format()}";

// Conditional expressions
bool:is_admin = check_admin();
string:role = "Role: ${is_admin ? "Admin" : "User"}";

// Array indexing
string:names[3] = ["Alice", "Bob", "Carol"];
string:first = "First: ${names[0]}";
```

---

## UTF-8 Encoding

### What is UTF-8?

UTF-8 is a variable-length encoding where:
- ASCII characters (0-127): 1 byte
- Latin, Greek, Cyrillic: 2 bytes
- Most other scripts (Chinese, Japanese, Arabic): 3 bytes
- Emoji, rare characters: 4 bytes

### Character vs Byte Length

```aria
string:ascii = "Hello";     // 5 characters = 5 bytes
string:mixed = "Café";      // 4 characters = 5 bytes (é = 2 bytes)
string:emoji = "Hello 😊";  // 7 characters = 10 bytes (😊 = 4 bytes)

// .length() returns BYTES, not characters!
i64:byte_len = emoji.length();      // 10 bytes
i64:char_count = emoji.char_count(); // 7 characters
```

### Why Bytes Not Characters?

```aria
// Indexing by byte offset (fast, O(1))
string:text = "Hello, World!";
char:c = text[7];  // 'W' (byte index 7)

// Indexing by character (slow, O(n) scan for UTF-8)
char:c = text.char_at(7);  // 'W' (character index 7)

// Aria defaults to byte operations for performance
// Use char_* methods when you need character-level semantics
```

### Valid UTF-8 Sequences

```aria
// Aria strings are ALWAYS valid UTF-8
// Invalid bytes cause compile/runtime errors

string:valid = "Café ☕ 文字";  // Valid UTF-8
// string:invalid = "\xFF\xFE";  // ERROR: Invalid UTF-8 sequence

// When reading from external sources:
Result<string>:text = file.read_utf8("data.txt");
if (text.is_ok()) {
    // Guaranteed valid UTF-8
} else {
    // Handle encoding error
}
```

---

## AriaString Structure

### Internal Representation

```c
typedef struct {
    const char* data;   // UTF-8 byte data
    int64_t length;     // Length in bytes
} AriaString;
```

### Memory Model

```aria
// String literals: Static memory (read-only)
string:literal = "Hello";  // Points to .rodata section

// Runtime strings: GC-allocated heap memory
string:dynamic = "Hello" + " World";  // Heap allocation

// Concatenation creates new string
string:a = "Hello";
string:b = " World";
string:c = a + b;  // New allocation (a and b unchanged)
```

### Immutability

```aria
string:original = "Hello";
string:modified = original.to_upper();  // Returns NEW string

// original is unchanged
assert(original == "Hello");
assert(modified == "HELLO");

// This is safe (different strings in memory)
```

---

## String Operations

### Length and Capacity

```aria
string:text = "Hello, World!";

// Byte length (fast, O(1))
i64:bytes = text.length();  // 13 bytes

// Character count (slower, O(n) scan)
i64:chars = text.char_count();  // 13 characters

// Check if empty
bool:empty = text.is_empty();  // false
```

### Indexing and Slicing

```aria
string:text = "Hello, World!";

// Byte indexing (O(1))
char:c = text[0];   // 'H'
char:w = text[7];   // 'W'

// Substring (byte range)
string:sub = text[0:5];   // "Hello" (bytes 0-4)
string:world = text[7:12]; // "World" (bytes 7-11)

// WARNING: Slicing in the middle of a multi-byte character is an error!
string:emoji = "😊";
// string:invalid = emoji[0:1];  // ERROR: Cuts emoji in half
```

### Concatenation

```aria
// Using + operator
string:a = "Hello";
string:b = " World";
string:c = a + b;  // "Hello World"

// Multiple concatenation (O(n²) - inefficient)
string:result = "";
result = result + "a";  // Allocation 1
result = result + "b";  // Allocation 2 (copies "a" again)
result = result + "c";  // Allocation 3 (copies "ab" again)

// Efficient: concat_n (single allocation)
string:parts[3] = ["a", "b", "c"];
string:result = string.concat_n(parts, 3);  // One allocation
```

### String Building Pattern

```aria
// For dynamic string construction, collect parts then join
string:parts[100];
i32:count = 0;

for (i: i32 = 0; i < 10; i++) {
    parts[count++] = "Item ";
    parts[count++] = i.to_string();
    parts[count++] = "\n";
}

string:result = string.concat_n(parts, count);
```

---

## String Comparison

### Equality

```aria
string:a = "Hello";
string:b = "Hello";
string:c = "hello";

bool:same = (a == b);     // true (same content)
bool:different = (a == c); // false (case-sensitive)

// Pointer equality (same memory address)
bool:same_ptr = (a === b);  // Maybe false (different allocations)
```

### Ordering

```aria
string:a = "apple";
string:b = "banana";
string:c = "cherry";

bool:less = (a < b);      // true (lexicographic)
bool:greater = (c > b);   // true

// Spaceship operator
i32:cmp = (a <=> b);  // -1 (a < b)
cmp = (b <=> b);      // 0  (b == b)
cmp = (c <=> a);      // 1  (c > a)
```

### Case-Insensitive Comparison

```aria
string:a = "Hello";
string:b = "HELLO";

// Standard comparison (case-sensitive)
bool:same = (a == b);  // false

// Case-insensitive
bool:same_ci = a.equals_ignore_case(b);  // true
```

---

## Searching and Matching

### contains()

```aria
string:text = "The quick brown fox";

bool:has_quick = text.contains("quick");  // true
bool:has_slow = text.contains("slow");    // false
bool:has_fox = text.contains("fox");      // true
```

### index_of()

```aria
string:text = "Hello, World!";

Result<i64>:pos = text.index_of("World");
if (pos.is_ok()) {
    i64:index = pos.unwrap();  // 7
}

Result<i64>:not_found = text.index_of("Goodbye");
// not_found.is_err() == true
```

### starts_with() / ends_with()

```aria
string:filename = "document.pdf";

bool:is_pdf = filename.ends_with(".pdf");    // true
bool:is_doc = filename.starts_with("doc");   // true
bool:is_txt = filename.ends_with(".txt");    // false
```

### Pattern Matching (Future)

```aria
// Future: Regular expressions
string:text = "Price: $19.99";
Result<Match>:match = text.match(`\$(\d+\.\d+)`);

if (match.is_ok()) {
    string:price = match.unwrap().group(1);  // "19.99"
}
```

---

## String Manipulation

### Trimming Whitespace

```aria
string:text = "  Hello, World!  \n";

string:trimmed = text.trim();        // "Hello, World!"
string:left = text.trim_start();     // "Hello, World!  \n"
string:right = text.trim_end();      // "  Hello, World!"
```

### Case Conversion

```aria
string:mixed = "Hello, World!";

string:upper = mixed.to_upper();  // "HELLO, WORLD!"
string:lower = mixed.to_lower();  // "hello, world!"

// Note: ASCII only for now (full Unicode case folding is complex)
string:special = "Café";
string:upper_ascii = special.to_upper();  // "CAFé" (only ASCII converted)
```

### Replacing

```aria
string:text = "Hello, World! Hello!";

// Replace first occurrence
string:replaced = text.replace("Hello", "Goodbye");
// "Goodbye, World! Hello!"

// Replace all occurrences
string:all = text.replace_all("Hello", "Hi");
// "Hi, World! Hi!"
```

### Repeating

```aria
string:dash = "-";
string:separator = dash.repeat(40);
// "----------------------------------------"

string:indent = "  ".repeat(3);  // "      " (6 spaces)
```

---

## Splitting and Joining

### split()

```aria
string:csv = "apple,banana,cherry";
string:fruits[] = csv.split(",");
// ["apple", "banana", "cherry"]

string:path = "/usr/local/bin";
string:parts[] = path.split("/");
// ["", "usr", "local", "bin"]
```

### split_whitespace()

```aria
string:text = "The  quick   brown\nfox";
string:words[] = text.split_whitespace();
// ["The", "quick", "brown", "fox"]
```

### join()

```aria
string:words[3] = ["apple", "banana", "cherry"];
string:csv = string.join(words, ", ");
// "apple, banana, cherry"

string:path_parts[3] = ["usr", "local", "bin"];
string:path = "/" + string.join(path_parts, "/");
// "/usr/local/bin"
```

---

## Type Conversions

### From Integers

```aria
i32:num = 42;
string:text = num.to_string();  // "42"

i64:big = 1234567890;
string:big_str = big.to_string();  // "1234567890"

// With formatting
string:hex = num.to_hex();      // "2a"
string:binary = num.to_binary(); // "101010"
```

### From Floats

```aria
f64:pi = 3.14159265;
string:text = pi.to_string();  // "3.14159265" (default precision)

// With precision control
string:short = pi.to_string(2);  // "3.14"
string:long = pi.to_string(8);   // "3.14159265"
```

### To Integers

```aria
string:text = "42";
Result<i32>:num = text.parse_i32();

if (num.is_ok()) {
    i32:value = num.unwrap();  // 42
} else {
    // Handle parse error
}

// Different bases
string:hex = "FF";
Result<i32>:value = hex.parse_i32_base(16);  // 255

string:binary = "1010";
Result<i32>:value = binary.parse_i32_base(2);  // 10
```

### To Floats

```aria
string:text = "3.14159";
Result<f64>:pi = text.parse_f64();

if (pi.is_ok()) {
    f64:value = pi.unwrap();  // 3.14159
}
```

---

## C Interoperability

### C String Conversion

```aria
// Aria string to C string (null-terminated)
string:aria = "Hello";
const char*:c_str = aria.to_cstr();  // "Hello\0"

// Use with C functions
extern func printf(const char*:format, ...);
printf(aria.to_cstr());  // Pass to C

// C string to Aria string
extern func getenv(const char*:name): const char*;
const char*:c_result = getenv("PATH");
if (c_result != NULL) {
    string:path = string.from_cstr(c_result);
}
```

### Lifetime Considerations

```aria
// WARNING: C string pointer lifetime tied to Aria string
string:temp = "Hello";
const char*:ptr = temp.to_cstr();

// This is UNSAFE if temp is freed!
temp = "";  // temp may be garbage collected
// ptr is now dangling! Don't use it.

// SAFE: Keep Aria string alive
string:persistent = get_string();
const char*:safe_ptr = persistent.to_cstr();
use_c_function(safe_ptr);
// persistent still in scope, ptr is valid
```

### Byte Array Access

```aria
string:text = "Hello";

// Raw bytes (UTF-8)
const byte*:bytes = text.as_bytes();
i64:length = text.length();

for (i: i64 = 0; i < length; i++) {
    byte:b = bytes[i];
    // Process byte
}
```

---

## Common Patterns

### Path Manipulation

```aria
string:base = "/usr/local";
string:subdir = "bin";
string:full_path = base + "/" + subdir;  // "/usr/local/bin"

// Extract filename
string:path = "/home/user/document.txt";
i64:last_slash = path.last_index_of("/").unwrap();
string:filename = path[last_slash + 1:];  // "document.txt"

// Extract extension
i64:last_dot = filename.last_index_of(".").unwrap();
string:ext = filename[last_dot + 1:];  // "txt"
```

### String Validation

```aria
string:input = get_user_input();

// Check not empty
if (input.is_empty()) {
    log.error("Input cannot be empty\n");
    return;
}

// Check length
if (input.length() > 100) {
    log.error("Input too long\n");
    return;
}

// Check content
if (!input.contains("@") || !input.contains(".")) {
    log.error("Invalid email format\n");
    return;
}
```

### CSV Parsing

```aria
string:csv_line = "Alice,25,Engineer";
string:fields[] = csv_line.split(",");

if (fields.length() == 3) {
    string:name = fields[0];
    i32:age = fields[1].parse_i32().unwrap();
    string:job = fields[2];
}
```

### Log Message Building

```aria
// Inefficient: Multiple concatenations
string:msg = "[" + level + "] " + timestamp + ": " + message;

// Efficient: Interpolation (uses concat_n internally)
string:msg = "[${level}] ${timestamp}: ${message}";

// Or collect parts
string:parts[5] = ["[", level, "] ", timestamp, ": ", message];
string:msg = string.concat_n(parts, 5);
```

---

## Performance Considerations

### Avoid Repeated Concatenation

```aria
// BAD: O(n²) - creates new string each iteration
string:result = "";
for (i: i32 = 0; i < 1000; i++) {
    result = result + get_item(i);  // Copies all previous data!
}

// GOOD: O(n) - single allocation
string:parts[1000];
for (i: i32 = 0; i < 1000; i++) {
    parts[i] = get_item(i);
}
string:result = string.concat_n(parts, 1000);
```

### String Literals are Free

```aria
// These are identical in memory (.rodata section)
string:a = "Hello";
string:b = "Hello";
string:c = "Hello";
// All point to same static string literal
```

### Substring Views (Future)

```aria
// Current: Substring creates copy
string:text = "Hello, World!";
string:hello = text[0:5];  // Allocates new 5-byte string

// Future: Zero-copy substring view
StringView:hello = text.view(0, 5);  // Just offset + length
// Points into original string (no allocation)
```

---

## Memory and Safety

### Garbage Collection

```aria
// Strings are GC-allocated
string:temp = "Hello " + "World";
// Allocation happens here

// When temp goes out of scope, GC will collect it
// You don't manually free strings
```

### String Lifetime

```aria
string:global = "I live forever";  // Static literal

func process(): string {
    string:local = "Temporary";
    return local;  // Returns copy/reference
}

string:result = process();  // result owns the string
// local from process() is gone, but string data persists
```

### Dangling References (Impossible)

```aria
// This CANNOT happen in Aria (unlike C)
string*:ptr;
{
    string:local = "Temporary";
    ptr = &local;
}
// ptr does NOT dangle - GC keeps string alive as long as ptr exists
```

---

## Best Practices

### ✅ DO: Use Interpolation for Clarity

```aria
// Clear
string:msg = "User ${name} has ${count} items";

// Less clear
string:msg = "User " + name + " has " + count.to_string() + " items";
```

### ✅ DO: Use Raw Strings for Paths and Regex

```aria
// Windows paths
string:path = `C:\Users\Alice\Documents\file.txt`;

// Regex patterns
string:pattern = `\d{3}-\d{2}-\d{4}`;  // SSN pattern
```

### ✅ DO: Preallocate for Bulk Operations

```aria
// Collect parts, then join
string:parts[100];
i32:count = 0;

for (item in items) {
    parts[count++] = item.to_string();
}

string:result = string.concat_n(parts, count);
```

### ❌ DON'T: Repeatedly Concatenate in Loops

```aria
// VERY inefficient
string:result = "";
for (i: i32 = 0; i < 1000; i++) {
    result = result + i.to_string();  // O(n²)!
}
```

### ❌ DON'T: Assume 1 Char = 1 Byte

```aria
string:emoji = "😊";
// emoji.length() is 4 (bytes), not 1
// Use emoji.char_count() for character count
```

### ❌ DON'T: Slice UTF-8 Carelessly

```aria
string:text = "Café";
// string:bad = text[0:3];  // Might cut é in half!

// Use character-aware methods
string:safe = text.char_slice(0, 3);  // Safe character slicing
```

---

## Common Gotchas

### 1. length() is Bytes, Not Characters

```aria
string:text = "Hello 😊";
i64:len = text.length();  // 10 bytes, not 7 characters!

// If you need character count:
i64:chars = text.char_count();  // 7 characters
```

### 2. Indexing is Byte Offset

```aria
string:emoji = "Hello 😊";
char:c = emoji[6];  // First byte of 😊 (invalid!)

// Use character-aware indexing:
char:c = emoji.char_at(6);  // The 😊 emoji (valid)
```

### 3. String Comparison is Binary

```aria
string:a = "Café";
string:b = "Cafe\u0301";  // e + combining accent

// These are NOT equal (different byte sequences)
bool:same = (a == b);  // false

// For Unicode normalization (future):
// bool:same = a.normalize() == b.normalize();
```

### 4. Immutability

```aria
string:text = "Hello";
text.to_upper();  // This does NOTHING!
// to_upper() returns new string, doesn't modify

// Correct:
text = text.to_upper();  // Reassign variable
```

---

## See Also

- [char](char.md) - Single character type
- [byte](byte.md) / [uint8](uint8.md) - Binary data
- [String Interpolation](../operators/interpolation.md) - $ and ${}syntax
- [Backtick Operator](../operators/backtick.md) - Raw strings
- [Result<T>](result.md) - Error handling for parsing

---

**Remember**: Strings are **immutable**, **UTF-8 encoded**, and **garbage collected**!
