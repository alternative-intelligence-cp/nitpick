#!/bin/bash
# Setup script for Aria VS Code extension development

set -e

echo "🚀 Setting up Aria VS Code Extension..."

# Check if in correct directory
if [ ! -f "package.json" ]; then
    echo "❌ Error: Must run from editors/vscode directory"
    exit 1
fi

# Check Node.js
if ! command -v node &> /dev/null; then
    echo "❌ Error: Node.js not found. Please install Node.js 18+ from https://nodejs.org"
    exit 1
fi

echo "📦 Installing dependencies..."
npm install

echo "🔨 Compiling TypeScript..."
npm run compile

# Check for aria-ls binary
if [ ! -f "bin/linux/aria-ls" ]; then
    echo "⚠️  Warning: aria-ls binary not found in bin/linux/"
    echo "   Run from aria root: cp build/aria-ls editors/vscode/bin/linux/"
fi

echo "✅ Setup complete!"
echo ""
echo "Next steps:"
echo "  1. Open this folder in VS Code"
echo "  2. Press F5 to launch Extension Development Host"
echo "  3. Open sample.aria to test syntax highlighting"
echo "  4. Verify LSP features (hover, diagnostics, go-to-definition)"
echo ""
echo "To package:"
echo "  npm run package"
