# Aria VS Code Extension

This directory contains the Visual Studio Code extension for Aria language support.

## Quick Start

```bash
# Install dependencies
npm install

# Compile TypeScript
npm run compile

# Watch mode (auto-recompile)
npm run watch

# Package extension
npm run package
```

## Testing Locally

1. Open this folder in VS Code
2. Press F5 to launch Extension Development Host
3. Open an `.aria` file
4. Verify syntax highlighting and LSP features

## Directory Structure

```
vscode/
├── src/
│   └── extension.ts          # Main extension code
├── syntaxes/
│   └── aria.tmLanguage.json  # TextMate grammar
├── bin/                       # Bundled binaries (copy from build/)
│   ├── linux/aria-ls
│   ├── macos/aria-ls
│   └── windows/aria-ls.exe
├── icons/                     # Extension icons
├── package.json               # Extension manifest
├── language-configuration.json
├── tsconfig.json
└── README.md
```

## Bundling aria-ls

To bundle the language server binary:

```bash
# From aria root
mkdir -p editors/vscode/bin/linux
mkdir -p editors/vscode/bin/macos
mkdir -p editors/vscode/bin/windows

# Linux
cp build/aria-ls editors/vscode/bin/linux/

# macOS (if you have a Mac or cross-compiler)
# cp build/aria-ls editors/vscode/bin/macos/

# Windows (if you have cross-compiler)
# cp build/aria-ls.exe editors/vscode/bin/windows/
```

## Publishing

```bash
# Login to VS Code marketplace
vsce login aria-lang

# Publish
vsce publish
```

See: https://code.visualstudio.com/api/working-with-extensions/publishing-extension
