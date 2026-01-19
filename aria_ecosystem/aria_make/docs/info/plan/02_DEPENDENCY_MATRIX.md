# Dependency Matrix & Build Order

## Component Dependency Graph

```
┌─────────────┐
│ Aria Lexer  │ (External - already exists)
└──────┬──────┘
       │
       v
┌──────────────────┐
│  ConfigParser    │
└────────┬─────────┘
         │
         v
┌──────────────────┐       ┌──────────────────┐
│  Interpolator    │       │ FileSystemTraits │ (Standalone)
└─────────┬────────┘       └────────┬─────────┘
          │                         │
          └────────┬────────────────┘
                   │
                   v
┌──────────────────────────────────┐
│  Validated Build Configuration   │
└────────────┬─────────────────────┘
             │
    ┌────────┴────────┐
    │                 │
    v                 v
┌─────────┐     ┌──────────────┐
│ GlobEng │     │ DepGraph     │ (Standalone)
│ ine     │     │              │
└────┬────┘     └──────┬───────┘
     │                 │
     │                 v
     │          ┌──────────────┐
     │          │ CycleDetector│
     │          └──────┬───────┘
     │                 │
     └────────┬────────┘
              │
              v
┌──────────────────────────────────┐
│  Validated DAG + Source Files     │
└────────────┬─────────────────────┘
             │
             v
┌──────────────────────────────────┐
│  Incremental Analysis             │
│  (Timestamp + Command Hash)       │
└────────────┬─────────────────────┘
             │
             v
┌──────────────────────────────────┐
│      Build Scheduler              │
│      (Kahn's Algorithm)           │
└────────┬───────────┬─────────────┘
         │           │
         v           v
    ┌────────┐  ┌─────────────────┐
    │ Thread │  │ Toolchain       │
    │ Pool   │  │ Orchestrator    │
    └───┬────┘  └────────┬────────┘
        │                │
        └───────┬────────┘
                │
                v
        ┌───────────────┐
        │ Process       │
        │ Execution PAL │
        └───────┬───────┘
                │
                v
        ┌───────────────┐
        │  Compiled     │
        │  Artifacts    │
        └───────────────┘
```

---

## Detailed Dependency Table

| Component | Depends On | Provides To | Standalone? |
|-----------|-----------|-------------|-------------|
| **Aria Lexer** | (external) | ConfigParser | ✅ |
| **FileSystemTraits** | None | GlobEngine, Toolchain | ✅ |
| **ConfigParser** | Aria Lexer | Interpolator, BuildGraph | ❌ |
| **Interpolator** | ConfigParser | BuildGraph | ❌ |
| **GlobEngine** | FileSystemTraits | BuildGraph | ⚠️ Semi |
| **DependencyGraph** | None | CycleDetector, Scheduler | ✅ |
| **CycleDetector** | DependencyGraph | Validator | ❌ |
| **ThreadPool** | None | BuildScheduler | ✅ |
| **Process Execution (PAL)** | None | BuildScheduler | ✅ |
| **ToolchainOrchestrator** | FileSystemTraits, DepGraph | BuildScheduler | ❌ |
| **BuildScheduler** | ThreadPool, DepGraph, Toolchain, PAL | Main | ❌ |
| **BuildState** | nlohmann/json | Incremental Logic | ⚠️ Semi |
| **DependencyPrinter** | Parser/AST | Compiler Driver | ❌ |
| **CompileDBWriter** | DepGraph, Toolchain | LSP Tools | ❌ |

**Legend:**
- ✅ **Standalone** - Can be implemented and tested independently
- ⚠️ **Semi-standalone** - Has external dependencies but can be mocked
- ❌ **Dependent** - Requires other components to be functional

---

## Implementation Order (Critical Path)

### Level 0: Pure Infrastructure (No Dependencies)
Can be implemented in parallel:

1. **FileSystemTraits**
   - Path normalization (`\ → /`)
   - Hidden file detection (Unix `.file` + Windows attributes)
   - Timestamp utilities
   - **Testing:** Unit tests with temp directories

2. **DependencyGraph (data structures only)**
   - `Node` class with atomic counters
   - `DependencyGraph` container
   - Basic graph construction (no validation yet)
   - **Testing:** Create graphs manually, verify structure

3. **ThreadPool**
   - Worker thread management
   - Task queue with mutex/condition variable
   - Graceful shutdown
   - **Testing:** Submit dummy tasks, verify parallelism

4. **Process Execution (PAL)**
   - Platform abstraction (POSIX vs Windows)
   - Pipe creation and stream capture
   - Threaded draining (prevent deadlock)
   - **Testing:** Execute `echo`, `cat`, verify output capture

---

### Level 1: Parsing (Depends on Aria Lexer)

5. **ConfigParser**
   - Recursive descent parser
   - Panic mode recovery
   - AST construction (`BuildFileNode`, `ProjectNode`, etc.)
   - **Testing:** Parse sample `build.aria` files

6. **Interpolator**
   - Tokenize `&{variable}` syntax
   - Tri-color DFS for cycle detection
   - Environment variable support (`&{ENV.VAR}`)
   - Memoization cache
   - **Testing:** Resolve complex nested variables, detect cycles

---

### Level 2: File Discovery (Depends on Level 0)

7. **GlobEngine**
   - Pattern parser (split into segments)
   - Anchor-point optimization
   - FastMatcher (Shifting Wildcard algorithm)
   - Exclusion pruning
   - **Testing:** Match against test directory trees

---

### Level 3: Graph Validation (Depends on Level 0 + Level 1)

8. **CycleDetector**
   - Tri-color DFS traversal
   - Path reconstruction (stack tracking)
   - Diamond vs cycle distinction
   - **Testing:** Construct graphs with known cycles, verify detection

---

### Level 4: Command Synthesis (Depends on Level 0)

9. **ToolchainOrchestrator**
   - Compiler command construction (`ariac` flags)
   - Linker command construction (platform-specific)
   - Include path resolution (`-I` flags from deps)
   - **Testing:** Mock graph, verify generated commands

---

### Level 5: Incremental Build Logic (Depends on Level 0)

10. **Timestamp Checker**
    - Compare source vs output mtimes
    - Dependency propagation
    - **Testing:** Touch files, verify dirty detection

11. **Command Signature Hasher**
    - FNV-1a hash implementation
    - BuildState JSON persistence
    - **Testing:** Change flags, verify rebuild trigger

---

### Level 6: Build Orchestration (Depends on ALL previous)

12. **BuildScheduler**
    - Kahn's algorithm implementation
    - Integration with ThreadPool
    - Dynamic graph reduction (in-degree updates)
    - Error handling and propagation
    - **Testing:** End-to-end builds with mock compiler

---

### Level 7: Tooling Integration (Depends on Level 6)

13. **CompileDBWriter**
    - JSON generation with proper escaping
    - Per-source-file entries
    - **Testing:** Validate JSON schema

14. **DependencyPrinter**
    - Makefile-format `.d` file generation
    - AST scanning for `use` and `embed_file()`
    - **Testing:** Parse generated `.d` files

---

## Parallel Development Strategy

To maximize development velocity, components can be developed concurrently by different engineers:

### Team A: Core Infrastructure
- FileSystemTraits
- ThreadPool
- Process Execution (PAL)
- DependencyGraph (data structures)

### Team B: Parsing & Configuration
- ConfigParser (reusing Aria Lexer)
- Interpolator
- CycleDetector

### Team C: File Discovery
- GlobEngine
- FastMatcher
- Exclusion logic

### Integration Team
- ToolchainOrchestrator
- BuildScheduler
- Incremental build logic

**Sync Points:**
1. After Level 0: Integrate FileSystemTraits with GlobEngine
2. After Level 3: Integrate Parser + Graph + Glob into unified pipeline
3. After Level 5: Integrate incremental logic with scheduler
4. After Level 6: End-to-end testing with real Aria compiler

---

## Testing Strategy

### Unit Tests (per component)
- **FileSystemTraits:** Temp directory manipulation, path edge cases
- **ConfigParser:** Valid/invalid ABC files, error recovery
- **Interpolator:** Nested vars, cycles, missing vars
- **GlobEngine:** Pattern matching, exclusions, sorting
- **CycleDetector:** Known cycles, diamonds, complex graphs
- **ThreadPool:** Task completion, error propagation, shutdown
- **PAL:** Output capture, exit codes, pipe deadlock prevention

### Integration Tests (component pairs)
- **Parser + Interpolator:** Full ABC file with variable resolution
- **Glob + FileSystemTraits:** Cross-platform path handling
- **Graph + CycleDetector:** Build DAG construction and validation
- **Scheduler + ThreadPool:** Parallel task execution
- **Scheduler + PAL:** Real compiler invocation

### End-to-End Tests
- **Simple project:** 1 source file, no dependencies
- **Library + executable:** Diamond dependency
- **Complex project:** 50+ files, deep dependency tree
- **Incremental rebuild:** Touch source, verify minimal rebuild
- **Flag change:** Modify config, verify full rebuild

---

## Critical Path Analysis

**Longest dependency chain (13 components):**

```
Aria Lexer (0)
  → ConfigParser (1)
  → Interpolator (2)
  → GlobEngine (3, also needs FileSystemTraits)
  → DependencyGraph (4)
  → CycleDetector (5)
  → Incremental Logic (6)
  → ToolchainOrchestrator (7)
  → BuildScheduler (8)
  → (Ready for first build)
```

**Minimum viable build system:** Components 0-8 above  
**Full-featured system:** Add CompileDB + DependencyPrinter (components 13-14)

**Estimated implementation time (1 engineer):**
- Level 0-1: 2 weeks (infrastructure + parsing)
- Level 2-3: 1 week (file discovery + validation)
- Level 4-5: 1 week (command synthesis + incrementalism)
- Level 6: 2 weeks (scheduler + integration)
- Level 7: 1 week (tooling)
- **Total:** ~7-8 weeks for MVP

**Estimated implementation time (3-engineer team):**
- Weeks 1-2: Parallel work on Levels 0-1
- Week 3: Integration + Level 2-3
- Week 4: Level 4-5
- Weeks 5-6: Level 6 integration + debugging
- Week 7: Level 7 + polish
- **Total:** ~7 weeks with better test coverage

---

## External Dependencies

### Build-time
- **C++17 compiler** (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake 3.12+** (for building aria_make itself)
- **Aria Lexer** (from aria compiler project)

### Runtime
- **Aria Compiler (`ariac`)** - Must be in PATH
- **LLVM Linker (`lld`)** - Optional but recommended
- **LLVM Interpreter (`lli`)** - For test target execution

### Optional
- **nlohmann/json** - For BuildState JSON serialization
  - Alternative: Hand-rolled JSON writer (simple for our use case)

---

## Risk Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| Aria Lexer API changes | HIGH | Maintain thin adapter layer |
| Platform-specific bugs | MEDIUM | Heavy cross-platform testing |
| Cycle detection false positives | HIGH | Comprehensive test suite with known graphs |
| Pipe deadlock in PAL | HIGH | Always spawn drain threads, verify with stress test |
| Glob performance on large repos | MEDIUM | Anchor-point optimization + exclusion caching |
| Command hash collisions | LOW | Use 64-bit FNV-1a (collision probability ~2^-64) |

---

**Next:** Read [03_IMPLEMENTATION_PHASES.md](./03_IMPLEMENTATION_PHASES.md) for detailed phase breakdown
