# QUICK REFERENCE: Research Task System

## Task Status

Run `python utils/index_tasks.py` for complete status.

**Quick Stats:**
- Total Tasks: 10
- CRITICAL: 3 (borrow checker, ternary, nonary)
- HIGH: 4 (file I/O, processes, streams, threading)
- MEDIUM: 3 (atomics, timers, comptime)

## Workflow

1. **Pick a Task:** Choose from `tasks/research_*.txt`
2. **Send to Gemini:** Copy task content + attach context files
3. **Receive Response:** Save to `responses/research_*_response.md`
4. **Integrate:** Review and incorporate findings into implementation

## Task Numbering

- `001-003`: CRITICAL - Core type system and safety
- `004-007`: HIGH - Essential standard library
- `008-010`: MEDIUM - Advanced features

## Time Estimates

All tasks designed for 20-45 minutes:
- ≤3 deliverables per task
- Focused scope
- Concrete output

## Context Files

Every task includes:
- `/home/randy/._____RANDY_____/REPOS/aria/docs/info/aria_specs.txt`
- Additional research docs as needed (see `research_context/index.md`)

## Creating New Tasks

```bash
python utils/create_task.py <number> "Task Title" <PRIORITY>
```

Example:
```bash
python utils/create_task.py 11 "WebAssembly Backend" HIGH
```

## Research Gaps vs Ready to Implement

**Need Research (10 tasks):**
- Borrow checker, balanced ternary/nonary
- File I/O, processes, modern streams
- Threading, atomics, timers, comptime

**Ready to Implement (16 features, 6-12 months work):**
- TBB arithmetic, generics, modules
- Lambda closures, async scheduler
- SIMD vectors, GC nursery, struct methods
- Stack traces, fat pointers, traits
- Wildx memory, runtime assembler
- Metaprogramming, diagnostics, safe navigation

See `RESEARCH_COVERAGE_ANALYSIS.md` in workspace for details.
