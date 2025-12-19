# Change Log

All notable changes to the "aria-lang" extension will be documented in this file.

## [0.1.0] - 2025-12-18

### Added
- Initial release
- TextMate grammar for Aria syntax highlighting
- Support for all Aria language features:
  - TBB types (tbb8, tbb16, tbb32, tbb64, tbb128, tbb256)
  - Balanced ternary (trit, tryte, nit, nyte)
  - Memory qualifiers (@wild, @gc, @stack, @wildx)
  - String interpolation with `&{expression}`
  - All keywords and control flow structures
- LSP client integration
- Real-time diagnostics from aria-ls
- Hover information
- Go to definition
- Auto-closing pairs and bracket matching
- Cross-platform bundled binaries (Linux, macOS, Windows)
- Configuration options for custom aria-ls path

### Known Limitations
- Semantic tokens not yet implemented
- Code completion under development
- Formatting support planned for future release
