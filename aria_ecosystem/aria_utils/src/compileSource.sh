#!/bin/bash

# Output file name
OUTPUT_FILE="source_comp_full.txt"

# Target directory (defaults to current directory if not provided)
TARGET_DIR="${1:-.}"

# Check if target directory exists
if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Directory '$TARGET_DIR' does not exist."
    exit 1
fi

# Remove the output file if it already exists to start fresh
if [ -f "$OUTPUT_FILE" ]; then
    rm "$OUTPUT_FILE"
fi

echo "Scanning '$TARGET_DIR' for C/C++ source files..."

# Find command:
# 1. Looks in TARGET_DIR
# 2. -type f: looks for files only
# 3. Matches common C/C++ extensions
# 4. -print0: Handles filenames with spaces correctly
find "$TARGET_DIR" -type f \( \
    -name "*.c" -o \
    -name "*.cpp" -o \
    -name "*.h" -o \
    -name "*.hpp" -o \
    -name "*.cc" -o \
    -name "*.cxx" \
\) -print0 | while IFS= read -r -d '' file; do
    
    # Formatting for the output file
    echo "=========================================" >> "$OUTPUT_FILE"
    echo "FILE: $file" >> "$OUTPUT_FILE"
    echo "=========================================" >> "$OUTPUT_FILE"
    
    # Append the file content
    cat "$file" >> "$OUTPUT_FILE"
    
    # Add a newline at the end to prevent files running into each other
    echo -e "\n" >> "$OUTPUT_FILE"
    
    echo "Processed: $file"
done

echo "Done! All source code compiled into $OUTPUT_FILE"
