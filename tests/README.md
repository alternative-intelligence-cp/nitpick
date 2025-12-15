# Aria Compiler - Testing Infrastructure

This directory contains the test suite for the Aria compiler.

## Test Structure

```
tests/
├── test_runner.cpp         # Main test runner
├── test_helpers.h          # Test macros and utilities
├── unit/                   # Unit tests for individual components
│   └── test_sample.cpp     # Sample test demonstrating framework
├── integration/            # Integration tests with .aria files
│   └── runner.sh           # Integration test harness
├── benchmarks/             # Performance benchmarks
└── fuzz/                   # Fuzz testing
```

## Writing Tests

### Unit Tests

Use the `TEST_CASE` macro to define tests:

```cpp
#include "../test_helpers.h"

TEST_CASE(my_test_name) {
    // Your test code here
    ASSERT_EQ(1 + 1, 2, "Basic math works");
    ASSERT_TRUE(true, "This should pass");
    ASSERT_FALSE(false, "This should also pass");
    ASSERT_NE(1, 2, "These are different");
}
```

Available assertion macros:
- `ASSERT(condition, message)` - Assert condition is true
- `ASSERT_EQ(actual, expected, message)` - Assert equality
- `ASSERT_NE(actual, expected, message)` - Assert inequality
- `ASSERT_TRUE(condition, message)` - Assert true
- `ASSERT_FALSE(condition, message)` - Assert false

### Integration Tests

Place `.aria` files in `tests/integration/` directory. The integration test runner will:
1. Compile each `.aria` file with `ariac`
2. Run the resulting executable
3. Check the output against expected results

Example integration test:
```aria
// tests/integration/hello.aria
fn main() -> i32 {
    print("Hello, World!");
    return 0;
}
```

## Running Tests

### Quick Test Run

```bash
./test.sh
```

### Manual Test Run

```bash
cd build
make
ctest --output-on-failure
```

### Running Specific Tests

```bash
cd build
./tests/test_runner        # Run all unit tests
```

### Verbose Output

```bash
cd build
ctest -V
```

## Memory Leak Detection (AddressSanitizer)

Enable AddressSanitizer to detect memory leaks and errors:

```bash
rm -rf build/
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON ..
make
ctest
```

ASAN will report:
- Memory leaks
- Use-after-free errors
- Buffer overflows
- Stack corruption
- And more...

## Code Coverage

Generate code coverage report:

```bash
./tools/coverage.sh
```

This will:
1. Clean and rebuild with coverage enabled
2. Run all tests
3. Generate coverage data
4. Create HTML report in `build/coverage_html/`

Open the report:
```bash
xdg-open build/coverage_html/index.html
```

Or manually:
```bash
rm -rf build/
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_COVERAGE=ON ..
make
ctest
make coverage
```

## Adding New Tests

1. Create a new `.cpp` file in `tests/unit/` directory
2. Include `test_helpers.h`
3. Write your tests using `TEST_CASE` macro
4. Add the file to `tests/CMakeLists.txt`:
   ```cmake
   add_executable(test_runner
       test_runner.cpp
       unit/test_sample.cpp
       unit/your_new_test.cpp  # Add this line
   )
   ```
5. Rebuild and run tests

## Integration Test Format

Integration tests should follow this pattern:

```bash
# tests/integration/test_name.aria
# Expected output: (what the program should print)
# Exit code: (expected exit code, default 0)

# Actual .aria code here
```

The integration runner will parse comments for expected output and exit code.

## CI/CD Integration

Tests can be run in CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
- name: Run Tests
  run: |
    ./build.sh
    cd build
    ctest --output-on-failure
```

## Test Organization Guidelines

- **Unit tests**: Test individual functions/classes in isolation
- **Integration tests**: Test complete .aria programs end-to-end
- **Benchmarks**: Measure performance of critical components
- **Fuzz tests**: Test parser with random/malformed input

## Current Test Coverage

As of Phase 0.2:
- ✅ Test framework created
- ✅ Sample unit test added
- ✅ Integration test harness created
- ✅ CMake integration complete
- ✅ ASAN support added
- ✅ Coverage support added

Phase 1+ will add tests for:
- Lexer tokenization
- Parser correctness
- Semantic analysis
- IR generation
- Runtime behavior
- Standard library functions
