# AriaX - Aria Package Manager & Distribution

**Component Type**: Package Manager + Linux Distribution  
**Language**: C++20 (planned) + Bash scripts  
**Repository**: Design phase (not yet created)  
**Status**: Concept / Design Phase  
**Version**: v0.0.1-concept

---

## Table of Contents

1. [Overview](#overview)
2. [Dual Purpose Design](#dual-purpose-design)
3. [Package Management](#package-management)
4. [Repository Protocol](#repository-protocol)
5. [Installation & Verification](#installation--verification)
6. [Custom Linux Distribution](#custom-linux-distribution)
7. [Kernel Patch for FD 3-5](#kernel-patch-for-fd-3-5)
8. [Integration Points](#integration-points)

---

## Overview

AriaX serves a **dual purpose**:

1. **Package Manager**: Like npm, cargo, or apt - manages Aria libraries and tools
2. **Linux Distribution**: Custom OS with kernel support for 6-stream I/O topology

### Package Manager Features

- Download and install Aria packages
- Dependency resolution with semantic versioning
- Cryptographic verification (GPG signatures + SHA-256)
- Global and local package installation
- Lock files for reproducible builds

### Linux Distribution Features

- Kernel patch: Reserve FDs 3-5 for applications
- Pre-installed Aria toolchain (compiler, runtime, shell, build system)
- Optimized for Aria development
- Based on existing distro (likely Arch or Gentoo for flexibility)

---

## Dual Purpose Design

### Why Combine Package Manager + OS?

**Problem**: Standard Linux doesn't reserve FDs 3-5. Applications can't rely on them being available.

**Solution**: AriaX Linux Distribution with kernel patch ensures:
- FDs 3-5 are **never** auto-assigned to files/sockets
- Applications can safely use them for stddbg/stddati/stddato
- Consistent behavior across all processes

**Package Manager Portability**:
- AriaX package manager works on **any** Linux (also Windows, macOS)
- AriaX OS **guarantees** 6-stream I/O works correctly
- Users can choose: portable (3 streams degraded to /dev/null) or native (6 streams fully functional)

---

## Package Management

### Package Repository Structure

**Central Repository**: `https://packages.aria-language.org/`

```
packages.aria-language.org/
├── index.json                    # Package index
├── std.collections/
│   ├── 1.0.0/
│   │   ├── std.collections-1.0.0.tar.gz
│   │   ├── std.collections-1.0.0.tar.gz.sig  (GPG signature)
│   │   └── manifest.json
│   └── 1.0.1/
│       └── ...
├── std.io/
│   └── 1.0.0/
│       └── ...
└── third-party/
    └── awesome-lib/
        └── 2.3.0/
            └── ...
```

### Package Manifest (manifest.json)

```json
{
  "name": "std.collections",
  "version": "1.0.0",
  "description": "Standard library collections (Vector, HashMap, etc.)",
  "authors": ["Aria Team <team@aria-language.org>"],
  "license": "MIT",
  
  "dependencies": {
    "std.mem": "^1.0.0"
  },
  
  "files": {
    "src": ["src/**/*.aria"],
    "headers": ["include/**/*.ariah"],
    "lib": ["lib/libstd_collections.a"]
  },
  
  "checksums": {
    "sha256": "abc123def456...",
    "md5": "789ghi012jkl..."
  }
}
```

### Installation Locations

**Global Installation** (system-wide):
```
/usr/local/aria/
├── packages/
│   ├── std.collections-1.0.0/
│   │   ├── src/
│   │   ├── include/
│   │   └── lib/
│   └── std.io-1.0.0/
│       └── ...
├── bin/                          # Executables (ariac, ariab, ariax)
├── lib/
│   └── libaria_runtime.a
└── include/
    └── aria/
        └── runtime/
```

**Local Installation** (project-specific):
```
my_project/
├── build.abc
├── build.lock
├── .aria_packages/               # Local package cache
│   ├── awesome-lib-2.3.0/
│   └── ...
└── src/
```

### Package Registry

**Global Registry** (`/usr/local/aria/package_registry.json`):
```json
{
  "installed": [
    {
      "name": "std.collections",
      "version": "1.0.0",
      "path": "/usr/local/aria/packages/std.collections-1.0.0",
      "installed_at": "2025-12-01T10:30:00Z",
      "checksum": "sha256:abc123..."
    },
    {
      "name": "std.io",
      "version": "1.0.0",
      "path": "/usr/local/aria/packages/std.io-1.0.0",
      "installed_at": "2025-12-01T10:30:05Z",
      "checksum": "sha256:def456..."
    }
  ],
  "repository": "https://packages.aria-language.org/"
}
```

---

## Repository Protocol

### Package Index (index.json)

```json
{
  "version": "1.0",
  "packages": [
    {
      "name": "std.collections",
      "latest_version": "1.0.1",
      "versions": ["1.0.0", "1.0.1"],
      "description": "Standard collections",
      "url": "https://packages.aria-language.org/std.collections/"
    },
    {
      "name": "std.io",
      "latest_version": "1.0.0",
      "versions": ["1.0.0"],
      "description": "I/O primitives",
      "url": "https://packages.aria-language.org/std.io/"
    }
  ]
}
```

### Semantic Versioning

**Version Constraints**:
- `1.0.0` - Exact version
- `^1.0.0` - Compatible with 1.0.0 (1.0.x, 1.y.z where y > 0)
- `~1.2.3` - Patch-level compatible (1.2.x)
- `>=1.0.0, <2.0.0` - Range

**Resolution Algorithm**:
1. Parse version constraints from build.abc
2. Query repository index for available versions
3. Find latest version satisfying all constraints
4. Recursively resolve dependencies
5. Generate build.lock with exact versions

---

## Installation & Verification

### Installation Workflow

```bash
$ ariax install std.collections
```

**Steps**:

1. **Query Repository**:
   ```
   GET https://packages.aria-language.org/index.json
     → Find std.collections-1.0.0
   ```

2. **Resolve Dependencies**:
   ```
   std.collections depends on:
     - std.mem ^1.0.0
   
   Resolve std.mem → 1.0.0 (latest compatible)
   
   Install order:
     1. std.mem-1.0.0
     2. std.collections-1.0.0
   ```

3. **Download Packages**:
   ```
   GET https://packages.aria-language.org/std.mem/1.0.0/std.mem-1.0.0.tar.gz
   GET https://packages.aria-language.org/std.mem/1.0.0/std.mem-1.0.0.tar.gz.sig
   
   GET https://packages.aria-language.org/std.collections/1.0.0/std.collections-1.0.0.tar.gz
   GET https://packages.aria-language.org/std.collections/1.0.0/std.collections-1.0.0.tar.gz.sig
   ```

4. **Verify Signatures**:
   ```
   GPG verify: std.mem-1.0.0.tar.gz using public key
     → Signature valid ✓
   
   SHA-256 hash: abc123... == abc123... (from index.json) ✓
   ```

5. **Extract**:
   ```
   tar -xzf std.mem-1.0.0.tar.gz -C /usr/local/aria/packages/
     → /usr/local/aria/packages/std.mem-1.0.0/
   
   tar -xzf std.collections-1.0.0.tar.gz -C /usr/local/aria/packages/
     → /usr/local/aria/packages/std.collections-1.0.0/
   ```

6. **Update Registry**:
   ```
   Add to /usr/local/aria/package_registry.json:
     - std.mem-1.0.0
     - std.collections-1.0.0
   ```

7. **Create Symlinks**:
   ```
   ln -s std.mem-1.0.0 /usr/local/aria/packages/std.mem
   ln -s std.collections-1.0.0 /usr/local/aria/packages/std.collections
   ```

### Cryptographic Verification

**GPG Signing**:
- Packages signed by Aria team (or third-party authors)
- Public keys distributed via keyserver
- Signature verification before extraction

**Checksum Verification**:
- SHA-256 hash in index.json
- Verify downloaded file integrity
- Protects against corrupted downloads

**Trust Model**:
- Aria official packages: Signed by Aria Team key
- Third-party packages: Signed by author, users trust at own risk
- Future: Web of trust, package reputation system

---

## Custom Linux Distribution

### AriaX Linux Distribution

**Base**: Arch Linux (or Gentoo for source-based flexibility)

**Modifications**:
1. **Kernel Patch**: Reserve FDs 3-5 (see below)
2. **Pre-installed Toolchain**:
   - ariac (Aria compiler)
   - libaria_runtime.a
   - aria_shell (AriaSH)
   - ariab (AriaBuild)
   - ariax (package manager)
3. **Optimized Environment**:
   - PATH includes `/usr/local/aria/bin`
   - Default shell: aria_shell
   - Development tools: gcc, clang, LLVM 20

### Installation Media

**ISO Image**: `ariax-linux-1.0.0.iso`

**Installation Options**:
- Graphical installer (GUI)
- Text-based installer (TUI for servers)
- Automated install (preseed/kickstart)

**Partitioning**:
- Standard Linux layout
- Optional: Separate `/home/aria` for Aria projects

---

## Kernel Patch for FD 3-5

### Problem

**Standard Linux Kernel**:
- File descriptors assigned sequentially: 0, 1, 2, 3, 4, 5, ...
- When opening files, gets next available FD (often 3)
- Conflicts with Aria's 6-stream I/O (wants FDs 3-5)

**Example Conflict**:
```c
int fd = open("file.txt", O_RDONLY);  // Returns FD 3
// Now FD 3 is a file, not stddbg!
```

### Solution: Kernel Patch

**Modify File Descriptor Allocation**:
```c
// In fs/file.c (Linux kernel source)

int get_unused_fd_flags(unsigned flags) {
    int fd;
    
    // Skip FDs 3-5 for application use
    fd = find_next_zero_bit(fdt->open_fds, fdt->max_fds, 0);
    if (fd >= 3 && fd <= 5) {
        fd = find_next_zero_bit(fdt->open_fds, fdt->max_fds, 6);
    }
    
    // ... rest of function
}
```

**Effect**:
- `open()` never returns FDs 3, 4, or 5
- FD sequence: 0, 1, 2, **6**, 7, 8, ... (skips 3-5)
- Applications can safely use FDs 3-5 for stddbg/stddati/stddato

### Kernel Build Process

```bash
# Download Linux kernel source
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.0.tar.xz
tar -xf linux-6.6.0.tar.xz
cd linux-6.6.0

# Apply AriaX patch
patch -p1 < /path/to/ariax-fd-reservation.patch

# Configure kernel
make menuconfig
# Enable: "Reserve FDs 3-5 for application use" (new config option)

# Build kernel
make -j$(nproc)

# Install
make modules_install
make install
```

### Backward Compatibility

**Kernel Config Option**:
```
CONFIG_RESERVED_FDS_3_5=y
```

**Boot Parameter**:
```
linux ... reserve_fds=3-5
```

Users can disable if needed (for compatibility with non-Aria software).

---

## Integration Points

### With AriaBuild

**Dependency Installation**:
```bash
ariab build
  ↓
Reads build.abc dependencies
  ↓
Calls: ariax install std.collections std.io
  ↓
AriaX downloads and installs packages
  ↓
AriaBuild continues with compilation
```

### With Aria Compiler

**Include Paths**:
```bash
ariac main.aria -I/usr/local/aria/packages/std.collections-1.0.0/include
```

AriaX maintains package registry, AriaBuild queries it for paths.

### With aria_shell

**Package Management Commands**:
```bash
aria_shell> ariax search collections
Found:
  - std.collections (v1.0.0): Standard library collections
  - third-party.awesome-collections (v2.1.0): Enhanced collections

aria_shell> ariax install std.collections
Installing std.collections-1.0.0...
  Downloading... Done
  Verifying signature... OK
  Extracting... Done
Installed std.collections-1.0.0

aria_shell> ariax list
Installed packages:
  - std.collections (1.0.0)
  - std.io (1.0.0)
  - std.mem (1.0.0)
```

---

## Package Commands

### Common Commands

| Command | Description |
|---------|-------------|
| `ariax install <package>` | Install package |
| `ariax uninstall <package>` | Remove package |
| `ariax update` | Update all packages |
| `ariax search <query>` | Search for packages |
| `ariax list` | List installed packages |
| `ariax info <package>` | Show package details |
| `ariax publish` | Publish package to repository |
| `ariax verify` | Verify installed packages |

### Usage Examples

```bash
# Install specific version
$ ariax install std.collections@1.0.0

# Install with constraint
$ ariax install "std.collections@^1.0.0"

# Update single package
$ ariax update std.collections

# Remove package
$ ariax uninstall std.collections

# Search for packages
$ ariax search json
Found 3 packages:
  - std.json (v1.0.0): Standard JSON library
  - third-party.fast-json (v2.1.0): High-performance JSON
  - third-party.json-schema (v1.5.0): JSON schema validator

# Show package info
$ ariax info std.collections
Name: std.collections
Version: 1.0.0
Description: Standard library collections (Vector, HashMap, etc.)
Authors: Aria Team <team@aria-language.org>
License: MIT
Dependencies:
  - std.mem ^1.0.0
Installed: Yes
Path: /usr/local/aria/packages/std.collections-1.0.0
```

---

## Publishing Packages

### Author Workflow

1. **Create Package**:
   ```bash
   mkdir my-awesome-lib
   cd my-awesome-lib
   ariax init
   ```

2. **Edit manifest.json**:
   ```json
   {
     "name": "my-awesome-lib",
     "version": "1.0.0",
     "description": "An awesome library",
     "authors": ["Alice <alice@example.com>"],
     "license": "MIT",
     "dependencies": {}
   }
   ```

3. **Write Code**:
   ```
   my-awesome-lib/
   ├── manifest.json
   ├── src/
   │   └── lib.aria
   └── README.md
   ```

4. **Test**:
   ```bash
   ariab test
   ```

5. **Publish**:
   ```bash
   ariax publish
   ```

### Repository Submission

**AriaX upload process**:
1. Create tarball: `my-awesome-lib-1.0.0.tar.gz`
2. Sign with GPG: `gpg --sign my-awesome-lib-1.0.0.tar.gz`
3. Upload to repository server
4. Server validates, adds to index.json
5. Package now available for installation

---

## Security Considerations

### Package Verification

- **GPG Signatures**: All packages must be signed
- **SHA-256 Checksums**: Verify integrity
- **Trusted Keys**: Only accept packages from known authors

### Sandboxing (Future)

- Run package installation in sandbox
- Prevent malicious packages from accessing system
- Post-install scripts: Audit and approve

### Vulnerability Scanning

- Automated scanning for known CVEs
- Notify users of vulnerable packages
- Auto-update critical security fixes

---

## Related Components

- **[AriaBuild](ARIABUILD.md)**: Uses AriaX for dependency resolution
- **[Aria Compiler](ARIA_COMPILER.md)**: Installed via AriaX
- **[Aria Runtime](ARIA_RUNTIME.md)**: Distributed as system package
- **[aria_shell](ARIA_SHELL.md)**: Invokes AriaX commands

---

**Document Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Design specification (implementation pending)
