# Next Steps After v0.0.10 (TBB Complete)

**Date**: December 11, 2025  
**Current Version**: v0.0.10  
**Status**: TBB arithmetic complete, 2 Gemini research reports received

---

## Research Reports Received

### ✅ research_001: Borrow Checker (406 lines)
**Location**: `docs/gemini/responses/research_001_borrow_checker.txt`

**Key Insights**:
- Comprehensive architectural specification for Aria Borrow Checker (ABC)
- Formalizes "Appendage Theory" (Host/Appendage relationship)
- Hybrid memory model: Stack, GC Heap, Wild Heap
- Operator semantics: `@` (address-of), `$` (safe ref), `#` (pin)
- Lifetime analysis algorithm
- Scope-weighted control flow graphs (SW-CFG)
- Flow-sensitive analysis
- Typestate tracking

**Complexity**: **CRITICAL** - Most complex subsystem in Aria
**Estimated Implementation**: 4-6 weeks

### ✅ research_002: Balanced Ternary (612 lines)
**Location**: `docs/gemini/responses/research_002_balanced_ternary_arithmetic.txt`

**Key Insights**:
- Mathematical foundations of balanced ternary (digit set {-1, 0, 1})
- Radix economy analysis (base-3 optimal)
- 10 trits packed into uint16 (59,049 states)
- Symmetric negation (no carry propagation)
- Unified signedness (inherently signed)
- Binary ↔ Ternary conversion algorithms
- Arithmetic emulation strategies

**Complexity**: **MODERATE** - Well-defined algorithms, similar to TBB
**Estimated Implementation**: 2-3 weeks

### ⏳ research_003: Balanced Nonary (Queued)
**Status**: Queued for execution in a few hours
**Expected**: Similar to ternary but with 5 nits in uint16

---

## Implementation Priority Decision

### Option 1: Balanced Ternary (Phase 1.2) - RECOMMENDED
**Why this first**:
1. ✅ **Unblocks Phase 1 completion** - Core type system
2. ✅ **Lower complexity** - Similar to TBB work we just completed
3. ✅ **Momentum** - Keep building on type system expertise
4. ✅ **Clear scope** - Well-defined algorithms, no external dependencies
5. ✅ **Quick win** - Can complete in 2-3 weeks

**What needs doing**:
- [ ] Extract algorithms from research_002 into design doc
- [ ] Implement `trit` type (single digit: -1, 0, 1)
- [ ] Implement `tryte` type (10 trits in uint16)
- [ ] Create packing/unpacking functions
- [ ] Implement ternary arithmetic (add/sub/mul/div)
- [ ] Create binary ↔ ternary conversions
- [ ] Write comprehensive test suite
- [ ] Integrate with type checker and codegen

**Timeline**: 2-3 weeks → v0.0.11

### Option 2: Borrow Checker (Phase 2.1) - DEFER
**Why defer**:
1. ⚠️ **Highest complexity** - 4-6 week implementation
2. ⚠️ **Foundational change** - Affects entire compiler architecture
3. ⚠️ **Requires sustained focus** - Can't be done in fragments
4. ⚠️ **Risk of stalling** - Better to complete Phase 1 first

**What needs doing** (when we start it):
- [ ] Extract formal specification from research_001
- [ ] Design lifetime representation in AST
- [ ] Implement scope depth tracking
- [ ] Build control flow graph (CFG)
- [ ] Create dominator tree analysis
- [ ] Implement Appendage Theory checks
- [ ] Add operator validation (@, $, #)
- [ ] Memory region interaction verification
- [ ] Wild pointer leak detection
- [ ] Comprehensive error messages

**Timeline**: 4-6 weeks → v0.1.0 (major milestone)

---

## Recommended Action Plan

### Immediate (Today/Tomorrow):
1. ✅ **Merge TBB to main** - DONE (v0.0.10)
2. ✅ **Commit research reports** - DONE
3. 🔄 **Process research_002** into design document
   - Extract key algorithms
   - Create implementation checklist
   - Document packing strategy
   - Define API surface

### Short-term (This Week):
4. 📝 **Create dev/balanced-ternary branch**
5. 🔨 **Implement trit type**
   - TypeKind::TRIT
   - Type checker integration
   - Basic validation
6. 🔨 **Implement tryte type**
   - TypeKind::TRYTE
   - Packing algorithm (10 trits → uint16)
   - Unpacking algorithm
7. ✅ **Test basic storage**

### Medium-term (Next 2-3 Weeks):
8. 🔨 **Implement ternary arithmetic**
   - Addition (with carry logic)
   - Subtraction (via negation)
   - Multiplication
   - Division
9. 🔨 **Binary ↔ Ternary conversions**
10. ✅ **Comprehensive test suite**
11. 📝 **Merge to main** → v0.0.11

### After Balanced Ternary:
12. ⏳ **Process research_003** (nonary - when received)
13. 🔨 **Implement balanced nonary** (nit/nyte)
14. ✅ **Complete Phase 1** (All core types done)
15. 🔥 **Start borrow checker** (Phase 2) - The big one!

---

## Why This Order?

### Building Momentum
- TBB → Ternary → Nonary = Natural progression
- Each reinforces understanding of exotic arithmetic
- Completes one major phase (Phase 1) before tackling another

### Risk Management
- Balanced ternary is **finite scope** (can complete it)
- Borrow checker is **open-ended** (could stall development)
- Better to have 3 exotic types done than 1 type + half a borrow checker

### User Value
- Core type system completion = immediate user benefit
- Users can write ternary/nonary code right away
- Borrow checker is invisible until you need wild pointers

### Mental Model
- Finish "type system brain" before switching to "safety brain"
- Both are complex domains - better to complete one at a time

---

## Success Metrics

### v0.0.11 (Balanced Ternary Complete)
- [ ] `trit` and `tryte` types compile
- [ ] Ternary arithmetic passes test suite
- [ ] Conversions between binary/ternary work
- [ ] Documentation complete
- [ ] Example programs demonstrating ternary advantage

### v0.0.12 (Balanced Nonary Complete)
- [ ] `nit` and `nyte` types compile
- [ ] Nonary arithmetic passes test suite
- [ ] Phase 1 (Core Type System) 100% complete

### v0.1.0 (Borrow Checker Complete) - MAJOR MILESTONE
- [ ] Lifetime analysis working
- [ ] Appendage Theory enforced
- [ ] Safe reference (`$`) validated
- [ ] Pin operator (`#`) correctly handles GC interaction
- [ ] Wild pointer safety verified
- [ ] Phase 2 (Memory Safety) complete

---

## Current Branch Status

```
main (v0.0.10) ← HEAD
  - TBB complete
  - Research reports integrated
  - Clean state, ready for next feature

dev/tbb-arithmetic (merged)
  - Can be deleted

Next: dev/balanced-ternary
  - Start fresh from main
  - Target: v0.0.11
```

---

## Questions for Randy

1. **Agree with ternary-first approach?** Or prefer to tackle borrow checker now?
2. **Timeline constraints?** Need something done quickly, or can sustain 2-3 week push?
3. **Interest level?** More excited about ternary math or safety systems?

My recommendation: **Start balanced ternary**. It's tractable, builds on TBB momentum, and completes a major phase. The borrow checker deserves our full attention when we're ready to commit 4-6 weeks.
