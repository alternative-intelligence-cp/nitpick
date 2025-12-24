# Contributing to Aria

Thank you for your interest in contributing to Aria. This document sets clear expectations for all contributors.

---

## Philosophy & Standards

### Core Principles

Aria is built on these **non-negotiable** principles:

1. **Errors Are Values** - No exceptions. Result<T,E> everywhere.
2. **Explicit Over Implicit** - No magic, no hidden behavior.
3. **Specification Compliance** - The specs in `aria_ecosystem/specs/` are law.
4. **Performance Matters** - Zero-cost abstractions, no unnecessary overhead.
5. **Memory Safety** - Compile-time borrow checking, no runtime cost.

**If you disagree with these principles, Aria is not the right project for you.** Fork it and build your vision.

### Non-Negotiables

These are **rules**, not suggestions:

- ✅ **Spec Compliance** - Every change must reference a specification document
- ✅ **No Simplification** - "Too hard" is not a reason to deviate from spec
- ✅ **No Bloat** - No features without clear purpose and spec approval
- ✅ **Test Coverage** - Features without tests don't get merged
- ✅ **Git Hygiene** - Pull before push. No merge commits from laziness.

### What We Will NOT Accept

- ❌ "I made it simpler by removing error handling"
- ❌ "I added this cool feature I thought of"
- ❌ "The spec says X but Y is easier so I did Y"
- ❌ "I couldn't get the tests to work so I commented them out"
- ❌ Cargo-cult code copied without understanding
- ❌ Dependencies added "just in case"
- ❌ Breaking changes without discussion

### Code Quality Standards

- **Follow existing patterns** - Don't reinvent the wheel in every file
- **Comments explain WHY, not WHAT** - Code shows what, comments show why
- **Performance matters** - Measure before optimizing, but don't be wasteful
- **Error handling is mandatory** - No unchecked errors, ever
- **No warnings** - Compiler warnings must be fixed, not ignored

---

## How to Contribute

### Step 1: Find Work

Check [TASKS.md](TASKS.md) for available work. Each task has:
- Clear specification reference
- Acceptance criteria
- Complexity estimate
- Tier requirement

**Tier Levels**:
- **Tier 1** - First-time contributors (bug fixes, tests, docs)
- **Tier 2** - Proven contributors (new features from specs)
- **Tier 3** - Core team (architecture decisions)

Start with Tier 1 tasks to build trust.

### Step 2: Claim Work

Before starting:
1. Comment on the task in TASKS.md or open a GitHub Discussion
2. State your approach and timeline
3. Ask questions about the spec if unclear
4. Wait for maintainer acknowledgment

**Why?** Prevents duplicate work and ensures you understand requirements.

### Step 3: Branch Strategy

```bash
# Always start from latest main
git checkout main
git pull origin main

# Create feature branch
git checkout -b feature/task-name

# Work in your branch
# Commit often with clear messages

# Before submitting PR
git fetch origin
git rebase origin/main  # NOT merge!
git push origin feature/task-name
```

**No merge commits.** Use rebase to keep history clean.

### Step 4: Development Process

**Testing**:
```bash
# Build compiler
cmake --build build --target ariac

# Run specific test
./build/ariac tests/your_test.aria

# Run all tests
cmake --build build --target test
```

**Debugging**:
```bash
# Debug build
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Verbose output
./build/ariac tests/your_test.aria --verbose

# GDB
gdb --args ./build/ariac tests/your_test.aria
```

### Step 5: Pull Request Requirements

Your PR must include:

- [ ] Reference to task from TASKS.md
- [ ] Reference to specification document (file and section)
- [ ] Description of what changed and why
- [ ] Tests that verify the change works
- [ ] All existing tests still pass
- [ ] No compiler warnings
- [ ] No merge conflicts
- [ ] Updated STATUS.md if feature is complete
- [ ] Code follows project style

Use the PR template (created automatically).

### Step 6: Review Process

**What to Expect**:
- Honest, direct feedback
- Requests for changes if spec compliance is violated
- Questions about implementation choices
- Possible rejection if approach is fundamentally wrong

**What NOT to Expect**:
- Sugarcoating
- Merging code "just because you worked hard"
- Maintainers fixing your merge conflicts
- Lowering standards to accept subpar work

**Timeline**:
- Initial review: Within 48 hours
- Follow-up reviews: Within 24 hours
- Merge: When all requirements met

---

## What Maintainers Will NOT Do

We respect your time, but we won't do your job:

- ❌ Fix merge conflicts from not rebasing
- ❌ Debug your environment setup
- ❌ Write tests for your code
- ❌ Simplify specs to make implementation easier
- ❌ Accept partial implementations
- ❌ Merge broken code with "will fix later"

**These are your responsibilities.**

---

## What Maintainers WILL Do

We're here to help you succeed:

- ✅ Answer questions about specifications
- ✅ Clarify architecture decisions
- ✅ Review code thoroughly and fairly
- ✅ Provide constructive feedback
- ✅ Merge quality work quickly
- ✅ Credit you for contributions
- ✅ Help you grow as a contributor

---

## Communication Guidelines

### Where to Discuss

- **Specification Questions** - GitHub Discussions in aria_ecosystem repo
- **Implementation Help** - GitHub Discussions in aria repo
- **Bug Reports** - GitHub Issues (use template)
- **Task Claiming** - Comment in TASKS.md or Discussion

### How to Ask Questions

**Good Question**:
> "In TBB_TYPES.md section 3.2, it says ERR is -128 for tbb8. Should the exhaustiveness checker treat this as a special case or just another value in the range?"

**Bad Question**:
> "How do I make this work?"

**Good questions show you've**:
1. Read the spec
2. Attempted to solve it yourself
3. Identified specific unclear point

### Response Time Expectations

- Spec clarifications: 24-48 hours
- PR reviews: 24-48 hours for initial review
- Questions about claimed tasks: 12-24 hours

---

## Task Abandonment

**If you claim a task but can't complete it:**
- Comment on the task explaining why
- No judgment, life happens
- Task will be made available again

**If you go silent for 2 weeks:**
- Task will be automatically unclaimed
- You can reclaim it later if still interested

**We prefer honest communication over ghost contributors.**

---

## Building Trust (Tier Progression)

### Tier 1 → Tier 2

Requirements:
- 3-5 successful PR merges
- All PRs met standards without major revisions
- Demonstrated understanding of specs
- Good communication

Unlocks:
- New feature implementation
- Refactoring tasks
- More complex work

### Tier 2 → Tier 3

Requirements:
- 15-20 successful PR merges over time
- Consistent high-quality contributions
- Deep understanding of architecture
- Helpful to other contributors

Unlocks:
- Architecture discussions
- Spec clarifications
- Design decisions
- Mentoring role

---

## Code Style

**Follow existing patterns.** We don't bikeshed formatting.

**General Guidelines**:
- Indent with 4 spaces (no tabs)
- Braces on same line for control structures
- Descriptive variable names (no `tmp`, `x`, `foo`)
- Max line length: 100 characters (guidance, not law)
- Include guards: `ARIA_COMPONENT_FILE_H`

**When in doubt, match surrounding code.**

---

## Testing Requirements

### Unit Tests (C++)
For compiler components (lexer, parser, etc.):
- Test normal cases
- Test edge cases
- Test error conditions
- Use existing test framework

### Integration Tests (Aria)
For language features:
- Write actual Aria programs
- Verify they compile (or fail correctly)
- Check runtime behavior
- Place in `tests/` directory

### Safety Tests
For safety features (borrow checking, exhaustiveness, etc.):
- Test that errors are caught
- Verify error messages are helpful
- Ensure no false positives

**Minimum**: Every PR must have at least one test.

---

## Commit Message Guidelines

**Good commit messages**:
```
feat: Add ERR keyword to lexer

Implements task ARIA-042 from TASKS.md.
References: specs/ERROR_HANDLING.md section 3.2

- Added ERR to keyword table in token.cpp
- Updated lexer to recognize ERR token
- Added tests for ERR in various contexts
```

**Format**:
```
<type>: <short description>

<detailed description>
<spec reference>
<changes made>
```

**Types**: feat, fix, refactor, test, docs, perf

---

## Licensing

By contributing, you agree:
- Your code is licensed under Aria's license (see LICENSE)
- You have the right to contribute this code
- No copyrighted code from other projects

**No exceptions.**

---

## Getting Help

**Before asking for help:**
1. Read relevant specification documents
2. Check ARCHITECTURE.md
3. Search existing GitHub Discussions
4. Look at similar code in the project

**When asking for help:**
1. Show what you've tried
2. Reference specific spec sections
3. Provide error messages
4. Share minimal reproduction case

**We help those who help themselves.**

---

## Final Words

Aria is a **serious** project with **high standards**.

We want **serious contributors** who:
- Care about correctness
- Read specifications
- Write tests
- Communicate clearly
- Take feedback well
- Deliver quality work

We don't want:
- Drive-by PRs with no context
- Spec violations "to make life easier"
- Contributors who argue against core principles
- Low-effort work that creates more maintenance burden

**If this sounds too strict, Aria isn't for you. That's okay.**

**If this sounds like exactly what you want, welcome aboard.**

---

## Quick Links

- [STATUS.md](STATUS.md) - What works, what's planned
- [ARCHITECTURE.md](ARCHITECTURE.md) - How Aria works
- [TASKS.md](TASKS.md) - Available work
- [aria_ecosystem/specs/](../aria_ecosystem/specs/) - Specifications
- [MASTER_ROADMAP.md](aria_ecosystem/MASTER_ROADMAP.md) - Long-term plan

---

*These guidelines are firm but fair. They exist to protect project quality and maintainer sanity.*
