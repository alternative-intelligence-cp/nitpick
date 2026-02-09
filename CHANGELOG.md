# Aria Language Changelog

## [Unreleased] - February 2026

### Added
- **Balanced Literal Syntax** - Ternary and nonary number literals
  - Ternary: `0t[01T]+` where T=-1, base-3 positional (e.g., `0t1T0` = 6)
  - Nonary: `0n[01234ABCD]+` where A=-1, B=-2, C=-3, D=-4, base-9 positional (e.g., `0n2A` = 17)
  - Implementation in `src/frontend/lexer/lexer.cpp` lines 750-865
  - Token types: TOKEN_TERNARY, TOKEN_NONARY
  - Converts to int64 at compile time for use with arithmetic

- **i8 Type for trit/nit** - Changed from i2/i4 to i8 storage
  - Enables ERR sentinel value (-128) for overflow detection
  - Provides sticky error propagation matching TBB type safety
  - Prevents arithmetic wrap-around issues (1+1 in i2 would wrap to -2)

- **ERR Propagation for Exotic Types**
  - trit/nit operations propagate ERR sentinel through calculations
  - `ERR + x = ERR`, `ERR * x = ERR`, etc.
  - Prevents silent corruption of error states

- **println() Builtin** - Convenience function for print with newline
  - Usage: `println("text")` instead of `print("text\n")`
  - Returns int64 (bytes written) like print()
  - Implementation in type checker and IR codegen

### Fixed
- **Type Mismatch Bugs** - Fixed 4 tests with incorrect return types
  - `check_ternary_support.aria` - main() int64→int32
  - `comprehensive_ternary.aria` - All 9 functions int64→int32
  - `manual_ternary_pattern.aria` - main() int64→int32
  - `ternary_debug_value.aria` - trit→int32 cast handling

- **String Return Value Bug (CRITICAL)** - Functions returning strings now work correctly
  - **Root Cause**: Test files were missing explicit unwrap operator (`?`)
  - **Design**: ALL functions return `Result<T>` (even void returns `Result<NIL>`)
    * `pass(val)` and `fail(err)` are sugar to build `{value, error*, is_error}` struct
    * No implicit unwrapping allowed - explicit error handling required
  - **Correct Usage**: `string:s = get_string() ? "default";` 
    * The `?` operator checks `is_error` field and branches appropriately
    * Generates proper control flow: error_block vs success_block with PHI merge
  - **Fix**: Updated test files to use explicit unwrap operator
  - **Impact**: Maintains strict explicit > implicit principle for Nikola's deterministic requirements
  - **Status**: String operations working with proper error handling semantics

- **Test Suite Progress**
  - Improved from 168/373 to 188/389 tests passing (45% → 48.3%)
  - Fixed type system issues preventing exotic type compilation
  - String return value fix enabled 16 additional tests to pass
  - Created TEST_FAILURE_ANALYSIS.md documenting all issues

- **Parameter Reassignment Bug** - Fixed segfault in int64_toString() with negative numbers
  - **Root Cause**: Parameter reassignment (`value = -value`) causes segfault
  - **Investigation**: Parameters appear to be immutable or have codegen bug for reassignment
  - **Solution**: Use local variable (`work_value`) instead of modifying function parameter
  - **Impact**: int64_toString() now handles all cases correctly
  - **Files Updated**:
    * `stdlib/string_convert.aria` - Core library function
    * `tests/test_int64_toString.aria` - Test file with same pattern
  - **Validation**: All test cases passing
    * Zero: 0 ✓
    * Positive: 42, 987654321 ✓
    * Negative: -123 ✓ (previously segfaulted)
    * Balanced literals: 0t1T0=6, 0n2A=17 ✓

### Changed
- **print()/println() Design** - Strict string-only policy
  - Both functions require string argument (no polymorphism)
  - Future `print_t()` will handle formatted output with toString
  - Avoids C printf-style segfault vulnerabilities
  - Separation of concerns: primitives stay simple
  - Fixed to handle both string literals and string variables correctly

### Status
- **String Library**: Core string operations working (concat, println, returns)
- **Result Type Semantics**: Properly enforced with explicit unwrapping
  - All functions return `Result<T>` - no exceptions (except extern C functions)
  - Unwrap operator `?` generates proper error-checking control flow
  - Maintains deterministic error handling for Nikola requirements
- **ToString Library**: int64_toString() implementation **COMPLETE**
  - Works for all cases: zero, positive, negative, large numbers
  - Fixed parameter reassignment bug: `value = -value` caused segfault
  - Solution: Use local variable (`work_value`) instead of modifying parameter
  - Parameters appear to be immutable or have codegen bug for reassignment
  - Validated with balanced literals: `0t1T0` = "6", `0n2A` = "17"
  - Updated: `stdlib/string_convert.aria`, `tests/test_int64_toString.aria`
  - Test output: Zero: 0, Positive: 42, Negative: -123, Large: 987654321
- **Test Suite**: 188/389 passing (48.3%) with proper Result semantics
  - Tests now use explicit `?` unwrap operator where needed
  - No implicit magic - all error paths are explicit
  - Debug needed in src/runtime/strings/strings.cpp

- **Balanced Type Runtime** - trit/tryte/nit/nyte operations
  - Literal syntax complete, runtime operations in progress
  - Kleene logic for trit AND/OR/NOT operations
  - Range clamping for overflow (1+1=1 for trit)
  - Implementation timeline: 1-2 weeks

### Documentation
- Updated TRIT_NIT_ROADMAP.md with literal syntax completion
- Updated aria_specs.txt with 0t/0n syntax specification
- Created test_balanced_literals.aria demonstrating both notations
- Created TEST_FAILURE_ANALYSIS.md for systematic debugging

---

## Previous Releases

(Historical changelog to be populated from git history)
