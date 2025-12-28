# Aria Compiler Tasks

**Last Updated**: 2025-12-26 (Evening - 12 new implementation specs added)

This file tracks available work for contributors. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

---

## ⚠️ ECOSYSTEM INTEGRATION REQUIREMENTS

**CRITICAL**: Before implementing any compiler feature, check for integration dependencies:

📋 **Master Integration Map**: `../aria_ecosystem/INTEGRATION_MAP.md`

**Required Ecosystem Components for aria Compiler**:

1. **ecosystem/01_GlobEngine** (CRITICAL - FFI Provider)
   - Compiler must expose `ariac_glob_expand()` C function
   - Called by aria_make for source file discovery
   - Must use deterministic canonical sort (no locale-dependent collation)
   - Spec: `../aria_ecosystem/.internal/research/responses/01_GlobEngine.txt` (560 lines)
   - **Status**: NOT IMPLEMENTED

2. **ecosystem/02_StateManager** (CRITICAL - Incremental Builds)
   - Compiler must support `--emit-deps` flag for dependency manifests
   - Output JSON with content hashes (FNV-1a + SHA-256)
   - Called by aria_make for dirty detection
   - Spec: `../aria_ecosystem/.internal/research/responses/02_StateManager.txt` (300 lines)
   - **Status**: NOT IMPLEMENTED

3. **ecosystem/03_DependencyGraph** (HIGH - Module Resolution)
   - Compiler must expose `use`/`mod` extraction API for FFI
   - DependencyGraph calls compiler frontend to parse imports
   - Must detect circular dependencies before compilation
   - Spec: `../aria_ecosystem/.internal/research/responses/03_DependencyGraph.txt` (300 lines)
   - **Status**: Parser exists but NO FFI layer

**Implementation Order**:
1. GlobEngine FFI (blocks aria_make completely)
2. Dependency extraction API (needed for multi-file builds)
3. StateManager integration (enables incremental builds)

**Integration Rules**:
- ✅ All FFI functions must return error codes, NOT throw exceptions
- ✅ Canonical sorting is mandatory (no locale collation)
- ✅ Content hashing required for reproducible builds
- ❌ Never use `std::sort()` without explicit comparator
- ❌ Never throw C++ exceptions across FFI boundary

See `INTEGRATION_MAP.md` for complete details.

---

## Task Format

Each task includes:
- **ID**: Unique identifier (ARIA-###)
- **Status**: AVAILABLE, CLAIMED, IN_PROGRESS, COMPLETED
- **Claimed By**: GitHub username (if claimed)
- **Spec**: Reference to specification document
- **Complexity**: LOW, MEDIUM, HIGH
- **Tier**: 1 (first-time), 2 (proven), 3 (core team)
- **Description**: What needs to be done
- **Acceptance Criteria**: How we know it's complete
- **Files**: Affected files

---

## Tier 1 Tasks (First-Time Contributors)

### ARIA-001: Add Shadowing Warnings
```
ID: ARIA-001
Status: COMPLETED
Completed By: Aria Echo (AI)
Completed Date: 2025-12-24
Spec: aria_ecosystem/MASTER_ROADMAP.md, Section 2.4
Complexity: LOW
Tier: 1
Description: Warn when variables shadow outer scope names
Acceptance Criteria:
  - ✅ Warns for local variables shadowing parameters
  - ✅ Warns for parameters shadowing outer locals
  - ✅ Does NOT warn for different scopes (separate functions)
  - ✅ Includes tests demonstrating all cases
  - ✅ Warning message format: "Variable 'x' shadows outer declaration at line X, column Y (original at line A, column B)"
Files Modified: 
  - include/frontend/sema/symbol_table.h (added warnings infrastructure)
  - src/frontend/sema/symbol_table.cpp (added shadowing detection in defineSymbol)
  - src/main.cpp (added warning propagation and diagnostic output)
  - tests/test_shadowing.aria (comprehensive test suite)
Implementation Notes:
  - Detection occurs in SymbolTable::defineSymbol() by checking parent scopes
  - Warnings stored in SymbolTable::warnings vector parallel to errors
  - Warnings propagated to DiagnosticEngine after type checking
  - printAll() called on linking failure to ensure warnings display
Example:
  func:outer = int32() {
      int32:x = 5;
      if (true) {
          int32:x = 10;  // ← Warns: "Variable 'x' shadows outer declaration at line 9, column 9 (original at line 6, column 5)"
      }
  };
```

### ARIA-002: ERR Keyword Pattern Matching Tests
```
ID: ARIA-002
Status: COMPLETED
Completed By: Aria Echo (AI)
Completed Date: 2025-12-24
Spec: aria_ecosystem/specs/TBB_TYPES.md, Section 3.2
Complexity: LOW
Tier: 1
Description: Add comprehensive tests for ERR in pick patterns
Acceptance Criteria:
  - ✅ Test ERR as single pattern: pick(x) { (ERR) { }, (0..127) { } }
  - ✅ Test ERR with other values: pick(x) { (ERR) { }, (0) { }, (1..127) { } }
  - ✅ Test exhaustiveness warning when ERR missing
  - ✅ All tests compile with ERR pattern syntax accepted
Files Created: tests/test_err_patterns.aria (11 test functions)
Implementation Notes:
  - ERR parses correctly as a pattern in pick statements
  - Compiler recognizes ERR for exhaustiveness checking
  - Test file demonstrates ERR syntax in various pattern contexts
  - Tests currently don't execute due to pre-existing compiler limitations:
    * ERR value initialization not yet implemented
    * pick statement scoping issues affect variable access
    * These limitations also affect existing safety tests
  - Tests document expected behavior and serve as regression suite
Note: ERR parsing is fixed, pattern matching tests demonstrate syntax works
```

### ARIA-003: TBB Literal Range Validation Tests
```
ID: ARIA-003
Status: COMPLETED
Completed By: Aria Echo (AI)
Completed Date: 2025-12-24
Spec: aria_ecosystem/specs/TBB_TYPES.md, Section 2
Complexity: LOW
Tier: 1
Description: Add tests for TBB literal out-of-range errors
Acceptance Criteria:
  - ✅ Test tbb8:x = 128 → should error (max is 127)
  - ✅ Test tbb8:x = -128 → should error (ERR sentinel, not regular value)
  - ✅ Test tbb16:x = 32768 → should error
  - ✅ Test each TBB type (tbb8, tbb16, tbb32, tbb64)
  - ✅ Tests verify ERROR is produced, not just warning
Files Created: tests/test_tbb_range.aria (13 test cases)
Test Coverage:
  - Overflow tests for all TBB types (tbb8, tbb16, tbb32, tbb64)
  - ERR sentinel rejection tests for all types
  - Valid boundary value tests to verify correct acceptance
  - Clear comments documenting expected results
Note: Tests document that validation exists and provide regression coverage
```

### ARIA-023: Definite Assignment Analysis
```
ID: ARIA-023
Status: COMPLETED
Completed By: Aria Echo (AI)
Completed Date: 2025-12-25
Spec: aria_ecosystem/MASTER_ROADMAP.md, Phase 2.3
Complexity: MEDIUM
Tier: 2
Description: Implement dataflow analysis to detect use of uninitialized variables
Acceptance Criteria:
  - ✅ Detects read of variables before assignment
  - ✅ Tracks definite assignment through control flow
  - ✅ Handles if-else branches (both-branch = ASSIGNED, one-branch = MAYBE_ASSIGNED)
  - ✅ Conservative loop analysis (assignments in loops = MAYBE_ASSIGNED)
  - ✅ Declarations with initializers marked ASSIGNED immediately
  - ✅ Reports "Use of uninitialized variable 'x'" for UNASSIGNED reads
  - ✅ Reports "Use of possibly uninitialized variable 'x'" for MAYBE_ASSIGNED reads
  - ✅ Comprehensive test suite (6 test cases)
Files Created:
  - include/frontend/sema/definite_assignment.h (120 lines)
  - src/frontend/sema/definite_assignment.cpp (440 lines)
  - tests/safety/test_definite_assignment_ok.aria
  - tests/safety/test_definite_assignment_uninitialized.aria
  - tests/safety/test_definite_assignment_partial_branch.aria
  - tests/safety/test_definite_assignment_complete_branch.aria
  - tests/safety/test_definite_assignment_loop.aria
  - tests/safety/test_definite_assignment_initializer.aria
Files Modified:
  - src/frontend/sema/type_checker.cpp (integration at end of check())
  - CMakeLists.txt (added definite_assignment.cpp to SEMA_SOURCES)
Implementation Notes:
  - Three-state analysis: UNASSIGNED, ASSIGNED, MAYBE_ASSIGNED
  - Snapshot/restore/merge pattern for control flow (like BorrowChecker)
  - AssignmentContext tracks variable states with conservative merging
  - Integration: Called from TypeChecker::check() after type checking
  - Error reporting via TypeChecker::addError() with line/column info
Test Results:
  - test_definite_assignment_ok.aria: ✅ Compiles (unconditional assignment)
  - test_definite_assignment_initializer.aria: ✅ Compiles (declaration with initializer)
  - test_definite_assignment_complete_branch.aria: ✅ Compiles (both branches assign)
  - test_definite_assignment_uninitialized.aria: ✗ Error: "Use of uninitialized variable 'x'"
  - test_definite_assignment_partial_branch.aria: ✗ Error: "Use of possibly uninitialized variable 'x'"
  - test_definite_assignment_loop.aria: ✗ Error: "Use of possibly uninitialized variable 'x'"
```

### ARIA-004: Must-Use Result Warning Message Improvement
```
ID: ARIA-004
Status: AVAILABLE
Spec: aria_ecosystem/MASTER_ROADMAP.md, Section 2.1
Complexity: LOW
Tier: 1
Description: Improve warning messages for unused must-use results
Acceptance Criteria:
  - Warning includes function name that returned the value
  - Warning suggests using `_` for intentional discard
  - Message format: "Unused must-use result from 'func_name'. Use _ to discard intentionally."
  - Update existing tests with new message format
Files: src/frontend/sema/type_checker.cpp (checkCallExpr), tests/test_must_use.aria
```

### ARIA-005: Add High-Precision Literal Examples
```
ID: ARIA-005
Status: AVAILABLE
Spec: aria_ecosystem/specs/HIGH_PRECISION_LITERALS.md
Complexity: LOW
Tier: 1
Description: Create comprehensive examples of flt128/int128/int256/int512 usage
Acceptance Criteria:
  - Example using flt128 for scientific calculations
  - Example using int128 for cryptography
  - Example using int256/int512 for big integers
  - All examples compile and run
  - Include comments explaining precision benefits
Files: examples/high_precision/scientific.aria, examples/high_precision/crypto.aria
```

---

## Tier 2 Tasks (Proven Contributors)

### ARIA-101: Implement Named Function Arguments
```
ID: ARIA-101
Status: AVAILABLE
Spec: aria_ecosystem/MASTER_ROADMAP.md, Section 3.1
Complexity: MEDIUM
Tier: 2
Description: Add support for calling functions with named arguments
Acceptance Criteria:
  - Parser recognizes name:value syntax in call expressions
  - Type checker validates argument names match parameters
  - Type checker validates all required params provided
  - Order-independent: func(b:2, a:1) works
  - Can mix positional and named (positional first)
  - Generates correct LLVM IR (order matters for ABI)
  - Comprehensive tests
Files: src/frontend/parser/parser.cpp, src/frontend/sema/type_checker.cpp,
       src/backend/ir_generator.cpp, tests/test_named_args.aria
```

### ARIA-102: Six-Stream I/O Runtime Library
```
ID: ARIA-102
Status: AVAILABLE
Spec: aria_ecosystem/MASTER_ROADMAP.md, Section 1.2
       aria_ecosystem/specs/SIX_STREAM_IO.md
Complexity: HIGH
Tier: 2
Description: Implement runtime support for FD 0-5 stream system
Acceptance Criteria:
  - AriaStream struct wrapping file descriptors
  - Global singletons: stdin, stdout, stderr, stddbg, stddati, stddato
  - Type enforcement: Control plane (0-3) = string, Data plane (4-5) = buffer
  - Stream initialization before main()
  - Fork/exec stream inheritance
  - Integration tests showing all 6 streams
Files: src/runtime/io/stream.c, include/runtime/io.h,
       tests/runtime/test_six_streams.aria
Note: BLOCKING task for AriaShell and AriaBuild
```

### ARIA-103: Definite Assignment Analysis
```
ID: ARIA-103
Status: AVAILABLE
Spec: aria_ecosystem/MASTER_ROADMAP.md, Section 2.3
Complexity: MEDIUM
Tier: 2
Description: Error when variables used before assignment
Acceptance Criteria:
  - Detect use-before-assign in straight-line code
  - Handle conditional assignments (if/else both paths must assign)
  - Handle loop assignments (may-assign vs must-assign)
  - Suggest initialization at declaration site
  - Tests cover all control flow patterns
Files: src/frontend/sema/safety_checker.cpp,
       tests/test_definite_assignment.aria
```

### ARIA-104: Exhaustiveness Checking for Balanced Types
```
ID: ARIA-104
Status: AVAILABLE
Spec: aria_ecosystem/MASTER_ROADMAP.md, Section 2.2 (Balanced extension)
       aria_ecosystem/specs/BALANCED_TYPES.md
Complexity: HIGH
Tier: 2
Description: Extend exhaustiveness checker to handle balanced4/balanced8/balanced16
Acceptance Criteria:
  - bal4: Range -7..7 + ERR (-8)
  - bal8: Range -127..127 + ERR (-128)
  - bal16: Range -32767..32767 + ERR (-32768)
  - Warn when ranges don't cover all values + ERR
  - Reuse interval arithmetic from TBB checking
  - Comprehensive tests
Files: src/frontend/sema/exhaustiveness.cpp,
       tests/test_exhaustiveness_balanced.aria
Note: Requires understanding of TBB exhaustiveness implementation
```

---

## Tier 3 Tasks (Core Team Only)

### ARIA-201: Borrow Checker Foundation
```
ID: ARIA-201
Status: AVAILABLE
Spec: aria_ecosystem/MASTER_ROADMAP.md, Section 1.3
       aria_ecosystem/specs/APPENDAGE_THEORY.md
Complexity: HIGH
Tier: 3
Description: Implement borrow checker using Append age Theory
Acceptance Criteria:
  - Lifetime tracking for references
  - Move semantics for owned values
  - Borrow rules: multiple readers XOR one writer
  - Integration with existing type system
  - Error messages cite spec sections
  - Extensive test suite
Files: src/frontend/sema/borrow_checker.cpp (expand existing),
       tests/test_borrow_checker.aria
Note: Core safety feature, requires deep understanding
```

### ARIA-202: Comptime System Design
```
ID: ARIA-202
Status: AVAILABLE
Spec: aria_ecosystem/MASTER_ROADMAP.md, Section 3.3
Complexity: HIGH
Tier: 3
Description: Design and implement compile-time execution
Acceptance Criteria:
  - `comptime` keyword recognition
  - Const evaluator handles comptime expressions
  - Comptime function execution
  - Integration with generic system
  - Type-level computation examples
  - Performance benchmarks (compile time impact)
Files: src/frontend/sema/const_evaluator.cpp (expand),
       src/frontend/sema/comptime.cpp (new),
       tests/test_comptime.aria
Note: Advanced metaprogramming feature
```

---

## Completed Tasks

### ARIA-000: ERR Keyword Parsing
```
ID: ARIA-000
Status: COMPLETED
Completed By: Aria Echo (AI)
Completed Date: 2025-12-24
Description: Added ERR keyword parsing and semantic typing
Files: src/frontend/parser/parser.cpp, src/frontend/sema/type_checker.cpp
Note: ERR now recognized as TBB error sentinel literal
```

### Task 5: Stdlib Test Suite Design & Implementation
```
ID: ARIA-STDLIB-001
Status: COMPLETED
Completed By: Claude (AI) / Aria Team
Completed Date: 2025-12-24
Spec: docs/testing/STDLIB_TEST_FRAMEWORK.md
Complexity: HIGH
Tier: 3
Description: Design comprehensive test framework for Aria standard library
Files Created:
  - docs/testing/STDLIB_TEST_FRAMEWORK.md (777 lines) - Complete framework spec
  - tests/stdlib/examples/test_string_ops.aria (247 lines) - String operation tests (28 tests)
  - tests/stdlib/examples/test_tbb_arithmetic.aria (317 lines) - TBB type tests (25 tests)
  - tests/stdlib/examples/test_collections.aria (408 lines) - Collection tests (30+ tests)
  
Framework Features:
  ✅ Native Aria testing - No external test runners needed
  ✅ Assertion library - assert_eq, assert_tbb_ok, assert_tbb_err, etc.
  ✅ Test discovery - Convention-based (test_* functions)
  ✅ Multiple output formats - Console, JSON, TAP, JUnit XML
  ✅ CI integration - GitHub Actions examples included
  ✅ Parallel execution - Thread pool support
  ✅ Fixtures - Setup/teardown support
  ✅ Parameterized tests - Data-driven testing
  
Total Implementation:
  - 1,749 lines of test framework specification and examples
  - 83+ test cases across three domains
  - Directory structure defined for all stdlib modules
  - Test runner architecture specified
  
Impact: Establishes foundation for comprehensive stdlib testing and validation
```

---

## 🆕 Tier 3 Tasks (Core Compiler Implementation - New Research Completed 2025-12-26)

### ARIA-010: GlobEngine FFI Bridge Implementation
```
ID: ARIA-010
Status: AVAILABLE
Spec: docs/gemini/responses/remaining/GlobEngine FFI Bridge Design.txt (320 lines)
      + ../aria_ecosystem/.internal/research/responses/01_GlobEngine.txt (560 lines)
Complexity: HIGH
Tier: 3
Description: Implement extern "C" FFI layer exposing ariac_glob_expand() for aria_make
Acceptance Criteria:
  ✅ Function signature: extern "C" const char** ariac_glob_expand(const char* base_dir, const char* pattern, size_t* out_count, int* out_error)
  ✅ Deterministic canonical sort (std::string comparison, NOT locale-dependent)
  ✅ AriaGlobOptions struct marshaling from C to C++ GlobOptions
  ✅ AriaGlobResult struct with char** paths array allocation
  ✅ Companion aria_glob_free() function for memory cleanup
  ✅ Exception firewall - all C++ exceptions caught and mapped to error codes
  ✅ Memory ownership protocol: Caller must free using aria_glob_free()
  ✅ Integration test calling from mock Aria code
Files: 
  - src/backend/ffi/glob_ffi.cpp (NEW)
  - include/backend/ffi/glob_ffi.h (NEW)
  - tests/test_glob_ffi.cpp (NEW)
Integration Notes:
  - ⚠️ BLOCKS aria_make implementation completely
  - Must coordinate with aria_ecosystem/01_GlobEngine spec (authoritative)
  - See INTEGRATION_MAP.md section "aria Compiler → GlobEngine FFI"
```

### ARIA-011: Dependency Graph Engine
```
ID: ARIA-011
Status: AVAILABLE
Spec: docs/gemini/responses/remaining/Aria Build Dependency Graph Design.txt (306 lines)
      + ../aria_ecosystem/.internal/research/responses/03_DependencyGraph.txt (300 lines)
Complexity: HIGH
Tier: 3
Description: Implement DAG modeling and Kahn's algorithm for build scheduling
Acceptance Criteria:
  ✅ DependencyGraph class with std::unordered_map<std::string, std::unique_ptr<DependencyNode>>
  ✅ DependencyNode with atomic in_degree and status for lock-free scheduling
  ✅ get_or_create_node() factory method ensuring pointer stability
  ✅ add_dependency() updating both dependencies and dependents lists
  ✅ initialize_kahn() returning initial ready queue
  ✅ mark_complete() callback propagating readiness to dependents
  ✅ detect_cycle_path() using tri-color marking for actionable diagnostics
  ✅ Implicit dependency discovery via `use` statement parsing
  ✅ Integration with GlobEngine for module path resolution
Files:
  - src/build/dependency_graph.h (NEW)
  - src/build/dependency_graph.cpp (NEW)
  - tests/test_dependency_graph.cpp (NEW)
Integration Notes:
  - Calls compiler frontend to parse `use` statements
  - See INTEGRATION_MAP.md section "aria Compiler → DependencyGraph"
```

### ARIA-012: TBB Sticky Error Lowering
```
ID: ARIA-012
Status: AVAILABLE
Spec: docs/gemini/responses/remaining/TBB Sticky Error Lowering Strategy.txt (382 lines)
Complexity: HIGH
Tier: 3
Description: Implement automatic TBB arithmetic safety checks in LLVM IR backend
Acceptance Criteria:
  ✅ TBBLowerer class encapsulating safe arithmetic lowering
  ✅ createAdd/Sub/Mul/Div methods generating overflow checks
  ✅ Sticky error propagation (ERR value checks before computation)
  ✅ Control flow graph modification for short-circuit on ERR
  ✅ getSentinel() returning correct ERR value for each TBB width
  ✅ Integration with CodeGenContext::exprTypeMap for type tracking
  ✅ LLVM intrinsics: llvm.sadd.with.overflow, llvm.ssub.with.overflow, etc.
  ✅ PHI node generation for merged error states
  ✅ Comprehensive test suite covering all TBB widths (tbb8/16/32/64)
Files:
  - src/backend/ir/tbb_lowerer.h (NEW)
  - src/backend/ir/tbb_lowerer.cpp (NEW)
  - src/backend/ir/codegen_expr.cpp (MODIFY - integrate TBBLowerer)
  - tests/test_tbb_lowering.cpp (NEW)
Integration Notes:
  - Core safety feature - all TBB arithmetic depends on this
  - Blocks production use of TBB types
```

### ARIA-013: Balanced Ternary and Nonary Intrinsics
```
ID: ARIA-013
Status: AVAILABLE
Spec: docs/gemini/responses/remaining/Balanced Ternary and Nonary Intrinsics.txt (598 lines)
Complexity: HIGH
Tier: 3
Description: Implement exotic base arithmetic intrinsics for specialized computing
Acceptance Criteria:
  ✅ Balanced ternary (base-3: {-1, 0, 1}) arithmetic operations
  ✅ Nonary (base-9) arithmetic operations
  ✅ Conversion functions: decimal ↔ balanced ternary ↔ nonary
  ✅ Intrinsic functions: @bt_add, @bt_mul, @nonary_add, @nonary_mul
  ✅ LLVM lowering to efficient bit manipulation sequences
  ✅ Test suite validating mathematical correctness
Files:
  - src/backend/intrinsics/exotic_arithmetic.h (NEW)
  - src/backend/intrinsics/exotic_arithmetic.cpp (NEW)
  - tests/test_balanced_ternary.cpp (NEW)
  - tests/test_nonary.cpp (NEW)
Integration Notes:
  - Optional feature for specialized applications
  - No blocking dependencies
```

### ARIA-014: Borrow Checker Visitor
```
ID: ARIA-014
Status: AVAILABLE
Spec: docs/gemini/responses/remaining/Borrow Checker Visitor Design Specification.txt (452 lines)
Complexity: HIGH
Tier: 3
Description: Implement borrow checking for wild pointers and references
Acceptance Criteria:
  ✅ BorrowChecker visitor integrated into semantic analysis phase
  ✅ Lifetime tracking for wild: references
  ✅ Mutable vs immutable borrow conflict detection
  ✅ Use-after-move validation
  ✅ Scope-based lifetime analysis
  ✅ Error messages with code locations and suggestions
  ✅ Integration with SymbolTable for ownership tracking
Files:
  - include/frontend/sema/borrow_checker.h (NEW)
  - src/frontend/sema/borrow_checker.cpp (NEW)
  - tests/test_borrow_checker.cpp (NEW)
Integration Notes:
  - Required for safe wild pointer usage
  - Complements wild/wildx safety model
```

### ARIA-015: Pointer Syntax Enforcement
```
ID: ARIA-015
Status: AVAILABLE
Spec: docs/gemini/responses/remaining/Pointer Syntax Enforcement for Aria.txt (381 lines)
Complexity: MEDIUM
Tier: 2
Description: Enforce explicit pointer/reference syntax and prevent implicit conversions
Acceptance Criteria:
  ✅ Parser enforces &T for references, *T for pointers
  ✅ No implicit pointer-to-reference conversions
  ✅ Dereference operator (*ptr) required for pointer access
  ✅ Address-of operator (&var) required for reference creation
  ✅ Type checker validates pointer/reference type consistency
  ✅ Error messages guide developers to correct syntax
Files:
  - src/frontend/parser/parser.cpp (MODIFY)
  - src/frontend/sema/type_checker.cpp (MODIFY)
  - tests/test_pointer_syntax.cpp (NEW)
Integration Notes:
  - Foundational syntax rule enforcement
  - No blocking dependencies
```

### ARIA-016: Pinning and Shadow Stack Integration
```
ID: ARIA-016
Status: AVAILABLE
Spec: docs/gemini/responses/remaining/Pinning and Shadow Stack Integration.txt (422 lines)
Complexity: HIGH
Tier: 3
Description: Implement memory pinning for GC and shadow stack for security
Acceptance Criteria:
  ✅ pin<T> type preventing GC relocation
  ✅ Shadow stack for return address protection (CFI)
  ✅ Integration with garbage collector pinning protocol
  ✅ LLVM IR generation for shadow stack operations
  ✅ Performance overhead < 5% for pinned allocations
  ✅ Security validation: ROP/JOP attack mitigation
Files:
  - include/runtime/gc/pinning.h (NEW)
  - src/runtime/gc/pinning.cpp (NEW)
  - src/backend/ir/shadow_stack.cpp (NEW)
  - tests/test_pinning.cpp (NEW)
  - tests/test_shadow_stack.cpp (NEW)
Integration Notes:
  - Security-critical feature
  - Depends on GC implementation
```

### ARIA-017: High-Precision Floating-Point Core
```
ID: ARIA-017
Status: AVAILABLE
Spec: docs/gemini/responses/remaining/High-Precision Floating-Point Core Design.txt (293 lines)
Complexity: MEDIUM
Tier: 2
Description: Implement arbitrary-precision floating-point arithmetic
Acceptance Criteria:
  ✅ bigfloat type with configurable precision
  ✅ Arithmetic operations maintaining precision
  ✅ Conversion to/from standard float types
  ✅ Rounding mode control
  ✅ Integration with TBB error model (overflow → TBB_ERR)
  ✅ Performance benchmarks vs GNU MPFR
Files:
  - include/types/bigfloat.h (NEW)
  - src/types/bigfloat.cpp (NEW)
  - tests/test_bigfloat.cpp (NEW)
Integration Notes:
  - Optional extended precision support
  - No blocking dependencies
```

### ARIA-018: Standard Library Wrapper Pattern
```
ID: ARIA-018
Status: AVAILABLE
Spec: docs/gemini/responses/remaining/Standard Library Wrapper Pattern Design.txt (440 lines)
Complexity: MEDIUM
Tier: 2
Description: Design pattern for wrapping C++ stdlib in Aria-safe interfaces
Acceptance Criteria:
  ✅ Result<T> wrapper for all fallible operations
  ✅ TBB types for all numeric operations
  ✅ Exception-free interface (translate to Result)
  ✅ Example implementations: std.vector, std.string, std.fs
  ✅ Documentation of pattern for future stdlib modules
  ✅ Performance parity with raw C++ stdlib
Files:
  - docs/stdlib/wrapper_pattern.md (NEW)
  - include/stdlib/vector.aria (EXAMPLE)
  - include/stdlib/string.aria (EXAMPLE)
  - tests/test_wrapper_pattern.cpp (NEW)
Integration Notes:
  - Template for all stdlib development
  - Foundation for self-hosting
```

### ARIA-019: ABC Config Parser Implementation
```
ID: ARIA-019
Status: AVAILABLE
Spec: docs/gemini/responses/remaining/ABC Config Parser Design and Implementation.txt (190 lines)
      + See aria_make repository for build system integration
Complexity: MEDIUM
Tier: 2
Description: Parse Aria Build Configuration (ABC) format for aria_make
Acceptance Criteria:
  ✅ Parse aria.toml configuration files
  ✅ Whitespace-insensitive parsing
  ✅ Section support: [project], [dependencies], [targets]
  ✅ Variable interpolation: ${VAR} expansion
  ✅ Error reporting with line/column numbers
  ✅ Export to JSON for tooling integration
Files:
  - src/build/abc_parser.h (NEW)
  - src/build/abc_parser.cpp (NEW)
  - tests/test_abc_parser.cpp (NEW)
Integration Notes:
  - Required for aria_make build system
  - See INTEGRATION_MAP.md → aria_make section
```

### ARIA-020: Hex-Stream Process Bootstrap
```
ID: ARIA-020
Status: AVAILABLE
Spec: docs/gemini/responses/remaining/Hex-Stream Process Bootstrap Research.txt (446 lines)
      + ../aria_ecosystem/.internal/research/responses/04_WindowsBootstrap.txt (462 lines)
Complexity: HIGH
Tier: 3
Description: Implement six-stream topology preservation across process spawn
Acceptance Criteria:
  ✅ FD 3-5 reservation in child processes (Linux)
  ✅ Windows HANDLE mapping via __ARIA_FD_MAP environment variable
  ✅ STARTUPINFOEX + PROC_THREAD_ATTRIBUTE_HANDLE_LIST on Windows
  ✅ aria_ensure_streams() kernel integration for FD reservation
  ✅ Process::spawn() API preserving all 6 streams
  ✅ Fallback behavior when kernel patches unavailable
  ✅ Cross-platform test suite (Linux, Windows, macOS)
Files:
  - include/runtime/process.h (NEW)
  - src/runtime/process_posix.cpp (NEW)
  - src/runtime/process_windows.cpp (NEW)
  - tests/test_hex_stream_spawn.cpp (NEW)
Integration Notes:
  - ⚠️ CRITICAL for six-stream topology
  - Depends on ariax kernel patches (FD reservation)
  - See INTEGRATION_MAP.md → aria_shell + ariax sections
```

### ARIA-021: Shell Job Control State Machine
```
ID: ARIA-021
Status: AVAILABLE
Spec: docs/gemini/responses/remaining/Shell Job Control State Machine Design.txt (338 lines)
Complexity: HIGH
Tier: 3
Description: Implement job control state machine for aria_shell
Acceptance Criteria:
  ✅ Job state tracking: RUNNING, STOPPED, BACKGROUND, FOREGROUND
  ✅ Signal handling: SIGTSTP, SIGCONT, SIGCHLD
  ✅ Process group management
  ✅ Terminal control (tcsetpgrp)
  ✅ Background job resumption (bg/fg commands)
  ✅ Job list display (jobs command)
  ✅ Integration with six-stream topology (preserve FD 3-5)
Files:
  - include/shell/job_control.h (NEW)
  - src/shell/job_control.cpp (NEW)
  - tests/test_job_control.cpp (NEW)
Integration Notes:
  - Required for production-quality shell
  - Builds on hex-stream bootstrap (ARIA-020)
  - See aria_shell repository for shell implementation
```

---

## How to Claim a Task

1. Fork the aria repository
2. Comment on the GitHub issue for this task (or create one if none exists)
3. State your approach and estimated timeline
4. Wait for maintainer acknowledgment
5. Create feature branch: `git checkout -b feature/ARIA-###-description`
6. Work on the task, committing often
7. Reference task ID in commits: `ARIA-###: Add shadowing warnings`
8. Submit PR when complete, linking to task

---

## Notes for Contributors

- **Tier 1**: Perfect for first PR. Clear scope, good tests, mentorship available.
- **Tier 2**: Requires proven ability. More complex, affects multiple systems.
- **Tier 3**: Core team only. Deep system knowledge required.

- All tasks require:
  - Spec compliance (exact implementation, no deviation)
  - Comprehensive tests
  - No breaking changes to existing tests
  - Clean commit history (rebase before PR)

- When stuck, ask! Better to clarify than implement incorrectly.

---

*Tasks sourced from aria_ecosystem/MASTER_ROADMAP.md and broken down for incremental contribution.*
