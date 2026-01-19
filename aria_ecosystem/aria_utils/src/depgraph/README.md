# depgraph - Aria Dependency Graph Engine

High-performance, parallel dependency graph engine for AriaBuild.

## Purpose

Core build scheduling infrastructure for:
- **aria_make** - Parallel build orchestration
- **ariac** - Compiler dependency tracking
- **IDE integration** - Incremental compilation support

## Features

- **DAG Modeling**: Build dependencies as Directed Acyclic Graph
- **Kahn's Algorithm**: O(V+E) topological sorting for build order
- **Cycle Detection**: Tri-Color DFS with actionable error messages
- **Parallel Execution**: M:N thread pool with work-stealing
- **Incremental Builds**: Timestamp-based rebuild decisions
- **Dependency Scanning**: Extract `use` statements from .aria files

## Quick Start

```cpp
#include "depgraph/graph.hpp"
#include "depgraph/scheduler.hpp"
#include "depgraph/builder.hpp"

using namespace aria::depgraph;

int main() {
    DependencyGraph graph;

    // Add targets
    auto* main_target = graph.add_node("main", NodeType::TARGET);
    auto* lib_target = graph.add_node("lib", NodeType::TARGET);

    // Define dependencies
    graph.add_edge("main", "lib");  // main depends on lib

    // Get build order
    auto schedule = TopologicalSorter::sort(graph);
    if (schedule.success()) {
        for (auto* node : schedule.order) {
            std::cout << "Build: " << node->id << "\n";
        }
    }

    return 0;
}
```

## Architecture

```
DependencyGraph    - Core DAG structure with O(1) node lookup
TopologicalSorter  - Kahn's algorithm for build ordering
CycleDetector      - Tri-Color DFS for cycle detection
DependencyScanner  - Extract 'use' statements from source
IncrementalChecker - Timestamp-based rebuild logic
ThreadPool         - M:N parallel task execution
BuildScheduler     - Orchestrates parallel builds
```

## Algorithms

### Topological Sort (Kahn's Algorithm)
- Time: O(V + E)
- Space: O(V)
- Naturally produces parallel execution opportunities

### Cycle Detection (Tri-Color DFS)
- Time: O(V + E)
- Space: O(V)
- Provides exact cycle path for error reporting

### Incremental Build
- Compares source timestamps against output
- Propagates dirty status through dependencies
- Achieves near-instant no-op builds

## Thread Pool

The thread pool uses an M:N threading model:
- M tasks mapped to N worker threads
- Default: hardware_concurrency threads
- Work-stealing for dynamic load balancing

```cpp
ThreadPool pool(4);  // 4 workers

pool.submit([]() {
    // Task code
});

pool.wait_all();
```

## Dependency Scanning

Extracts `use` statements from Aria source:

```cpp
auto result = DependencyScanner::scan_file("main.aria");

for (const auto& dep : result.dependencies) {
    std::cout << "Depends on: " << dep << "\n";
}
```

Handles:
- Logical paths: `use std.io`
- Relative paths: `use "local/module.aria"`
- Ignores comments and strings

## Building

```bash
mkdir build && cd build
cmake ..
make

# Run tests
make test
./test_depgraph
```

## Error Handling

Cycle detection provides actionable error messages:

```
Circular dependency detected: A -> B -> C -> A
```

Build failures are tracked per-target:

```cpp
if (!stats.success()) {
    std::cerr << "Failed: " << stats.failed << " targets\n";
}
```

## Performance

Designed for large projects:
- 1000+ targets: <100ms scheduling
- Incremental (1 change): <10ms
- No-op build: <1ms

## License

Part of the Aria Language Project. See LICENSE.md in repository root.
