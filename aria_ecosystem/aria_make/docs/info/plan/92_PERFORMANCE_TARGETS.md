# Performance Targets & Benchmarks

This document defines the performance requirements and measurement strategies for aria_make.

---

## 📊 Performance Requirements

### Parsing & Configuration

| Operation | Target | Measurement |
|-----------|--------|-------------|
| Parse 1000-line build.aria | < 10ms | wall-clock time |
| Variable resolution (50 vars) | < 1ms | wall-clock time |
| Cycle detection (100 nodes) | < 5ms | wall-clock time |

**Rationale:** Configuration parsing happens once per build. Sub-10ms ensures developers never notice initialization latency.

---

### File Discovery

| Operation | Target | Measurement |
|-----------|--------|-------------|
| Glob 10,000 files | < 50ms | wall-clock time |
| Exclusion pruning (skip node_modules) | < 1ms | comparison to full traversal |
| Pattern compilation | < 1ms | one-time cost |

**Rationale:** Large projects may have 10k+ files. Glob operations must not dominate build time.

**Test Setup:**
```bash
# Create test directory with 10,000 files
mkdir -p test_repo/{src,build,node_modules}/{a..z}/{0..100}
touch test_repo/src/**/*.aria
touch test_repo/node_modules/**/*.js
```

---

### Graph Operations

| Operation | Target | Measurement |
|-----------|--------|-------------|
| Build DAG (500 nodes) | < 20ms | construction + validation |
| Cycle detection (500 nodes, 1000 edges) | < 10ms | DFS traversal |
| Topological sort (500 nodes) | < 5ms | Kahn's algorithm |

**Rationale:** Even large projects (500+ targets) should initialize quickly.

---

### Incremental Build

| Operation | Target | Measurement |
|-----------|--------|-------------|
| Timestamp check (100 files) | < 5ms | filesystem queries |
| Command hash (500 targets) | < 10ms | FNV-1a hashing |
| BuildState load/save | < 20ms | JSON serialization |

**Rationale:** Incremental analysis overhead should be < 1% of total build time.

**Example:**
- Full build: 60 seconds
- Incremental overhead: < 600ms
- Acceptable: Yes (1%)

---

### Parallel Execution

| Metric | Target | Measurement |
|--------|--------|-------------|
| Thread pool overhead | < 100μs per task | timing granularity |
| Scheduling latency | < 1ms | task submission to start |
| Atomic operations | Lock-free | TSan verification |

**Scalability Target:**
- 4-core system: 3.5x speedup (87.5% efficiency)
- 8-core system: 7.0x speedup (87.5% efficiency)
- 16-core system: 14.0x speedup (87.5% efficiency)

**Rationale:** 12.5% overhead accounts for:
- Synchronization costs
- Cache coherency
- Task granularity limits

---

### Process Execution

| Operation | Target | Measurement |
|-----------|--------|-------------|
| Spawn overhead (POSIX) | < 5ms | fork + exec |
| Spawn overhead (Windows) | < 10ms | CreateProcess |
| Stream capture (1MB output) | < 50ms | drain threads |
| No deadlocks | 100% | stress test with 10MB output |

**Rationale:** Process spawning is the dominant cost in builds. Minimize overhead to maximize compiler time.

---

## 🎯 Benchmark Suite

### Micro-Benchmarks

**ConfigParser:**
```cpp
void benchmark_parser() {
    std::string config = generateLargeBuildFile(1000);  // 1000 lines
    
    auto start = std::chrono::high_resolution_clock::now();
    ConfigParser parser(config);
    auto ast = parser.parse();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    assert(duration.count() < 10);  // < 10ms
}
```

**GlobEngine:**
```cpp
void benchmark_glob() {
    createTestRepo(10000);  // 10k files
    
    GlobEngine engine;
    engine.addExclusion("**/node_modules");
    
    auto start = std::chrono::high_resolution_clock::now();
    auto files = engine.expand("src/**/*.aria");
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    assert(duration.count() < 50);  // < 50ms
}
```

**CycleDetector:**
```cpp
void benchmark_cycle_detection() {
    DependencyGraph graph = generateComplexGraph(500, 1000);  // 500 nodes, 1000 edges
    
    auto start = std::chrono::high_resolution_clock::now();
    CycleDetector detector(graph);
    auto cycle = detector.detectCycle();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    assert(duration.count() < 10);  // < 10ms
}
```

---

### Integration Benchmarks

**Full Build (Cold Start):**
```
Project: 100 targets, 500 source files
Expected:
  - Parse + analyze: < 100ms
  - Compile (parallel, 8 cores): ~60s (depends on ariac performance)
  - Total: ~60.1s

Overhead target: < 0.2% (100ms / 60s)
```

**Incremental Build (No Changes):**
```
Project: 100 targets, 500 source files
Expected:
  - Parse + analyze: < 100ms
  - Timestamp checks: < 20ms
  - Compile: 0 (nothing dirty)
  - Total: < 120ms

User perception: "Instant" (< 200ms)
```

**Incremental Build (1 File Changed):**
```
Project: 100 targets, 500 source files
Change: 1 source file
Expected:
  - Parse + analyze: < 100ms
  - Recompile 1 file: ~1-2s
  - Relink: ~500ms
  - Total: ~2.6s

Overhead: < 4% (100ms / 2.6s)
```

---

## 📈 Profiling Strategy

### Timing Sections

```cpp
class SectionTimer {
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
    
public:
    explicit SectionTimer(std::string name) 
        : name_(std::move(name))
        , start_(std::chrono::high_resolution_clock::now()) {}
    
    ~SectionTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
        std::cerr << "[TIMING] " << name_ << ": " << duration.count() << "ms\n";
    }
};

// Usage
void build() {
    {
        SectionTimer t("Parse configuration");
        auto ast = parser.parse();
    }
    
    {
        SectionTimer t("Build dependency graph");
        graph.construct(ast);
    }
    
    {
        SectionTimer t("Incremental analysis");
        markDirtyNodes();
    }
    
    {
        SectionTimer t("Parallel compilation");
        scheduler.build();
    }
}
```

**Expected Output:**
```
[TIMING] Parse configuration: 8ms
[TIMING] Build dependency graph: 12ms
[TIMING] Incremental analysis: 15ms
[TIMING] Parallel compilation: 60234ms
```

---

### Memory Profiling

**Valgrind Massif:**
```bash
valgrind --tool=massif aria_make build

# Analyze
ms_print massif.out.12345
```

**Expected peak memory:**
- Small project (10 targets): < 10MB
- Medium project (100 targets): < 50MB
- Large project (1000 targets): < 500MB

**Breakdown:**
- AST: ~1KB per target
- DependencyGraph: ~500 bytes per node
- ThreadPool: ~8MB per worker (thread stack)
- BuildState: ~100 bytes per target

---

### CPU Profiling

**Linux perf:**
```bash
perf record -g aria_make build
perf report
```

**Expected hotspots:**
1. Compiler execution (ariac) - 95%+
2. Filesystem operations - 2-3%
3. Graph operations - 1-2%
4. Everything else - < 1%

**Red flags:**
- Parsing > 5% (should be negligible)
- Glob > 10% (likely missing anchor optimization)
- Cycle detection > 5% (graph too complex or algorithm issue)

---

### Thread Sanitizer

**Detect data races:**
```bash
g++ -fsanitize=thread -g -O2 src/*.cpp -o aria_make_tsan
./aria_make_tsan build
```

**Expected:** 0 warnings (all synchronization correct)

**Common issues:**
- Non-atomic in-degree updates
- Unprotected queue access
- Shared cache without mutex

---

## 🚀 Optimization Checklist

### Parsing
- [ ] Reuse Aria lexer (don't reimplement)
- [ ] Predictive descent (no backtracking)
- [ ] Panic mode recovery (don't cascade errors)
- [ ] Memoize variable resolution

### File Discovery
- [ ] Anchor-point optimization (start deep)
- [ ] Exclusion pruning (skip entire directories)
- [ ] Sort once at end (not during iteration)
- [ ] Cache directory hashes

### Graph Operations
- [ ] Topological order (Kahn's algorithm, O(V+E))
- [ ] Tri-color marking (distinguish diamonds from cycles)
- [ ] Atomic counters (lock-free parallel updates)
- [ ] Pre-allocate nodes (avoid fragmentation)

### Parallel Execution
- [ ] Fixed thread pool (not per-task threads)
- [ ] Lock-free atomic operations where possible
- [ ] Condition variables (not busy-waiting)
- [ ] Batching (submit multiple tasks at once)

### Incremental Builds
- [ ] Early exit (skip analysis if nothing changed)
- [ ] Command hashing (detect flag changes)
- [ ] Dependency propagation (dirty flag flows up graph)
- [ ] Persistent state (JSON for fast load/save)

---

## 📉 Anti-Patterns to Avoid

### DON'T: Spawn Threads Per Task

**Bad:**
```cpp
for (auto& node : nodes) {
    std::thread([&]() {
        compile(node);
    }).detach();
}
```

**Why:** 1000 targets → 1000 threads → system overload.

**Good:** Use fixed thread pool.

---

### DON'T: Use std::async for Builds

**Bad:**
```cpp
std::vector<std::future<void>> futures;
for (auto& node : nodes) {
    futures.push_back(std::async(std::launch::async, [&]() {
        compile(node);
    }));
}
```

**Why:** `std::async` may spawn unlimited threads (implementation-defined).

**Good:** Explicit ThreadPool with fixed size.

---

### DON'T: Block in Thread Pool Workers

**Bad:**
```cpp
void worker() {
    auto task = queue.pop();
    auto result = execute_blocking(task);  // ← Blocks thread!
}
```

**Why:** Wastes worker thread while waiting for I/O.

**Good:** For I/O-bound tasks, use async I/O or a separate I/O thread pool. For CPU-bound tasks (compilation), blocking is OK.

---

### DON'T: Parse Multiple Times

**Bad:**
```cpp
void validateConfig() {
    auto ast1 = parser.parse(config);
    // ...
}

void buildGraph() {
    auto ast2 = parser.parse(config);  // ← Redundant!
    // ...
}
```

**Good:** Parse once, pass AST around.

---

### DON'T: Ignore Filesystem Ordering

**Bad:**
```cpp
for (auto& entry : fs::directory_iterator(dir)) {
    files.push_back(entry);  // ← Non-deterministic order!
}
```

**Good:** Always sort results.

---

## 🏆 Success Metrics

Build system is considered performant if:

1. ✅ **Initialization < 100ms** (parse + graph construction)
2. ✅ **No-op build < 200ms** (incremental with no changes)
3. ✅ **Overhead < 1%** (on full builds)
4. ✅ **Linear scaling** (2x cores → ~2x speedup)
5. ✅ **No deadlocks** (stress tested with 10k tasks)
6. ✅ **Deterministic** (same input → same output, always)

---

**Note:** Absolute times depend on hardware. Targets assume modern CPU (2020+, 8 cores, 3+ GHz).

---

**Next:** Review [99_CRITICAL_NOTES.md](./99_CRITICAL_NOTES.md) before implementing!
