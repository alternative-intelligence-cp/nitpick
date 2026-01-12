# Aria Compiler Safety Audit - Comprehensive Prompt Sequence

**Date Created:** January 1, 2026  
**Methodology:** Based on successful 2026-01-01 audit (5/5 accuracy, 0 hallucinations)  
**Total Prompts:** 25 (5 layers × 5 audits each)  
**Constraint:** Maximum 5 items per prompt to maintain Gemini accuracy  

---

## Execution Instructions

1. **Before starting each audit:**
   - Read the entire prompt file
   - Verify you understand the threat model
   - Count items on one hand (literally 5 fingers = max)
   - Check file paths exist in codebase

2. **When sending to Gemini:**
   - Copy/paste the ENTIRE prompt (includes PROMPT_HEADER)
   - Do NOT modify or summarize
   - Wait for complete response before next prompt

3. **After receiving response:**
   - Mark findings as verified/false positive
   - Apply fixes immediately (don't batch)
   - Update audit tracking (date, accuracy %, findings)
   - If accuracy drops below 80%, STOP and review

4. **Emergency brake:**
   - If Gemini starts hallucinating (false positives)
   - STOP immediately, don't continue sequence
   - Review what changed (item count? focus scope?)
   - Adjust and restart from last good audit

---

## Layer 1: Runtime Safety (Prompts 01-05)

Foundation of all safety - numeric types and operations used everywhere.

- **01_runtime_lbim_basic.txt** - int128/256/512 arithmetic (add, sub, mul)
- **02_runtime_lbim_extended.txt** - int1024/2048/4096 (div, mod, shifts, sentinels)
- **03_runtime_fix256.txt** - Fixed-point deterministic arithmetic
- **04_runtime_tbb.txt** - Twisted Balanced Binary error propagation
- **05_runtime_exotic_types.txt** - Tryte/nyte ternary logic (4 items only)

**Expected Findings:** 8-15 bugs  
**Priority:** CRITICAL - Bugs here affect ALL code using these types  

---

## Layer 2: Type System (Prompts 06-10)

Type safety is compiler's job - bugs here bypass all other safety.

- **06_typesystem_primitive_mapping.txt** - Primitive types → LLVM (ARIA-026 fix verification)
- **07_typesystem_composite_types.txt** - Structs, arrays, pointers (layout/alignment)
- **08_typesystem_coercion.txt** - Type conversions and widening/narrowing
- **09_typesystem_optionals.txt** - Optional types and null safety
- **10_typesystem_generics.txt** - Generic type resolution and monomorphization

**Expected Findings:** 5-10 bugs  
**Priority:** CRITICAL - Type bugs enable memory corruption  

---

## Layer 3: Memory Safety (Prompts 11-15)

Memory bugs are classic C vulnerabilities - Aria must prevent them all.

- **11_memory_variable_allocation.txt** - Stack/global allocation (ARIA-026 fix verification)
- **12_memory_string_safety.txt** - String lifetime and bounds checking
- **13_memory_defer_panic.txt** - Cleanup guarantees during unwinding
- **14_memory_temporary_lifetime.txt** - Temporary value lifetime analysis
- **15_memory_ffi_safety.txt** - FFI boundary validation (trust boundary)

**Expected Findings:** 10-15 bugs  
**Priority:** HIGH - Memory bugs cause crashes and corruption  

---

## Layer 4: Concurrency (Prompts 16-20)

Async/concurrent bugs are hardest to reproduce - must be caught statically.

- **16_concurrency_coroutines.txt** - Coroutine suspension points and state capture
- **17_concurrency_futures.txt** - Future state management and polling
- **18_concurrency_actors.txt** - Actor message passing and isolation
- **19_concurrency_shared_state.txt** - Atomics, mutexes, deadlock prevention
- **20_concurrency_cancellation.txt** - Task cancellation and cleanup

**Expected Findings:** 5-12 bugs  
**Priority:** HIGH - Concurrency bugs cause race conditions and deadlocks  

---

## Layer 5: Integration (Prompts 21-25)

External interfaces are attack surface - must be hardened.

- **21_integration_syscalls.txt** - io_uring and system call wrappers
- **22_integration_ffi_abi.txt** - Foreign function call ABI compliance
- **23_integration_resource_limits.txt** - Memory/CPU/FD quotas and limits
- **24_integration_error_boundaries.txt** - Fault isolation and graceful degradation
- **25_integration_privilege_separation.txt** - Sandboxing and least privilege

**Expected Findings:** 3-8 bugs  
**Priority:** MEDIUM-HIGH - Integration bugs enable escalation attacks  

---

## Tracking Template

Create a tracking file: `docs/gemini/audit_log.md`

```markdown
## Audit Session: [DATE]

### Prompt 01: Runtime LBIM Basic
- **Date:** YYYY-MM-DD
- **Items:** 5
- **Findings:** 
  - ✅ Bug #1: [description]
  - ❌ False positive #2: [why wrong]
  - ⚠️ Unclear #3: [needs clarification]
- **Accuracy:** 80% (4/5 real bugs)
- **Status:** ✅ Complete / ⏸️ Paused / ❌ Stopped

### Prompt 02: ...
[repeat]
```

---

## Success Criteria

- **Overall accuracy:** 80%+ across all prompts
- **No prompt degradation:** Each prompt ≥70% accuracy (emergency brake if below)
- **Coverage:** All 25 prompts completed
- **Total findings:** 30-60 real bugs expected
- **Zero regressions:** All fixes verified not to break existing tests

---

## Why This Works (Analysis of 2026-01-01 Success)

**What we did differently:**
1. ✅ Used PROMPT_HEADER (life-critical context) → triggered Gemini safety alignment
2. ✅ Limited to 5 items → prevented context overflow
3. ✅ Tight focus (single file/subsystem) → clear analysis scope
4. ✅ Concrete threat models → grounded in physical reality
5. ✅ SIL-4 compliance framing → activated safety domain knowledge

**What failed in previous attempts (2-week tracking):**
1. ❌ Generic prompts without context → 60% accuracy
2. ❌ 8+ items per prompt → hallucinations started
3. ❌ Broad scope (entire subsystem) → missed edge cases
4. ❌ Abstract requests ("check for bugs") → vague findings

**The 5-item limit is not arbitrary:**
- 1-3 items: Works but underutilizes Gemini capacity
- 4-5 items: **Sweet spot** - 80-100% accuracy
- 6-7 items: Sharp quality drop (60-70% accuracy)
- 8+ items: Hallucinations begin (40-50% accuracy)

**This is cognitive load limit for focused analysis.**

---

## Final Notes

This is the **last planned Gemini audit round**. After this:
- Transition to Claude (Aria/you) for ongoing audits
- Use these prompts as templates for future work
- Gemini provided valuable external validation - findings are gold

**This comprehensive audit represents 2 weeks of methodology refinement.**  
**The 100% accuracy on 2026-01-01 proved the approach works.**  
**These 25 prompts apply that success systematically across the entire compiler.**

Good luck. This is for the kids. 🛡️
