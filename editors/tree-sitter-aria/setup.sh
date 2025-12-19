#!/bin/bash
# Setup script for Tree-sitter Aria grammar

set -e

echo "Setting up Tree-sitter Aria grammar..."

# Check for npm
if ! command -v npm &> /dev/null; then
    echo "Error: npm not found. Please install Node.js and npm."
    exit 1
fi

# Check for tree-sitter CLI
if ! command -v tree-sitter &> /dev/null; then
    echo "Installing tree-sitter-cli..."
    npm install -g tree-sitter-cli
fi

# Install dependencies
echo "Installing dependencies..."
npm install

# Generate parser
echo "Generating parser..."
tree-sitter generate

# Run tests
echo "Running tests..."
tree-sitter test

echo ""
echo "✓ Tree-sitter Aria grammar setup complete!"
echo ""
echo "Next steps:"
echo "  - For Neovim: Follow installation instructions in README.md"
echo "  - For Helix: Run 'tree-sitter build -o libtree-sitter-aria.so'"
echo "  - Test parsing: tree-sitter parse ../vscode/sample.aria"
