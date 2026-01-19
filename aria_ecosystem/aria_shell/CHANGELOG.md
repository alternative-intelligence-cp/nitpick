# Changelog

All notable changes to AriaSH will be documented in this file.

## [0.1.0] - 2026-01-15

### Added
- **Modal Input System**: Dual-mode editing (RUN/EDIT) with ESC toggle
- **Double-Semicolon Submit**: Type `;;` in EDIT mode to submit multi-line code
- **Expression Statements**: REPL now evaluates and displays expression results
- **ExprStmt AST Node**: New statement type for interactive expression evaluation
- **Null-Safe Execution**: Parser and executor handle errors without crashing
- **Semicolon-Tolerant Commands**: Built-in commands work with or without trailing semicolons
- **Clean Output Mode**: Removed debug spam for production-quality REPL experience

### Fixed
- **CRITICAL: Parser Infinite Loop**: Lexer now properly adds EOF token to prevent infinite loops
  - Root cause: `tokenize()` broke before adding EOF, causing `peek()` to loop forever
  - Impact: Parser hung on any input, making shell unusable
  - Solution: Add EOF token before breaking, with fallback safety check
  
- **CRITICAL: Segmentation Fault**: Parse errors no longer crash the shell
  - Root cause: Attempting to execute null or empty AST after parse failures
  - Impact: Invalid syntax caused immediate crash
  - Solution: Null checks in parseProgram() and execution guard in main loop
  
- **Built-in Commands in EDIT Mode**: Commands like `exit;;` now work properly
  - Root cause: Trailing semicolon from `;;` removal prevented exact string match
  - Impact: Could not exit from EDIT mode cleanly
  - Solution: Strip all trailing semicolons before command matching

### Changed
- **Parser Debug Output**: Moved from verbose logging to clean production mode
- **Error Handling**: Improved synchronization on parse errors
- **Statement Parsing**: Added lookahead logic to distinguish expressions from commands

### Technical Details

**Lexer Improvements** (`src/parser/lexer.cpp`):
```cpp
// Before: EOF not added, causing infinite loops
while (!isAtEnd()) {
    Token token = nextToken();
    if (token.type != TokenType::UNKNOWN) tokens.push_back(token);
    if (token.type == TokenType::END_OF_FILE) break;  // Breaks before adding!
}

// After: EOF always present
while (!isAtEnd()) {
    Token token = nextToken();
    tokens.push_back(token);  // Add all tokens including EOF
    if (token.type == TokenType::END_OF_FILE) break;
}
// Safety: Ensure EOF always present
if (tokens.empty() || tokens.back().type != TokenType::END_OF_FILE) {
    tokens.push_back(makeToken(TokenType::END_OF_FILE, ""));
}
```

**Parser Safety** (`src/parser/parser.cpp`):
```cpp
// Null-safe statement addition
auto stmt = parseStatement();
if (stmt) {  // Only add non-null statements
    program->statements.push_back(std::move(stmt));
}
```

**Execution Guard** (`src/repl/ariash_main.cpp`):
```cpp
// Only execute if we have valid statements
if (ast && !ast->statements.empty()) {
    executor::Executor exec(globalEnv);
    exec.execute(*ast);
}
```

## Session 4 Summary

**Duration**: ~4 hours of intensive debugging and feature development

**Major Milestones**:
1. Identified and fixed parser infinite loop (lexer EOF bug)
2. Implemented modal input system with ESC toggle
3. Added double-semicolon submit pattern for multi-line editing
4. Created ExprStmt for REPL expression evaluation
5. Eliminated segfaults with null-safe guards
6. Cleaned up debug output for production quality

**Testing Results**:
- ✅ Multi-line variable declarations with arithmetic
- ✅ Expression evaluation: `x + y` → `=> 30`
- ✅ Built-in commands in both modes
- ✅ Error recovery without crashes
- ✅ Modal switching with visual feedback

**External Achievement**:
- 🎊 ATPM paper published on ai.viXra.org (citation: 2601.0052v1)

## [0.0.1] - 2026-01-10 (Previous Sessions)

### Initial Implementation
- Basic lexer with token recognition
- Recursive descent parser
- AST visitor pattern executor
- Simple variable storage
- Command execution framework
- Terminal raw mode handling

---

**Note**: This changelog follows semantic versioning and documents all significant changes to the AriaSH project.
