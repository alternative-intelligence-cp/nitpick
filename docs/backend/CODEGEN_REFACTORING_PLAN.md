# CodeGen Refactoring Plan - Execution Guide

## ✅ PHASE 1 COMPLETE

**Completed:** December 7, 2025
**Status:** Successfully extracted CodeGenContext to separate header

### Results:
- ✅ Created `src/backend/codegen_context.h` (340 lines)
- ✅ Moved `CodeGenContext` class (270 lines)
- ✅ Moved `ScopeGuard` RAII helper (15 lines)
- ✅ Reduced `codegen.cpp` from 4,041 → 3,767 lines (274 lines extracted)
- ✅ Compilation successful
- ✅ Basic functionality verified

### Decision on Further Refactoring:
After analysis, **further file splitting not pursued** due to:
- High coupling between CodeGenVisitor methods
- visitExpr() calls many helper methods within the class
- Risk/benefit ratio favors keeping visitor methods together
- Phase 1 extraction already provides main benefits:
  - Faster recompilation when context changes
  - Clearer separation of concerns
  - Easier to understand symbol/scope management

---

## Original State Analysis

**File:** `src/backend/codegen.cpp`
**Original Size:** 4,041 lines
**Current Size:** 3,767 lines
**Status:** Context extracted, visitor methods remain integrated

### Structure Breakdown

```
Lines    | Component
---------|------------------------------------------
1-89     | Includes and using declarations
90-360   | CodeGenContext class definition
361-375  | ScopeGuard RAII helper class
376-3900 | CodeGenVisitor class (MASSIVE)
3900-4041| Module generation and entry point
```

### CodeGenVisitor Method Categories

| Category | Methods | Lines (Approx) |
|----------|---------|----------------|
| **Declarations** | visit(VarDecl), visit(StructDecl), visit(FuncDecl) | ~500 |
| **Statements** | visit(IfStmt), visit(WhileLoop), visit(Block), etc. | ~700 |
| **Expressions** | visitExpr() - handles ALL expression types | ~1,400 |
| **Control Flow** | visit(PickStmt), visit(TillLoop), visit(ForLoop) | ~600 |
| **Helpers** | createSyscall(), runtime function declarations | ~400 |

## Refactoring Strategy

### Phase 1: Extract Context (IMMEDIATE - Low Risk)
**Goal:** Move CodeGenContext to separate header
**Risk:** LOW - No behavioral changes
**Benefit:** Reduces main file size, enables reuse

#### Steps:
1. Create `src/backend/codegen_context.h`
2. Move CodeGenContext class definition
3. Move ScopeGuard class definition
4. Add necessary includes
5. Update `codegen.cpp` to include new header
6. **Verify compilation**

### Phase 2: Extract Expression Generation (Next - Medium Risk)
**Goal:** Move visitExpr() to separate file
**Risk:** MEDIUM - Large method with many dependencies
**Benefit:** ~1,400 lines removed from main file

#### Steps:
1. Create `src/backend/codegen_expr.cpp`
2. Create `src/backend/codegen_expr.h`
3. Move `visitExpr()` method implementation
4. Add `CodeGenVisitor` friend declaration if needed
5. Update CMakeLists.txt
6. **Verify compilation**

### Phase 3: Extract Statement Generation (Later - Medium Risk)
**Goal:** Move statement visitors to separate file
**Risk:** MEDIUM - Many visitor methods
**Benefit:** ~700 lines removed

#### Steps:
1. Create `src/backend/codegen_stmt.cpp`
2. Create `src/backend/codegen_stmt.h`
3. Move statement visitor implementations
4. Update CMakeLists.txt
5. **Verify compilation**

### Phase 4: Extract Declaration Handling (Later - Low Risk)
**Goal:** Move declaration visitors to separate file  
**Risk:** LOW - Relatively independent
**Benefit:** ~500 lines removed

#### Steps:
1. Create `src/backend/codegen_decl.cpp`
2. Create `src/backend/codegen_decl.h`
3. Move declaration visitor implementations
4. Update CMakeLists.txt
5. **Verify compilation**

### Phase 5: Extract Helper Functions (Final - Low Risk)
**Goal:** Move runtime function declarations and syscall helpers
**Risk:** LOW - Utility functions
**Benefit:** ~400 lines removed, better organization

#### Steps:
1. Create `src/backend/codegen_runtime.cpp`
2. Create `src/backend/codegen_runtime.h`
3. Move runtime helper functions
4. Update CMakeLists.txt
5. **Verify compilation**

## Final Structure

```
src/backend/
├── codegen.h                    # Public API (unchanged)
├── codegen.cpp                  # Main orchestration (~500 lines)
├── codegen_context.h            # CodeGenContext class
├── codegen_expr.cpp/h           # Expression generation
├── codegen_stmt.cpp/h           # Statement generation
├── codegen_decl.cpp/h           # Declaration handling
├── codegen_runtime.cpp/h        # Runtime helpers
├── codegen_tbb.cpp/h            # TBB arithmetic (existing)
└── tbb_optimizer.cpp/h          # TBB optimizations (existing)
```

## Benefits After Completion

### Maintainability
- ✅ Smaller, focused files (each <1000 lines)
- ✅ Easier to navigate and understand
- ✅ Clearer separation of concerns

### Collaboration
- ✅ Reduced merge conflicts
- ✅ Multiple developers can work simultaneously
- ✅ Easier code reviews

### Compilation
- ✅ Faster incremental builds
- ✅ Parallel compilation of modules
- ✅ Better header dependency tracking

### Testing
- ✅ Unit testing of individual components
- ✅ Isolated bug fixes
- ✅ Easier to add new features

## Risk Mitigation

### For Each Phase:
1. ✅ Create git branch before changes
2. ✅ Move code, don't rewrite
3. ✅ Maintain exact semantics
4. ✅ Compile after each step
5. ✅ Run test suite
6. ✅ Git commit when verified

### Rollback Plan:
- Each phase is independently committable
- Can rollback to any phase if issues arise
- All changes are additive (new files)

## Current Status

- [x] Phase 0: Analysis and planning (THIS DOCUMENT)
- [ ] Phase 1: Extract CodeGenContext (NEXT)
- [ ] Phase 2: Extract Expression Generation
- [ ] Phase 3: Extract Statement Generation
- [ ] Phase 4: Extract Declaration Handling
- [ ] Phase 5: Extract Helper Functions

## Execution Timeline

**Immediate (Today):**
- Phase 1: Extract CodeGenContext (~30 minutes)

**Short-term (This Week):**
- Phase 2: Extract Expression Generation (~2 hours)
- Phase 3: Extract Statement Generation (~2 hours)

**Medium-term (Next Week):**
- Phase 4: Extract Declaration Handling (~1 hour)
- Phase 5: Extract Helper Functions (~1 hour)

## Notes

### Why Not Do It All At Once?
- **Risk Management**: Incremental changes allow verification at each step
- **Compilation Safety**: Easier to identify breaking changes
- **Git History**: Clear commits showing logical progression
- **Team Workflow**: Smaller PRs are easier to review

### Alternative Approach (Not Recommended):
- Complete rewrite from scratch
- Risk: High chance of introducing bugs
- Time: Much longer
- Benefit: None (same final result)

### Dependencies to Watch:
- CodeGenVisitor methods call each other
- Context is passed by reference everywhere
- Visitor pattern requires proper inheritance
- LLVM IR builders are stateful

## Success Criteria

✅ **After Phase 1:**
- codegen_context.h exists
- codegen.cpp includes it
- Compilation succeeds
- File size reduced by ~300 lines

✅ **After Complete Refactoring:**
- All 6 phases complete
- codegen.cpp < 1,000 lines
- All tests passing
- No behavior changes
- Faster compilation times

---

**Document Version:** 1.0
**Date:** December 7, 2025
**Status:** Planning Complete - Ready for Phase 1 Execution
