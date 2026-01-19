#!/bin/bash

# Script to compile all Gemini research .txt files into a single document
# Usage: ./compile.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_FILE="$SCRIPT_DIR/aria_utils_research_full.txt"
CONTEXT_DIR="$SCRIPT_DIR/research/gemini/context"
RESPONSES_DIR="$SCRIPT_DIR/research/gemini/responses"

# Clear output file if it exists
> "$OUTPUT_FILE"

echo "==================================================================" >> "$OUTPUT_FILE"
echo "ARIA UTILS RESEARCH COMPILATION" >> "$OUTPUT_FILE"
echo "Generated: $(date)" >> "$OUTPUT_FILE"
echo "==================================================================" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Process context files
if [ -d "$CONTEXT_DIR" ]; then
    CONTEXT_FILES=$(find "$CONTEXT_DIR" -name "*.txt" -type f | sort)
    if [ -n "$CONTEXT_FILES" ]; then
        echo "==================================================================" >> "$OUTPUT_FILE"
        echo "CONTEXT FILES" >> "$OUTPUT_FILE"
        echo "==================================================================" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        
        while IFS= read -r file; do
            echo "──────────────────────────────────────────────────────────────────" >> "$OUTPUT_FILE"
            echo "FILE: $(basename "$file")" >> "$OUTPUT_FILE"
            echo "PATH: $file" >> "$OUTPUT_FILE"
            echo "──────────────────────────────────────────────────────────────────" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
            cat "$file" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
        done <<< "$CONTEXT_FILES"
    fi
fi

# Process response files
if [ -d "$RESPONSES_DIR" ]; then
    RESPONSE_FILES=$(find "$RESPONSES_DIR" -name "*.txt" -type f | sort)
    if [ -n "$RESPONSE_FILES" ]; then
        echo "==================================================================" >> "$OUTPUT_FILE"
        echo "UTILITY SPECIFICATIONS" >> "$OUTPUT_FILE"
        echo "==================================================================" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        
        while IFS= read -r file; do
            echo "──────────────────────────────────────────────────────────────────" >> "$OUTPUT_FILE"
            echo "FILE: $(basename "$file")" >> "$OUTPUT_FILE"
            echo "PATH: $file" >> "$OUTPUT_FILE"
            echo "──────────────────────────────────────────────────────────────────" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
            cat "$file" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
        done <<< "$RESPONSE_FILES"
    fi
fi

echo "==================================================================" >> "$OUTPUT_FILE"
echo "END OF COMPILATION" >> "$OUTPUT_FILE"
echo "==================================================================" >> "$OUTPUT_FILE"

echo "✓ Compilation complete: $OUTPUT_FILE"
echo "  Total size: $(wc -c < "$OUTPUT_FILE") bytes"
echo "  Total lines: $(wc -l < "$OUTPUT_FILE") lines"

