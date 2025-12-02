# Phase 1: Foundation

**Duration:** 5 weeks  
**Dependencies:** None  
**Goal:** Get fundamental systems working independently

---

## Overview

Phase 1 focuses on building the core infrastructure that everything else depends on:
1. **Complete Lexer** - Tokenize all Aria syntax
2. **Build Preprocessor** - Macro expansion and conditional compilation
3. **Build Comptime Evaluator** - Compile-time computation

These three components have **no dependencies on each other** and can be developed in parallel if needed, but we'll do them sequentially for quality.

---

## Tasks

### ✅ Task 0: Planning & Analysis
**Status:** Complete  
**Duration:** 1 day (Dec 2, 2025)

**Deliverables:**
- [x] IMPLEMENTATION_STATUS.md
- [x] IMPLEMENTATION_DETAILS.md
- [x] Planning infrastructure
- [x] All task breakdowns

---

### Task 1: Complete Lexer
**Status:** 🟢 Ready to Start  
**Duration:** 1 week  
**Assignee:** To be started  
**Dependencies:** None

**Objective:**  
Extend the existing lexer (currently 95% complete) to handle ALL token types from the v0.0.6 spec.

**Current State:**
- ✅ 243 token types defined
- ✅ Keywords recognized
- ✅ Decimal integer literals
- ✅ Basic string literals
- ✅ Template strings
- ✅ Comments
- ✅ All operators

**Missing:**
- ❌ Hex literals (0xFF, 0xDEADBEEF)
- ❌ Binary literals (0b1010, 0b11111111)
- ❌ Octal literals (0o755, 0o644)
- ❌ Float literals (3.14, 1e10, 0x1.2p3)
- ❌ Character literals ('a', '\n', '\x41')
- ❌ Preprocessor directive tokens (%, %macro, etc.)

**Deliverables:**
- [ ] Extended lexer.cpp with all literal types
- [ ] Comprehensive test suite (tests/test_lexer.cpp)
- [ ] All spec examples tokenize correctly
- [ ] Zero regressions in existing tests

**Success Criteria:**
1. All number formats from spec parse correctly
2. All character escapes work
3. Preprocessor tokens recognized (for future preprocessor)
4. 100% of spec code examples tokenize without error
5. All tests pass

**See:** [tasks/task_01_complete_lexer.md](../tasks/task_01_complete_lexer.md)

---

### Task 2: Build Preprocessor
**Status:** ⏳ Waiting (depends on Task 1)  
**Duration:** 2 weeks  
**Dependencies:** Task 1 (needs complete tokenization)

**Objective:**  
Implement NASM-style preprocessor with macro expansion, context stack, and conditional compilation.

**Deliverables:**
- [ ] src/frontend/preprocessor.h
- [ ] src/frontend/preprocessor.cpp
- [ ] src/frontend/macro.h
- [ ] src/frontend/macro_expander.cpp
- [ ] tests/test_preprocessor.cpp
- [ ] tests/macros/ (test macro files)

**Features:**
- Macro definition and expansion (%macro/%endmacro)
- Context stack (%push/%pop/%context)
- Parameter substitution (%1, %2, ...)
- Context-local labels (%$label)
- Conditional compilation (%ifdef, %if, %elif, %else, %endif)
- Repeat blocks (%rep/%endrep)
- File inclusion (%include)
- Constant definition (%define/%undef)

**Success Criteria:**
1. All spec section 5.2 examples work
2. Nested macros expand correctly
3. Context stack prevents label conflicts
4. Circular includes detected
5. All tests pass

**See:** [tasks/task_02_build_preprocessor.md](../tasks/task_02_build_preprocessor.md)

---

### Task 3: Build Comptime Evaluator
**Status:** ⏳ Waiting (depends on Task 2)  
**Duration:** 2 weeks  
**Dependencies:** Task 2 (preprocessor done), AST (already exists)

**Objective:**  
Implement Zig-style comptime evaluation for compile-time computation and type generation.

**Deliverables:**
- [ ] src/frontend/sema/comptime.h
- [ ] src/frontend/sema/comptime.cpp
- [ ] src/frontend/sema/comptime_interpreter.cpp
- [ ] tests/test_comptime.cpp
- [ ] tests/comptime/ (test files)

**Features:**
- Comptime expression evaluation
- Comptime function interpretation
- Comptime variable management
- Type-as-value computation
- Purity checking
- Integration with parser for array sizes and type expressions

**Success Criteria:**
1. All spec section 5.3 examples work
2. Comptime functions execute at compile time
3. Type selection works (selectIntType example)
4. Non-pure functions rejected
5. Array sizes computed correctly
6. All tests pass

**See:** [tasks/task_03_build_comptime.md](../tasks/task_03_build_comptime.md)

---

## Phase Completion Criteria

Phase 1 is complete when:
1. ✅ All 3 tasks marked complete
2. ✅ All tests passing
3. ✅ All spec examples from sections 5.2, 5.3 work
4. ✅ Zero known bugs
5. ✅ Documentation updated

**Next Phase:** [Phase 2: Parser Completion](./phase2_parser.md)
