# Code Generation Refactoring Plan

## Overview
Split the monolithic 3,799-line `src/backend/codegen.cpp` into modular components for better maintainability.

## Current Structure Analysis

**Current File:** `src/backend/codegen.cpp` (3,799 lines)

**Major Sections:**
1. **Lines 1-400**: Headers, Context, Type System (~400 lines)
2. **Lines 400-664**: Helper functions, Syscalls (~264 lines)
3. **Lines 665-910**: Declaration visitors (VarDecl, StructDecl, FuncDecl) (~245 lines)
4. **Lines 911-1688**: Statement visitors (Pick, Fall, Loops, If, Defer, Async) (~778 lines)
5. **Lines 1689-3286**: Expression visitor (visitExpr - massive switch) (~1,597 lines)
6. **Lines 3287-3799**: Return, UseStmt, ModDef, externals (~512 lines)

## Proposed Structure

### File 1: `codegen_internal.h` (NEW - Shared Internal Header)
**Purpose:** Internal definitions shared across all codegen modules
**Contents:**
- CodeGenContext class definition
- ScopeGuard class
- Forward declarations
- Allocation strategy enums
- Symbol struct definition

**Why:** Allows all split files to access shared context without circular dependencies

### File 2: `codegen.cpp` (REFACTORED - Main Entry Point)
**Purpose:** Module management, driver, context initialization
**Contents:**
- Main includes
- CodeGenContext implementation (type system, scope management)
- Helper functions (syscalls, allocators)
- Module-level code generation
- Public API: `generate(AST)` function

**Approximate Lines:** ~800

### File 3: `codegen_decl.cpp` (NEW - Declaration Visitors)
**Purpose:** Handle all AST declaration nodes
**Contents:**
- `visit(VarDecl*)`
- `visit(StructDecl*)`  
- `visit(FuncDecl*)` - including async coroutine setup
- `visit(ExternBlock*)`

**Approximate Lines:** ~400

### File 4: `codegen_stmt.cpp` (NEW - Statement Visitors)
**Purpose:** Handle all AST statement nodes
**Contents:**
- `visit(Block*)`
- `visit(IfStmt*)`
- `visit(PickStmt*)` - pattern matching
- `visit(FallStmt*)`
- `visit(TillLoop*)`, `visit(WhileLoop*)`, `visit(ForLoop*)`, `visit(WhenLoop*)`
- `visit(BreakStmt*)`, `visit(ContinueStmt*)`
- `visit(DeferStmt*)`
- `visit(AsyncBlock*)`, `visit(AwaitExpr*)`, `visit(WhenExpr*)`
- `visit(ExpressionStmt*)`

**Approximate Lines:** ~1,000

### File 5: `codegen_expr.cpp` (NEW - Expression Visitor)
**Purpose:** Handle all expression code generation
**Contents:**
- `visitExpr(Expression*)` - main expression dispatcher
- All expression cases:
  - IntLiteral, FloatLiteral, BoolLiteral, StringLiteral
  - VarExpr (variable lookup)
  - CallExpr (function calls)
  - BinaryOp (arithmetic, comparisons, TBB operations)
  - UnaryOp (negation, address-of, dereference)
  - IndexExpr (array access)
  - MemberAccess (struct field access)
  - TernaryExpr
  - LambdaExpr
  - TemplateString
  - ObjectLiteral

**Approximate Lines:** ~1,600

### File 6: `codegen.h` (EXISTING - Public Header)
**No changes - already exists**

### File 7: `codegen_tbb.cpp` (EXISTING - TBB Arithmetic)
**No changes - already properly separated**

## Implementation Strategy

### Phase 1: Create Internal Header
1. Create `codegen_internal.h` with:
   - Forward declare `CodeGenVisitor`
   - Move `CodeGenContext` class definition
   - Move `ScopeGuard` class definition
   - Add `#include "codegen.h"` for public API

### Phase 2: Extract Expression Visitor
1. Create `codegen_expr.cpp`
2. Copy `visitExpr()` method and all dependencies
3. Include `codegen_internal.h`
4. Implement as `CodeGenVisitor::visitExpr()`

### Phase 3: Extract Statement Visitor
1. Create `codegen_stmt.cpp`
2. Copy all statement visit methods
3. Include `codegen_internal.h`
4. Implement as `CodeGenVisitor::visit*()` methods

### Phase 4: Extract Declaration Visitor
1. Create `codegen_decl.cpp`
2. Copy all declaration visit methods
3. Include `codegen_internal.h`

### Phase 5: Refactor Main File
1. Remove extracted methods from `codegen.cpp`
2. Keep context implementation and helpers
3. Add forward declarations for visitor methods
4. Keep main driver logic

### Phase 6: Update Build System
1. Add new `.cpp` files to `CMakeLists.txt`
2. Update dependencies

## Benefits

1. **Maintainability**: Each file < 2,000 lines (target: < 1,000)
2. **Navigation**: Easy to find specific visitor implementations
3. **Compilation**: Parallel compilation of separate units
4. **Merge Conflicts**: Reduced likelihood when multiple devs work on different visitors
5. **Code Review**: Easier to review focused changes
6. **Debugging**: Clearer stack traces with named files

## Risks & Mitigations

**Risk:** Breaking existing code
**Mitigation:** Incremental refactoring with compilation checks after each phase

**Risk:** Circular dependencies
**Mitigation:** Careful header design with `codegen_internal.h` as single point of coupling

**Risk:** Increased compilation time
**Mitigation:** Use forward declarations, minimize includes in headers

## Testing Strategy

1. Compile after each phase
2. Run full test suite after each phase
3. Verify LLVM IR output is identical before/after refactor
4. Performance regression testing (compilation time should improve)

## Rollback Plan

- Git commit after each phase
- Tag known-good state before starting
- Each phase is independently reversible

## Timeline Estimate

- Phase 1 (Internal Header): 30 minutes
- Phase 2 (Expression Visitor): 1 hour
- Phase 3 (Statement Visitor): 1 hour  
- Phase 4 (Declaration Visitor): 30 minutes
- Phase 5 (Main Refactor): 45 minutes
- Phase 6 (Build System): 15 minutes
- **Total:** ~4 hours of careful refactoring

## Success Criteria

- ✅ All new files compile without errors
- ✅ All existing tests pass
- ✅ LLVM IR output identical to original
- ✅ Each file < 2,000 lines
- ✅ No circular dependencies
- ✅ Documentation updated
