# Aria Compiler (ariac)

**Component Type**: Compiler/Transpiler  
**Language**: C++20  
**Backend**: LLVM 20  
**Repository**: `/home/randy/._____RANDY_____/REPOS/aria`  
**Version**: v0.1.0-dev  
**Status**: Active Development (70 commits ahead of last tag)

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Pipeline Stages](#pipeline-stages)
4. [Type System](#type-system)
5. [IR Generation](#ir-generation)
6. [Runtime Integration](#runtime-integration)
7. [Development Status](#development-status)
8. [Known Issues](#known-issues)

---

## Overview

The Aria Compiler (ariac) is the reference implementation compiler for the Aria programming language. It compiles `.aria` source files into native machine code by:

1. **Parsing** Aria syntax into an Abstract Syntax Tree (AST)
2. **Type checking** with semantic analysis
3. **Generating** LLVM Intermediate Representation (IR)
4. **Optimizing** via LLVM optimization passes
5. **Emitting** native object code
6. **Linking** with `libaria_runtime.a` to produce executables

### Key Features

- **LLVM-based backend**: Leverages industry-standard compiler infrastructure
- **Type-Based Bounded (TBB) integers**: Verified range checking at compile time
- **6-stream I/O integration**: Compiler knows about FDs 0-5 semantic roles
- **Result<T> monadic errors**: First-class compile-time support for error handling
- **Zero-cost abstractions**: High-level features compile to efficient machine code
- **Wild memory model**: Explicit control over heap allocations

---

## Architecture

### Compiler Components

```
┌─────────────────────────────────────────────────────────────┐
│                     ARIA COMPILER                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌───────────┐   ┌────────────┐   ┌──────────────┐       │
│  │  Lexer    │──▶│  Parser    │──▶│ AST Builder  │       │
│  └───────────┘   └────────────┘   └──────────────┘       │
│       ▲                                    │               │
│       │                                    ▼               │
│  ┌───────────────────────────────────────────────┐        │
│  │         Semantic Analyzer                     │        │
│  │  - Type checking                              │        │
│  │  - TBB range verification                     │        │
│  │  - Symbol resolution                          │        │
│  │  - Error detection                            │        │
│  └───────────────────────────────────────────────┘        │
│                     │                                      │
│                     ▼                                      │
│  ┌───────────────────────────────────────────────┐        │
│  │         IR Generator (LLVM)                   │        │
│  │  - Convert AST → LLVM IR                      │        │
│  │  - Builtin function lowering                  │        │
│  │  - Runtime call insertion                     │        │
│  │  - Result<T> monadic transforms               │        │
│  └───────────────────────────────────────────────┘        │
│                     │                                      │
│                     ▼                                      │
│  ┌───────────────────────────────────────────────┐        │
│  │         LLVM Optimizer                        │        │
│  │  - Dead code elimination                      │        │
│  │  - Constant propagation                       │        │
│  │  - Function inlining                          │        │
│  │  - Loop optimizations                         │        │
│  └───────────────────────────────────────────────┘        │
│                     │                                      │
│                     ▼                                      │
│  ┌───────────────────────────────────────────────┐        │
│  │         Code Emitter                          │        │
│  │  - LLVM IR → Machine code                     │        │
│  │  - Target: x86_64, ARM64                      │        │
│  │  - Output: ELF/PE object files                │        │
│  └───────────────────────────────────────────────┘        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
                  ┌──────────────────┐
                  │   Object File    │
                  │   (.o / .obj)    │
                  └──────────────────┘
                            │
                            ▼
                  ┌──────────────────┐
                  │     Linker       │◀── libaria_runtime.a
                  └──────────────────┘
                            │
                            ▼
                  ┌──────────────────┐
                  │   Executable     │
                  └──────────────────┘
```

---

## Pipeline Stages

### 1. Lexical Analysis (Lexer)

**Purpose**: Break source code into tokens

**Input**: Raw UTF-8 source text  
**Output**: `vector<Token>`

**Token Types**:
- **Keywords**: `func`, `pass`, `result`, `use`, `tbb8`, `wild`, etc.
- **Identifiers**: Function names, variable names
- **Literals**: Numbers, strings, booleans
- **Operators**: `+`, `-`, `*`, `/`, `=`, `==`, `<`, `>`, etc.
- **Delimiters**: `(`, `)`, `{`, `}`, `;`, `:`, etc.

**Example**:
```aria
func:main = int64() { pass(0); }
```
Tokens:
```
KEYWORD(func), COLON, IDENTIFIER(main), EQUALS, TYPE(int64),
LPAREN, RPAREN, LBRACE, KEYWORD(pass), LPAREN, NUMBER(0),
RPAREN, SEMICOLON, RBRACE
```

---

### 2. Syntax Analysis (Parser)

**Purpose**: Build Abstract Syntax Tree (AST) from tokens

**Input**: `vector<Token>`  
**Output**: `unique_ptr<ASTNode>` (root of AST)

**Grammar**: Recursive descent parser (likely, based on C++ implementation)

**AST Node Types**:
- `FunctionDecl`: Function declarations
- `VarDecl`: Variable declarations with type annotations
- `ExprStmt`: Expression statements
- `IfStmt`: Conditional branches
- `WhileStmt`: Loops
- `BinaryExpr`: `a + b`, `x == y`, etc.
- `CallExpr`: Function calls
- `Literal`: Constant values

**Error Recovery**: Likely synchronizes on statement boundaries (`;`, `}`)

---

### 3. Semantic Analysis

**Purpose**: Type checking, symbol resolution, error detection

**Input**: AST  
**Output**: Typed AST + `SymbolTable`

**Responsibilities**:

1. **Symbol Table Construction**
   - Track all declared variables, functions, types
   - Enforce scoping rules (block-level scopes)

2. **Type Checking**
   - Infer types where possible
   - Verify type compatibility in assignments, operations
   - Check function signatures match calls

3. **TBB Range Verification**
   - Analyze `tbb8:x = 5;` → verify `5 ∈ [-128, 127]`
   - Constant propagation for compile-time range checks
   - Warn on potential overflow in expressions

4. **Result<T> Validation**
   - Ensure `result<T>` values are checked before use
   - Enforce `.is_err()` or `.is_ok()` checks before `.ok()`/`.err()` access
   - Optionally warn on ignored results

5. **Error Reporting**
   - Undefined variables
   - Type mismatches
   - Invalid operations
   - TBB overflow warnings

**Example Error**:
```aria
tbb8:x = 200;  // ERROR: 200 out of range for tbb8 [-128, 127]
```

---

### 4. IR Generation (LLVM)

**Purpose**: Translate typed AST into LLVM Intermediate Representation

**Input**: Typed AST  
**Output**: `llvm::Module*`

**Key Transformations**:

1. **Builtin Function Lowering**
   - Aria builtins → Runtime function calls
   - Example: `io.stdout.write(...)` → `@aria_stdout_write(...)`

2. **Memory Allocator Calls**
   - `wild byte*:buf = alloc(1024);` → `@aria_alloc(1024)`
   - Recently fixed: 19 allocator builtins now properly mapped

3. **TBB Type Mapping**
   - `tbb8` → `i8` with range metadata
   - Overflow checks inserted at runtime or compile-time constant folding

4. **Result<T> Monadic Transforms**
   - `result<T>` → `%AriaResult*` (pointer to struct)
   - `.is_err()` → null check on `err` field
   - `.ok()` → extract `val` field with type cast

5. **6-Stream I/O Calls**
   - Compiler knows about `stdin`, `stdout`, `stderr`, `stddbg`, `stddati`, `stddato`
   - Maps to FDs 0-5 at runtime

**LLVM IR Example**:
```llvm
define i64 @main() {
entry:
  %result = call %AriaResult* @aria_alloc(i64 1024)
  %is_err = icmp ne i8* %result.err, null
  br i1 %is_err, label %error_handler, label %success

success:
  %ptr = extractvalue %AriaResult* %result, 1  ; val field
  ; ... use ptr ...
  ret i64 0

error_handler:
  %err_msg = extractvalue %AriaResult* %result, 0  ; err field
  call void @aria_stderr_write(i8* %err_msg, i64 ...)
  ret i64 1
}
```

---

### 5. LLVM Optimization

**Purpose**: Apply standard compiler optimizations

**Optimization Passes** (likely includes):
- **Dead Code Elimination (DCE)**: Remove unused variables, unreachable code
- **Constant Propagation**: Replace `x = 5; y = x + 3;` with `y = 8;`
- **Function Inlining**: Inline small functions to reduce call overhead
- **Loop Optimizations**: Unrolling, invariant code motion
- **Register Allocation**: Efficient use of CPU registers

**Example**:
```aria
func:compute = int64() {
    int64:x = 5;
    int64:y = x + 3;  // Constant propagation → y = 8
    pass(y);
}
```
Optimized IR:
```llvm
define i64 @compute() {
  ret i64 8  ; Fully optimized to constant
}
```

---

### 6. Code Emission

**Purpose**: Generate native machine code from optimized LLVM IR

**Supported Targets**:
- **x86_64** (Linux, Windows, macOS)
- **ARM64** (likely future support for Apple Silicon, ARM servers)

**Output Formats**:
- **Linux**: ELF object files (`.o`)
- **Windows**: COFF object files (`.obj`)
- **macOS**: Mach-O object files

**Calling Convention**: Platform-specific (SysV on Linux, Microsoft x64 on Windows)

---

### 7. Linking

**Purpose**: Combine object files with runtime library into executable

**Link Inputs**:
- `main.o` (compiled user code)
- `/usr/local/lib/libaria_runtime.a` (static library)
- System libraries (libc, libm, etc.)

**Link Command** (typical):
```bash
clang main.o /usr/local/lib/libaria_runtime.a -o my_app
```

**Symbol Resolution**:
- User code references: `aria_alloc`, `aria_stdout_write`, etc.
- Runtime library provides implementations
- All symbols must resolve (undefined → link error)

**Output**: ELF/PE executable ready to run

---

## Type System

### Primitive Types

| Aria Type | LLVM Type | Size | Range | Description |
|-----------|-----------|------|-------|-------------|
| `int8` | `i8` | 1 byte | -128 to 127 | Signed 8-bit |
| `int16` | `i16` | 2 bytes | -32,768 to 32,767 | Signed 16-bit |
| `int32` | `i32` | 4 bytes | -2^31 to 2^31-1 | Signed 32-bit |
| `int64` | `i64` | 8 bytes | -2^63 to 2^63-1 | Signed 64-bit |
| `uint8` | `i8` | 1 byte | 0 to 255 | Unsigned 8-bit |
| `uint16` | `i16` | 2 bytes | 0 to 65,535 | Unsigned 16-bit |
| `uint32` | `i32` | 4 bytes | 0 to 2^32-1 | Unsigned 32-bit |
| `uint64` | `i64` | 8 bytes | 0 to 2^64-1 | Unsigned 64-bit |
| `float32` | `float` | 4 bytes | IEEE 754 | 32-bit float |
| `float64` | `double` | 8 bytes | IEEE 754 | 64-bit float |
| `bool` | `i1` | 1 bit | true/false | Boolean |
| `byte` | `i8` | 1 byte | 0 to 255 | Raw byte |
| `string` | `i8*` | ptr | UTF-8 | String (likely ptr + length) |

### Type-Based Bounded (TBB) Integers

**Purpose**: Bounded integers with compile-time range verification

**Syntax**: `tbb<N>` where N is the max value

**Examples**:
- `tbb8:age = 25;` → Range [-128, 127]
- `tbb16:port = 8080;` → Range [-32768, 32767]

**Compile-Time Checks**:
```aria
tbb8:x = 200;  // ERROR: 200 > 127
```

**Runtime Overflow**:
- Returns ERR sentinel (-128 for tbb8, -32768 for tbb16, etc.)
- Sticky error propagation (ERR + anything = ERR)

**LLVM Mapping**: `tbb8` → `i8` with metadata/range annotations

---

### Result<T> Type

**Purpose**: Monadic error handling without exceptions

**Structure**:
```c
struct AriaResult {
    char* err;      // Error message (NULL if success)
    void* val;      // Success value (NULL if error)
    size_t val_size; // Size of value (for copying)
};
```

**Usage**:
```aria
result<string>:content = io.read_file("file.txt");
if (content.is_err()) {
    io.stderr.write("Error: " + content.err());
    pass(1);
}
io.stdout.write(content.ok());
```

**Compiler Enforcement** (possible future feature):
- Warn on unchecked results
- Require explicit `.is_err()` check before `.ok()` access

---

## IR Generation

### Builtin Functions

The compiler recognizes builtin functions and lowers them to runtime calls.

**Recent Fix** (December 2025):
- **Issue**: Allocator builtins were silent (IR generator didn't know about them)
- **Fix**: Added 19 allocator builtins to `src/backend/ir_generator.cpp`

**Allocator Builtins** (now working):
```
arena_new, arena_alloc, arena_reset, arena_free,
pool_new, pool_alloc, pool_free,
slab_new, slab_alloc, slab_free,
wild_alloc, wild_realloc, wild_free,
gc_alloc, gc_collect, gc_enable, gc_disable,
ref_new, ref_count
```

**I/O Builtins**:
```
aria_stdin_read, aria_stdout_write, aria_stderr_write,
aria_stddbg_write, aria_stddati_read, aria_stddato_write,
aria_read_file, aria_write_file
```

**Process Builtins**:
```
aria_spawn, aria_wait, aria_kill, aria_exit
```

---

### Calling Convention

**Platform-Specific**:

**Linux (SysV AMD64 ABI)**:
- Integer args: `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`
- Float args: `xmm0`-`xmm7`
- Return: `rax` (integer), `xmm0` (float)
- Stack alignment: 16 bytes

**Windows (Microsoft x64)**:
- Integer args: `rcx`, `rdx`, `r8`, `r9`
- Float args: `xmm0`-`xmm3`
- Return: `rax` (integer), `xmm0` (float)
- Stack alignment: 16 bytes
- Shadow space: 32 bytes on stack

**Compiler Handles**:
- LLVM automatically generates platform-correct calling sequences
- User code is platform-agnostic

---

## Runtime Integration

### Linking with libaria_runtime.a

**Automatic Linking**:
- Compiler always links with `libaria_runtime.a`
- No explicit linker flags needed (handled by compiler driver)

**Symbol Dependencies**:
- User code calls builtins → undefined symbols
- Runtime library provides implementations
- Linker resolves all symbols at link time

**Runtime Initialization**:
- `__attribute__((constructor))` runs before `main()`
- Sets up 6-stream I/O handles
- Initializes allocators
- Prepares async runtime

**Runtime Shutdown**:
- `__attribute__((destructor))` runs after `main()` returns
- Flushes all streams
- Frees allocated memory
- Shuts down GC (if enabled)

---

## Development Status

**Current State**: Active development, v0.1.0-dev

**Recent Work**:
- ✅ Fixed allocator builtin IR generation (December 2025)
- ✅ 6-stream I/O integration
- ✅ Result<T> basic support
- ✅ TBB type checking (partial)

**In Progress**:
- TBB compile-time constant overflow detection
- Result<T> mandatory checking enforcement
- LSP integration (aria-lsp)
- DAP integration (aria-dap)

**Future Plans**:
- Custom formatters for debugger (Result<T>, TBB pretty-printing)
- Package system integration (importing from AriaX)
- Incremental compilation
- Better error messages (colored, with source context)

---

## Known Issues

### 1. Allocator Builtins Missing from IR Generator (FIXED)

**Status**: ✅ RESOLVED (December 2025)

**Problem**: Calls to allocator builtins (arena_new, pool_alloc, etc.) were not generating LLVM IR.

**Fix**: Added all 19 allocator builtins to `src/backend/ir_generator.cpp`

**Uncommitted**: Changes are in working tree, not yet committed to git

---

### 2. TBB Compile-Time Overflow Detection Incomplete

**Status**: ⏳ IN PROGRESS

**Problem**: Not all constant expressions with TBB types are checked at compile-time.

**Example**:
```aria
tbb8:x = 100;
tbb8:y = 100;
tbb8:z = x + y;  // Should warn: 100 + 100 = 200, overflow!
```

**Current**: Checked if both operands are literals, but not for variables with known constant values.

**Planned**: Enhanced constant propagation to track TBB ranges through expressions.

---

### 3. Result<T> Unchecked Access

**Status**: 🔴 OPEN

**Problem**: Code can call `.ok()` without checking `.is_err()` first, leading to undefined behavior.

**Example**:
```aria
result<string>:content = io.read_file("missing.txt");
io.stdout.write(content.ok());  // BUG: No check, will crash if error
```

**Current**: No compiler enforcement

**Planned**: Add lint warning or error for unchecked result access

---

### 4. Windows 6-Stream I/O Bootstrap

**Status**: 🔴 NOT IMPLEMENTED

**Problem**: Windows doesn't support FDs 3-5 natively. Shell needs custom bootstrap protocol.

**Solution**: aria_shell passes FD handles via command-line args or environment variables. Child process parses and opens handles.

**Status**: Design complete (see aria_shell research), not yet implemented

---

## Related Components

- **[Aria Runtime](ARIA_RUNTIME.md)**: Runtime library linked with compiled code
- **[AriaBuild](ARIABUILD.md)**: Build system that invokes compiler
- **[aria-lsp](ARIA_LSP.md)**: Language server for IDE integration
- **[aria-dap](ARIA_DAP.md)**: Debug adapter for debugger integration

---

**Document Version**: 1.0  
**Last Updated**: December 22, 2025
