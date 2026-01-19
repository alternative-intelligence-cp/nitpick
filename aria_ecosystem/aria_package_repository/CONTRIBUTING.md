# Contributing to Aria Package Repository

Thank you for your interest in contributing to the Aria package ecosystem!

## Getting Started

1. **Set up Aria compiler**:
   ```bash
   git clone https://github.com/aria-lang/aria.git
   cd aria
   cmake -B build
   cmake --build build
   ```

2. **Install aria-pkg**:
   ```bash
   ./build/aria-pkg init
   ```

3. **Fork this repository**:
   ```bash
   git clone https://github.com/YOUR_USERNAME/aria_package_repository.git
   cd aria_package_repository
   ```

## Submitting a Package

### 1. Create Your Package

Create a new directory with your package source:

```
my-package/
├── aria-package.toml
├── README.md
├── LICENSE
├── src/
│   └── main.aria
├── tests/
│   └── test_main.aria
└── docs/
    └── api.md
```

### 2. Write aria-package.toml

```toml
[package]
name = "my-package"
version = "1.0.0"
authors = ["Your Name <you@example.com>"]
license = "MIT"
description = "A brief description of your package"
repository = "https://github.com/yourusername/my-package"
documentation = "https://docs.example.com/my-package"
keywords = ["keyword1", "keyword2"]
categories = ["math", "algorithms"]

[dependencies]
# other-package = "^1.0.0"

[build]
type = "library"
entry = "src/main.aria"
outputs = ["lib/libmypackage.a"]
```

### 3. Write Tests

Create comprehensive tests in `tests/`:

```aria
// tests/test_main.aria
import my_package;

fn test_basic_functionality() {
    let result = my_package.do_something(42);
    assert(result == expected_value);
}
```

### 4. Write Documentation

- **README.md**: Overview, installation, usage examples
- **API docs**: Use doc comments (`///` or `/** */`)
- **Examples**: Provide working code examples

### 5. Create Package Tarball

```bash
cd my-package/
tar -czf my-package-1.0.0.aria-pkg -C . .
```

### 6. Test Installation

```bash
aria-pkg install ./my-package-1.0.0.aria-pkg
aria-pkg info my-package
```

### 7. Create Package Metadata

Create `metadata.json`:

```json
{
  "name": "my-package",
  "version": "1.0.0",
  "description": "A brief description",
  "authors": ["Your Name <you@example.com>"],
  "license": "MIT",
  "repository": "https://github.com/yourusername/my-package",
  "documentation": "https://docs.example.com/my-package",
  "keywords": ["keyword1", "keyword2"],
  "categories": ["math", "algorithms"],
  "checksum": {
    "algorithm": "sha256",
    "hash": "RUN: shasum -a 256 my-package-1.0.0.aria-pkg"
  },
  "downloads": 0,
  "created_at": "2025-12-18T23:00:00Z",
  "updated_at": "2025-12-18T23:00:00Z"
}
```

Calculate checksum:
```bash
shasum -a 256 my-package-1.0.0.aria-pkg
```

### 8. Add to Repository

```bash
# Create package directory
mkdir -p packages/my-package/1.0.0/

# Copy files
cp my-package-1.0.0.aria-pkg packages/my-package/1.0.0/
cp metadata.json packages/my-package/1.0.0/

# Create versions.json
cat > packages/my-package/versions.json << 'EOF'
{
  "name": "my-package",
  "versions": ["1.0.0"],
  "latest": "1.0.0"
}
EOF
```

### 9. Update registry.json

Add your package to the `packages` array in `registry.json`:

```json
{
  "name": "my-package",
  "latest_version": "1.0.0",
  "description": "A brief description",
  "categories": ["math", "algorithms"],
  "downloads": 0,
  "created_at": "2025-12-18T23:00:00Z"
}
```

### 10. Submit Pull Request

```bash
git checkout -b add-my-package
git add packages/my-package/
git add registry.json
git commit -m "Add my-package v1.0.0"
git push origin add-my-package
```

Create pull request with:
- Package description
- Key features
- Test results
- Breaking changes (if updating)

## Package Review Checklist

Your package will be reviewed for:

- [ ] Valid aria-package.toml with all required fields
- [ ] Appropriate package name (3-64 chars, lowercase)
- [ ] Semantic versioning
- [ ] Compatible license (MIT, Apache-2.0, BSD, etc.)
- [ ] README with usage examples
- [ ] Doc comments on public APIs
- [ ] Tests with >80% coverage
- [ ] No compiler warnings
- [ ] No security vulnerabilities
- [ ] Checksum matches .aria-pkg file
- [ ] No malicious code

## Code Standards

### Style Guidelines
- 4 spaces for indentation
- No trailing whitespace
- Unix line endings (LF)
- UTF-8 encoding
- Max line length: 100 characters

### Documentation
- All public functions must have doc comments
- Include examples in doc comments
- Document error conditions
- Document thread safety

### Testing
- Test all public APIs
- Test error conditions
- Test edge cases
- Performance tests for critical code

## Updating Packages

To release a new version:

1. Update version in aria-package.toml
2. Update CHANGELOG.md
3. Create new .aria-pkg tarball
4. Add to `packages/package-name/X.Y.Z/`
5. Update `versions.json`
6. Update `registry.json`
7. Submit pull request

## Best Practices

### Naming
- Use descriptive names
- Avoid generic names (e.g., "utils", "helpers")
- Use hyphens for multiple words (e.g., "http-client")
- Match repository name when possible

### Dependencies
- Minimize dependencies
- Use semantic version constraints
- Document why each dependency is needed
- Keep dependencies up to date

### Versioning
- Start at 1.0.0 for stable APIs
- Use 0.x.y for experimental packages
- Document breaking changes in CHANGELOG
- Follow semantic versioning strictly

### Performance
- Benchmark critical code
- Document performance characteristics
- Provide async alternatives when appropriate
- Consider memory usage

### Security
- Validate all inputs
- Use safe defaults
- Document security considerations
- Follow security best practices

## Questions?

- Open an issue for questions
- Join our Discord for real-time help
- Check existing packages for examples

Thank you for contributing to Aria!
