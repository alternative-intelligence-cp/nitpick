# Aria Compiler IR Optimization Analysis
**Date**: December 3, 2025  
**Version**: v0.0.6  
**Status**: Baseline Performance Assessment

## Current IR Quality

### ✅ Working Well

1. **Control Flow**
   - Proper PHI nodes in loops (`%"$" = phi i64 [ 0, %entry ], [ %next_val, %loop_body ]`)
   - Correct basic block structure
   - Branch conditions properly formed
   - No unnecessary branching

2. **Loop Optimization**
   - Till loops use PHI nodes for iterator (SSA form correct)
   - While loops have proper condition checking
   - When loops correctly handle then/end blocks
   - Break/continue properly implemented

3. **Pattern Matching**
   - Pick statements generate efficient comparison chains
   - Range checks optimized (single comparison for each bound)
   - No redundant comparisons

4. **Assignment Operations**
   - Compound assigns properly load, operate, store
   - No unnecessary intermediate values
   - Proper address calculation for heap vs stack

### ⚠️ Optimization Opportunities

#### 1. Type Consistency Issues (HIGH PRIORITY)
**Problem**: Mixed type arithmetic without proper conversion
```llvm
%addtmp = add i8 %3, i64 3  ; ❌ INVALID: mixing i8 and i64
```

**Expected**:
```llvm
%3_ext = sext i8 %3 to i64
%addtmp = add i64 %3_ext, 3
; OR
%3_i8 = trunc i64 3 to i8
%addtmp = add i8 %3, %3_i8
```

**Fix**: Ensure all arithmetic operands are same type. Current type promotion code exists but isn't being applied consistently.

**Impact**: Critical - invalid IR may cause LLVM backend failures

#### 2. Excessive GC Allocation for Locals (MEDIUM PRIORITY)
**Problem**: Every local variable allocates heap memory
```llvm
%0 = call ptr @get_current_thread_nursery()
%1 = call ptr @aria_gc_alloc(ptr %0, i64 1)
%x = alloca ptr, align 8
store ptr %1, ptr %x, align 8
```

**Expected** (for local int8):
```llvm
%x = alloca i8, align 1
```

**Fix**: Stack-allocate primitive types by default. Only heap-allocate when:
- Variable escapes function scope
- Explicitly marked as `wild` or `gc`
- Is a large struct/array

**Impact**: Major performance improvement (10-100x for tight loops)

#### 3. Alignment Mismatches (MEDIUM PRIORITY)
**Problem**: Inconsistent alignment for same memory location
```llvm
store i64 5, ptr %2, align 4   ; Storing i64 with align 4
%3 = load i8, ptr %x, align 1  ; Loading i8 with align 1
```

**Expected**:
```llvm
store i8 5, ptr %x, align 1    ; Consistent i8 with align 1
%3 = load i8, ptr %x, align 1
```

**Fix**: Track actual type size and use appropriate alignment

**Impact**: Moderate - may cause performance issues on some architectures

#### 4. Redundant Load/Store Sequences (LOW PRIORITY)
**Problem**: Multiple loads/stores from same address within same basic block
```llvm
store i64 5, ptr %2, align 4
%3 = load i8, ptr %x, align 1  ; Could use known value 5
```

**Expected**: LLVM optimizer should handle this (mem2reg pass)

**Fix**: Run LLVM optimization passes or improve codegen

**Impact**: Low - LLVM already optimizes this

#### 5. Function Prologue Overhead (LOW PRIORITY)
**Problem**: Every function declares runtime functions
```llvm
declare ptr @get_current_thread_nursery()
declare ptr @aria_gc_alloc(ptr, i64)
```

**Expected**: Declare once per module

**Fix**: Move declarations to module init

**Impact**: Low - just bloats IR, doesn't affect runtime

### 🔧 Recommended Fixes (Prioritized)

1. **CRITICAL**: Fix type consistency in BinaryOp codegen
   - Ensure both operands have same LLVM type
   - File: src/backend/codegen.cpp, line ~900-970
   
2. **HIGH**: Implement stack allocation for primitive locals
   - Add escape analysis to determine allocation strategy
   - Only heap-allocate when necessary
   - File: src/frontend/sema/escape_analysis.cpp (already exists!)

3. **MEDIUM**: Fix alignment calculations
   - Use getLLVMType() to get proper type
   - Calculate alignment from type size
   - File: src/backend/codegen.cpp, variable declaration section

4. **LOW**: Enable LLVM optimization passes
   - Add PassManager with basic optimizations
   - mem2reg, instcombine, simplifycfg
   - File: src/backend/codegen.cpp, runCodegen()

## Performance Estimates

### Current Performance (Baseline)
- Simple loop (100 iterations): ~1000 ns (estimated)
- GC overhead per allocation: ~50 ns
- Function call overhead: ~10 ns

### After Optimizations
| Optimization | Expected Improvement |
|--------------|---------------------|
| Fix type consistency | Correctness (prevents crashes) |
| Stack-allocate locals | 10-100x faster (eliminates GC calls) |
| Fix alignment | 5-10% faster (better cache utilization) |
| LLVM passes | 20-50% faster (constant folding, dead code elimination) |

**Total expected improvement**: 10-100x for tight loops, 2-5x for typical code

## IR Size Analysis

### Current IR Characteristics
- Function prologue: ~10 lines (GC setup)
- Simple assignment: ~8 lines
- Loop iteration: ~15 lines
- Function epilogue: ~3 lines

### After Optimization
- Function prologue: ~2 lines (stack alloc)
- Simple assignment: ~2 lines
- Loop iteration: ~10 lines
- Function epilogue: ~1 line

**IR size reduction**: ~40% smaller IR, ~50% fewer runtime calls

## Next Steps

1. Run `llc` on generated IR to verify it compiles to assembly
2. Enable LLVM verification pass to catch IR errors
3. Implement stack allocation optimization
4. Add optimization flag (`-O0`, `-O1`, `-O2`, `-O3`)
5. Benchmark before/after optimizations

## Test Coverage for Optimizations

Current tests verify correctness but not performance:
- ✅ Functional correctness tests exist
- ❌ No performance benchmarks
- ❌ No IR quality tests
- ❌ No optimization level comparisons

**Recommendation**: Add `tests/benchmarks/` directory with:
- Loop performance tests
- Memory allocation benchmarks
- IR quality assertions
