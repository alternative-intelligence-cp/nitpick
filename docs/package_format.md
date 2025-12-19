# Aria Package Format Specification
Version: 0.1.0  
Date: December 18, 2025  
Phase: 7.5.1 - Package Manager Foundations

## Overview

The `.aria-pkg` format is the standard distribution format for Aria libraries and applications. It provides a simple, self-contained way to share and install Aria code with all necessary metadata and dependencies.

## Package Structure

An `.aria-pkg` file is a **gzip-compressed tarball** with the following internal structure:

```
package-name-1.0.0.aria-pkg
├── aria-package.toml        # Package metadata (REQUIRED)
├── src/                     # Source code (REQUIRED)
│   ├── lib.aria
│   ├── module1.aria
│   └── module2.aria
├── include/                 # Public headers/interfaces (OPTIONAL)
│   └── api.aria
├── tests/                   # Test files (OPTIONAL)
│   ├── test_module1.aria
│   └── test_module2.aria
├── examples/                # Example code (OPTIONAL)
│   └── demo.aria
├── docs/                    # Documentation (OPTIONAL)
│   ├── README.md
│   └── API.md
├── LICENSE                  # License file (RECOMMENDED)
└── README.md                # Package readme (RECOMMENDED)
```

## aria-package.toml Specification

The `aria-package.toml` file contains all package metadata. It extends the `aria.toml` project configuration with packaging-specific fields.

### Required Fields

```toml
[package]
name = "package-name"           # Package identifier (alphanumeric, hyphens, underscores)
version = "1.0.0"               # Semantic version (MAJOR.MINOR.PATCH)
authors = ["Author Name <email@example.com>"]
description = "Brief package description"
license = "MIT"                 # SPDX license identifier
```

### Optional Fields

```toml
[package]
homepage = "https://example.com/package"
repository = "https://github.com/user/package"
documentation = "https://docs.example.com/package"
readme = "README.md"
keywords = ["keyword1", "keyword2"]  # Max 5 keywords
categories = ["utilities", "parsing"]

# Minimum Aria compiler version
min_aria_version = "0.1.0"

# Include/exclude patterns
include = ["src/**", "include/**", "tests/**"]
exclude = ["**/*.tmp", "**/.git*"]

[dependencies]
# Other packages this package depends on
other-package = "^1.2.0"        # Caret: compatible with 1.x.x
another-lib = "~2.1.0"          # Tilde: compatible with 2.1.x
specific-lib = "3.0.0"          # Exact version

[dependencies.conditional]
# Platform-specific dependencies
linux-utils = { version = "1.0", platform = "linux" }
win-support = { version = "2.0", platform = "windows" }

[dev-dependencies]
# Dependencies only for development/testing
test-framework = "1.0.0"
benchmark-lib = "0.5.0"

[build]
# Build configuration
type = "library"                # or "executable"
output = "libpackage.so"        # For libraries
entry = "src/lib.aria"          # Main entry point

[features]
# Optional feature flags
default = ["feature1"]
feature1 = []
feature2 = ["dep-for-feature2"]
```

## Version Specifiers

Aria uses semantic versioning with the following specifiers:

| Specifier | Example | Matches |
|-----------|---------|---------|
| Exact     | `1.2.3` | Only 1.2.3 |
| Caret     | `^1.2.3` | ≥1.2.3, <2.0.0 (compatible) |
| Tilde     | `~1.2.3` | ≥1.2.3, <1.3.0 (patch updates) |
| Wildcard  | `1.2.*` | Any 1.2.x version |
| Range     | `>=1.2.0, <2.0.0` | Explicit range |

## Package Naming Conventions

Package names must:
- Be 3-64 characters long
- Start with a letter
- Contain only lowercase letters, numbers, hyphens, underscores
- Not start or end with hyphen/underscore
- Not contain consecutive special characters

Valid examples:
- `my-package`
- `http_client`
- `aria-stdlib`
- `math-utils-2d`

Invalid examples:
- `My-Package` (uppercase)
- `-bad-start` (starts with hyphen)
- `too__many` (consecutive underscores)
- `x` (too short)

## Package Types

### Library Package
```toml
[build]
type = "library"
entry = "src/lib.aria"
outputs = ["include/", "lib/"]
```

Generates:
- Compiled library (`.so`/`.dylib`/`.dll`)
- Public interface files
- Documentation

### Executable Package
```toml
[build]
type = "executable"
entry = "src/main.aria"
binary = "my-app"
```

Generates:
- Standalone executable
- Man pages (if included)
- Configuration files

### Meta Package
```toml
[build]
type = "meta"
# No build output, just dependency aggregation
```

Used for:
- Bundle of related packages
- Platform-specific toolchains
- Development environments

## Installation Layout

Packages are installed to `~/.aria/packages/` with the following structure:

```
~/.aria/packages/
├── registry/               # Package registry index
│   └── index.json
├── cache/                  # Downloaded .aria-pkg files
│   └── package-name-1.0.0.aria-pkg
├── installed/              # Extracted packages
│   └── package-name/
│       └── 1.0.0/
│           ├── aria-package.toml
│           ├── src/
│           ├── lib/
│           └── include/
└── bin/                    # Symlinked executables
    └── my-app -> ../installed/my-app/1.0.0/bin/my-app
```

## Package Distribution

### Local Installation
```bash
aria pkg install ./my-package-1.0.0.aria-pkg
```

### Registry Installation (future)
```bash
aria pkg install my-package
aria pkg install my-package@1.0.0
aria pkg install my-package@^1.2.0
```

### Git Installation (future)
```bash
aria pkg install git+https://github.com/user/package.git
aria pkg install git+https://github.com/user/package.git#v1.0.0
```

## Package Creation

### Manual Creation
```bash
# Create package structure
mkdir my-package
cd my-package
mkdir -p src tests docs

# Write aria-package.toml
cat > aria-package.toml << EOF
[package]
name = "my-package"
version = "1.0.0"
authors = ["Your Name <you@example.com>"]
description = "My awesome package"
license = "MIT"
EOF

# Write source code
cat > src/lib.aria << EOF
// Package implementation
func:greet = string(string:name) {
    return "Hello, &{name}!";
};
EOF

# Create package
tar czf my-package-1.0.0.aria-pkg \
    aria-package.toml \
    src/ \
    tests/ \
    docs/ \
    README.md \
    LICENSE
```

### Using aria-pkg Tool (Phase 7.5.2)
```bash
# Initialize new package
aria pkg init my-package

# Build package
aria pkg build

# Generates: my-package-1.0.0.aria-pkg
```

## Verification and Signing (Future)

For security, packages can be signed:

```bash
# Sign package
aria pkg sign my-package-1.0.0.aria-pkg --key ~/.aria/signing-key.pem

# Verify package
aria pkg verify my-package-1.0.0.aria-pkg
```

Signed packages include:
- `SIGNATURE` file with cryptographic signature
- `MANIFEST` file with SHA256 checksums

## Dependency Resolution

### Algorithm

1. **Parse** `aria-package.toml` dependencies
2. **Resolve** version constraints (newest compatible)
3. **Check** for conflicts (diamond dependency problem)
4. **Download** missing packages (if not in cache)
5. **Verify** package integrity (checksums)
6. **Install** packages in dependency order
7. **Link** executables to `~/.aria/packages/bin/`

### Lock File

For reproducible builds, generate `aria-package.lock`:

```toml
# aria-package.lock
# This file is auto-generated. Do not edit manually.

[[package]]
name = "dependency-1"
version = "1.2.3"
checksum = "sha256:abc123..."
source = "registry"

[[package]]
name = "dependency-2"
version = "2.0.0"
checksum = "sha256:def456..."
source = "git+https://github.com/user/repo.git#v2.0.0"
```

## Package Manager Commands

```bash
# Install package
aria pkg install <package-name>[@version]

# Remove package
aria pkg remove <package-name>

# Update packages
aria pkg update [<package-name>]

# List installed packages
aria pkg list

# Search for packages
aria pkg search <query>

# Show package info
aria pkg info <package-name>

# Create new package
aria pkg init <package-name>

# Build package
aria pkg build

# Publish package (future)
aria pkg publish

# Clean cache
aria pkg clean
```

## Example Package

Complete example: `math-utils` package

**aria-package.toml:**
```toml
[package]
name = "math-utils"
version = "1.0.0"
authors = ["Aria Team <team@aria-lang.org>"]
description = "Mathematical utility functions"
license = "MIT"
keywords = ["math", "utilities"]
categories = ["mathematics"]

[dependencies]
# No dependencies for this example

[build]
type = "library"
entry = "src/lib.aria"
```

**src/lib.aria:**
```aria
// Mathematical utility functions

func:abs = int32(int32:x) {
    if x < 0 then
        return -x;
    else
        return x;
    end;
};

func:max = int32(int32:a, int32:b) {
    if a > b then return a; else return b; end;
};

func:min = int32(int32:a, int32:b) {
    if a < b then return a; else return b; end;
};
```

**Build and install:**
```bash
# Build package
tar czf math-utils-1.0.0.aria-pkg \
    aria-package.toml \
    src/ \
    README.md \
    LICENSE

# Install
aria pkg install ./math-utils-1.0.0.aria-pkg

# Use in code
use math_utils;
int32:result = math_utils:max(10, 20);
```

## Future Enhancements

1. **Registry Server**: Central package repository
2. **Binary Caching**: Pre-compiled packages for common platforms
3. **Workspaces**: Monorepo support for multi-package projects
4. **Audit**: Security vulnerability scanning
5. **Yank**: Mark package versions as deprecated
6. **Badges**: Quality/build status indicators
7. **Documentation Hosting**: Automatic docs.aria-lang.org generation

## References

- Semantic Versioning: https://semver.org/
- SPDX License List: https://spdx.org/licenses/
- Cargo Package Format: https://doc.rust-lang.org/cargo/reference/manifest.html
- NPM Package.json: https://docs.npmjs.com/cli/v9/configuring-npm/package-json
