# char: Character Type

## Overview

**Purpose**: Single character values for text processing, UTF-8 encoding, and character manipulation.

**Characteristics**:
- Represents a single Unicode code point
- UTF-8 encoded (1-4 bytes)
- Distinct from byte/uint8 (character vs binary semantics)
- Part of string operations
- ASCII subset (0-127) guaranteed 1 byte

**When to Use**:
- Text character manipulation
- Single-character comparisons
- Character classification (is_digit, is_alpha)
- Building strings character-by-character
- Parsing text streams

**When NOT to Use**:
- Binary data (use byte/uint8)
- Multi-character text (use string)
- Numeric values (use int, even for digit characters)
- Bit manipulation (use uint8)

---

## char vs byte: Semantics Matter

### The Critical Distinction

**char**: Character semantics
- Purpose: Represent text characters
- Encoding: UTF-8 (1-4 bytes per character)
- Operations: Character classification, case conversion
- Range: Unicode code points (U+0000 to U+10FFFF)
- Intent: "What is this letter/symbol?"

**byte** (alias for uint8): Binary semantics
- Purpose: Raw data, binary values
- Encoding: None (raw 8-bit value)
- Operations: Bit manipulation, arithmetic
- Range: 0-255 (unsigned 8-bit integer)
- Intent: "What is this binary value?"

### Examples of the Difference

```aria
// char: Text character
char:letter = 'A';  // Code point U+0041
char:emoji = '😊';  // Code point U+1F60A (4 bytes in UTF-8)

if (letter.is_alpha()) {
    log.write("It's a letter\n");
}

// byte: Binary data
byte:value = 0x41;  // Just the number 65
byte:mask = 0xFF;

value &= mask;  // Bit manipulation
```

### ASCII Subset (0-127)

```aria
// ASCII characters: 1 byte in UTF-8, same as byte value
char:a = 'A';  // UTF-8: 0x41 (1 byte)
byte:b = 0x41; // Binary: 0x41 (1 byte)

// Same byte representation, different semantics
string:s1 = a.to_string();  // "A" (character)
string:s2 = b.to_hex();     // "41" (hex value)
```

---

## Character Literals

### Single Characters

```aria
char:letter = 'a';
char:digit = '5';
char:symbol = '@';
char:space = ' ';
char:newline = '\n';  // Escape sequence
```

### Escape Sequences

```aria
char:newline = '\n';      // Line feed (U+000A)
char:carriage_return = '\r';  // CR (U+000D)
char:tab = '\t';          // Horizontal tab (U+0009)
char:quote = '\'';        // Single quote (escape to use in char literal)
char:backslash = '\\';    // Backslash (escape)
char:null = '\0';         // Null character (U+0000)
```

### Unicode Literals

```aria
// Unicode escape: \u{XXXX}
char:euro = '\u{20AC}';    // € (U+20AC)
char:heart = '\u{2764}';   // ❤ (U+2764)
char:lambda = '\u{03BB}';  // λ (U+03BB)

// Emoji (requires 4 bytes in UTF-8)
char:smile = '\u{1F60A}';  // 😊 (U+1F60A)
char:rocket = '\u{1F680}'; // 🚀 (U+1F680)
```

---

## UTF-8 Encoding

### Variable-Length Encoding

**UTF-8 uses 1-4 bytes per character**:

| Range | Bytes | Example |
|-------|-------|---------|
| U+0000 - U+007F | 1 | ASCII ('A', '5', '\n') |
| U+0080 - U+07FF | 2 | Latin Extended ('é', 'ñ', 'Ω') |
| U+0800 - U+FFFF | 3 | Most symbols ('€', '中', '한') |
| U+10000 - U+10FFFF | 4 | Emoji ('😊', '🚀') |

```aria
// ASCII: 1 byte
char:a = 'A';
int32:size_a = a.utf8_byte_count();  // 1

// Latin Extended: 2 bytes
char:e = 'é';
int32:size_e = e.utf8_byte_count();  // 2

// CJK: 3 bytes
char:chinese = '中';
int32:size_cn = chinese.utf8_byte_count();  // 3

// Emoji: 4 bytes
char:emoji = '😊';
int32:size_em = emoji.utf8_byte_count();  // 4
```

### Code Point vs Byte Representation

```aria
// char stores code point, not bytes
char:lambda = 'λ';  // U+03BB

// Get code point value
int32:code_point = lambda.to_code_point();  // 955 (0x03BB)

// Get UTF-8 byte encoding
byte[]:bytes = lambda.to_utf8_bytes();  // [0xCE, 0xBB] (2 bytes)
```

---

## Character Classification

### Built-in Predicates

```aria
char:c = 'A';

bool:is_alpha = c.is_alpha();         // true (letter)
bool:is_digit = c.is_digit();         // false
bool:is_alnum = c.is_alphanumeric();  // true (letter or digit)
bool:is_space = c.is_whitespace();    // false
bool:is_upper = c.is_uppercase();     // true
bool:is_lower = c.is_lowercase();     // false
bool:is_ascii = c.is_ascii();         // true (code point < 128)
```

### Examples

```aria
// Classify different characters
char:a = 'a';
a.is_alpha();       // true
a.is_lowercase();   // true
a.is_ascii();       // true

char:five = '5';
five.is_digit();    // true
five.is_alpha();    // false

char:space = ' ';
space.is_whitespace();  // true

char:emoji = '😊';
emoji.is_ascii();   // false
emoji.is_alpha();   // false (emoji not classified as alpha)
```

### Whitespace Characters

```aria
char:space = ' ';
char:tab = '\t';
char:newline = '\n';
char:return = '\r';

space.is_whitespace();    // true
tab.is_whitespace();      // true
newline.is_whitespace();  // true
return.is_whitespace();   // true
```

---

## Case Conversion

### to_uppercase / to_lowercase

```aria
char:lower = 'a';
char:upper = lower.to_uppercase();  // 'A'

char:upper2 = 'Z';
char:lower2 = upper2.to_lowercase();  // 'z'

// Non-letters unchanged
char:digit = '5';
char:same = digit.to_uppercase();  // '5' (no change)
```

### ASCII vs Unicode

```aria
// ASCII case conversion (simple)
char:a = 'a';
char:A = a.to_uppercase();  // 'A'

// Unicode case conversion (complex)
char:german = 'ß';  // German sharp s
char:upper = german.to_uppercase();  // 'SS' (maps to two characters!)
// Note: Returns first char 'S', full conversion needs string

// Some characters have no uppercase/lowercase
char:emoji = '😊';
char:same = emoji.to_uppercase();  // '😊' (no change)
```

---

## Character Arithmetic

### Sequential Characters

```aria
// ASCII arithmetic (code point math)
char:a = 'a';
char:b = char.from_code_point(a.to_code_point() + 1);  // 'b'

char:capital_a = 'A';
char:capital_c = char.from_code_point(capital_a.to_code_point() + 2);  // 'C'

// Wrap as function for clarity
char:next_char(char:c) -> char {
    return char.from_code_point(c.to_code_point() + 1);
}

char:a = 'a';
char:b = next_char(a);  // 'b'
```

### Digit to Number

```aria
// Convert digit character to integer value
int32:digit_to_int(char:c) -> int32 {
    if (!c.is_digit()) return -1;  // Error
    return c.to_code_point() - '0'.to_code_point();
}

char:five = '5';
int32:value = digit_to_int(five);  // 5

// Reverse: number to digit
char:int_to_digit(int32:n) -> char {
    if (n < 0 || n > 9) return '?';  // Error
    return char.from_code_point('0'.to_code_point() + n);
}

int32:seven = 7;
char:digit = int_to_digit(seven);  // '7'
```

---

## Common Patterns

### Character Filtering

```aria
// Filter out non-alphanumeric characters
string:filter_alnum(string:input) -> string {
    char[]:result;
    loop (input.length:i) {
        char:c = input[i];
        if (c.is_alphanumeric()) {
            result.append(c);
        }
    }
    return string.from_chars(result);
}

string:messy = "Hello, World! 123";
string:clean = filter_alnum(messy);  // "HelloWorld123"
```

### Character Counting

```aria
// Count occurrences of character in string
int32:count_char(string:text, char:target) -> int32 {
    int32:count = 0;
    loop (text.length:i) {
        if (text[i] == target) {
            count += 1;
        }
    }
    return count;
}

string:sentence = "hello world";
int32:l_count = count_char(sentence, 'l');  // 3
```

### Character Replacement

```aria
// Replace all occurrences of character
string:replace_char(string:text, char:old, char:new) -> string {
    char[]:result;
    loop (text.length:i) {
        char:c = text[i];
        result.append(c == old ? new : c);
    }
    return string.from_chars(result);
}

string:original = "hello";
string:replaced = replace_char(original, 'l', 'r');  // "herro"
```

### Case Conversion (String Level)

```aria
// Convert entire string to uppercase
string:to_uppercase(string:text) -> string {
    char[]:result;
    loop (text.length:i) {
        result.append(text[i].to_uppercase());
    }
    return string.from_chars(result);
}

string:lower = "hello world";
string:upper = to_uppercase(lower);  // "HELLO WORLD"
```

---

## Character Comparison

### Equality

```aria
char:a = 'a';
char:b = 'a';
char:c = 'A';

bool:equal = a == b;      // true
bool:not_equal = a != c;  // true (different case)
```

### Relational (Lexicographic)

```aria
char:a = 'a';
char:b = 'b';
char:A = 'A';

bool:less = a < b;        // true (code point comparison)
bool:greater = b > a;     // true
bool:upper_less = A < a;  // true (uppercase before lowercase in Unicode)

// Digit comparison
char:five = '5';
char:nine = '9';
bool:digit_less = five < nine;  // true
```

---

## Reading and Writing

### Console I/O

```aria
// Read single character from input
char:c = io.read_char();

// Write character to output
char:letter = 'A';
io.write_char(letter);  // Outputs: A

// Write with newline
io.write_char('X');
io.write_char('\n');
```

### File I/O

```aria
// Read characters from file
file:f = file.open("input.txt", "r");
loop till f.eof() {
    char:c = f.read_char();
    process(c);
}
f.close();

// Write characters to file
file:out = file.open("output.txt", "w");
out.write_char('H');
out.write_char('i');
out.write_char('\n');
out.close();
```

---

## Anti-Patterns

### Using byte for Characters

```aria
// BAD: Binary semantics for text
byte:letter = 0x41;  // Unclear: Is this 'A' or just 65?
if (letter == 0x42) {  // Comparing to 'B'? Or 66?
    // ...
}

// GOOD: Character semantics for text
char:letter = 'A';
if (letter == 'B') {
    // ...
}
```

### Assuming ASCII

```aria
// BAD: Assumes ASCII (breaks for é, 中, 😊)
char:c = get_input();
byte:b = c.to_byte();  // COMPILE ERROR if c > 127!

// GOOD: Handle UTF-8 properly
char:c = get_input();
int32:code_point = c.to_code_point();  // Works for all Unicode
```

### Character Arithmetic Without Bounds

```aria
// BAD: Arithmetic without checking
char:c = 'z';
char:next = char.from_code_point(c.to_code_point() + 1);  // '{' (not a letter!)

// GOOD: Check bounds
char:next_letter(char:c) -> char {
    if (c == 'z') return 'a';  // Wrap
    if (c == 'Z') return 'A';  // Wrap
    return char.from_code_point(c.to_code_point() + 1);
}
```

### Treating char as int

```aria
// BAD: Numeric operations on characters (confusing)
char:a = 'A';
char:result = a + 5;  // COMPILE ERROR (good!)

// GOOD: Explicit conversion
int32:code = a.to_code_point();
int32:new_code = code + 5;
char:result = char.from_code_point(new_code);  // 'F'
```

---

## Memory and Performance

### Storage Size

**Variable Length** (UTF-8):
- ASCII: 1 byte
- Extended Latin: 2 bytes
- Most Unicode: 3 bytes
- Emoji: 4 bytes

```aria
// Character array size varies by content
char[3]:ascii = {'A', 'B', 'C'};      // 3 bytes
char[3]:mixed = {'A', 'é', '中'};     // 1 + 2 + 3 = 6 bytes
char[3]:emoji = {'😊', '🚀', '❤'};   // 4 + 4 + 3 = 11 bytes
```

### String Indexing

**Important**: String indexing by character position (not byte position)

```aria
string:text = "Aé中😊";  // 1+2+3+4 = 10 bytes, but 4 characters

char:c0 = text[0];  // 'A' (byte 0)
char:c1 = text[1];  // 'é' (bytes 1-2)
char:c2 = text[2];  // '中' (bytes 3-5)
char:c3 = text[3];  // '😊' (bytes 6-9)

int32:len = text.length;  // 4 (character count, not byte count)
```

---

## Common Character Sets

### ASCII Printable (32-126)

```aria
// Printable ASCII characters
char:space = ' ';      // 32 (first printable)
char:tilde = '~';      // 126 (last printable)

bool:is_printable_ascii(char:c) -> bool {
    int32:code = c.to_code_point();
    return (code >= 32) && (code <= 126);
}
```

### Digits (48-57)

```aria
char:zero = '0';   // 48
char:nine = '9';   // 57

bool:is_digit(char:c) -> bool {
    return c.is_digit();  // Built-in
}
```

### Uppercase Letters (65-90)

```aria
char:A = 'A';  // 65
char:Z = 'Z';  // 90

bool:is_uppercase_letter(char:c) -> bool {
    return c.is_uppercase() && c.is_alpha();
}
```

### Lowercase Letters (97-122)

```aria
char:a = 'a';  // 97
char:z = 'z';  // 122

bool:is_lowercase_letter(char:c) -> bool {
    return c.is_lowercase() && c.is_alpha();
}
```

---

## Character Validation

### Alphanumeric Check

```aria
bool:is_valid_identifier_char(char:c, bool:first) -> bool {
    if (first) {
        // First character: letter or underscore
        return c.is_alpha() || c == '_';
    } else {
        // Subsequent: letter, digit, or underscore
        return c.is_alphanumeric() || c == '_';
    }
}

string:identifier = "hello_world123";
bool:valid = is_valid_identifier_char(identifier[0], true);
```

### Printable Check

```aria
bool:is_printable(char:c) -> bool {
    // Printable: not control character
    int32:code = c.to_code_point();
    
    // ASCII control characters (0-31, 127)
    if (code < 32) return false;
    if (code == 127) return false;
    
    // Printable ASCII or extended Unicode (generally printable)
    return true;
}
```

---

## Best Practices

### Naming Conventions

```aria
// Single character variables: Use descriptive names
char:delimiter = ',';
char:quote_char = '"';
char:newline_char = '\n';

// Not just 'c' unless in tight loop
loop (text.length:i) {
    char:c = text[i];  // OK in loop context
    process(c);
}
```

### Prefer Built-in Classification

```aria
// GOOD: Use built-in methods
if (c.is_digit()) {
    // ...
}

// BAD: Manual range check (less clear)
int32:code = c.to_code_point();
if (code >= 48 && code <= 57) {
    // ...
}
```

### Explicit UTF-8 Awareness

```aria
// Remember: char can be 1-4 bytes
char:emoji = '😊';

// Don't assume 1 byte
byte[]:bytes = emoji.to_utf8_bytes();  // 4 bytes
// bytes.length == 4, not 1
```

---

## Summary

**char is for**:
- ✅ Text characters (letters, digits, symbols)
- ✅ UTF-8 encoded Unicode code points
- ✅ Character classification and conversion
- ✅ Building/parsing strings
- ✅ Text processing

**char is NOT for**:
- ❌ Binary data (use byte/uint8)
- ❌ Bit manipulation (use uint8)
- ❌ Numeric values (use int)
- ❌ Multi-character text (use string)

**Key Principles**:
1. **Character semantics** (not binary)
2. **UTF-8 encoded** (1-4 bytes variable length)
3. **Unicode code points** (U+0000 to U+10FFFF)
4. **vs byte**: Text vs binary semantics
5. **Classification built-in** (is_alpha, is_digit, etc.)
6. **Case conversion** (to_uppercase, to_lowercase)

**Best Practices**:
- Use char for text, byte for binary
- Leverage built-in classification methods
- Be aware of UTF-8 variable length
- Explicit code point conversions for arithmetic
- Use string for multi-character text
- Descriptive variable names (not just 'c')

**Remember**: char represents **text characters**, not binary data. Choose char when working with human-readable text, byte/uint8 when working with raw binary data.
