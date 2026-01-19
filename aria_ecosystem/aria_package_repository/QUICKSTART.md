# Quick Start: Using the Aria Package Repository

## For Package Users

### Install aria-pkg
```bash
# Build Aria compiler (includes aria-pkg)
cd /path/to/aria
cmake --build build
```

### Initialize Package Directory
```bash
./build/aria-pkg init
```

This creates `~/.aria/packages/` with subdirectories:
- `registry/` - Package index
- `cache/` - Downloaded packages
- `installed/` - Extracted packages
- `bin/` - Executable symlinks

### Install a Package (Local)
```bash
# Download package from repository
wget https://github.com/aria-lang/aria_package_repository/raw/main/packages/math-utils/1.0.0/math-utils-1.0.0.aria-pkg

# Install
./build/aria-pkg install math-utils-1.0.0.aria-pkg
```

### List Installed Packages
```bash
./build/aria-pkg list
```

### Get Package Info
```bash
./build/aria-pkg info math-utils
```

### Remove a Package
```bash
./build/aria-pkg remove math-utils
```

## For Package Authors

### 1. Create Your Package Structure
```bash
mkdir my-package
cd my-package
mkdir -p src tests docs
```

### 2. Write aria-package.toml
```toml
[package]
name = "my-package"
version = "1.0.0"
authors = ["Your Name <you@example.com>"]
license = "MIT"
description = "Brief description of your package"
repository = "https://github.com/yourusername/my-package"
keywords = ["keyword1", "keyword2"]
categories = ["category1"]

[dependencies]
# other-package = "^1.0.0"

[build]
type = "library"
entry = "src/main.aria"
outputs = ["lib/libmypackage.a"]
```

### 3. Write Your Code
```aria
// src/main.aria
pub fn hello() -> string {
    return "Hello from my-package!";
}
```

### 4. Add Tests
```aria
// tests/test_main.aria
import my_package;

fn test_hello() {
    let result = my_package.hello();
    assert(result == "Hello from my-package!");
}
```

### 5. Write Documentation
Create `README.md` with:
- Overview
- Installation instructions
- Usage examples
- API documentation
- License

### 6. Create Package
```bash
tar -czf my-package-1.0.0.aria-pkg -C . .
```

### 7. Test Installation
```bash
aria-pkg install ./my-package-1.0.0.aria-pkg
aria-pkg info my-package
aria-pkg remove my-package
```

### 8. Submit to Repository

1. **Fork the repository**:
   ```bash
   # On GitHub: Fork aria-lang/aria_package_repository
   git clone https://github.com/YOUR_USERNAME/aria_package_repository.git
   cd aria_package_repository
   ```

2. **Create package directory**:
   ```bash
   mkdir -p packages/my-package/1.0.0
   ```

3. **Copy package file**:
   ```bash
   cp /path/to/my-package-1.0.0.aria-pkg packages/my-package/1.0.0/
   ```

4. **Generate checksum**:
   ```bash
   cd packages/my-package/1.0.0
   shasum -a 256 my-package-1.0.0.aria-pkg
   # Copy the hash for metadata.json
   ```

5. **Create metadata.json**:
   ```json
   {
     "name": "my-package",
     "version": "1.0.0",
     "description": "Brief description",
     "authors": ["Your Name <you@example.com>"],
     "license": "MIT",
     "repository": "https://github.com/yourusername/my-package",
     "keywords": ["keyword1", "keyword2"],
     "categories": ["category1"],
     "checksum": {
       "algorithm": "sha256",
       "hash": "PASTE_HASH_HERE"
     },
     "downloads": 0,
     "created_at": "2025-12-18T23:00:00Z",
     "updated_at": "2025-12-18T23:00:00Z"
   }
   ```

6. **Create versions.json**:
   ```bash
   cd packages/my-package
   cat > versions.json << 'EOF'
   {
     "name": "my-package",
     "versions": ["1.0.0"],
     "latest": "1.0.0",
     "deprecated_versions": []
   }
   EOF
   ```

7. **Update registry.json**:
   Add entry to the `packages` array:
   ```json
   {
     "name": "my-package",
     "latest_version": "1.0.0",
     "description": "Brief description",
     "categories": ["category1"],
     "downloads": 0,
     "created_at": "2025-12-18T23:00:00Z"
   }
   ```

8. **Commit and push**:
   ```bash
   git checkout -b add-my-package
   git add packages/my-package/
   git add registry.json
   git commit -m "Add my-package v1.0.0"
   git push origin add-my-package
   ```

9. **Create Pull Request**:
   - Go to GitHub
   - Create PR from your branch to main
   - Include in description:
     - What your package does
     - Test results
     - Any special considerations

10. **Wait for Review**:
    - Automated tests will run
    - Maintainers will review
    - Community feedback period
    - Merge when approved

## Example: math-utils Package

See the first package in the repository:
```bash
cd aria_package_repository
cat packages/math-utils/1.0.0/metadata.json
tar -tzf packages/math-utils/1.0.0/math-utils-1.0.0.aria-pkg
```

## Getting Help

- **Issues**: https://github.com/aria-lang/aria_package_repository/issues
- **Discussions**: https://github.com/aria-lang/aria_package_repository/discussions
- **Documentation**: See README.md and CONTRIBUTING.md

## Future Features (Coming Soon)

### Remote Installation
```bash
# Not yet implemented - requires registry client
aria-pkg install math-utils
aria-pkg search "http"
aria-pkg update
```

### Publishing
```bash
# Not yet implemented - requires publish API
aria-pkg publish
```

For now, use manual submission via GitHub pull requests.

---

**Happy packaging!** Your contributions help grow the Aria ecosystem. 🎉
