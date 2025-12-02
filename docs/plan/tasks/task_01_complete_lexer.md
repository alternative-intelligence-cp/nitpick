# Task 1: Complete Lexer

**Phase:** 1 - Foundation  
**Duration:** 1 week  
**Status:** 🟢 Ready to Start  
**Dependencies:** None  
**Priority:** HIGH (blocking everything)

---

## Objective

Extend the existing lexer to handle **ALL** token types from Aria v0.0.6 specification, including numeric literals in all bases, floating-point numbers, character literals, and preprocessor tokens.

---

## Current State Analysis

### What Works (Don't Break!)
✅ **File:** `src/frontend/lexer.cpp` (1384 lines)  
✅ **Tokens:** `src/frontend/tokens.h` (243 types defined)

**Working Features:**
- Decimal integer literals: `42`, `1000`, `255`
- String literals: `"hello"`, `"with\nescapes"`
- Template strings: `` `Hello &{name}!` ``
- Keywords: All 37 keywords recognized
- Operators: All operators with maximal munch
- Comments: `//` and `/* */`
- Identifiers: Variable/function names

### What's Missing

**GOOD NEWS:** After reviewing the lexer code, almost everything is implemented!

✅ **Already Working:**
- Hex literals: `0xFF`, `0xDEADBEEF` 
- Binary literals: `0b1010`, `0b11111111`
- Octal literals: `0o755`, `0o644`
- Float literals: `3.14`, `1e10`, `1.5e-3`
- Char literals: `'a'`, `'\n'`, `'\x41'`
- All operators and delimiters
- Template strings with interpolation

❌ **Actually Missing:**

#### 1. Hex Escape Sequences in Character Literals

**Current State:** Basic escapes work (`\n`, `\t`, etc.) but `\x` hex escapes are not implemented.

**Example:**
```aria
'\x41'        // Should be 'A' (65 in decimal)
'\x0A'        // Should be newline
```

**Implementation Needed:** Add hex escape parsing in char literal handler (line ~447 in lexer.cpp)

#### 2. Unicode Escape Sequences

**Not in current lexer:**
```aria
'\u{1F600}'   // Unicode escape (😀)
'\u{0041}'    // 'A'
```

**Note:** This is **low priority** - can defer to Phase 2 if needed. Basic ASCII works fine.

#### 3. Hex Floating-Point Literals

**Not in current lexer:**
```aria
0x1.2p3       // IEEE 754 hex float: 1.125 * 2^3 = 9.0
0x1.0p-3      // 0.125
```

**Note:** This is **low priority** - decimal floats and scientific notation work. Hex floats are rare.

#### 4. Preprocessor Directive Tokens (PRIMARY TASK)
```aria
%macro
%endmacro
%push
%pop
%context
%define
%undef
%ifdef
%ifndef
%if
%elif
%else
%endif
%include
%rep
%endrep
```

**Lexer Rules:**
- Start with `%`
- Followed by directive name  
- Return appropriate `TOKEN_KW_*` or create new token types

**CRITICAL:** First need to **add preprocessor token types** to `tokens.h`, then implement lexer support.

**Directives from Spec Section 5.2:**
```
%macro NAME PARAMS    // Define macro
%endmacro            // End macro definition
%push CONTEXT        // Push context onto stack
%pop                 // Pop context from stack
%context NAME        // Set current context
%define NAME VALUE   // Define constant
%undef NAME          // Undefine constant
%ifdef NAME          // If defined
%ifndef NAME         // If not defined
%if EXPR             // If expression
%elif EXPR           // Else if
%else                // Else
%endif               // End if
%include "file"      // Include file
%rep COUNT           // Repeat block
%endrep              // End repeat
%1, %2, ...          // Macro parameters
%$label              // Context-local label
```

**Token Types Needed (add to tokens.h):**
```cpp
// Preprocessor Directives
TOKEN_PREPROC_MACRO,        // %macro
TOKEN_PREPROC_ENDMACRO,     // %endmacro
TOKEN_PREPROC_PUSH,         // %push
TOKEN_PREPROC_POP,          // %pop
TOKEN_PREPROC_CONTEXT,      // %context
TOKEN_PREPROC_DEFINE,       // %define
TOKEN_PREPROC_UNDEF,        // %undef
TOKEN_PREPROC_IFDEF,        // %ifdef
TOKEN_PREPROC_IFNDEF,       // %ifndef
TOKEN_PREPROC_IF,           // %if
TOKEN_PREPROC_ELIF,         // %elif
TOKEN_PREPROC_ELSE,         // %else
TOKEN_PREPROC_ENDIF,        // %endif
TOKEN_PREPROC_INCLUDE,      // %include
TOKEN_PREPROC_REP,          // %rep
TOKEN_PREPROC_ENDREP,       // %endrep
TOKEN_PREPROC_PARAM,        // %1, %2, ... (macro parameter reference)
TOKEN_PREPROC_LOCAL,        // %$label (context-local symbol)
```

---

## Implementation Plan (REVISED)

### Step 1: Add Preprocessor Tokens to tokens.h (30 minutes)

**File:** `src/frontend/tokens.h`

**Add after line ~243 (before closing enum):**
```cpp
// Preprocessor Directives (NASM-style)
TOKEN_PREPROC_MACRO,        // %macro
TOKEN_PREPROC_ENDMACRO,     // %endmacro
TOKEN_PREPROC_PUSH,         // %push
TOKEN_PREPROC_POP,          // %pop
TOKEN_PREPROC_CONTEXT,      // %context
TOKEN_PREPROC_DEFINE,       // %define
TOKEN_PREPROC_UNDEF,        // %undef
TOKEN_PREPROC_IFDEF,        // %ifdef
TOKEN_PREPROC_IFNDEF,       // %ifndef
TOKEN_PREPROC_IF,           // %if
TOKEN_PREPROC_ELIF,         // %elif
TOKEN_PREPROC_ELSE,         // %else
TOKEN_PREPROC_ENDIF,        // %endif
TOKEN_PREPROC_INCLUDE,      // %include
TOKEN_PREPROC_REP,          // %rep
TOKEN_PREPROC_ENDREP,       // %endrep
TOKEN_PREPROC_PARAM,        // %1, %2, ... (macro parameter reference)
TOKEN_PREPROC_LOCAL,        // %$label (context-local symbol)
```

### Step 2: Implement Preprocessor Token Lexing (2 hours)

**File:** `src/frontend/lexer.cpp`

**Add after operators section (around line 600-700):**
```cpp
// Preprocessor directives (NASM-style)
if (c == '%') {
    size_t start_line = line, start_col = col;
    advance(); // Skip %
    
    // Check for %$label (context-local)
    if (peek() == '$') {
        advance(); // Skip $
        std::string label;
        while (isalnum(peek()) || peek() == '_') {
            label += peek();
            advance();
        }
        return {TOKEN_PREPROC_LOCAL, "%$" + label, start_line, start_col};
    }
    
    // Check for %1, %2, ... (macro parameters)
    if (isdigit(peek())) {
        std::string param;
        while (isdigit(peek())) {
            param += peek();
            advance();
        }
        return {TOKEN_PREPROC_PARAM, "%" + param, start_line, start_col};
    }
    
    // Directive name
    std::string directive;
    while (isalnum(peek()) || peek() == '_') {
        directive += peek();
        advance();
    }
    
    // Map directive to token type
    if (directive == "macro") return {TOKEN_PREPROC_MACRO, "%macro", start_line, start_col};
    if (directive == "endmacro") return {TOKEN_PREPROC_ENDMACRO, "%endmacro", start_line, start_col};
    if (directive == "push") return {TOKEN_PREPROC_PUSH, "%push", start_line, start_col};
    if (directive == "pop") return {TOKEN_PREPROC_POP, "%pop", start_line, start_col};
    if (directive == "context") return {TOKEN_PREPROC_CONTEXT, "%context", start_line, start_col};
    if (directive == "define") return {TOKEN_PREPROC_DEFINE, "%define", start_line, start_col};
    if (directive == "undef") return {TOKEN_PREPROC_UNDEF, "%undef", start_line, start_col};
    if (directive == "ifdef") return {TOKEN_PREPROC_IFDEF, "%ifdef", start_line, start_col};
    if (directive == "ifndef") return {TOKEN_PREPROC_IFNDEF, "%ifndef", start_line, start_col};
    if (directive == "if") return {TOKEN_PREPROC_IF, "%if", start_line, start_col};
    if (directive == "elif") return {TOKEN_PREPROC_ELIF, "%elif", start_line, start_col};
    if (directive == "else") return {TOKEN_PREPROC_ELSE, "%else", start_line, start_col};
    if (directive == "endif") return {TOKEN_PREPROC_ENDIF, "%endif", start_line, start_col};
    if (directive == "include") return {TOKEN_PREPROC_INCLUDE, "%include", start_line, start_col};
    if (directive == "rep") return {TOKEN_PREPROC_REP, "%rep", start_line, start_col};
    if (directive == "endrep") return {TOKEN_PREPROC_ENDREP, "%endrep", start_line, start_col};
    
    // Unknown directive
    return {TOKEN_INVALID, "UNKNOWN_DIRECTIVE_%" + directive, start_line, start_col};
}
```

### Step 3: Add Hex Escape Sequences (1 hour)

**File:** `src/frontend/lexer.cpp` (around line 447)

**Replace char literal escape handling:**
```cpp
if (peek() == '\\') {
    // Escape sequence
    advance();
    char next = peek();
    if (next == 'n') ch += '\n';
    else if (next == 't') ch += '\t';
    else if (next == 'r') ch += '\r';
    else if (next == '\\') ch += '\\';
    else if (next == '\'') ch += '\'';
    else if (next == '0') ch += '\0';
    else if (next == 'x') {
        // Hex escape: \xHH
        advance(); // Skip 'x'
        std::string hex;
        if (isxdigit(peek())) {
            hex += peek();
            advance();
        }
        if (isxdigit(peek())) {
            hex += peek();
            advance();
        }
        if (hex.empty()) {
            return {TOKEN_INVALID, "INVALID_HEX_ESCAPE", start_line, start_col};
        }
        char hex_char = static_cast<char>(std::stoi(hex, nullptr, 16));
        ch += hex_char;
        continue; // Don't advance again
    }
    else ch += next;
    advance();
}
```

### Step 4: Testing (1 day)

**Create test file:** `tests/test_lexer_complete.cpp`

```cpp
#include <gtest/gtest.h>
#include "frontend/lexer.h"

using namespace aria::frontend;

TEST(LexerTest, PreprocessorDirectives) {
    AriaLexer lexer("%macro TEST 2\n%endmacro");
    auto tokens = lexer.tokenize();
    
    EXPECT_EQ(tokens[0].type, TOKEN_PREPROC_MACRO);
    EXPECT_EQ(tokens[1].type, TOKEN_IDENTIFIER);  // TEST
    EXPECT_EQ(tokens[2].type, TOKEN_INT_LITERAL); // 2
    EXPECT_EQ(tokens[3].type, TOKEN_PREPROC_ENDMACRO);
}

TEST(LexerTest, MacroParameters) {
    AriaLexer lexer("%1 %2 %10");
    auto tokens = lexer.tokenize();
    
    EXPECT_EQ(tokens[0].type, TOKEN_PREPROC_PARAM);
    EXPECT_EQ(tokens[0].value, "%1");
    EXPECT_EQ(tokens[1].type, TOKEN_PREPROC_PARAM);
    EXPECT_EQ(tokens[1].value, "%2");
    EXPECT_EQ(tokens[2].type, TOKEN_PREPROC_PARAM);
    EXPECT_EQ(tokens[2].value, "%10");
}

TEST(LexerTest, ContextLocalLabels) {
    AriaLexer lexer("%$start %$end");
    auto tokens = lexer.tokenize();
    
    EXPECT_EQ(tokens[0].type, TOKEN_PREPROC_LOCAL);
    EXPECT_EQ(tokens[0].value, "%$start");
    EXPECT_EQ(tokens[1].type, TOKEN_PREPROC_LOCAL);
    EXPECT_EQ(tokens[1].value, "%$end");
}

TEST(LexerTest, HexEscapeInChar) {
    AriaLexer lexer("'\\x41' '\\x0A'");
    auto tokens = lexer.tokenize();
    
    EXPECT_EQ(tokens[0].type, TOKEN_CHAR_LITERAL);
    EXPECT_EQ(tokens[0].value[0], 'A');  // 0x41 = 65 = 'A'
    EXPECT_EQ(tokens[1].type, TOKEN_CHAR_LITERAL);
    EXPECT_EQ(tokens[1].value[0], '\n'); // 0x0A = 10 = newline
}

TEST(LexerTest, AllNumericFormatsStillWork) {
    AriaLexer lexer("255 0xFF 0b11111111 0o377 3.14 1e10");
    auto tokens = lexer.tokenize();
    
    EXPECT_EQ(tokens[0].type, TOKEN_INT_LITERAL);
    EXPECT_EQ(tokens[1].type, TOKEN_INT_LITERAL);
    EXPECT_EQ(tokens[2].type, TOKEN_INT_LITERAL);
    EXPECT_EQ(tokens[3].type, TOKEN_INT_LITERAL);
    EXPECT_EQ(tokens[4].type, TOKEN_FLOAT_LITERAL);
    EXPECT_EQ(tokens[5].type, TOKEN_FLOAT_LITERAL);
}
```

### Step 5: Integration Testing (0.5 day)

**Test spec section 5.2 examples:**

```bash
cd /home/randy/._____RANDY_____/REPOS/aria
mkdir -p tests/lexer_integration

# Create test file from spec
cat > tests/lexer_integration/macro_test.aria << 'EOF'
%macro DEBUG_PRINT 1
    print("Debug: %1")
%endmacro

DEBUG_PRINT("Hello World")
EOF

# Test lexing (no errors expected)
./build/ariac --lex-only tests/lexer_integration/macro_test.aria
```

---

## Revised Testing Strategy

**File:** `src/frontend/lexer.cpp`

**Current code location (around line 500-600):**
```cpp
// Existing integer literal parsing
if (isdigit(current_char)) {
    return lexNumber();
}
```

**Add after decimal check:**
```cpp
std::string Lexer::lexNumber() {
    std::string num;
    
    // Check for hex, binary, octal
    if (current_char == '0' && position + 1 < source.length()) {
        char next = source[position + 1];
        
        // Hexadecimal
        if (next == 'x' || next == 'X') {
            num += current_char;  // '0'
            advance();
            num += current_char;  // 'x'
            advance();
            
            while (isxdigit(current_char)) {
                num += current_char;
                advance();
            }
            
            // Convert hex string to integer
            long long value = std::stoll(num.substr(2), nullptr, 16);
            return Token(TokenType::INT_LITERAL, std::to_string(value));
        }
        
        // Binary
        if (next == 'b' || next == 'B') {
            // Similar implementation
        }
        
        // Octal
        if (next == 'o' || next == 'O') {
            // Similar implementation
        }
    }
    
    // Existing decimal logic...
}
```

**Test cases to add:**
```cpp
// tests/test_lexer.cpp
TEST(LexerTest, HexLiterals) {
    Lexer lexer("0xFF 0xDEADBEEF 0x1A2B");
    auto tokens = lexer.tokenize();
    
    EXPECT_EQ(tokens[0].type, TokenType::INT_LITERAL);
    EXPECT_EQ(tokens[0].value, "255");
    
    EXPECT_EQ(tokens[1].type, TokenType::INT_LITERAL);
    EXPECT_EQ(tokens[1].value, "3735928559");
    
    EXPECT_EQ(tokens[2].type, TokenType::INT_LITERAL);
    EXPECT_EQ(tokens[2].value, "6699");
}
```

### Step 2: Add Binary Literal Support (Day 1)

Similar to hex, but using `std::stoll(num.substr(2), nullptr, 2)`.

### Step 3: Add Octal Literal Support (Day 1)

Similar to hex, but using `std::stoll(num.substr(2), nullptr, 8)`.

### Step 4: Add Float Literal Support (Day 2-3)

**Challenges:**
- Distinguish between integer and float
- Handle scientific notation
- Handle hex floats (IEEE 754 format)

**Implementation:**
```cpp
std::string Lexer::lexFloat() {
    std::string num;
    bool has_decimal = false;
    bool has_exponent = false;
    
    // Integer part
    while (isdigit(current_char)) {
        num += current_char;
        advance();
    }
    
    // Decimal point
    if (current_char == '.') {
        has_decimal = true;
        num += current_char;
        advance();
        
        while (isdigit(current_char)) {
            num += current_char;
            advance();
        }
    }
    
    // Exponent
    if (current_char == 'e' || current_char == 'E') {
        has_exponent = true;
        num += current_char;
        advance();
        
        if (current_char == '+' || current_char == '-') {
            num += current_char;
            advance();
        }
        
        while (isdigit(current_char)) {
            num += current_char;
            advance();
        }
    }
    
    return Token(TokenType::FLOAT_LITERAL, num);
}
```

**Hex float support:**
```cpp
// For hex floats like 0x1.2p3
// Use std::hexfloat or manual IEEE 754 conversion
```

### Step 5: Add Character Literal Support (Day 4)

**Escape sequence handling:**
```cpp
char Lexer::parseEscapeSequence() {
    advance(); // Skip backslash
    
    switch (current_char) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        case 'x': {
            // Hex escape: \xHH
            advance();
            std::string hex;
            hex += current_char;
            advance();
            hex += current_char;
            return static_cast<char>(std::stoi(hex, nullptr, 16));
        }
        case 'u': {
            // Unicode escape: \u{HHHHHH}
            // Requires UTF-8 encoding
            // More complex implementation
        }
        default:
            error("Unknown escape sequence");
    }
}

Token Lexer::lexCharLiteral() {
    advance(); // Skip opening '
    
    char value;
    if (current_char == '\\') {
        value = parseEscapeSequence();
    } else {
        value = current_char;
    }
    
    advance(); // Move to closing '
    expect('\'');
    
    return Token(TokenType::CHAR_LITERAL, std::string(1, value));
}
```

### Step 6: Add Preprocessor Token Support (Day 5)

**Already have token types defined in tokens.h:**
```cpp
// From earlier work
MACRO, ENDMACRO, PUSH, POP, CONTEXT,
DEFINE, UNDEF, IFDEF, IFNDEF, IF_PREPROC,
ELIF, ELSE_PREPROC, ENDIF, INCLUDE,
REP, ENDREP, MACRO_PARAM, CONTEXT_LABEL
```

**Just need to recognize them:**
```cpp
Token Lexer::lexPreprocessor() {
    advance(); // Skip %
    
    std::string directive;
    while (isalnum(current_char) || current_char == '_') {
        directive += current_char;
        advance();
    }
    
    // Map directive name to token type
    if (directive == "macro") return Token(TokenType::MACRO, "%macro");
    if (directive == "endmacro") return Token(TokenType::ENDMACRO, "%endmacro");
    if (directive == "push") return Token(TokenType::PUSH, "%push");
    // ... etc for all directives
    
    error("Unknown preprocessor directive: %" + directive);
}
```

### Step 7: Integration & Testing (Day 6-7)

**Test spec examples:**
```bash
# Create test files from spec
cd /home/randy/._____RANDY_____/REPOS/aria/tests/lexer_tests/

# Test all literal formats
echo "0xFF 0b1010 0o755 3.14 1e10 'a' '\n'" > test_literals.aria
./ariac --lex-only test_literals.aria

# Verify output
```

**Regression testing:**
```bash
# Run ALL existing tests
cd /home/randy/._____RANDY_____/REPOS/aria/build
make test

# Should have zero failures
```

---

## Testing Strategy

### Unit Tests (tests/test_lexer.cpp)

**Test Cases to Add:**

1. **Hex Literals**
   - Basic: `0xFF` → 255
   - Large: `0xDEADBEEF` → 3735928559
   - Mixed case: `0XaBcD` → 43981
   - Edge: `0x0` → 0

2. **Binary Literals**
   - Basic: `0b1010` → 10
   - Byte: `0b11111111` → 255
   - Single: `0b1` → 1
   - Edge: `0b0` → 0

3. **Octal Literals**
   - Permissions: `0o755` → 493
   - Basic: `0o10` → 8
   - Edge: `0o0` → 0

4. **Float Literals**
   - Decimal: `3.14` → 3.14
   - Scientific: `1e10` → 10000000000.0
   - Negative exp: `1.5e-3` → 0.0015
   - Hex float: `0x1.2p3` → 9.0
   - Leading dot: `.5` → 0.5
   - Trailing dot: `5.` → 5.0

5. **Character Literals**
   - Basic: `'a'` → 'a'
   - Escape: `'\n'` → newline
   - Hex: `'\x41'` → 'A'
   - Quote: `'\''` → single quote

6. **Preprocessor Tokens**
   - All 18 directive types
   - Case sensitivity
   - Unknown directives (should error)

### Integration Tests

**Test spec examples:**
```aria
// From section 2.1.1 - All numeric types
int8:x = 127
int16:y = 0xFF
int32:z = 0b11111111
uint64:big = 0xDEADBEEF
flt32:pi = 3.14159
flt64:e = 2.71828e0
```

**Expected:** All tokenize correctly without errors.

---

## Validation Checklist

Before marking this task complete:

- [ ] All hex literals parse correctly
- [ ] All binary literals parse correctly
- [ ] All octal literals parse correctly
- [ ] All float formats parse correctly
- [ ] All character escapes work
- [ ] All preprocessor directives recognized
- [ ] No regressions in existing tests
- [ ] Spec section 2.1 examples tokenize
- [ ] Spec section 8.1 examples tokenize
- [ ] Documentation updated (lexer.h comments)
- [ ] Code reviewed for edge cases
- [ ] Memory leaks checked (valgrind)

---

## Success Criteria

✅ **Task 1 is complete when:**

1. **All literal types work:**
   - Hex: `0xFF` → 255
   - Binary: `0b1010` → 10
   - Octal: `0o755` → 493
   - Float: `3.14`, `1e10`, `0x1.2p3`
   - Char: `'a'`, `'\n'`, `'\x41'`

2. **All preprocessor tokens recognized:**
   - 18 directive types from tokens.h

3. **Zero regressions:**
   - All existing tests still pass
   - No broken features

4. **100% spec coverage:**
   - Every code example in spec tokenizes
   - No syntax errors from valid code

5. **Clean implementation:**
   - No memory leaks
   - No compiler warnings
   - Well-documented code

---

## Files to Modify

**Primary:**
- `src/frontend/lexer.cpp` - Add literal parsing functions
- `src/frontend/lexer.h` - Update function declarations

**Testing:**
- `tests/test_lexer.cpp` - Add comprehensive test suite
- `tests/lexer_tests/*.aria` - Create test files

**Documentation:**
- Update comments in lexer.h
- Add examples to lexer.cpp

---

## Estimated Time Breakdown (REVISED - Realistic)

| Task | Time | Notes |
|------|------|-------|
| Add preprocessor tokens to tokens.h | 0.5 hours | Simple enum additions |
| Implement preprocessor lexing | 2 hours | Straightforward string matching |
| Add hex escape sequences | 1 hour | Modify existing char literal code |
| Unit tests | 4 hours | Test all preprocessor tokens + hex escapes |
| Integration tests | 2 hours | Test spec examples |
| Documentation | 1 hour | Update comments |
| **Total** | **10.5 hours** | **~2 days** |

**Buffer:** +1 day for unexpected issues = **3 days total**

**MUCH FASTER than original estimate because:**
- ✅ Hex/binary/octal literals already implemented
- ✅ Float literals already implemented  
- ✅ Char literals already implemented
- ✅ Only need preprocessor tokens + hex escapes

---

## Next Steps After Completion

Once Task 1 is ✅ complete:
1. Update PROGRESS.md
2. Commit with message: "feat: complete lexer with all literal types"
3. Tag: `v0.0.6-lexer-complete`
4. Begin Task 2: Build Preprocessor

---

**Let's do this! 🚀**
