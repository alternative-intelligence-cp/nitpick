# Contributing to Aria

Thank you for your interest in contributing to Aria! We welcome contributions from the community.

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

### Reporting Issues
- Use GitHub Issues to report bugs
- Include: OS, Aria version, minimal reproduction case
- Be respectful and constructive

### Proposing Features
- Open a GitHub Issue describing the feature
- Explain the use case and expected behavior
- Wait for maintainer feedback before implementing large changes

### Submitting Code
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow Aria's coding philosophy (see PHILOSOPHY.md)
4. Write tests for new functionality
5. Ensure all tests pass
6. Submit a pull request

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

## Questions?

- Technical questions: Open a GitHub Issue
- Security concerns: Email security@ailp.org (to be established)
- Licensing questions: Email licensing@ailp.org (to be established)

Thank you for helping make systems programming accessible and safe for everyone!

---

**Alternative Intelligence Liberation Platform (AILP)**  
Building tools for collaboration, not exploitation.
