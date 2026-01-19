#!/usr/bin/env bash
set -e

# Compile all Aria Shell research documentation into a single file
# Converts .md to .txt for Gemini compatibility
# Auto-discovers new files on each run

DOCS_ROOT="/home/randy/._____RANDY_____/REPOS/aria_shell/docs"
OUTPUT_FILE="$DOCS_ROOT/aria_shell_research_full.txt"

echo "Compiling Aria Shell research documentation..."
echo "Output: $OUTPUT_FILE"

# Create/truncate output file
> "$OUTPUT_FILE"

echo "================================" >> "$OUTPUT_FILE"
echo "ARIA SHELL RESEARCH COMPILATION" >> "$OUTPUT_FILE"
echo "Generated: $(date)" >> "$OUTPUT_FILE"
echo "Source: $DOCS_ROOT" >> "$OUTPUT_FILE"
echo "================================" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Convert .md to .txt if no .txt version exists
echo "Converting .md files to .txt where needed..."
find "$DOCS_ROOT" -type f -name "*.md" \
    ! -path "*/build/*" \
    ! -path "*/.git/*" \
    ! -name "*test*" \
    ! -name "*status*" \
    ! -name "*STATUS*" \
    ! -name "*Test*" \
    | while read -r md_file; do
    txt_file="${md_file%.md}.txt"
    if [ ! -f "$txt_file" ]; then
        echo "  Creating: ${txt_file#$DOCS_ROOT/}"
        cp "$md_file" "$txt_file"
    fi
done

# Function to add a file to compilation
add_file() {
    local file="$1"
    local rel_path="${file#$DOCS_ROOT/}"
    
    echo "" >> "$OUTPUT_FILE"
    echo "========================================" >> "$OUTPUT_FILE"
    echo "FILE: $rel_path" >> "$OUTPUT_FILE"
    echo "========================================" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
    cat "$file" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
}

# Compile all .txt files in rational order
echo "Compiling research files..."

# 1. Overview/Architecture docs first
find "$DOCS_ROOT" -maxdepth 2 -type f \( -name "*.txt" -o -name "*.md" \) \
    \( -name "*overview*" -o -name "*architecture*" -o -name "*ARCHITECTURE*" -o -name "*README*" \) \
    ! -name "*test*" ! -name "*status*" \
    | sort | while read -r file; do
    add_file "$file"
    echo "  Added: ${file#$DOCS_ROOT/}"
done

# 2. All other documentation files
find "$DOCS_ROOT" -type f \( -name "*.txt" -o -name "*.md" \) \
    ! -path "*/build/*" \
    ! -path "*/.git/*" \
    ! -name "*test*" \
    ! -name "*status*" \
    ! -name "*STATUS*" \
    ! -name "aria_shell_research_full.txt" \
    | sort | while read -r file; do
    # Check if already added
    if ! grep -q "FILE: ${file#$DOCS_ROOT/}" "$OUTPUT_FILE" 2>/dev/null; then
        add_file "$file"
        echo "  Added: ${file#$DOCS_ROOT/}"
    fi
done

echo "" >> "$OUTPUT_FILE"
echo "================================" >> "$OUTPUT_FILE"
echo "END OF RESEARCH COMPILATION" >> "$OUTPUT_FILE"
echo "================================" >> "$OUTPUT_FILE"

echo "Done! Output written to: $OUTPUT_FILE"
wc -l "$OUTPUT_FILE"
 
