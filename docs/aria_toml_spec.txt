# Aria.toml Specification v0.1.0

## Overview
`aria.toml` is the project manifest file for Aria projects. It defines project metadata, dependencies, build configuration, and other project-specific settings. This specification is inspired by Cargo.toml (Rust) and follows TOML syntax.

## File Location
- Project root: `aria.toml`
- Workspace root: `aria.toml` (for multi-package workspaces)

## Minimal Example
```toml
[package]
name = "hello_world"
version = "0.1.0"
```

## Complete Example
```toml
[package]
name = "myproject"
version = "0.2.1"
authors = ["Alice <alice@example.com>", "Bob <bob@example.com>"]
edition = "2025"
license = "MIT OR Apache-2.0"
description = "A sample Aria project"
homepage = "https://example.com/myproject"
repository = "https://github.com/user/myproject"
readme = "README.md"
keywords = ["example", "sample"]

[dependencies]
std = "0.1"
aria-http = "1.2.3"
aria-json = { version = "2.0", features = ["validation"] }
local-lib = { path = "../local-lib" }
optional-lib = { version = "1.0", optional = true }

[dev-dependencies]
aria-test = "0.1"
aria-bench = "0.1"

[build-dependencies]
aria-codegen = "1.0"

[build]
target = "executable"  # or "library", "shared", "static"
optimization = "release"  # or "debug", "size"
output = "build/"
sources = ["src/**/*.aria"]
exclude = ["src/tests/**"]

[features]
default = ["feature1"]
feature1 = []
feature2 = ["optional-lib"]
all = ["feature1", "feature2"]

[profile.release]
opt-level = 3
lto = true
strip = true

[profile.debug]
opt-level = 0
debug-info = true

[workspace]
members = ["crates/*"]
exclude = ["examples/*"]

[lib]
name = "mylib"
crate-type = ["lib", "staticlib"]  # or ["dylib", "cdylib"]
path = "src/lib.aria"

[bin]
name = "mycli"
path = "src/main.aria"

[[bin]]
name = "tool1"
path = "src/tool1.aria"

[[bin]]
name = "tool2"
path = "src/tool2.aria"

[test]
harness = true
```

## Section Specifications

### [package] (Required)
Defines basic package metadata.

**Required fields:**
- `name` (string): Package name. Must be alphanumeric with hyphens/underscores. 1-64 characters.
- `version` (string): Semantic version (e.g., "1.2.3"). Must follow semver.

**Optional fields:**
- `authors` (array of strings): Author names and emails in format "Name <email@example.com>"
- `edition` (string): Aria edition year (e.g., "2025", "2026"). Default: latest stable.
- `license` (string): SPDX license identifier (e.g., "MIT", "Apache-2.0", "MIT OR Apache-2.0")
- `description` (string): Brief package description. Max 256 characters.
- `homepage` (string): URL to project homepage
- `repository` (string): URL to source repository
- `readme` (string): Path to README file. Default: "README.md"
- `keywords` (array of strings): Search keywords. Max 5 keywords, 20 chars each.
- `categories` (array of strings): Package categories
- `publish` (boolean): Allow publishing to package registry. Default: true

**Example:**
```toml
[package]
name = "awesome-lib"
version = "1.0.0"
authors = ["Dev Team <dev@example.com>"]
edition = "2025"
license = "MIT"
description = "An awesome Aria library"
keywords = ["awesome", "utility"]
```

### [dependencies]
Project dependencies from package registry or local paths.

**Formats:**
```toml
# Simple version
std = "0.1"

# Detailed version with features
aria-http = { version = "1.2", features = ["tls", "compression"] }

# Local path dependency
local-lib = { path = "../local-lib" }

# Git dependency
git-lib = { git = "https://github.com/user/repo", branch = "main" }
git-lib2 = { git = "https://github.com/user/repo", tag = "v1.0.0" }
git-lib3 = { git = "https://github.com/user/repo", rev = "abc123" }

# Optional dependency (enabled by features)
optional-lib = { version = "1.0", optional = true }
```

**Version specifiers:**
- `"1.2.3"` - Exact version
- `"^1.2.3"` - Compatible with 1.2.3 (>=1.2.3, <2.0.0)
- `"~1.2.3"` - Approximately 1.2.3 (>=1.2.3, <1.3.0)
- `">= 1.2"` - Greater than or equal to 1.2
- `"< 2.0"` - Less than 2.0
- `"1.2.*"` - Wildcard (any patch version)

### [dev-dependencies]
Dependencies only used during development (tests, benchmarks).

```toml
[dev-dependencies]
aria-test = "0.1"
aria-mock = "1.0"
```

### [build-dependencies]
Dependencies used by build scripts.

```toml
[build-dependencies]
aria-codegen = "1.0"
```

### [build]
Build configuration.

**Fields:**
- `target` (string): Build target type
  - `"executable"` - Executable binary (default)
  - `"library"` - Library (static + dynamic)
  - `"static"` - Static library only
  - `"shared"` - Shared/dynamic library only
- `optimization` (string): Optimization level
  - `"debug"` - No optimization, debug symbols (default for dev)
  - `"release"` - Full optimization (default for release)
  - `"size"` - Optimize for size
- `output` (string): Output directory. Default: "build/"
- `sources` (array of strings): Source file globs. Default: ["src/**/*.aria"]
- `exclude` (array of strings): Files to exclude from build
- `main` (string): Entry point file. Default: "src/main.aria"

**Example:**
```toml
[build]
target = "executable"
optimization = "release"
output = "dist/"
sources = ["src/**/*.aria"]
exclude = ["src/tests/**", "src/examples/**"]
main = "src/main.aria"
```

### [features]
Feature flags for conditional compilation.

```toml
[features]
default = ["std"]  # Features enabled by default
std = []  # Standard library support
tls = ["dep:aria-tls"]  # Enable TLS (requires aria-tls dependency)
experimental = ["feature1", "feature2"]  # Combine features
```

**Feature naming:**
- Alphanumeric, hyphens, underscores
- Use `dep:package-name` to enable optional dependencies

### [profile.*]
Build profiles for different optimization levels.

**Available profiles:**
- `[profile.debug]` - Development builds
- `[profile.release]` - Production builds
- `[profile.test]` - Test builds
- `[profile.bench]` - Benchmark builds

**Fields:**
- `opt-level` (integer): Optimization level 0-3
  - `0` - No optimization
  - `1` - Basic optimization
  - `2` - Full optimization
  - `3` - Aggressive optimization
- `debug-info` (boolean): Include debug symbols. Default: true for debug, false for release
- `lto` (boolean): Link-time optimization. Default: false
- `strip` (boolean): Strip debug symbols. Default: false
- `overflow-checks` (boolean): Enable integer overflow checks. Default: true for debug
- `panic` (string): Panic handling strategy
  - `"unwind"` - Stack unwinding (default)
  - `"abort"` - Immediate abort

**Example:**
```toml
[profile.release]
opt-level = 3
lto = true
strip = true
debug-info = false

[profile.debug]
opt-level = 0
debug-info = true
overflow-checks = true
```

### [workspace]
Multi-package workspace configuration.

```toml
[workspace]
members = ["crates/*", "tools/cli"]
exclude = ["examples/*", "archive/*"]
```

**Fields:**
- `members` (array of strings): Package paths (supports globs)
- `exclude` (array of strings): Paths to exclude (supports globs)

### [lib]
Library configuration (for library targets).

```toml
[lib]
name = "mylib"
crate-type = ["lib"]
path = "src/lib.aria"
```

**Fields:**
- `name` (string): Library name. Default: package name
- `crate-type` (array of strings): Library types
  - `"lib"` - Aria library (static + dynamic)
  - `"staticlib"` - Static C-compatible library
  - `"dylib"` - Dynamic Aria library
  - `"cdylib"` - C-compatible dynamic library
- `path` (string): Library entry point. Default: "src/lib.aria"

### [[bin]]
Binary target configuration. Use `[[bin]]` for multiple binaries.

```toml
[[bin]]
name = "mycli"
path = "src/main.aria"

[[bin]]
name = "tool"
path = "src/tool.aria"
```

**Fields:**
- `name` (string): Binary name. Default: package name
- `path` (string): Entry point. Default: "src/main.aria"

### [test]
Test configuration.

```toml
[test]
harness = true
```

**Fields:**
- `harness` (boolean): Use built-in test harness. Default: true

## Validation Rules

1. **Package name:**
   - Alphanumeric, hyphens, underscores only
   - Must start with letter
   - 1-64 characters
   - Cannot be reserved word (std, core, etc.)

2. **Version:**
   - Must follow semantic versioning (major.minor.patch)
   - Optional pre-release: "1.0.0-alpha.1"
   - Optional build metadata: "1.0.0+build.123"

3. **Paths:**
   - Relative to aria.toml location
   - Use forward slashes (/) on all platforms
   - Cannot escape project root

4. **Dependencies:**
   - Version must be valid semver range
   - Cannot have circular dependencies
   - Local paths must exist

## Future Extensions (Post v0.1.0)

- `[scripts]` - Build scripts (pre-build, post-build)
- `[metadata]` - Custom metadata for tools
- `[patch]` - Dependency patching
- `[replace]` - Dependency replacement
- `[target.*]` - Platform-specific configuration
- `[badges]` - README badges
- `[aliases]` - Command aliases

## TOML Parser Requirements

The aria.toml parser must:
- Support TOML v1.0.0 specification
- Validate all required fields
- Provide helpful error messages with line numbers
- Support all standard TOML data types (string, integer, float, boolean, datetime, array, table)
- Handle inline tables and array of tables syntax

## Error Messages

Errors should be clear and actionable:

```
Error: Missing required field 'name' in [package]
  --> aria.toml:1:1
  |
1 | [package]
  | ^^^^^^^^^ 'name' field is required
  |
  = help: Add name = "myproject" to the [package] section
```

```
Error: Invalid version format
  --> aria.toml:3:11
  |
3 | version = "1.0"
  |           ^^^^^ expected semantic version (e.g., "1.0.0")
  |
  = help: Version must be in format major.minor.patch
```

## Implementation Notes

1. Use existing TOML parsing library (e.g., toml11, cpptoml)
2. Create validation module to check all rules
3. Integrate with compiler driver (Phase 7.4.2)
4. Support both aria.toml and Aria.toml (case-insensitive on Windows)
5. Cache parsed aria.toml to avoid re-parsing on every build

## Version History

- v0.1.0 (2025-12-18): Initial specification
