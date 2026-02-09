# atomic<T> Implementation - Path Forward Assessment

**Date**: February 7, 2026  
**Status**: Phase 2.2 Complete, Phase 2.3 Blocked  

## What We've Accomplished

### ✅ Phase 2.2: Type Constraints (COMPLETE)
- Compile-time type validation works perfectly
- `atomic<int32>` ✅ accepted  
- `atomic<string>` ❌ rejected with clear error
- All 13 lock-free types (int8-64, uint8-64, bool, tbb8-64) validated
- Type system integration complete
- No regressions in existing tests

**Achievement**: The *type* `atomic<T>` now exists in the Aria type system and is properly validated.

### Example Working Code
```aria
func:test_valid_atomics = int32() {
    stack atomic<int32>:a;    // ✅ Type-checks correctly
    stack atomic<bool>:b;     // ✅ Type-checks correctly
    stack atomic<tbb32>:c;    // ✅ Type-checks correctly
    return 0;
};
```

## The Problem

The original Phase 2.3 plan assumes language features that don't exist yet:

### Original Plan (from TODO)
```
Phase 2.3: Map atomic<T> methods to runtime calls
- Transform: atomic<int32>::new(0) → aria_atomic_int32_create(0)
- Transform: counter.store(42, RELEASE) → aria_atomic_int32_store(...)
- Handle class methods, imports, default parameters
```

### Missing Prerequisites
1. **No class/struct methods** - Aria has structs but not methods yet
2. **No module/import system** - Can't `import stdlib::atomic`
3. **No associated functions** - Can't call `atomic<int32>::new()`
4. **No method call syntax** - Can't call `counter.store()`

The `stdlib/atomic.aria` file is **aspirational** - it shows what the API *should* look like when these features exist.

## Options for Moving Forward

### Option 1: Mark as "Structurally Complete" ✅ (RECOMMENDED)
**Status**: atomic<T> type system integration complete, waiting on language features

**What's Done**:
- Type validation ✅
- Memory ordering keywords ✅
- MemoryOrder enum ✅  
- Error messages ✅
- Test infrastructure ✅

**What's Waiting**:
- Method syntax (future language feature)
- Module system (future language feature)
- Actual runtime implementation (blocked on above)

**Advantage**: Accurate status, clear path forward, no wasted effort

**Next Steps**:
1. Document what's complete
2. Create "waiting on language features" list
3. Move to another P0 feature that doesn't have blockers

### Option 2: Create C Runtime Library + Manual Bindings
**Implement**: Runtime atomic operations in C, manually bind in Aria

**Effort**: ~8-10 hours

**Code Would Look Like**:
```aria
// Instead of: counter.store(42, release)
// We'd have: aria_atomic_int32_store(counter, 42, 2) 

extern func aria_atomic_int32_create(int32) = @void;
extern func aria_atomic_int32_store(@void, int32, int32) = void;
// ...etc for all operations

func:test = int32() {
    @void:counter = aria_atomic_int32_create(0);
    aria_atomic_int32_store(counter, 42, 2 # RELEASE
);    return 0;
};
```

**Advantages**:
- Tests actual atomicity/concurrency
- Proves runtime integration works
- Useful for learning compiler internals

**Disadvantages**:
- Ugly API (no type safety, manual memory management)
- All work will be replaced when language features land
- Doesn't advance core language development

### Option 3: Hybrid Approach
**Implement**: Core runtime in C, but focus on OTHER P0 features first

1. Create minimal C runtime (`runtime/atomic.c` with ~200 lines)
2. Create one simple binding test to prove it works
3. **Then move to P0 features that ARE ready**:
   - Error propagation with TBB types
   - Handle<T> capability system
   - Safe reference tracking
   - Pattern matching exhaustiveness

**Advantages**:
- Proves atomic runtime concept works
- Moves forward on other critical features
- Better use of development time

**Disadvantages**:
- Still some throwaway code

## Recommendation

**Go with Option 1**: Mark atomic<T> as structurally complete.

**Reasoning**:
1. We've accomplished the hard part (type system integration)
2. The blockers are language-level features, not atomic-specific issues
3. These language features will benefit ALL of Aria, not just atomics
4. Better ROI to focus on features that ARE ready

**Concrete Next Steps**:
1. Create `ATOMIC_TYPE_SYSTEM_COMPLETE.md` documenting achievements  
2. Create `WAITING_ON_LANGUAGE_FEATURES.md` listing blockers
3. Move to next P0 feature:
   - **TBB error propagation** (✅ types exist, needs operator support)
   - **Handle<T> capability system** (✅ spec ready, needs implementation)
   - **Pattern matching** (✅ some syntax exists, needs exhaustiveness)

## What Should We Do?

**Your call!** I can:

**A)** Mark atomic<T> complete, document achievements, move to TBB/Handle/patterns  
**B)** Create minimal C runtime to prove the concept (~2 hours)  
**C)** Full manual binding implementation (~10 hours)  
**D)** Something else you have in mind

Which direction makes the most sense for the Aria project roadmap?
