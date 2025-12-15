# ARIA TODO.TXT - COMPREHENSIVE REVIEW & ANALYSIS
Date: December 14, 2025
Status: **CRITICAL ISSUES FOUND - NEEDS UPDATES**

## EXECUTIVE SUMMARY

The current todo.txt is **INCOMPLETE** and has several missing critical elements:

### 🔴 CRITICAL ISSUES
1. **TODO IS TRUNCATED** - Only Phase 0 and Phase 1 exist, need Phases 2-7
2. **MISSING TOKENS** - Token enum missing several required tokens from specs
3. **MISSING KEYWORDS** - Several keywords from specs not in token list
4. **INCOMPLETE OPERATORS** - Template interpolation tokens missing
5. **NO CLEANUP STEP** - .o files in root need cleaning before we start

### ⚠️  MODERATE ISSUES  
1. **TOKEN_KW_AWAIT missing** - Specs show AWAIT keyword
2. **TOKEN_KW_CATCH missing** - Specs show CATCH keyword
3. **TOKEN_KW_CFG missing** - Specs show CFG for conditional compilation
4. **TOKEN_INTERP_START missing** - For &{ in templates
5. **TOKEN_INTERP_END missing** - For } in templates
6. **TOKEN_KW_LOOP missing** - loop() construct from specs
7. **Additional type keywords** - BINARY, BUFFER, STREAM, PROCESS, PIPE, DEBUG, LOG

### ✅ STRENGTHS
1. Research references are accurate and complete
2. Directory structure is correct (src/, tests/, examples/)
3. Testing strategy is comprehensive
4. Modular philosophy is sound
5. Git workflow is well defined

---

## DETAILED ANALYSIS

### 1. MISSING TOKENS IN ENUM (Section 1.1.1)

#### Missing Keywords:
```cpp
// Missing from current token list:
TOKEN_KW_AWAIT,      // async/await system [research_029]
TOKEN_KW_CATCH,      // error handling
TOKEN_KW_CFG,        // conditional compilation [research_028]
TOKEN_KW_LOOP,       // loop(start, limit, step) [research_018]

// Additional type keywords from specs:
TOKEN_KW_BINARY,     // binary data type
TOKEN_KW_BUFFER,     // buffer type
TOKEN_KW_STREAM,     // stream type [research_006]
TOKEN_KW_PROCESS,    // process type [research_005]
TOKEN_KW_PIPE,       // pipe type [research_005]
TOKEN_KW_DEBUG,      // debug type
TOKEN_KW_LOG,        // log type
```

#### Missing Operator Tokens:
```cpp
// Template interpolation (critical for string templates):
TOKEN_INTERP_START,  // &{ (start of interpolation)
TOKEN_INTERP_END,    // } (end of interpolation, contextual)
TOKEN_AMPERSAND_LBRACE, // Alternative: explicit &{ token

// Note: TOKEN_AMPERSAND and TOKEN_LEFT_BRACE already exist,
// but need TOKEN_INTERP_START/END for proper template parsing
```

#### Literal Tokens:
```cpp
// Current list has:
// - Integer, Float, String, Template, Boolean, Null, Char
// But needs explicit:
TOKEN_LITERAL_INTEGER,
TOKEN_LITERAL_FLOAT,
TOKEN_LITERAL_STRING,
TOKEN_LITERAL_TEMPLATE,
TOKEN_LITERAL_BOOLEAN,
TOKEN_LITERAL_NULL,
TOKEN_LITERAL_CHAR,
```

### 2. MISSING PHASES (Major Gap)

**Current:** Only Phase 0 (Infrastructure) and Phase 1 (Lexer)

**Missing Phases:**

#### PHASE 2: PARSER & AST
Reference: [research_012-020, research_027-028]
- Recursive descent parser
- AST node definitions for all constructs
- Expression parsing (precedence climbing)
- Statement parsing
- Type annotation parsing
- Module/use statement parsing
- Error recovery in parser
- Parser tests

#### PHASE 3: SEMANTIC ANALYSIS
Reference: [research_001, research_012-017, research_027-028]
- Symbol table implementation
- Type checking
- Type inference for `dyn` and generics
- Borrow checker integration (lifetime analysis)
- Module resolution
- Generic monomorphization
- Semantic error reporting
- Semantic analysis tests

#### PHASE 4: IR GENERATION
Reference: All research docs for complete language semantics
- LLVM IR generation
- Type mapping (Aria types -> LLVM types)
- TBB type implementation (symmetric ranges + ERR sentinel)
- Function code generation
- Expression code generation
- Control flow translation
- Memory model implementation (gc/wild/stack)
- IR optimization passes
- IR generation tests

#### PHASE 5: RUNTIME IMPLEMENTATION
Reference: [research_004-009, research_021-023]
- Garbage collector [research_021]
- Wild memory allocator [research_022]
- WildX executable allocator [research_022, research_023]
- Runtime assembler for wildx [research_023]
- Process management [research_005]
- Modern I/O streams (6 channels) [research_006]
- Threading library [research_007]
- Atomics [research_008]
- Timer/clock [research_009]
- Async runtime [research_029]
- Runtime tests

#### PHASE 6: STANDARD LIBRARY
Reference: [research_031, research_004-009]
- Core types implementation
- aria.alloc/free/gc_alloc functions
- File I/O (readFile, writeFile, etc.)
- Process functions (spawn, fork, exec, createPipe)
- Functional programming (filter, map, reduce)
- Math library
- String operations
- HTTP client basics
- Stdlib tests

#### PHASE 7: COMPILER DRIVER & TOOLING
- Command-line interface
- Multi-file compilation
- Linker integration
- Error reporting and diagnostics
- LSP server basics (for vscode-aria)
- Build system integration
- Package manager foundations
- Documentation generator

### 3. DIRECTORY STRUCTURE ISSUES

**Current Root Has Build Artifacts:**
```
test_break_final.o
test_cn_final.o
test_comp_final.o
... (17 .o files)
```

**Action Required:** Add cleanup step to Phase 0:
```
[ ] 0.0.1: Clean repository root
    File: ./
    Action: Remove all .o build artifacts
    Command: find . -maxdepth 1 -name "*.o" -delete
    Reason: Keep root clean before starting fresh
    Test: ls *.o returns no results
```

### 4. RESEARCH DOCUMENT MAPPING

**Verify all research docs are properly referenced:**

✅ **Correctly Referenced:**
- [research_001] - Borrow checker (Phase 3)
- [research_002] - TBB arithmetic (Phase 1, 4)
- [research_003] - Balanced nonary (Phase 1, 4)
- [research_012] - Standard integers (Phase 1, 3, 4)

❌ **Missing References in TODO:**
- [research_013] - Floating point (needed in Phase 1.1.1, 3, 4)
- [research_014] - Composite types part 1 (needed in Phase 2, 3)
- [research_015] - Composite types part 2 (needed in Phase 2, 3)
- [research_016] - Functional types (needed in Phase 2, 3)
- [research_017] - Mathematical types (needed in Phase 1.1.1, 3, 4)
- [research_018] - Looping constructs (needed in Phase 2)
- [research_019] - Conditionals (needed in Phase 2)
- [research_020] - Control transfer (needed in Phase 2, 3)
- [research_021] - GC system (needed in Phase 5)
- [research_022] - Wild/WildX (needed in Phase 4, 5)
- [research_023] - Runtime assembler (needed in Phase 5)
- [research_024] - Arithmetic/bitwise ops (needed in Phase 2, 4)
- [research_025] - Comparison/logical ops (needed in Phase 2, 4)
- [research_026] - Special operators (needed in Phase 1, 2)
- [research_027] - Generics (needed in Phase 3, 4)
- [research_028] - Module system (needed in Phase 2, 3)
- [research_029] - Async/await (needed in Phase 5)
- [research_030] - Const/comptime (needed in Phase 3, 4)
- [research_031] - Essential stdlib (needed in Phase 6)

### 5. TESTING INFRASTRUCTURE

**Current Plan:** ✅ Good foundation
- Unit tests
- Integration tests  
- Benchmarks

**Missing:**
- [ ] Fuzzing infrastructure (add to Phase 0)
- [ ] Memory leak detection setup (valgrind/ASAN)
- [ ] Coverage reporting setup (lcov/gcov)

### 6. TOKEN DISAMBIGUATION DETAILS

**Template Literals Need Special Handling:**

The specs show template literals use `backticks` with `&{expr}` interpolation.

**Problem:** How to distinguish:
- `&` (bitwise AND) vs `&{` (interpolation start)
- `{` (left brace) vs `{` (inside interpolation)

**Solution in 1.2.7:**
Should track lexer state:
```cpp
enum LexerState {
    NORMAL,
    IN_TEMPLATE,
    IN_INTERPOLATION
};
```

When lex ` (backtick):
- Enter IN_TEMPLATE state
- Return TOKEN_BACKTICK

In IN_TEMPLATE state:
- If see `&{` -> push IN_INTERPOLATION state, return TOKEN_INTERP_START
- If see ``` -> pop to NORMAL, return TOKEN_BACKTICK
- Otherwise lex as template text

In IN_INTERPOLATION state:
- Lex tokens normally
- Track brace depth
- When closing } at depth 0 -> pop state, return TOKEN_INTERP_END

**Action:** Section 1.2.7 needs state machine specification added.

### 7. OPERATOR PRECEDENCE (Missing)

**Not in current TODO but CRITICAL for Phase 2:**

Need to add to Phase 2 (Parser):
```
[ ] 2.1.1: Define operator precedence table
    File: include/frontend/parser/precedence.h
    Reference: [research_024], [research_025], [research_026]
    
    Precedence levels (low to high):
    1. Pipeline: |>, <|
    2. Null coalescing: ??
    3. Logical OR: ||
    4. Logical AND: &&
    5. Comparison: ==, !=, <, >, <=, >=, <=>
    6. Range: .., ...
    7. Bitwise OR: |
    8. Bitwise XOR: ^
    9. Bitwise AND: &
    10. Shift: <<, >>
    11. Addition: +, -
    12. Multiplication: *, /, %
    13. Unary: -, !, ~
    14. Postfix: ++, --, ., ->, ?., [], ()
    15. Primary: literals, identifiers, (expr)
```

### 8. MODULE SYSTEM COMPLEXITY

**Specs show complex module features:**
- Relative paths: `use "./utils.aria"`
- Absolute paths: `use "/usr/lib/aria/graphics"`
- Selective imports: `use std.collections.{array, map}`
- Wildcards: `use math.*`
- Aliases: `use "./utils.aria" as utils`
- Conditional: `use cfg(target_os = "linux") std.os.linux`
- External C: `extern "libc" { ... }`

**TODO needs Phase 2 module parsing section** with all these cases.

### 9. EXAMPLE FILES NEEDED

**Not in TODO but should add:**

```
[ ] 0.3: Create example files
    Purpose: Have test cases ready from day one
    
    [ ] 0.3.1: examples/01_hello_world.aria
        Basic print statement
        
    [ ] 0.3.2: examples/02_variables.aria
        All type declarations
        
    [ ] 0.3.3: examples/03_functions.aria
        Function definitions, calls, closures
        
    [ ] 0.3.4: examples/04_control_flow.aria
        if, while, for, loop, till, when, pick
        
    [ ] 0.3.5: examples/05_memory.aria
        gc, wild, stack allocations
        
    [ ] 0.3.6: examples/06_modules.aria
        use statements, module definitions
        
    [ ] 0.3.7: examples/07_generics.aria
        Generic functions, type inference
        
    [ ] 0.3.8: examples/08_async.aria
        Async functions, await
        
    [ ] 0.3.9: examples/09_tbb.aria
        TBB arithmetic, ERR handling
        
    [ ] 0.3.10: examples/10_complete_app.aria
        Full application demonstrating multiple features
```

---

## RECOMMENDATIONS

### IMMEDIATE ACTIONS (Before Phase 0 starts):

1. **Add Phase 0.0: Repository Cleanup**
   - Remove .o files from root
   - Verify archive/ is complete
   - Check for any other stray build artifacts

2. **Update Section 1.1.1: Complete Token Enum**
   - Add missing keywords (AWAIT, CATCH, CFG, LOOP)
   - Add missing type keywords (BINARY, BUFFER, STREAM, etc.)
   - Add interpolation tokens (INTERP_START, INTERP_END)
   - Explicitly list all literal tokens

3. **Update Section 1.2.7: Template Literal State Machine**
   - Add lexer state machine specification
   - Define state transitions
   - Clarify interpolation brace tracking

4. **Add Phases 2-7** (Full compiler pipeline)
   - Phase 2: Parser & AST
   - Phase 3: Semantic Analysis
   - Phase 4: IR Generation
   - Phase 5: Runtime Implementation
   - Phase 6: Standard Library
   - Phase 7: Compiler Driver & Tooling

5. **Add Phase 0.3: Example Files**
   - Create 10 example .aria files
   - Cover all language features
   - Use for integration testing

6. **Add Testing Enhancements to Phase 0.2**
   - Fuzzing setup
   - Memory leak detection
   - Coverage reporting

### LONG-TERM IMPROVEMENTS:

1. **Detailed timelines** - Estimate hours per task
2. **Dependency tracking** - Which tasks block others
3. **Success criteria** - More specific "DONE" definitions
4. **Performance targets** - Beyond just lexer benchmark
5. **Documentation tasks** - When to write docs
6. **Code review checkpoints** - When to pause for review

---

## CONCLUSION

The TODO is a **strong foundation** but **critically incomplete**. It needs:

### Must-Have Before Starting:
- ✅ Research foundation complete
- ✅ Directory structure defined
- ✅ Git workflow clear
- ❌ **Complete token list** (missing ~10 tokens)
- ❌ **Repository cleanup** (remove .o files)
- ❌ **Full phase list** (Phases 2-7 missing)
- ❌ **Example files** (none planned)

### Current State:
- **Phase 0:** 95% complete (just needs cleanup step)
- **Phase 1:** 90% complete (missing tokens, state machine detail)
- **Phases 2-7:** 0% complete (not yet written)

### Risk Assessment:
- 🔴 **HIGH RISK:** Starting without Phases 2-7 defined will cause scope creep
- 🟡 **MEDIUM RISK:** Missing tokens will require lexer refactor later
- 🟢 **LOW RISK:** Foundation (Phase 0) is solid

### Recommendation:
**DO NOT START** implementation until:
1. Phases 2-7 are written (at same detail level as Phase 1)
2. Complete token list verified against specs
3. Repository cleaned
4. Example files created

**Estimated Time to Complete TODO:**
- Add missing sections: 4-6 hours
- Review and validate: 2 hours
- **Total: 6-8 hours of planning**

This upfront planning will save **weeks** of refactoring later.

---

## NEXT STEPS

1. **Update todo.txt** with all missing elements
2. **Create examples/** directory with 10 starter files
3. **Run cleanup** (remove .o files)
4. **Review updated TODO** one final time
5. **Begin Phase 0** implementation

Once TODO is complete, we'll have a **bulletproof roadmap** for building Aria from scratch with zero guesswork.
