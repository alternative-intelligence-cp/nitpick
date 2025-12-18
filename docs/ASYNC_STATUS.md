# Phase 4.5.3: Async/Await Implementation Status

**Current Progress: 35%** 🚀

**Reference**: research_029_async_await_system.txt (535 lines)

## Overview

Async/await support for Aria, enabling non-blocking concurrent code with LLVM coroutines.
This implements the state machine transformation detailed in research_029.

---

## Current State

### ✅ COMPLETE (35%)

#### 1. Parser & AST (100%)
- ✅ `async` keyword lexing and parsing
- ✅ `await` keyword lexing and parsing
- ✅ `AwaitExpr` AST node with operand
- ✅ `FuncDeclStmt::isAsync` flag
- ✅ Semantic validation via `AsyncAnalyzer`

#### 2. LLVM Coroutine Intrinsics (100%)
All 8 intrinsics implemented in `StmtCodegen`:
- ✅ `@llvm.coro.id` - Generate coroutine identity token
- ✅ `@llvm.coro.size.i64` - Get frame allocation size
- ✅ `@llvm.coro.begin` - Mark coroutine start, return handle
- ✅ `@llvm.coro.save` - Prepare for suspension
- ✅ `@llvm.coro.suspend` - Suspend execution (returns 0/1/-1)
- ✅ `@llvm.coro.end` - Mark coroutine completion
- ✅ `@llvm.coro.free` - Get memory to free
- ✅ `@llvm.coro.resume` - Resume suspended coroutine

#### 3. Async Function Codegen (100%)
**File**: `src/backend/ir/codegen_stmt.cpp`

Prologue (Entry Block):
- ✅ Detect async via `FuncDeclStmt::isAsync`
- ✅ Return `i8*` (coroutine handle) instead of actual type
- ✅ Call `@llvm.coro.id` with alignment and null pointers
- ✅ Get frame size with `@llvm.coro.size.i64`
- ✅ Allocate frame with `malloc` (TODO: RAMP optimization)
- ✅ Begin coroutine with `@llvm.coro.begin`
- ✅ Create `coro.suspend` and `coro.cleanup` blocks

Epilogue (Suspend & Cleanup Blocks):
- ✅ Final suspend point (`coro.suspend` block)
- ✅ Save state with `@llvm.coro.save`
- ✅ Suspend with `final=true` flag
- ✅ Switch on suspend result for cleanup
- ✅ Cleanup block: free frame with `@llvm.coro.free` + `free()`
- ✅ End coroutine with `@llvm.coro.end`
- ✅ Return coroutine handle

#### 4. Await Expression Codegen (100%)
**File**: `src/backend/ir/codegen_expr.cpp`

- ✅ `codegenAwait()` implementation
- ✅ Evaluate operand (Future<T> or coroutine handle)
- ✅ Resume awaited coroutine with `@llvm.coro.resume`
- ✅ Save state with `@llvm.coro.save`
- ✅ Suspend with `@llvm.coro.suspend(save, final=false)`
- ✅ Create `await.resume` and `await.cleanup` blocks
- ✅ Switch on suspend result (0=resume, 1=destroy)
- ✅ Set insertion point to resume block for continuation

---

### 🚧 IN PROGRESS (0%)

#### Testing & Validation
- [ ] Create real async execution test
- [ ] Verify LLVM coroutine pass transforms IR correctly
- [ ] Validate state machine generation
- [ ] Test multiple await points

---

### ❌ NOT STARTED (65%)

#### Type System (Deferred)
- [ ] Future<T> trait with poll() method
- [ ] Result<T> integration with Future
- [ ] Promise type for completion

#### Optimizations (Deferred)
- [ ] RAMP: Stack-to-heap promotion only on actual suspension
- [ ] Custom CoroutineFrame struct generation
- [ ] Variable spilling optimization

#### Runtime (Deferred - Phase 3+)
- [ ] M:N work-stealing scheduler (Chase-Lev deque)
- [ ] Wild Affinity: Pin tasks with wild pointers
- [ ] Task spawn/await primitives
- [ ] Reactor/I/O integration with 6-stream topology
- [ ] TBB error propagation across await boundaries

---

## Implementation Milestones

### ✅ Milestone 1: Coroutine Infrastructure (COMPLETE)
**Duration**: 2 hours  
**Commits**: 2 (29c47e0, 3f60edd)

- Implemented 8 LLVM coroutine intrinsics
- Added async function prologue/epilogue
- Added await expression suspension points
- **Result**: All 913 tests passing, infrastructure compiles cleanly

### 🚧 Milestone 2: Execution Validation (CURRENT)
**Goal**: Verify async execution works with LLVM coroutine pass

Tasks:
1. [ ] Create async execution test
2. [ ] Dump LLVM IR to verify coroutine markers
3. [ ] Check state machine transformation
4. [ ] Refine based on findings

### ⏳ Milestone 3: Polish & Integration (Next)
**Goal**: Production-ready async/await

Tasks:
- [ ] Error handling across await
- [ ] TBB sentinel propagation
- [ ] Integration tests with closures
- [ ] Documentation and examples

### 🔮 Milestone 4: Runtime & Scheduler (Future)
**Goal**: Full async runtime with M:N scheduling

Tasks:
- [ ] Basic single-threaded executor
- [ ] Task spawn primitives
- [ ] M:N work-stealing scheduler
- [ ] Wild Affinity pinning
- [ ] io_uring integration

---

## Key Design Decisions

### 1. LLVM Coroutines vs Custom State Machine
**Decision**: Use LLVM's built-in coroutine transformation  
**Rationale**: 
- Proven, optimized implementation
- Automatic state machine generation
- Stack analysis and variable spilling
- Integration with optimizer passes

### 2. Frame Allocation Strategy
**Current**: Always heap allocate with `malloc`/`free`  
**Future**: RAMP optimization (Ramp Allocation for Memory Pools)
- Start on stack in "ramp" function
- Promote to heap only if actual suspension occurs
- Zero-cost for synchronous completion

### 3. Type System Integration
**Current**: Defer Future<T> trait, use `i8*` coroutine handles  
**Rationale**: 
- Get core codegen working first
- Add type safety layer later
- Allows incremental development

### 4. NIL vs void
**Decision**: 
- `void` for FFI/C interop (triggers FFI brain patterns)
- `NIL` for native Aria (triggers Aria brain patterns)
- Both map to LLVM void type

---

## Test Coverage

### ✅ Existing Tests (All Passing)
- Parser: 7 async/await parser tests
- Semantic: 11 async context validation tests
- Backend: 5 placeholder async execution tests

### 🎯 Needed Tests
- [ ] Async immediate return (no await)
- [ ] Single await point
- [ ] Multiple await points (state machine)
- [ ] Async lambda/closure
- [ ] Error propagation with ?
- [ ] TBB arithmetic across await
- [ ] Nested async calls

---

## LLVM IR Examples

### Async Function (Target IR)
```llvm
define i8* @async_func() {
entry:
  %id = call token @llvm.coro.id(i32 8, i8* null, i8* null, i8* null)
  %size = call i64 @llvm.coro.size.i64()
  %mem = call i8* @malloc(i64 %size)
  %hdl = call i8* @llvm.coro.begin(token %id, i8* %mem)
  ; ... function body ...
  br label %coro.suspend

coro.suspend:
  %save = call token @llvm.coro.save(i8* %hdl)
  %suspend = call i8 @llvm.coro.suspend(token %save, i1 true)
  switch i8 %suspend, label %coro.cleanup [i8 1, label %coro.cleanup]

coro.cleanup:
  %mem2 = call i8* @llvm.coro.free(token %id, i8* %hdl)
  call void @free(i8* %mem2)
  call i1 @llvm.coro.end(i8* %hdl, i1 false)
  ret i8* %hdl
}
```

### Await Expression (Target IR)
```llvm
  %future = call i8* @async_operation()
  call void @llvm.coro.resume(i8* %future)
  %save = call token @llvm.coro.save(i8* %hdl)
  %suspend = call i8 @llvm.coro.suspend(token %save, i1 false)
  switch i8 %suspend, label %await.resume [
    i8 1, label %await.cleanup
  ]

await.resume:
  ; Continue execution after resumption
  ; Extract value from future (TODO: Future<T> trait)
```

---

## Next Actions

### Immediate (Today)
1. ✅ Update ASYNC_STATUS.md (this file)
2. [ ] Create async execution validation test
3. [ ] Dump IR and verify coroutine markers
4. [ ] Refine implementation based on findings

### Short-term (This Week)
5. [ ] Implement Future<T> trait basics
6. [ ] Add proper result extraction in await
7. [ ] Error propagation tests
8. [ ] Integration with closures

### Medium-term (Next Week)
9. [ ] Basic single-threaded executor
10. [ ] Task spawn primitives
11. [ ] Performance benchmarking

### Long-term (Future Phases)
12. [ ] M:N work-stealing scheduler
13. [ ] Wild Affinity implementation
14. [ ] RAMP optimization
15. [ ] io_uring integration

---

## References

- **research_029_async_await_system.txt**: Complete async/await specification
- **LLVM Coroutines**: https://llvm.org/docs/Coroutines.html
- **Rust async**: Inspiration for Future<T> design
- **C++20 coroutines**: Similar LLVM lowering approach

---

**Status as of**: December 17, 2025, 10:45 PM  
**Last Update**: Completed core coroutine codegen (commits 29c47e0, 3f60edd)  
**All Tests**: 913 passing ✅  
**Next Milestone**: Execution validation
