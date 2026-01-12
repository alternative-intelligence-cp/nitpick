# Zero Implicit Conversion - Implementation Plan

## Overview
Implementation plan for eliminating ALL implicit type conversions in Aria by requiring explicit type suffixes on all numeric literals.

**Design Decision:** All numeric literals MUST have explicit type suffixes (e.g., `1u32`, `0i64`, `3.14f32`).

**Philosophy:** "Autism weaponized as language design" - zero tolerance for implicit behavior. Literal interpretation prevents hidden assumptions and social engineering attacks.

**Safety Rationale:** Nikola will interact with children - no affordances for cutting corners.

---

## Phase 1: Lexer Changes (Token Generation)

### 1.1: Expand TokenType Enum

Add typed literal tokens to `include/frontend/token.h`:

```cpp
// After existing TOKEN_INTEGER and TOKEN_FLOAT:

// Integer literals with explicit type suffixes
TOKEN_INTEGER_U8,       // 255u8
TOKEN_INTEGER_U16,      // 65535u16
TOKEN_INTEGER_U32,      // 4294967295u32
TOKEN_INTEGER_U64,      // 18446744073709551615u64
TOKEN_INTEGER_U128,     // ...u128
TOKEN_INTEGER_U256,     // ...u256
TOKEN_INTEGER_U512,     // ...u512
TOKEN_INTEGER_U1024,    // ...u1024

TOKEN_INTEGER_I8,       // -128i8
TOKEN_INTEGER_I16,      // -32768i16
TOKEN_INTEGER_I32,      // -2147483648i32
TOKEN_INTEGER_I64,      // -9223372036854775808i64
TOKEN_INTEGER_I128,     // ...i128
TOKEN_INTEGER_I256,     // ...i256
TOKEN_INTEGER_I512,     // ...i512
TOKEN_INTEGER_I1024,    // ...i1024

// Float literals with explicit type suffixes
TOKEN_FLOAT_F32,        // 3.14f32
TOKEN_FLOAT_F64,        // 3.14159265358979f64
TOKEN_FLOAT_F128,       // ...f128
TOKEN_FLOAT_F256,       // ...f256
TOKEN_FLOAT_F512,       // ...f512

// TBB (Twisted Balanced Binary) literals
TOKEN_TBB8,             // 127tbb8
TOKEN_TBB16,            // 32767tbb16
TOKEN_TBB32,            // ...tbb32
TOKEN_TBB64,            // ...tbb64

// Fixed-point literals
TOKEN_FIX256,           // 1.5fix256 (Q128.128 deterministic physics)

// Future: Fraction, TFP, etc. (Phase 2)
```

### 1.2: Add Suffix Parsing to scanNumber()

Location: `src/frontend/lexer/lexer.cpp`, end of `scanNumber()` function (after line 840).

**Current code:**
```cpp
if (isFloat) {
    double float_value = std::stod(cleaned_text);
    addToken(TokenType::TOKEN_FLOAT, float_value, cleaned_text);
} else {
    try {
        int64_t value = std::stoll(cleaned_text);
        addToken(TokenType::TOKEN_INTEGER, value);
    } catch (std::out_of_range&) {
        addToken(TokenType::TOKEN_INTEGER, (int64_t)0, cleaned_text);
    }
}
```

**New implementation:**
```cpp
// After number parsing, check for REQUIRED type suffix
std::string suffix = scanTypeSuffix();  // NEW helper function

if (suffix.empty()) {
    // ERROR: Zero-conversion policy requires explicit type suffix
    error("Numeric literal missing required type suffix (e.g., 1u32, 0i64, 3.14f32). "
          "Aria requires explicit types for all numeric literals. "
          "See docs/programming_guide/types/zero_implicit_conversion.md");
    // Add error token and return
    addToken(TokenType::TOKEN_ERROR);
    return;
}

// Parse suffix and create appropriate typed token
TokenType tokenType = suffixToTokenType(suffix, isFloat);  // NEW helper
if (tokenType == TokenType::TOKEN_ERROR) {
    error("Invalid type suffix '" + suffix + "' for numeric literal. "
          "Valid suffixes: u8, u16, u32, u64, i8, i16, i32, i64, f32, f64, tbb8, tbb16, fix256, etc.");
    addToken(TokenType::TOKEN_ERROR);
    return;
}

// Create typed token with appropriate value
if (isFloat) {
    double float_value = std::stod(cleaned_text);
    addToken(tokenType, float_value, cleaned_text);
} else {
    try {
        int64_t value = std::stoll(cleaned_text);
        addToken(tokenType, value);
    } catch (std::out_of_range&) {
        // High-precision value
        addToken(tokenType, (int64_t)0, cleaned_text);
    }
}
```

### 1.3: Add Helper Functions

Add to `lexer.cpp`:

```cpp
// Scan type suffix after numeric literal
// Returns empty string if no suffix found
std::string Lexer::scanTypeSuffix() {
    size_t suffix_start = current;
    
    // Type suffixes start with letter (u, i, f, t) or 'fix'
    if (!isAlpha(peek())) {
        return "";  // No suffix
    }
    
    // Scan letters and digits for suffix (e.g., "u32", "i64", "tbb16", "fix256")
    while (isAlphaNumeric(peek())) {
        advance();
    }
    
    return source.substr(suffix_start, current - suffix_start);
}

// Convert type suffix to TokenType
// Returns TOKEN_ERROR if invalid suffix for given context (int vs float)
TokenType Lexer::suffixToTokenType(const std::string& suffix, bool isFloat) {
    // Integer suffixes
    if (!isFloat) {
        if (suffix == "u8") return TokenType::TOKEN_INTEGER_U8;
        if (suffix == "u16") return TokenType::TOKEN_INTEGER_U16;
        if (suffix == "u32") return TokenType::TOKEN_INTEGER_U32;
        if (suffix == "u64") return TokenType::TOKEN_INTEGER_U64;
        if (suffix == "u128") return TokenType::TOKEN_INTEGER_U128;
        if (suffix == "u256") return TokenType::TOKEN_INTEGER_U256;
        if (suffix == "u512") return TokenType::TOKEN_INTEGER_U512;
        if (suffix == "u1024") return TokenType::TOKEN_INTEGER_U1024;
        
        if (suffix == "i8") return TokenType::TOKEN_INTEGER_I8;
        if (suffix == "i16") return TokenType::TOKEN_INTEGER_I16;
        if (suffix == "i32") return TokenType::TOKEN_INTEGER_I32;
        if (suffix == "i64") return TokenType::TOKEN_INTEGER_I64;
        if (suffix == "i128") return TokenType::TOKEN_INTEGER_I128;
        if (suffix == "i256") return TokenType::TOKEN_INTEGER_I256;
        if (suffix == "i512") return TokenType::TOKEN_INTEGER_I512;
        if (suffix == "i1024") return TokenType::TOKEN_INTEGER_I1024;
        
        if (suffix == "tbb8") return TokenType::TOKEN_TBB8;
        if (suffix == "tbb16") return TokenType::TOKEN_TBB16;
        if (suffix == "tbb32") return TokenType::TOKEN_TBB32;
        if (suffix == "tbb64") return TokenType::TOKEN_TBB64;
        
        // Float suffix on integer literal is ERROR
        if (suffix.substr(0, 1) == "f") return TokenType::TOKEN_ERROR;
    } else {
        // Float suffixes
        if (suffix == "f32") return TokenType::TOKEN_FLOAT_F32;
        if (suffix == "f64") return TokenType::TOKEN_FLOAT_F64;
        if (suffix == "f128") return TokenType::TOKEN_FLOAT_F128;
        if (suffix == "f256") return TokenType::TOKEN_FLOAT_F256;
        if (suffix == "f512") return TokenType::TOKEN_FLOAT_F512;
        
        if (suffix == "fix256") return TokenType::TOKEN_FIX256;
        
        // Integer suffix on float literal is ERROR
        if (suffix.substr(0, 1) == "u" || suffix.substr(0, 1) == "i") {
            return TokenType::TOKEN_ERROR;
        }
    }
    
    return TokenType::TOKEN_ERROR;  // Unknown suffix
}
```

Add declarations to `include/frontend/lexer/lexer.h`:

```cpp
// In private section of Lexer class:

// Type suffix scanning (zero-conversion policy)
std::string scanTypeSuffix();
TokenType suffixToTokenType(const std::string& suffix, bool isFloat);
```

---

## Phase 2: Parser Changes (AST Node Creation)

### 2.1: Update IntegerLiteral and FloatLiteral AST Nodes

Location: `include/frontend/ast/expr.h`

Add explicit type information to literal nodes:

```cpp
class IntegerLiteral : public Expr {
public:
    int64_t value;
    std::string raw_text;          // For high-precision
    std::string explicit_type;     // NEW: "u32", "i64", etc.
    
    IntegerLiteral(int64_t val, const std::string& raw, 
                   const std::string& type, int line, int col)
        : Expr(line, col), value(val), raw_text(raw), explicit_type(type) {}
};

class FloatLiteral : public Expr {
public:
    double value;
    std::string raw_text;
    std::string explicit_type;     // NEW: "f32", "f64", etc.
    
    FloatLiteral(double val, const std::string& raw,
                 const std::string& type, int line, int col)
        : Expr(line, col), value(val), raw_text(raw), explicit_type(type) {}
};
```

### 2.2: Update Parser to Handle Typed Tokens

Location: `src/frontend/parser/parser.cpp`

Modify literal parsing to extract type from token:

```cpp
Expr* Parser::parsePrimary() {
    // ... existing code ...
    
    // Integer literals
    if (match({TokenType::TOKEN_INTEGER_U8, TokenType::TOKEN_INTEGER_U16,
               TokenType::TOKEN_INTEGER_U32, TokenType::TOKEN_INTEGER_U64,
               TokenType::TOKEN_INTEGER_I8, TokenType::TOKEN_INTEGER_I16,
               TokenType::TOKEN_INTEGER_I32, TokenType::TOKEN_INTEGER_I64,
               // ... all typed integer tokens ...
               TokenType::TOKEN_TBB8, TokenType::TOKEN_TBB16, /* etc */})) {
        Token tok = previous();
        std::string type = tokenTypeToTypeString(tok.type);  // "u32", "i64", etc.
        return new IntegerLiteral(tok.value.int_value, tok.raw_literal_text, 
                                  type, tok.line, tok.column);
    }
    
    // Float literals
    if (match({TokenType::TOKEN_FLOAT_F32, TokenType::TOKEN_FLOAT_F64,
               TokenType::TOKEN_FLOAT_F128, /* etc */})) {
        Token tok = previous();
        std::string type = tokenTypeToTypeString(tok.type);  // "f32", "f64", etc.
        return new FloatLiteral(tok.value.float_value, tok.raw_literal_text,
                                type, tok.line, tok.column);
    }
    
    // ... rest of parsing ...
}
```

---

## Phase 3: Type Checker Changes (Remove Implicit Conversion)

### 3.1: Remove Numeric Literal Inference

Location: `src/frontend/sema/type_checker.cpp`

**OLD behavior (DELETE):**
```cpp
// Auto-infer type from context
if (literal without suffix) {
    infer int32 or uint32 based on value
}
```

**NEW behavior:**
```cpp
// Literal already has explicit type from parser
Type* TypeChecker::visitIntegerLiteral(IntegerLiteral* node) {
    // Type is EXPLICIT from suffix - no inference
    return lookupType(node->explicit_type);  // "u32" -> uint32 type
}

Type* TypeChecker::visitFloatLiteral(FloatLiteral* node) {
    // Type is EXPLICIT from suffix - no inference
    return lookupType(node->explicit_type);  // "f32" -> flt32 type
}
```

### 3.2: Remove Implicit Conversions in Binary Operations

**OLD behavior (DELETE):**
```cpp
// Auto-promote operands to common type
if (left_type != right_type) {
    auto-promote to wider type
}
```

**NEW behavior:**
```cpp
// REQUIRE exact type match OR explicit cast
Type* TypeChecker::visitBinaryExpr(BinaryExpr* node) {
    Type* left_type = visit(node->left);
    Type* right_type = visit(node->right);
    
    if (!typesEqual(left_type, right_type)) {
        error("Type mismatch: " + typeToString(left_type) + 
              " vs " + typeToString(right_type) +
              ". Explicit cast required. Example: cast<u32>(" + 
              node->left->toString() + ")");
        return ErrorType;
    }
    
    return left_type;
}
```

### 3.3: Add Explicit Cast Expression Support

Already exists in parser, but ensure type checker validates:

```cpp
Type* TypeChecker::visitCastExpr(CastExpr* node) {
    Type* source_type = visit(node->expr);
    Type* target_type = lookupType(node->target_type_name);
    
    // Validate cast is valid (size checks, signed/unsigned warnings, etc.)
    if (!isValidCast(source_type, target_type)) {
        error("Invalid cast from " + typeToString(source_type) +
              " to " + typeToString(target_type));
        return ErrorType;
    }
    
    return target_type;
}
```

---

## Phase 4: IR Generation Changes (Type Enforcement)

### 4.1: Use Explicit Types from AST

Location: `src/backend/ir_gen.cpp`

```cpp
llvm::Value* IRGenerator::visitIntegerLiteral(IntegerLiteral* node) {
    // Get LLVM type from explicit suffix
    llvm::Type* llvm_type = getLLVMType(node->explicit_type);  // "u32" -> i32
    
    if (node->raw_text.empty()) {
        // Normal int64_t value
        return llvm::ConstantInt::get(llvm_type, node->value, 
                                      isSigned(node->explicit_type));
    } else {
        // High-precision: parse APInt from raw string
        llvm::APInt big_value(getBitWidth(node->explicit_type), 
                              node->raw_text, 10);
        return llvm::ConstantInt::get(context, big_value);
    }
}

llvm::Value* IRGenerator::visitFloatLiteral(FloatLiteral* node) {
    llvm::Type* llvm_type = getLLVMType(node->explicit_type);  // "f32" -> float
    return llvm::ConstantFP::get(llvm_type, node->value);
}
```

### 4.2: Remove Automatic Type Promotion in Operations

```cpp
llvm::Value* IRGenerator::visitBinaryExpr(BinaryExpr* node) {
    llvm::Value* left = visit(node->left);
    llvm::Value* right = visit(node->right);
    
    // Types MUST match (type checker enforces this)
    assert(left->getType() == right->getType() && 
           "Type checker should have caught mismatch");
    
    // Generate operation - NO auto-promotion
    switch (node->op) {
        case OP_ADD:
            return builder.CreateAdd(left, right);
        // ...
    }
}
```

---

## Phase 5: Update Standard Library

### 5.1: Add Type Suffixes to All Numeric Literals

Example from `stdlib/core/int.aria`:

**Before:**
```aria
func add_u32(a: uint32, b: uint32) -> uint32 is
    if a > 0 && b > (4294967295 - a) then  // ❌ No suffix
        fail(ERR)
    end
    pass(a + b)
end
```

**After:**
```aria
func add_u32(a: uint32, b: uint32) -> uint32 is
    if a > 0u32 && b > (4294967295u32 - a) then  // ✅ Explicit suffix
        fail(ERR)
    end
    pass(a + b)
end
```

### 5.2: Update All Test Files

Every numeric literal in every test needs suffix:

```aria
// Before
assert_eq(add(1, 2), 3)

// After  
assert_eq(add(1u32, 2u32), 3u32)
```

---

## Phase 6: Testing Strategy

### 6.1: Unit Tests (Lexer)

Test file: `tests/lexer/test_type_suffixes.cpp`

```cpp
TEST(LexerTypeSuffixes, IntegerU32) {
    Lexer lexer("123u32");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens[0].type, TokenType::TOKEN_INTEGER_U32);
    ASSERT_EQ(tokens[0].value.int_value, 123);
}

TEST(LexerTypeSuffixes, MissingSuffixError) {
    Lexer lexer("123");  // Missing suffix
    auto tokens = lexer.tokenize();
    auto errors = lexer.getErrors();
    ASSERT_FALSE(errors.empty());
    ASSERT_THAT(errors[0], HasSubstr("missing required type suffix"));
}

TEST(LexerTypeSuffixes, FloatF32) {
    Lexer lexer("3.14f32");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens[0].type, TokenType::TOKEN_FLOAT_F32);
}

TEST(LexerTypeSuffixes, InvalidSuffixError) {
    Lexer lexer("3.14i32");  // Integer suffix on float
    auto tokens = lexer.tokenize();
    auto errors = lexer.getErrors();
    ASSERT_FALSE(errors.empty());
    ASSERT_THAT(errors[0], HasSubstr("Invalid type suffix"));
}
```

### 6.2: Integration Tests

After lexer unit tests pass, test full compilation:

```aria
// test_zero_conversion.aria
func main() -> int32 is
    let x: uint32 = 42u32;     // Explicit type match
    let y: uint32 = 100u32;
    let z: uint32 = x + y;     // Same types - OK
    
    // This should ERROR (different types):
    // let bad: uint32 = 42i32;  // Type mismatch
    
    pass(0i32)
end
```

---

## Phase 7: Documentation Updates

### 7.1: Error Message Guide

Create `docs/errors/E_MISSING_TYPE_SUFFIX.md`:

```markdown
# E_MISSING_TYPE_SUFFIX

## Error Message
```
Numeric literal missing required type suffix (e.g., 1u32, 0i64, 3.14f32)
```

## Cause
Aria requires ALL numeric literals to have explicit type suffixes.

## Examples

**Wrong:**
```aria
let x = 42;        // ❌ Missing suffix
let y = 3.14;      // ❌ Missing suffix
```

**Correct:**
```aria
let x = 42u32;     // ✅ Unsigned 32-bit
let y = 3.14f32;   // ✅ 32-bit float
```

## See Also
- Zero Implicit Conversion Guide
- Type Suffix Reference
```

---

## Implementation Order

1. ✅ **Documentation first** (DONE - specs, guides, references created)
2. **Lexer changes** (Phase 1)
   - Expand TokenType enum
   - Add suffix parsing to scanNumber()
   - Add helper functions
3. **Lexer unit tests** (Phase 6.1)
   - Test each suffix type
   - Test missing suffix error
   - Test invalid suffix error
4. **Parser changes** (Phase 2)
   - Update AST nodes
   - Handle typed tokens
5. **Type checker changes** (Phase 3)
   - Remove implicit conversion
   - Enforce exact type matching
   - Validate casts
6. **IR generation changes** (Phase 4)
   - Use explicit types
   - Remove auto-promotion
7. **Update stdlib** (Phase 5)
   - Add suffixes to all literals
   - Fix all tests
8. **Integration testing** (Phase 6.2)
   - End-to-end compilation tests
   - Error message validation

---

## Success Criteria

- [  ] Lexer emits error for numeric literal without suffix
- [  ] All type suffixes parse correctly (u8-u1024, i8-i1024, f32-f512, tbb8-tbb64, fix256)
- [  ] Parser creates AST nodes with explicit type information
- [  ] Type checker rejects operations on mismatched types
- [  ] IR generator uses exact types without promotion
- [  ] All stdlib functions compile with suffixed literals
- [  ] All tests pass with suffixed literals
- [  ] Error messages are clear and helpful
- [  ] Zero implicit conversions remain in compiled code

---

## Notes

**Why this matters:**
- Type pollution bug CANNOT be fixed with workarounds
- Implicit conversion enables entire classes of subtle bugs
- Zero-conversion prevents social engineering (no hidden assumptions)
- Critical for Nikola safety (children will use this)
- Eliminates gatekeeping (12-year-old can understand 1u32)

**Breaking change:** YES - all existing code must be updated. This is intentional. "It will look worse before it looks better" - construction principles apply.

**Timeline:** Estimated 2-3 days for complete implementation and testing.
