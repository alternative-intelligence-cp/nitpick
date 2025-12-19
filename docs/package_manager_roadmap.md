# Future Package Manager Enhancements

This document outlines planned features for integrating `aria-pkg` with the central package repository.

## Phase 1: Remote Registry Support (Planned)

### Search Command
```bash
aria-pkg search <query>
```

Implementation plan:
- Fetch registry.json from repository
- Parse package metadata
- Filter by keywords, categories, description
- Display results with package info

Example:
```bash
$ aria-pkg search "http client"
Found 3 packages:

http-client (v2.1.0)
  Description: Full-featured HTTP/1.1 and HTTP/2 client
  Categories: networking, web
  Downloads: 1,245

async-http (v1.0.3)
  Description: Async HTTP client built on Aria's async runtime
  Categories: networking, async
  Downloads: 892

rest-client (v3.0.1)
  Description: REST API client with automatic serialization
  Categories: web, networking
  Downloads: 2,103
```

### Install from Registry
```bash
aria-pkg install <package-name>[@version]
```

Implementation plan:
- Query registry for package
- Resolve version (latest if not specified)
- Download .aria-pkg from repository
- Verify checksum
- Install to ~/.aria/packages/

Example:
```bash
$ aria-pkg install http-client
Fetching package info from registry...
Downloading http-client v2.1.0...
Verifying checksum...
Installing to ~/.aria/packages/installed/http-client/2.1.0/
Successfully installed http-client v2.1.0
```

### Update Command
```bash
aria-pkg update [package-name]
```

Implementation plan:
- Check registry for newer versions
- Download and install updates
- Keep old versions unless --force-remove

### Publish Command
```bash
aria-pkg publish
```

Implementation plan:
- Read aria-package.toml
- Create .aria-pkg tarball
- Calculate checksum
- Generate metadata.json
- Submit to repository via PR or API

## Phase 2: Dependency Resolution (Planned)

### Automatic Dependency Installation
When installing a package with dependencies:
```bash
$ aria-pkg install web-framework
Resolving dependencies...
  - http-client ^2.1.0
  - template-engine ^1.0.0
  - session-store ^0.9.0
Downloading 4 packages...
Installing web-framework v3.0.0...
Installing http-client v2.1.5...
Installing template-engine v1.2.1...
Installing session-store v0.9.3...
Successfully installed web-framework v3.0.0 and 3 dependencies
```

### Lock File Generation
```bash
aria-pkg lock
```

Creates `aria-package.lock` with exact versions:
```json
{
  "packages": {
    "web-framework": {
      "version": "3.0.0",
      "checksum": "abc123...",
      "dependencies": {
        "http-client": "2.1.5",
        "template-engine": "1.2.1",
        "session-store": "0.9.3"
      }
    }
  }
}
```

## Phase 3: Advanced Features (Future)

### Package Statistics
```bash
$ aria-pkg stats http-client
Package: http-client
Latest version: 2.1.0
Total downloads: 1,245
Weekly downloads: 87
Stars: 124
Issues: 3 open, 45 closed
Last updated: 2025-12-15
```

### Verify Command
```bash
aria-pkg verify <package-name>
```

Verifies:
- Package checksum matches registry
- GPG signature (if available)
- No known security vulnerabilities

### Audit Command
```bash
aria-pkg audit
```

Scans installed packages for:
- Known vulnerabilities
- Deprecated versions
- Breaking updates available

### Workspace Support
```toml
# aria-workspace.toml
[workspace]
members = [
    "app",
    "lib/core",
    "lib/utils"
]

[workspace.dependencies]
http-client = "^2.1.0"
```

## Implementation Notes

### Repository URL Configuration
Add to aria-package.toml or global config:
```toml
[registry]
default = "https://raw.githubusercontent.com/aria-lang/aria_package_repository/main"
mirrors = [
    "https://mirror1.aria-pkg.org",
    "https://mirror2.aria-pkg.org"
]
```

### Caching Strategy
- Cache registry.json locally (TTL: 1 hour)
- Cache .aria-pkg files in ~/.aria/packages/cache/
- Verify checksums on every install

### Network Layer
Use existing libraries:
- HTTP client: libcurl or system HTTP
- JSON parsing: nlohmann/json (already integrated)
- Checksum: SHA-256 (standard library)

### Error Handling
- Network timeouts: Retry with backoff
- Checksum mismatch: Abort with error
- Dependency conflicts: Show resolution options
- Missing packages: Suggest similar names

## Code Structure

### Proposed Files
```
include/tools/pkg/
├── installer.h          (existing)
├── registry_client.h    (new)
├── dependency_resolver.h (new)
└── package_verifier.h   (new)

src/tools/pkg/
├── installer.cpp        (existing)
├── registry_client.cpp  (new)
├── dependency_resolver.cpp (new)
└── package_verifier.cpp (new)
```

### RegistryClient Interface
```cpp
class RegistryClient {
public:
    // Fetch registry index
    bool fetchRegistry(const std::string& url);
    
    // Search for packages
    std::vector<PackageInfo> search(const std::string& query);
    
    // Get package metadata
    std::optional<PackageMetadata> getPackage(const std::string& name);
    
    // Download package
    bool downloadPackage(const std::string& name, 
                        const std::string& version,
                        const std::string& output_path);
    
    // Verify checksum
    bool verifyChecksum(const std::string& file_path,
                       const std::string& expected_checksum);
};
```

### DependencyResolver Interface
```cpp
class DependencyResolver {
public:
    // Resolve dependencies recursively
    std::vector<PackageMetadata> resolve(const PackageMetadata& root);
    
    // Check for conflicts
    bool checkConflicts(const std::vector<PackageMetadata>& packages);
    
    // Generate lock file
    bool generateLockFile(const std::vector<PackageMetadata>& packages,
                         const std::string& output_path);
};
```

## Timeline

### Near Term (Q1 2026)
- [x] Phase 7.5: Basic package manager ✅
- [x] Create aria_package_repository ✅
- [ ] Implement registry_client.cpp
- [ ] Add search command
- [ ] Add install from registry

### Medium Term (Q2 2026)
- [ ] Dependency resolution
- [ ] Lock file support
- [ ] Update command
- [ ] Publish command (manual PR)

### Long Term (Q3-Q4 2026)
- [ ] Package verification
- [ ] Audit command
- [ ] Workspace support
- [ ] Automated publish API
- [ ] Web-based package browser

## Testing Strategy

### Unit Tests
- Registry JSON parsing
- Version resolution
- Checksum verification
- Dependency graph building

### Integration Tests
- Download from mock registry
- Install with dependencies
- Update scenarios
- Conflict resolution

### End-to-End Tests
- Full install workflow
- Publish workflow
- Search and discovery
- Multi-package projects

## Security Considerations

### Package Verification
- SHA-256 checksums mandatory
- GPG signatures for verified publishers
- Malware scanning (future)

### Network Security
- HTTPS only for registry connections
- Certificate validation
- No code execution during download

### Sandboxing (Future)
- Run package tests in sandboxed environment
- Scan for suspicious system calls
- Limit network access during build

---

This roadmap aligns with the Aria language development timeline and provides a clear path for community-driven package ecosystem growth.
