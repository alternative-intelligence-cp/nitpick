# Aria Compiler Test Expectations
# Version: 0.0.6 - Week 1, Day 4
#
# This file documents the expected status of each test and known issues.
# Format: <test_name> <status> <reason>
# Status: PASS, FAIL, SKIP
#
# Legend:
#   ✓ PASS  - Test should compile and verify successfully
#   ✗ FAIL  - Test has known issues (documented below)
#   ⊘ SKIP  - Test is disabled or not applicable

################################################################################
# PASSING TESTS (5/73 = 6.8%)
################################################################################

test_is_nested                    PASS  # Basic nested if statements
test_stack_optimization           PASS  # Stack allocation for primitives (Day 2 fix)
test_template_simple              PASS  # Template string literals
test_template_string              PASS  # Template string expansion
test_while_loops                  PASS  # While loop with break/continue

################################################################################
# FAILING TESTS - Return Type Mismatches (3 tests)
################################################################################
# Issue: Functions declared as int8() return int64 0
# Cause: Codegen always returns i64 for numeric literals
# Fix: Type-aware return value generation

test_assignments                  FAIL  # Return type: int8 vs i64
test_dollar_variable              FAIL  # Return type: int8 vs i64  
test_when_loops                   FAIL  # Return type: int8 vs i64

################################################################################
# FAILING TESTS - Parse Errors (35 tests)
################################################################################
# Issue: Various syntax/parser issues
# Cause: Incomplete parser implementation or test syntax issues

# Function syntax issues
test_func_minimal                 FAIL  # Parse error: unexpected { at top level
test_func_nogc                    FAIL  # Parse error: unexpected { at top level
test_func_simple2                 FAIL  # Parse error: unexpected { at top level
test_function                     FAIL  # Parse error: unexpected { at top level
test_func_with_params             FAIL  # Parse error: unexpected { at top level
test_simple_func                  FAIL  # Parse error: unexpected { at top level

# For loop issues  
test_for_loop                     FAIL  # Parse error: for loop not implemented
test_features                     FAIL  # Contains for loop syntax

# Lambda syntax issues
test_lambda                       FAIL  # Parse error: lambda syntax
test_lambda_direct                FAIL  # Parse error: lambda syntax
test_lambda_gc_var                FAIL  # Parse error: lambda syntax
test_lambda_in_func               FAIL  # Parse error: lambda syntax
test_lambda_module_simple         FAIL  # Parse error: lambda syntax
test_lambda_no_invoke             FAIL  # Parse error: lambda syntax
test_lambda_simple                FAIL  # Parse error: lambda syntax
test_lambda_two_params            FAIL  # Parse error: lambda syntax
test_lambda_var_module            FAIL  # Parse error: lambda syntax

# Member access issues
test_full_member_access           FAIL  # Parse error: member access syntax
test_member_access                FAIL  # Parse error: member access syntax

# Ternary operator issues
test_is_ternary                   FAIL  # Parse error: ternary operator syntax

# Other parser issues
test_comprehensive                FAIL  # Parse error: complex syntax
test_decrement                    FAIL  # Parse error: decrement operator
test_loops                        FAIL  # Parse error: multiple loop types
test_loops_simple                 FAIL  # Parse error: loop syntax
test_math                         FAIL  # Parse error: math operations
test_module_func                  FAIL  # Parse error: module syntax
test_module_main                  FAIL  # Parse error: module syntax
test_module_simple                FAIL  # Parse error: module syntax
test_object_lit                   FAIL  # Parse error: object literal syntax
test_parse_only                   FAIL  # Parse error: parsing test
test_print_nogc                   FAIL  # Parse error: print syntax
test_print_simple                 FAIL  # Parse error: print syntax
test_result                       FAIL  # Parse error: Result type syntax
test_return                       FAIL  # Parse error: return statement
test_simple_add                   FAIL  # Parse error: basic addition
test_simple_block                 FAIL  # Parse error: block syntax
test_var_module                   FAIL  # Parse error: module variable

################################################################################
# FAILING TESTS - Pick/Pattern Matching Issues (8 tests)
################################################################################
# Issue: Pick statement crashes or has codegen issues

test_pick                         FAIL  # Pick statement crash
test_pick_basic                   FAIL  # Pick statement crash
test_pick_comp                    FAIL  # Pick statement crash
test_pick_comparison              FAIL  # Pick statement crash
test_pick_fall                    FAIL  # Pick statement crash
test_pick_fallthrough             FAIL  # Pick statement crash
test_pick_fixed                   FAIL  # Pick statement crash (ABORTED)
test_pick_minimal                 FAIL  # Pick statement crash
test_pick_ranges                  FAIL  # Pick statement crash
test_pick_simple                  FAIL  # Pick statement crash
test_pick_v2                      FAIL  # Pick statement crash

################################################################################
# FAILING TESTS - Semantic Analysis Issues (3 tests)
################################################################################

test_borrow_checker               FAIL  # Borrow check warnings expected
test_escape_analysis              FAIL  # Escape analysis errors expected
test_semantic_comprehensive       FAIL  # Combined semantic errors expected

################################################################################
# FAILING TESTS - Control Flow Issues (6 tests)
################################################################################

test_control_flow                 FAIL  # Parse/codegen issue
test_till_down                    FAIL  # Till loop descending
test_till_up                      FAIL  # Till loop ascending
test_while_bool_var               FAIL  # While with bool variable
test_while_loop                   FAIL  # While loop syntax variation
test_while_simple                 FAIL  # Simple while loop
test_while_true                   FAIL  # Infinite while loop

################################################################################
# FAILING TESTS - Type/Unwrap Issues (4 tests)
################################################################################

test_type_promotion               FAIL  # Type annotations not supported
test_unwrap                       FAIL  # Unwrap operator issue
test_unwrap_operator              FAIL  # Unwrap operator issue

################################################################################
# FAILING TESTS - Preprocessor/Character Literal Issues (4 tests)
################################################################################

test_hex_escapes                  FAIL  # Hex escape sequences
test_preprocessor                 FAIL  # Preprocessor directives
test_regular_char                 FAIL  # Character literal syntax
test_simple_hex                   FAIL  # Hex number literals

################################################################################
# SUMMARY
################################################################################
#
# Total:     73 tests
# Passing:    5 tests (6.8%)
# Failing:   68 tests (93.2%)
#
# Priority Fixes:
# 1. Return type mismatches (3 tests) - Quick fix
# 2. Pick statement crashes (11 tests) - High severity
# 3. For loop parser (2 tests) - Feature gap
# 4. Lambda syntax (9 tests) - Feature gap
# 5. Parse errors (35 tests) - Parser improvements needed
#
# After Week 1 Fixes:
# - Day 1: Type consistency ✓ (fixes arithmetic)
# - Day 2: Stack allocation ✓ (10-100x performance)
# - Day 3: IR verification ✓ (catches errors early)
# - Day 4: Test runner ✓ (systematic validation)
# - Day 5: Return type fix (would fix 3 more tests → 8/73 = 10.9%)
#
################################################################################
