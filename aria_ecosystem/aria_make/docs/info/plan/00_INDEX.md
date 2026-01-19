# AriaMake Build System - Engineering Plan Index

**Generated:** December 20, 2025  
**Source:** 34 Gemini research reports (gem_01-07, gem2_01-08, gem3_01-08, gem4_01-06, task_01-03)  
**Status:** Consolidated Implementation Plan  

---

## 📚 Document Organization

This engineering plan is organized into logical sections. Read them in numerical order for implementation:

### Core Documents

- **[00_INDEX.md](./00_INDEX.md)** (this file) - Navigation and overview
- **[01_OVERVIEW.md](./01_OVERVIEW.md)** - Executive summary, goals, and architecture
- **[02_DEPENDENCY_MATRIX.md](./02_DEPENDENCY_MATRIX.md)** - Component dependencies and build order
- **[03_IMPLEMENTATION_PHASES.md](./03_IMPLEMENTATION_PHASES.md)** - Phased rollout plan with priorities

### Component Specifications

- **[10_PARSER_CONFIG.md](./10_PARSER_CONFIG.md)** - ConfigParser & Variable Interpolation
- **[20_FILE_DISCOVERY.md](./20_FILE_DISCOVERY.md)** - Globbing Engine & FileSystemTraits
- **[30_DEPENDENCY_GRAPH.md](./30_DEPENDENCY_GRAPH.md)** - DAG construction & Cycle Detection
- **[40_EXECUTION_ENGINE.md](./40_EXECUTION_ENGINE.md)** - ThreadPool, Scheduler, Process Execution
- **[50_INCREMENTAL_BUILDS.md](./50_INCREMENTAL_BUILDS.md)** - Timestamp checking & Command hashing
- **[60_TOOLING_INTEGRATION.md](./60_TOOLING_INTEGRATION.md)** - Compilation database & Dependency tracking

### Reference Materials

- **[90_ALGORITHMS.md](./90_ALGORITHMS.md)** - Key algorithms (Kahn's, Tri-color DFS, Shifting Wildcard)
- **[91_PLATFORM_COMPAT.md](./91_PLATFORM_COMPAT.md)** - Cross-platform considerations
- **[92_PERFORMANCE_TARGETS.md](./92_PERFORMANCE_TARGETS.md)** - Benchmarks and optimization requirements
- **[99_CRITICAL_NOTES.md](./99_CRITICAL_NOTES.md)** - Common pitfalls and gotchas

---

## 🎯 Quick Start

**New to the project?** Read in this order:
1. [01_OVERVIEW.md](./01_OVERVIEW.md) - Understand the big picture
2. [03_IMPLEMENTATION_PHASES.md](./03_IMPLEMENTATION_PHASES.md) - See the roadmap
3. [02_DEPENDENCY_MATRIX.md](./02_DEPENDENCY_MATRIX.md) - Understand component relationships
4. Component specs (10-60) as needed

**Ready to implement?**
1. Check [03_IMPLEMENTATION_PHASES.md](./03_IMPLEMENTATION_PHASES.md) for current phase
2. Read the relevant component spec (10-60)
3. Review [90_ALGORITHMS.md](./90_ALGORITHMS.md) for algorithm details
4. Check [99_CRITICAL_NOTES.md](./99_CRITICAL_NOTES.md) before writing code

---

## 📊 Research Source Mapping

| Component | Primary Sources |
|-----------|----------------|
| ConfigParser | gem_01.txt, task_01_parser_implementation.txt |
| Variable Interpolation | gem_02.txt, gem2_07.txt |
| Globbing Engine | gem_03.txt, task_02_globbing_engine.txt, gem2_05.txt |
| FileSystemTraits | gem_04.txt |
| Dependency Graph | gem_05.txt, task_03_dependency_graph.txt |
| Cycle Detection | gem2_06.txt |
| Thread Pool & Scheduler | gem_06.txt |
| Process Execution | gem_07.txt, gem2_02.txt, gem3_01.txt |
| Toolchain Orchestration | gem2_03.txt, gem3_04.txt |
| Command Hashing | gem2_01.txt, gem4_01.txt |
| Dependency Tracking | gem2_04.txt |
| Exclusion Logic | gem2_05.txt |
| Environment Variables | gem2_07.txt |
| Compilation Database | gem2_08.txt |
| Module Resolution | gem3_02.txt, gem3_03.txt |
| Test Automation | gem3_05.txt, gem4_03.txt |
| Macro Hygiene | gem3_06.txt |
| Monomorphization Cache | gem3_07.txt |
| FileWatcher | gem3_08.txt |
| Clean Target | gem4_02.txt |

---

## 🚀 Project Philosophy

From the research synthesis, aria_make is built on these principles:

1. **"Build System as Code"** - Configuration is data (JSON-superset), not imperative scripts
2. **Determinism First** - Identical builds across platforms, sorted outputs, hermetic state
3. **Developer Experience** - Human-readable configs, precise error messages, LSP integration
4. **Performance** - Sub-10ms parsing, optimal parallelism, minimal I/O
5. **No Deferrals** - Every component must work completely before moving forward

---

## ⚠️ Important Notes

- All paths use **POSIX forward slashes** (`/`) internally, even on Windows
- **Close pipe write ends** in parent process or readers hang forever
- Use **atomic counters** for parallel in-degree updates
- **Sort all filesystem results** for deterministic builds
- **Tri-color marking** (white/gray/black) distinguishes diamonds from cycles
- **Cache interpolation results** - variables may appear 50+ times

---

## 📖 Version History

| Date | Version | Changes |
|------|---------|---------|
| 2025-12-20 | 1.0 | Initial consolidated plan from 34 research files |

---

**Next:** Read [01_OVERVIEW.md](./01_OVERVIEW.md) for system architecture overview
