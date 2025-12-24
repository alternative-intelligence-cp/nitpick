# Aria Compiler Architecture

**Purpose**: The Aria language compiler - transforms Aria source code into executable binaries via LLVM.

---

## Philosophy

### Design Principles

1. **Errors Are Values** - No exceptions. Result<T,E> everywhere.
2. **Explicit Over Implicit** - No hidden behavior, no magic.
3. **Memory Safety Without Runtime Cost** - Compile-time borrow checking.
4. **Performance Matters** - Zero-cost abstractions, direct LLVM integration.
5. **Balanced Ternary Computing** - Native support for -1/0/+1 computation via TBB types.

### Non-Negotiables

- **Spec Compliance** - aria_ecosystem/specs/ documents are law
- **No Simplification** - Difficulty is not a reason to deviate
- **No Bloat** - Every feature must serve a clear purpose
- **Test Coverage** - Features without tests don't merge
- **LLVM Integration** - No custom backends, LLVM is the target

---

## High-Level Architecture

```
Aria Source Code (.aria)
    ↓
[Preprocessor] - Handles #include, conditional compilation
    ↓
[Lexer] - Tokenization (src/frontend/lexer/)
    ↓
[Parser] - AST construction (src/frontend/parser/)
    ↓
[Semantic Analysis] - Type checking, borrow checking (src/frontend/sema/)
    ├→ Type Checker
    ├→ Borrow Checker
    ├→ Exhaustiveness Analyzer
    ├→ Symbol Table
    └→ Module Resolver
    ↓
[IR Generation] - LLVM IR emission (src/backend/ir/)
    ↓
[LLVM Optimization Passes]
    ↓
[Code Generation] - Platform-specific assembly
    ↓
[Linker] - Final executable
```

---

## Component Breakdown

### Frontend (src/frontend/)

#### Lexer (lexer/)
- **Purpose**: Convert source text into tokens
- **Key Files**:
  - `lexer.cpp` - Main tokenization logic
  - `token.cpp` - Token definitions
- **Responsibilities**:
  - UTF-8 handling
  - Keyword recognition
  - Number literal parsing (including TBB values)
  - String literal processing (including template literals)
  - Position tracking for error messages

#### Parser (parser/)
- **Purpose**: Build Abstract Syntax Tree (AST) from tokens
- **Key Files**:
  - `parser.cpp` - Main parsing logic (~3000 lines)
  - Statement parsing (pick, when, defer, etc.)
  - Expression parsing (precedence climbing)
- **Responsibilities**:
  - Syntax validation
  - AST node construction
  - Error recovery
  - Operator precedence
  - Pattern matching for pick statements

#### AST (ast/)
- **Purpose**: Data structures representing program structure
- **Key Files**:
  - `ast_node.h` - Base AST node class
  - `expr.h` - Expression nodes
  - `stmt.h` - Statement nodes
  - `type.h` - Type nodes
- **Node Types**:
  - Literals, identifiers, binary/unary ops
  - Function calls, array indexing, member access
  - Variable declarations, function declarations
  - Control flow (when, pick, defer)
  - Pick patterns (ranges, wildcards, ERR)

#### Semantic Analysis (sema/)
- **Purpose**: Validate program correctness, infer types, check ownership

##### Type Checker (type_checker.cpp)
- Type inference for expressions
- Type compatibility validation
- Must-use result enforcement (nodiscard)
- Pick statement exhaustiveness checking
- Generic type resolution

##### Borrow Checker (borrow_checker.cpp)
- Ownership tracking (move semantics)
- Lifetime validation
- Pinning reference analysis
- Appendage Theory enforcement
- Use-after-move detection

##### Exhaustiveness Analyzer (exhaustiveness.cpp)
- **NEW** - Pattern coverage analysis for pick statements
- Type domain calculation (TBB ranges, bool values, enum variants)
- Interval arithmetic for gap finding
- ERR sentinel enforcement for TBB types

##### Symbol Table (symbol_table.cpp)
- Variable scoping
- Function signatures
- Type definitions
- Module imports

##### Module Resolver (module_resolver.cpp)
- Import path resolution
- Dependency graph construction
- Circular dependency detection

---

### Backend (src/backend/)

#### IR Generation (ir/)
- **Purpose**: Emit LLVM IR from validated AST
- **Key Files**:
  - `ir_generator.cpp` - Main IR emission
  - `codegen_expr.cpp` - Expression codegen
  - `codegen_stmt.cpp` - Statement codegen
  - `tbb_codegen.cpp` - TBB-specific optimizations
  - `ternary_codegen.cpp` - Ternary operator lowering

#### Code Generation Strategy
1. **Function-at-a-time** - Process each function independently
2. **SSA Form** - Static Single Assignment via LLVM
3. **Type Lowering** - Aria types → LLVM types
4. **Optimization** - Leverage LLVM's optimization passes

---

### Runtime (src/runtime/)

#### Memory Management
- **GC** - Garbage collector for default allocations
- **Arena** - Bump allocator for temporary data
- **Pool** - Fixed-size object pools
- **Slab** - Variable-size slab allocator
- **Wild** - Manual memory (opt-out of GC)

#### Streams (streams/)
- Six-stream I/O implementation
- FD 3-5 initialization (stddbg, stddati, stddato)
- Platform-specific stream handling

#### Async Runtime (async/)
- M:N threading scheduler (planned)
- Work-stealing queue (planned)
- Coroutine state management (planned)

---

## Dependencies

### External
- **LLVM 20.1.2** - IR generation and optimization
- **CMake 3.20+** - Build system
- **C++17** - Compiler implementation language

### Internal (Aria Ecosystem)
- **aria_lang_specs** - Language specification (source of truth)
- **aria_ecosystem** - Architecture documentation
- **AriaBuild** - Build system integration (planned)
- **AriaX** - Package management (planned)

---

## Build System

### CMakeLists.txt Structure
```cmake
# Phase 1: Lexer
LEX_SOURCES = lexer.cpp, token.cpp

# Phase 2: Parser  
PARSER_SOURCES = parser.cpp

# Phase 3: Semantic Analysis
SEMA_SOURCES = type_checker.cpp, borrow_checker.cpp, exhaustiveness.cpp, ...

# Phase 4: IR Generation
IR_SOURCES = ir_generator.cpp, codegen_expr.cpp, codegen_stmt.cpp, ...

# Phase 5: Runtime
RUNTIME_SOURCES = gc.cpp, streams.cpp, ...

# Main Executable
ariac = LEX + PARSER + SEMA + IR + RUNTIME + LLVM
```

### Build Commands
```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build compiler
cmake --build build --target ariac

# Run tests
cmake --build build --target test

# Install
cmake --build build --target install
```

---

## Code Organization

```
aria/
├── include/              # Public headers
│   ├── frontend/
│   │   ├── ast/         # AST node definitions
│   │   ├── lexer/       # Lexer interface
│   │   ├── parser/      # Parser interface
│   │   └── sema/        # Semantic analysis interfaces
│   ├── backend/
│   │   └── ir/          # IR generation interface
│   └── runtime/         # Runtime library headers
│
├── src/                 # Implementation files
│   ├── frontend/
│   │   ├── lexer/
│   │   ├── parser/
│   │   ├── ast/
│   │   └── sema/
│   ├── backend/
│   │   └── ir/
│   ├── runtime/
│   │   ├── gc/
│   │   ├── streams/
│   │   └── async/
│   └── main.cpp         # Compiler entry point
│
├── tests/               # Test suite
│   ├── unit/           # Unit tests (C++)
│   ├── integration/    # Integration tests (Aria code)
│   ├── safety/         # Safety feature tests
│   └── runtime/        # Runtime library tests
│
├── aria_ecosystem/      # Documentation (submodule)
│   ├── specs/          # Language specifications
│   └── MASTER_ROADMAP.md
│
└── docs/               # Generated documentation
```

---

## Data Flow Example

```aria
func:main = int8() {
    tbb8:result = 42;
    pick(result) {
        (0..127) { stddbg << "Valid\n"; },
        (ERR) { stddbg << "Error\n"; }
    }
    pass(0);
};
```

### Compilation Pipeline

1. **Lexer Output** (tokens):
   ```
   FUNC, COLON, IDENT(main), EQUAL, IDENT(int8), LPAREN, RPAREN, LBRACE,
   IDENT(tbb8), COLON, IDENT(result), EQUAL, NUMBER(42), SEMICOLON,
   PICK, LPAREN, IDENT(result), RPAREN, LBRACE, ...
   ```

2. **Parser Output** (AST):
   ```
   FuncDeclStmt
     ├─ name: "main"
     ├─ returnType: int8
     ├─ body: BlockStmt
     │   ├─ VarDeclStmt (tbb8:result = 42)
     │   └─ PickStmt
     │       ├─ selector: IdentifierExpr("result")
     │       └─ cases:
     │           ├─ PickCase (0..127)
     │           └─ PickCase (ERR)
   ```

3. **Type Checker**:
   - Infers `result` type as `tbb8`
   - Validates pick selector is tbb8
   - Calls ExhaustivenessAnalyzer
   - Verifies both 0..127 and ERR cover full tbb8 domain

4. **Borrow Checker**:
   - Tracks `result` ownership
   - No moves detected (value semantics for tbb8)
   - No lifetime issues

5. **IR Generation** (LLVM IR):
   ```llvm
   define i8 @main() {
   entry:
     %result = alloca i8
     store i8 42, i8* %result
     %0 = load i8, i8* %result
     %1 = icmp eq i8 %0, -128  ; ERR check
     br i1 %1, label %err_case, label %range_case
   
   range_case:
     call void @stddbg_print(i8* "Valid\n")
     br label %exit
   
   err_case:
     call void @stddbg_print(i8* "Error\n")
     br label %exit
   
   exit:
     ret i8 0
   }
   ```

6. **LLVM Optimization**:
   - Dead code elimination
   - Constant propagation (`%result` is always 42)
   - Branch elimination (never takes ERR path)

7. **Code Generation**:
   - Platform-specific assembly
   - Link against Aria runtime
   - Produce executable binary

---

## Key Algorithms

### Exhaustiveness Checking
1. Calculate type domain (min/max values, symbols)
2. Extract coverage from pick cases (ranges, literals, ERR)
3. Sort and merge overlapping intervals
4. Find gaps using sweep algorithm
5. Check ERR coverage if TBB type
6. Generate helpful error message

### Borrow Checking (Appendage Theory)
1. Assign scope depth to each variable
2. Track moves and pinning references
3. Validate lifetime rules (child cannot outlive parent)
4. Detect use-after-move
5. Verify drop order (children first, parents last)

### Type Inference
1. Bottom-up traversal of expression tree
2. Literal types from token (42 → int64, 3.14 → double)
3. Operator result types from operands
4. Function return types from declaration
5. Generic instantiation via unification

---

## Testing Strategy

### Unit Tests (C++)
- Test individual components (lexer, parser, etc.)
- Mock dependencies
- Fast feedback (~1 second)

### Integration Tests (Aria)
- Test actual Aria programs
- Verify end-to-end compilation
- Check runtime behavior

### Safety Tests
- Test error detection (must-use, exhaustiveness, borrow checking)
- Verify helpful error messages
- Ensure bugs are caught at compile-time

---

## Performance Characteristics

| Component | Complexity | Bottleneck |
|-----------|------------|------------|
| Lexer | O(n) | UTF-8 decoding |
| Parser | O(n) | Memory allocation for AST |
| Type Checker | O(n·m) | Generic instantiation |
| Borrow Checker | O(n·d) | Scope depth tracking |
| Exhaustiveness | O(k·log k) | Interval sorting (k = cases) |
| IR Generation | O(n) | LLVM API calls |

---

## Future Directions

### v0.1.0 Goals
- Complete core language features
- Stable ABI
- Basic standard library
- Documentation generator

### v0.2.0+ Goals
- Full async/await runtime
- Incremental compilation
- Language server protocol (LSP)
- Debugger integration (LLDB)
- Cross-compilation support

---

## For Contributors

**Read This First**:
1. [CONTRIBUTING.md](CONTRIBUTING.md) - Rules and expectations
2. [TASKS.md](TASKS.md) - Available work
3. [aria_ecosystem/specs/](../aria_ecosystem/specs/) - Language specifications

**Key Questions**:
- **Where do I start?** Read TASKS.md for beginner-friendly work.
- **How do I test?** `cmake --build build && ./build/ariac tests/your_test.aria`
- **What's the code style?** Match existing code. We don't bikeshed formatting.
- **Can I add a feature?** Only if it's in the specs. Want a new feature? Discuss in aria_ecosystem first.

**Common Pitfalls**:
- Not reading the spec before implementing
- Simplifying because "it's easier this way"
- Adding dependencies without discussion
- Not testing edge cases
- Ignoring error handling

---

*This document evolves with the project. Last major update: 2024-12-24*
