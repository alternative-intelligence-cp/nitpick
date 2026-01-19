# AriaBuild (aria_make) - Gemini Research Task Breakdown
**Generated**: December 19, 2025
**Source**: gemini_gap_todo.txt architectural audit

## Overview

This document breaks down the comprehensive gap analysis into 7 discrete, actionable research tasks. Each task includes:
- Priority level
- Required context files to upload to Gemini
- Specific prompt/question
- Dependencies on other tasks
- Expected deliverables

---

## TASK 1: Configuration Parser and AST Visitor

**Priority**: CRITICAL (Blocks all other features)
**Estimated Complexity**: High
**Dependencies**: None

### Problem Statement
The current aria::Parser is designed for the Aria language, not for the ABC (Aria Build Configuration) format. We need a specialized ConfigParser that can handle:
- JSON-superset syntax with unquoted keys
- Top-level sections: `project:`, `variables:`, `targets:`
- Object and array literals (currently stubbed)
- Visitor pattern for AST traversal

### Required Context Files

**From aria repository**:
1. `include/frontend/ast/ast_node.h` - Base AST structure
2. `include/frontend/ast/expr.h` - Expression nodes
3. `src/frontend/lexer/lexer.cpp` - Token scanning logic
4. `src/frontend/parser/parser.h` - Current parser interface
5. `src/frontend/parser/parser.cpp` - Parser implementation (partial)
6. `include/frontend/token.h` - Token definitions

**From aria_make repository**:
1. `docs/research/gemini/responses/task_01_parser_implementation.txt` - Detailed design spec
2. `docs/info/Designing a JSON-like Build Tool.txt` - ABC syntax specification
3. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (Section 2)

### Gemini Prompt

```
You have identified a critical gap between the general-purpose Aria language parser and the specific requirements for the Aria Build Configuration (ABC) format. Based on the design in 'task_01_parser_implementation.txt' and the ABC specification, generate the complete C++ header and implementation for the ConfigParser class.

Requirements:
1. Reuse the aria::frontend::Lexer but implement a LexerAdapter to filter language-specific tokens
2. Implement recursive descent methods for:
   - parseProjectSection() - handles project metadata
   - parseVariablesBlock() - handles variable definitions
   - parseTargetsBlock() - handles build target definitions
3. Implement the missing parseObjectLiteral() and parseArrayLiteral() logic specifically for build configurations, supporting unquoted keys (e.g., name: "value" not just "name": "value")
4. Define a BuildFileAST structure distinct from the language AST
5. Define a Visitor interface (abstract base class) with methods like visit(ProjectNode*), visit(TargetNode*), visit(VariableNode*)
6. Implement accept() methods in each AST node to dispatch to the visitor
7. Ensure strict error recovery using "panic mode" synchronization points at section boundaries

The parser must handle the ABC grammar without triggering errors on valid build files. Provide both header (.h) and implementation (.cpp) files with detailed comments explaining the grammar rules.
```

### Expected Deliverables
- `include/config/config_parser.h` - ConfigParser class declaration
- `src/config/config_parser.cpp` - Implementation
- `include/config/build_ast.h` - Build-specific AST nodes
- `include/config/visitor.h` - Visitor pattern interface
- Example usage code showing parsing of a sample build.aria file

---

## TASK 2: Variable Interpolation Engine

**Priority**: HIGH (Required for usable build files)
**Estimated Complexity**: Medium
**Dependencies**: TASK 1 (needs AST structure)

### Problem Statement
The ABC format supports variable substitution using `&{var_name}` syntax. This requires:
- Recursive variable resolution (variables can reference other variables)
- Environment variable access via `ENV.` prefix
- Cycle detection to prevent infinite loops
- Proper scoping (local > global > environment)

### Required Context Files

**From aria repository**:
1. `include/frontend/sema/symbol_table.h` - For reference (not direct reuse)
2. `src/frontend/sema/symbol_table.cpp` - Symbol lookup patterns

**From aria_make repository**:
1. `docs/info/Designing a JSON-like Build Tool.txt` - Section 2.3 on variable substitution
2. `docs/research/gemini/responses/task_01_parser_implementation.txt` - Section 6 on interpolation
3. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (Section 2.2)

### Gemini Prompt

```
The variable substitution logic for the AriaBuild system is missing. Using the specification in 'Designing a JSON-like Build Tool.txt' (Section 2.3) and 'task_01_parser_implementation.txt' (Section 6), provide the complete C++ implementation for the Interpolator class.

Requirements:
1. Accept a raw string containing &{...} tokens and a VariableMap (std::map<std::string, std::string>)
2. Recursively resolve variable references:
   - If &{A} contains &{B}, resolve B first, then substitute into A
   - Handle multi-level nesting (A→B→C)
3. Support environment variable access:
   - Detect ENV. prefix (e.g., &{ENV.HOME})
   - Use std::getenv() for lookups
   - Throw clear error if env var doesn't exist
4. Implement strict cycle detection:
   - Use a std::unordered_set<std::string> for the current resolution path
   - Detect if a variable references itself (directly or transitively)
   - Generate human-readable error: "Cycle detected: A → B → C → A"
5. Handle undefined variables:
   - Throw exception with variable name and location if possible
6. Optimization: Cache resolved values to avoid redundant resolution

Provide header and implementation with unit test examples showing:
- Simple substitution: &{src} → "src/"
- Nested: &{output} = "&{build}/bin" where build="build/"
- Environment: &{home} = "&{ENV.HOME}"
- Cycle detection: A=&{B}, B=&{A} → error
```

### Expected Deliverables
- `include/config/interpolator.h` - Interpolator class
- `src/config/interpolator.cpp` - Implementation
- `tests/config/test_interpolator.cpp` - Unit tests

---

## TASK 3: Globbing Engine and FastMatcher

**Priority**: CRITICAL (Required for source file discovery)
**Estimated Complexity**: Very High
**Dependencies**: None (standalone component)

### Problem Statement
AriaBuild needs a high-performance, non-regex globbing engine for pattern matching like `src/**/*.aria`. The system must:
- Implement the "Shifting Wildcard" algorithm for efficiency
- Handle recursive globstar `**` correctly
- Support POSIX character classes `[a-z]`, `[!abc]`
- Provide platform-independent hidden file detection
- Implement caching with proper invalidation

### Required Context Files

**From aria repository**:
None directly needed (this is a standalone C++ component)

**From aria_make repository**:
1. `docs/research/gemini/responses/task_02_globbing_engine.txt` - Complete design specification
2. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (Section 3)

### Gemini Prompt

```
The build system requires a high-performance, non-regex globbing engine as detailed in 'task_02_globbing_engine.txt'. The provided specification is incomplete (missing character class logic and cache invalidation). Provide the complete C++17 implementation for the GlobPattern, FastMatcher, and GlobEngine classes.

Requirements:

**GlobPattern Class**:
1. Parse a glob pattern string (e.g., "src/**/*.aria")
2. Normalize path separators (Windows \ → Unix /)
3. Tokenize into segments, identifying:
   - Static segments: "src", "core"
   - Wildcard segments: "*", "**"
   - Character class segments: "[a-z]", "[!0-9]"
4. Calculate anchor point (longest static prefix for optimization)

**FastMatcher Class** (the "Shifting Wildcard" algorithm):
1. Implement match(const std::string& text, const std::string& pattern)
2. Handle * (matches any characters except /)
3. Handle ? (matches single character except /)
4. Handle ** (matches zero or more directory levels)
5. Implement match_class(char c, const std::string& class_pattern):
   - Parse character ranges: [a-z] matches 'a' through 'z'
   - Parse negation: [!abc] or [^abc] matches anything except a, b, c
   - Parse literal characters: [abc] matches a, b, or c
   - Handle special cases: []] (bracket as first char), [-] (dash as first/last)
   - Return true if character matches the class
6. Ensure / (path separator) is never matched by *, ?, or character classes

**GlobEngine Class**:
1. Implement expand(const std::string& pattern) returning std::vector<std::filesystem::path>
2. Use segment-based traversal:
   - Start at anchor point (don't scan from root)
   - For ** segments, recursively descend
   - For * segments, list directory and filter
3. Implement platform-specific hidden file detection:
   - Unix: starts with '.'
   - Windows: use GetFileAttributesW() to check FILE_ATTRIBUTE_HIDDEN
4. Sort results lexicographically for determinism
5. Implement caching:
   - Key: hash of (pattern + last modified time of anchor directory)
   - Invalidate cache if directory timestamp changes
   - Store in std::unordered_map<size_t, std::vector<path>>

Provide detailed comments explaining the algorithm, particularly the match_class logic which was omitted in the specification.
```

### Expected Deliverables
- `include/glob/glob_pattern.h` - Pattern parser
- `include/glob/fast_matcher.h` - Matching algorithm
- `include/glob/glob_engine.h` - Engine with caching
- `src/glob/glob_pattern.cpp` - Implementation
- `src/glob/fast_matcher.cpp` - Implementation
- `src/glob/glob_engine.cpp` - Implementation
- `tests/glob/test_glob_engine.cpp` - Comprehensive unit tests
- Performance benchmarks comparing to std::regex

---

## TASK 4: Platform-Specific Filesystem Extensions

**Priority**: MEDIUM (Enhances Task 3)
**Estimated Complexity**: Low
**Dependencies**: TASK 3 (extends GlobEngine)

### Problem Statement
The GlobEngine needs platform-specific helpers for:
- Hidden file detection on Windows (FILE_ATTRIBUTE_HIDDEN)
- Cache invalidation based on directory modification times
- Portable path normalization

### Required Context Files

**From aria_make repository**:
1. `docs/research/gemini/responses/task_02_globbing_engine.txt` - Section 3.4 on caching
2. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (Section 3.3, 3.4)

### Gemini Prompt

```
To ensure cross-platform determinism in the globbing engine, provide a C++ helper class FileSystemTraits that extends the functionality described in 'task_02_globbing_engine.txt'.

Requirements:
1. Implement isHidden(const std::filesystem::path& p) → bool:
   - On Windows: Use GetFileAttributesW() to check FILE_ATTRIBUTE_HIDDEN
   - On Unix: Check if filename starts with '.'
   - Use #ifdef _WIN32 for platform detection
2. Implement getDirHash(const std::filesystem::path& dir) → size_t:
   - Hash the last_write_time of the directory itself
   - Used as cache invalidation key
   - Handle errors if directory doesn't exist
3. Implement normalizePath(std::filesystem::path& p) → void:
   - Convert all backslashes to forward slashes
   - Ensure consistent representation across platforms

Use C++17 std::filesystem where possible, falling back to platform APIs (Windows.h, unistd.h) only where necessary. Provide both header and implementation with conditional compilation.
```

### Expected Deliverables
- `include/glob/fs_traits.h` - Platform traits
- `src/glob/fs_traits.cpp` - Implementation with platform conditionals
- `tests/glob/test_fs_traits.cpp` - Cross-platform tests

---

## TASK 5: Dependency Graph and Cycle Detection

**Priority**: CRITICAL (Core build algorithm)
**Estimated Complexity**: Medium
**Dependencies**: TASK 1 (needs BuildFileAST)

### Problem Statement
The dependency graph implementation in the research is incomplete:
- CycleDetector's DFS logic terminates mid-implementation
- Missing path reconstruction for error reporting
- Missing backtracking logic (recursion stack cleanup)

### Required Context Files

**From aria_make repository**:
1. `docs/research/gemini/responses/task_03_dependency_graph.txt` - Graph design (incomplete)
2. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (Section 4.1)

### Gemini Prompt

```
The implementation of the CycleDetector class in 'task_03_dependency_graph.txt' is incomplete and terminates abruptly during the is_cyclic_dfs method. Provide the complete, corrected C++ implementation for the CycleDetector class.

Requirements:
1. Complete the is_cyclic_dfs method:
   - Mark node as visiting (in recursion stack)
   - For each neighbor, check if:
     * Already in recursion stack → cycle found, call reconstruct_path
     * Not yet visited → recurse
   - Mark node as visited and REMOVE from recursion stack (backtracking)
   - Critical: Must distinguish between:
     * Cycle: A→B→C→A (error)
     * Diamond: A→B→D, A→C→D (valid)
2. Implement reconstruct_path(int start, int end):
   - Walk back through the parent map (stored during DFS)
   - Build a vector of node IDs representing the cycle
   - Return as std::vector<int> for error reporting
3. Implement get_cycle_names(const std::vector<int>& cycle_ids):
   - Convert node IDs to human-readable target names
   - Format as "A → B → C → A"
4. Ensure the DependencyGraph class has:
   - std::map<int, std::string> node_id_to_name for error messages
   - add_edge method that properly maintains adjacency lists

Provide the complete implementation with comments explaining the DFS coloring (white/gray/black) or equivalent recursion stack approach.
```

### Expected Deliverables
- `include/graph/cycle_detector.h` - CycleDetector class (corrected)
- `include/graph/dependency_graph.h` - DependencyGraph with name mapping
- `src/graph/cycle_detector.cpp` - Complete DFS implementation
- `src/graph/dependency_graph.cpp` - Graph management
- `tests/graph/test_cycle_detection.cpp` - Diamond vs cycle tests

---

## TASK 6: Build Scheduler and Thread Pool

**Priority**: CRITICAL (Execution engine)
**Estimated Complexity**: Very High
**Dependencies**: TASK 5 (needs DependencyGraph)

### Problem Statement
The build execution system is entirely missing:
- ThreadPool for parallel task execution
- BuildScheduler implementing dynamic Kahn's algorithm
- Incremental build logic (dirty checking)
- Thread-safe graph updates during execution

### Required Context Files

**From aria_make repository**:
1. `docs/research/gemini/responses/task_03_dependency_graph.txt` - Partial scheduler design
2. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (Section 4.2, 4.3)

### Gemini Prompt

```
The execution engine for AriaBuild is missing from the provided documentation. Based on the requirements in 'task_03_dependency_graph.txt', provide the complete C++17 implementation for the ThreadPool and BuildScheduler classes.

Requirements:

**ThreadPool Class**:
1. Use std::thread, std::mutex, std::condition_variable
2. Fixed worker count (default: std::thread::hardware_concurrency())
3. Task queue: std::queue<std::function<void()>>
4. Submit method: enqueue(std::function<void()> task)
5. Worker thread loop:
   - Wait on condition variable for tasks
   - Pop and execute task
   - Handle exceptions gracefully
6. Destructor: Join all threads cleanly

**BuildScheduler Class**:
1. Implement schedule(DependencyGraph& graph):
   - Calculate initial in-degrees for all nodes
   - Populate ReadyQueue with nodes having in-degree 0
2. Execute loop:
   - Pop task from ReadyQueue
   - Check is_dirty (using check_is_dirty function)
   - If dirty, submit to ThreadPool
   - On task completion:
     * Lock graph mutex
     * For each dependent, decrement in-degree
     * If in-degree reaches 0, add to ReadyQueue
3. Thread-safe queue operations:
   - Use std::mutex for ReadyQueue access
   - Use std::condition_variable to signal new tasks
4. Error handling:
   - If any task fails, set global error flag
   - Continue executing independent tasks
   - Propagate failure to final status

**Incremental Build Logic** (check_is_dirty function):
1. For each source file, compare std::filesystem::last_write_time
2. If source is newer than output → dirty
3. If any dependency is dirty → transitively dirty
4. Cache dirty status in Node structure

Provide complete implementation with detailed comments on the synchronization strategy. Include example showing how BuildScheduler interacts with DependencyGraph and ThreadPool.
```

### Expected Deliverables
- `include/scheduler/thread_pool.h` - ThreadPool class
- `include/scheduler/build_scheduler.h` - BuildScheduler class
- `src/scheduler/thread_pool.cpp` - Implementation
- `src/scheduler/build_scheduler.cpp` - Implementation with Kahn's algorithm
- `tests/scheduler/test_thread_pool.cpp` - Concurrency tests
- `tests/scheduler/test_scheduler.cpp` - Integration tests

---

## TASK 7: Process Execution and Toolchain Orchestration

**Priority**: HIGH (Required for actual compilation)
**Estimated Complexity**: Medium
**Dependencies**: TASK 6 (invoked by scheduler)

### Problem Statement
AriaBuild must spawn external processes (ariac, lli) and capture their output:
- std::system is blocking and dumps to console (breaks parallelism)
- popen only captures stdout by default (stderr has errors)
- Need separate stdout/stderr capture for colored output
- Need proper exit code handling

### Required Context Files

**From aria repository**:
1. `docs/info/aria_specs.txt` - Compiler CLI interface
2. Documentation on ariac command-line flags

**From aria_make repository**:
1. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (Section 5)

### Gemini Prompt

```
AriaBuild requires a robust mechanism to spawn child processes (compilers, interpreters) and capture their output. Provide a complete C++ implementation for process execution and toolchain orchestration.

Requirements:

**execute_command function**:
1. Signature: execute_command(const std::string& cmd, std::string& stdout_capture, std::string& stderr_capture) → int (exit code)
2. Implementation options:
   - Preferred: Use popen with shell redirection to capture both streams
   - Alternative: Platform-specific (fork/exec/pipe on POSIX, CreateProcess on Windows)
3. Requirements:
   - Non-blocking from ThreadPool perspective (can block calling thread)
   - Capture stdout to stdout_capture
   - Capture stderr to stderr_capture
   - Return exit code (0 = success)
   - Handle command not found (e.g., ariac missing)
4. Provide both a C++17-only version (using popen with 2>&1) and a platform-specific version for separate stream capture if feasible

**ToolchainOrchestrator Class**:
1. Method: build_aria_binary(const TargetConfig& target) → bool
   - Construct ariac command line:
     * Source files from glob expansion
     * -o flag for output path
     * --emit-llvm if target type is "library"
     * Include directories from target.includes
   - Execute command via execute_command
   - Parse stderr for errors/warnings
   - Return true if exit code == 0
2. Method: run_with_lli(const std::filesystem::path& ll_file) → int
   - Construct lli command: "lli <file.ll>"
   - Execute and capture output
   - Return exit code

Provide implementation with error handling for missing tools, detailed logging, and example usage showing compilation of a multi-file Aria project.
```

### Expected Deliverables
- `include/toolchain/process_executor.h` - execute_command function
- `include/toolchain/toolchain_orchestrator.h` - ToolchainOrchestrator class
- `src/toolchain/process_executor.cpp` - Implementation (portable)
- `src/toolchain/toolchain_orchestrator.cpp` - CLI construction
- `tests/toolchain/test_executor.cpp` - Process spawn tests

---

## Task Dependency Graph

```
           TASK 1 (Parser)
          /       |       \
         /        |        \
    TASK 2    TASK 5    [TASK 7]
    (Interp)  (Graph)   (Toolchain)
                 |            |
              TASK 6 ---------|
            (Scheduler)
                 |
              TASK 7
            (Execution)

    TASK 3 (Globbing) ────────────→ TASK 1 (used by targets)
         |
    TASK 4 (FS traits)
```

**Critical Path**: TASK 1 → TASK 5 → TASK 6 → TASK 7
**Parallel Path**: TASK 3 (can be developed independently)

---

## Implementation Priority Order

### Phase 1: Foundation (Week 1-2)
1. **TASK 3**: Globbing Engine (standalone, testable)
2. **TASK 4**: Filesystem traits (extends Task 3)
3. **TASK 1**: ConfigParser (enables configuration reading)

### Phase 2: Core Logic (Week 3-4)
4. **TASK 2**: Interpolator (extends Task 1)
5. **TASK 5**: Cycle Detection (standalone algorithm)

### Phase 3: Execution (Week 5-6)
6. **TASK 7**: Process Executor (standalone, testable with mock)
7. **TASK 6**: Build Scheduler (integrates everything)

---

## File Upload Checklist for Each Task

### General Files (Upload for ALL tasks)
- `docs/research/gemini/tasks/gemini_gap_todo.txt` - Context on gaps
- `docs/info/aria_specs.txt` - Language specification (partial)
- `docs/info/Designing a JSON-like Build Tool.txt` - ABC specification

### Task-Specific Files (see each task section above)

---

## Success Criteria

Each task is complete when:
1. ✅ All required classes implemented with headers and source
2. ✅ Unit tests written and passing
3. ✅ Integration test demonstrating the feature in context
4. ✅ Documentation comments explaining algorithms
5. ✅ Code compiles with C++17 standard on Linux/Windows
6. ✅ No memory leaks (validated with valgrind or ASAN)

---

## Notes for Gemini Interaction

**Context Window Management**:
- Max 10 files per upload
- Prioritize task-specific research docs over source code
- If aria source is needed, prefer headers over implementations (smaller)
- Use markdown code blocks for inline examples rather than full files

**Response Format Request**:
- Request header file first, review, then implementation
- Request one class at a time for complex tasks (e.g., Task 6)
- Ask for example usage code to validate understanding

**Iteration Strategy**:
- Start with skeleton/interface, validate design
- Implement core algorithm, validate correctness
- Add error handling and edge cases
- Optimize performance if needed
