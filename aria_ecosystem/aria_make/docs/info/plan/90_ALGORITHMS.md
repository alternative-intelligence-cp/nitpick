# Key Algorithms Reference

This document provides detailed pseudo-code and implementation notes for the core algorithms used in aria_make.

---

## Kahn's Algorithm (Topological Sort + Parallel Scheduling)

### Purpose
Schedule build tasks in dependency order while maximizing parallelism.

### Algorithm

```
Input: Directed Acyclic Graph G = (V, E)
Output: Topological ordering of nodes, or cycle detection

1. Initialize:
   - Queue Q (ready nodes)
   - For each node v in V:
       in_degree[v] = number of incoming edges
   
2. Enqueue all nodes with in_degree == 0 into Q

3. While Q is not empty:
   a. Dequeue node u from Q
   b. Process u (submit to thread pool)
   c. For each neighbor v of u:
       i.  Decrement in_degree[v]
       ii. If in_degree[v] == 0:
           - Enqueue v into Q
   
4. If processed_count != |V|:
   - Graph has cycle (some nodes never reached 0 in-degree)
   - Return error
   
5. Otherwise: Success (all nodes processed)
```

### Parallel Build Modification

```cpp
void buildScheduler() {
    // 1. Find initial ready nodes
    std::queue<Node*> ready_queue;
    for (auto& node : graph.nodes()) {
        if (node->get_in_degree() == 0) {
            ready_queue.push(node);
        }
    }
    
    // 2. Submit initial batch to thread pool
    while (!ready_queue.empty()) {
        Node* node = ready_queue.front();
        ready_queue.pop();
        
        thread_pool.enqueue([this, node]() {
            executeNode(node);
        });
    }
    
    // 3. Workers call onTaskComplete() when done
}

void executeNode(Node* node) {
    // Check if dirty (incremental build)
    if (!node->is_dirty()) {
        onTaskComplete(node, {0, "", "", true});  // Skip, pretend success
        return;
    }
    
    // Build command
    auto [binary, args] = orchestrator.buildCommand(node);
    
    // Execute
    ExecResult result = execute(binary, args);
    
    // Notify scheduler
    onTaskComplete(node, result);
}

void onTaskComplete(Node* node, const ExecResult& result) {
    if (!result.success()) {
        // Propagate failure
        node->set_status(Node::Status::FAILED);
        propagateFailure(node);
        return;
    }
    
    node->set_status(Node::Status::COMPLETED);
    
    // Unlock dependents (ATOMIC!)
    for (Node* dependent : node->dependents()) {
        int new_in_degree = dependent->decrement_in_degree();
        
        if (new_in_degree == 0) {
            // This dependent is now ready
            thread_pool.enqueue([this, dependent]() {
                executeNode(dependent);
            });
        }
    }
}
```

### Why Atomic Counters?

**Problem:** Multiple threads may complete dependencies simultaneously.

```
Thread 1: LibA done → App.in_degree-- (read: 2, write: 1)
Thread 2: LibB done → App.in_degree-- (read: 2, write: 1)  ← DATA RACE!
Result: in_degree = 1 (should be 0!)
```

**Solution:**
```cpp
class Node {
    std::atomic<int> in_degree_;  // Thread-safe
    
    int decrement_in_degree() {
        return --in_degree_;  // Atomic decrement
    }
};
```

### Complexity
- **Time:** O(V + E) - each node and edge processed once
- **Space:** O(V) - queue and marking arrays

---

## Tri-Color DFS (Cycle Detection)

### Purpose
Detect cycles in dependency graph while allowing diamond dependencies.

### Color Meanings
- **WHITE:** Unvisited node
- **GRAY:** Currently in recursion stack (active path)
- **BLACK:** Fully processed (all descendants explored)

### Algorithm

```cpp
enum Color { WHITE, GRAY, BLACK };
std::unordered_map<Node*, Color> color;
std::vector<Node*> path_stack;  // For cycle reconstruction

bool detectCycle(DependencyGraph& graph) {
    // Initialize all WHITE
    for (auto& node : graph.nodes()) {
        color[node] = WHITE;
    }
    
    // DFS from each unvisited node
    for (auto& node : graph.nodes()) {
        if (color[node] == WHITE) {
            if (dfs(node)) {
                return true;  // Cycle found
            }
        }
    }
    
    return false;  // No cycle
}

bool dfs(Node* u) {
    color[u] = GRAY;  // Mark as "in progress"
    path_stack.push_back(u);
    
    for (Node* v : u->dependencies) {
        if (color[v] == GRAY) {
            // Back edge detected → CYCLE!
            reconstructCycle(v);
            return true;
        }
        
        if (color[v] == WHITE) {
            if (dfs(v)) {
                return true;  // Cycle in descendant
            }
        }
        
        // BLACK: Safe (cross edge or forward edge)
    }
    
    color[u] = BLACK;  // Mark as "fully processed"
    path_stack.pop_back();
    return false;
}

void reconstructCycle(Node* cycle_start) {
    std::vector<std::string> cycle_path;
    
    // Find where cycle starts in stack
    auto it = std::find(path_stack.begin(), path_stack.end(), cycle_start);
    
    // Extract cycle portion
    for (; it != path_stack.end(); ++it) {
        cycle_path.push_back((*it)->name());
    }
    
    // Close the loop
    cycle_path.push_back(cycle_start->name());
    
    // Report: "A → B → C → A"
    std::string message = "Cycle detected: ";
    for (size_t i = 0; i < cycle_path.size(); ++i) {
        message += cycle_path[i];
        if (i < cycle_path.size() - 1) {
            message += " → ";
        }
    }
    
    throw CycleError(message);
}
```

### Example Execution

**Graph:**
```
A → B → C
A → D → C  (Diamond - OK)
C → E → B  (Cycle!)
```

**DFS Trace:**
```
dfs(A):
  color[A] = GRAY
  path_stack = [A]
  
  dfs(B):
    color[B] = GRAY
    path_stack = [A, B]
    
    dfs(C):
      color[C] = GRAY
      path_stack = [A, B, C]
      
      dfs(E):
        color[E] = GRAY
        path_stack = [A, B, C, E]
        
        Check B:
          color[B] == GRAY  ← CYCLE!
          path_stack = [A, B, C, E]
          cycle_start = B
          
          Extract: [B, C, E, B]
          Error: "B → C → E → B"
```

### Why Three Colors?

**Two colors (visited/unvisited) can't distinguish:**

```
Diamond:              Cycle:
    A                    A
   / \                  / \
  B   C                B   C
   \ /                  \ /
    D                    B  (← back to B!)
```

**With GRAY:**
- Diamond: When DFS from A→B→D completes, B becomes BLACK. When DFS from A→C→D reaches D, it sees B is BLACK (not in current path) → OK.
- Cycle: When DFS from A→B→C tries to visit B again, B is GRAY (still in path) → CYCLE!

### Complexity
- **Time:** O(V + E) - each node visited once
- **Space:** O(V) - recursion stack + color map

---

## Shifting Wildcard Matcher (Glob Pattern Matching)

### Purpose
Match filenames against glob patterns WITHOUT using regex.

### Supported Patterns
- `*` - Matches 0+ characters (excluding `/`)
- `?` - Matches exactly 1 character (excluding `/`)
- `[abc]` - Matches one of: a, b, c
- `[a-z]` - Matches range: a through z
- `[!abc]` - Matches anything except: a, b, c
- `\*` - Literal asterisk (escaped)

### Algorithm

```cpp
bool match(std::string_view str, std::string_view pat) {
    size_t s = 0, p = 0;
    size_t star_idx = std::string::npos, match_idx = 0;
    
    while (s < str.length()) {
        if (p < pat.length() && (pat[p] == '?' || pat[p] == str[s])) {
            // Exact match or question mark
            s++;
            p++;
        }
        else if (p < pat.length() && pat[p] == '*') {
            // Star: remember position and try matching 0 chars
            star_idx = p;
            match_idx = s;
            p++;  // Advance pattern, NOT string
        }
        else if (p < pat.length() && pat[p] == '[') {
            // Character class
            if (matchCharClass(str[s], pat, p)) {
                s++;
            } else if (star_idx != std::string::npos) {
                // Backtrack to last star
                p = star_idx + 1;
                match_idx++;
                s = match_idx;
            } else {
                return false;
            }
        }
        else if (star_idx != std::string::npos) {
            // Mismatch, but we have a star: backtrack
            // Try matching one more character with the star
            p = star_idx + 1;
            match_idx++;
            s = match_idx;
        }
        else {
            // Mismatch, no star to backtrack to
            return false;
        }
    }
    
    // Consume trailing stars
    while (p < pat.length() && pat[p] == '*') {
        p++;
    }
    
    return p == pat.length();
}
```

### Character Class Matching

```cpp
bool matchCharClass(char c, std::string_view pat, size_t& p) {
    size_t start = p;
    p++;  // Skip '['
    
    bool negate = false;
    if (p < pat.length() && (pat[p] == '!' || pat[p] == '^')) {
        negate = true;
        p++;
    }
    
    bool matched = false;
    while (p < pat.length() && pat[p] != ']') {
        if (p + 2 < pat.length() && pat[p + 1] == '-') {
            // Range: a-z
            char start = pat[p];
            char end = pat[p + 2];
            if (c >= start && c <= end) {
                matched = true;
            }
            p += 3;
        } else {
            // Single char
            if (pat[p] == c) {
                matched = true;
            }
            p++;
        }
    }
    
    p++;  // Skip ']'
    
    return negate ? !matched : matched;
}
```

### Example Traces

**Pattern:** `a*b?c`  
**String:** `axxxbyyc`

```
s=0, p=0: 'a' == 'a' → match (s=1, p=1)
s=1, p=1: '*' → star_idx=1, match_idx=1, p=2
s=1, p=2: 'x' != 'b' → backtrack (match_idx=2, s=2, p=2)
s=2, p=2: 'x' != 'b' → backtrack (match_idx=3, s=3, p=2)
s=3, p=2: 'x' != 'b' → backtrack (match_idx=4, s=4, p=2)
s=4, p=2: 'b' == 'b' → match (s=5, p=3)
s=5, p=3: '?' → match any (s=6, p=4)
s=6, p=4: 'y' != 'c' → backtrack (match_idx=5, s=5, p=2)
... (eventually matches)
```

### Performance
- **Best case:** O(N) - simple patterns without stars
- **Worst case:** O(N × M) - many stars with backtracking
- **Typical:** O(N) - build patterns are usually simple

### Comparison to Regex

| Aspect | Regex | Shifting Wildcard |
|--------|-------|------------------|
| Compilation | Slow (NFA/DFA) | Instant (no compilation) |
| Memory | High (state machine) | Low (few variables) |
| Semantics | Different (. matches /) | Correct (glob rules) |
| Performance | Variable | Predictable |

---

## FNV-1a Hash (Command Signature)

### Purpose
Fast, non-cryptographic hash for detecting command changes.

### Algorithm

```cpp
uint64_t fnv1a_64(std::string_view data) {
    uint64_t hash = 14695981039346656037ULL;  // FNV offset basis
    
    for (unsigned char c : data) {
        hash ^= c;                          // XOR with byte
        hash *= 1099511628211ULL;           // Multiply by FNV prime
    }
    
    return hash;
}
```

### Properties
- **Fast:** Single pass, simple operations
- **Good distribution:** Low collision rate for typical inputs
- **Deterministic:** Same input → same hash (across platforms!)
- **Not cryptographic:** Don't use for security

### Collision Probability
For 64-bit FNV-1a with N distinct inputs:
```
P(collision) ≈ N² / 2^65 ≈ N² × 2.71 × 10^-20
```

For 10,000 build targets:
```
P(collision) ≈ 10^8 / 2^65 ≈ 2.71 × 10^-12  (negligible)
```

### Why Not std::hash?

`std::hash<std::string>` is:
- Implementation-defined (not portable!)
- GCC and Clang use different algorithms
- Hash value differs across platforms → breaks BuildState serialization

FNV-1a is:
- Standardized algorithm
- Same result on all platforms
- Suitable for persistence

---

## Timestamp Comparison (Incremental Build)

### Purpose
Determine if a target needs rebuilding based on file modification times.

### Algorithm

```cpp
bool isDirty(Node* node) {
    // 1. Output missing?
    if (!fs::exists(node->output_file)) {
        return true;
    }
    
    auto output_time = fs::last_write_time(node->output_file);
    
    // 2. Any source newer than output?
    for (const auto& src : node->source_files) {
        if (!fs::exists(src)) {
            throw FileNotFound(src);
        }
        
        if (fs::last_write_time(src) > output_time) {
            return true;  // Source modified after output built
        }
    }
    
    // 3. Any dependency rebuilt?
    for (Node* dep : node->dependencies) {
        if (dep->is_dirty()) {
            return true;  // Dependency will be rebuilt
        }
        
        if (fs::exists(dep->output_file)) {
            auto dep_time = fs::last_write_time(dep->output_file);
            if (dep_time > output_time) {
                return true;  // Dependency newer than this output
            }
        }
    }
    
    return false;  // Up-to-date
}
```

### Propagation Order

Must check dependencies in topological order:

```
LibA (modified) → LibB (depends on A) → App (depends on B)

1. Check LibA: source modified → dirty
2. Check LibB: dep (LibA) is dirty → dirty
3. Check App: dep (LibB) is dirty → dirty
```

**Wrong order:**
```
1. Check App: deps not yet checked → not dirty (WRONG!)
2. Check LibB: ...
```

**Solution:** Always traverse in topological order (Kahn's algorithm naturally provides this).

### Edge Cases

| Case | Behavior |
|------|----------|
| Output missing | Always dirty |
| Source missing | Error (file not found) |
| Dep output missing | Dep will be dirty → this dirty |
| Times equal | Not dirty (same timestamp) |
| Future timestamp | Warn, treat as dirty |

---

**Next:** [91_PLATFORM_COMPAT.md](./91_PLATFORM_COMPAT.md) for cross-platform code patterns
