# Change Log

All notable changes to the "aria-lang" extension will be documented in this file.

## [0.2.1.1] - 2026-06

### Added
- **Debugger support** — Integrated aria-dap Debug Adapter Protocol server for debugging Aria programs directly in VS Code
- **Debug launch configurations** — Auto-compile with `-g`, breakpoint support, variable inspection, stepping
- **Configuration settings** — `aria.debugger.path` and `aria.compiler.path` for custom binary locations
- **Launch.json snippets** — Quick configuration for Aria debug sessions

### Changed
- Updated to match Aria compiler v0.2.1.1

## [0.2.1] - 2026-03

### Added
- **Code completion** — Aria keywords (37 with descriptions), built-in types (15), and file-scoped symbols from AST analysis. Trigger characters: `:` and `.`

### Fixed
- **Hover** — Now shows full type signatures for declared symbols (functions with parameter types, structs with fields, enums with variants, traits with methods, constants, variables). Falls back to descriptions for Aria builtin types (int8–int64, float32/64, string, unknown, pass, fail, etc.)
- **Go to definition** — Jumps to actual AST declaration locations instead of naive text search

### Changed
- Updated bundled aria-ls binary with AST-based analysis
- Updated to match Aria compiler v0.2.1

## [0.2.0] - 2026-03

### Changed
- Updated to match Aria compiler v0.2.0
- Version bumps across all components

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
