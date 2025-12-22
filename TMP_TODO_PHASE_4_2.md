# Phase 4.2: Extended Memory Features - Detailed Breakdown

**Status**: Not Started  
**Estimated Time**: 15-20 hours  
**Dependencies**: Phase 4.1 complete ✅, Generics parser (for testing)

---

## Stage 1: Research & Design (2-3 hours)

### 1.1 Result Type Research (45 min)
- [ ] Study Rust's `Result<T, E>` pattern
- [ ] Review existing Aria error handling mechanisms
- [ ] Design `result<T, error>` type for Aria
- [ ] Plan integration with wild pointer allocations
- [ ] Sketch API: `result<wild T@, alloc_error> alloc_result<T>()`

**Deliverable**: Design document for result types

### 1.2 Arena Allocator Research (60 min)
- [ ] Study arena allocation patterns (Rust, Zig, C++)
- [ ] Research memory block chaining strategies
- [ ] Design `Arena` type API
- [ ] Plan bulk allocation + single deallocation
- [ ] Consider alignment requirements for mixed-type arenas

**Deliverable**: Arena allocator API specification

### 1.3 Custom Allocator Patterns (45 min)
- [ ] Research pool allocator (fixed-size blocks)
- [ ] Research slab allocator (kernel-style caching)
- [ ] Design allocator trait/interface
- [ ] Plan allocator composition patterns
- [ ] Sketch realloc implementation strategy

**Deliverable**: Custom allocator design document

---

## Stage 2: Result Type Integration (2-3 hours)

### 2.1 Error Type Definition (30 min)
- [ ] Create `lib/std/core/error.aria` module
- [ ] Define `alloc_error` enum (OutOfMemory, InvalidAlignment, etc.)
- [ ] Implement error message functions
- [ ] Add error propagation helpers

**Files**: `lib/std/core/error.aria`

### 2.2 Result Type Implementation (60 min)
- [ ] Create `lib/std/core/result.aria` module
- [ ] Define `result<T, E>` generic type (or placeholder)
- [ ] Implement `is_ok()`, `is_err()`, `unwrap()`, `expect()`
- [ ] Add pattern matching helpers
- [ ] Document usage patterns

**Files**: `lib/std/core/result.aria`

### 2.3 Memory API Integration (60 min)
- [ ] Add result-based allocation functions to `memory.aria`
  - `alloc_result<T>() -> result<wild T@, alloc_error>`
  - `alloc_array_result<T>(count) -> result<wild T@, alloc_error>`
  - `alloc_aligned_result<T>(align) -> result<wild T@, alloc_error>`
- [ ] Update documentation with error handling patterns
- [ ] Add usage examples

**Files**: `lib/std/core/memory.aria`, `docs/stdlib/memory_api.md`

---

## Stage 3: Arena Allocators (3-4 hours)

### 3.1 Arena Type Design (60 min)
- [ ] Define `Arena` struct in `lib/std/core/arena.aria`
- [ ] Plan memory block structure (linked list vs array)
- [ ] Design block size growth strategy (fixed vs exponential)
- [ ] Add capacity tracking fields
- [ ] Document ownership semantics

**Files**: `lib/std/core/arena.aria`

### 3.2 Runtime Implementation (90 min)
- [ ] Implement `aria_arena_create()` in C++
- [ ] Implement `aria_arena_alloc()` with alignment
- [ ] Implement `aria_arena_destroy()` (free all blocks)
- [ ] Add block chaining logic
- [ ] Register functions in IR generator

**Files**: 
- `include/runtime/allocators.h`
- `src/runtime/allocators/arena_alloc.cpp`
- `src/backend/ir/codegen_stmt.cpp`

### 3.3 Aria Wrapper Functions (45 min)
- [ ] Implement `Arena.new() -> Arena`
- [ ] Implement `Arena.alloc<T>() -> wild T@`
- [ ] Implement `Arena.alloc_array<T>(count) -> wild T@`
- [ ] Implement `Arena.destroy()`
- [ ] Add reset() for reusing arena without deallocation

**Files**: `lib/std/core/arena.aria`

### 3.4 Arena Testing (45 min)
- [ ] C++ unit tests (15 tests)
  - Basic allocation
  - Multiple allocations
  - Block chaining
  - Bulk deallocation
  - Alignment handling
- [ ] Aria integration tests (3 tests)
  - Simple arena usage
  - Mixed-type allocations
  - Nested arenas

**Files**: 
- `tests/runtime/test_arena_allocator.cpp`
- `tests/stdlib/test_arena_*.aria`

---

## Stage 4: Custom Allocators (4-5 hours)

### 4.1 Pool Allocator (120 min)
- [ ] Design `PoolAllocator<T>` type (fixed-size blocks)
- [ ] Implement free list data structure
- [ ] Runtime: `aria_pool_create(block_size, capacity)`
- [ ] Runtime: `aria_pool_alloc()` and `aria_pool_free()`
- [ ] Aria wrapper functions
- [ ] C++ tests (10 tests)
- [ ] Aria integration tests (2 tests)

**Files**:
- `lib/std/core/pool.aria`
- `src/runtime/allocators/pool_alloc.cpp`
- `tests/runtime/test_pool_allocator.cpp`
- `tests/stdlib/test_pool_*.aria`

### 4.2 Slab Allocator (120 min)
- [ ] Design `SlabAllocator` type (kernel-style caching)
- [ ] Implement slab cache structure
- [ ] Runtime: `aria_slab_create(object_size, slab_size)`
- [ ] Runtime: `aria_slab_alloc()` and `aria_slab_free()`
- [ ] Add per-CPU cache simulation (optional)
- [ ] C++ tests (10 tests)
- [ ] Aria integration tests (2 tests)

**Files**:
- `lib/std/core/slab.aria`
- `src/runtime/allocators/slab_alloc.cpp`
- `tests/runtime/test_slab_allocator.cpp`
- `tests/stdlib/test_slab_*.aria`

### 4.3 Realloc Implementation (60 min)
- [ ] Implement `aria_realloc()` in wild_alloc.cpp
- [ ] Handle size increase (new alloc + memcpy)
- [ ] Handle size decrease (no-op or shrink)
- [ ] Add overflow protection
- [ ] Aria wrapper: `realloc<T>(ptr, new_count) -> wild T@`
- [ ] C++ tests (8 tests)
- [ ] Aria integration test (1 test)

**Files**:
- `src/runtime/allocators/wild_alloc.cpp`
- `lib/std/core/memory.aria`
- `tests/runtime/test_memory_phase_4_1.cpp` (add realloc section)
- `tests/stdlib/test_memory_realloc.aria`

---

## Stage 5: Performance & Benchmarking (2-3 hours)

### 5.1 Benchmark Infrastructure (60 min)
- [ ] Create `tools/benchmarks/` directory
- [ ] Implement timing utilities (high-resolution clock)
- [ ] Design benchmark harness
- [ ] Add statistical analysis (mean, stddev, percentiles)
- [ ] Create output formatting (CSV, JSON, human-readable)

**Files**: `tools/benchmarks/benchmark_harness.cpp`

### 5.2 Memory Allocation Benchmarks (90 min)
- [ ] Benchmark: Wild allocation vs GC allocation
- [ ] Benchmark: Single alloc/free pairs
- [ ] Benchmark: Bulk allocations
- [ ] Benchmark: Arena vs individual allocations
- [ ] Benchmark: Pool allocator performance
- [ ] Benchmark: Aligned allocation overhead
- [ ] Generate performance report

**Files**: 
- `tools/benchmarks/bench_memory.cpp`
- `docs/performance/memory_benchmarks.md`

### 5.3 Memory Tracking Enhancement (30 min)
- [ ] Distinguish aligned vs regular allocations in stats
- [ ] Add allocation source tracking (debug builds)
- [ ] Implement `ARIA_TRACK_ALLOCATIONS` compile flag
- [ ] Update statistics reporting
- [ ] Document tracking overhead

**Files**: `src/runtime/allocators/wild_alloc.cpp`

---

## Stage 6: Platform Testing & Documentation (2-3 hours)

### 6.1 Windows Platform Testing (90 min)
- [ ] Set up Windows test environment (WSL or VM)
- [ ] Fix _aligned_free issue for aligned allocations
- [ ] Add allocation type tracking (aligned flag)
- [ ] Run full test suite on Windows
- [ ] Fix platform-specific bugs
- [ ] Document Windows-specific considerations

**Files**: `src/runtime/allocators/wild_alloc.cpp`, `docs/platform/windows.md`

### 6.2 macOS Platform Testing (45 min)
- [ ] Test on macOS (if available)
- [ ] Verify posix_memalign behavior
- [ ] Run full test suite
- [ ] Document macOS-specific notes

**Files**: `docs/platform/macos.md`

### 6.3 Valgrind & Sanitizer Testing (30 min)
- [ ] Run all tests under Valgrind
- [ ] Run with AddressSanitizer (-fsanitize=address)
- [ ] Run with MemorySanitizer (-fsanitize=memory)
- [ ] Fix any detected issues
- [ ] Document sanitizer findings

**Files**: `docs/testing/sanitizers.md`

### 6.4 Documentation & Integration (45 min)
- [ ] Update `docs/stdlib/memory_api.md` with Phase 4.2 features
- [ ] Create `docs/stdlib/arena_api.md`
- [ ] Create `docs/stdlib/allocators_api.md`
- [ ] Update `lib/std/STATUS.md`
- [ ] Create `docs/phases/PHASE_4_2_COMPLETE.md`
- [ ] Update main `TODO.md` to mark Phase 4.2 complete

---

## Success Criteria

**Must Have** (Phase 4.2 incomplete without these):
- [ ] Result type implemented with error handling
- [ ] Arena allocator functional with tests
- [ ] Pool allocator implemented
- [ ] Realloc function working
- [ ] All C++ tests passing
- [ ] Windows aligned allocation fixed
- [ ] Documentation complete

**Should Have** (Important but can defer):
- [ ] Slab allocator implemented
- [ ] Performance benchmarks run
- [ ] Valgrind clean on all tests
- [ ] macOS platform tested

**Nice to Have** (Can defer to Phase 4.3+):
- [ ] Per-CPU cache for slab allocator
- [ ] Custom allocator composition
- [ ] Allocation profiling tools

---

## Known Dependencies

**Blockers**:
- Generics parser (`<T>` syntax) for testing Aria code
- Result type implementation depends on error type design

**Parallelizable Work**:
- Arena allocator can be developed independently
- Pool and slab allocators can be parallel
- Benchmarks can run after any allocator is complete

---

## Risk Assessment

**High Risk**:
- Windows _aligned_free bug may require runtime tracking overhead
- Generic syntax still placeholder (limits testing)

**Medium Risk**:
- Arena block chaining complexity
- Realloc edge cases (shrink, grow, alignment preservation)
- Performance regression vs current wild allocations

**Low Risk**:
- Result type pattern well-established in other languages
- Pool/slab allocators are well-understood algorithms

---

## Time Estimates by Stage

| Stage | Description | Estimated Time | Priority |
|-------|-------------|----------------|----------|
| 1 | Research & Design | 2-3 hours | CRITICAL |
| 2 | Result Type Integration | 2-3 hours | HIGH |
| 3 | Arena Allocators | 3-4 hours | HIGH |
| 4 | Custom Allocators | 4-5 hours | MEDIUM |
| 5 | Performance & Benchmarking | 2-3 hours | MEDIUM |
| 6 | Platform Testing & Docs | 2-3 hours | HIGH |
| **TOTAL** | | **15-21 hours** | |

---

## Notes

- Stage 1 is critical - good design saves implementation time
- Consider doing Stage 2 (Result types) completely before Stage 3
- Stages 3 and 4 can partially overlap (arena vs pool/slab)
- Stage 5 benchmarks can run incrementally as allocators complete
- Stage 6 Windows testing is HIGH priority due to known bug

---

**Ready to begin?** Start with Stage 1, Task 1.1 when you're ready to proceed.
