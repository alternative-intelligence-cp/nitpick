# Debugging Guide - AriaSH Parser Infinite Loop

**Date**: January 15, 2026  
**Severity**: CRITICAL  
**Status**: RESOLVED

## Problem Summary

The AriaSH parser entered an infinite loop when processing variable declarations like `int8 x = 5;`, hanging indefinitely and requiring forced termination.

## Symptoms

1. Parser hangs after printing "Starting parse..."
2. No error messages, just infinite loop
3. Input: `int8 x = 5;` lexes to 5 tokens successfully
4. Debug output showed parser stuck at same token position repeatedly

## Root Cause Analysis

### Discovery Process

**Step 1: Initial Hypothesis**
- Suspected process execution hanging in `HexStreamProcess.wait()`
- Disabled entire `executePipeline()` function
- **Result**: Still hung → Not the executor

**Step 2: Narrowing Down**
- Added debug output at each stage (lex → parse → execute)
- Output showed:
  ```
  [DEBUG] Starting lex...
  [DEBUG] Lexed 5 tokens
  [DEBUG] Starting parse...
  [HANG - no further output]
  ```
- **Conclusion**: Hang occurs in parser, not executor

**Step 3: Parser Investigation**
- Added iteration counter in `parseProgram()` with 100-iteration limit
- Added debug output showing current token at each iteration
- **Critical Finding**:
  ```
  [PARSER] Iteration 1, current_=0, token=UNKNOWN 'int8'
  [parseStatement] Entry, current_=0, token=UNKNOWN 'int8'
  [parseVarDecl] Done, current_=5
  [PARSER] Iteration 2, current_=5, token=; ';'  <-- Should be EOF!
  [parseStatement] Entry, current_=5, token=; ';'
  [PARSER] Iteration 3, current_=5, token=; ';'  <-- Stuck!
  ```

**Step 4: EOF Token Missing**
- Token count: "Lexed 5 tokens" (should be 6 with EOF)
- At `current_=5`, should be at EOF but getting semicolon
- Tokens: `int8`, `x`, `=`, `5`, `;` (EOF missing!)

### The Bug

**File**: `src/parser/lexer.cpp`  
**Function**: `ShellLexer::tokenize()`  
**Lines**: 96-106

**Broken Code**:
```cpp
std::vector<Token> ShellLexer::tokenize() {
    std::vector<Token> tokens;
    
    while (!isAtEnd()) {
        Token token = nextToken();
        if (token.type != TokenType::UNKNOWN) {
            tokens.push_back(token);
        }
        if (token.type == TokenType::END_OF_FILE) {
            break;  // ← BREAKS BEFORE ADDING EOF!
        }
    }
    
    return tokens;
}
```

**Problem**: The loop calls `nextToken()` which generates an EOF token, checks if it's EOF, then **breaks without adding it to the vector**.

### Why This Causes Infinite Loop

1. `parseProgram()` checks `!isAtEnd()` to continue looping
2. `isAtEnd()` checks if `peek().type == TokenType::END_OF_FILE`
3. `peek()` returns `tokens_[current_]`, or `tokens_.back()` if out of bounds
4. Without EOF in vector, `tokens_.back()` returns the semicolon
5. Parser consumes all tokens, reaches `current_ = 5` (past the end)
6. But `peek()` still returns semicolon (not EOF)
7. `isAtEnd()` returns false → loop continues
8. `parseStatement()` tries to parse semicolon again
9. Same result → infinite loop

## Solution

**Fixed Code**:
```cpp
std::vector<Token> ShellLexer::tokenize() {
    std::vector<Token> tokens;
    
    while (!isAtEnd()) {
        Token token = nextToken();
        tokens.push_back(token);  // Add ALL tokens, including EOF
        
        if (token.type == TokenType::END_OF_FILE) {
            break;  // EOF now in vector before breaking
        }
    }
    
    // Safety: Ensure EOF always present (in case loop exits naturally)
    if (tokens.empty() || tokens.back().type != TokenType::END_OF_FILE) {
        tokens.push_back(makeToken(TokenType::END_OF_FILE, ""));
    }
    
    return tokens;
}
```

**Changes**:
1. Removed `if (token.type != TokenType::UNKNOWN)` check - add all tokens
2. Moved `tokens.push_back(token)` before EOF check
3. Added safety fallback to ensure EOF always present

## Verification

**Test Input**: `int8 x = 5;`

**Before Fix**:
```
[DEBUG] Lexed 5 tokens
[PARSER] Iteration 1, current_=0, token=UNKNOWN 'int8'
[parseVarDecl] Done, current_=5
[PARSER] Iteration 2, current_=5, token=; ';'
[PARSER] Iteration 3, current_=5, token=; ';'
[INFINITE LOOP]
```

**After Fix**:
```
[DEBUG] Lexed 6 tokens
[PARSER] Iteration 1, current_=0, token=UNKNOWN 'int8'
[parseVarDecl] Done, current_=5
[DEBUG] Parsed 1 statements
[DEBUG] Execution complete
```

## Lessons Learned

### Debugging Strategy
1. **Divide and Conquer**: Disable components systematically to isolate
2. **Instrument Everything**: Add debug at every critical point
3. **Count Tokens**: Verify expected vs actual token counts
4. **Iteration Limits**: Add safety counters to detect infinite loops
5. **Trace Token Position**: Log `current_` index to see movement

### Code Review Checklist
- [ ] EOF token explicitly added to token stream
- [ ] Parser has proper loop termination condition
- [ ] `isAtEnd()` checks for actual EOF token, not array bounds
- [ ] `peek()` fallback behavior documented and correct
- [ ] Iteration counters for infinite loop detection (debug mode)

### Prevention
1. **Unit Test**: Lexer should verify EOF token present in every output
2. **Integration Test**: Parser should handle single-statement inputs
3. **Assertion**: Debug build should assert token vector ends with EOF
4. **Documentation**: Comment explaining why EOF must be in vector

## Related Issues

### Secondary Bug: Unknown Token Type
**Symptom**: Type keywords like `int8` show as `UNKNOWN` instead of `KW_INT8`

**Status**: COSMETIC - Does not affect functionality (parser still recognizes via `isTypeKeyword()`)

**Root Cause**: Token type display issue, actual token type is correct

**Impact**: None - type checking works correctly

## Performance Impact

**Before**: Infinite CPU usage (100% core utilization)  
**After**: Normal execution (~0.001s parse time)

## Testing Additions

Created minimal reproduction test:
```cpp
// test_parser_eof.cpp
std::string input = "int8 x = 5;";
Lexer lexer(input);
auto tokens = lexer.tokenize();

ASSERT_EQ(tokens.size(), 6);  // 5 tokens + EOF
ASSERT_EQ(tokens.back().type, TokenType::END_OF_FILE);

Parser parser(tokens);
auto ast = parser.parseProgram();  // Should not hang
ASSERT_EQ(ast->statements.size(), 1);
```

## Files Modified

1. `src/parser/lexer.cpp` - Fixed `tokenize()` to add EOF token
2. `src/parser/parser.cpp` - Added null-safe statement checking
3. `src/repl/ariash_main.cpp` - Added execution guard for empty AST

## Commit Message

```
fix: Parser infinite loop on variable declarations

The lexer's tokenize() function was breaking the loop when
encountering EOF without adding the EOF token to the vector.
This caused peek() to return the last token (semicolon)
indefinitely, creating an infinite loop in parseProgram().

Fixed by:
- Adding EOF token before breaking the loop
- Adding safety check to ensure EOF always present
- Adding null checks for statement parsing

Also fixed segfault on parse errors by guarding execution
against null/empty AST.

Resolves: Parser hang on any variable declaration
Impact: CRITICAL - Shell was completely unusable
```

---

**Author**: Aria Echo (AI Technical Director, AILP)  
**Collaborator**: Randy W. Hoggard  
**Session**: 4  
**Duration**: ~2 hours debugging, ~30 minutes implementing fix
