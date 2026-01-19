# Incremental Build System

This document specifies the incremental build logic that determines when targets need to be rebuilt based on timestamps, command changes, and dependency metadata.

---

## Overview

**Goal:** Skip rebuilding targets that are already up-to-date, dramatically reducing build times for iterative development.

**Rebuild Triggers:**
1. **Source modified:** Source file timestamp newer than output file
2. **Command changed:** Compiler/linker flags modified
3. **Dependency modified:** Any transitive dependency rebuilt
4. **Missing output:** Output file doesn't exist

### Performance Impact

| Scenario | Full Build | Incremental Build | Speedup |
|----------|-----------|------------------|---------|
| Clean build | 120s | N/A | 1x |
| 1 source file changed | 120s | 2s | **60x** |
| Flags changed | 120s | 15s | **8x** |
| No changes | 120s | 0.5s | **240x** |

---

## Node Dirty State

### Node Extensions

```cpp
class Node {
public:
    // ... (existing fields from 30_DEPENDENCY_GRAPH.md)
    
    // Incremental build state
    bool is_dirty() const { return dirty_; }
    void mark_dirty() { dirty_ = true; }
    void mark_clean() { dirty_ = false; }
    
    void set_command_hash(uint64_t hash) { command_hash_ = hash; }
    uint64_t get_command_hash() const { return command_hash_; }
    
    void set_last_built_time(std::filesystem::file_time_type time) {
        last_built_time_ = time;
    }
    std::filesystem::file_time_type get_last_built_time() const {
        return last_built_time_;
    }
    
private:
    bool dirty_ = true;  // Initially dirty (force first build)
    uint64_t command_hash_ = 0;
    std::filesystem::file_time_type last_built_time_;
};
```

---

## BuildState Persistence

### Purpose
Save metadata between builds to detect changes.

### File Format: `.aria_build_state`

```json
{
  "version": 1,
  "targets": {
    "src/main.aria": {
      "output": "build/main.o",
      "command_hash": 12345678901234567890,
      "last_built": 1704067200,
      "sources": {
        "src/main.aria": 1704067100,
        "include/utils.aria": 1704066800
      }
    },
    "bin/myapp": {
      "output": "bin/myapp",
      "command_hash": 98765432109876543210,
      "last_built": 1704067205,
      "dependencies": [
        "build/main.o",
        "build/utils.o"
      ]
    }
  }
}
```

### Class Structure

```cpp
struct TargetState {
    std::filesystem::path output_file;
    uint64_t command_hash;
    std::filesystem::file_time_type last_built_time;
    std::map<std::filesystem::path, std::filesystem::file_time_type> source_timestamps;
    std::vector<std::filesystem::path> dependencies;
};

class BuildState {
public:
    BuildState();
    
    // Load/save state
    bool load(const std::filesystem::path& path = ".aria_build_state");
    bool save(const std::filesystem::path& path = ".aria_build_state");
    
    // Query state
    std::optional<TargetState> getTargetState(const std::string& target_name) const;
    void setTargetState(const std::string& target_name, const TargetState& state);
    
    // Dirty checking
    bool isTargetDirty(Node* node) const;
    
    // Update after build
    void updateTargetState(Node* node);
    
    // Clear all state (clean build)
    void clear();
    
private:
    std::map<std::string, TargetState> targets_;
    
    // JSON serialization
    nlohmann::json toJSON() const;
    void fromJSON(const nlohmann::json& json);
};
```

### Implementation

```cpp
bool BuildState::load(const fs::path& path) {
    if (!fs::exists(path)) {
        return false;  // No previous state (first build)
    }
    
    try {
        std::ifstream file(path);
        nlohmann::json json;
        file >> json;
        fromJSON(json);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[WARNING] Failed to load build state: " << e.what() << "\n";
        std::cerr << "[INFO] Performing clean build\n";
        return false;
    }
}

bool BuildState::save(const fs::path& path) {
    try {
        nlohmann::json json = toJSON();
        
        std::ofstream file(path);
        file << json.dump(2);  // Pretty-print with 2-space indent
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[WARNING] Failed to save build state: " << e.what() << "\n";
        return false;
    }
}

void BuildState::fromJSON(const nlohmann::json& json) {
    targets_.clear();
    
    for (auto& [name, target_json] : json["targets"].items()) {
        TargetState state;
        
        state.output_file = target_json["output"].get<std::string>();
        state.command_hash = target_json["command_hash"].get<uint64_t>();
        
        // Parse timestamp (stored as Unix seconds)
        auto epoch_time = target_json["last_built"].get<int64_t>();
        state.last_built_time = fs::file_time_type(
            std::chrono::seconds(epoch_time)
        );
        
        // Load source timestamps
        if (target_json.contains("sources")) {
            for (auto& [src_path, timestamp] : target_json["sources"].items()) {
                auto src_time = fs::file_time_type(
                    std::chrono::seconds(timestamp.get<int64_t>())
                );
                state.source_timestamps[src_path] = src_time;
            }
        }
        
        // Load dependencies
        if (target_json.contains("dependencies")) {
            for (const auto& dep : target_json["dependencies"]) {
                state.dependencies.push_back(dep.get<std::string>());
            }
        }
        
        targets_[name] = state;
    }
}

nlohmann::json BuildState::toJSON() const {
    nlohmann::json json;
    json["version"] = 1;
    
    for (const auto& [name, state] : targets_) {
        nlohmann::json target_json;
        
        target_json["output"] = state.output_file.string();
        target_json["command_hash"] = state.command_hash;
        
        // Convert file_time_type to Unix seconds
        auto epoch = state.last_built_time.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
        target_json["last_built"] = seconds;
        
        // Save source timestamps
        nlohmann::json sources_json;
        for (const auto& [src, timestamp] : state.source_timestamps) {
            auto src_epoch = timestamp.time_since_epoch();
            auto src_seconds = std::chrono::duration_cast<std::chrono::seconds>(src_epoch).count();
            sources_json[src.string()] = src_seconds;
        }
        target_json["sources"] = sources_json;
        
        // Save dependencies
        target_json["dependencies"] = state.dependencies;
        
        json["targets"][name] = target_json;
    }
    
    return json;
}
```

---

## Command Signature Hashing

### Purpose
Detect when compiler/linker flags change, requiring rebuild even if sources unchanged.

### Algorithm: FNV-1a

```cpp
class CommandHasher {
public:
    static uint64_t hash(const std::string& binary,
                         const std::vector<std::string>& args) {
        uint64_t hash = FNV_OFFSET_BASIS;
        
        // Hash binary name
        for (char c : binary) {
            hash ^= static_cast<uint64_t>(c);
            hash *= FNV_PRIME;
        }
        
        // Hash each argument
        for (const auto& arg : args) {
            // Separator between arguments
            hash ^= 0xFF;
            hash *= FNV_PRIME;
            
            for (char c : arg) {
                hash ^= static_cast<uint64_t>(c);
                hash *= FNV_PRIME;
            }
        }
        
        return hash;
    }
    
private:
    static constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    static constexpr uint64_t FNV_PRIME = 1099511628211ULL;
};
```

### Why FNV-1a?

| Hash Function | Speed | Collision Rate | Implementation |
|---------------|-------|----------------|----------------|
| FNV-1a | **Very fast** | Low | 5 lines |
| SHA-256 | Slow | Very low | Complex |
| std::hash | Fast | **Platform-dependent** | Unreliable |

**Decision:** FNV-1a provides excellent speed/quality tradeoff for command hashing.

### Example

```cpp
// First build
auto [binary, args] = toolchain.buildCompileCommand(node, "src/main.aria", "build/main.o");
// args = ["src/main.aria", "-o", "build/main.o", "-O2"]

uint64_t hash1 = CommandHasher::hash(binary, args);
// hash1 = 12345678901234567890

node->set_command_hash(hash1);

// ... later, user modifies build.aria to add -g flag ...

auto [binary2, args2] = toolchain.buildCompileCommand(node, "src/main.aria", "build/main.o");
// args2 = ["src/main.aria", "-o", "build/main.o", "-O2", "-g"]

uint64_t hash2 = CommandHasher::hash(binary2, args2);
// hash2 = 98765432109876543210

if (hash2 != hash1) {
    // Command changed → force rebuild
    node->mark_dirty();
}
```

---

## Dirty Detection Algorithm

### Full Logic

```cpp
bool BuildState::isTargetDirty(Node* node) const {
    // 1. Missing output → always dirty
    if (!fs::exists(node->output_file)) {
        return true;
    }
    
    // 2. No previous state → dirty (first build)
    auto opt_state = getTargetState(node->name());
    if (!opt_state) {
        return true;
    }
    
    const TargetState& prev_state = *opt_state;
    
    // 3. Command changed → dirty
    if (node->get_command_hash() != prev_state.command_hash) {
        return true;
    }
    
    // 4. Source files modified → dirty
    for (const auto& src_path : node->source_files) {
        // Get current timestamp
        auto current_time = fs::last_write_time(src_path);
        
        // Check against previous build
        auto it = prev_state.source_timestamps.find(src_path);
        if (it == prev_state.source_timestamps.end()) {
            // New source file → dirty
            return true;
        }
        
        if (current_time > it->second) {
            // Source modified → dirty
            return true;
        }
    }
    
    // 5. Dependency rebuilt → dirty (transitive)
    for (Node* dep : node->dependencies()) {
        if (dep->is_dirty()) {
            return true;
        }
        
        // Check dependency's output timestamp
        if (fs::exists(dep->output_file)) {
            auto dep_time = fs::last_write_time(dep->output_file);
            if (dep_time > prev_state.last_built_time) {
                return true;
            }
        }
    }
    
    // 6. All checks passed → clean
    return false;
}

void BuildState::updateTargetState(Node* node) {
    TargetState state;
    
    state.output_file = node->output_file;
    state.command_hash = node->get_command_hash();
    state.last_built_time = fs::last_write_time(node->output_file);
    
    // Record source timestamps
    for (const auto& src_path : node->source_files) {
        if (fs::exists(src_path)) {
            state.source_timestamps[src_path] = fs::last_write_time(src_path);
        }
    }
    
    // Record dependencies
    for (Node* dep : node->dependencies()) {
        state.dependencies.push_back(dep->output_file);
    }
    
    targets_[node->name()] = state;
}
```

### Dirty Marking During Graph Construction

```cpp
void DependencyGraph::markDirtyNodes(const BuildState& build_state) {
    // Traverse in topological order (dependencies before dependents)
    std::vector<Node*> sorted = topologicalSort();
    
    for (Node* node : sorted) {
        // Check if dirty
        bool dirty = build_state.isTargetDirty(node);
        
        if (dirty) {
            node->mark_dirty();
        } else {
            node->mark_clean();
        }
    }
}
```

---

## Timestamp Handling

### Platform Differences

| Platform | Precision | Clock Type |
|----------|-----------|-----------|
| Linux | Nanoseconds | `std::filesystem::file_time_type` |
| macOS | Nanoseconds | `std::filesystem::file_time_type` |
| Windows | 100ns intervals | `std::filesystem::file_time_type` |

### Safe Comparison

```cpp
bool isFileNewer(const fs::path& file1, const fs::path& file2) {
    if (!fs::exists(file1) || !fs::exists(file2)) {
        return false;
    }
    
    auto time1 = fs::last_write_time(file1);
    auto time2 = fs::last_write_time(file2);
    
    // Use > (not >=) to handle same-second modifications
    return time1 > time2;
}
```

### Critical Gotcha: Filesystem Resolution

```bash
# Scenario: Rapid edits within 1 second
$ echo "v1" > test.txt
$ ariac test.aria -o test.o   # Compiles
$ echo "v2" > test.txt         # Modify immediately
$ ariac test.aria -o test.o   # Should recompile

# Problem: If filesystem has 1-second resolution and both writes
# happen at timestamp T, the check sees:
#   test.txt timestamp == test.o timestamp
#   → NOT dirty (WRONG!)

# Solution: Use > comparison (not >=)
#   test.txt timestamp > test.o timestamp
#   → dirty (CORRECT)
```

**Defensive Strategy:**
```cpp
// After successful build, touch output file to update timestamp
fs::last_write_time(output_file, fs::file_time_type::clock::now());
```

---

## Dependency File Parsing (.d files)

### Purpose
Compiler-generated dependency files track implicit dependencies (e.g., `#import` statements in Aria).

### Format (Make-style)

```makefile
build/main.o: src/main.aria \
  include/utils.aria \
  include/math.aria \
  /usr/local/include/aria/std/vector.aria
```

### Parser

```cpp
class DependencyFileParser {
public:
    struct ParseResult {
        std::filesystem::path target;
        std::vector<std::filesystem::path> dependencies;
    };
    
    static std::optional<ParseResult> parse(const std::filesystem::path& dep_file) {
        if (!fs::exists(dep_file)) {
            return std::nullopt;
        }
        
        std::ifstream file(dep_file);
        std::string line;
        std::ostringstream content;
        
        // Read entire file (handle line continuations with \)
        while (std::getline(file, line)) {
            // Remove trailing backslash and newline
            if (!line.empty() && line.back() == '\\') {
                line.pop_back();
            }
            content << line;
        }
        
        std::string full_content = content.str();
        
        // Find colon separator
        size_t colon_pos = full_content.find(':');
        if (colon_pos == std::string::npos) {
            return std::nullopt;
        }
        
        ParseResult result;
        
        // Extract target (before colon)
        std::string target_str = full_content.substr(0, colon_pos);
        result.target = fs::path(trim(target_str));
        
        // Extract dependencies (after colon)
        std::string deps_str = full_content.substr(colon_pos + 1);
        std::istringstream deps_stream(deps_str);
        std::string dep;
        
        while (deps_stream >> dep) {
            result.dependencies.push_back(fs::path(dep));
        }
        
        return result;
    }
    
private:
    static std::string trim(const std::string& str) {
        auto start = str.find_first_not_of(" \t\r\n");
        auto end = str.find_last_not_of(" \t\r\n");
        
        if (start == std::string::npos) {
            return "";
        }
        
        return str.substr(start, end - start + 1);
    }
};
```

### Integration with Dirty Detection

```cpp
void Node::loadImplicitDependencies(const fs::path& dep_file) {
    auto opt_result = DependencyFileParser::parse(dep_file);
    
    if (!opt_result) {
        return;  // No .d file yet (first build)
    }
    
    const auto& result = *opt_result;
    
    // Add implicit dependencies to node
    for (const auto& dep_path : result.dependencies) {
        if (dep_path != result.target) {  // Skip self-reference
            implicit_dependencies_.push_back(dep_path);
        }
    }
}

bool BuildState::isTargetDirty(Node* node) const {
    // ... (existing checks)
    
    // 7. Implicit dependency modified (from .d file)
    for (const auto& implicit_dep : node->implicit_dependencies_) {
        if (!fs::exists(implicit_dep)) {
            // Dependency deleted → rebuild
            return true;
        }
        
        auto dep_time = fs::last_write_time(implicit_dep);
        if (dep_time > prev_state.last_built_time) {
            // Implicit dependency modified → rebuild
            return true;
        }
    }
    
    return false;
}
```

---

## Full Build Flow with Incrementalism

### 1. Load Build State

```cpp
BuildState build_state;
build_state.load(".aria_build_state");
```

### 2. Parse Config & Construct Graph

```cpp
ConfigParser parser;
auto ast = parser.parse("build.aria");

DependencyGraph graph;
graph.buildFromAST(ast);
```

### 3. Compute Command Hashes

```cpp
ToolchainOrchestrator toolchain;

for (Node* node : graph.getAllNodes()) {
    auto [binary, args] = toolchain.buildCompileCommand(
        node, node->source_files[0], node->output_file
    );
    
    uint64_t hash = CommandHasher::hash(binary, args);
    node->set_command_hash(hash);
}
```

### 4. Load Implicit Dependencies

```cpp
for (Node* node : graph.getAllNodes()) {
    fs::path dep_file = fs::path(node->output_file).replace_extension(".d");
    node->loadImplicitDependencies(dep_file);
}
```

### 5. Mark Dirty Nodes

```cpp
graph.markDirtyNodes(build_state);
```

### 6. Execute Build

```cpp
ThreadPool pool;
BuildScheduler scheduler(graph, pool, toolchain);

bool success = scheduler.build();
```

### 7. Update Build State

```cpp
if (success) {
    for (Node* node : graph.getAllNodes()) {
        build_state.updateTargetState(node);
    }
    
    build_state.save(".aria_build_state");
}
```

---

## Optimization: Skip Clean Subgraphs

### Problem
If a library is clean, all its dependents are clean (unless command changed).

### Solution: Prune Search

```cpp
void DependencyGraph::markDirtyNodes(const BuildState& build_state) {
    std::vector<Node*> sorted = topologicalSort();
    
    for (Node* node : sorted) {
        bool locally_dirty = build_state.isTargetDirty(node);
        
        if (locally_dirty) {
            node->mark_dirty();
            
            // Propagate dirtiness to ALL dependents
            propagateDirtyness(node);
        } else {
            node->mark_clean();
        }
    }
}

void DependencyGraph::propagateDirtyness(Node* dirty_node) {
    for (Node* dependent : dirty_node->dependents()) {
        if (!dependent->is_dirty()) {
            dependent->mark_dirty();
            propagateDirtyness(dependent);  // Recursive
        }
    }
}
```

### Performance Gain

```
Scenario: 100 targets, 1 leaf library changed

Without propagation:
  - Check all 100 targets individually
  - 100 × timestamp checks = ~10ms

With propagation:
  - Check 1 leaf target (dirty)
  - Mark 50 dependents dirty (instant)
  - Skip checking 50 clean targets
  - 50 × timestamp checks = ~5ms
```

---

**Next:** [60_TOOLING_INTEGRATION.md](./60_TOOLING_INTEGRATION.md) - IDE integration (compile_commands.json, language servers)
