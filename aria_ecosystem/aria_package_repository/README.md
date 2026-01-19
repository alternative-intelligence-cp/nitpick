# Aria Package Repository

Official community repository for verified Aria packages.

## Overview

This repository serves as the central registry for Aria language packages. Each package is verified for quality, security, and compatibility before being added to this repository.

## Repository Structure

```
aria_package_repository/
├── packages/              # Package metadata and .aria-pkg files
│   ├── math-utils/
│   │   ├── 1.0.0/
│   │   │   ├── math-utils-1.0.0.aria-pkg
│   │   │   └── metadata.json
│   │   └── versions.json
│   └── ...
├── registry.json          # Master package index
├── categories.json        # Package categories
└── verified_publishers/   # Trusted package publishers
```

## Package Structure

Each package directory contains:
- **versions.json**: List of all available versions
- **Version directories**: One per version with:
  - `.aria-pkg` file: The actual package tarball
  - `metadata.json`: Package metadata, checksums, signatures

## Adding a Package

### Prerequisites
- Package must follow [Aria Package Format Specification](https://github.com/aria-lang/aria/blob/main/docs/package_format.md)
- All tests must pass
- Documentation must be complete
- License must be compatible (MIT, Apache-2.0, BSD, etc.)

### Submission Process

1. **Create your package**:
   ```bash
   cd your-package/
   # Ensure aria-package.toml is complete
   tar -czf package-name-version.aria-pkg -C . .
   ```

2. **Test installation**:
   ```bash
   aria-pkg install ./package-name-version.aria-pkg
   aria-pkg info package-name
   ```

3. **Submit for review**:
   - Fork this repository
   - Add your package to `packages/package-name/version/`
   - Update `registry.json`
   - Create pull request with:
     - Package description
     - Test results
     - Documentation link
     - License information

4. **Review process**:
   - Automated tests run
   - Manual code review
   - Security scan
   - Documentation review
   - Community feedback period (7 days)

5. **Approval**:
   - Package merged to main branch
   - Available via `aria-pkg search`
   - Listed on package registry website

## Package Guidelines

### Naming
- 3-64 characters
- Lowercase only
- Alphanumeric plus hyphen and underscore
- Must start with a letter
- Descriptive and unique

### Versioning
- Semantic versioning (MAJOR.MINOR.PATCH)
- Breaking changes require major version bump
- New features require minor version bump
- Bug fixes require patch version bump

### Quality Standards
- All functions documented with doc comments
- Comprehensive test coverage (>80%)
- Examples provided
- README with usage instructions
- No compiler warnings
- No known security vulnerabilities

### Documentation
Each package must include:
- README.md with overview and examples
- API documentation (generated with `aria doc`)
- CHANGELOG.md with version history
- LICENSE file

### Dependencies
- List all dependencies in aria-package.toml
- Use semantic version constraints
- Minimize dependency count
- Avoid circular dependencies

## Package Categories

Packages are organized into categories:
- **std-extensions**: Standard library extensions
- **data-structures**: Advanced data structures
- **algorithms**: Algorithm implementations
- **networking**: Network protocols and utilities
- **web**: Web frameworks and tools
- **databases**: Database drivers and ORMs
- **cryptography**: Cryptographic utilities
- **math**: Mathematical functions and libraries
- **graphics**: Graphics and visualization
- **audio**: Audio processing
- **testing**: Testing frameworks and tools
- **development**: Development tools and utilities

## Verified Publishers

Trusted publishers can publish packages with expedited review:
- Aria core team
- Established open source organizations
- Individual developers with track record

To become a verified publisher:
1. Submit 3+ high-quality packages
2. Maintain packages for 6+ months
3. Active community participation
4. Apply for verified status

## Security

### Reporting Vulnerabilities
If you discover a security vulnerability in a package:
1. **DO NOT** open a public issue
2. Email security@aria-lang.org
3. Include package name, version, and details
4. We'll respond within 48 hours

### Package Signing
All packages are signed with GPG keys:
- Verify signatures: `aria-pkg verify package-name`
- Publisher keys in `verified_publishers/`

### Checksums
Each package includes SHA-256 checksum in metadata.json:
```json
{
  "checksum": {
    "algorithm": "sha256",
    "hash": "abc123..."
  }
}
```

## Future Enhancements

### Planned Features
- [ ] Web-based package browser
- [ ] Automated testing pipeline
- [ ] Package statistics and analytics
- [ ] Dependency visualization
- [ ] Package search and filtering
- [ ] Version compatibility matrix
- [ ] Package quality scores
- [ ] Download statistics
- [ ] Community ratings and reviews

### Integration with aria-pkg
Future versions of `aria-pkg` will support:
```bash
# Search for packages
aria-pkg search "http client"

# Install from registry
aria-pkg install http-client

# Install specific version
aria-pkg install http-client@1.2.3

# Update all packages
aria-pkg update

# Publish to registry
aria-pkg publish

# Show package statistics
aria-pkg stats http-client
```

## Contributing

We welcome contributions! See [CONTRIBUTING.md](CONTRIBUTING.md) for details.

### Ways to Contribute
- Submit packages
- Review pull requests
- Improve documentation
- Report issues
- Suggest enhancements
- Maintain existing packages

## Community

- **Discord**: https://discord.gg/aria-lang
- **Forum**: https://forum.aria-lang.org
- **GitHub**: https://github.com/aria-lang/aria
- **Website**: https://aria-lang.org

## License

This repository structure and documentation are licensed under MIT.
Individual packages have their own licenses as specified in their metadata.

## Acknowledgments

Thank you to all package authors and contributors who are building the Aria ecosystem!

---

**Note**: This is an early-stage repository. Structure and processes will evolve as the Aria community grows. Feedback and suggestions are welcome!
