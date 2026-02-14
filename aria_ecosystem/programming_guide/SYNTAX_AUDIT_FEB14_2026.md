# Aria Programming Guide Syntax Audit

**Date**: February 14, 2026  
**Status**: Phase 1 (Type System) Complete, Phase 2 (Syntax Cleanup) Starting  
**Auditor**: GitHub Copilot (Claude Sonnet 4.5)  
**Scope**: 359 markdown files in programming_guide/

---

## Executive Summary

**Type documentation is COMPLETE** (50 guides, ~41,273 lines, Sessions 1-26). Now we need to **correct syntax inconsistencies** across the remaining ~309 files before documenting new language features.

**Identified Issues**:
1. **Error handling syntax** - 284 occurrences across 55 files (return Error/Err/Ok → fail/pass)
2. **Lambda operator** - 495 occurrences across 44 files (=> doesn't exist in Aria)
3. **Loop syntax** - 517 occurrences of C-style for loops (should be till/loop)
4. **Result type signatures** - 12 occurrences of Result<T> (should be Result<T,E>)

**High-Priority Files** (need immediate fixing):
- `advanced_features/error_handling.md` - Ironically has old error syntax!
- `README.md` - Front door to the guide (needs to be current)
- `stdlib/*` - Many lambda => examples need correction

---

## Detailed Findings

### 1. Error Handling Syntax (CRITICAL)

**Issue**: Using Rust-style `return Error(...)`, `return Err(...)`, `return Ok(...)` instead of Aria's `fail(...)` and `pass(...)`.

**Scope**:
- **284 total occurrences** across 55 files
- **Affected directories**:
  - `advanced_features/` - 18 files (including error_handling.md!)
  - `operators/` - Question operator examples
  - `modules/` - C interop examples
  - `memory_model/` - Stack examples
  - `stdlib/` - Multiple files

**Example (from error_handling.md:21)**:
```aria
// ❌ WRONG (current):
return Ok(config);

// ✅ CORRECT (Aria syntax):
pass(config);
```

**Fix Strategy**:
```bash
# Automated find/replace (after manual verification):
sed -i 's/return Error(/fail(/g' **/*.md
sed -i 's/return Err(/fail(/g' **/*.md
sed -i 's/return Ok(/pass(/g' **/*.md
sed -i 's/return Success(/pass(/g' **/*.md
```

**Manual Review Required**: Some cases might be in comparison sections or historical notes (don't auto-replace everything blindly).

---

### 2. Lambda Arrow Operator (CRITICAL)

**Issue**: Using `=>` arrow operator (doesn't exist in Aria - use `=` for lambdas).

**Scope**:
- **495 total occurrences** across 44 files
- **Heavily affected**: `stdlib/filter.md` and other higher-order function examples

**Example (from stdlib/filter.md)**:
```aria
// ❌ WRONG (current):
int64[]:evens = filter(numbers, n => n % 2 == 0);

// ✅ CORRECT (Aria syntax - needs full lambda syntax):
int64[]:evens = filter(numbers, func(int64:n) = (n % 2 == 0));
```

**Fix Strategy**:
- **NOT automatable** - Context-dependent conversion
- Need to determine if inline lambda or stored function reference
- May need to add explicit type annotations
- Estimate: ~44 files × 30 minutes each = **22 hours**

**Priority**: HIGH (affects stdlib documentation heavily)

---

### 3. Loop Syntax (MEDIUM PRIORITY)

**Issue**: Using C-style `for` loops instead of Aria's `till` keyword with `$` index variable.

**Scope**:
- **517 occurrences** of `for` loops
- Affects many example code blocks across guide

**Example**:
```aria
// ❌ WRONG (C-style):
for (int i = 0; i < 10; i++) {
    print(i);
}

// ✅ CORRECT (Aria):
till 10 loop
    print($);
end
```

**Fix Strategy**:
- **Semi-automatable** - Pattern recognition for simple loops
- Complex loops need manual restructuring
- Estimate: Review patterns in 10 files, then batch-apply

**Priority**: MEDIUM (doesn't affect correctness as much as readability)

---

### 4. Result Type Signatures (LOW PRIORITY)

**Issue**: Using `Result<T>` instead of `Result<T,E>` (missing error type parameter).

**Scope**:
- **12 occurrences** (mostly in older guides)
- Already fixed in Session 26 Result.md guide

**Example**:
```aria
// ❌ OLD (single parameter):
Result<int64>

// ✅ NEW (explicit error type):
Result<int64, string>
Result<int64, int8>
Result<int64, ERR_TYPE>
```

**Fix Strategy**:
- Manual review (context-dependent - what error type makes sense?)
- Low count, can fix during normal editing

**Priority**: LOW (only 12 occurrences, already documented correctly in new guides)

---

### 5. Type Annotation Order (VERIFICATION NEEDED)

**Issue**: Possible use of `name: Type` instead of Aria's `Type:name`.

**Scope**:
- Only found 2 instances in initial grep
- `functions/function_params.md` has example that might be incorrect

**Example**:
```aria
// ❌ WRONG (Rust/TypeScript style):
fn function_name(param_name: Type) { }

// ✅ CORRECT (Aria):
func:function_name = (Type:param_name) { }
```

**Fix Strategy**:
- Needs manual verification (might be false positives)
- Check function signature guides first

**Priority**: LOW (very few instances found)

---

## Files Requiring Immediate Attention

### Tier 1: High-Visibility (Fix First)

1. **README.md** (11KB)
   - Front door to documentation
   - Needs current status update
   - Last updated: Feb 14 (today), but may have outdated syntax in examples

2. **advanced_features/error_handling.md**
   - **IRONIC**: The error handling guide has old error handling syntax!
   - 10+ instances of `return Ok/Err` found
   - Critical because people will copy these examples

3. **stdlib/filter.md** (and other stdlib/)
   - Heavy use of `=>` lambda syntax
   - People will copy these patterns
   - Needs careful lambda syntax correction

### Tier 2: Category Leaders

4-23. **advanced_features/** directory (32 files total)
   - 18 files have error handling issues
   - Many use `=>` syntax
   - These are reference examples for advanced users

### Tier 3: Batch Corrections

24-55. **Remaining 32 files** with error handling syntax
   - Can fix in batches after establishing patterns from Tier 1-2

---

## Recommended Action Plan

### Session 27: High-Visibility Quick Wins (4-6 hours)

**Goal**: Fix the most-copied examples to prevent syntax propagation.

1. **Fix error_handling.md** (~1 hour)
   - Replace all `return Ok/Err/Error` with `pass/fail`
   - Verify all Result<T,E> examples correct
   - Test examples for consistency

2. **Update README.md** (~30 min)
   - Update status to reflect Session 26 completion
   - Ensure any code examples use correct syntax
   - Add note about syntax standardization in progress

3. **Fix stdlib/filter.md** (~2 hours)
   - Convert all `=>` lambda syntax to proper Aria lambdas
   - Document correct lambda pattern clearly
   - Use as reference for other stdlib files

4. **Document canonical patterns** (~1 hour)
   - Create SYNTAX_REFERENCE.md with correct patterns
   - Use as reference for remaining corrections

**Deliverable**: 3-4 key files fixed, reference document created

---

### Session 28-30: Batch Corrections (15-20 hours)

**Goal**: Fix all error handling and lambda syntax systematically.

1. **Error handling batch** (~5 hours)
   - Use sed for simple replacements (verify each directory first)
   - Manual review of edge cases
   - Test representative files after batch changes

2. **Lambda syntax batch** (~10 hours)
   - More manual work required (context-dependent)
   - Fix stdlib/ files first (establish pattern)
   - Apply pattern to advanced_features/
   - Document common conversion patterns

3. **Loop syntax review** (~3 hours)
   - Identify common for-loop patterns
   - Create till-loop equivalents
   - Script simple cases, manual review complex cases

**Deliverable**: All 55 error-syntax files, 44 lambda files corrected

---

### Session 31+: Language Feature Documentation

**Goal**: Document operators, control flow, contracts with CORRECT syntax from start.

1. **Operators** (~10 hours, 8-10 guides)
   - Arithmetic, comparison, logical, bitwise
   - Special operators (?, ??, ?.)
   - Precedence & associativity reference
   - **USE THESE AS CANONICAL EXAMPLES**

2. **Control Flow** (~12 hours, 8-10 guides)
   - till loops with $ index variable
   - loop...end (infinite/conditional)
   - when guards
   - pick/match statements
   - if/then/else/elseif
   - **REFERENCE FOR LOOP SYNTAX**

3. **Contracts** (~8 hours, 5-6 guides)
   - requires/ensures clauses
   - Invariants
   - Runtime validation
   - Integration with Result<T,E>

**Deliverable**: 20-25 new guides with perfect syntax, usable as references for final cleanup

---

### Session 35+: Final Cleanup & Verification

**Goal**: Ensure ALL 359 files have consistent, correct Aria syntax.

1. **Automated syntax check** (~5 hours)
   - Write grep patterns for all known issues
   - Verify zero occurrences of old syntax
   - Generate compliance report

2. **Cross-reference verification** (~5 hours)
   - Ensure all type links work
   - Verify all examples compile (when compiler ready)
   - Check for orphaned files

3. **Index regeneration** (~3 hours)
   - Update all category indexes
   - Regenerate table of contents
   - Ensure navigation works

**Deliverable**: Complete, syntax-correct programming guide ready for fuzzer/man pages/website

---

## Automation Opportunities

### Can Script (Low Risk)

```bash
# Simple replacements with manual verification:
find . -name "*.md" -exec sed -i 's/return Error(/fail(/g' {} +
find . -name "*.md" -exec sed -i 's/return Err(/fail(/g' {} +
find . -name "*.md" -exec sed -i 's/return Ok(/pass(/g' {} +
```

### Cannot Script (High Risk)

- Lambda `=>` to `=` conversions (context-dependent, need type annotations)
- `for` to `till` loops (logic restructuring required)
- Result<T> to Result<T,E> (need to determine appropriate error type)

### Best Approach

1. **Fix 10 representative files manually** - Establish patterns
2. **Script simple replacements** - Use sed for safe patterns
3. **Manual review post-script** - Verify changes make sense
4. **Test examples** - Ensure code logic preserved

---

## Estimated Timeline

| Phase | Scope | Hours | Sessions (4-6h each) |
|-------|-------|-------|---------------------|
| **Tier 1 Fixes** | 3-4 critical files | 4-6 | 1 session (Session 27) |
| **Error Syntax Batch** | 55 files | 5-8 | 1-2 sessions |
| **Lambda Syntax Batch** | 44 files | 10-15 | 2-3 sessions |
| **Loop Syntax Review** | Unknown files | 3-5 | 1 session |
| **New Feature Docs** | 20-25 guides | 30-40 | 5-7 sessions |
| **Final Verification** | 359 files | 10-15 | 2-3 sessions |
| **TOTAL** | | **62-89 hours** | **12-18 sessions** |

**Optimistic** (with good automation): 12-15 sessions (3-4 weeks at 1 session/day)  
**Realistic** (manual review heavy): 15-18 sessions (4-5 weeks)  
**Conservative** (unexpected issues): 18-20 sessions (5-6 weeks)

---

## Success Metrics

### After Session 27 (High-Visibility)
- ✅ error_handling.md has zero `return Ok/Err/Error`
- ✅ README.md reflects current status (Session 26 complete)
- ✅ stdlib/filter.md demonstrates correct lambda syntax
- ✅ SYNTAX_REFERENCE.md exists as canonical reference

### After Sessions 28-30 (Batch Corrections)
- ✅ Zero grep hits for `return Error|Err|Ok|Success` in code examples
- ✅ Zero grep hits for ` => ` in Aria code (except comparison sections)
- ✅ Common loop patterns documented and converted

### After Sessions 31+ (New Features)
- ✅ Operator guides complete with precedence table
- ✅ Control flow guides complete with till/loop/when/pick
- ✅ Contract guides complete with requires/ensures
- ✅ All new guides have perfect syntax (no corrections needed)

### Final Verification
- ✅ All 359 files pass syntax compliance checks
- ✅ All cross-references resolved
- ✅ Documentation ready for fuzzer generation
- ✅ Documentation ready for man page extraction
- ✅ Documentation ready for website deployment

---

## Risks & Mitigations

### Risk 1: Breaking Working Examples
**Mitigation**: Test representative examples after each batch, maintain backup copies

### Risk 2: Context Loss in Automated Replacements
**Mitigation**: Manual review of scripted changes, fix 10 files manually first to establish patterns

### Risk 3: Scope Creep (Finding More Issues)
**Mitigation**: Document new issues in this audit, but defer to later phases unless critical

### Risk 4: Inconsistent Patterns Discovered
**Mitigation**: Create SYNTAX_REFERENCE.md early, use as single source of truth

### Risk 5: Compiler Not Ready for Testing
**Mitigation**: Verify syntax against existing implementation docs, compiler tests when available

---

## Next Steps (Session 27)

1. **Create SYNTAX_REFERENCE.md** with canonical examples
2. **Fix advanced_features/error_handling.md** completely
3. **Update README.md** with Session 26 status
4. **Fix stdlib/filter.md** lambda examples
5. **Document patterns found** for batch application

**Estimated Duration**: 4-6 hours (1 session)

---

## Notes

- **Type guides are DONE** - Don't touch types/ directory (already correct)
- **Backup before scripting** - Use git to track all changes
- **Test as you go** - Fix 10 files, verify pattern, then batch
- **Document discoveries** - Update this audit as new issues found
- **Focus on user impact** - Fix high-visibility files first to prevent syntax propagation

---

**Philosophy**: We're not just fixing syntax - we're establishing Aria's canonical style that will be copied by thousands of developers. Get it right once, benefit forever.

**Motivation**: Clean documentation → correct fuzzer → accurate man pages → professional website → successful language adoption → changed lives.

**Timeline**: No rush. Quality over speed. Building for decades, not quarters.
