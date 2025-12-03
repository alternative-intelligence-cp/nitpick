# Week 1 Action Plan: Critical Bug Fixes
**Date**: December 3-9, 2025  
**Focus**: Correctness & Stability  
**Goal**: Fix all blocking issues for professional release

---

## 🎯 This Week's Objectives

1. ✅ Fix type consistency in arithmetic (CRITICAL)
2. ✅ Implement stack allocation for primitives (HIGH)
3. ✅ Add LLVM IR verification (CRITICAL)
4. ✅ Create automated test runner (HIGH)
5. ✅ Update documentation to reflect fixes

---

## 📋 Day-by-Day Plan

### Day 1 (Today): Type Consistency Fix
**Problem**: Arithmetic mixes i8 and i64 causing invalid LLVM IR

**Tasks**:
- [x] Document the issue (already in IR_OPTIMIZATION_ANALYSIS.md)
- [ ] Fix type promotion in BinaryOp codegen
- [ ] Ensure both operands match before operation
- [ ] Add type consistency tests
- [ ] Verify with LLVM IR validation

**Code Changes**:
File: `src/backend/codegen.cpp`, line ~900-1000

**Test**:
```aria
func:test_type_promotion = int64() {
    int8:a = 5;
    int64:b = 100;
    int64:c = a + b;  // Should promote a to int64
    return c;  // Should be 105
}
```

**Success Criteria**: All arithmetic operations generate valid LLVM IR

### Day 2: Stack Allocation Implementation
**Problem**: All variables heap-allocated, even local primitives

**Tasks**:
- [ ] Review existing escape_analysis.cpp
- [ ] Wire up escape analysis to codegen
- [ ] Stack-allocate non-escaping primitives
- [ ] Only heap-allocate when necessary
- [ ] Test performance improvement

**Code Changes**:
- `src/frontend/sema/escape_analysis.cpp` - Enable
- `src/backend/codegen.cpp` - Use escape analysis results

**Test**:
```aria
func:test_stack_alloc = int8() {
    int8:local = 5;   // Should be stack alloca
    return local;
}

func:test_heap_alloc = int8@() {
    int8:heap = 5;    // Should be heap (escapes)
    return @heap;
}
```

**Success Criteria**: Local primitives use `alloca`, not GC calls

### Day 3: LLVM IR Verification
**Problem**: No automated validation of generated IR

**Tasks**:
- [ ] Add LLVM Verifier pass to codegen
- [ ] Verify module after generation
- [ ] Add `-verify` compiler flag
- [ ] Report verification errors clearly
- [ ] Test with all existing tests

**Code Changes**:
File: `src/backend/codegen.cpp`, add verifier after module generation

```cpp
#include "llvm/IR/Verifier.h"

// After generating module
std::string errors;
llvm::raw_string_ostream errorStream(errors);
if (llvm::verifyModule(*module, &errorStream)) {
    std::cerr << "LLVM IR Verification Failed:\n";
    std::cerr << errors << "\n";
    return nullptr;
}
```

**Success Criteria**: All tests generate verified LLVM IR

### Day 4: Automated Test Infrastructure
**Problem**: Manual testing is error-prone

**Tasks**:
- [ ] Create `tests/run_tests.sh` script
- [ ] Test all .aria files in tests/
- [ ] Validate LLVM IR with llvm-as
- [ ] Generate test report
- [ ] Add color output for pass/fail

**Script Structure**:
```bash
#!/bin/bash
PASSED=0
FAILED=0

for test in tests/unit/*.aria tests/integration/*.aria; do
    echo -n "Testing $(basename $test)... "
    if build/ariac "$test" -o /tmp/test.ll 2>&1 >/dev/null; then
        if llvm-as /tmp/test.ll -o /tmp/test.bc 2>&1 >/dev/null; then
            echo -e "\033[32m✓ PASS\033[0m"
            ((PASSED++))
        else
            echo -e "\033[31m✗ FAIL (invalid IR)\033[0m"
            ((FAILED++))
        fi
    else
        echo -e "\033[31m✗ FAIL (compile error)\033[0m"
        ((FAILED++))
    fi
done

echo ""
echo "Results: $PASSED passed, $FAILED failed"
exit $FAILED
```

**Success Criteria**: Automated test runner reports all results

### Day 5: Alignment & Polish
**Problem**: Alignment mismatches, loose ends

**Tasks**:
- [ ] Fix alignment consistency
- [ ] Run full test suite
- [ ] Fix any remaining issues
- [ ] Update IMPLEMENTATION_STATUS.md
- [ ] Document all fixes in CHANGELOG.md

**Success Criteria**: 
- Zero LLVM verification errors
- All tests passing
- Documentation updated

---

## 📊 Success Metrics

### Technical Metrics
- [ ] LLVM IR passes verification: 100%
- [ ] Test pass rate: 100%
- [ ] Performance improvement: >10x (stack allocation)
- [ ] Code quality: No TODO in critical paths

### Documentation Metrics
- [ ] IMPLEMENTATION_STATUS.md updated
- [ ] CHANGELOG.md created
- [ ] IR_OPTIMIZATION_ANALYSIS.md reflects fixes

---

## 🚨 Blockers & Risks

### Potential Issues
1. **Escape analysis complexity** - May need simplified heuristic first
2. **Type system gaps** - May discover more type issues
3. **Test failures** - May uncover new bugs

### Mitigation
- Start with simple heuristic: stack-allocate all locals that don't escape
- Fix issues as discovered, don't defer
- Document all new bugs in regression tests

---

## 🎯 End-of-Week Deliverables

### Code
- [x] All critical bugs fixed
- [ ] Type-safe arithmetic operations
- [ ] Stack allocation working
- [ ] LLVM IR verification enabled

### Tests
- [ ] Automated test runner
- [ ] All existing tests passing
- [ ] New tests for fixed bugs
- [ ] Test coverage report

### Documentation
- [ ] CHANGELOG.md with this week's fixes
- [ ] Updated IMPLEMENTATION_STATUS.md
- [ ] Updated IR_OPTIMIZATION_ANALYSIS.md

### Validation
- [ ] Zero LLVM verification errors
- [ ] Zero known critical bugs
- [ ] 10-100x performance improvement for locals
- [ ] Professional code quality

---

## 📅 Daily Checklist Template

### Morning (Planning)
- [ ] Review yesterday's progress
- [ ] Set today's priority tasks
- [ ] Check for any blockers

### Afternoon (Execution)
- [ ] Complete planned tasks
- [ ] Run tests continuously
- [ ] Commit working changes

### Evening (Review)
- [ ] Run full test suite
- [ ] Update progress in PROGRESS.md
- [ ] Plan tomorrow's tasks
- [ ] Commit and push

---

## 🔄 Progress Tracking

### Day 1 Progress
- [x] Created RELEASE_ROADMAP.md
- [x] Created WEEK_1_PLAN.md
- [ ] Started type consistency fix

### Day 2 Progress
- [ ] (To be filled)

### Day 3 Progress
- [ ] (To be filled)

### Day 4 Progress
- [ ] (To be filled)

### Day 5 Progress
- [ ] (To be filled)

---

## 🎓 Learning Outcomes

### For Nikola Training
This week establishes the quality bar:
- Correct type handling
- Efficient memory allocation
- Automated validation
- Professional testing

All fixes become part of Nikola's training corpus, ensuring it learns correct patterns from day one.

### For Grant Applications
This week demonstrates:
- Technical excellence
- Systematic problem solving
- Professional development practices
- Quality-first approach

---

**Let's make this week count! Every bug fixed is a step toward professional release.** 🚀
