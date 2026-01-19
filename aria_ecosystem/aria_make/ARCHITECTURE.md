# aria_make Architecture

**Purpose**: Build system and package manager for Aria projects.

---

## Philosophy

### Core Principles

1. **Convention Over Configuration** - Sensible defaults, minimal boilerplate
2. **Explicit Dependencies** - No hidden build steps or magic
3. **Reproducible Builds** - Same input = same output, always
4. **Performance Matters** - Fast incremental builds, parallel where possible
5. **Integration With Ecosystem** - Works seamlessly with ariac, aria_shell, aria_utils

### Non-Negotiables

- ✅ **Spec Compliance** - Follows build system specification in aria_ecosystem
- ✅ **No External Dependencies** - Pure Aria implementation (eventually)
- ✅ **Hermetic Builds** - No reliance on system state
- ✅ **Deterministic** - Same inputs always produce same outputs
- ✅ **Fast** - Incremental builds must be near-instant

---

## Component Overview

```
aria_make/
├── src/
│   ├── init/          # Project initialization
│   ├── build/         # Build orchestration
│   ├── resolve/       # Dependency resolution
│   ├── cache/         # Build artifact caching
│   └── package/       # Package management
├── docs/              # Documentation
├── tests/             # Test suite
└── examples/          # Example projects
```

---

## Component Breakdown

### 1. Project Initialization (src/init/)

**Purpose**: Create new Aria projects from templates.

**Key Files**:
- `init.aria` - Main initialization logic
- `templates/` - Project templates

**Responsibilities**:
- Generate project structure
- Create build.aria manifest
- Set up git repository (optional)
- Initialize dependency tracking

**Outputs**:
```
new_project/
├── build.aria        # Project manifest
├── src/
│   └── main.aria     # Entry point
├── tests/            # Test directory
└── .gitignore
```

### 2. Build Orchestration (src/build/)

**Purpose**: Coordinate compilation of Aria projects.

**Key Files**:
- `build.aria` - Main build logic
- `compiler_interface.aria` - Interface to ariac
- `graph.aria` - Dependency graph construction

**Responsibilities**:
- Parse build.aria manifest
- Construct dependency graph
- Determine build order
- Invoke ariac for compilation
- Handle build errors

**Build Pipeline**:
```
build.aria manifest → Dependency Graph → Build Order → ariac invocation → Binary
```

### 3. Dependency Resolution (src/resolve/)

**Purpose**: Resolve and fetch project dependencies.

**Key Files**:
- `resolve.aria` - Dependency resolution algorithm
- `fetch.aria` - Package fetching
- `lock.aria` - Lock file generation

**Responsibilities**:
- Parse dependency specifications
- Resolve version constraints
- Fetch packages from repository
- Generate lock file
- Verify package integrity

**Algorithm**:
- Start with direct dependencies
- Recursively resolve transitive dependencies
- Detect and report conflicts
- Generate minimal dependency set

### 4. Build Caching (src/cache/)

**Purpose**: Cache build artifacts for fast incremental builds.

**Key Files**:
- `cache.aria` - Cache management
- `hash.aria` - Content hashing
- `invalidate.aria` - Cache invalidation

**Responsibilities**:
- Hash source files
- Track dependencies between files
- Invalidate stale cache entries
- Store/retrieve compiled artifacts

**Strategy**:
- Content-based hashing (not timestamp-based)
- Dependency tracking at file level
- Parallel cache lookups

### 5. Package Management (src/package/)

**Purpose**: Publish and consume Aria packages.

**Key Files**:
- `publish.aria` - Package publishing
- `install.aria` - Package installation
- `registry.aria` - Registry interface

**Responsibilities**:
- Package projects for distribution
- Upload to package repository
- Install packages from repository
- Verify package signatures

---

## Data Flow

### Build Flow

```
User runs: aria_make build

1. Parse build.aria manifest
   ↓
2. Resolve dependencies
   ↓
3. Fetch missing packages
   ↓
4. Construct dependency graph
   ↓
5. Check build cache
   ↓
6. Compile changed files (via ariac)
   ↓
7. Link final binary
   ↓
8. Update cache
   ↓
9. Report results
```

### Dependency Resolution Flow

```
User runs: aria_make install

1. Parse build.aria dependencies
   ↓
2. Check lock file (if exists)
   ↓
3. Resolve version constraints
   ↓
4. Fetch packages from registry
   ↓
5. Verify package integrity
   ↓
6. Extract to local cache
   ↓
7. Update lock file
   ↓
8. Report installed packages
```

---

## Key Algorithms

### Dependency Resolution

**Input**: Set of dependency constraints  
**Output**: Resolved dependency set or conflict error

```
Algorithm: Version Resolution
1. Create dependency graph
2. Topological sort to find build order
3. For each package:
   - Collect all version constraints
   - Find satisfying version
   - If no version satisfies all constraints, report conflict
4. Return resolved set
```

**Time Complexity**: O(P * V) where P = packages, V = versions per package

### Build Graph Construction

**Input**: Project source files and dependencies  
**Output**: Directed acyclic graph of build tasks

```
Algorithm: Build Graph
1. Scan source files for imports
2. For each file:
   - Add node to graph
   - Add edges to imported files
3. Detect cycles (invalid)
4. Topological sort for build order
```

**Time Complexity**: O(F + I) where F = files, I = imports

### Incremental Build

**Input**: Previous build state, changed files  
**Output**: Minimal set of files to recompile

```
Algorithm: Incremental Build
1. Hash all source files
2. Compare with previous hashes
3. Mark changed files as dirty
4. Mark files that import dirty files as dirty (transitive)
5. Rebuild only dirty files
```

**Time Complexity**: O(F + D * I) where D = dirty files

---

## File Formats

### build.aria Manifest

```aria
package "my_project" {
    version = "0.1.0"
    authors = ["Randy <randy@example.com>"]
    
    dependencies = {
        "std" = "0.1.0",
        "collections" = "^0.2.0"
    }
    
    build = {
        entry = "src/main.aria",
        target = "bin/my_project"
    }
}
```

### Lock File Format

```aria
# Generated by aria_make - DO NOT EDIT
lock_version = "1"

[[package]]
name = "std"
version = "0.1.0"
source = "registry+https://packages.aria-lang.org"
checksum = "sha256:abc123..."

[[package]]
name = "collections"
version = "0.2.1"
source = "registry+https://packages.aria-lang.org"
checksum = "sha256:def456..."
```

---

## Testing Strategy

### Unit Tests
- Dependency resolution algorithm
- Build graph construction
- Cache invalidation logic
- Version constraint parsing

### Integration Tests
- Full build of sample projects
- Incremental build correctness
- Dependency fetching and installation
- Error handling and reporting

### Performance Tests
- Large project build times
- Incremental build performance
- Parallel build speedup
- Cache hit rate

---

## Performance Goals

### Build Performance
- **Cold build**: Comparable to manual ariac invocations
- **Incremental build**: <100ms for single file change
- **Dependency resolution**: <1s for typical project

### Cache Efficiency
- **Hit rate**: >95% for typical development workflow
- **Storage**: <2x size of build artifacts

---

## Future Considerations

### Planned Features
- Cross-compilation support
- Custom build scripts in Aria
- Build profiles (debug/release/custom)
- Plugin system for custom build steps

### Potential Optimizations
- Distributed builds
- Remote caching
- Aggressive parallelization
- Predictive prefetching

---

## Integration Points

### With ariac
- Invokes compiler for source → binary
- Passes compilation flags
- Receives error output

### With aria_shell
- Can be invoked from shell scripts
- Provides scripting interface

### With aria_utils
- Uses utility functions
- Shares common libraries

### With Package Repository
- Fetches/publishes packages
- Verifies signatures
- Handles authentication

---

*This architecture prioritizes simplicity, correctness, and performance - in that order.*
