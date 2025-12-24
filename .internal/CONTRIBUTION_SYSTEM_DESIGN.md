# Contribution System Design - Complete Plan

**Date**: December 24, 2024  
**Purpose**: Enable quality contributions while preventing chaos and maintaining spec compliance

---

## 1. REPOSITORY CONSOLIDATION

### Decision: Keep Both Repos
- **aria_docs** - User-facing (tutorials, guides, API reference)
- **aria_ecosystem** - Architecture/internals (specs, design docs, component details)

**Reasoning**: Different audiences, different purposes. Merging would create confusion.

---

## 2. REPOSITORIES REQUIRING CONTRIBUTION SYSTEMS

### Priority 1 (Immediate)
1. **aria** - Compiler (highest complexity, most contributors)
2. **aria_make** - Build system
3. **aria_shell** - Shell
4. **aria_utils** - Utilities
5. **ariax** - Package manager

### Priority 2 (Later)
- aria_docs (documentation contributions are lower risk)
- nikola_src (not ready for external contributors yet - depends on full ecosystem)

---

## 3. CORE PROBLEM STATEMENT

**Must Prevent**:
- Duplicate work (two people implementing same feature)
- Spec violations (simplification, bloat, unauthorized changes)
- Merge hell (conflicts, paperwork overhead)
- Low-quality contributions (won't follow basic git hygiene)
- "Planned feature" syndrome (Tsoding test - everything user-facing must WORK)

**Must Enable**:
- Self-service for competent contributors
- Clear task ownership
- Spec compliance verification
- Quality filtering (incompetent people filtered out automatically)
- Progress visibility

---

## 4. PROPOSED SYSTEM ARCHITECTURE

### Layer 1: Repository Files (Self-Documenting)

Each repo gets these files in root:

#### STATUS.md
```markdown
# Project Status

## What Works ✅
[List of fully working features with test coverage]

## In Progress 🔄
[Features currently being implemented - CLAIMED by who]

## Planned 📋
[Features from specs not yet started]

## Known Issues 🐛
[Bugs that need fixing - link to task in TASKS.md]
```

**Purpose**: Instant readiness check. Tsoding opens repo, reads this, knows what's real vs planned.

---

#### ARCHITECTURE.md
```markdown
# Architecture Overview

## Philosophy
[Why this component exists, design principles]

## Components
[High-level breakdown of major parts]

## Dependencies
[What this needs from other Aria components]

## Build System
[How to build, test, run]

## Code Organization
[Directory structure, what lives where]
```

**Purpose**: Contributors understand the WHY before touching the WHAT.

---

#### CONTRIBUTING.md
```markdown
# Contributing to [Project]

## Philosophy & Standards

### Non-Negotiables
1. Spec compliance - changes must reference specification
2. No simplification - difficulty is not a reason to deviate
3. No bloat - every line must serve a purpose
4. Test coverage - features without tests don't merge
5. Git hygiene - pull before push, no merge commits from not pulling

### Code Quality
- Follow existing patterns
- No cargo-cult code
- Performance matters
- Comments explain WHY, not WHAT

## How to Contribute

### 1. Find Work
Check TASKS.md for available work. Each task has:
- Clear spec reference
- Acceptance criteria
- Complexity estimate

### 2. Claim Work
Comment on task or update TASKS.md showing:
- Your GitHub username
- Expected completion timeframe
- Any questions about spec

### 3. Branch Strategy
```bash
git checkout -b feature/task-name
# Work in your branch
# Test thoroughly
# When ready:
git pull origin main --rebase
git push origin feature/task-name
# Open PR with task reference
```

### 4. Pull Request Requirements
- [ ] References task from TASKS.md
- [ ] References specification document
- [ ] Includes tests
- [ ] All tests pass
- [ ] No merge conflicts
- [ ] Follows code style
- [ ] Updates STATUS.md if needed

### 5. Review Process
Maintainer will review for:
- Spec compliance
- Code quality
- Test coverage
- Architecture fit

Expect honest feedback. If it doesn't meet standards, you'll be asked to fix it.

## What Maintainers Will NOT Do
- Fix your merge conflicts from not pulling
- Debug your environment issues
- Write tests for your code
- Simplify specs to make your job easier

## What Maintainers WILL Do
- Answer questions about specs
- Clarify architecture decisions
- Review code thoroughly
- Merge quality work quickly
```

**Purpose**: Sets expectations. Serious people appreciate clarity. Low-effort people self-filter.

---

#### TASKS.md
```markdown
# Available Tasks

## Format
Each task follows this structure:

### [TASK-###] Feature Name
**Status**: Available | Claimed | In Progress | Review | Complete  
**Claimed By**: @username (if claimed)  
**Spec Reference**: [link to spec doc]  
**Complexity**: Easy | Medium | Hard | Expert  
**Dependencies**: [other tasks that must complete first]  

**Description**:
[What needs to be built]

**Acceptance Criteria**:
- [ ] Criterion 1
- [ ] Criterion 2
- [ ] Tests pass

**Files Likely Affected**:
- path/to/file1.cpp
- path/to/file2.h

---

## Available Tasks

[List of tasks in dependency order]

## Claimed Tasks

[Tasks currently being worked on - shows WHO is doing WHAT]

## Completed Tasks

[Archive of finished work - shows progress]
```

**Purpose**: Task ownership. No duplicate work. Clear specs. Dependency management.

---

### Layer 2: Task Tracking (GitHub Projects)

**Why Not Just Issues?**
- Issues encourage chaos (anyone opens anything)
- No dependency visualization
- Hard to see "what's claimed"
- Lots of noise

**GitHub Projects Board**:
- Columns: Available | Claimed | In Progress | Review | Done
- Each card links to task in TASKS.md
- Shows at-a-glance: who's working on what
- Can filter by complexity, component, etc.

**Process**:
1. Contributor finds task in TASKS.md
2. Comments they're claiming it
3. Maintainer moves card to "Claimed" column, assigns GitHub user
4. Contributor works, moves to "In Progress"
5. PR opened, moves to "Review"
6. Merged, moves to "Done"

**Benefit**: Visual, no paperwork overhead, prevents duplicate work.

---

### Layer 3: Quality Gates (PR Template)

`.github/pull_request_template.md`:
```markdown
## Task Reference
Closes task: [TASK-### from TASKS.md]

## Specification Reference
Implements: [link to spec document, section number]

## Changes Made
[Brief description]

## Testing
- [ ] Unit tests added
- [ ] Integration tests pass
- [ ] Manual testing completed

## Checklist
- [ ] Code follows project style
- [ ] No spec deviations
- [ ] Documentation updated if needed
- [ ] STATUS.md updated if feature complete
- [ ] No merge conflicts
- [ ] All tests pass

## Additional Notes
[Anything reviewer should know]
```

**Purpose**: Force contributor to think through requirements. Self-filtering quality gate.

---

### Layer 4: Contributor Tiers (Trust Levels)

Not everyone gets access to everything immediately.

**Tier 1: First-Time Contributors**
Can work on:
- Bug fixes (small, clear scope)
- Test additions (low risk)
- Documentation improvements

**Cannot work on**:
- Core architecture changes
- New language features
- Breaking changes

**Tier 2: Proven Contributors**
Unlocked after 3-5 quality PRs merged.

Can work on:
- New features from TASKS.md
- Refactoring with approval
- Performance improvements

**Tier 3: Core Team**
After sustained quality contributions.

Can work on:
- Architecture decisions
- Design discussions
- Spec clarifications
- Mentoring other contributors

**How Enforced**:
- TASKS.md tags tasks: `[Tier 1]`, `[Tier 2]`, `[Tier 3]`
- Maintainer simply says "This task requires Tier 2" if someone premature claims it
- No complex permission system needed

---

## 5. IMPLEMENTATION PLAN

### Phase 1: Create Core Files
For each repo (aria, aria_make, aria_shell, aria_utils, ariax):

1. **STATUS.md** - Document current state
2. **ARCHITECTURE.md** - Explain structure
3. **CONTRIBUTING.md** - Set expectations
4. **TASKS.md** - List available work
5. **.github/pull_request_template.md** - Quality gate

### Phase 2: Populate Tasks
- Review MASTER_ROADMAP.md
- Break down features into tasks
- Add spec references
- Estimate complexity
- Note dependencies

### Phase 3: Set Up GitHub Projects
- Create project board per repo
- Add task cards
- Link to TASKS.md

### Phase 4: Test with First Contributor
- Wait for someone (Indian CS student?) to express interest
- Direct them to CONTRIBUTING.md
- See if system works
- Iterate based on feedback

---

## 6. MAINTENANCE OVERHEAD

**What Randy Must Do**:
- Review PRs (unavoidable, but clear standards reduce back-and-forth)
- Move GitHub Project cards (30 seconds per task)
- Answer spec questions (but ARCHITECTURE.md reduces this)
- Update STATUS.md when features complete (30 seconds)

**What Randy Does NOT Do**:
- Fix contributor merge conflicts
- Debug contributor environments
- Chase people for updates
- Manage "assigned" tasks that go stale (after 2 weeks, unclaim it)

**Time Estimate**: ~30min/week for 5 active contributors

---

## 7. SUCCESS METRICS

### For Tsoding Test
Open aria repo, read STATUS.md:
- See what actually works
- See what's in progress
- No surprise "planned features"
- Can write a program with working features immediately

### For Contributor Experience
- Find task in 2 minutes
- Understand what to build from spec reference
- Know acceptance criteria
- Submit PR with confidence
- Get merged or clear feedback within 48 hours

### For Randy's Sanity
- No merge conflict hell
- No duplicate work
- No spec violations making it to PR
- Contributors self-filter (low quality don't make it past CONTRIBUTING.md)
- Can focus on review, not paperwork

---

## 8. RISK MITIGATION

### Risk: No One Contributes
**Mitigation**: System works even with 0 external contributors. Randy uses TASKS.md for himself. No wasted effort.

### Risk: Low Quality Flood
**Mitigation**: CONTRIBUTING.md filters. Tier system limits damage. PR template catches issues early.

### Risk: Contributor Disagreement with Spec
**Mitigation**: "Spec is spec. If you think it should change, open discussion in aria_ecosystem repo. PRs implement specs, not change them."

### Risk: Abandoned Tasks
**Mitigation**: After 2 weeks of no updates, maintainer unclaims task. No guilt, just practicality.

### Risk: Merge Conflicts Anyway
**Mitigation**: TASKS.md shows who's working on what files. Contributors can coordinate. If they don't, that's on them.

---

## 9. NEXT STEPS

1. **Review this plan** - Does it meet your requirements?
2. **Refine if needed** - Any concerns or missing pieces?
3. **Generate actual files** - Create templates for each repo
4. **Populate initial tasks** - Convert roadmap items to task format
5. **Test with aria repo first** - Get one working before rolling out to others
6. **Document in README** - Add "Contributing" section pointing to CONTRIBUTING.md

---

## 10. SAMPLE TASK (for reference)

```markdown
### [ARIA-042] Implement ERR Keyword Recognition
**Status**: Available  
**Spec Reference**: aria_ecosystem/specs/ERROR_HANDLING.md, Section 3.2  
**Complexity**: Medium  
**Tier**: 2  
**Dependencies**: None  

**Description**:
Add ERR as a recognized keyword/identifier in the lexer and parser so it can be used in pick statement patterns to handle TBB error sentinels.

**Context**:
Currently, ERR is not recognized by the lexer. When users write:
```aria
pick(result) {
    (ERR) { handle_error(); }
}
```
The parser fails with "Expected expression, found token ERR".

**Acceptance Criteria**:
- [ ] ERR recognized as keyword in lexer (src/frontend/lexer/token.cpp)
- [ ] ERR parsed as valid pattern in pick cases
- [ ] test_exhaustiveness_tbb8_complete.aria compiles successfully
- [ ] ERR cannot be used as variable name (reserved keyword)
- [ ] All existing tests still pass

**Files Likely Affected**:
- src/frontend/lexer/token.cpp (add ERR to keyword map)
- src/frontend/lexer/lexer.cpp (tokenize ERR)
- src/frontend/parser/parser.cpp (handle ERR in pick pattern parsing)
- tests/safety/test_err_keyword.aria (new test)

**Estimated Time**: 2-4 hours
```

---

**Does this design meet your requirements?**
- Minimal paperwork overhead
- Self-filtering for quality
- Prevents chaos and duplicate work
- Makes project look serious (Tsoding test)
- Scales from 0 to many contributors
- Clear about what works vs planned
