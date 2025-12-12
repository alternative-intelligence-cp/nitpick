# Backward Pipeline Operator Bug Fix - Summary

## Problem
The backward pipeline operator `<|` was not working in the Aria compiler. Test case `f <| 5;` would fail with parse errors.

## Root Cause Analysis

### Bug #1: Token Identifier Classification
**Location**: `parser.cpp:2308` - `isTypeToken()` function  
**Issue**: The function returned `true` for ALL `TOKEN_IDENTIFIER` tokens, causing `parseProgram()` to treat user variables like `f` as type declarations, expecting pattern `f:name`.

**Fix**: Removed `TOKEN_IDENTIFIER` from `isTypeToken()` check. Now only built-in type keywords (TOKEN_TYPE_VOID through TOKEN_TYPE_STRING) are considered type tokens in statement context.

### Bug #2: Lexer State Desynchronization in parseStmt()
**Location**: `parser.cpp:1295-1350` - `parseStmt()` lookahead logic  
**Issue**: When distinguishing between variable declarations (`type:name`) and expression statements (`identifier`), `parseStmt()` would:
1. Save current token (e.g., `f`)
2. Call `advance()` to peek at next token (e.g., `<|`)  
3. If not a colon, restore `current = saved` back to `f`
4. But **lexer internal position had already moved past `<|` to `5`!**

When `parseExpr()` was then called, it would start from `f` but the lexer would return `5` instead of `<|`, completely skipping the pipeline operator.

**Fix**: Simplified `parseStmt()` to avoid lookahead for identifiers:
- Built-in type keywords still use lookahead (acceptable since they're rare)
- Identifiers are now assumed to be expressions (no lookahead)
- Variable declarations with user-defined types must use explicit syntax at module level

## Changes Made

### File: `src/frontend/parser.cpp`

#### 1. Modified `isTypeToken()` (line 2308)
```cpp
// Before:
return (type >= TOKEN_TYPE_VOID && type <= TOKEN_TYPE_STRING) || 
       type == TOKEN_IDENTIFIER ||   // BUG: All identifiers treated as types!
       type == TOKEN_KW_FUNC ||
       type == TOKEN_TYPE_FUNC;

// After:
return (type >= TOKEN_TYPE_VOID && type <= TOKEN_TYPE_STRING) || 
       type == TOKEN_KW_FUNC ||
       type == TOKEN_TYPE_FUNC;
// TOKEN_IDENTIFIER explicitly excluded
```

#### 2. Simplified `parseStmt()` (lines 1295-1345)
```cpp
// Before: Complex lookahead for both type keywords AND identifiers
// After: Separated handling:

// Type keywords: Use lookahead (rare case)
if (current.type >= TOKEN_TYPE_VOID && current.type <= TOKEN_TYPE_STRING) {
    // ... lookahead logic ...
}

// Identifiers: Assume expression (NO lookahead)
if (current.type == TOKEN_IDENTIFIER) {
    auto expr = parseExpr();  // Directly parse as expression
    expect(TOKEN_SEMICOLON);
    return std::make_unique<ExpressionStmt>(std::move(expr));
}
```

#### 3. Cleaned up debug output
- Removed all `[DEBUG ...]` print statements
- Removed temporary token tracking code

## Verification

### Test Cases
1. **Backward pipeline**: `f <| 5` ✅ PASSES (parses correctly)
2. **Forward pipeline**: `5 |> f` ✅ PASSES (still works)
3. **Minimal test**: `test_minimal_back.aria` ✅ PASSES

### Lexer Verification
Standalone lexer test confirms tokens are correct:
```
Token type=9 (IDENTIFIER) value='f'
Token type=206 (PIPE_BACKWARD) value='<|'
Token type=3 (INT_LITERAL) value='5'
Token type=221 (SEMICOLON) value=';'
```

## Implementation Details

### Pipeline Operator Precedence
- **Forward `|>`**: Token 205, right-to-left application: `x |> f` → `f(x)`
- **Backward `<|`**: Token 206, left-to-right application: `f <| x` → `f(x)`
- Both operators transform into `CallExpr` nodes during parsing

### Parser Architecture
```
parseExpr()
  → parseAssignment()
    → parsePipeline()        ← Pipeline operators handled here
      → parseNullCoalesce()
        → parseTernary()
          → ... (precedence chain)
            → parsePrimary()  ← Identifiers parsed here
```

## Known Limitations

1. **Type keyword statements**: Type keywords at statement level (inside functions) MUST be followed by `:` for declarations. Expression-like usage will throw an error.

2. **User-defined type declarations**: Inside function bodies, user-defined types cannot be used for variable declarations. Use built-in types or declare at module level.

3. **Lexer token buffer**: Parser lacks token pushback mechanism. Future improvement would add `peek()` to lexer for proper lookahead.

## Next Steps

1. ✅ **Backward pipeline parsing** - COMPLETE
2. 🔄 **Codegen for pipelines** - Needs fix for Result type system
3. ⏳ **Null coalescing `??`** - Awaiting BinaryOp enum addition
4. ⏳ **Safe navigation `?.`** - Implementation pending
5. ⏳ **Range operators `..` and `...`** - RangeExpr AST needed

## Files Modified
- `src/frontend/parser.cpp` (lines 50-55, 933-1003, 1295-1345, 2308-2318)

## Testing Commands
```bash
# Backward pipeline
./build/ariac tests/parser/test_minimal_back.aria
./build/ariac tests/parser/test_simple_back.aria

# Forward pipeline (regression test)
./build/ariac tests/parser/test_simple_forward.aria

# Lexer standalone test
g++ -std=c++17 -I. -o /tmp/test_lexer test_lexer.cpp src/frontend/lexer.cpp
/tmp/test_lexer
```

## Conclusion
The backward pipeline operator `<|` now parses correctly. The root cause was a combination of:
1. Over-broad type token detection
2. Lexer state desynchronization during lookahead

The fix simplifies the parser logic and eliminates the desync issue by avoiding lookahead for identifiers in statement context.
