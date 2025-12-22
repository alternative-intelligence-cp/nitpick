# AddressSanitizer (ASAN) Quick Reference

## What is AddressSanitizer?

AddressSanitizer (ASAN) is a fast memory error detector that finds:
- Out-of-bounds accesses (heap, stack, globals)
- Use-after-free
- Use-after-return
- Use-after-scope
- Double-free, invalid free
- Memory leaks

## Enabling ASAN

### Build with ASAN

```bash
rm -rf build/
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON ..
make
```

### Run Tests with ASAN

```bash
ctest --output-on-failure
```

Or run test runner directly:
```bash
./tests/test_runner
```

## ASAN Output

When ASAN detects an error, it will print a detailed report:

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x60300000eff0
READ of size 4 at 0x60300000eff0 thread T0
    #0 0x... in function_name file.cpp:42
    #1 0x... in main file.cpp:100
...
=================================================================
```

## Common Errors and Fixes

### Memory Leak

**Error:**
```
ERROR: LeakSanitizer: detected memory leaks

Direct leak of 100 byte(s) in 1 object(s) allocated from:
    #0 0x... in malloc
    #1 0x... in my_function file.cpp:42
```

**Fix:** Make sure to `delete` or `free` allocated memory.

### Use-After-Free

**Error:**
```
ERROR: AddressSanitizer: heap-use-after-free
```

**Fix:** Don't access memory after it has been freed. Consider using smart pointers (`std::unique_ptr`, `std::shared_ptr`).

### Buffer Overflow

**Error:**
```
ERROR: AddressSanitizer: heap-buffer-overflow
```

**Fix:** Check array bounds. Don't write past the end of buffers.

### Stack Buffer Overflow

**Error:**
```
ERROR: AddressSanitizer: stack-buffer-overflow
```

**Fix:** Check array indices on stack-allocated arrays.

## ASAN Environment Variables

Control ASAN behavior with environment variables:

```bash
# Halt on first error (default: don't halt)
ASAN_OPTIONS=halt_on_error=1 ./tests/test_runner

# Continue on errors (useful for testing)
ASAN_OPTIONS=halt_on_error=0 ./tests/test_runner

# Check for leaks
ASAN_OPTIONS=detect_leaks=1 ./tests/test_runner

# Don't check for leaks (faster)
ASAN_OPTIONS=detect_leaks=0 ./tests/test_runner

# Multiple options
ASAN_OPTIONS=halt_on_error=0:detect_leaks=1 ./tests/test_runner
```

## Performance Impact

ASAN has ~2x slowdown:
- Tests will run slower but catch more bugs
- Use for development and testing
- Disable for benchmarks and production

## Integration with Tests

Tests automatically use ASAN when built with `-DUSE_ASAN=ON`. The test configuration sets appropriate ASAN options:

```cmake
set_tests_properties(unit_tests PROPERTIES
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1:halt_on_error=0"
)
```

## Best Practices

1. **Run tests with ASAN regularly** - Catch bugs early
2. **Fix ASAN errors immediately** - They indicate real bugs
3. **Don't ignore ASAN warnings** - They're not false positives
4. **Use ASAN during development** - Catches bugs before they become problems
5. **Test with ASAN in CI/CD** - Prevents memory bugs from reaching production

## Limitations

ASAN cannot detect:
- Race conditions (use ThreadSanitizer)
- Logic errors
- Undefined behavior (use UndefinedBehaviorSanitizer)

## Combining with Other Sanitizers

```bash
# Use ASAN + UndefinedBehaviorSanitizer
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" \
      ..
```

Note: Cannot combine ASAN with ThreadSanitizer (they conflict).

## Debugging ASAN Errors

1. **Read the stack trace** - Shows where error occurred
2. **Check the line number** - ASAN points to exact location
3. **Look at allocation site** - ASAN shows where memory was allocated
4. **Use debugger** - Run with `gdb` to get more context:
   ```bash
   gdb --args ./tests/test_runner
   (gdb) run
   ```

## Resources

- [ASAN Wiki](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [ASAN Documentation](https://clang.llvm.org/docs/AddressSanitizer.html)
- [ASAN FAQ](https://github.com/google/sanitizers/wiki/AddressSanitizerFAQ)
