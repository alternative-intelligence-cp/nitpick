# Borrow Checker Test Suite

This directory contains comprehensive tests for Aria's borrow checker implementation.

## Test Categories

### Passing Tests (Should Compile Successfully)
- `test_immutable_aliasing.aria` - Multiple immutable borrows (N immutable refs allowed)
- `test_pinning_basic.aria` - Basic # operator pinning
- `test_address_of.aria` - Basic @ operator address-of
- `test_nested_borrows.aria` - Borrow tracking across scopes
- `test_mixed_operators.aria` - All three operators ($, #, @) together
- `test_borrow_chains.aria` - Multiple borrows of same variable
- `test_annotations_populated.aria` - Verify AST annotations are set

### Failing Tests (Should Reject with Borrow Errors)
*To be added once mutable borrow syntax is finalized:*
- Double mutable borrow (should fail)
- Mutable + immutable mix (should fail)
- Modifying pinned variables (should fail)
- Wild memory leaks (should fail)
- Use after free (should fail)

## Current Implementation Status

**✅ Working:**
- Immutable borrows with `$` operator
- Pinning with `#` operator
- Address-of with `@` operator
- AST annotations for borrow tracking
- Borrow checker integration in compilation pipeline

**🔧 In Progress:**
- Mutable borrow syntax design
- Violation detection and error reporting
- Scope-based lifetime validation

## Running Tests

```bash
# Compile and run a single test
./build/ariac tests/borrow/test_immutable_aliasing.aria && ./a.out

# Expected output: All tests should pass and print success messages
```

## Borrow Checker Architecture

The borrow checker operates in Phase 3.5 of the compilation pipeline:
1. **Phase 3**: Type checking annotates AST nodes
2. **Phase 3.5**: Borrow checker validates memory safety rules
3. **Phase 4**: IR generation proceeds if no borrow errors

### AST Annotations

**VarDeclStmt annotations:**
- `scope_depth`: Scope level for Appendage Theory validation
- `requires_drop`: True for wild/stack variables needing cleanup
- `is_pinned_shadow`: True if this variable is a pin reference
- `pinned_target`: Name of pinned variable (if is_pinned_shadow)

**UnaryExpr annotations:**
- `creates_loan`: True for `$` (borrow) operator
- `loan_target`: Variable name being borrowed
- `creates_pin`: True for `#` (pin) operator
- `pin_target`: Variable name being pinned
