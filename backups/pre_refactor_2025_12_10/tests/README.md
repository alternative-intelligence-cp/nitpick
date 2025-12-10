# Aria Compiler Test Suite

Automated testing infrastructure for the Aria compiler v0.0.6.

## Quick Start

```bash
# Run all tests
./run_tests.sh

# Run with verbose output
./run_tests.sh -v

# Run specific tests (pattern matching)
./run_tests.sh stack
./run_tests.sh while

# Skip IR verification (for debugging)
./run_tests.sh --no-verify

# Stop at first failure
./run_tests.sh --stop-on-fail
```

## Test Organization

```
tests/
├── run_tests.sh              # Main test runner
├── TEST_EXPECTATIONS.md      # Expected test results and known issues
├── TEST_SUITE_v0.0.6.md     # Test documentation
├── test_*.aria               # Test source files (73 tests)
└── /tmp/aria_test_output/   # Test outputs and logs
```

## Test Categories

### Core Language (5 passing)
- `test_stack_optimization` - Stack allocation for primitives ✓
- `test_template_simple` - Template strings ✓
- `test_template_string` - String expansion ✓
- `test_while_loops` - While loops with break/continue ✓
- `test_is_nested` - Nested conditionals ✓

### Control Flow (8 tests)
- While loops (5 tests)
- When loops (1 test)
- Till loops (2 tests)

### Pattern Matching (11 tests)
- Pick statements with ranges, comparisons, fall-through

### Functions & Lambdas (15 tests)
- Function declarations
- Lambda expressions
- Module-level functions

### Type System (4 tests)
- Type promotion
- Unwrap operator
- Result types

### Memory Management (3 tests)
- Borrow checking
- Escape analysis
- Stack/heap allocation

### Operators (3 tests)
- Assignment operators
- Arithmetic operators
- Decrement operator

## Test Status

**Week 1, Day 4 Baseline:**
- Total: 73 tests
- Passing: 5 (6.8%)
- Failing: 68 (93.2%)

See `TEST_EXPECTATIONS.md` for detailed breakdown of failures and fixes.

## Common Issues

### Return Type Mismatches (3 tests)
```
Error: Function return type does not match operand type of return inst!
Fix: Type-aware return value generation (Day 5 task)
```

### Parse Errors (35 tests)
```
Error: Parse Error at line X
Fix: Parser improvements for various syntax forms
```

### Pick Statement Crashes (11 tests)
```
Error: Aborted (core dumped)
Fix: Pick statement codegen debugging
```

## Adding New Tests

1. Create `test_feature_name.aria` in tests/
2. Run `./run_tests.sh feature_name` to test
3. Update `TEST_EXPECTATIONS.md` if needed

## Output Files

Test runner creates:
- `/tmp/aria_test_output/<test_name>.ll` - Generated LLVM IR
- `/tmp/aria_test_output/<test_name>.log` - Compilation output

## Exit Codes

- `0` - All tests passed
- `1` - One or more tests failed

Use in CI/CD:
```bash
./run_tests.sh || exit 1  # Fail build on test failure
```

## Verbose Mode

Shows detailed compilation output for each test:

```bash
$ ./run_tests.sh -v stack

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Aria Compiler Test Suite
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Found 1 test files

▶ TEST test_stack_optimization
Result: ✓ PASS - compiled successfully

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Test Summary
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Total tests:   1
Passed:        1
Failed:        0

✓ All tests passed!
```

## Pattern Filtering

Run subset of tests by pattern:

```bash
./run_tests.sh while        # All while loop tests
./run_tests.sh pick         # All pick statement tests
./run_tests.sh lambda       # All lambda tests
./run_tests.sh test_stack   # Exact name match
```

## Integration with CI/CD

### GitHub Actions Example
```yaml
- name: Run Aria Test Suite
  run: |
    cd tests
    ./run_tests.sh || exit 1
```

### Make Target Example
```makefile
test:
	cd tests && ./run_tests.sh

test-verbose:
	cd tests && ./run_tests.sh -v

test-pattern:
	cd tests && ./run_tests.sh $(PATTERN)
```

## Development Workflow

1. Make compiler changes
2. Rebuild: `cd build && make`
3. Run tests: `cd tests && ./run_tests.sh`
4. Fix failures
5. Repeat

Quick iteration:
```bash
# Terminal 1: Watch for changes
while true; do make -C build; sleep 1; done

# Terminal 2: Run tests
cd tests && ./run_tests.sh -v
```

## Week 1 Progress

- **Day 1**: Type consistency ✓ (fixed arithmetic type mismatches)
- **Day 2**: Stack allocation ✓ (10-100x performance improvement)
- **Day 3**: IR verification ✓ (catches errors at compile-time)
- **Day 4**: Test runner ✓ (systematic validation) ← YOU ARE HERE
- **Day 5**: Polish and fixes (return types, alignment, etc.)

## Next Steps

Priority fixes to increase test pass rate:
1. Fix return type generation (3 tests → 10.9%)
2. Fix pick statement crashes (11 tests → 25%)
3. Implement for loop parser (2 tests → 28%)
4. Fix lambda syntax (9 tests → 40%)

Target for Week 1: 40%+ pass rate
Target for v0.1.0: 90%+ pass rate
