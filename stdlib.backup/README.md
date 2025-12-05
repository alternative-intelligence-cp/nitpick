# Aria Standard Library and Training Samples

## Overview

This directory contains the Aria standard library written in Aria itself, plus comprehensive training samples for Nikola AGI.

## Structure

```
stdlib/                     - Standard library modules written in Aria
  result.aria              - Result type utilities (isOk, isErr, unwrapOr, etc.)
  math.aria                - Math functions (abs, min, max, gcd, isPrime, etc.)
  memory.aria              - Memory management helpers (allocSafe, copy, fill, etc.)
  collections.aria         - Array/collection operations (sort, search, find, etc.)

training_samples/           - Code samples for training
  01_foundational/         - Basic language features (11 samples)
  02_data_structures/      - Data structures (4 samples)
  03_algorithms/           - Algorithm implementations (4 samples)
  04_systems/              - Systems programming (TBD)
  05_functional/           - Functional programming (2 samples)
  06_real_world/           - Real-world applications (2 samples)
```

## Current Status

**✅ Stdlib Modules: 4**
- `result.aria` - 7 utility functions
- `math.aria` - 9 math functions
- `memory.aria` - 6 memory functions
- `collections.aria` - 11 collection functions

**✅ Training Samples: 21**

### 01_foundational (11 samples)
- `array_basics.aria` - Array operations, loops, function calls
- `result_handling.aria` - Result types, unwrap, error checking
- `math_operations.aria` - Mathematical function usage
- `memory_basics.aria` - Wild memory, defer, RAII patterns
- `loops_all_types.aria` - while, for, till, when loops
- `defer_patterns.aria` - Defer statement, cleanup, LIFO
- `pick_patterns.aria` - Pattern matching, fall, labeled cases
- `lambda_basics.aria` - Lambda expressions, closures
- `type_declarations.aria` - Variables, types, arrays, gc vs wild

### 02_data_structures (4 samples)
- `linked_list.aria` - Linked list implementation with pointers
- `stack.aria` - LIFO stack (array-based)
- `queue.aria` - FIFO queue (circular buffer)
- `sorting_demo.aria` - Bubble sort and quicksort usage

### 03_algorithms (4 samples)
- `binary_search_demo.aria` - Binary search on sorted array
- `fibonacci.aria` - Recursive and iterative implementations
- `merge_sort.aria` - Divide and conquer sorting
- `prime_sieve.aria` - Sieve of Eratosthenes

### 05_functional (2 samples)
- `map_filter_reduce.aria` - Functional programming patterns
- `composition.aria` - Function composition

### 06_real_world (2 samples)
- `calculator.aria` - Simple calculator with error handling
- `statistics.aria` - Mean, median, variance calculations

## Design Philosophy

### Dogfooding
All stdlib code is written in Aria itself, proving the language works for production code.

### Self-hosting Path
This is the foundation for eventual self-hosting (Aria compiler written in Aria).

### Training Quality
Every sample:
- Compiles with ariac
- Follows v0.0.6 spec exactly
- Demonstrates real patterns
- Uses actual stdlib imports
- Includes error handling
- Shows best practices

### Progressive Complexity
Samples organized from simple (foundational) to complex (real-world applications).

## Usage for Nikola Training

These samples provide:
1. **Syntax patterns** - All language constructs demonstrated
2. **Idioms** - Aria-style code patterns
3. **Error handling** - Result types, defer, safe operations
4. **Memory management** - gc vs wild, RAII patterns
5. **Algorithms** - Common implementations
6. **Real-world code** - Practical applications

## Next Steps

- [ ] Add more algorithm samples (graph algorithms, dynamic programming)
- [ ] Create systems programming samples (when I/O ready)
- [ ] Add more real-world applications
- [ ] Test all samples compile with ariac
- [ ] Generate 100+ additional samples for comprehensive coverage

## Compilation

All samples should compile with:
```bash
cd /home/randy/._____RANDY_____/REPOS/aria/build
../ariac ../training_samples/01_foundational/array_basics.aria
```

## License

Same as Aria language project (dual-licensed AGPL-3.0 + Commercial).
