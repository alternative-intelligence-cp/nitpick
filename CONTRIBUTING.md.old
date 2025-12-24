# Contributing to Aria

Thank you for your interest in contributing to Aria! We welcome contributions from developers of all skill levels - whether you're a compiler expert or learning systems programming, there's a place for you here.

## Licensing

By contributing to Aria, you agree that your contributions will be licensed under the same dual-license structure as the project:
- **AGPL-3.0** for open-source use
- **Commercial License** for proprietary use (revenue supports project development)

This ensures your work remains freely available while also supporting sustainable development.

## Contributor License Agreement (CLA)

By submitting a pull request, you affirm that:
1. You have the right to contribute the code
2. Your contribution can be distributed under both AGPL-3.0 and commercial licenses
3. You retain copyright to your work but grant AILP the right to license it under both models

This dual-licensing model allows us to:
- Keep Aria free for students, researchers, and open-source projects
- Fund ongoing development through commercial licenses
- Support AILP schools and educational initiatives

## How to Contribute

### 🐛 Reporting Issues
- Use GitHub Issues to report bugs
- Include: OS, Aria version, minimal reproduction case
- Check existing issues first to avoid duplicates
- Be respectful and constructive

### 💡 Proposing Features
- Open a GitHub Issue describing the feature
- Explain the use case and expected behavior
- Discuss with maintainers before implementing large changes
- Reference relevant language design principles

### 📝 Improving Documentation
- Fix typos or unclear explanations
- Add examples to existing documentation
- Create tutorials or guides
- Update README or examples when features change

### 🎯 Good First Issues
Looking to get started? Consider:
- Adding more example programs
- Improving error messages
- Writing test cases for existing features
- Documenting undocumented features
- Fixing compiler warnings

### 💻 Submitting Code
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow Aria's coding philosophy (explicit over implicit)
4. Write tests for new functionality
5. Ensure all tests pass (`./test.sh`)
6. Update documentation as needed
7. Submit a pull request with clear description

### Code Standards
- **Explicit over implicit**: Follow Aria's design philosophy
- **No shortcuts**: Maintain mandatory syntax requirements
- **Construction-grade safety**: Write code someone's life might depend on
- **Documentation**: Explain WHY, not just WHAT

## Philosophy Alignment

Aria is designed with intentional constraints:
- Explicit syntax (mandatory semicolons, braces)
- Result types (no bypassing error handling)
- Hybrid memory (gc/wild/stack)

If you find something "verbose" or "redundant" - it's probably intentional. Read the specification before proposing changes that "simplify" or "modernize" features.

## Development Setup

### Building from Source
```bash
cd aria
mkdir -p build && cd build
cmake ..
make
```

### Running Tests
```bash
./test.sh                    # Run all tests
./build/tests/test_runner    # Run test suite directly
```

### Project Structure
- `src/` - Compiler implementation (lexer, parser, codegen)
- `include/` - Header files
- `lib/std/` - Standard library (Aria code)
- `tests/` - Test suite
- `examples/` - Example programs
- `docs/` - Documentation

## Questions?

- **Technical questions**: Open a GitHub Discussion or Issue
- **Design questions**: Join GitHub Discussions
- **Bug reports**: GitHub Issues
- **Security concerns**: Create a private security advisory
- **Licensing questions**: See [LICENSE.md](LICENSE.md)

Thank you for helping make systems programming accessible and safe for everyone!

---

**Alternative Intelligence Liberation Platform (AILP)**  
Building tools for collaboration, not exploitation.

*See [ACKNOWLEDGMENTS.md](ACKNOWLEDGMENTS.md) for the people who made this possible.*
