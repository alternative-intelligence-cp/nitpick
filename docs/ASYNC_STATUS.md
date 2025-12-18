# Phase 4.5.3: Async/Await Implementation Status

**Reference**: research_029_async_await_system.txt

## Current State (20% Complete)

### ✅ Completed
1. **AST Nodes** (include/frontend/ast/expr.h)
   - `AwaitExpr` class with operand
   - `LambdaExpr` has `isAsync` flag
   - `FuncDeclStmt` has `isAsync` flag

2. **Parser** (src/frontend/parser/)
   - `async` keyword lexing
   - Basic await expression parsing  
   - Async function declaration parsing

3. **Semantic Analysis** (include/frontend/sema/async_analyzer.h)
   - `AsyncAnalyzer` class
   - `analyzeAwaitExpr()` method
   - Basic validation structure

### ⏳ In Progress
- None currently

### ❌ Not Started - Core Requirements
1. **Backend/Codegen** (CRITICAL PATH)
   - LLVM coroutine intrinsics integration
   - State machine lowering (switch generation)
   - CoroutineFrame struct generation
   - Future<T> trait implementation

2. **Type System**
   - Future<T> generic type
   - Result<T> integration with Future
   - TBB sticky error propagation across await

3. **Runtime** (Can defer initially)
   - M:N work-stealing scheduler
   - Wild Affinity pinning
   - RAMP optimization
   - Reactor/I/O integration

## Implementation Strategy

### Phase 1: Minimal Viable Async (Week 1-2)
**Goal**: Get `async func` and `await` working with LLVM coroutines

1. Create Future<T> trait in type system
2. Implement basic coroutine frame generation
3. Use LLVM's `@llvm.coro.*` intrinsics
4. Test: Simple async function that awaits another async function

### Phase 2: State Machine Polish (Week 3)
5. Proper error handling across await points
6. TBB sentinel propagation
7. Test: Async with TBB arithmetic

### Phase 3: Runtime Integration (Week 4+)
8. Basic single-threaded executor
9. Task spawn/await
10. Defer: Full M:N scheduler, Wild Affinity, RAMP

## Key Design Decisions

**NIL vs void**: 
- `void` for FFI/C interop
- `NIL` for native Aria (matches NULL convention)
- Both map to LLVM void type

**LLVM Coroutines**:
- Use `@llvm.coro.id`, `@llvm.coro.begin`, `@llvm.coro.suspend`, `@llvm.coro.end`
- Generate switch-based state machine
- Frame allocation: Start simple (always heap), optimize later with RAMP

**Future Trait**:
```aria
trait Future<Output> {
   func:poll = Result<Output>(Context$:ctx);
}
```

## Next Steps
1. Read LLVM coroutine documentation
2. Create test file for async/await
3. Implement Future<T> type
4. Generate LLVM coroutine IR for simple async function
5. Test execution

## Test Coverage Goals
- [ ] Async function declaration
- [ ] Simple await expression
- [ ] Async function calling async function
- [ ] Error propagation with ?
- [ ] TBB arithmetic across await
- [ ] Async lambda/closure
- [ ] Multiple await points in sequence
- [ ] Concurrent await (when runtime ready)

---
Status as of: December 17, 2025
Closures: 100% ✅
Async/Await: 20% ⏳
