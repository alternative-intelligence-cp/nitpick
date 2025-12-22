# Phase 4.1 Testing Guide

## Test Coverage

### C++ Unit Tests (`tests/runtime/test_memory_phase_4_1.cpp`)

**Basic Allocation (5 tests)**:
- `memory_alloc_basic` - Allocate, write, read, free
- `memory_alloc_zero` - Zero-size returns NULL
- `memory_alloc_negative` - Negative size returns NULL
- `memory_free_null` - Free NULL is safe
- `memory_alloc_basic` - Pattern writing verification

**Array Allocation (5 tests)**:
- `memory_alloc_array_basic` - Array of 100 int64 values
- `memory_alloc_array_zero_count` - Zero count returns NULL
- `memory_alloc_array_zero_size` - Zero element size returns NULL
- `memory_alloc_array_overflow` - INT64_MAX overflow detection
- `memory_alloc_array_overflow_2` - Large multiplication overflow

**Aligned Allocation (5 tests)**:
- `memory_alloc_aligned_basic` - 64-byte alignment
- `memory_alloc_aligned_various` - Test 8, 16, 32, 64, 128, 256-byte
- `memory_alloc_aligned_zero_init` - Zero-initialization verification
- `memory_alloc_aligned_invalid` - Non-power-of-2 rejected
- `memory_alloc_aligned_zero_alignment` - Zero = default alignment

**String Allocation (2 tests)**:
- `memory_alloc_string_basic` - Allocate with null terminator
- `memory_alloc_string_zero` - Zero-length string handling

**Memory Leak Tests (2 tests)**:
- `memory_no_leaks_simple` - 1000 alloc/free cycles
- `memory_no_leaks_array` - 100 array alloc/free cycles

**Statistics (1 test)**:
- `memory_stats_tracking` - Verify allocation counts

**Edge Cases (3 tests)**:
- `memory_large_allocation` - 100MB allocation
- `memory_many_small_allocs` - 10,000 x 16-byte allocations
- `memory_no_corruption` - Multi-allocation corruption check

**Total: 26 C++ unit tests**

---

### Aria Integration Tests (`tests/stdlib/`)

1. **test_memory_basic.aria**
   - Single value allocation
   - Write and read back
   - Manual free

2. **test_memory_array.aria**
   - Array of 10 integers
   - Fill with pattern (i * 2)
   - Verify all elements
   - Defer cleanup

3. **test_memory_defer.aria**
   - Defer automatic cleanup
   - Early return triggers defer
   - No manual free needed

4. **test_memory_null.aria**
   - is_null() helper function
   - Free null pointer safety
   - OOM handling

5. **test_memory_zeroed.aria**
   - alloc_zeroed() for single value
   - alloc_array_zeroed() for arrays
   - Verify all bytes are zero

**Total: 5 Aria integration tests**

---

## Running Tests

### Build Tests

```bash
cd /home/randy/._____RANDY_____/REPOS/aria
mkdir -p build && cd build
cmake ..
make

# Run C++ unit tests
./tests/runtime/test_memory_phase_4_1

# Expected output:
# ✓ PASS: memory_alloc_basic
# ✓ PASS: memory_alloc_zero
# ... (all tests)
# 
# Summary: 26/26 tests passed
```

### Run Aria Integration Tests

```bash
# Compile each test
./build/ariac tests/stdlib/test_memory_basic.aria -o test_memory_basic
./build/ariac tests/stdlib/test_memory_array.aria -o test_memory_array
./build/ariac tests/stdlib/test_memory_defer.aria -o test_memory_defer
./build/ariac tests/stdlib/test_memory_null.aria -o test_memory_null
./build/ariac tests/stdlib/test_memory_zeroed.aria -o test_memory_zeroed

# Run each test (exit code 0 = success)
./test_memory_basic && echo "✓ Basic test passed"
./test_memory_array && echo "✓ Array test passed"
./test_memory_defer && echo "✓ Defer test passed"
./test_memory_null && echo "✓ Null test passed"
./test_memory_zeroed && echo "✓ Zeroed test passed"
```

### Memory Leak Detection (Valgrind)

```bash
# Install Valgrind (if not already)
sudo apt-get install valgrind

# Run C++ tests with Valgrind
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes \
         ./tests/runtime/test_memory_phase_4_1

# Expected output:
# ==12345== HEAP SUMMARY:
# ==12345==     in use at exit: 0 bytes in 0 blocks
# ==12345==   total heap usage: N allocs, N frees, X bytes allocated
# ==12345== All heap blocks were freed -- no leaks are possible

# Run Aria tests with Valgrind
valgrind --leak-check=full ./test_memory_basic
valgrind --leak-check=full ./test_memory_array
valgrind --leak-check=full ./test_memory_defer
valgrind --leak-check=full ./test_memory_null
valgrind --leak-check=full ./test_memory_zeroed
```

---

## Test Categories

### Correctness Tests ✓
- Allocation succeeds and returns non-NULL
- Written data can be read back correctly
- Array indexing works properly
- Alignment is correct (modulo check)
- Zero-initialization verified

### Error Handling Tests ✓
- Zero-size allocation returns NULL
- Negative size returns NULL
- Overflow detection in array allocation
- Invalid alignment rejected
- Free NULL is safe

### Memory Safety Tests ✓
- No leaks (Valgrind verification)
- No corruption between allocations
- Defer ensures cleanup
- Double-free detection (by borrow checker, not runtime)

### Performance Tests (Not in Phase 4.1)
- ⏸ Benchmark allocation speed
- ⏸ Compare wild vs GC allocation overhead
- ⏸ Arena allocator performance

---

## Known Limitations (Phase 4.1)

1. **Windows Aligned Allocations**:
   - `aria_alloc_buffer` on Windows uses `_aligned_malloc`
   - Should use `_aligned_free` but currently uses `free()`
   - **Workaround**: Track allocations in Phase 4.2
   - **Impact**: May cause issues on Windows (Linux/macOS unaffected)

2. **Statistics Tracking**:
   - Size tracking imprecise (no headers on allocations)
   - Peak usage may be underestimated
   - **Impact**: Stats are approximate, not exact

3. **Generic Syntax**:
   - `@sizeof(T)`, `@cast<T>`, `@extern()` are placeholders
   - Will be replaced when generics parser is complete
   - **Impact**: Tests use simplified syntax for now

---

## Success Criteria

Phase 4.1 is complete when:
- [x] All 26 C++ unit tests pass
- [ ] All 5 Aria integration tests compile and run (exit code 0)
- [ ] Valgrind shows zero memory leaks
- [ ] No use-after-free or double-free errors
- [ ] Tests run successfully on Linux and macOS

---

## Next Steps (Phase 4.2+)

1. Add result<T, error> return types instead of NULL
2. Implement allocation tracking (debug mode)
3. Add arena allocators
4. Add realloc wrapper
5. Implement wildx (executable memory) tests
6. Add benchmarks
7. Test on Windows platform
