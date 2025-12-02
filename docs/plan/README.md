# Aria v0.0.6 Implementation Plan

**Status:** Active Development  
**Started:** December 2, 2025  
**Current Phase:** Phase 1 - Foundation  
**Current Task:** Task 1 - Complete Lexer

---

## Quick Navigation

- **[IMPLEMENTATION_STATUS.md](./IMPLEMENTATION_STATUS.md)** - High-level overview of what's complete vs planned
- **[IMPLEMENTATION_DETAILS.md](./IMPLEMENTATION_DETAILS.md)** - Deep technical analysis with interfaces and dependencies
- **[PROGRESS.md](./PROGRESS.md)** - Daily progress tracking log
- **[phases/](./phases/)** - Detailed work packages for each phase
- **[tasks/](./tasks/)** - Individual task breakdowns with checklists

---

## Development Philosophy

> **"One task at a time. Test completely before moving on."**

- ✅ No rushing to demos
- ✅ Fix problems now, not later
- ✅ Document everything
- ✅ Test thoroughly
- ✅ No scope creep

---

## Current Status

**Phase 1: Foundation** (5 weeks estimated)
- [x] Task 1: Complete Lexer (3 hours actual!) ← **✅ COMPLETE**
- [ ] Task 2: Build Preprocessor (2 weeks) ← **YOU ARE HERE**
- [ ] Task 3: Build Comptime Evaluator (2 weeks)

---

## File Structure

```
docs/plan/
├── README.md                        (This file - navigation hub)
├── IMPLEMENTATION_STATUS.md         (High-level status overview)
├── IMPLEMENTATION_DETAILS.md        (Deep technical analysis)
├── PROGRESS.md                      (Daily progress log)
│
├── phases/
│   ├── phase1_foundation.md         (Tasks 1-3: Lexer, Preprocessor, Comptime)
│   ├── phase2_parser.md             (Tasks 4-6: Function, Operators, Declarations)
│   ├── phase3_semantic.md           (Tasks 7-8: Type Checker, Borrow Checker)
│   ├── phase4_codegen.md            (Tasks 9-11: Control Flow, Memory, Exotic Types)
│   ├── phase5_runtime.md            (Tasks 12-15: GC, I/O, Process, Stdlib)
│   └── phase6_advanced.md           (Tasks 16-17: Async, Build System)
│
└── tasks/
    ├── task_01_complete_lexer.md
    ├── task_02_build_preprocessor.md
    ├── task_03_build_comptime.md
    ├── task_04_function_parsing.md
    ├── task_05_operators.md
    ├── task_06_declarations.md
    ├── task_07_type_checker.md
    ├── task_08_borrow_checker.md
    ├── task_09_basic_codegen.md
    ├── task_10_memory_codegen.md
    ├── task_11_exotic_codegen.md
    ├── task_12_complete_gc.md
    ├── task_13_io_streams.md
    ├── task_14_process_mgmt.md
    ├── task_15_stdlib.md
    ├── task_16_async_await.md
    └── task_17_build_tooling.md
```

---

## How to Use This System

### Starting a New Task:
1. Read `tasks/task_XX_name.md` for detailed requirements
2. Review dependencies in `IMPLEMENTATION_DETAILS.md`
3. Create tests FIRST (TDD approach)
4. Implement feature
5. Run all tests
6. Update `PROGRESS.md` with results
7. Mark task complete in this README
8. Move to next task

### Daily Workflow:
1. Check current task status
2. Work on implementation
3. Run tests frequently
4. Update PROGRESS.md at end of day
5. Commit with meaningful message

### When Stuck:
1. Review interface definitions in `IMPLEMENTATION_DETAILS.md`
2. Check spec examples in `../research/Aria_v0.0.6_Specs.txt`
3. Look at existing working code for patterns
4. Don't guess - search for answers

---

## Timeline

**Total Estimated: 30 weeks (7.5 months)**

- Phase 1: 5 weeks (Tasks 1-3)
- Phase 2: 3 weeks (Tasks 4-6)
- Phase 3: 3 weeks (Tasks 7-8)
- Phase 4: 6 weeks (Tasks 9-11)
- Phase 5: 6 weeks (Tasks 12-15)
- Phase 6: 3 weeks (Tasks 16-17)
- Buffer: +4 weeks

**Target Completion:** ~July 2026

---

## Progress Tracking

See [PROGRESS.md](./PROGRESS.md) for detailed daily logs.

**Completion Summary:**
- ✅ Planning & Analysis: 100%
- ⏳ Phase 1: 0%
- ⏳ Phase 2: 0%
- ⏳ Phase 3: 0%
- ⏳ Phase 4: 0%
- ⏳ Phase 5: 0%
- ⏳ Phase 6: 0%

---

**Let's build this right! 🚀**
