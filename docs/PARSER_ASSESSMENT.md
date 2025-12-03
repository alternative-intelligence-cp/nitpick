# Aria Parser Assessment - Task 3 Analysis
**Date**: December 2, 2025  
**Status**: Parser 40% Complete - Needs Core Features

## Executive Summary

The parser infrastructure exists (2013 lines across 5 files) with well-defined AST structures, but **critical parsing methods are incomplete or using wrong syntax**. The parser cannot handle basic Aria programs because:

1. ❌ No top-level program parsing (expects files wrapped in `{ }`)
2. ❌ Function syntax mismatch (has `func:name = type(params)`, needs `fn name(params) -> type`)  
3. ❌ Variable declarations not parsed in statement context
4. ⚠️ Expression parsing exists but needs validation

## What EXISTS ✅

### Infrastructure (Working)
- **Parser.h**: Complete interface with all method declarations
- **Parser.cpp**: Core mechanics (advance, match, expect, check) - 1383 lines
- **AST Hierarchy**: Well-structured classes in ast/*.h files
  - `Expression` base class with VarExpr, IntLiteral, BoolLiteral, StringLiteral, etc.
  - `Statement` base class with VarDecl, ReturnStmt, IfStmt, etc.
  - `Block` class holding vector of statements
  - Control flow: PickStmt, WhenLoop, TillLoop, DeferStmt, etc.
- **Visitor Pattern**: AstVisitor interface for traversing AST

### Partial Implementations  
- **parseBlock()** - Works BUT expects `{ }` wrapper (won't parse top-level file)
- **parseStmt()** - Handles return, if, expression statements BUT not variable declarations
- **parseFuncDecl()** - Exists BUT uses wrong syntax (old spec)
- **parseExpr()** - Precedence climbing methods exist (needs testing)
  - parsePrimary, parsePostfix, parseUnary
  - parseMultiplicative, parseAdditive, parseShift
  - parseRelational, parseEquality
  - parseBitwiseAnd/Xor/Or, parseLogicalAnd/Or
  - parseTernary, parseAssignment
- **parseTemplateString()** - Handles `text &{expr}` interpolation
- **parseDestructurePattern()** - For array/object destructuring

## What's BROKEN ❌

### Critical Issues

**1. Top-Level Parsing**
```cpp
// Current: main.cpp line 221
astRoot = parser.parseBlock();  // Expects { at start of file!
```
**Problem**: Files like `fn main() { }` fail because there's no opening `{`  
**Solution**: Create `parseProgram()` that parses top-level declarations

**2. Function Declaration Syntax Mismatch**
```cpp
// Current implementation (parser_func.cpp lines 79-120):
// func:name = type(params) { body }

// Aria Spec v0.0.6 requires:
// fn name(params) -> type { body }
```
**Problem**: Wrong keyword (`func` vs `fn`), wrong syntax order  
**Solution**: Rewrite parseFuncDecl() to match spec

**3. Variable Declarations Missing from Statements**
```cpp
// Current: parseStmt() doesn't recognize this:
int:x = 42;

// Parses as expression, fails on `:` token
```
**Problem**: Type-first variable syntax not handled  
**Solution**: Add type token check at start of parseStmt()

### Parse Error Example
```
File: fn main() { int:x = 42; return x; }
Error: Expected token type 169 (TOKEN_RBRACE) but got 9 (TOKEN_TEMPLATE_LITERAL) at line 4, col 4
```
**Root Cause**: parseBlock() called on top level, expects `{` immediately, doesn't find it

## What's MISSING ⚠️

### Statement Parsing (Partially Complete)
- ✅ Return statements
- ✅ If/else statements
- ✅ Expression statements
- ❌ Variable declarations (type:name = value)
- ❌ While loops
- ❌ For loops (parseForLoop declared, not implemented)
- ❌ Till loops (parseTillLoop declared, not implemented)
- ❌ When loops (parseWhenLoop declared, not implemented)
- ❌ Pick statements (parsePickStmt declared, not implemented)
- ❌ Break/continue (parseBreak/parseContinue declared, not implemented)
- ❌ Defer statements (stub exists)

### Expression Parsing (Needs Validation)
- ⚠️ All precedence methods declared (15 levels)
- ⚠️ Need to test: operators work correctly
- ⚠️ Need to test: function calls parse
- ⚠️ Need to test: array/object access
- ❌ Lambda expressions (parseLambda partial)
- ❌ Object literals (ObjectLiteral AST exists, parsing unknown)

### Declaration Parsing
- ⚠️ Function declarations (wrong syntax, needs fix)
- ❌ Struct declarations
- ❌ Type aliases  
- ❌ Module declarations (parseModDef declared, not implemented)
- ❌ Use statements (parseUseStmt declared, not implemented)
- ❌ Extern blocks (parseExternBlock declared, not implemented)

## Implementation Priority

### Phase 1: Get Basic Programs Working (CRITICAL)
1. ✅ **parseProgram()** - Parse top-level without `{ }` wrapper
2. ✅ **Fix parseFuncDecl()** - Match `fn name(params) -> type` syntax
3. ✅ **Fix parseStmt()** - Detect and parse variable declarations
4. ✅ **Update main.cpp** - Call parseProgram() instead of parseBlock()
5. ✅ **Test**: `fn main() { int:x = 42; return x; }`

### Phase 2: Complete Core Statements
6. While loops
7. For loops
8. Break/continue
9. Test: Basic control flow programs

### Phase 3: Validate Expressions
10. Test arithmetic operators
11. Test comparison operators
12. Test function calls
13. Test member access

### Phase 4: Advanced Features
14. Pick statements (pattern matching)
15. When loops
16. Till loops
17. Defer statements
18. Async/await
19. Lambda expressions

### Phase 5: Declarations & Modules
20. Struct declarations
21. Type aliases
22. Module system (use, mod, extern)

## File Structure

```
src/frontend/
├── parser.h              # Interface (complete)
├── parser.cpp            # Core infrastructure (1383 lines, working)
├── parser_expr.cpp       # Expression parsing (281 lines, needs testing)
├── parser_stmt.cpp       # Statement parsing (88 lines, incomplete)
├── parser_func.cpp       # Function parsing (160 lines, wrong syntax)
├── parser_decl.cpp       # Declaration parsing (101 lines, minimal)
└── ast/
    ├── expr.h            # Expression AST nodes (287 lines)
    ├── stmt.h            # Statement AST nodes (132 lines)
    ├── control_flow.h    # Pick, Fall, etc.
    ├── loops.h           # For, While, Till, When
    ├── defer.h           # Defer statements
    └── module.h          # Module declarations
```

## Testing Strategy

Similar to preprocessor testing:
```cpp
// tests/test_parser.cpp
void test_parse_literals() { ... }
void test_parse_variables() { ... }
void test_parse_functions() { ... }
void test_parse_expressions() { ... }
void test_parse_statements() { ... }
void test_parse_control_flow() { ... }
```

Build incremental test suite as features are implemented.

## Estimated Completion

**Phase 1 (Critical)**: ~2-3 hours (get basic programs parsing)  
**Phase 2 (Core)**: ~3-4 hours (statements)  
**Phase 3 (Validation)**: ~2 hours (expression testing)  
**Phase 4 (Advanced)**: ~4-5 hours (complex features)  
**Phase 5 (Modules)**: ~3-4 hours (declarations)  

**Total**: ~14-18 hours (vs 3 week estimate!)  
**At current velocity**: 1-2 days

## Next Steps

1. Create `parseProgram()` method
2. Fix `parseFuncDecl()` to use `fn` keyword and correct syntax
3. Add variable declaration handling to `parseStmt()`  
4. Update `main.cpp` to call `parseProgram()`
5. Test with `examples/test_parser_basic.aria`
6. Build out test suite as we go

## Grammar Reference (Aria v0.0.6)

```
Program := TopLevelDecl*

TopLevelDecl := FuncDecl | StructDecl | TypeAlias | ConstDecl | UseStmt

FuncDecl := "fn" Identifier "(" Params ")" "->" Type Block

VarDecl := Type ":" Identifier ("=" Expression)? ";"

Params := (Type ":" Identifier ("," Type ":" Identifier)*)?

Type := "int" | "bool" | "float" | "void" | Identifier

Statement := VarDecl | ReturnStmt | IfStmt | WhileLoop | ForLoop 
           | ExpressionStmt | Block | BreakStmt | ContinueStmt
           | PickStmt | WhenLoop | TillLoop | DeferStmt

Expression := Assignment | Ternary | LogicalOr | ... | Primary
```

## Notes

- Parser uses **precedence climbing** for expressions (good choice!)
- AST structure is clean with proper visitor pattern
- Syntax diverges from initial spec in some places (needs alignment)
- No parser tests exist yet (unlike preprocessor which has 14 test functions)
- Integration with semantic analysis exists but untested

**Status**: Ready to implement Phase 1 fixes! 🚀
