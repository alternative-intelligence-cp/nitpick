# Research 017 - Mathematical Types: File Upload Priority List

**Task**: Advanced Mathematical Types (tensor, matrix)  
**Max Additional Files**: 5 (after 5 required context files)  
**Strategy**: Prioritize tensor/matrix implementation files over stdlib

## Required Context Files (Already Specified - 5 files)
1. ✅ `docs/info/aria_specs.txt` (847 lines)
2. ✅ `docs/gemini/responses/research_015_composite_types_part2.txt` (252 lines)
3. ✅ `docs/gemini/responses/research_013_floating_point_types.txt` (639 lines)
4. ✅ `src/backend/codegen_vector.cpp` (310 lines)
5. ✅ `src/backend/codegen_vector.h` (116 lines)

**Subtotal**: 5 files, 2,164 lines

## Available Additional Files (5 slots)

### Analysis of Candidates:

**Tensor/Matrix Implementation**:
- `src/backend/tensor_ops.h` (287 lines) - Full Tensor<T> template class ⭐
- `src/runtime/types/tensor.h` (12 lines) - Runtime Tensor descriptor
- `tests/tensor/test_tensor_ops.cpp` (346 lines) - Comprehensive test suite
- `tests/demo_matrix.aria` (168 lines) - Aria matrix examples

**Stdlib Files**:
- `src/stdlib/io.aria` (194 lines) - Not relevant to math types
- `src/stdlib/io/fast_read.cpp` (149 lines) - Not relevant
- `src/stdlib/log/logger.aria` (137 lines) - Not relevant
- `src/stdlib/math/ternary.aria` (66 lines) - Ternary math (tangential)

### Priority Ranking for 5 Slots:

## Final Selection (5 additional files)

### Tier 1: Core Tensor Implementation (CRITICAL)
1. **`src/backend/tensor_ops.h`** (287 lines) ⭐ MUST HAVE
   - Complete Tensor<T> template class
   - Shape, stride calculation, memory layout
   - Element access, slicing, broadcasting
   - Row-major memory layout implementation
   - **Why**: Answers ALL tensor architecture questions

2. **`src/runtime/types/tensor.h`** (12 lines) ⭐ MUST HAVE
   - Runtime Tensor descriptor struct
   - dtype, rank, shape, strides fields
   - **Why**: Shows type system integration, only 12 lines

### Tier 2: Examples and Tests (HIGH Priority)
3. **`tests/tensor/test_tensor_ops.cpp`** (346 lines)
   - Comprehensive tensor operation tests
   - Broadcasting examples
   - Slicing examples
   - Performance benchmarks
   - **Why**: Shows actual usage patterns and edge cases

4. **`tests/demo_matrix.aria`** (168 lines)
   - Aria-native matrix examples
   - Matrix multiplication syntax
   - Linear algebra operations
   - **Why**: Shows user-facing API and syntax

### Tier 3: Tangential but Useful (MEDIUM Priority)
5. **`src/stdlib/math/ternary.aria`** (66 lines)
   - Ternary math operations
   - Shows stdlib math style
   - **Why**: Context for mathematical stdlib design, small file

**Total Additional**: 5 files, 879 lines

## NOT Uploaded (stdlib irrelevant to math types):
- `src/stdlib/io.aria` (194 lines) - I/O, not math
- `src/stdlib/io/fast_read.cpp` (149 lines) - I/O, not math
- `src/stdlib/log/logger.aria` (137 lines) - Logging, not math

## Final Upload List (10 files total)

### Required Context (5 files):
1. docs/info/aria_specs.txt
2. docs/gemini/responses/research_015_composite_types_part2.txt
3. docs/gemini/responses/research_013_floating_point_types.txt
4. src/backend/codegen_vector.cpp
5. src/backend/codegen_vector.h

### Mathematical Types Files (5 files):
6. src/backend/tensor_ops.h ⭐ CRITICAL (287 lines)
7. src/runtime/types/tensor.h ⭐ CRITICAL (12 lines)
8. tests/tensor/test_tensor_ops.cpp (346 lines)
9. tests/demo_matrix.aria (168 lines)
10. src/stdlib/math/ternary.aria (66 lines)

**Grand Total**: 10 files, 3,043 lines

## Key Coverage

### Fully Answered by Uploaded Files:
✅ N-dimensional tensor implementation (tensor_ops.h)  
✅ Shape encoding (runtime in Tensor struct)  
✅ Stride calculation (row-major, computeStrides() method)  
✅ Memory layout (row-major C-style)  
✅ Element access (at() methods with multi-indexing)  
✅ Tensor slicing (shown in test_tensor_ops.cpp)  
✅ Runtime type descriptor (tensor.h struct)  
✅ Integration with type system (dtype enum)  
✅ Matrix operations (demo_matrix.aria examples)  
✅ Broadcasting (test examples)  

### Needs Design/Specification:
⚠️ BLAS integration (not visible, needs design)  
⚠️ GPU acceleration hooks (not implemented)  
⚠️ ML framework interop (ONNX/TensorFlow/PyTorch)  
⚠️ Sparse matrix support (not visible)  
⚠️ Matrix decompositions (LU, QR, SVD, Cholesky)  
⚠️ Strassen algorithm (not implemented)  
⚠️ Column-major layout option (only row-major visible)  
⚠️ Compile-time shape templates (only runtime shapes visible)  

## Prompt Addition

```
File Selection Notes:
The stdlib/ directory primarily contains I/O and logging (480 lines), not mathematical operations. I've uploaded the 5 most critical tensor/matrix implementation files (879 lines):

Core Tensor Implementation (299 lines):
- tensor_ops.h: Full Tensor<T> template with shape/stride/broadcasting (287 lines)
- tensor.h: Runtime descriptor struct with dtype/rank/shape/strides (12 lines)

Examples and Tests (514 lines):
- test_tensor_ops.cpp: Comprehensive tensor tests with broadcasting/slicing (346 lines)
- demo_matrix.aria: Matrix operations in Aria syntax (168 lines)

Stdlib Context (66 lines):
- math/ternary.aria: Example of mathematical stdlib design patterns

Not uploaded (stdlib files irrelevant to math types):
- io.aria (194 lines), fast_read.cpp (149 lines), logger.aria (137 lines)

Key visible features:
✅ Row-major Tensor<T> template with dynamic shapes
✅ Stride-based indexing for efficient multi-dimensional access
✅ Runtime Tensor descriptor (dtype, rank, shape, strides)
✅ Element access, slicing, and broadcasting examples
✅ Matrix syntax and operations

Questions needing design:
- BLAS/LAPACK integration
- GPU acceleration (CUDA/OpenCL hooks)
- ML framework interop (ONNX/TensorFlow)
- Sparse matrix support
- Matrix decompositions (LU/QR/SVD/Cholesky)
- Compile-time shape templates
- Column-major layout option
```

## Upload Order (Optimal)

1. docs/info/aria_specs.txt
2. research_015_composite_types_part2.txt
3. research_013_floating_point_types.txt
4. codegen_vector.cpp
5. codegen_vector.h
6. **tensor_ops.h** ← Most critical
7. **tensor.h** ← Only 12 lines, quick context
8. test_tensor_ops.cpp
9. demo_matrix.aria
10. math/ternary.aria

---

**Summary**: Replaced "src/stdlib/" with 5 specific files. Prioritized tensor/matrix implementation (879 lines) over stdlib I/O (480 lines). Full tensor architecture visible in tensor_ops.h.
