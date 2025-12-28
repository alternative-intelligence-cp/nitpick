#!/usr/bin/env bash
set -e

# Compile ALL aria lang source code (.c, .h, .cpp, .hpp, .aria) from key directories
# Covers: src/, include/, lib/, stdlib/, third_party/, vendor/, editors/
# Excludes: tests/, build/, docs/, .git/, CMakeFiles/, examples/
# Output: aria_source_full.txt for Gemini Deep Research

REPO_ROOT="/home/randy/._____RANDY_____/REPOS/aria"
OUTPUT_FILE="$REPO_ROOT/docs/gemini/research_context/aria_source_full.txt"

echo "=================================================="
echo "Aria Language Source Code Compilation"
echo "=================================================="
echo "Repository: $REPO_ROOT"
echo "Output: $OUTPUT_FILE"
echo ""

# Create/truncate output file
> "$OUTPUT_FILE"

# Write header
{
    echo "=================================================="
    echo "ARIA LANGUAGE SOURCE CODE COMPILATION"
    echo "=================================================="
    echo "Generated: $(date)"
    echo "Repository: $REPO_ROOT"
    echo ""
    echo "Included Directories:"
    echo "  - src/          (compiler source)"
    echo "  - include/      (compiler headers)"
    echo "  - lib/          (standard library .aria files)"
    echo "  - stdlib/       (runtime C++ stdlib)"
    echo "  - third_party/  (vendored dependencies)"
    echo "  - vendor/       (vendored dependencies)"
    echo "  - editors/      (editor integrations)"
    echo ""
    echo "Excluded Directories:"
    echo "  - tests/        (test files - 447 files excluded)"
    echo "  - build/        (build artifacts)"
    echo "  - docs/         (documentation)"
    echo "  - examples/     (example programs)"
    echo "  - .git/         (git metadata)"
    echo ""
    echo "File Types: .c, .h, .cpp, .hpp, .aria, CMakeLists.txt"
    echo "=================================================="
    echo ""
} >> "$OUTPUT_FILE"

# Function to add a file to the compilation
add_file() {
    local file="$1"
    local rel_path="${file#$REPO_ROOT/}"
    
    echo "" >> "$OUTPUT_FILE"
    echo "========================================" >> "$OUTPUT_FILE"
    echo "FILE: $rel_path" >> "$OUTPUT_FILE"
    echo "========================================" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
    cat "$file" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
}

# Count files
count_files() {
    find "$REPO_ROOT" -type f \( \
        -name "*.c" -o -name "*.h" -o \
        -name "*.cpp" -o -name "*.hpp" -o \
        -name "*.aria" -o -name "CMakeLists.txt" \
    \) \
        -path "*/src/*" -o -path "*/include/*" -o \
        -path "*/lib/*" -o -path "*/stdlib/*" -o \
        -path "*/third_party/*" -o -path "*/vendor/*" -o \
        -path "*/editors/*" -o \
        \( -name "CMakeLists.txt" ! -path "*/build/*" ! -path "*/tests/*" ! -path "*/.git/*" \) \
    2>/dev/null | wc -l
}

echo "Scanning source directories..."
FILE_COUNT=$(count_files)
echo "Found $FILE_COUNT source files to compile"
echo ""

# Process files from specific directories
# This ensures we get ONLY the directories we want

DIRS_TO_SCAN=(
    "$REPO_ROOT/src"
    "$REPO_ROOT/include"
    "$REPO_ROOT/lib"
    "$REPO_ROOT/stdlib"
    "$REPO_ROOT/third_party"
    "$REPO_ROOT/vendor"
    "$REPO_ROOT/editors"
)

echo "Processing source files by directory..."
echo ""

for dir in "${DIRS_TO_SCAN[@]}"; do
    if [ ! -d "$dir" ]; then
        echo "  [SKIP] Directory not found: ${dir#$REPO_ROOT/}"
        continue
    fi
    
    dir_name="${dir#$REPO_ROOT/}"
    echo "  [SCAN] $dir_name"
    
    # Find all source files in this directory and process them
    find "$dir" -type f \( \
        -name "*.h" -o -name "*.hpp" -o \
        -name "*.c" -o -name "*.cpp" -o -name "*.cc" -o \
        -name "*.aria" \
    \) \
        ! -path "*/build/*" \
        ! -path "*/.git/*" \
        ! -path "*/CMakeFiles/*" \
        ! -path "*/node_modules/*" \
        2>/dev/null | sort | while IFS= read -r file; do
        add_file "$file"
        echo "    + ${file#$REPO_ROOT/}"
    done
done

echo ""
echo "Processing CMakeLists.txt files..."
find "$REPO_ROOT" -type f -name "CMakeLists.txt" \
    ! -path "*/build/*" \
    ! -path "*/tests/*" \
    ! -path "*/.git/*" \
    ! -path "*/examples/*" \
    ! -path "*/archive/*" \
    | sort | while read -r file; do
    add_file "$file"
    echo "    + ${file#$REPO_ROOT/}"
done

echo ""
{
    echo ""
    echo "=================================================="
    echo "END OF COMPILATION"
    echo "=================================================="
} >> "$OUTPUT_FILE"

echo ""
echo "=================================================="
echo "Compilation Complete!"
echo "=================================================="
echo "Output file: $OUTPUT_FILE"
wc -l "$OUTPUT_FILE"
ls -lh "$OUTPUT_FILE"
echo ""
