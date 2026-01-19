# Implementation Phases

## Phase Breakdown

This document provides a detailed, prioritized implementation roadmap for aria_make. Each phase builds on previous phases and includes acceptance criteria.

---

## 🚀 Phase 1: Foundation (Week 1-2)

**Goal:** Establish core infrastructure that all other components depend on.

### Components

#### 1.1 FileSystemTraits
**Priority:** CRITICAL  
**Complexity:** LOW  
**Time:** 2-3 days

**Implementation:**
- Path normalization (Windows `\` → POSIX `/`)
- Hidden file detection (Unix dotfiles + Windows attributes)
- Timestamp utilities (handle C++17 `file_time_type` portability)
- Directory hashing for cache invalidation

**Deliverables:**
```cpp
class FileSystemTraits {
    static std::string normalizePath(const fs::path& path);
    static bool isHidden(const fs::path& path);
    static size_t hashDirectory(const fs::path& path, bool recursive);
    static std::filesystem::file_time_type getModTime(const fs::path& path);
};
```

**Tests:**
- Path normalization across platforms
- Hidden file detection on Unix/Windows
- Directory hash stability (sorted order)
- Timestamp comparison edge cases

**Acceptance Criteria:**
- ✅ All paths use `/` separator internally
- ✅ Hidden files correctly detected on all platforms
- ✅ Directory hashes are deterministic (same input → same hash)

---

#### 1.2 DependencyGraph (Data Structures)
**Priority:** CRITICAL  
**Complexity:** MEDIUM  
**Time:** 3-4 days

**Implementation:**
- `Node` class with atomic in-degree counter
- `DependencyGraph` container with unique_ptr ownership
- Edge management (dependencies + dependents)
- Basic graph construction API

**Deliverables:**
```cpp
class Node {
    std::atomic<int> in_degree_;
    std::atomic<Status> status_;
    std::vector<Node*> dependencies_;
    std::vector<Node*> dependents_;
    std::string name_, output_file, command;
    std::vector<std::string> source_files, flags;
};

class DependencyGraph {
    std::unordered_map<std::string, std::unique_ptr<Node>> nodes_;
    
    Node* addNode(std::string name);
    void addEdge(Node* from, Node* to);
    const std::vector<Node*> getRoots() const;
};
```

**Tests:**
- Node creation and ownership
- Edge addition (bidirectional links)
- Graph traversal
- Memory safety (no leaks)

**Acceptance Criteria:**
- ✅ Nodes owned by graph (unique_ptr)
- ✅ Edges are raw pointers (non-owning)
- ✅ Atomic counters for thread safety
- ✅ No memory leaks under Valgrind

---

#### 1.3 ThreadPool
**Priority:** CRITICAL  
**Complexity:** MEDIUM  
**Time:** 3-4 days

**Implementation:**
- Fixed-size worker pool (`std::thread::hardware_concurrency()`)
- Mutex-protected task queue
- Condition variable for worker signaling
- Graceful shutdown mechanism

**Deliverables:**
```cpp
class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_flag_;
    
    void enqueue(std::function<void()> task);
    ~ThreadPool();  // Join all workers
};
```

**Tests:**
- Submit 100 tasks, verify all complete
- Concurrent task execution
- Graceful shutdown (join all threads)
- Exception handling in tasks

**Acceptance Criteria:**
- ✅ Workers correctly drain task queue
- ✅ No deadlocks on shutdown
- ✅ Exceptions in tasks don't crash pool
- ✅ Thread count matches `hardware_concurrency()`

---

#### 1.4 Process Execution (PAL)
**Priority:** CRITICAL  
**Complexity:** HIGH  
**Time:** 4-5 days

**Implementation:**
- POSIX implementation (fork/exec, pipe)
- Windows implementation (CreateProcess, HANDLE)
- Threaded stream draining (prevent pipe deadlock)
- Timeout and signal handling

**Deliverables:**
```cpp
struct ExecOptions {
    std::optional<fs::path> working_dir;
    std::optional<unsigned> timeout_seconds;
    std::map<std::string, std::string> env_vars;
};

struct ExecResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
    bool success() const { return exit_code == 0; }
};

ExecResult execute(
    const std::string& binary,
    const std::vector<std::string>& args,
    const ExecOptions& opts = {}
);
```

**Tests:**
- Execute simple commands (`echo`, `cat`)
- Capture stdout/stderr separately
- Verify exit code propagation
- Prevent pipe deadlock (large output)
- Timeout handling (kill hung process)

**Acceptance Criteria:**
- ✅ Correctly captures output on both platforms
- ✅ No pipe deadlocks with 1MB+ output
- ✅ Exit codes correctly returned
- ✅ Timeout kills process gracefully

---

### Phase 1 Integration Test
**End-to-end scenario:**
```cpp
// Use ThreadPool to execute processes in parallel
ThreadPool pool(4);
std::vector<ExecResult> results(4);

for (int i = 0; i < 4; i++) {
    pool.enqueue([&, i]() {
        results[i] = execute("echo", {"test"});
    });
}

// Wait for completion (ThreadPool destructor joins)
pool.~ThreadPool();

// Verify all succeeded
for (auto& r : results) {
    assert(r.success() && r.stdout_output == "test\n");
}
```

---

## 📖 Phase 2: Parsing & Configuration (Week 2-3)

**Goal:** Parse `build.aria` files and resolve variables.

### Components

#### 2.1 ConfigParser
**Priority:** CRITICAL  
**Complexity:** HIGH  
**Time:** 5-6 days

**Implementation:**
- Reuse `aria::frontend::Lexer` via adapter
- Recursive descent parser with panic mode
- AST construction (BuildFileNode, ProjectNode, VariablesNode, TargetsNode)
- Error reporting with line/column precision

**Deliverables:**
```cpp
class ConfigParser {
    aria::frontend::Lexer lexer_;
    Token current_token_;
    bool panic_mode_;
    std::vector<ParseError> errors_;
    
    std::unique_ptr<BuildFileNode> parse(const std::string& source);
    
private:
    void parseProjectSection(BuildFileNode* root);
    void parseVariablesSection(BuildFileNode* root);
    void parseTargetsSection(BuildFileNode* root);
    
    std::unique_ptr<ObjectNode> parseObject();
    std::unique_ptr<ArrayNode> parseArray();
    std::unique_ptr<ValueNode> parseValue();
};
```

**Tests:**
- Parse valid ABC files
- Error recovery (missing colons, unclosed braces)
- Unquoted keys
- Trailing commas
- Comments

**Acceptance Criteria:**
- ✅ Parses valid JSON (strict subset)
- ✅ Parses ABC extensions (unquoted keys, comments)
- ✅ Reports precise error locations (line:column)
- ✅ Panic mode recovers gracefully

---

#### 2.2 Variable Interpolator
**Priority:** CRITICAL  
**Complexity:** MEDIUM  
**Time:** 3-4 days

**Implementation:**
- Tokenize `&{variable}` syntax
- Tri-color DFS for cycle detection
- Environment variable support (`&{ENV.PATH}`)
- Memoization cache for performance

**Deliverables:**
```cpp
class Interpolator {
    using VariableMap = std::map<std::string, std::string>;
    
    const VariableMap& variables_;
    std::unordered_set<std::string> recursion_set_;  // Gray
    std::unordered_map<std::string, std::string> cache_;  // Black
    
    std::string resolve(const std::string& input);
    
private:
    std::string resolveVariable(const std::string& key);
    std::string getEnvVar(const std::string& name);
};
```

**Tests:**
- Simple substitution: `&{var}` → value
- Nested: `&{a}` → `&{b}` → final value
- Cycle detection: `&{a}` → `&{b}` → `&{a}` (error!)
- Environment vars: `&{ENV.HOME}`
- Missing vars (error handling)

**Acceptance Criteria:**
- ✅ Resolves nested variables correctly
- ✅ Detects cycles with full path
- ✅ Caches resolved values (performance)
- ✅ Environment vars work cross-platform

---

### Phase 2 Integration Test
**End-to-end scenario:**
```javascript
// build.aria
variables: {
    root: "/home/user",
    src: "&{root}/src",
    output: "&{root}/build"
}

targets: [
    {
        name: "app",
        sources: "&{src}/**/*.aria",
        output: "&{output}/app"
    }
]
```

Parse → Interpolate → Verify:
- `sources` resolves to `/home/user/src/**/*.aria`
- `output` resolves to `/home/user/build/app`

---

## 🌲 Phase 3: File Discovery (Week 3-4)

**Goal:** Expand glob patterns to sorted file lists.

### Components

#### 3.1 GlobEngine
**Priority:** HIGH  
**Complexity:** HIGH  
**Time:** 6-7 days

**Implementation:**
- Pattern parser (segment splitting)
- Anchor-point optimization (deepest static path)
- FastMatcher (Shifting Wildcard algorithm)
- Recursive traversal with exclusion pruning
- Deterministic sorting

**Deliverables:**
```cpp
class GlobEngine {
    std::vector<std::string> excludes_;
    std::unordered_map<size_t, std::vector<fs::path>> cache_;
    
    std::vector<fs::path> expand(const std::string& pattern);
    
private:
    fs::path findAnchor(const std::string& pattern);
    bool matchSegment(std::string_view filename, std::string_view pattern);
    bool isExcluded(const fs::path& path);
};
```

**Tests:**
- Literal paths: `src/main.aria`
- Wildcards: `src/*.aria`
- Globstar: `src/**/*.aria`
- Character classes: `test_[0-9].aria`
- Exclusions: Skip `node_modules/`, `.git/`
- Sorting: Results are lexicographically ordered

**Acceptance Criteria:**
- ✅ Matches all supported glob patterns
- ✅ Exclusions prune entire directory trees
- ✅ Results are deterministic (sorted)
- ✅ Performance: 10k files in < 50ms

---

### Phase 3 Integration Test
**End-to-end scenario:**

```bash
# Create test directory
mkdir -p test/{src,build,node_modules}
touch test/src/{main,utils,test}.aria
touch test/node_modules/lib.aria
```

```cpp
GlobEngine glob;
glob.addExclusion("**/node_modules");

auto files = glob.expand("test/**/*.aria");
// Expected: [test/src/main.aria, test/src/test.aria, test/src/utils.aria]
// NOT included: test/node_modules/lib.aria
```

---

## ✅ Phase 4: Graph Validation (Week 4)

**Goal:** Validate dependency graph structure.

### Components

#### 4.1 CycleDetector
**Priority:** CRITICAL  
**Complexity:** MEDIUM  
**Time:** 3-4 days

**Implementation:**
- Tri-color DFS traversal
- Path reconstruction (stack tracking)
- Distinguish diamonds from cycles

**Deliverables:**
```cpp
class CycleDetector {
    const DependencyGraph& graph_;
    
    std::optional<std::vector<std::string>> detectCycle();
    
private:
    enum Color { WHITE, GRAY, BLACK };
    std::unordered_map<Node*, Color> marks_;
    std::vector<Node*> path_stack_;
    
    bool dfs(Node* node);
    std::vector<std::string> reconstructPath(Node* cycle_start);
};
```

**Tests:**
- Valid DAG with diamond (no cycle)
- Simple cycle: A → B → A
- Complex cycle: A → B → C → D → B
- Self-loop: A → A
- Path reconstruction accuracy

**Acceptance Criteria:**
- ✅ Detects all cycles
- ✅ Allows diamond dependencies
- ✅ Returns full cycle path
- ✅ O(V + E) complexity

---

### Phase 4 Integration Test
**End-to-end scenario:**

```cpp
DependencyGraph graph;
auto a = graph.addNode("A");
auto b = graph.addNode("B");
auto c = graph.addNode("C");

graph.addEdge(a, b);
graph.addEdge(b, c);
graph.addEdge(c, a);  // Cycle!

CycleDetector detector(graph);
auto cycle = detector.detectCycle();

assert(cycle.has_value());
assert(cycle.value() == std::vector<std::string>{"A", "B", "C", "A"});
```

---

## ⚙️ Phase 5: Command Synthesis (Week 5)

**Goal:** Translate targets into compiler/linker commands.

### Components

#### 5.1 ToolchainOrchestrator
**Priority:** HIGH  
**Complexity:** MEDIUM  
**Time:** 4-5 days

**Implementation:**
- Compiler command construction (`ariac` flags)
- Linker command construction (ELF/COFF/Mach-O)
- Include path resolution from dependencies
- Platform detection (POSIX vs Windows)

**Deliverables:**
```cpp
class ToolchainOrchestrator {
    fs::path compiler_bin_ = "ariac";
    fs::path linker_bin_ = "lld";
    
    std::pair<std::string, std::vector<std::string>>
    constructCompileCommand(
        Node* node,
        const fs::path& source,
        const fs::path& output
    );
    
    std::pair<std::string, std::vector<std::string>>
    constructLinkCommand(
        Node* node,
        const std::vector<fs::path>& objects
    );
    
private:
    std::vector<std::string> buildIncludePaths(Node* node);
};
```

**Tests:**
- Generate compile command with flags
- Generate link command (executable vs library)
- Include paths from dependencies
- Platform-specific linker syntax

**Acceptance Criteria:**
- ✅ Commands are correctly escaped
- ✅ Include paths correctly resolved
- ✅ Platform-specific syntax (Linux/Windows/macOS)

---

## 📊 Phase 6: Incremental Builds (Week 5-6)

**Goal:** Detect which nodes need rebuilding.

### Components

#### 6.1 Timestamp Checker
**Priority:** HIGH  
**Complexity:** MEDIUM  
**Time:** 2-3 days

**Implementation:**
- Compare source vs output mtimes
- Dependency propagation (if dep rebuilt, dependent is dirty)
- Handle missing outputs

**Deliverables:**
```cpp
class IncrementalAnalyzer {
    bool isDirty(Node* node);
    
private:
    bool outputMissing(Node* node);
    bool sourceNewer(Node* node);
    bool dependencyNewer(Node* node);
};
```

**Tests:**
- Missing output → dirty
- Source newer than output → dirty
- Dependency rebuilt → dirty
- Everything up-to-date → not dirty

**Acceptance Criteria:**
- ✅ Correctly propagates dirty flag
- ✅ Handles missing files gracefully

---

#### 6.2 Command Signature Hasher
**Priority:** MEDIUM  
**Complexity:** LOW  
**Time:** 2-3 days

**Implementation:**
- FNV-1a hash of command string
- BuildState JSON persistence
- Comparison with previous build

**Deliverables:**
```cpp
class BuildState {
    std::unordered_map<std::string, uint64_t> command_hashes_;
    
    void load(const fs::path& state_file);
    void save(const fs::path& state_file);
    
    bool commandChanged(const std::string& target, const std::string& command);
    void updateCommand(const std::string& target, const std::string& command);
    
private:
    uint64_t hashCommand(const std::string& command);
};
```

**Tests:**
- Hash stability (same input → same hash)
- Detect flag changes (`-O0` → `-O3`)
- Persist and reload state

**Acceptance Criteria:**
- ✅ Detects command changes
- ✅ Persists to JSON correctly
- ✅ No false positives

---

## 🏗️ Phase 7: Build Orchestration (Week 6-7)

**Goal:** Execute parallel builds using Kahn's algorithm.

### Components

#### 7.1 BuildScheduler
**Priority:** CRITICAL  
**Complexity:** HIGH  
**Time:** 7-8 days

**Implementation:**
- Kahn's algorithm for topological scheduling
- Integration with ThreadPool
- Atomic in-degree updates
- Error handling and propagation
- Progress reporting

**Deliverables:**
```cpp
class BuildScheduler {
    DependencyGraph& graph_;
    ThreadPool& pool_;
    ToolchainOrchestrator& toolchain_;
    
    struct BuildOptions {
        bool incremental = true;
        bool verbose = false;
        unsigned max_jobs = std::thread::hardware_concurrency();
    };
    
    bool build(const BuildOptions& opts);
    
private:
    void executeNode(Node* node);
    void onTaskComplete(Node* node, const ExecResult& result);
    void propagateFailure(Node* node);
};
```

**Tests:**
- Simple linear chain: A → B → C
- Diamond dependency
- Parallel independent targets
- Error propagation (one task fails)
- Incremental build (skip up-to-date)

**Acceptance Criteria:**
- ✅ Correctly schedules parallel tasks
- ✅ Respects dependency order
- ✅ Stops on first error (default)
- ✅ Updates in-degree atomically

---

### Phase 7 Integration Test
**End-to-end scenario:**

```javascript
// build.aria
targets: [
    {name: "libA", sources: "a.aria", output: "a.o"},
    {name: "libB", sources: "b.aria", output: "b.o"},
    {name: "app", sources: "main.aria", output: "app", dependencies: ["libA", "libB"]}
]
```

**Expected behavior:**
1. `libA` and `libB` compile in parallel
2. `app` waits for both to finish
3. `app` compiles with `-I` flags from dependencies
4. Final link produces executable

---

## 🔧 Phase 8: Tooling Integration (Week 7-8)

**Goal:** Generate metadata for LSP and dependency tracking.

### Components

#### 8.1 CompileDBWriter
**Priority:** MEDIUM  
**Complexity:** LOW  
**Time:** 2 days

**Implementation:**
- Generate `compile_commands.json`
- Proper JSON escaping (shell + JSON)
- One entry per source file

**Deliverables:**
```cpp
class CompileDBWriter {
    bool generate(const fs::path& output_path, const DependencyGraph& graph);
    
private:
    std::string escapeJson(const std::string& str);
    std::string escapeShell(const std::string& str);
};
```

**Tests:**
- Valid JSON output
- Correct schema (directory, file, command)
- Escaped paths with spaces

**Acceptance Criteria:**
- ✅ Valid JSON schema
- ✅ Works with LSP (clangd, etc.)

---

#### 8.2 DependencyPrinter
**Priority:** LOW  
**Complexity:** MEDIUM  
**Time:** 3 days

**Implementation:**
- Generate Makefile-compatible `.d` files
- Scan AST for `use` statements
- Scan AST for `embed_file()` calls
- Phony targets for robustness

**Deliverables:**
```cpp
class DependencyPrinter {
    std::set<std::string> dependencies_;  // Auto-sorted
    std::string target_name_;
    bool emit_phony_;
    
    void addDependency(const std::string& path);
    void print(std::ostream& out);
};
```

**Tests:**
- Generate `.d` file
- Parse with Make
- Handle spaces in filenames

**Acceptance Criteria:**
- ✅ Valid Makefile syntax
- ✅ Phony targets prevent deletion errors

---

## �️ Phase 9: Clean Target Lifecycle (Week 8)

**Source:** aria_02.txt - Architectural Specification for Clean Target Lifecycle  
**Goal:** Implement precise, surgical artifact removal with bidirectional lifecycle management.  
**Priority:** MEDIUM-HIGH - Essential for hermetic builds and preventing directory pollution

### The Problem: Build Entropy

Traditional build systems treat "clean" as an afterthought - typically `rm -rf build`. This causes:
- **Directory Pollution:** Stale artifacts from renamed files accumulate
- **Phantom State:** Build system believes file exists (cached hash) but file deleted
- **Non-Hermetic Builds:** Incremental != fresh build (orphaned .o files linked)
- **Hard-to-Debug Failures:** Undefined references from stale object files

### The Solution: Lifecycle-Aware Artifact Management

Transform aria_make from a purely constructive DAG to a **bidirectional lifecycle engine** that can precisely create AND destroy artifacts.

---

### Components

#### 9.1 CleanTargetRegistry
**Priority:** HIGH  
**Complexity:** MEDIUM  
**Time:** 2-3 days

**Purpose:** Track all artifacts for precise removal without glob wildcards.

**Implementation:**
```cpp
class CleanTargetRegistry {
public:
    // Register an artifact (output file or directory)
    void registerArtifact(
        const std::string& target_name,
        const fs::path& artifact_path,
        ArtifactType type  // FILE or DIRECTORY
    );
    
    // Get all artifacts for a target
    std::vector<fs::path> getArtifacts(const std::string& target_name) const;
    
    // Get all artifacts in build (for 'clean all')
    std::vector<fs::path> getAllArtifacts() const;
    
    // Persist to .aria_artifacts.json
    void save(const fs::path& state_file);
    void load(const fs::path& state_file);
    
private:
    // Map: target_name → list of artifact paths
    std::unordered_map<std::string, std::vector<fs::path>> artifacts_;
};
```

**Usage:**
```cpp
// During build, register outputs
registry.registerArtifact("app", "build/app", ArtifactType::FILE);
registry.registerArtifact("app", "build/app.o", ArtifactType::FILE);
registry.registerArtifact("utils", "build/libutils.a", ArtifactType::FILE);

// At end of build
registry.save(".aria_artifacts.json");
```

**Artifacts File Format:**
```json
{
    "app": [
        {"path": "build/app", "type": "file"},
        {"path": "build/app.o", "type": "file"}
    ],
    "utils": [
        {"path": "build/libutils.a", "type": "file"}
    ]
}
```

---

#### 9.2 CleanOperation Executor
**Priority:** HIGH  
**Complexity:** MEDIUM  
**Time:** 2-3 days

**Purpose:** Execute removal operations with atomic state updates.

**Implementation:**
```cpp
class CleanOperation {
public:
    enum class Mode {
        TARGET,      // Clean specific target: aria_make clean app
        ALL,         // Clean all targets: aria_make clean
        STALE        // Clean orphaned artifacts only
    };
    
    CleanOperation(CleanTargetRegistry& registry, BuildState& state);
    
    // Execute clean operation
    bool execute(
        Mode mode,
        const std::string& target_name = ""  // Only for Mode::TARGET
    );
    
    // Dry run (show what would be deleted)
    void dryRun(Mode mode, const std::string& target_name = "");
    
private:
    CleanTargetRegistry& registry_;
    BuildState& state_;
    
    // Delete single artifact and update state
    bool deleteArtifact(const fs::path& path, bool update_state = true);
    
    // Find orphaned artifacts (exist on disk but not in current build graph)
    std::vector<fs::path> findOrphans();
};
```

**Atomic State Update:**
```cpp
bool CleanOperation::deleteArtifact(const fs::path& path, bool update_state) {
    try {
        // 1. Delete physical file
        if (fs::exists(path)) {
            fs::remove(path);
        }
        
        // 2. Remove from BuildState cache
        if (update_state) {
            state_.removeCommandHash(path.string());
        }
        
        // 3. Remove from artifact registry
        registry_.removeArtifact(path);
        
        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Failed to clean " << path << ": " << e.what() << "\n";
        return false;
    }
}
```

**Orphan Detection:**
```cpp
std::vector<fs::path> CleanOperation::findOrphans() {
    std::vector<fs::path> orphans;
    
    // Get all artifacts from registry (from previous build)
    auto registered_artifacts = registry_.getAllArtifacts();
    
    // Get all artifacts from current build graph
    auto current_artifacts = graph_.collectAllOutputs();
    
    // Find registered artifacts not in current graph
    for (const auto& artifact : registered_artifacts) {
        if (current_artifacts.find(artifact) == current_artifacts.end()) {
            if (fs::exists(artifact)) {
                orphans.push_back(artifact);
            }
        }
    }
    
    return orphans;
}
```

---

#### 9.3 CLI Integration
**Priority:** MEDIUM  
**Complexity:** LOW  
**Time:** 1-2 days

**Command Syntax:**
```bash
# Clean specific target
aria_make clean app

# Clean all targets
aria_make clean

# Clean only orphaned artifacts (safe)
aria_make clean --stale

# Dry run (show what would be deleted)
aria_make clean --dry-run

# Clean and rebuild (common workflow)
aria_make clean && aria_make build
```

**Implementation:**
```cpp
int main(int argc, char* argv[]) {
    // Parse command line
    if (argc >= 2 && std::string(argv[1]) == "clean") {
        CleanTargetRegistry registry;
        registry.load(".aria_artifacts.json");
        
        BuildState state;
        state.load(".aria_build_state.json");
        
        CleanOperation cleaner(registry, state);
        
        // Determine mode
        CleanOperation::Mode mode = CleanOperation::Mode::ALL;
        std::string target;
        bool dry_run = false;
        
        for (int i = 2; i < argc; ++i) {
            if (std::string(argv[i]) == "--stale") {
                mode = CleanOperation::Mode::STALE;
            } else if (std::string(argv[i]) == "--dry-run") {
                dry_run = true;
            } else {
                mode = CleanOperation::Mode::TARGET;
                target = argv[i];
            }
        }
        
        // Execute
        if (dry_run) {
            cleaner.dryRun(mode, target);
        } else {
            cleaner.execute(mode, target);
            
            // Persist state
            registry.save(".aria_artifacts.json");
            state.save(".aria_build_state.json");
        }
        
        return 0;
    }
    
    // ... normal build logic
}
```

---

### The "Tabula Rasa" Principle

**Goal:** Guarantee that incremental build == clean build (bit-for-bit identical output).

**Implementation Strategy:**
1. **Track Everything:** Registry captures ALL outputs (executables, libraries, .o files, .d files)
2. **Atomic Updates:** File deletion + state update happen together (no phantom state)
3. **Orphan Cleanup:** Automatically detect and warn about stale artifacts
4. **Verification:** Post-clean sanity check (no artifacts should exist)

```cpp
bool CleanOperation::verifyClean() {
    auto artifacts = registry_.getAllArtifacts();
    
    bool clean = true;
    for (const auto& artifact : artifacts) {
        if (fs::exists(artifact)) {
            std::cerr << "WARNING: Artifact still exists after clean: " 
                     << artifact << "\n";
            clean = false;
        }
    }
    
    return clean;
}
```

---

### Integration with BuildScheduler

**Auto-Register Artifacts During Build:**
```cpp
void BuildScheduler::executeNode(Node* node) {
    // ... normal build execution ...
    
    if (build_successful) {
        // Register all outputs
        registry_.registerArtifact(
            node->name,
            node->output_file,
            ArtifactType::FILE
        );
        
        // Register intermediate files (.o, .d)
        for (const auto& obj : node->object_files) {
            registry_.registerArtifact(
                node->name,
                obj,
                ArtifactType::FILE
            );
        }
    }
}
```

---

### Testing Strategy

```cpp
TEST(CleanTarget, RemovesSpecificTarget) {
    // Setup: Build two targets
    aria_make_build({"app", "utils"});
    
    ASSERT_TRUE(fs::exists("build/app"));
    ASSERT_TRUE(fs::exists("build/libutils.a"));
    
    // Clean only "app"
    aria_make_clean("app");
    
    ASSERT_FALSE(fs::exists("build/app"));
    ASSERT_TRUE(fs::exists("build/libutils.a"));  // Should NOT be deleted
}

TEST(CleanTarget, DetectsOrphans) {
    // Build original graph
    aria_make_build();
    
    // Modify build.aria (remove "old_target")
    modify_config_remove_target("old_target");
    
    // Clean stale
    auto orphans = clean_operation.findOrphans();
    
    ASSERT_EQ(orphans.size(), 1);
    ASSERT_EQ(orphans[0], "build/old_target");
}

TEST(CleanTarget, AtomicStateUpdate) {
    aria_make_build();
    
    // Verify artifact in state
    ASSERT_TRUE(build_state.hasCommandHash("build/app"));
    
    // Clean
    aria_make_clean();
    
    // Verify removed from state
    ASSERT_FALSE(build_state.hasCommandHash("build/app"));
}
```

---

### Success Criteria

- ✅ `aria_make clean` removes all artifacts
- ✅ `aria_make clean <target>` removes only target artifacts
- ✅ State file (.aria_build_state.json) updated atomically
- ✅ Orphan detection works (stale artifacts from config changes)
- ✅ No phantom state (hash exists but file missing)
- ✅ Hermetic guarantee: `clean && build` == `incremental build`

---

## 📅 Timeline Summary

| Phase | Duration | Components |
|-------|----------|-----------|
| Phase 1: Foundation | Week 1-2 | FileSystemTraits, DependencyGraph, ThreadPool, PAL |
| Phase 2: Parsing | Week 2-3 | ConfigParser, Interpolator |
| Phase 3: File Discovery | Week 3-4 | GlobEngine |
| Phase 4: Validation | Week 4 | CycleDetector |
| Phase 5: Command Synthesis | Week 5 | ToolchainOrchestrator |
| Phase 6: Incrementalism | Week 5-6 | Timestamp Checker, Command Hasher |
| Phase 7: Orchestration | Week 6-7 | BuildScheduler |
| Phase 8: Tooling | Week 7-8 | CompileDB, DependencyPrinter |
| Phase 9: Clean Lifecycle | Week 8 | CleanTargetRegistry, CleanOperation |

**Total:** ~8 weeks (single engineer) or ~7 weeks (3-engineer team)

---

## 🎯 Milestones

### Milestone 1: Infrastructure Complete (End of Week 2)
- ✅ All Phase 1 components functional
- ✅ Can execute processes in parallel
- ✅ Cross-platform compatibility verified

### Milestone 2: MVP Build System (End of Week 5)
- ✅ Phases 1-5 complete
- ✅ Can parse `build.aria`
- ✅ Can resolve dependencies
- ✅ Can execute single-threaded builds

### Milestone 3: Parallel & Incremental (End of Week 7)
- ✅ Phases 1-6 complete
- ✅ Parallel builds working
- ✅ Incremental builds functional
- ✅ Performance targets met

### Milestone 4: Production Ready (End of Week 8)
- ✅ All phases complete
- ✅ LSP integration working
- ✅ Comprehensive test suite
- ✅ Documentation complete

---

**Next:** Read component specifications ([10_PARSER_CONFIG.md](./10_PARSER_CONFIG.md), etc.)
