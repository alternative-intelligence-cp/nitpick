# Aria Preprocessor Test Results

## Integration Tests

### Basic Preprocessing (`-E` flag)
✅ **test_preprocessor.aria**
```bash
./build/ariac -E examples/test_preprocessor.aria
```
- Constants: `%define VERSION "0.1.0"`
- Conditionals: `%ifdef DEBUG`
- Macros: `%macro PRINT_MSG 1`
- Repetition: `%rep 3`

### File Inclusion (`-I` flag)
✅ **test_cli_flags.aria with include_test.aria**
```bash
./build/ariac -E -I examples examples/test_cli_flags.aria
```
- Include search paths work
- Macros from included files available
- Constants from included files available

### Command-line Defines (`-D` flag)
✅ **Conditional compilation with -D**
```bash
# Production mode
./build/ariac -E -I examples -D PRODUCTION examples/test_cli_flags.aria
# Development mode (no -D flag)
./build/ariac -E -I examples examples/test_cli_flags.aria
```
- Command-line defines override file definitions
- Conditionals respond to -D flags correctly

## Unit Tests
✅ **All 26 assertions passing**
```bash
./build/test_preprocessor
```

### Test Coverage
1. **Basic Defines** (4 tests)
   - Simple constant substitution
   - Multiple defines
   - Undef works
   - Redefine protection

2. **Conditionals** (6 tests)
   - ifdef/ifndef/else/endif
   - Nested conditionals
   - Multiple conditional blocks
   - Unresolved symbol handling

3. **Macros** (5 tests)
   - Simple parameter substitution
   - Multiple parameters
   - Macro calls in code
   - Nested calls
   - Recursion detection

4. **Repetition** (3 tests)
   - Basic %rep loops
   - Nested %rep (2x3)
   - Zero repetitions

5. **File Inclusion** (3 tests)
   - Simple includes
   - Nested includes
   - Circular include protection

6. **Expression Evaluation** (6 tests)
   - Arithmetic (+, -, *, /, %)
   - Comparisons (>, <, ==, !=)
   - Logical operators (&&, ||, !)
   - Parentheses & order of operations
   - Unary operators (!, -)
   - Complex expressions

7. **Nested Macro Expansion** (4 tests)
   - Macro calling macro
   - Multi-level nesting
   - Arguments in nested calls
   - Direct recursion prevention

## Known Behaviors

### Quote Handling in Macros
When macro arguments contain quotes, they are preserved literally:
```aria
%macro LOG 1
print("[LOG] %1")
%endmacro

LOG("message")  →  print("[LOG] "message"")
```
This is NASM-compatible behavior. Arguments are substituted exactly as written.

### Context-Local Labels
The preprocessor preserves `%$label` syntax for context-local labels, which will be handled by the assembler/compiler later in the pipeline.

## Pipeline Integration

### Compiler Phases (7 total)
1. **Preprocessing** ← NEW!
   - Directive processing
   - Macro expansion
   - File inclusion
   - Conditional compilation
2. Lexing
3. Parsing
4. Semantic Analysis (Type checking)
5. Semantic Analysis (Symbol resolution)
6. Semantic Analysis (Type inference)
7. Code Generation

### CLI Flags
- `-E` : Preprocess only (output to stdout)
- `-I <path>` : Add include search path
- `-D <name>[=<value>]` : Define constant (default value = 1)
- `-v` : Verbose output

## Bug Fixes Applied

### 1. String Literal Protection
**Problem**: Macro names inside string literals were being expanded.
```aria
%macro LOG 1
print("[LOG] %1")
%endmacro
```
When expanding `LOG("msg")`, the preprocessor would see "LOG" inside the string "[LOG]" and try to expand it again, causing false recursion detection.

**Fix**: Added `in_quotes` state tracking to main loop. Macro expansion now skipped when inside string literals.

### 2. Conditional Stack Isolation
**Problem**: Recursive macro processing shared conditional stack with parent context.
```aria
%ifdef PRODUCTION   ← Stack: [PRODUCTION=active]
LOG("msg")          ← Recursive call inherits conditional stack
%endif              ← Expected by parent, but child might have popped it
```

**Fix**: Save and restore `conditional_stack` around recursive `process()` calls. Each macro expansion gets clean conditional state.

## Test File Structure
```
examples/
  ├── test_preprocessor.aria    # Basic feature demo
  ├── include_test.aria         # Common definitions for inclusion
  ├── test_cli_flags.aria       # Tests -I and -D flags
  └── PREPROCESSOR_TESTS.md     # This file
```

## Validation Status
- ✅ All 26 unit tests passing
- ✅ Integration tests working
- ✅ CLI flags functional
- ✅ String literal protection working
- ✅ Conditional stack isolation working
- ✅ Nested macro expansion working
- ✅ Recursion detection working
- ✅ File inclusion working
- ✅ Circular include protection working

**Status: PREPROCESSOR COMPLETE AND INTEGRATED** 🎉
