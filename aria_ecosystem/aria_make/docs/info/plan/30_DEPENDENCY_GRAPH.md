# Dependency Graph Components

This document provides detailed specifications for the dependency graph construction, validation, and cycle detection subsystems.

---

## DependencyGraph - Build Target DAG

### Purpose
Represent build targets and their dependencies as a Directed Acyclic Graph (DAG), enabling topological scheduling and incremental builds.

### Core Concepts

**Node:** A build target (library, executable, object file)  
**Edge:** Dependency relationship (A depends on B → edge from A to B)  
**In-Degree:** Count of dependencies (edges pointing TO this node)  
**Out-Degree:** Count of dependents (edges pointing FROM this node)

### Node Class

```cpp
class Node {
public:
    enum class Status {
        NOT_STARTED,
        PENDING,      // In ready queue
        BUILDING,     // Worker thread executing
        COMPLETED,    // Successfully built
        FAILED        // Build error
    };
    
    enum class TargetType {
        EXECUTABLE,
        STATIC_LIBRARY,
        SHARED_LIBRARY,
        OBJECT_FILE,
        CUSTOM
    };
    
    explicit Node(std::string name);
    
    // Delete copy/move to maintain pointer stability
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;
    
    // Accessors
    const std::string& name() const { return name_; }
    TargetType type() const { return type_; }
    
    // Graph topology
    const std::vector<Node*>& dependencies() const { return dependencies_; }
    const std::vector<Node*>& dependents() const { return dependents_; }
    
    void add_dependency(Node* node);
    void add_dependent(Node* node);
    
    // Dynamic state (thread-safe)
    int get_in_degree() const { return in_degree_.load(); }
    void increment_in_degree() { in_degree_++; }
    int decrement_in_degree() { return --in_degree_; }
    void reset_in_degree(int value) { in_degree_ = value; }
    
    Status get_status() const { return status_.load(); }
    void set_status(Status s) { status_ = s; }
    
    // Incremental build
    bool is_dirty() const { return is_dirty_; }
    void set_dirty(bool dirty) { is_dirty_ = dirty; }
    
    // Target configuration
    std::filesystem::path output_file;
    std::vector<std::filesystem::path> source_files;
    std::vector<std::string> compile_flags;
    std::vector<std::string> link_flags;
    std::string command;  // Full command string (for hashing)
    
    // Metadata
    std::filesystem::file_time_type output_timestamp;
    
private:
    std::string name_;
    TargetType type_;
    
    // Graph structure (raw pointers = non-owning)
    std::vector<Node*> dependencies_;   // Nodes this depends ON
    std::vector<Node*> dependents_;     // Nodes that depend on THIS
    
    // Dynamic state (atomic for thread safety)
    std::atomic<int> in_degree_;
    std::atomic<Status> status_;
    
    bool is_dirty_;
};
```

**Why atomic counters?**
In parallel builds, multiple threads may decrement in-degree simultaneously:

```cpp
// Thread 1: LibA completes
app_node->decrement_in_degree();  // 2 → 1

// Thread 2: LibB completes (simultaneously)
app_node->decrement_in_degree();  // 2 → 1 (DATA RACE without atomic!)

// Expected: 2 → 1 → 0
// Actual (without atomic): 2 → 1 (missed one decrement!)
```

**Solution:** `std::atomic<int>` provides lock-free thread-safe operations.

### DependencyGraph Class

```cpp
class DependencyGraph {
public:
    DependencyGraph() = default;
    
    // Node management
    Node* addNode(const std::string& name);
    Node* getNode(const std::string& name);
    bool hasNode(const std::string& name) const;
    
    // Edge management
    void addEdge(Node* from, Node* to);
    void addEdge(const std::string& from_name, const std::string& to_name);
    
    // Traversal
    std::vector<Node*> getRoots() const;  // Nodes with no dependencies
    std::vector<Node*> getLeaves() const; // Nodes with no dependents
    std::vector<Node*> getAllNodes() const;
    
    // Statistics
    size_t nodeCount() const { return nodes_.size(); }
    size_t edgeCount() const;
    
    // Validation
    bool hasEdge(Node* from, Node* to) const;
    
    // Reset dynamic state (for rebuilds)
    void resetState();
    
private:
    // Ownership: Graph owns all nodes
    std::unordered_map<std::string, std::unique_ptr<Node>> nodes_;
};
```

**Memory Model:**
- **Ownership:** `DependencyGraph` owns all `Node` instances via `unique_ptr`
- **References:** Edges are raw pointers (non-owning, graph-scoped)
- **Lifetime:** Nodes live as long as the graph exists
- **Safety:** No dangling pointers (graph outlives traversal)

### Construction

```cpp
Node* DependencyGraph::addNode(const std::string& name) {
    if (nodes_.count(name)) {
        return nodes_[name].get();
    }
    
    auto node = std::make_unique<Node>(name);
    Node* ptr = node.get();
    nodes_[name] = std::move(node);
    
    return ptr;
}

void DependencyGraph::addEdge(Node* from, Node* to) {
    // Validate
    if (!from || !to) {
        throw std::invalid_argument("Cannot add edge with null node");
    }
    
    // Prevent duplicate edges
    if (hasEdge(from, to)) {
        return;
    }
    
    // Add bidirectional links
    from->add_dependency(to);      // from depends ON to
    to->add_dependent(from);        // to is depended on BY from
    
    // Update in-degree (used by Kahn's algorithm)
    from->increment_in_degree();
}

bool DependencyGraph::hasEdge(Node* from, Node* to) const {
    const auto& deps = from->dependencies();
    return std::find(deps.begin(), deps.end(), to) != deps.end();
}
```

### Traversal Utilities

```cpp
std::vector<Node*> DependencyGraph::getRoots() const {
    std::vector<Node*> roots;
    
    for (const auto& [name, node] : nodes_) {
        if (node->dependencies().empty()) {
            roots.push_back(node.get());
        }
    }
    
    return roots;
}

std::vector<Node*> DependencyGraph::getAllNodes() const {
    std::vector<Node*> all_nodes;
    all_nodes.reserve(nodes_.size());
    
    for (const auto& [name, node] : nodes_) {
        all_nodes.push_back(node.get());
    }
    
    return all_nodes;
}

void DependencyGraph::resetState() {
    for (auto& [name, node] : nodes_) {
        node->set_status(Node::Status::NOT_STARTED);
        node->set_dirty(false);
        
        // Reset in-degree to actual dependency count
        node->reset_in_degree(node->dependencies().size());
    }
}
```

---

## CycleDetector - Graph Validation

### Purpose
Detect cycles in the dependency graph while allowing diamond dependencies.

### Key Insight: Tri-Color Marking

**Problem:** Naive "visited" flag can't distinguish diamonds from cycles.

**Diamond (VALID):**
```
    App
   /   \
LibA   LibB
   \   /
   Core
```

**Cycle (INVALID):**
```
A → B → C → A
```

**Solution:** Three colors
- **WHITE:** Unvisited
- **GRAY:** Currently in recursion stack (active path)
- **BLACK:** Fully explored (safe to revisit)

**Algorithm:**
- If we encounter a GRAY node → **CYCLE** (back edge)
- If we encounter a BLACK node → **SAFE** (cross/forward edge, diamond OK)

### Class Structure

```cpp
class CycleDetector {
public:
    explicit CycleDetector(const DependencyGraph& graph);
    
    // Returns empty optional if no cycle, otherwise cycle path
    std::optional<std::vector<std::string>> detectCycle();
    
    // Check specific node (for incremental checking)
    bool hasCycleFrom(Node* start);
    
private:
    enum Color { WHITE, GRAY, BLACK };
    
    const DependencyGraph& graph_;
    std::unordered_map<Node*, Color> colors_;
    std::vector<Node*> path_stack_;  // For cycle reconstruction
    
    bool dfs(Node* node);
    std::vector<std::string> reconstructCycle(Node* cycle_start);
};
```

### Detection Algorithm

```cpp
std::optional<std::vector<std::string>> CycleDetector::detectCycle() {
    // Initialize all nodes as WHITE
    for (Node* node : graph_.getAllNodes()) {
        colors_[node] = WHITE;
    }
    
    path_stack_.clear();
    
    // DFS from each unvisited node
    for (Node* node : graph_.getAllNodes()) {
        if (colors_[node] == WHITE) {
            if (dfs(node)) {
                // Cycle detected, path already reconstructed
                return reconstructCycle(path_stack_.back());
            }
        }
    }
    
    return std::nullopt;  // No cycle
}

bool CycleDetector::dfs(Node* u) {
    colors_[u] = GRAY;  // Mark as "in progress"
    path_stack_.push_back(u);
    
    for (Node* v : u->dependencies()) {
        if (colors_[v] == GRAY) {
            // Back edge detected → CYCLE!
            path_stack_.push_back(v);  // Complete the cycle
            return true;
        }
        
        if (colors_[v] == WHITE) {
            if (dfs(v)) {
                return true;  // Cycle found in subtree
            }
        }
        
        // BLACK: Safe (already fully explored)
    }
    
    colors_[u] = BLACK;  // Mark as "fully processed"
    path_stack_.pop_back();
    return false;
}
```

### Cycle Path Reconstruction

```cpp
std::vector<std::string> CycleDetector::reconstructCycle(Node* cycle_start) {
    std::vector<std::string> cycle_path;
    
    // Find where the cycle starts in the path stack
    auto it = std::find(path_stack_.begin(), path_stack_.end(), cycle_start);
    
    if (it == path_stack_.end()) {
        // Should never happen if algorithm is correct
        return {"[Error reconstructing cycle path]"};
    }
    
    // Extract the cycle portion
    for (; it != path_stack_.end(); ++it) {
        cycle_path.push_back((*it)->name());
    }
    
    return cycle_path;
}
```

### Example Execution

**Graph:**
```
A → B → C
A → D → C  (Diamond)
C → E → B  (Cycle!)
```

**DFS Trace:**
```
dfs(A):
  color[A] = GRAY, stack = [A]
  
  dfs(B):
    color[B] = GRAY, stack = [A, B]
    
    dfs(C):
      color[C] = GRAY, stack = [A, B, C]
      
      dfs(E):
        color[E] = GRAY, stack = [A, B, C, E]
        
        Check B:
          color[B] == GRAY  ← CYCLE DETECTED!
          stack = [A, B, C, E, B]
          
          Reconstruct from B:
          cycle = [B, C, E, B]
```

**Output:**
```
Error: Circular dependency detected: B → C → E → B
```

### Diamond vs Cycle Example

**Valid Diamond:**
```cpp
DependencyGraph graph;
auto app = graph.addNode("App");
auto libA = graph.addNode("LibA");
auto libB = graph.addNode("LibB");
auto core = graph.addNode("Core");

graph.addEdge(app, libA);
graph.addEdge(app, libB);
graph.addEdge(libA, core);
graph.addEdge(libB, core);

CycleDetector detector(graph);
auto cycle = detector.detectCycle();
assert(!cycle.has_value());  // ✅ No cycle (diamond is OK)
```

**Invalid Cycle:**
```cpp
DependencyGraph graph;
auto a = graph.addNode("A");
auto b = graph.addNode("B");
auto c = graph.addNode("C");

graph.addEdge(a, b);
graph.addEdge(b, c);
graph.addEdge(c, a);  // ← Creates cycle!

CycleDetector detector(graph);
auto cycle = detector.detectCycle();
assert(cycle.has_value());  // ❌ Cycle detected
assert(cycle.value() == std::vector<std::string>{"A", "B", "C", "A"});
```

### Integration with Build System

```cpp
void validateBuildGraph(DependencyGraph& graph) {
    CycleDetector detector(graph);
    auto cycle = detector.detectCycle();
    
    if (cycle.has_value()) {
        std::stringstream ss;
        ss << "Circular dependency detected: ";
        
        const auto& path = cycle.value();
        for (size_t i = 0; i < path.size(); ++i) {
            ss << path[i];
            if (i < path.size() - 1) {
                ss << " → ";
            }
        }
        
        throw BuildError(ss.str());
    }
}
```

---

## Graph Construction from AST

### Purpose
Translate parsed `build.aria` configuration into `DependencyGraph`.

### Algorithm

```cpp
DependencyGraph buildGraphFromAST(const BuildFileNode* ast) {
    DependencyGraph graph;
    
    // 1. Create all nodes first
    for (const auto& target_node : ast->targets->elements) {
        const auto* target_obj = dynamic_cast<const ObjectNode*>(target_node.get());
        if (!target_obj) continue;
        
        std::string name = getStringField(target_obj, "name");
        Node* node = graph.addNode(name);
        
        // Set node properties
        node->output_file = getStringField(target_obj, "output");
        
        // Parse sources (may be glob pattern)
        std::string sources_pattern = getStringField(target_obj, "sources");
        // (Will be expanded later by GlobEngine)
        
        // Parse type
        std::string type_str = getStringField(target_obj, "type");
        if (type_str == "executable") {
            node->type_ = Node::TargetType::EXECUTABLE;
        } else if (type_str == "library") {
            node->type_ = Node::TargetType::STATIC_LIBRARY;
        }
        // ... etc
        
        // Store flags
        if (hasField(target_obj, "flags")) {
            node->compile_flags = getStringArrayField(target_obj, "flags");
        }
    }
    
    // 2. Add edges (dependencies)
    for (const auto& target_node : ast->targets->elements) {
        const auto* target_obj = dynamic_cast<const ObjectNode*>(target_node.get());
        if (!target_obj) continue;
        
        std::string name = getStringField(target_obj, "name");
        Node* node = graph.getNode(name);
        
        if (hasField(target_obj, "dependencies")) {
            auto deps = getStringArrayField(target_obj, "dependencies");
            for (const auto& dep_name : deps) {
                Node* dep = graph.getNode(dep_name);
                if (!dep) {
                    throw BuildError("Unknown dependency: " + dep_name + 
                                   " (required by " + name + ")");
                }
                
                graph.addEdge(node, dep);
            }
        }
    }
    
    // 3. Validate (detect cycles)
    CycleDetector detector(graph);
    auto cycle = detector.detectCycle();
    if (cycle.has_value()) {
        // ... throw error with cycle path
    }
    
    return graph;
}
```

### Helper Functions

```cpp
std::string getStringField(const ObjectNode* obj, const std::string& key) {
    auto it = obj->members.find(key);
    if (it == obj->members.end()) {
        throw BuildError("Missing required field: " + key);
    }
    
    const auto* str_node = dynamic_cast<const StringNode*>(it->second.get());
    if (!str_node) {
        throw BuildError("Field '" + key + "' must be a string");
    }
    
    return str_node->value;
}

std::vector<std::string> getStringArrayField(const ObjectNode* obj, const std::string& key) {
    auto it = obj->members.find(key);
    if (it == obj->members.end()) {
        return {};  // Optional field
    }
    
    const auto* arr_node = dynamic_cast<const ArrayNode*>(it->second.get());
    if (!arr_node) {
        throw BuildError("Field '" + key + "' must be an array");
    }
    
    std::vector<std::string> result;
    for (const auto& elem : arr_node->elements) {
        const auto* str_node = dynamic_cast<const StringNode*>(elem.get());
        if (!str_node) {
            throw BuildError("Array '" + key + "' must contain strings");
        }
        result.push_back(str_node->value);
    }
    
    return result;
}
```

---

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Add node | O(1) amortized | HashMap insertion |
| Add edge | O(1) amortized | Vector append |
| Cycle detection | O(V + E) | DFS traversal |
| Get roots | O(V) | Linear scan |
| Reset state | O(V) | Linear iteration |

**Space Complexity:** O(V + E)
- Nodes: O(V)
- Edge lists: O(E)
- Colors/stack: O(V)

---

## Testing Strategy

### Unit Tests

```cpp
TEST(DependencyGraph, SimpleChain) {
    DependencyGraph graph;
    auto a = graph.addNode("A");
    auto b = graph.addNode("B");
    auto c = graph.addNode("C");
    
    graph.addEdge(a, b);
    graph.addEdge(b, c);
    
    EXPECT_EQ(a->dependencies().size(), 1);
    EXPECT_EQ(b->dependencies().size(), 1);
    EXPECT_EQ(c->dependencies().size(), 0);
    
    EXPECT_EQ(a->dependents().size(), 0);
    EXPECT_EQ(b->dependents().size(), 1);
    EXPECT_EQ(c->dependents().size(), 1);
}

TEST(CycleDetector, DetectsCycle) {
    DependencyGraph graph;
    auto a = graph.addNode("A");
    auto b = graph.addNode("B");
    auto c = graph.addNode("C");
    
    graph.addEdge(a, b);
    graph.addEdge(b, c);
    graph.addEdge(c, a);  // Cycle!
    
    CycleDetector detector(graph);
    auto cycle = detector.detectCycle();
    
    ASSERT_TRUE(cycle.has_value());
    EXPECT_EQ(cycle.value().size(), 4);  // A → B → C → A
}

TEST(CycleDetector, AllowsDiamond) {
    DependencyGraph graph;
    auto app = graph.addNode("App");
    auto liba = graph.addNode("LibA");
    auto libb = graph.addNode("LibB");
    auto core = graph.addNode("Core");
    
    graph.addEdge(app, liba);
    graph.addEdge(app, libb);
    graph.addEdge(liba, core);
    graph.addEdge(libb, core);
    
    CycleDetector detector(graph);
    auto cycle = detector.detectCycle();
    
    EXPECT_FALSE(cycle.has_value());  // Diamond is OK
}
```

---

**Next:** [40_EXECUTION_ENGINE.md](./40_EXECUTION_ENGINE.md) - Parallel build execution
