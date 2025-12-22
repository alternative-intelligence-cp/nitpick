#!/usr/bin/env bash
set -e

#need to compile ALL aria lang source code (.c, .h, .cpp, .hpp, etc) but no documents that are in  /home/randy/._____RANDY_____/REPOS/aria
#script should auto find any new additions to source whenever it is run
#output should be aria_source_full.txt

REPO_ROOT="/home/randy/._____RANDY_____/REPOS/aria"
OUTPUT_FILE="$REPO_ROOT/docs/gemini/research_context/aria_source_full.txt"

echo "Compiling Aria source code..."
echo "Output: $OUTPUT_FILE"

# Create/truncate output file
> "$OUTPUT_FILE"

echo "================================" >> "$OUTPUT_FILE"
echo "ARIA LANGUAGE SOURCE CODE COMPILATION" >> "$OUTPUT_FILE"
echo "Generated: $(date)" >> "$OUTPUT_FILE"
echo "Repository: $REPO_ROOT" >> "$OUTPUT_FILE"
echo "================================" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

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

# Find all source files, excluding:
# - build/ directory
# - docs/ directory (no documents)
# - tests/ directory (test files)
# - .git/
# - CMake generated files

echo "Finding source files..."

# Headers first (.h, .hpp)
echo "Processing header files..."
find "$REPO_ROOT" -type f \( -name "*.h" -o -name "*.hpp" \) \
    ! -path "*/build/*" \
    ! -path "*/docs/*" \
    ! -path "*/.git/*" \
    ! -path "*/CMakeFiles/*" \
    | sort | while read -r file; do
    add_file "$file"
    echo "  Added: ${file#$REPO_ROOT/}"
done

# Source files (.c, .cpp, .cc)
echo "Processing source files..."
find "$REPO_ROOT" -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.cc" \) \
    ! -path "*/build/*" \
    ! -path "*/docs/*" \
    ! -path "*/.git/*" \
    ! -path "*/CMakeFiles/*" \
    ! -path "*/tests/*" \
    | sort | while read -r file; do
    add_file "$file"
    echo "  Added: ${file#$REPO_ROOT/}"
done

# Aria language files (.aria)
echo "Processing .aria files..."
find "$REPO_ROOT" -type f -name "*.aria" \
    ! -path "*/build/*" \
    ! -path "*/docs/*" \
    ! -path "*/.git/*" \
    ! -path "*/tests/*" \
    | sort | while read -r file; do
    add_file "$file"
    echo "  Added: ${file#$REPO_ROOT/}"
done

# CMakeLists.txt files
echo "Processing CMake files..."
find "$REPO_ROOT" -type f -name "CMakeLists.txt" \
    ! -path "*/build/*" \
    ! -path "*/.git/*" \
    | sort | while read -r file; do
    add_file "$file"
    echo "  Added: ${file#$REPO_ROOT/}"
done

echo "" >> "$OUTPUT_FILE"
echo "================================" >> "$OUTPUT_FILE"
echo "END OF COMPILATION" >> "$OUTPUT_FILE"
echo "================================" >> "$OUTPUT_FILE"

echo "Done! Output written to: $OUTPUT_FILE"
wc -l "$OUTPUT_FILE"