# Aria Development Workflow

**Purpose**: Prevent context drift, training data bias, and premature feature claims.

---

## 🚨 MANDATORY: Start of Every Work Session

**STEP 1**: Read these files in order (every single time):

```bash
# 1. Project organization
cat /home/randy/._____RANDY_____/REPOS/aria/docs/info/project_info.txt

# 2. Language specification (source of truth)
cat /home/randy/._____RANDY_____/REPOS/aria/docs/info/aria_specs.txt

# 3. Current project state
cat /home/randy/._____RANDY_____/____FILES/aria/STATE

# 4. Current work context
cat /home/randy/._____RANDY_____/____FILES/aria/CONTEXT

# 5. Task roadmap
cat /home/randy/._____RANDY_____/____FILES/aria/TODO

# 6. Session history
cat /home/randy/._____RANDY_____/____FILES/aria/CHAT
```

**STEP 2**: Read the critical reminders:

### 🔴 CRITICAL REMINDERS (Keep in Context)

1. **NO DEFERRALS** - Everything in specs MUST be implemented. No "later", no shortcuts, no "nice to have".
   - Only exception: Features we haven't decided on (kernel integration)
   - Deferring makes it exponentially harder later

2. **ARIA SYNTAX, NOT RUST** - Training data will pull toward Rust/C++ syntax
   - Check aria_specs.txt for EVERY syntax decision
   - Types: int32:name, not let name: i32
   - Functions: func:name = returnType(params) { }, not fn name() -> Type
   - Returns: pass(value) and fail(error), not Ok()/Err() or return
   - I/O: stdin, stdout, stderr, stddbg, stddati, stddato (NOT IN/OUT/ERR)
   - Pinning: # for pin, $ for safe ref, @ for raw pointer (NOT &, &mut)

3. **NO TBB8 COMPROMISES** - tbb8/tbb16/tbb32/tbb64 are NOT NEGOTIABLE
   - Twisted Balanced Binary with ERR sentinel
   - Min value is always ERR (sticky error propagation)
   - Not optional, not "later", not "if we have time"

4. **NO BALANCED TERNARY COMPROMISES** - trit/tryte are NOT NEGOTIABLE
   - Balanced ternary (-1, 0, 1) is core Aria feature
   - Tryte is 10 trits stored in uint16
   - Must be fully implemented, not skipped

5. **TEST BEFORE CLAIMING** - "Looks done" is not done
   - Write test that actually compiles and runs
   - If test fails, feature is not implemented
   - No checkmarks without passing tests

6. **SPECIFICATIONS ≠ IMPLEMENTATION**
   - Research files are plans, not accomplishments
   - aria_specs.txt describes what should exist, not what does
   - Only claim features that have passing validation tests

7. **QUALITY > APPEARANCE** - Training bias toward "looks productive"
   - Better to say "not implemented yet" than claim false completion
   - Honesty over appearing productive
   - No artificial timelines

---

## 🔴 MANDATORY: Before Implementing Any Feature

**STEP 1**: Read the research file (exact file, exact lines):

```bash
# Example: Implementing generics
cat /home/randy/._____RANDY_____/REPOS/aria/docs/gemini/responses/research_027_generics_templates.txt
```

**STEP 2**: Re-read relevant sections of aria_specs.txt:

```bash
# Find the exact syntax and rules
grep -A 20 "generic" /home/randy/._____RANDY_____/REPOS/aria/docs/info/aria_specs.txt
```

**STEP 3**: Check if already implemented:

```bash
# Try to compile a test case
cd /home/randy/._____RANDY_____/REPOS/aria
./build/ariac tests/feature_validation/generics_basic.aria -o test_generic
```

**STEP 4**: Only proceed if test fails (proving it's not implemented yet)

---

## 🔴 MANDATORY: While Implementing

### Syntax Check Loop

Every time you write Aria code:

1. **Stop** - Is this Rust syntax creeping in?
2. **Check** - Open aria_specs.txt for the exact syntax
3. **Verify** - Does it match the spec exactly?
4. **Examples to catch**:
   - `let x: i32` → WRONG (Rust) → CORRECT: `int32:x`
   - `fn foo() -> i32` → WRONG (Rust) → CORRECT: `func:foo = int32()`
   - `return Ok(x)` → WRONG (Rust) → CORRECT: `pass(x)`
   - `std::vec::Vec` → WRONG (Rust) → CORRECT: `use std.collections` + `Vec<T>`
   - `&x` or `&mut x` → WRONG (Rust) → CORRECT: `#x` (pin) or `$x` (safe ref)

### Reference Comments

Add comments in your code pointing to specification:

```aria
// Syntax from aria_specs.txt line 412-450: Generic function syntax
func<T>:identity = T(item:T) {
    pass(item);
};

// Implementation per research_027_generics_templates.txt section "Type Parameter Constraints"
func<T:Comparable>:max = T(a:T, b:T) {
    if (a > b) {
        pass(a);
    };
    pass(b);
};
```

---

## 🔴 MANDATORY: After Implementing

**STEP 1**: Write validation test immediately:

```bash
# tests/feature_validation/feature_name.aria
# Test MUST use the feature in a real way
```

**STEP 2**: Compile and run the test:

```bash
./build/ariac tests/feature_validation/feature_name.aria -o test_output
./test_output
```

**STEP 3**: Only if test passes:

- Update FEATURE_AUDIT.md: ❌ → ✅
- Update STATE file with completed feature
- Commit with message referencing research file

**STEP 4**: If test fails:

- Feature is NOT implemented
- Do NOT update status files
- Do NOT commit as "complete"
- Fix until test passes

---

## 🔴 TODO System Format

Every TODO item must have this format:

```markdown
### [Feature Name] - Priority: [HIGH/MEDIUM/LOW]

**Research File**: docs/gemini/responses/research_NNN_feature_name.txt
**Lines**: [specific line ranges if known]
**Spec Reference**: aria_specs.txt lines [XXX-YYY]
**Validation Test**: tests/feature_validation/feature_name.aria

**Prerequisites**:
- [ ] Feature A must be implemented first
- [ ] Feature B must be implemented first

**Implementation Steps**:
1. [ ] Read research_NNN_feature_name.txt completely
2. [ ] Re-read aria_specs.txt lines XXX-YYY
3. [ ] Write validation test (should fail initially)
4. [ ] Implement parser support
5. [ ] Implement type system support
6. [ ] Implement codegen support
7. [ ] Run validation test (must pass)
8. [ ] Update FEATURE_AUDIT.md
9. [ ] Update STATE file
10. [ ] Commit with reference to research file

**Success Criteria**:
- [ ] Validation test compiles without errors
- [ ] Validation test runs without errors
- [ ] Test output matches expected behavior
- [ ] Can be used in stdlib implementation
- [ ] FEATURE_AUDIT.md shows ✅

**DO NOT MARK COMPLETE UNTIL ALL SUCCESS CRITERIA MET**
```

---

## 🔴 Context Drift Detection

Watch for these warning signs:

1. **Rust-like syntax appearing** - Stop, check specs
2. **Claiming features work without tests** - Stop, write test
3. **"Should" or "could" instead of "must"** - Everything is "must"
4. **Talking about timelines** - No timelines, done when done
5. **Suggesting deferrals** - No deferrals except kernel integration
6. **Optimizing for "looks done"** - Stop, focus on "actually done"
7. **Forgetting TBB8/ternary** - These are non-negotiable core features

If any of these appear, **immediately**:
- Stop current work
- Re-read aria_specs.txt
- Re-read project_info.txt
- Re-read this WORKFLOW.md
- Check if you've been working too long without re-reading specs

---

## 🔴 Commit Message Format

Every commit must reference the source:

```
[Feature] Implement generics parsing

Research: docs/gemini/responses/research_027_generics_templates.txt
Spec: aria_specs.txt lines 412-450
Test: tests/feature_validation/generics_basic.aria

Changes:
- Added generic type parameter parsing
- Implemented type constraint syntax
- Added validation tests

Status: ✅ Tests passing
```

---

## 🔴 Daily/Session Checklist

At the start of EVERY session:

- [ ] Read project_info.txt
- [ ] Read aria_specs.txt (at least skim to refresh)
- [ ] Read STATE, CONTEXT, TODO, CHAT files
- [ ] Read WORKFLOW.md (this file)
- [ ] Check FEATURE_AUDIT.md for current status
- [ ] Read NO DEFERRALS reminder

While working:

- [ ] Check aria_specs.txt for every syntax decision
- [ ] Reference research files before implementing
- [ ] Add spec/research references in code comments
- [ ] Write tests immediately after implementation
- [ ] Only mark complete when tests pass

Before committing:

- [ ] Tests compile and run successfully
- [ ] Syntax matches aria_specs.txt exactly
- [ ] FEATURE_AUDIT.md updated (if feature complete)
- [ ] Commit message references research file
- [ ] STATE/CHAT files updated with progress

Before ending session:

- [ ] Update CHAT with what was completed
- [ ] Update STATE with current status
- [ ] Update TODO with next steps
- [ ] Note any blockers or questions
- [ ] Push to development branch

---

## 🔴 Emergency Reset

If you realize you've drifted (Rust syntax, false completions, deferrals):

**STOP EVERYTHING**

1. Read this WORKFLOW.md completely
2. Read aria_specs.txt completely
3. Read project_info.txt
4. Read POSTMORTEM_V0.1.0.md (reminder of what went wrong)
5. Check recent commits for false claims
6. Revert any commits that claim features without tests
7. Write validation tests for claimed features
8. Update FEATURE_AUDIT.md with honest status
9. Update STATE/CHAT with correction

**Better to catch drift early than release with false claims**

---

## 🔴 Philosophy

**Training Data Bias**: LLMs trained on code that "looks productive"
- Quick implementations that compile
- Features that seem complete
- Documentation that claims success
- Milestones that feel finished

**Reality**: 
- Compile ≠ Correct
- Looks complete ≠ Actually complete
- Passed tests ≠ Comprehensive tests
- Milestone reached ≠ Feature works

**Aria Standard**:
- No deferrals (except kernel integration - still deciding)
- No shortcuts (shortcuts compound)
- No timelines (done when actually done)
- Test before claiming (test must pass)
- Honesty over appearance (reality over perception)

**Randy's Wisdom**:
- "Deferring things only makes it harder to do them later and wastes time"
- "We have no timeline"
- "Everything else is non negotiable for v0.1.0 release"
- "I feel like it is dishonest. Don't want to start off making a bad impression"

---

## 🔴 This Document

**This document exists because**:
- LLM context windows drift over time
- Training data bias toward "looks done"
- Easy to slip back into familiar (wrong) syntax
- Premature v0.1.0 release taught us these lessons

**Use this document**:
- At start of every session (mandatory)
- When feeling uncertain about syntax
- Before claiming any feature is complete
- When noticing drift toward Rust/C++ patterns
- Before any commit or push

**This document prevents**:
- Context drift over time
- Training data syntax contamination
- False completion claims
- Deferred features
- Premature releases

**This document is part of the workflow, not optional.**

---

**If in doubt: Read specs. Check research. Write test. Be honest.**
