#!/usr/bin/env bash

# Compile aria_shell source files into single document
# Collects: include/, src/, CMakeLists.txt, README, etc.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_FILE="$SCRIPT_DIR/aria_shell_source_full.txt"

echo "Compiling aria_shell source files..."
echo "Repository: $REPO_ROOT"
echo "Output: $OUTPUT_FILE"

# Clear output file
> "$OUTPUT_FILE"

# Header
cat >> "$OUTPUT_FILE" << 'EOF'
================================================================================
ARIA_SHELL SOURCE CODE COMPILATION
Generated: $(date)
Repository: aria_shell
================================================================================

EOF

# Function to add file with header
add_file() {
    local file="$1"
    if [[ -f "$file" ]]; then
        echo "" >> "$OUTPUT_FILE"
        echo "================================================================================"  >> "$OUTPUT_FILE"
        echo "FILE: ${file#$REPO_ROOT/}" >> "$OUTPUT_FILE"
        echo "================================================================================"  >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        cat "$file" >> "$OUTPUT_FILE"
        echo "✓ Added: ${file#$REPO_ROOT/}"
    fi
}

# CMake and build configuration
add_file "$REPO_ROOT/CMakeLists.txt"

# Documentation
add_file "$REPO_ROOT/README.md"
add_file "$REPO_ROOT/LICENSE"

# Headers from include/
if [[ -d "$REPO_ROOT/include" ]]; then
    while IFS= read -r -d '' file; do
        add_file "$file"
    done < <(find "$REPO_ROOT/include" -type f \( -name "*.h" -o -name "*.hpp" \) -print0 | sort -z)
fi

# Source files from src/
if [[ -d "$REPO_ROOT/src" ]]; then
    while IFS= read -r -d '' file; do
        add_file "$file"
    done < <(find "$REPO_ROOT/src" -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.cc" \) -print0 | sort -z)
fi

# Shell built-ins and commands
if [[ -d "$REPO_ROOT/builtins" ]]; then
    while IFS= read -r -d '' file; do
        add_file "$file"
    done < <(find "$REPO_ROOT/builtins" -type f \( -name "*.c" -o -name "*.cpp" \) -print0 | sort -z)
fi

echo ""
echo "Compilation complete!"
echo "Output: $OUTPUT_FILE"
echo "Size: $(wc -l < "$OUTPUT_FILE") lines"

