# Aria Compiler Development Roadmap
**Date**: December 7, 2025
**Current Version**: v0.0.6
**Status**: 93-95% Complete

---

## 🎯 PHASE 1: TSODING-READY (Current Phase)
**Goal**: Make repo presentable, demonstrate what works, get feedback from Tsoding
**Timeline**: ASAP (waiting on Tsoding response)

### ✅ Completed
- [x] Lexer (100%)
- [x] Preprocessor (100% - 16 directives, macro expansion)
- [x] Parser (95% - 13/14 features)
- [x] LLVM Backend (93%)
- [x] Collections library (130 functions)
- [x] Math library (24 functions)
- [x] Programming guide documentation
- [x] Fixed auto-wrap bug (Dec 4, 2025)
- [x] Fixed result keyword issues
- [x] Identified macro variable bug

### 🔄 In Progress (This Week)
- [ ] **Test unknown stdlib files** (math_core, math_v2, string_core, math_advanced, string_manipulation)
- [ ] **Create test suite** for 154 working functions
- [ ] **Document all working features** with examples
- [ ] **Clean up repo** (organize test files, remove debug artifacts)
- [ ] **Create demo programs** showcasing Aria's strengths
- [ ] **Write README.md** with quick start guide
- [ ] **Prepare examples/** directory with best-of-the-best code samples

### 📦 Deliverables for Tsoding
1. Clean, organized repo with clear structure
2. Working demo programs (collections, math operations, real-world examples)
3. Comprehensive documentation (programming guide, API reference)
4. Test suite proving stability of implemented features
5. KNOWN_ISSUES.md listing blockers with workarounds
6. Clear value proposition: "Turing-complete preprocessor + LLVM backend"

---

## 🚀 PHASE 2: NIKOLA-READY (Post-Tsoding)
**Goal**: Production-ready compiler for AI training dataset generation
**Timeline**: After Tsoding feedback incorporated
**Critical**: Nikola requires stable, bug-free Aria as its native training language

### 🔧 Compiler Fixes Required

#### Priority 1: Blocking stdlib (Required for Nikola)
- [ ] **Implement pointer types** (`@` operator)
  - Currently blocks: io.aria (file operations)
  - Syntax: `uint8@:buffer` for pointer declarations
  - Need: Address-of, dereference, pointer arithmetic
  
- [ ] **Implement wildx type** (executable memory allocator)
  - Currently blocks: mem.aria (dynamic allocation)
  - Syntax: `wildx uint8@` for executable pointers
  - Need: Runtime support for executable memory regions

- [ ] **Fix macro variable scoping bug**
  - Currently blocks: bit_operations.aria (136 functions)
  - Bug: `%1:varname` in function body conflicts across invocations
  - Need: Proper variable scoping in macro expansion
  - Workaround exists but needs implementation

- [ ] **Fix string.aria LLVM codegen bug**
  - Error: "Terminator found in the middle of a basic block"
  - Affects: All string manipulation functions
  - Need: Debug control flow IR generation

#### Priority 2: Language completeness
- [ ] **Arrays and object literals**
  - Currently deferred in parser
  - Syntax: `[1, 2, 3]` for array literals
  - Need: Parser support + LLVM codegen

- [ ] **Semantic analyzer**
  - Currently missing entirely
  - Need: Type checking, scope validation, dead code detection
  - Critical for production use

- [ ] **Enhanced error messages**
  - Current: Token-level errors (col/line)
  - Need: Context-aware suggestions, multi-line error display

#### Priority 3: Standard library expansion
- [ ] **Complete I/O library** (after pointers implemented)
  - File operations: read, write, seek, close
  - Buffered I/O for performance
  - Error handling with result types

- [ ] **Complete memory library** (after wildx implemented)
  - Allocators: malloc, calloc, realloc, free
  - Executable memory: alloc_exec, protect_exec
  - Memory utilities: memcpy, memset, memcmp

- [ ] **Complete bit operations** (after macro bug fixed)
  - Population count, leading/trailing zeros
  - Bit rotation, reversal, parity
  - All integer types (17 functions × 8 types = 136 functions)

- [ ] **Complete string library** (after LLVM bug fixed)
  - String manipulation, searching, formatting
  - UTF-8 support
  - String interpolation integration

- [ ] **New stdlib categories**:
  - [ ] Network operations (sockets, HTTP)
  - [ ] File system operations (paths, directories)
  - [ ] Date/time utilities
  - [ ] Hashing and crypto
  - [ ] Regex support
  - [ ] JSON parsing
  - [ ] Command-line argument parsing

#### Priority 4: Performance and optimization
- [ ] **LLVM optimization passes**
  - Current: Basic IR generation
  - Need: Dead code elimination, inlining, loop optimization
  
- [ ] **Compilation speed improvements**
  - Profile preprocessor (currently slowest phase)
  - Cache macro expansions
  - Parallel compilation of independent modules

- [ ] **Runtime performance**
  - Benchmark against C/C++
  - Optimize generated IR patterns
  - Add SIMD intrinsics for collections

#### Priority 5: Developer experience
- [ ] **Better debugging support**
  - DWARF debug info generation
  - Source maps for macros
  - Stack trace generation

- [ ] **Language server protocol (LSP)**
  - Autocomplete
  - Go to definition
  - Real-time error checking
  - Documentation on hover

- [ ] **Package manager**
  - Dependency management
  - Versioning
  - Central repository

### 🧪 Testing Requirements for Nikola

**Must have 100% confidence in:**
1. ✅ Type system correctness (all 10 types)
2. ✅ Function calls and returns
3. ✅ Control flow (if/while/till/defer)
4. ⚠️ Memory safety (need semantic analyzer)
5. ❌ Standard library completeness (currently 2/6 working)
6. ❌ Error handling (need better diagnostics)

**Test coverage needed:**
- [ ] Unit tests for every stdlib function
- [ ] Integration tests for real programs
- [ ] Fuzz testing for compiler robustness
- [ ] Memory leak detection (valgrind/asan)
- [ ] Performance benchmarks vs C
- [ ] Edge case documentation

### 📊 Success Metrics for Nikola

**Compiler stability:**
- Zero crashes on valid input
- Clear errors on invalid input
- Deterministic compilation (same input → same output)
- Fast compile times (< 1s for typical programs)

**Language completeness:**
- Full stdlib (collections, math, string, io, mem, bits, network, filesystem)
- All type system features working (pointers, arrays, structs)
- Semantic analysis catching common errors
- Production-ready error messages

**Documentation quality:**
- Complete language reference
- API documentation for all stdlib
- Tutorial series (beginner → advanced)
- Best practices guide
- Migration guide (from C/C++)

---

## 📋 Task Dependencies

```
PHASE 1 (Tsoding) - Can do NOW
├── Test unknown stdlib files (independent)
├── Create test suite for working code (independent)  
├── Write documentation (independent)
└── Clean repo (independent)

PHASE 2 (Nikola) - Sequential dependencies
├── 1. Implement pointers (@)
│   └── Enables: io.aria, advanced collections
├── 2. Implement wildx
│   └── Enables: mem.aria, JIT compilation
├── 3. Fix macro variable bug
│   └── Enables: bit_operations.aria
├── 4. Fix string.aria LLVM bug
│   └── Enables: string manipulation
├── 5. Implement semantic analyzer
│   └── Enables: Production readiness
├── 6. Add arrays/objects
│   └── Enables: Modern syntax
└── 7. Expand stdlib
    └── Enables: Real-world programs
```

---

## 🎯 Current Focus

**THIS WEEK** (Phase 1):
1. Test all stdlib files - find what else works
2. Create comprehensive test suite
3. Document working features with examples
4. Clean and organize repo for presentation
5. Wait for Tsoding response

**AFTER TSODING FEEDBACK** (Phase 2):
1. Implement pointer types (biggest blocker)
2. Fix macro variable scoping bug
3. Implement semantic analyzer
4. Complete all stdlib libraries
5. Achieve production stability for Nikola

---

## 📝 Notes

**Why Tsoding First?**
- Get expert feedback on architecture decisions
- Validate preprocessor approach (most unique feature)
- Identify blind spots we haven't considered
- Build credibility in PL community

**Why Nikola Needs Stability?**
- Aria will be Nikola's native training language
- Training data must be deterministic
- Compiler bugs = corrupted training data
- Need 100% confidence in every feature we expose

**Risk Assessment:**
- ⚠️ Pointers are complex - may take 1-2 weeks
- ⚠️ Semantic analyzer is large - may take 2-3 weeks  
- ⚠️ String LLVM bug may be deep - unknown timeline
- ✅ Macro bug has known workaround - can proceed without fix
- ✅ Current working features are stable - safe for Tsoding demo
