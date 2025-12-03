# Aria Semantic Analysis Tests

## Overview
This directory contains tests for Aria's semantic analysis phases:
1. **Borrow Checker** - Memory safety analysis
2. **Escape Analysis** - Stack lifetime analysis
3. **Type Checker** - Type system validation

## Test Files

### test_borrow_checker.aria
Tests the borrow checker's ability to detect:
- Wild allocations without corresponding `free()`
- Multiple wild allocations in same scope
- Wild allocations in nested functions
- Proper handling of stack and heap allocations

**Expected Behavior:**
- ✅ Warnings for wild allocations ('leaked', 'ptr1', 'ptr2', 'nested')
- ✅ No warnings for stack allocations (auto-freed)
- ✅ No warnings for normal heap allocations (GC managed)

**Run Test:**
```bash
./build/ariac tests/test_borrow_checker.aria 2>&1 | grep "Warning"
```

**Expected Output:**
```
Borrow Check Warning: Wild allocation 'leaked' may not be freed. Consider using 'defer aria.free(leaked);'
Borrow Check Warning: Wild allocation 'ptr2' may not be freed. Consider using 'defer aria.free(ptr2);'
Borrow Check Warning: Wild allocation 'ptr1' may not be freed. Consider using 'defer aria.free(ptr1);'
Borrow Check Warning: Wild allocation 'nested' may not be freed. Consider using 'defer aria.free(nested);'
```

### test_escape_analysis.aria
Tests the escape analyzer's ability to detect:
- Stack-allocated variables being returned (use-after-free)
- Addresses of local variables escaping
- Safe returns of heap/wild allocations

**Expected Behavior:**
- ❌ ERROR for returning stack variable
- ⚠️  WARNING for return value referencing local
- ✅ OK for returning heap/wild variables

**Run Test:**
```bash
./build/ariac tests/test_escape_analysis.aria 2>&1 | grep "Escape"
```

**Expected Output:**
```
Escape Analysis Error: Returning stack-allocated variable 'local' which will be destroyed after function returns
Escape Analysis Warning: Return value may reference local stack variable
```

## Semantic Analysis Phases

### Phase 4: Borrow Checking
Implements Aria's "Appendage Theory" memory safety model:
- **Wild pointers** (`wild` keyword): Must be explicitly freed
- **Pinned values** (`#` operator): Cannot be moved by GC
- **Stack allocations** (`stack` keyword): Automatically freed at scope exit
- **Safe references** (`$` operator): Must not outlive pinned hosts

### Phase 4b: Escape Analysis
Detects when local values escape their scope:
- Stack variables returning from functions
- Addresses of locals escaping via arguments
- References outliving their referents

### Phase 4c: Type Checking
Validates type correctness:
- Expression type inference
- Type compatibility in operations
- Function signature matching
- Literal type assignment

## Implementation Status

| Feature | Status | File |
|---------|--------|------|
| Borrow Checker | ✅ Complete | `src/frontend/sema/borrow_checker.cpp` |
| Escape Analysis | ✅ Complete | `src/frontend/sema/escape_analysis.cpp` |
| Type Checker | ✅ Partial | `src/frontend/sema/type_checker.cpp` |

## Known Limitations

1. **Defer Statement**: Parser support for `defer` is incomplete, so tests use direct returns
2. **When Expression**: Not fully implemented in parser yet
3. **Type Checker**: Some visitor methods have simplified implementations
4. **Codegen**: Function calls not yet fully implemented (causes runtime crashes after semantic analysis)

## Future Work

- [ ] Implement `defer` statement parsing
- [ ] Complete type checker for all expression types
- [ ] Add lifetime analysis for references
- [ ] Implement move semantics tracking
- [ ] Add more comprehensive escape scenarios
- [ ] Test safe reference (`$`) to pinned host (`#`) relationships
