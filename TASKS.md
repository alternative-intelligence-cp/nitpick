# Aria Compiler Tasks

**Last Updated**: 2025-12-24

This file tracks available work for contributors. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

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
Status: AVAILABLE
Spec: aria_ecosystem/specs/TBB_TYPES.md, Section 3.2
Complexity: LOW
Tier: 1
Description: Add comprehensive tests for ERR in pick patterns
Acceptance Criteria:
  - Test ERR as single pattern: pick(x) { (ERR) { } }
  - Test ERR with other values: pick(x) { (ERR) { }, (0) { } }
  - Test exhaustiveness warning when ERR missing
  - All tests must pass
Files: tests/test_err_patterns.aria
Note: ERR parsing is fixed, but pattern-specific tests needed
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
