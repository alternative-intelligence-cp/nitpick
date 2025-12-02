# Aria v0.0.6 - Detailed Implementation Analysis
**Last Updated:** December 2, 2025  
**Purpose:** Deep technical analysis of implementation status with interfaces, dependencies, and action plan

---

## Table of Contents
1. [Component Dependency Tree](#component-dependency-tree)
2. [Detailed Component Analysis](#detailed-component-analysis)
3. [Interface Definitions](#interface-definitions)
4. [File-Level Implementation Status](#file-level-implementation-status)
5. [Action Plan - Logical Implementation Order](#action-plan)

---

## Component Dependency Tree

```
┌─────────────────────────────────────────────────────────────┐
│                        ARIA COMPILER                         │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        ▼                     ▼                     ▼
   ┌─────────┐          ┌─────────┐          ┌──────────┐
   │ FRONTEND│          │ BACKEND │          │ RUNTIME  │
   └─────────┘          └─────────┘          └──────────┘
        │                     │                     │
        │                     │                     │
   ┌────┴────┐           ┌────┴────┐          ┌────┴────┐
   │         │           │         │          │         │
   ▼         ▼           ▼         ▼          ▼         ▼
┌─────┐  ┌─────┐    ┌─────┐  ┌─────┐   ┌─────┐  ┌─────┐
│Prepro││Parser│    │Code │  │LLVM │   │  GC │  │Alloc│
│cessor││      │    │ Gen │  │ IR  │   │     │  │     │
└─────┘  └─────┘    └─────┘  └─────┘   └─────┘  └─────┘
   │         │           │                  │        │
   │         │           │                  │        │
   ▼         ▼           ▼                  ▼        ▼
┌─────┐  ┌─────┐    ┌─────┐           ┌──────────────┐
│Macro│  │ AST │    │Type │           │   mimalloc   │
│Expan││     │    │Lower│           │   (vendor)   │
└─────┘  └─────┘    └─────┘           └──────────────┘
   │         │           │
   │         │           │
   ▼         ▼           ▼
┌─────┐  ┌─────┐    ┌─────┐
│Lexer│  │Sema │    │Opti │
│     │  │     │    │mizer│
└─────┘  └─────┘    └─────┘

DEPENDENCY FLOW:
1. Source File (.aria)
2. Preprocessor (macros, comptime) → Expanded Source
3. Lexer → Token Stream
4. Parser → AST
5. Semantic Analysis (types, borrow checking) → Validated AST
6. Codegen → LLVM IR
7. LLVM Optimizer → Optimized IR
8. LLVM Backend → Object File
9. Linker → Executable (with runtime library)
```

---

## Detailed Component Analysis

### 1. PREPROCESSOR COMPONENT
**Status:** ❌ COMPLETELY MISSING  
**Required By:** Everything (runs before lexer)  
**Dependencies:** None (standalone text processor)

#### Required Interfaces:
```cpp
// src/frontend/preprocessor.h (DOES NOT EXIST)

class MacroContext {
public:
    std::string name;
    std::map<std::string, std::string> local_labels;
    std::map<std::string, std::string> local_vars;
};

class Preprocessor {
public:
    // Core API
    std::string process(const std::string& source_file);
    
    // Macro Management
    void defineMacro(const std::string& name, int param_count, const std::string& body);
    void undefineMacro(const std::string& name);
    bool isMacroDefined(const std::string& name);
    
    // Context Stack
    void pushContext(const std::string& name);
    void popContext();
    std::string getCurrentContext();
    
    // Constant Management
    void defineConstant(const std::string& name, const std::string& value);
    void undefineConstant(const std::string& name);
    
    // File Inclusion
    std::string includeFile(const std::string& path);
    
private:
    std::map<std::string, Macro> macros;
    std::stack<MacroContext> context_stack;
    std::map<std::string, std::string> constants;
    std::set<std::string> included_files; // Prevent circular includes
};
```

#### Implementation Requirements:
1. **Macro Expansion Engine**
   - Parameter substitution (%1, %2, ...)
   - Recursive expansion support
   - Nested macro handling
   
2. **Context Stack Manager**
   - Push/pop context operations
   - Context-local label resolution (%$label)
   - Context-local variable scoping (%$var)
   
3. **Conditional Compilation**
   - %ifdef/%ifndef evaluation
   - %if expression evaluation (needs simple expression parser)
   - %elif/%else branching
   
4. **Repeat Block Expansion**
   - %rep count parsing
   - Loop unrolling during expansion
   
5. **File Inclusion**
   - Recursive file processing
   - Circular include detection
   - Path resolution (relative/absolute)

#### Files to Create:
- `src/frontend/preprocessor.h` - Interface definitions
- `src/frontend/preprocessor.cpp` - Core preprocessor logic
- `src/frontend/macro.h` - Macro representation
- `src/frontend/macro_expander.cpp` - Macro expansion engine
- `tests/test_preprocessor.cpp` - Unit tests
- `tests/macros/` - Test macro files

---

### 2. COMPTIME EVALUATOR COMPONENT
**Status:** ❌ COMPLETELY MISSING  
**Required By:** Parser (for type expressions), Codegen (for constant folding)  
**Dependencies:** AST, Type System

#### Required Interfaces:
```cpp
// src/frontend/sema/comptime.h (DOES NOT EXIST)

class ComptimeValue {
public:
    enum class Kind {
        INT, FLOAT, STRING, BOOL, TYPE, NONE
    };
    
    Kind kind;
    union {
        int64_t int_val;
        double float_val;
        bool bool_val;
    };
    std::string string_val;
    std::shared_ptr<Type> type_val;
};

class ComptimeEvaluator {
public:
    // Evaluate comptime expression
    ComptimeValue evaluate(Expression* expr);
    
    // Evaluate comptime function
    ComptimeValue evaluateFunction(FuncDecl* func, std::vector<ComptimeValue> args);
    
    // Register comptime variable
    void defineComptimeVar(const std::string& name, ComptimeValue value);
    
    // Get comptime variable
    ComptimeValue getComptimeVar(const std::string& name);
    
    // Check if expression is comptime-evaluable
    bool isComptimeEvaluable(Expression* expr);
    
private:
    std::map<std::string, ComptimeValue> comptime_vars;
    std::map<std::string, FuncDecl*> comptime_functions;
};
```

#### Implementation Requirements:
1. **Expression Evaluator**
   - Constant folding (arithmetic, logical)
   - Type expressions (selectIntType example)
   - Pure function calls
   
2. **Function Interpreter**
   - AST interpretation (not LLVM execution)
   - Parameter binding
   - Return value extraction
   
3. **Type System Integration**
   - Type-as-value representation
   - Type selection/generation
   - Type validation at compile time
   
4. **Purity Checker**
   - Verify no side effects in comptime functions
   - Ensure all inputs are compile-time known
   - Error on runtime dependencies

#### Integration Points:
- **Parser:** Resolve comptime array sizes, type expressions
- **Codegen:** Fold comptime constants, eliminate dead branches
- **Type System:** Generate types based on comptime computation

#### Files to Create:
- `src/frontend/sema/comptime.h` - Interface
- `src/frontend/sema/comptime.cpp` - Core evaluator
- `src/frontend/sema/comptime_interpreter.cpp` - Function interpreter
- `tests/test_comptime.cpp` - Unit tests
- `tests/comptime/` - Test comptime expressions

---

### 3. LEXER COMPONENT
**Status:** ✅ 95% COMPLETE  
**Required By:** Parser  
**Dependencies:** Preprocessor (receives preprocessed source)

#### Current Implementation:
- **File:** `src/frontend/lexer.cpp` (1384 lines)
- **Interface:** `src/frontend/lexer.h`
- **Token Definitions:** `src/frontend/tokens.h` (243 token types)

#### What Works:
✅ All 243 token types defined  
✅ Keyword recognition (37 keywords)  
✅ Integer literals (decimal only)  
✅ String literals with escapes (\n, \t, \\, \", etc.)  
✅ Template literals with &{} interpolation  
✅ Comments (line // and block /* */)  
✅ All operators with maximal munch  
✅ All delimiters

#### What's Missing:
❌ Hex literals (0xFF)  
❌ Binary literals (0b1010)  
❌ Octal literals (0o755)  
❌ Float literals (3.14, 1e10, 0x1.2p3)  
❌ Char literals ('a')  
❌ Preprocessor directive tokens (%, %macro, etc.)

#### Files Needing Updates:
- `src/frontend/lexer.cpp` - Add missing literal types
- `tests/test_lexer.cpp` - Comprehensive lexer tests

---

### 4. PARSER COMPONENT
**Status:** 🟡 60% COMPLETE  
**Required By:** Semantic Analysis  
**Dependencies:** Lexer (token stream), AST (node creation)

#### Current Implementation Structure:
```
src/frontend/
├── parser.h              (Base class, 150 lines)
├── parser.cpp            (Core parsing, 1384 lines)
├── parser_expr.cpp       (Expression parsing)
├── parser_stmt.cpp       (Statement parsing)
├── parser_decl.cpp       (Declaration parsing)
└── parser_func.cpp       (Function parsing - stub)
```

#### Parsing Status by Category:

**Expressions (parser_expr.cpp):**
- ✅ Binary operators (all working)
- ✅ Unary operators (-, !, ~)
- ✅ Postfix (++, --)
- ✅ Literals (int, string, bool, template strings)
- ✅ Variable references
- ✅ Function calls
- ✅ Member access (obj.field)
- ✅ Array indexing
- ✅ Ternary (is cond : true : false)
- ✅ Lambda expressions with immediate invocation
- ✅ Object literals ({ field: value })
- ❌ Pipeline operators (|>, <|)
- ❌ Null safety operators (??, ?., ?)
- ❌ Spaceship operator (<=>)
- ❌ Range expressions (.., ...)

**Statements (parser_stmt.cpp):**
- ✅ Variable declarations
- ✅ Expression statements
- ✅ Return statements
- ✅ If/else statements
- ✅ While loops (parsed, codegen missing)
- ✅ Till loops (complete)
- ✅ When/then/end loops (parsed, codegen missing)
- ✅ Pick/fall pattern matching (complete)
- ✅ Block statements
- 🟡 For loops (AST exists, parser incomplete)
- 🟡 Break/continue (minimal)
- ❌ Defer statements (AST exists, not parsed)

**Declarations (parser_decl.cpp):**
- 🟡 Function declarations (basic parsing)
- ❌ Struct declarations
- ❌ Type aliases
- ❌ Module declarations (stub exists)
- ❌ Use statements (stub exists)
- ❌ Extern blocks (stub exists)

#### Required Interfaces:
```cpp
// src/frontend/parser.h

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    std::unique_ptr<Block> parse();
    
    // Expression Parsing
    std::unique_ptr<Expression> parseExpression();
    std::unique_ptr<Expression> parsePrimary();
    std::unique_ptr<Expression> parseBinaryOp(int precedence);
    std::unique_ptr<Expression> parseUnaryOp();
    std::unique_ptr<Expression> parsePostfixOp();
    std::unique_ptr<Expression> parseLambda();
    std::unique_ptr<Expression> parseObjectLiteral();
    
    // Statement Parsing
    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<Statement> parseVarDecl();
    std::unique_ptr<Statement> parseIfStmt();
    std::unique_ptr<Statement> parseWhileLoop();
    std::unique_ptr<Statement> parseForLoop();
    std::unique_ptr<Statement> parseTillLoop();
    std::unique_ptr<Statement> parseWhenLoop();
    std::unique_ptr<Statement> parsePickStmt();
    std::unique_ptr<Statement> parseDeferStmt();
    
    // Declaration Parsing
    std::unique_ptr<FuncDecl> parseFuncDecl();
    std::unique_ptr<StructDecl> parseStructDecl();
    std::unique_ptr<UseStmt> parseUseStmt();
    std::unique_ptr<ExternBlock> parseExternBlock();
    
private:
    std::vector<Token> tokens;
    size_t current;
    
    Token peek();
    Token advance();
    bool match(TokenType type);
    Token expect(TokenType type);
};
```

#### Files Needing Work:
- `src/frontend/parser_func.cpp` - Complete function parsing
- `src/frontend/parser_decl.cpp` - Add struct, type alias parsing
- `src/frontend/parser_expr.cpp` - Add missing operators
- `tests/test_parser.cpp` - Comprehensive parser tests

---

### 5. AST COMPONENT
**Status:** ✅ 85% COMPLETE  
**Required By:** Semantic Analysis, Codegen  
**Dependencies:** None (pure data structures)

#### Current Structure:
```
src/frontend/ast/
├── ast.h                 (Base classes, visitor pattern)
├── expr.h                (Expression nodes)
├── stmt.h                (Statement nodes)
├── control_flow.h        (Pick, Fall, When, Till)
├── loops.h               (For, While, Till)
└── defer.h               (Defer statement)
```

#### Complete Node Types:
✅ **Expressions:**
- VarExpr, IntLiteral, BoolLiteral, StringLiteral
- TemplateString, BinaryOp, UnaryOp, PostfixOp
- CallExpr, LambdaExpr, TernaryExpr
- MemberAccess, ArrayIndex, ObjectLiteral

✅ **Statements:**
- VarDecl, ExpressionStmt, ReturnStmt
- Block, IfStmt
- PickStmt, PickCase, FallStmt
- TillLoop, WhenLoop
- DeferStmt

🟡 **Partially Defined:**
- ForLoop (structure exists, not fully used)
- WhileLoop (structure exists, codegen incomplete)
- BreakStmt, ContinueStmt (minimal)

❌ **Missing:**
- StructDecl, TypeAlias
- UseStmt (stub exists)
- ExternBlock (stub exists)
- ModDef (stub exists)
- AsyncBlock, AwaitExpr

#### Visitor Pattern Status:
✅ Base `AstVisitor` interface complete  
✅ All expression/statement nodes have `accept()` methods  
✅ Lambda accept() fixed (was commented out)

#### Files Needing Work:
- Create `src/frontend/ast/decl.h` for declarations
- Add async/await node types
- Complete ForLoop integration

---

### 6. SEMANTIC ANALYSIS COMPONENT
**Status:** 🟡 40% COMPLETE  
**Required By:** Codegen  
**Dependencies:** AST, Type System

#### Current Implementation:
```
src/frontend/sema/
├── types.h               (Type system definitions, 251 lines)
├── type_checker.h        (Type checker interface)
├── type_checker.cpp      (Type checking visitor, 482 lines)
├── borrow_checker.h      (Borrow checker interface)
├── borrow_checker.cpp    (Borrow checking logic)
├── escape_analysis.h     (Escape analysis interface)
└── escape_analysis.cpp   (Escape analysis logic)
```

#### Type System Status:

**Type Definitions (types.h):**
```cpp
enum class TypeKind {
    VOID, BOOL,
    INT8, INT16, INT32, INT64, INT128, INT256, INT512,
    UINT8, UINT16, UINT32, UINT64,
    FLT32, FLT64,
    TRIT, TRYTE, NIT, NYTE,  // Exotic types
    STRING,
    DYN,  // ← Recently added
    POINTER, ARRAY, FUNCTION, STRUCT,
    UNKNOWN, ERROR
};

class Type {
    TypeKind kind;
    std::string name;
    
    // For pointers
    bool is_wild;
    bool is_pinned;
    std::shared_ptr<Type> pointee;
    
    // For arrays
    std::shared_ptr<Type> element_type;
    int array_size;
    
    // For functions
    std::shared_ptr<Type> return_type;
    std::vector<std::shared_ptr<Type>> param_types;
};
```

**Type Checker (type_checker.cpp):**
- ✅ Basic type checking for variables
- ✅ Binary/unary operator type checking
- ✅ Lambda return type handling
- ✅ Null safety (prevents crashes)
- ❌ Function signature checking
- ❌ Result type validation
- ❌ Exotic type operations
- ❌ Array type checking
- ❌ Struct field validation

**Borrow Checker (borrow_checker.cpp):**
- 🟡 Basic borrow checking exists
- ❌ Not integrated with wild/gc/stack keywords
- ❌ Pin operator (#) not checked
- ❌ Safe reference ($) not validated

**Escape Analysis (escape_analysis.cpp):**
- 🟡 Basic escape analysis exists
- ❌ Not used for optimization decisions

#### Required Enhancements:

1. **Complete Type Checker:**
```cpp
class TypeChecker : public AstVisitor {
    // Add missing visitors
    void visit(StructDecl* node);
    void visit(FuncDecl* node);  // Full signature checking
    void visit(ResultType* node);  // Validate Result<T,E>
    void visit(ArrayType* node);
    
    // Add exotic type operations
    void checkTritOperation(BinaryOp* node);
    void checkNyteOperation(BinaryOp* node);
};
```

2. **Integrate Borrow Checker:**
```cpp
class BorrowChecker : public AstVisitor {
    // Check wild pointer usage
    void checkWildPointerSafety(VarDecl* node);
    
    // Check pinning operations
    void checkPinOperation(UnaryOp* node);
    
    // Check safe references
    void checkSafeReference(VarDecl* node);
};
```

#### Files Needing Work:
- `src/frontend/sema/type_checker.cpp` - Complete all visitors
- `src/frontend/sema/borrow_checker.cpp` - Integrate with parser
- `src/frontend/sema/exotic_types.cpp` - NEW: Exotic type validation
- `tests/test_type_checker.cpp` - Comprehensive tests

---

### 7. CODEGEN COMPONENT
**Status:** 🟡 45% COMPLETE  
**Required By:** LLVM Backend  
**Dependencies:** AST, Type System, LLVM

#### Current Implementation:
```
src/backend/
├── codegen.h             (Interface)
├── codegen.cpp           (Main codegen, 1205 lines)
└── lowering_ternary.cpp  (Exotic type lowering - minimal)
```

#### CodeGen Architecture:
```cpp
class CodeGenContext {
    std::unique_ptr<LLVMContext> llvmContext;
    std::unique_ptr<Module> module;
    std::unique_ptr<IRBuilder<>> builder;
    Function* currentFunction;
    
    // Symbol table with scope management
    std::vector<std::map<std::string, Symbol>> scopes;
    
    void pushScope();
    void popScope();
    void define(std::string name, Value* val, bool is_ref);
    Symbol* lookup(std::string name);
    
    // Type mapping
    llvm::Type* getLLVMType(const std::string& ariaType);
};

class CodeGenVisitor : public AstVisitor {
    CodeGenContext& ctx;
    
    // Expression visitors (return Value*)
    Value* visitExpr(Expression* node);
    
    // Statement visitors (return void)
    void visit(VarDecl* node) override;
    void visit(FuncDecl* node) override;
    void visit(IfStmt* node) override;
    void visit(PickStmt* node) override;
    void visit(TillLoop* node) override;
    // ... etc
};
```

#### What Works:
✅ Basic type lowering (int8-int512, flt32-flt64, bool, string → LLVM types)  
✅ Variable declarations (GC allocation via runtime calls)  
✅ Binary/unary operators → LLVM instructions  
✅ If/else → LLVM branches  
✅ Pick/fall pattern matching → switch/branch  
✅ Till loops → complete with bidirectional counting  
✅ Ternary (is) → LLVM select  
✅ Template strings → string concatenation  
✅ Lambda expressions → anonymous functions  
✅ Member access → GEP instructions  
✅ Module system → `__aria_module_init()` wrapper  

#### What's Missing:
❌ Function calls with Result type unwrapping  
❌ While/for loop codegen  
❌ When/then/end loop codegen  
❌ Defer statement → cleanup blocks  
❌ Exotic type operations (trit/tryte/nit/nyte)  
❌ Wild/stack allocation (only GC works)  
❌ Pointer operations (@, #, $, *, ->)  
❌ Array operations (indexing, slicing)  
❌ Struct field access  
❌ Async/await state machines  
❌ SIMD optimizations for exotic types  

#### Type Lowering Details:

**Current getLLVMType() Implementation:**
```cpp
llvm::Type* CodeGenContext::getLLVMType(const std::string& ariaType) {
    // Integers
    if (ariaType == "int1") return Type::getInt1Ty(llvmContext);
    if (ariaType == "int8") return Type::getInt8Ty(llvmContext);
    if (ariaType == "int16") return Type::getInt16Ty(llvmContext);
    if (ariaType == "int32") return Type::getInt32Ty(llvmContext);
    if (ariaType == "int64") return Type::getInt64Ty(llvmContext);
    if (ariaType == "int128") return Type::getInt128Ty(llvmContext);
    if (ariaType == "int512") return Type::getIntNTy(llvmContext, 512);
    
    // Floats
    if (ariaType == "flt32") return Type::getFloatTy(llvmContext);
    if (ariaType == "flt64") return Type::getDoubleTy(llvmContext);
    
    // Special
    if (ariaType == "void") return Type::getVoidTy(llvmContext);
    if (ariaType == "dyn") return PointerType::getUnqual(llvmContext);
    
    // Default: pointer (for strings, arrays, objects)
    return PointerType::getUnqual(llvmContext);
}
```

**Missing Exotic Type Lowering:**
```cpp
// NEEDS TO BE ADDED:
if (ariaType == "trit") return Type::getInt2Ty(llvmContext);  // -1, 0, 1 → 2 bits
if (ariaType == "tryte") return Type::getInt16Ty(llvmContext);  // 59,049 values
if (ariaType == "nit") return Type::getInt4Ty(llvmContext);  // -4 to 4 → 4 bits
if (ariaType == "nyte") return Type::getInt16Ty(llvmContext);  // 59,049 values

// SIMD packed types for bulk operations:
if (ariaType == "tryte32") return VectorType::get(Type::getInt16Ty(ctx), 32, false);
if (ariaType == "nyte32") return VectorType::get(Type::getInt16Ty(ctx), 32, false);
```

#### Module System (WORKING):
```cpp
// Generated IR structure:
define internal void @__aria_module_init() {
    // All module-level code (variables, print statements, lambdas)
    ret void
}

define i8 @__user_main() {
    // User's main function (if exists)
    ret i8 0
}

define i64 @main() {
entry:
    call void @__aria_module_init()  // Run module code first
    %0 = call i8 @__user_main()      // Then run user's main
    %1 = sext i8 %0 to i64
    ret i64 %1
}
```

#### Files Needing Work:
- `src/backend/codegen.cpp` - Add missing visitors
- `src/backend/lowering_exotic.cpp` - NEW: Exotic type operations
- `src/backend/lowering_simd.cpp` - NEW: SIMD optimizations
- `src/backend/memory_ops.cpp` - NEW: Wild/stack allocation
- `tests/test_codegen.cpp` - Comprehensive tests

---

### 8. RUNTIME COMPONENT
**Status:** 🟡 40% COMPLETE  
**Required By:** Generated code  
**Dependencies:** mimalloc (vendor)

#### Current Implementation:
```
src/runtime/
├── gc/
│   ├── nursery.cpp       (GC nursery allocation, 200+ lines)
│   └── gc_impl.cpp       (Mark-sweep GC, partial)
├── concurrency/
│   ├── scheduler.cpp     (Coroutine scheduler, exists but not used)
│   └── ramp.cpp          (RAMP optimization, exists but not used)
├── io/
│   ├── print.cpp         (Print function)
│   └── file.cpp          (File operations, partial)
├── core/
│   └── string_impl.cpp   (String operations)
├── io_linux.cpp          (Linux I/O, partial)
└── io_windows.cpp        (Windows I/O, partial)
```

#### Runtime API Requirements:

**Memory Management:**
```cpp
// src/runtime/gc/gc.h
extern "C" {
    // GC allocation
    void* get_current_thread_nursery();
    void* aria_gc_alloc(void* nursery, size_t size);
    void aria_gc_collect();
    
    // Manual allocation
    void* aria_alloc(size_t size);
    void aria_free(void* ptr);
    
    // Stack allocation (compiler-managed, no runtime call)
    // Just uses LLVM alloca instruction
}
```

**I/O System (6 streams):**
```cpp
// NEEDS TO BE CREATED: src/runtime/io/streams.h
extern "C" {
    // Text streams
    void aria_stdin_read(char* buffer, size_t size);
    void aria_stdout_write(const char* data, size_t size);
    void aria_stderr_write(const char* data, size_t size);
    
    // Debug stream
    void aria_stddbg_write(const char* json_log);
    
    // Binary streams
    void aria_stddati_read(void* buffer, size_t size);
    void aria_stddato_write(const void* data, size_t size);
}
```

**Concurrency:**
```cpp
// src/runtime/concurrency/coroutine.h
extern "C" {
    void* aria_spawn_coroutine(void (*func)(void*), void* args);
    void aria_yield();
    void aria_await(void* coroutine_handle);
}
```

**Process Management:**
```cpp
// NEEDS TO BE CREATED: src/runtime/process/process.h
extern "C" {
    struct AriaProcessResult {
        int pid;
        bool is_child;
        bool success;
    };
    
    AriaProcessResult aria_fork();
    int aria_exec(const char* program, const char** args);
    int aria_wait(int pid);
    
    struct AriaPipe {
        int read_fd;
        int write_fd;
    };
    
    AriaPipe aria_create_pipe();
}
```

#### What Works:
✅ GC nursery allocation  
✅ Basic print() function  
✅ String operations  

#### What's Missing:
❌ Complete GC (mark, sweep, collection)  
❌ Wild allocation wrappers  
❌ 6-stream I/O system  
❌ Coroutine scheduler integration  
❌ Process/fork/exec functions  
❌ Pipe creation  
❌ File I/O functions  
❌ Network functions  

#### Files to Create/Complete:
- `src/runtime/gc/gc_impl.cpp` - Complete GC
- `src/runtime/memory/allocator.cpp` - Wild allocation
- `src/runtime/io/streams.cpp` - NEW: 6-stream system
- `src/runtime/process/fork.cpp` - NEW: Process management
- `src/runtime/process/pipe.cpp` - NEW: IPC
- `src/runtime/file/operations.cpp` - NEW: File I/O
- `src/runtime/network/http.cpp` - NEW: HTTP client

---

## Interface Definitions

### Critical Cross-Component Interfaces

#### 1. Preprocessor → Lexer
```cpp
// Preprocessor outputs plain text with macros expanded
std::string preprocessed_source = preprocessor.process("input.aria");

// Lexer consumes preprocessed source
Lexer lexer(preprocessed_source);
std::vector<Token> tokens = lexer.tokenize();
```

#### 2. Lexer → Parser
```cpp
// Token stream interface
struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
};

std::vector<Token> tokens = lexer.tokenize();
Parser parser(tokens);
std::unique_ptr<Block> ast = parser.parse();
```

#### 3. Parser → Semantic Analysis
```cpp
// AST is validated before codegen
TypeCheckResult type_result = checkTypes(ast.get());
BorrowCheckResult borrow_result = checkBorrows(ast.get());
EscapeAnalysisResult escape_result = runEscapeAnalysis(ast.get());

if (!type_result.success || !borrow_result.safe) {
    // Compilation fails
}
```

#### 4. Semantic Analysis → Codegen
```cpp
// Codegen receives validated AST
CodeGenContext ctx;
CodeGenVisitor codegen(ctx);
ast->accept(codegen);

// Output LLVM IR
ctx.module->print(llvm::outs(), nullptr);
```

#### 5. Codegen → Runtime
```cpp
// Generated code calls runtime functions
declare void @get_current_thread_nursery()
declare void* @aria_gc_alloc(void*, i64)
declare void @puts(ptr)

// Runtime library must export these symbols
extern "C" {
    void* get_current_thread_nursery() { ... }
    void* aria_gc_alloc(void* nursery, size_t size) { ... }
}
```

---

## File-Level Implementation Status

### Files That Work (Don't Touch Unless Necessary):
```
✅ src/frontend/lexer.cpp (1384 lines) - 95% complete
✅ src/frontend/tokens.h (243 token types)
✅ src/frontend/parser.cpp (1384 lines) - Core parsing solid
✅ src/frontend/ast/control_flow.h - Pick/fall complete
✅ src/frontend/ast/loops.h - Till loop complete
✅ src/backend/codegen.cpp (1205 lines) - Basic features work
✅ src/runtime/gc/nursery.cpp - Nursery allocation works
✅ src/runtime/io/print.cpp - Print works
```

### Files Needing Completion:
```
🟡 src/frontend/parser_func.cpp - Stub only, needs full implementation
🟡 src/frontend/parser_expr.cpp - Missing advanced operators
🟡 src/frontend/sema/type_checker.cpp - Basic types only
🟡 src/backend/codegen.cpp - Missing defer, while, when, exotic types
🟡 src/runtime/gc/gc_impl.cpp - Partial GC implementation
```

### Files That Don't Exist But Are Required:
```
❌ src/frontend/preprocessor.h
❌ src/frontend/preprocessor.cpp
❌ src/frontend/macro.h
❌ src/frontend/macro_expander.cpp
❌ src/frontend/sema/comptime.h
❌ src/frontend/sema/comptime.cpp
❌ src/frontend/sema/exotic_types.cpp
❌ src/frontend/ast/decl.h (for struct, type alias)
❌ src/backend/lowering_exotic.cpp
❌ src/backend/lowering_simd.cpp
❌ src/backend/memory_ops.cpp
❌ src/runtime/io/streams.cpp (6-stream system)
❌ src/runtime/process/fork.cpp
❌ src/runtime/process/pipe.cpp
❌ src/runtime/file/operations.cpp
❌ src/runtime/network/http.cpp
❌ src/stdlib/* (all standard library functions)
```

---

## Action Plan - Logical Implementation Order

### Phase 1: Foundation (No Dependencies)
**Goal:** Get fundamental systems working independently

1. **Complete Lexer** (1 week)
   - Add hex, binary, octal literals
   - Add float literal parsing
   - Add char literal parsing
   - Add preprocessor directive tokens
   - **Files:** `lexer.cpp`
   - **Tests:** `tests/test_lexer.cpp`
   - **Validation:** All spec examples tokenize correctly

2. **Build Preprocessor** (2 weeks)
   - Create preprocessor infrastructure
   - Implement macro expansion
   - Implement context stack
   - Implement conditional compilation
   - **Files:** `preprocessor.h`, `preprocessor.cpp`, `macro.h`, `macro_expander.cpp`
   - **Tests:** `tests/test_preprocessor.cpp`, `tests/macros/*`
   - **Validation:** Spec section 8.7 examples work

3. **Build Comptime Evaluator** (2 weeks)
   - Create comptime value types
   - Implement expression evaluator
   - Implement function interpreter
   - Integrate with type system
   - **Files:** `comptime.h`, `comptime.cpp`, `comptime_interpreter.cpp`
   - **Tests:** `tests/test_comptime.cpp`, `tests/comptime/*`
   - **Validation:** Spec section 8.7 comptime examples work

### Phase 2: Parser Completion (Depends on Phase 1)
**Goal:** Parse all spec syntax correctly

4. **Complete Function Parsing** (1 week)
   - Full function declaration parsing
   - Parameter lists with types
   - Result type enforcement
   - **Files:** `parser_func.cpp`
   - **Tests:** `tests/test_func_parsing.cpp`
   - **Validation:** Spec section 8.4 examples parse

5. **Add Missing Operators** (1 week)
   - Pipeline (|>, <|)
   - Null safety (??, ?., ?)
   - Spaceship (<=>)
   - Range (.., ...)
   - **Files:** `parser_expr.cpp`
   - **Tests:** `tests/test_operators.cpp`
   - **Validation:** All operator examples parse

6. **Add Declaration Parsing** (1 week)
   - Struct declarations
   - Type aliases
   - Complete use/mod/extern
   - **Files:** `parser_decl.cpp`, `ast/decl.h`
   - **Tests:** `tests/test_declarations.cpp`
   - **Validation:** Module examples parse

### Phase 3: Semantic Analysis (Depends on Phase 2)
**Goal:** Validate all parsed code

7. **Complete Type Checker** (2 weeks)
   - Function signature checking
   - Result type validation
   - Array/struct type checking
   - Exotic type operations
   - **Files:** `type_checker.cpp`, `exotic_types.cpp`
   - **Tests:** `tests/test_type_system.cpp`
   - **Validation:** Type errors caught correctly

8. **Integrate Borrow Checker** (1 week)
   - Wild pointer safety
   - Pin operation validation
   - Safe reference checking
   - **Files:** `borrow_checker.cpp`
   - **Tests:** `tests/test_borrow_checker.cpp`
   - **Validation:** Memory safety enforced

### Phase 4: Code Generation (Depends on Phase 3)
**Goal:** Generate correct LLVM IR for all features

9. **Complete Basic Codegen** (2 weeks)
   - While/for loops
   - When/then/end loops
   - Defer statements
   - Function calls with Result unwrapping
   - **Files:** `codegen.cpp`
   - **Tests:** `tests/test_codegen_control.cpp`
   - **Validation:** All control flow works

10. **Memory Management Codegen** (2 weeks)
    - Wild allocation
    - Stack allocation
    - Pointer operations (@, *, ->)
    - Pin operation (#)
    - Safe reference ($)
    - **Files:** `memory_ops.cpp`
    - **Tests:** `tests/test_memory_ops.cpp`
    - **Validation:** Spec section 8.5 examples work

11. **Exotic Type Codegen** (2 weeks)
    - Trit/tryte operations
    - Nit/nyte operations
    - SIMD optimizations
    - **Files:** `lowering_exotic.cpp`, `lowering_simd.cpp`
    - **Tests:** `tests/test_exotic_types.cpp`
    - **Validation:** Exotic type math works

### Phase 5: Runtime Library (Depends on Phase 4)
**Goal:** Complete runtime support for all features

12. **Complete GC** (1 week)
    - Mark phase
    - Sweep phase
    - Collection triggers
    - **Files:** `gc_impl.cpp`
    - **Tests:** `tests/test_gc.cpp`
    - **Validation:** No memory leaks

13. **Build 6-Stream I/O** (1 week)
    - Implement all 6 streams
    - stdin/stdout/stderr
    - stddbg/stddati/stddato
    - **Files:** `io/streams.cpp`
    - **Tests:** `tests/test_io_streams.cpp`
    - **Validation:** All streams work

14. **Process Management** (1 week)
    - fork/exec/wait
    - Pipe creation
    - **Files:** `process/fork.cpp`, `process/pipe.cpp`
    - **Tests:** `tests/test_process.cpp`
    - **Validation:** Spec section 8.6 works

15. **Standard Library** (3 weeks)
    - File I/O functions
    - Collection utilities
    - Math functions
    - Network functions
    - **Files:** `stdlib/*`
    - **Tests:** `tests/test_stdlib.cpp`
    - **Validation:** All stdlib functions work

### Phase 6: Advanced Features (Depends on Phase 5)
**Goal:** Async, optimization, tooling

16. **Async/Await** (2 weeks)
    - State machine generation
    - Coroutine integration
    - **Files:** `async_codegen.cpp`, `scheduler.cpp`
    - **Tests:** `tests/test_async.cpp`
    - **Validation:** Async examples work

17. **Build System & Tooling** (1 week)
    - Docker environment
    - AppImage packaging
    - LSP server basics
    - **Files:** `Dockerfile`, packaging scripts
    - **Tests:** End-to-end build test
    - **Validation:** Can distribute binaries

---

## Estimated Timeline

**Total Estimated Time: ~24 weeks (6 months)**

- Phase 1: 5 weeks
- Phase 2: 3 weeks  
- Phase 3: 3 weeks
- Phase 4: 6 weeks
- Phase 5: 6 weeks
- Phase 6: 3 weeks

**Buffer for debugging/testing: +25% = 30 weeks total (~7.5 months)**

---

## Next Steps

1. **Review this analysis** - Validate dependencies and order
2. **Create work packages** - Break each phase into daily tasks
3. **Set up testing infrastructure** - Ensure tests exist before implementation
4. **Begin Phase 1, Task 1** - Complete lexer (1 file, 1 week)
5. **Document as we go** - Update this file with progress

**One task at a time. Test completely before moving on.**

---
**End of Implementation Details**
