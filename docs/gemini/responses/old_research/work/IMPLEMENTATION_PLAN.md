# Aria Compiler v0.0.7 - Gemini Work Package Implementation Plan

**Generated:** December 8, 2025  
**Source:** 5 Gemini Deep Research Work Package Responses  
**Total Issues:** 15 from knownProblems.txt (83% coverage)  
**Strategy:** Prioritized implementation based on dependencies and impact

---

## 📊 EXECUTIVE SUMMARY

This plan implements 15 critical compiler improvements across 5 work packages:

- **Package 001:** Parser Foundation (3 issues) - **HIGH PRIORITY** ⚡
- **Package 002:** Advanced Features (3 issues) - **HIGH PRIORITY** ⚡
- **Package 003:** Runtime Integration (3 issues) - **MEDIUM PRIORITY** 🔧
- **Package 004:** Developer Experience (3 issues) - **MEDIUM PRIORITY** 🔧
- **Package 005:** Quality & Optimization (3 issues) - **LOW PRIORITY** 🎯

**Estimated Total Effort:** 8-10 weeks (1 developer, full-time)

---

## 🎯 PACKAGE 001: PARSER FOUNDATION (Week 1)

**Priority:** CRITICAL - Foundation for all graphics/vector features  
**Estimated Effort:** 22 hours / 3 days  
**Dependencies:** None - can start immediately

### Issue 1.1: Vector Type Keywords
**File:** `src/frontend/lexer.cpp`, `src/frontend/tokens.h`

**Implementation Steps:**
1. Add X-Macro token list for all vector types:
   - Float vectors: `vec2`, `vec3`, `vec4` (GLSL standard)
   - Double vectors: `dvec2`, `dvec3`, `dvec4`
   - Integer vectors: `ivec2`, `ivec3`, `ivec4`
   - Unsigned vectors: `uvec2`, `uvec3`, `uvec4`
   - Boolean vectors: `bvec2`, `bvec3`, `bvec4`
   - Matrix types: `mat2`, `mat3`, `mat4`, `mat2x3`, etc.

2. Update `Lexer::initKeywords()` to register all vector types
3. Update `Parser::isTypeToken()` to recognize vector tokens
4. Add vector type mappings in `CodeGenContext::getLLVMType()`

**Deliverables:**
- ✅ Vector types parse as TYPE tokens (not IDENTIFIER)
- ✅ Type checking recognizes vector types
- ✅ LLVM backend maps to `FixedVectorType`

**Testing:**
```aria
// Should parse successfully
func:test = void(v: vec4, m: mat3) { }
var vec2:position;
```

**Estimated Time:** 2 hours

---

### Issue 1.2: Vector Literal Syntax
**Files:** `src/frontend/ast.h`, `src/frontend/parser_expr.cpp`, `src/backend/codegen.cpp`

**Implementation Steps:**

**Phase 1: AST Node (30 min)**
1. Create `VectorLiteral` class inheriting from `Expression`
2. Add fields:
   - `TokenType vectorType` - The specific vector type (vec4, ivec3, etc.)
   - `std::vector<std::unique_ptr<Expression>> elements` - Constructor arguments
3. Add visitor method to `AstVisitor`

**Phase 2: Parser Logic (2 hours)**
1. Add `Parser::isVectorType()` helper
2. Implement `Parser::parseVectorLiteral()`:
   - Consume vector type token (e.g., `vec4`)
   - Consume `(`
   - Parse comma-separated expressions
   - Consume `)`
   - Return `VectorLiteral` node
3. Integrate into `Parser::parsePrefix()` (Pratt parser)

**Phase 3: Semantic Analysis (2 hours)**
1. Implement `TypeChecker::visit(VectorLiteral*)`
2. Validate component count:
   - Broadcasting: `vec4(1.0)` → 1 scalar → valid
   - Exact match: `vec3(1.0, 2.0, 3.0)` → 3 components → valid
   - Composition: `vec4(vec2(1,2), 3.0, 4.0)` → 2+1+1 = 4 → valid
   - Error: `vec3(vec2(1,2))` → 2 components → ERROR
3. Type checking: Ensure element types match vector base type

**Phase 4: Code Generation (6 hours)**
1. Implement `CodegenVisitor::visit(VectorLiteral*)`

**Case A: Constant Folding**
```cpp
// All elements are compile-time constants
std::vector<llvm::Constant*> constElements;
for (auto& expr : elements) {
    expr->accept(*this);
    constElements.push_back(llvm::cast<llvm::Constant>(lastValue));
}
lastValue = llvm::ConstantVector::get(constElements);
```

**Case B: Runtime Construction (insertelement chain)**
```cpp
llvm::Value* vec = llvm::PoisonValue::get(targetLLVMType);
for (unsigned i = 0; i < elements.size(); i++) {
    elements[i]->accept(*this);
    vec = builder.CreateInsertElement(vec, lastValue, i);
}
lastValue = vec;
```

**Case C: Broadcasting**
```cpp
// vec4(scalar) → splat to all lanes
if (elements.size() == 1) {
    elements[0]->accept(*this);
    llvm::Value* scalar = lastValue;
    llvm::Value* poison = llvm::PoisonValue::get(targetType);
    llvm::Value* inserted = builder.CreateInsertElement(poison, scalar, 0);
    std::vector<int> mask(targetSize, 0); // <0,0,0,0>
    lastValue = builder.CreateShuffleVector(inserted, inserted, mask);
}
```

**Case D: vec3 Padding**
```cpp
// CRITICAL: vec3 requires 16-byte alignment (std140)
// Implement as <4 x float> with 4th element = 0.0
if (vectorType == TOKEN_TYPE_VEC3 && elements.size() == 3) {
    elements.push_back(ConstantFP::get(ctx, 0.0)); // Pad to vec4
}
```

**Deliverables:**
- ✅ GLSL-style vector constructors parse correctly
- ✅ Broadcasting works: `vec4(1.0)` creates `<1,1,1,1>`
- ✅ Composition works: `vec4(vec2(x,y), z, w)`
- ✅ vec3 correctly padded to 16 bytes

**Testing:**
```aria
var vec4:color = vec4(1.0, 0.5, 0.0, 1.0);
var vec3:pos = vec3(0.0); // Broadcasting
var vec4:mixed = vec4(vec2(1.0, 2.0), 3.0, 4.0); // Composition
```

**Estimated Time:** 10.5 hours

---

### Issue 5.1: Parser Error Messages
**Files:** `src/frontend/tokens.h`, `src/frontend/diagnostic.h/.cpp`, `src/frontend/parser.cpp`

**Implementation Steps:**

**Phase 1: Token String Conversion (1 hour)**
1. Refactor `tokens.h` to use X-Macro pattern:
```cpp
#define TOKEN_LIST(T) \
   T(TOKEN_EOF, "end of file") \
   T(TOKEN_INVALID, "invalid token") \
   T(TOKEN_IDENTIFIER, "identifier") \
   T(TOKEN_INT_LITERAL, "integer literal") \
   T(TOKEN_KW_FUNC, "func") \
   T(TOKEN_TYPE_VEC4, "vec4") \
   T(TOKEN_LEFT_PAREN, "(") \
   // ... all tokens ...

enum TokenType {
   #define DEFINE_ENUM(name, str) name,
   TOKEN_LIST(DEFINE_ENUM)
   #undef DEFINE_ENUM
};

inline const char* tokenTypeToString(TokenType type) {
   static const char* names[] = {
       #define DEFINE_STRING(name, str) str,
       TOKEN_LIST(DEFINE_STRING)
       #undef DEFINE_STRING
   };
   return names[type];
}
```

**Phase 2: Diagnostic Engine (4 hours)**
1. Create `DiagnosticEngine` class:
```cpp
class DiagnosticEngine {
   const std::string& source;
   std::string filename;
   
   // ANSI Colors
   const char* COLOR_RED = "\033[1;31m";
   const char* COLOR_RESET = "\033[0m";
   const char* COLOR_YELLOW = "\033[1;33m";
   
public:
   void error(int line, int col, const std::string& message);
   void warning(int line, int col, const std::string& message);
   void help(const std::string& suggestion);
};
```

2. Implement `error()` method:
   - Extract source line
   - Print filename and location: `main.aria:12:5: error: ...`
   - Print source line with line number
   - Print caret indicator: `    ^~~~~`
   - Use ANSI colors for visibility

**Phase 3: "Did You Mean" Suggestions (3 hours)**
1. Implement Levenshtein distance algorithm:
```cpp
size_t levenshteinDistance(const std::string& s1, const std::string& s2) {
   // Dynamic programming solution
   // Returns edit distance between strings
}
```

2. Implement `findClosestMatch()`:
   - Search known keywords/types for closest match
   - Threshold: distance ≤ 2
   - Return best suggestion

3. Integrate into parser error handling:
```cpp
if (current.type == TOKEN_IDENTIFIER) {
   std::string typo = current.lexeme;
   std::string suggestion = findClosestMatch(typo, allKeywords);
   if (!suggestion.empty()) {
       diag.help("Did you mean '" + suggestion + "'?");
   }
}
```

**Deliverables:**
- ✅ Errors show human-readable token names
- ✅ Source context with caret indicator
- ✅ ANSI color highlighting
- ✅ "Did you mean" suggestions for typos

**Testing:**
```aria
// Error: Expected ')' but got 'func'
vec4 v = vec4(1.0, 2.0, 3.0;

// Error: Unknown type 'vec5'. Did you mean 'vec4'?
var vec5:x;

// Error: Expected expression but got ';'
var int32:x = ;
```

**Example Output:**
```
main.aria:5:12: error: Unknown type 'vec5'
    var vec5:x;
        ^~~~~
help: Did you mean 'vec4'?
```

**Estimated Time:** 8 hours

---

## 📦 PACKAGE 002: ADVANCED FEATURES (Week 2-3)

**Priority:** HIGH - Enables type-safe libraries and functional programming  
**Estimated Effort:** 2 weeks  
**Dependencies:** Package 001 (optional - can work in parallel)

### Issue 1.3: Generic Template Instantiation
**Files:** `src/backend/codegen.cpp`, `src/backend/codegen_context.h`, `src/frontend/semantic.cpp`

**Strategy:** Monomorphization (C++/Rust style) - Generate specialized code at call sites

**Implementation Steps:**

**Phase 1: AST Changes (1 day)**
1. Add `GenericDecl` wrapper for `FuncDecl`:
```cpp
class GenericDecl {
   std::string name;
   std::vector<std::string> typeParams; // ["T", "U"]
   std::unique_ptr<FuncDecl> templateFunc;
};
```

2. Parser: Parse `func:max<T>(T:a, T:b) = T { ... }`
   - Store template AST without generating code

**Phase 2: Instantiation Registry (2 days)**
1. Create `GenericTemplate` registry in `CodeGenContext`:
```cpp
struct GenericTemplate {
   std::unique_ptr<FuncDecl> ast;
   std::map<std::vector<Type*>, Function*> specializations;
};

std::map<std::string, GenericTemplate> genericRegistry;
```

2. Implement `monomorphize()` function:
```cpp
Function* monomorphize(const std::string& name, 
                      const std::vector<Type*>& typeArgs) {
   // 1. Check cache
   auto& templ = genericRegistry[name];
   if (templ.specializations.count(typeArgs)) {
       return templ.specializations[typeArgs];
   }
   
   // 2. Clone AST
   auto* clonedAST = cloneAST(templ.ast);
   
   // 3. Type substitution
   TypeSubstitutionMap subst;
   for (size_t i = 0; i < typeArgs.size(); i++) {
       subst[templ.ast->typeParams[i]] = typeArgs[i];
   }
   applyTypeSubstitution(clonedAST, subst);
   
   // 4. Mangle name
   std::string mangledName = mangleName(name, typeArgs);
   // max<int8> → _Z3maxI2i8E
   
   // 5. Codegen
   Function* specialized = codegenFunction(clonedAST, mangledName);
   
   // 6. Cache
   templ.specializations[typeArgs] = specialized;
   return specialized;
}
```

**Phase 3: Call Site Instantiation (2 days)**
1. Update `visit(CallExpr)`:
   - If callee is generic, extract type arguments
   - Perform type inference if not explicit
   - Call `monomorphize()`
   - Replace call target with specialized function

2. Type inference algorithm:
```cpp
// max(5, 10) → infer T = int8
std::vector<Type*> inferTypeArgs(CallExpr* call, GenericDecl* generic) {
   // Unification algorithm
   // Match argument types to parameter types
}
```

**Phase 4: Name Mangling (1 day)**
1. Implement Itanium-style name mangling:
   - `max<int8>` → `_Z3maxI2i8E`
   - `swap<vec4, int32>` → `_Z4swapI4vec42i32E`

**Deliverables:**
- ✅ Generic functions compile without errors
- ✅ Call sites trigger instantiation
- ✅ Type inference works for simple cases
- ✅ Multiple specializations coexist
- ✅ No code bloat from duplicate specializations

**Estimated Time:** 6 days

---

### Issue 1.4: Lambda Closure Capture
**Files:** `src/frontend/ast.h`, `src/backend/codegen.cpp`

**Strategy:** Heap-allocated environment structs

**Implementation Steps:**

**Phase 1: Environment Generation (2 days)**
1. Analyze captured variables in lambda body
2. Generate environment struct:
```cpp
// Lambda captures x:int32, y:flt64
%LambdaEnv_1 = type { i32, double }
```

3. Allocate environment on heap:
```cpp
%env = call i8* @malloc(i64 16) ; sizeof(env)
%env_typed = bitcast i8* %env to %LambdaEnv_1*
```

4. Populate environment:
```cpp
%x_ptr = getelementptr %LambdaEnv_1, %LambdaEnv_1* %env_typed, i32 0, i32 0
store i32 %x_val, i32* %x_ptr
```

**Phase 2: Lambda Function Generation (2 days)**
1. Transform lambda to function with environment pointer:
```cpp
// Original: |x, y| { captured_var + x }
// Generated:
define i32 @lambda_impl_1(%LambdaEnv_1* %env, i32 %x, i32 %y) {
   %captured_ptr = getelementptr %LambdaEnv_1, %env, i32 0, i32 0
   %captured_val = load i32, i32* %captured_ptr
   %result = add i32 %captured_val, %x
   ret i32 %result
}
```

2. Update variable lookups in lambda body:
   - Captured variables → load from environment
   - Parameters → normal access

**Phase 3: Closure Object (1 day)**
1. Define closure type:
```cpp
%Closure = type { i8*, i8* } ; { function_ptr, env_ptr }
```

2. Create closure at lambda expression site:
```cpp
%closure = alloca %Closure
%fn_ptr = bitcast @lambda_impl_1 to i8*
%env_ptr = bitcast %env_typed to i8*
store i8* %fn_ptr, %closure.0
store i8* %env_ptr, %closure.1
```

**Phase 4: Closure Invocation (1 day)**
1. Update `visit(CallExpr)` to handle closures:
```cpp
if (calleeType == ClosureType) {
   Value* fnPtr = builder.CreateLoad(closure, 0);
   Value* envPtr = builder.CreateLoad(closure, 1);
   builder.CreateCall(fnPtr, {envPtr, args...});
}
```

**Deliverables:**
- ✅ Lambdas can capture variables
- ✅ Captured variables accessible in lambda body
- ✅ Closures can be returned from functions
- ✅ Closure invocation works correctly

**Known Limitation:** Memory leaks (no GC yet - acceptable for v0.0.7)

**Estimated Time:** 6 days

---

### Issue 1.5: Module System and UseStmt
**Files:** `src/frontend/parser.cpp`, `src/frontend/module.h/.cpp`, `src/main.cpp`

**Strategy:** Source-based importing + LLVM bitcode linking

**Implementation Steps:**

**Phase 1: pub Keyword (1 day)**
1. Add `TOKEN_KW_PUB` to lexer
2. Add `bool isPublic` field to all declaration AST nodes
3. Parser: Consume optional `pub` before declarations

**Phase 2: Module Loader (2 days)**
1. Create `ModuleLoader` class:
```cpp
class ModuleLoader {
   std::map<std::string, ModuleAST*> loadedModules;
   
public:
   ModuleAST* load(const std::string& modulePath);
   std::string resolvePath(const std::string& usePath);
};
```

2. Path resolution strategy:
   - `use ./utils` → Same directory
   - `use std.io` → `$ARIA_HOME/lib/std/io.aria`
   - `use core.types` → Project source root

**Phase 3: Symbol Exporter (2 days)**
1. Extract public symbols from parsed module:
```cpp
void exportSymbols(ModuleAST* module, SymbolTable& targetScope) {
   for (auto& decl : module->declarations) {
       if (decl->isPublic) {
           std::string mangledName = mangleModuleName(module->name, decl->name);
           targetScope.insert(mangledName, decl->type);
       }
   }
}
```

2. Handle circular dependency detection

**Phase 4: Build Pipeline (3 days)**
1. Create compilation driver:
```cpp
void compileProject(const std::string& mainFile) {
   std::queue<std::string> workQueue;
   std::set<std::string> compiled;
   
   workQueue.push(mainFile);
   
   while (!workQueue.empty()) {
       std::string file = workQueue.front();
       workQueue.pop();
       
       // Parse
       ModuleAST* ast = parseFile(file);
       
       // Queue dependencies
       for (auto& use : ast->useStatements) {
           std::string depPath = resolvePath(use);
           if (!compiled.count(depPath)) {
               workQueue.push(depPath);
           }
       }
       
       // Codegen to .bc
       compileToLLVM(ast, file + ".bc");
       compiled.insert(file);
   }
   
   // Link all .bc files
   linkModules(compiled);
}
```

**Phase 5: LLVM Linker Integration (1 day)**
1. Use `llvm::Linker` to merge bitcode files
2. Handle symbol conflicts (should be prevented by mangling)

**Deliverables:**
- ✅ `use` statements import modules
- ✅ `pub` controls symbol visibility
- ✅ Multi-file projects compile successfully
- ✅ Standard library bootstrap (std.io, std.math)

**Estimated Time:** 9 days

---

## 🔧 PACKAGE 003: RUNTIME INTEGRATION (Week 4-5)

**Priority:** MEDIUM - Connects existing runtime code  
**Estimated Effort:** 2 weeks  
**Dependencies:** Package 002 (modules help organize runtime code)

### Issue 2.1: Async/Await Scheduler Bridge
**Files:** `src/backend/codegen.cpp`, `src/runtime/concurrency/scheduler.cpp`

**Key Insight:** LLVM coroutines exist, work-stealing scheduler exists - just need to connect them

**Implementation Steps:**

**Phase 1: Task<T> Type (2 days)**
1. Define Task<T> wrapper:
```cpp
// src/runtime/concurrency/task.h
template<typename T>
struct Task {
   enum State { PENDING, RUNNING, COMPLETE, FAILED };
   
   alignas(64) std::atomic<State> state;
   void* awaiter_handle;     // LLVM coroutine handle
   void (*awaiter_resume_fn)(void*);
   T result;
   uint32_t flags; // HAS_WILD_AFFINITY, etc.
};
```

**Phase 2: Resume Thunks (2 days)**
1. Generate resume thunk at await points:
```cpp
void CodegenVisitor::visit(AwaitExpr* node) {
   // 1. Suspend coroutine
   builder.CreateCall(llvm_coro_suspend);
   
   // 2. Create resume thunk
   Function* resumeThunk = createResumeThunk(currentCoroutine);
   
   // 3. Enqueue to scheduler
   builder.CreateCall(@aria_scheduler_enqueue, {
       taskPtr,
       resumeThunk,
       coroHandle
   });
}

Function* createResumeThunk(Function* coro) {
   // Static trampoline: void resume(void* handle)
   Function* thunk = Function::Create(...);
   IRBuilder<> b(thunk);
   Value* handle = thunk->getArg(0);
   b.CreateCall(@llvm_coro_resume, handle);
   b.CreateRetVoid();
   return thunk;
}
```

**Phase 3: Wild Affinity (1 day)**
1. Track wild allocations in Task<T>
2. Prevent work stealing if `HAS_WILD_AFFINITY` flag set

**Deliverables:**
- ✅ `async` functions compile to LLVM coroutines
- ✅ `await` suspends and enqueues to scheduler
- ✅ Scheduler correctly resumes tasks
- ✅ Wild affinity prevents cross-thread execution

**Estimated Time:** 5 days

---

### Issue 2.3: SIMD Vector Operations
**Files:** `src/backend/codegen.cpp`

**Key Insight:** Types already map to LLVM vectors, just need proper instruction selection

**Implementation Steps:**

**Phase 1: Binary Operation Dispatch (2 days)**
1. Update `visit(BinaryOp)`:
```cpp
void CodegenVisitor::visit(BinaryOp* node) {
   node->left->accept(*this);
   Value* L = lastValue;
   node->right->accept(*this);
   Value* R = lastValue;
   
   // Handle broadcasting (scalar + vector)
   if (L->getType()->isVectorTy() && !R->getType()->isVectorTy()) {
       R = createSplat(R, L->getType()->getVectorNumElements());
   }
   
   // Dispatch based on element type
   Type* elemType = getVectorElementType(L->getType());
   if (elemType->isFloatingPointTy()) {
       switch (node->op) {
           case OP_ADD: lastValue = builder.CreateFAdd(L, R); break;
           case OP_MUL: lastValue = builder.CreateFMul(L, R); break;
           // ...
       }
   } else if (elemType->isIntegerTy()) {
       switch (node->op) {
           case OP_ADD: lastValue = builder.CreateAdd(L, R); break;
           case OP_MUL: lastValue = builder.CreateMul(L, R); break;
           // ...
       }
   }
}
```

**Phase 2: Intrinsic Functions (3 days)**
1. Implement dot product:
```cpp
Value* emitDotProduct(Value* A, Value* B) {
   // Element-wise multiply
   Value* mul = builder.CreateFMul(A, B);
   
   // Horizontal reduction (tree reduction for vec4)
   unsigned size = mul->getType()->getVectorNumElements();
   Value* tmp1 = builder.CreateShuffleVector(mul, mul, {2,3,undef,undef});
   Value* sum1 = builder.CreateFAdd(mul, tmp1); // X+Z, Y+W
   Value* tmp2 = builder.CreateShuffleVector(sum1, sum1, {1,undef,undef,undef});
   Value* dot = builder.CreateFAdd(sum1, tmp2);
   return builder.CreateExtractElement(dot, 0);
}
```

2. Implement cross product (vec3 only)
3. Implement normalize, length, distance

**Phase 3: Swizzling (2 days)**
1. Parser: Detect swizzles in member access
2. Codegen: Translate to shufflevector:
```cpp
// v.wzyx → shufflevector <4 x float> %v, <4 x float> undef, <3,2,1,0>
// v.xy → shufflevector <4 x float> %v, <4 x float> undef, <0,1>
```

**Deliverables:**
- ✅ Vector arithmetic generates SIMD instructions
- ✅ Broadcasting works (vec4 + float)
- ✅ dot(), cross(), normalize() intrinsics
- ✅ Swizzling (v.xyzw, v.rgba)

**Estimated Time:** 7 days

---

### Issue 3.1: GC Nursery Integration
**Files:** `src/backend/codegen.cpp`, `src/runtime/gc/nursery.cpp`

**Key Insight:** Nursery allocator exists, just needs to be called

**Implementation Steps:**

**Phase 1: TLS Setup (1 day)**
1. Initialize thread-local nursery on worker threads:
```cpp
// src/runtime/concurrency/scheduler.cpp
void Worker::init() {
   current_nursery = new Nursery(4 * 1024 * 1024); // 4MB
   gc_register_nursery(current_nursery);
}
```

**Phase 2: Inline Fast Path (2 days)**
1. Codegen for allocations:
```cpp
void CodegenVisitor::visit(AllocExpr* node) {
   // Load TLS pointers
   Value* bumpPtr = builder.CreateLoad(@current_nursery.bump_ptr);
   Value* limitPtr = builder.CreateLoad(@current_nursery.limit_ptr);
   
   // Tentative increment
   Value* newBump = builder.CreateGEP(bumpPtr, size);
   
   // Branch on overflow
   Value* fits = builder.CreateICmpULE(newBump, limitPtr);
   BasicBlock* fastPath = BasicBlock::Create("alloc.fast");
   BasicBlock* slowPath = BasicBlock::Create("alloc.slow");
   builder.CreateCondBr(fits, fastPath, slowPath);
   
   // Fast path: bump allocation (~5 instructions)
   builder.SetInsertPoint(fastPath);
   builder.CreateStore(newBump, @current_nursery.bump_ptr);
   Value* ptr = bumpPtr;
   // ...
   
   // Slow path: call runtime
   builder.SetInsertPoint(slowPath);
   Value* slowPtr = builder.CreateCall(@aria_gc_alloc_slow, size);
   // ...
}
```

**Phase 3: Write Barriers (2 days)**
1. Inject card marking on pointer writes:
```cpp
void CodegenVisitor::visit(AssignmentOp* node) {
   // ... normal assignment logic ...
   
   // If LHS is GC object and RHS is pointer
   if (isGCManaged(lhs) && isPointerType(rhs)) {
       // Card marking
       Value* cardIndex = builder.CreateLShr(lhsAddr, 9); // Divide by 512
       Value* cardTable = builder.CreateLoad(@gc_card_table);
       Value* cardPtr = builder.CreateGEP(cardTable, cardIndex);
       builder.CreateStore(ConstantInt::get(i8, 1), cardPtr);
   }
}
```

**Phase 4: Shadow Stack (2 days)**
1. Generate stack frame push/pop:
```cpp
void CodegenVisitor::visit(FuncDecl* node) {
   // Function entry
   builder.CreateCall(@aria_gc_push_frame);
   
   // Register GC roots
   for (auto& local : gcManagedLocals) {
       builder.CreateCall(@aria_gc_register_root, local);
   }
   
   // ... function body ...
   
   // Function exit
   builder.CreateCall(@aria_gc_pop_frame);
}
```

**Deliverables:**
- ✅ Allocations use nursery bump pointer
- ✅ Minor GC triggers on overflow
- ✅ Write barriers track old→young pointers
- ✅ Stack scanning finds roots

**Estimated Time:** 7 days

---

## 🐛 PACKAGE 004: DEVELOPER EXPERIENCE (Week 6-7)

**Priority:** MEDIUM - Debugging and usability  
**Estimated Effort:** 2 weeks  
**Dependencies:** Package 001 (struct methods use vector types)

### Issue 4.1: Struct Methods
**Files:** `src/frontend/parser_struct.cpp`, `src/backend/codegen.cpp`

**Implementation Steps:**

**Phase 1: Parser (2 days)**
1. Update `parseStructDecl()`:
   - Detect `func` keyword inside struct block
   - Parse method signature with `self` parameter
   - Store in `StructDecl::instance_methods` or `static_methods`

**Phase 2: Name Mangling (1 day)**
1. Mangle method names: `Point.distance` → `_Aria_Point_distance`

**Phase 3: Semantic Analysis (2 days)**
1. Transform method calls:
   - `p.distance()` → `_Aria_Point_distance(p)`
   - Move object to first argument position

**Phase 4: Codegen (2 days)**
1. Generate methods as regular functions:
   - First parameter is struct (by value) or pointer
   - No vtables (static dispatch only)

**Deliverables:**
- ✅ Methods defined inside structs
- ✅ Method calls use dot syntax
- ✅ `self` parameter works correctly

**Estimated Time:** 7 days

---

### Issue 5.2: Runtime Stack Traces
**Files:** `src/backend/codegen.cpp`, `src/runtime/debug/stacktrace.cpp`

**Implementation Steps:**

**Phase 1: DWARF Generation (3 days)**
1. Integrate `llvm::DIBuilder`:
   - Create `DICompileUnit` for each file
   - Create `DISubprogram` for each function
   - Emit debug locations before each instruction

**Phase 2: libunwind Integration (2 days)**
1. Implement `print_stacktrace()`:
```cpp
void print_stacktrace() {
   unw_cursor_t cursor;
   unw_context_t context;
   unw_getcontext(&context);
   unw_init_local(&cursor, &context);
   
   while (unw_step(&cursor) > 0) {
       unw_word_t pc;
       unw_get_reg(&cursor, UNW_REG_IP, &pc);
       
       char sym[256];
       if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
           fprintf(stderr, "  at %s+0x%lx [0x%lx]\n", sym, offset, pc);
       }
   }
}
```

**Phase 3: Signal Handler (1 day)**
1. Install signal handlers for SIGSEGV, SIGABRT, etc.
2. Call `print_stacktrace()` on crash

**Deliverables:**
- ✅ Crashes print stack traces
- ✅ Function names appear in traces
- ✅ Works with async/spawn

**Estimated Time:** 6 days

---

### Issue 2.4: Fat Pointer Runtime Checks
**Files:** `src/backend/codegen.cpp`, `src/runtime/debug/fat_pointer.cpp`

**Implementation Steps:**

**Phase 1: Metadata Table (2 days)**
1. Implement sharded hash map:
```cpp
struct FatPointerMetadata {
   uintptr_t base_addr;
   size_t size;
   uint32_t scope_id;
   bool is_freed;
};

const int NUM_SHARDS = 64;
std::mutex shard_locks[NUM_SHARDS];
std::unordered_map<void*, FatPointerMetadata> shards[NUM_SHARDS];
```

**Phase 2: Codegen Instrumentation (3 days)**
1. Inject checks on wild pointer operations:
```cpp
// Allocation
%ptr = call i8* @aria_alloc(i64 %size)
%scope = call i32 @aria_get_current_scope()
call void @aria_fat_ptr_create(i8* %ptr, i32 %scope, i64 %size)

// Dereference
call void @aria_fat_ptr_validate(i8* %ptr, i8* %filename, i32 %line)
%val = load i32, i32* %ptr

// Scope management
call void @aria_fat_ptr_scope_enter()
// { block }
call void @aria_fat_ptr_scope_exit()
```

**Phase 3: Runtime Checks (2 days)**
1. Implement validation:
```cpp
void aria_fat_ptr_validate(void* ptr, const char* file, int line) {
   auto* meta = lookup_metadata(ptr);
   if (!meta) {
       abort_with_trace("Use of untracked pointer at %s:%d", file, line);
   }
   if (meta->is_freed) {
       abort_with_trace("Use-after-free at %s:%d", file, line);
   }
   if (!is_scope_valid(meta->scope_id)) {
       abort_with_trace("Use-after-scope at %s:%d", file, line);
   }
}
```

**Deliverables:**
- ✅ Debug builds track wild pointers
- ✅ Detect use-after-free
- ✅ Detect use-after-scope
- ✅ Detect buffer overflows

**Estimated Time:** 7 days

---

## 🎯 PACKAGE 005: QUALITY & OPTIMIZATION (Week 8-10)

**Priority:** LOW - Polish and advanced features  
**Estimated Effort:** 3 weeks  
**Dependencies:** All previous packages

### Issue 2.2: TBB Optimizer
### Issue 3.2: Platform Abstraction Layer  
### Issue 4.2: Trait System

**Note:** These are lower priority. Implement after core functionality is stable.

**Estimated Time:** 3 weeks total

---

## 📈 IMPLEMENTATION TIMELINE

```
Week 1: Package 001 (Parser Foundation)
├─ Day 1-2: Vector types + error messages
├─ Day 3-5: Vector literals
└─ Complete: 3 issues ✅

Week 2-3: Package 002 (Advanced Features)
├─ Day 1-6: Generic instantiation
├─ Day 7-12: Lambda closures
└─ Day 13-14: Module system
└─ Complete: 3 issues ✅

Week 4-5: Package 003 (Runtime Integration)
├─ Day 1-5: Async/await bridge
├─ Day 6-12: SIMD operations
└─ Day 13-14: GC nursery
└─ Complete: 3 issues ✅

Week 6-7: Package 004 (Developer Experience)
├─ Day 1-7: Struct methods
├─ Day 8-13: Stack traces
└─ Day 14: Fat pointers
└─ Complete: 3 issues ✅

Week 8-10: Package 005 (Quality)
├─ TBB optimizer
├─ Platform abstraction
└─ Trait system
└─ Complete: 3 issues ✅
```

---

## 🧪 TESTING STRATEGY

### Unit Tests (Per Package)
- **Parser:** Verify AST structure for new syntax
- **Codegen:** Check LLVM IR output with FileCheck
- **Runtime:** C++ GTest for runtime libraries

### Integration Tests
- **Graphics Demo:** Vector math, swizzling, SIMD
- **Async Network Server:** Task scheduling, wild affinity
- **Multi-Module Project:** Standard library usage
- **Memory Safety:** Intentional bugs caught by fat pointers

### Performance Benchmarks
- **Vector Operations:** Compare scalar vs SIMD performance
- **GC Throughput:** Allocation-heavy workload vs mi_malloc
- **Generic Instantiation:** Compile time and code size

---

## 🎯 SUCCESS CRITERIA

**Package 001:**
- ✅ Graphics-style code compiles: `var vec4:color = vec4(1,0,0,1);`
- ✅ Errors are readable with source context and suggestions

**Package 002:**
- ✅ Generic `max<T>()` works for all types
- ✅ Closures capture variables correctly
- ✅ Multi-file projects build successfully

**Package 003:**
- ✅ Async functions execute on scheduler
- ✅ Vector math generates SIMD instructions
- ✅ GC allocation is 10x+ faster than malloc

**Package 004:**
- ✅ OOP-style struct methods work
- ✅ Crashes show function names and locations
- ✅ Debug builds catch memory errors

**Package 005:**
- ✅ TBB loops eliminate redundant checks
- ✅ Compiles on Linux, Windows, macOS
- ✅ Trait-based polymorphism works

---

## 📝 NOTES

**Code Quality:**
- Follow existing Aria codebase style
- Add comments for complex algorithms (especially LLVM IR generation)
- Keep functions under 100 lines where possible

**Documentation:**
- Update language spec with new features
- Add examples to docs/examples/
- Create migration guide for existing code

**Backward Compatibility:**
- All changes are additive (no breaking changes)
- Old code continues to compile

**Future Work (v0.0.8+):**
- Memory management for closures (GC/ARC)
- Advanced trait features (associated types, default impls)
- Cross-module generic instantiation optimization
- Profile-guided optimization for TBB checks

---

**Last Updated:** December 8, 2025  
**Status:** Ready to begin implementation  
**Next Step:** Start Package 001, Issue 1.1 (Vector Type Keywords)
