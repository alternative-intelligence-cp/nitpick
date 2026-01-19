# AriaSH - Aria Interactive Shell

**Version 0.1.0** - A modern, interactive shell combining Aria language execution with UNIX process orchestration.

## Overview

AriaSH is an interactive REPL (Read-Eval-Print Loop) that bridges the Aria programming language with traditional shell capabilities. It features a sophisticated modal input system, variable storage with expression evaluation, and clean error handling.

## Features

### 🎯 Core Capabilities

- **Aria Language Execution**: Full support for Aria's type system and expressions
- **Variable Declarations**: Create and manage typed variables (`int8`, `int16`, `int32`, `int64`, etc.)
- **Expression Evaluation**: Interactive arithmetic and logic evaluation with immediate feedback
- **Process Orchestration**: Execute shell commands and pipelines (single commands supported)
- **Modal Input System**: Vim-inspired dual-mode editing for flexible input handling

### 🎨 User Experience

- **Clean Output**: No debug spam, just results
- **Error Recovery**: Parse errors display helpful messages without crashing
- **History Navigation**: Arrow key support for command history
- **Multi-line Editing**: Compose complex statements across multiple lines
- **Visual Feedback**: Mode indicators show current input mode

## Modal Input System

AriaSH features a dual-mode input system inspired by Vim:

### 🟢 RUN Mode (Default)
- **Enter** submits immediately
- Perfect for quick commands and single-line statements
- Indicated by `[RUN] aria>` prompt

### 🔵 EDIT Mode
- **End with `;;`** then press Enter to submit
- Multi-line editing for complex code blocks
- **Enter** adds newlines within your code
- Indicated by `[EDIT] aria>` prompt

### Mode Switching
- Press **ESC** to toggle between modes
- Visual feedback shows current mode in prompt

## Quick Start

### Building

```bash
mkdir build && cd build
cmake ..
make ariash
```

### Running

```bash
./build/ariash
```

### Basic Usage

**RUN Mode (single-line):**
```aria
[RUN] aria> int8 x = 5;
[RUN] aria> x
=> 5
```

**EDIT Mode (multi-line):**
```aria
[EDIT] aria> int8 x = 15;
[EDIT] ... int8 y = 15;
[EDIT] ... int8 sum = x + y;;

[EDIT] aria> sum;;
=> 30
```

### Built-in Commands

- **help** - Display help information
- **clear** - Clear screen and redisplay banner
- **exit** / **quit** - Exit the shell (works in both modes with or without `;`)

## Language Features

### Type System

Supported integer types:
- `int8`, `int16`, `int32`, `int64` - Signed integers
- `tbb8`, `tbb16`, `tbb32`, `tbb64` - Two's complement balanced binary
- `string` - String type
- `buffer` - Binary buffer type
- `bool` - Boolean type

### Operators

**Arithmetic**: `+`, `-`, `*`, `/`  
**Comparison**: `==`, `!=`, `<`, `>`, `<=`, `>=`  
**Logical**: `&&`, `||`, `!`  
**Assignment**: `=`

### Examples

**Variable Declaration:**
```aria
int8 temperature = 72;
int16 altitude = 5280;
string city = "Denver";
```

**Expressions:**
```aria
int8 a = 10;
int8 b = 20;
a + b * 2
=> 50
```

**Control Flow (planned):**
```aria
if (x > 10) {
    print("Greater than 10");
}
```

## Architecture

### Components

**Lexer** (`src/parser/lexer.cpp`)
- Tokenizes input with keyword recognition
- Handles operators, literals, and identifiers
- Critical fix: Ensures EOF token added to prevent infinite loops

**Parser** (`src/parser/parser.cpp`)
- Recursive descent parser with precedence climbing
- Expression statement support for REPL
- Robust error recovery with synchronization
- Null-safe AST construction

**Executor** (`src/executor/executor.cpp`)
- AST visitor pattern for evaluation
- Environment-based variable storage
- Expression evaluation with result display
- Single-command process execution

**Input Engine** (`src/repl/input_engine.cpp`)
- Modal state machine (RUN/EDIT)
- Double-semicolon submit pattern
- Raw terminal mode with proper cleanup
- Edit buffer with multi-line support

**Terminal** (`src/repl/terminal.cpp`)
- Cross-platform terminal I/O
- Raw mode for capturing control keys
- ANSI escape sequence support
- Output post-processing for proper formatting

### AST Nodes

**Statements:**
- `VarDeclStmt` - Variable declarations with optional initializer
- `AssignStmt` - Variable assignment
- `ExprStmt` - Expression evaluation (REPL display)
- `IfStmt`, `WhileStmt`, `ForStmt` - Control flow
- `BlockStmt` - Statement grouping
- `CommandStmt`, `PipelineStmt` - Process execution

**Expressions:**
- `IntegerLiteral`, `StringLiteral` - Literal values
- `VariableExpr` - Variable references
- `BinaryOpExpr`, `UnaryOpExpr` - Operations
- `CallExpr` - Function calls

## Critical Bug Fixes (Session 4)

### Parser Infinite Loop (RESOLVED)
**Issue**: Parser hung indefinitely on variable declarations like `int8 x = 5;`

**Root Cause**: Lexer's `tokenize()` function broke the loop when encountering EOF without adding the EOF token to the vector. This caused `peek()` to return the last token (semicolon) indefinitely, creating an infinite loop in `parseProgram()`.

**Fix**: Modified `tokenize()` to add EOF token before breaking, with fallback to ensure EOF is always present:
```cpp
while (!isAtEnd()) {
    Token token = nextToken();
    tokens.push_back(token);
    if (token.type == TokenType::END_OF_FILE) break;
}
// Ensure EOF always present
if (tokens.empty() || tokens.back().type != TokenType::END_OF_FILE) {
    tokens.push_back(makeToken(TokenType::END_OF_FILE, ""));
}
```

### Segmentation Fault on Parse Errors (RESOLVED)
**Issue**: Invalid syntax caused segfault after displaying parse error.

**Root Cause**: Attempting to execute empty or null AST after parse errors.

**Fix**: Added null-safe guards:
```cpp
auto stmt = parseStatement();
if (stmt) {  // Only add non-null statements
    program->statements.push_back(std::move(stmt));
}

// Execute only if we have statements
if (ast && !ast->statements.empty()) {
    executor::Executor exec(globalEnv);
    exec.execute(*ast);
}
```

### Built-in Commands in EDIT Mode (RESOLVED)
**Issue**: Commands like `exit;;` didn't work in EDIT mode.

**Root Cause**: The `;;` pattern remover left a trailing semicolon, so `exit;;` became `exit;` which didn't match the exact string `"exit"`.

**Fix**: Strip trailing semicolons before command matching:
```cpp
while (!trimmed.empty() && trimmed.back() == ';') {
    trimmed.pop_back();
}
```

## Project Structure

```
aria_shell/
├── inc/                  # Header files
│   ├── executor/         # AST execution engine
│   ├── parser/           # Lexer, parser, AST definitions
│   └── repl/             # Interactive shell components
├── src/                  # Implementation files
│   ├── executor/         # Executor and environment
│   ├── parser/           # Lexer and parser logic
│   ├── platform/         # Platform-specific terminal code
│   └── repl/             # Input engine and main loop
├── tests/                # Unit tests
└── build/                # Build output (generated)
```

## Development Status

### ✅ Completed
- [x] Lexer with keyword recognition
- [x] Parser with expression statements
- [x] Variable declarations and assignments
- [x] Expression evaluation with arithmetic
- [x] Modal input system (RUN/EDIT modes)
- [x] Double-semicolon submit pattern
- [x] Built-in commands (help, clear, exit)
- [x] Clean error handling without crashes
- [x] Multi-line editing support
- [x] Result display for REPL

### 🚧 In Progress
- [ ] Control flow execution (if/while/for)
- [ ] Function definitions and calls
- [ ] Multi-command pipelines with FD chaining
- [ ] Process execution with proper I/O handling
- [ ] Command history persistence

### 📋 Planned
- [ ] Tab completion
- [ ] Syntax highlighting
- [ ] Redirection operators (>, <, >>)
- [ ] Background job control (&)
- [ ] Script file execution
- [ ] Debugger integration

## Testing

Run the test suite:
```bash
cd build
make test
./tests/test_lexer
./tests/test_parser
./tests/test_executor
```

## Known Limitations

1. **Process Execution**: Multi-command pipelines not yet implemented (FD chaining required)
2. **Control Flow**: If/while/for statements parse but execution is incomplete
3. **Functions**: Function definitions and calls not yet fully implemented
4. **Token Types**: `int8` and other type keywords currently show as `UNKNOWN` in debug (cosmetic issue, doesn't affect functionality)

## Contributing

AriaSH is part of the Alternative Intelligence Liberation Platform (AILP) research initiative. Development focuses on safety-critical systems and deterministic execution models.

## License

Part of the Aria Language Project - AILP Research Initiative

## Acknowledgments

Built as part of the Aria compiler and runtime system research, incorporating lessons from:
- Brane Gas Cosmology
- Signal Processing Theory
- Discrete Geometry
- The 179-Degree Asymmetric Toroidal Phase Model (ATPM)

**Recent Achievement**: ATPM paper published on ai.viXra.org (2601.0052v1) - January 14, 2026

---

**Version**: 0.1.0  
**Status**: Alpha - Core REPL functional, process execution in development  
**Last Updated**: January 15, 2026

