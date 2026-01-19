# Aria Language Server Protocol (aria-lsp)

**Component Type**: Language Server / IDE Integration  
**Language**: C++20 (planned)  
**Protocol**: LSP (Language Server Protocol) v3.17  
**Repository**: Design phase (not yet created)  
**Status**: Design / Specification Phase  
**Version**: v0.0.1-concept

---

## Table of Contents

1. [Overview](#overview)
2. [LSP Architecture](#lsp-architecture)
3. [Shared Components with Compiler](#shared-components-with-compiler)
4. [Core Features](#core-features)
5. [Aria-Specific Extensions](#aria-specific-extensions)
6. [Performance Optimizations](#performance-optimizations)
7. [Integration Points](#integration-points)

---

## Overview

The Aria Language Server (aria-lsp) provides IDE integration for Aria development. It implements the Language Server Protocol (LSP), enabling rich editing features in any LSP-compatible editor (VSCode, Vim, Emacs, etc.).

### Key Features

- **Syntax Highlighting**: Semantic token highlighting
- **Code Completion**: Context-aware suggestions
- **Diagnostics**: Real-time error/warning reporting
- **Go to Definition**: Navigate to symbol definitions
- **Find References**: Locate all uses of a symbol
- **Hover Information**: Type info, documentation on hover
- **Code Actions**: Quick fixes, refactoring suggestions
- **Formatting**: Consistent code style
- **TBB-Aware**: Understands Type-Based Bounded integers
- **Result<T> Support**: Monadic error handling assistance

---

## LSP Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        ARIA-LSP                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ LSP Protocol Handler                                       ││
│  │  - JSON-RPC message parsing                                ││
│  │  - Request routing                                         ││
│  │  - Response serialization                                  ││
│  └────────────────────────────────────────────────────────────┘│
│                           ↓                                     │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ Document Manager                                           ││
│  │  - Track open files                                        ││
│  │  - Incremental updates (textDocument/didChange)           ││
│  │  - Version tracking                                        ││
│  └────────────────────────────────────────────────────────────┘│
│                           ↓                                     │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ SHARED WITH COMPILER                                       ││
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    ││
│  │  │    Lexer     │→ │    Parser    │→ │  Semantic    │    ││
│  │  │              │  │              │  │  Analyzer    │    ││
│  │  └──────────────┘  └──────────────┘  └──────────────┘    ││
│  │        ↓                   ↓                  ↓            ││
│  │     Tokens               AST            Symbol Table      ││
│  └────────────────────────────────────────────────────────────┘│
│                           ↓                                     │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ LSP Feature Providers                                      ││
│  │  - Completion Provider (autocomplete)                      ││
│  │  - Hover Provider (type info)                              ││
│  │  - Definition Provider (go to def)                         ││
│  │  - References Provider (find usages)                       ││
│  │  - Diagnostics Provider (errors/warnings)                  ││
│  │  - Formatting Provider (code style)                        ││
│  │  - Code Action Provider (quick fixes)                      ││
│  └────────────────────────────────────────────────────────────┘│
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
                            ↕
                     JSON-RPC over stdio
                            ↕
┌─────────────────────────────────────────────────────────────────┐
│                    IDE / EDITOR                                 │
│              (VSCode, Vim, Emacs, etc.)                         │
└─────────────────────────────────────────────────────────────────┘
```

---

## Shared Components with Compiler

### Why Share Code?

**Benefits**:
1. **Consistency**: LSP sees same errors as compiler
2. **Reduced Maintenance**: One codebase for lexing/parsing/semantic analysis
3. **Correctness**: No divergence between IDE and compiler behavior

**Shared Components**:
- **Lexer**: Tokenization (same token stream as compiler)
- **Parser**: AST construction (same tree structure)
- **Semantic Analyzer**: Type checking, symbol resolution
- **Type System**: TBB, Result<T>, generic types

### Implementation Strategy

**Library Extraction**:
```
libaria_frontend.a
├── lexer.cpp
├── parser.cpp
├── semantic_analyzer.cpp
├── type_checker.cpp
└── symbol_table.cpp
```

**Used by**:
- `ariac` (compiler): Links with libaria_frontend.a
- `aria-lsp` (language server): Links with libaria_frontend.a

**Difference**:
- Compiler: Full pipeline (lexer → parser → semantic → IR → codegen)
- LSP: Stops at semantic analysis (no IR generation, no codegen)

---

## Core Features

### 1. Syntax Highlighting (Semantic Tokens)

**LSP Request**: `textDocument/semanticTokens/full`

**Token Types**:
- **Keywords**: `func`, `use`, `pass`, `result`, `tbb8`, `wild`, etc.
- **Types**: `int64`, `string`, `result<T>`, custom structs
- **Variables**: Local variables, function parameters
- **Functions**: Function names
- **Operators**: `+`, `-`, `*`, `=`, etc.
- **Literals**: Numbers, strings, booleans

**Example**:
```aria
func:main = int64() {
    tbb8:age = 25;
    result<string>:data = io.read_file("input.txt");
    pass(0);
}
```

**Semantic Tokens**:
```
func      → keyword
main      → function
int64     → type
tbb8      → type (TBB)
age       → variable
25        → number
result    → type
string    → type
data      → variable
io        → namespace
read_file → function
pass      → keyword
```

---

### 2. Code Completion

**LSP Request**: `textDocument/completion`

**Trigger**: User types `.` or identifier prefix

**Example**:
```aria
io.█
```

**Completion Items**:
```
io.stdin          (TextStream)
io.stdout         (TextStream)
io.stderr         (TextStream)
io.stddbg         (TextStream)
io.stddati        (BinaryStream)
io.stddato        (BinaryStream)
io.read_file      (func(path: string) -> result<string>)
io.write_file     (func(path: string, data: string) -> result<void>)
```

**Context-Aware**:
```aria
result<string>:data = io.read_file("test.txt");
if (data.█
```

**Completion for Result<T>**:
```
data.is_ok()      (func() -> bool)
data.is_err()     (func() -> bool)
data.ok()         (func() -> string)  [warning: check is_err first!]
data.err()        (func() -> string)
```

---

### 3. Diagnostics (Errors & Warnings)

**LSP Notification**: `textDocument/publishDiagnostics`

**Error Detection**:
- **Syntax Errors**: Missing semicolons, unmatched braces
- **Type Errors**: Type mismatch, undefined variable
- **TBB Overflow**: `tbb8:x = 200;` → Error: 200 out of range
- **Unchecked Result**: `data.ok()` without `is_err()` check → Warning

**Example**:
```aria
func:main = int64() {
    tbb8:x = 200;  // ERROR: 200 out of range for tbb8 [-128, 127]
    
    result<string>:data = io.read_file("test.txt");
    io.stdout.write(data.ok());  // WARNING: Unchecked result access
    
    pass(0);
}
```

**Diagnostics Sent to Editor**:
```json
{
  "diagnostics": [
    {
      "range": {
        "start": {"line": 1, "character": 13},
        "end": {"line": 1, "character": 16}
      },
      "severity": 1,  // Error
      "message": "Value 200 out of range for tbb8 [-128, 127]"
    },
    {
      "range": {
        "start": {"line": 4, "character": 19},
        "end": {"line": 4, "character": 27}
      },
      "severity": 2,  // Warning
      "message": "Accessing result value without checking for error. Consider using 'if (data.is_err())' first."
    }
  ]
}
```

---

### 4. Go to Definition

**LSP Request**: `textDocument/definition`

**Example**:
```aria
// File: src/main.aria
use parser;

func:main = int64() {
    result<ast>:tree = parser.parse("code");
                       ^^^^^^ (cursor here)
    pass(0);
}
```

**LSP Response**: Jump to definition in `src/parser.aria`:
```aria
// File: src/parser.aria
func:parse = result<ast>(code: string) {
    // ... implementation
}
```

**Symbol Table Lookup**:
1. Resolve `parser` → module `src/parser.aria`
2. Resolve `parse` → function definition at line 5, col 6
3. Return location to editor

---

### 5. Find References

**LSP Request**: `textDocument/references`

**Example**: Find all uses of `parse_expr` function

**Search**:
1. Query symbol table for `parse_expr` definition
2. Scan all files for calls to `parse_expr`
3. Return list of locations

**Results**:
```
src/parser.aria:10:6    - Definition
src/parser.aria:45:12   - Call: parse_expr(tokens)
src/main.aria:23:9      - Call: expr = parse_expr(input)
tests/test_parser.aria:15:5 - Call: parse_expr("1 + 2")
```

---

### 6. Hover Information

**LSP Request**: `textDocument/hover`

**Example**:
```aria
result<string>:data = io.read_file("test.txt");
                      ^^^^^^^^^^^^
```

**Hover Response**:
```
func:read_file = result<string>(path: string)

Reads entire file into a string.

Returns:
  - Ok(string): File contents
  - Err(string): Error message if file cannot be read

Example:
  result<string>:content = io.read_file("input.txt");
  if (content.is_err()) {
      io.stderr.write("Error: " + content.err());
  }
```

**Type Information**:
```aria
tbb8:age = 25;
     ^^^
```

**Hover**:
```
tbb8:age
Type: tbb8 (Type-Based Bounded integer, range: -128 to 127)
Value: 25
```

---

### 7. Code Actions (Quick Fixes)

**LSP Request**: `textDocument/codeAction`

**Example 1: Add Error Check**
```aria
result<string>:data = io.read_file("test.txt");
io.stdout.write(data.ok());  // ← Cursor here, warning shown
```

**Code Action**:
```
⚡ Add error check
```

**Result**:
```aria
result<string>:data = io.read_file("test.txt");
if (data.is_err()) {
    io.stderr.write("Error: " + data.err());
    pass(1);
}
io.stdout.write(data.ok());
```

**Example 2: Fix TBB Overflow**
```aria
tbb8:x = 200;  // Error: out of range
```

**Code Actions**:
```
⚡ Change type to int16
⚡ Clamp value to 127
⚡ Remove assignment
```

---

### 8. Formatting

**LSP Request**: `textDocument/formatting`

**Format Rules**:
- Indentation: 4 spaces (or tabs)
- Brace style: K&R or Allman (configurable)
- Line length: 100 characters (configurable)
- Spacing: `x = 5` (spaces around operators)

**Before**:
```aria
func:main=int64(){tbb8:x=25;if(x>0){pass(x);}else{pass(0);}}
```

**After**:
```aria
func:main = int64() {
    tbb8:x = 25;
    if (x > 0) {
        pass(x);
    } else {
        pass(0);
    }
}
```

---

## Aria-Specific Extensions

### TBB Range Information

**Custom LSP Extension**: `aria/tbbRange`

**Request**:
```json
{
  "method": "aria/tbbRange",
  "params": {
    "textDocument": {"uri": "file:///src/main.aria"},
    "position": {"line": 2, "character": 10}
  }
}
```

**Response**:
```json
{
  "type": "tbb8",
  "range": [-128, 127],
  "currentValue": 25,
  "canOverflow": false
}
```

**Use Case**: IDE shows visual indicator for TBB overflow potential

---

### Result<T> Flow Analysis

**Custom LSP Extension**: `aria/resultFlow`

**Analyze if Result<T> is checked before use**:
```json
{
  "method": "aria/resultFlow",
  "params": {
    "textDocument": {"uri": "file:///src/main.aria"},
    "position": {"line": 5, "character": 20}
  }
}
```

**Response**:
```json
{
  "variable": "data",
  "type": "result<string>",
  "checkedBefore": false,
  "suggestion": "Add 'if (data.is_err())' check before accessing '.ok()'"
}
```

---

## Performance Optimizations

### Incremental Parsing

**Problem**: Re-parsing entire file on every keystroke is slow

**Solution**: Incremental updates

**LSP Notification**: `textDocument/didChange`
```json
{
  "contentChanges": [
    {
      "range": {
        "start": {"line": 10, "character": 4},
        "end": {"line": 10, "character": 4}
      },
      "text": ";"
    }
  ]
}
```

**Implementation**:
1. Identify changed region (line 10, char 4)
2. Re-parse only affected subtree
3. Update AST incrementally
4. Re-run semantic analysis on modified scope

**Speedup**: 10-100x faster than full re-parse

---

### Lazy Symbol Resolution

**Problem**: Type-checking all files on project open is slow

**Solution**: Lazy loading

**Strategy**:
1. Parse all files (fast, just tokenize + AST)
2. Build symbol table (function signatures, types)
3. **Defer** full semantic analysis until file is opened
4. Cache results for opened files

**Example**:
- Project has 100 files
- User opens `main.aria`
- LSP parses all 100 files (symbol table only)
- Full analysis only on `main.aria` + its imports
- Result: Instant startup, accurate diagnostics for open files

---

### Parallel Analysis

**Multi-threaded Processing**:
- Thread 1: Handle LSP requests
- Thread 2: Parse modified files
- Thread 3: Semantic analysis
- Thread 4: Background indexing (for find references)

**Work Queue**:
```
User types → Queue parse request → Background thread processes → Send diagnostics
```

---

## Integration Points

### With Aria Compiler

**Shared Library**: `libaria_frontend.a`

**Consistency**: LSP uses same lexer/parser/semantic analyzer as compiler

**Compiler Flags**: LSP respects `.arialsrc` config for warnings, optimizations

---

### With AriaBuild

**Project Context**:
- LSP reads `build.abc` to understand project structure
- Resolves imports based on build dependencies
- Includes package paths from AriaX

**Example**:
```json
// build.abc
{
  "dependencies": {
    "std.collections": "1.0.0"
  }
}
```

**LSP includes**: `/usr/local/aria/packages/std.collections-1.0.0/include/`

---

### With AriaX Package Manager

**Package Completion**:
```aria
use std.█
```

**LSP queries AriaX**:
```bash
ariax list --installed
```

**Completion**:
```
std.collections
std.io
std.mem
```

---

### With VSCode Extension

**Installation**:
```bash
code --install-extension aria-lang.aria-vscode
```

**Extension activates aria-lsp**:
```json
{
  "languageServer": {
    "command": "aria-lsp",
    "args": ["--stdio"]
  }
}
```

**Features**:
- Syntax highlighting
- IntelliSense (completion)
- Error squiggles
- Go to definition (F12)
- Find all references (Shift+F12)
- Hover info (mouse hover)
- Formatting (Shift+Alt+F)

---

## Configuration

### .arialsrc (LSP Config)

```json
{
  "lsp": {
    "diagnostics": {
      "tbb_overflow": "error",
      "unchecked_result": "warning",
      "unused_variable": "info"
    },
    "completion": {
      "auto_import": true,
      "snippets": true
    },
    "formatting": {
      "indentation": "spaces",
      "indent_size": 4,
      "max_line_length": 100
    }
  }
}
```

---

## Related Components

- **[Aria Compiler](ARIA_COMPILER.md)**: Shares frontend (lexer, parser, semantic)
- **[aria-dap](ARIA_DAP.md)**: Debug adapter (complementary to LSP)
- **[AriaBuild](ARIABUILD.md)**: Provides project context
- **[AriaX](ARIAX.md)**: Package resolution for imports

---

**Document Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Design specification (implementation pending)
