#!/bin/bash
# Add failsafe to .aria files that need it
# Usage: ./add_failsafe.sh <directory>

DIR="${1:-.}"
COUNT_FILE="/tmp/failsafe_count_$$"
echo "0" > "$COUNT_FILE"

FAILSAFE_CODE='
func:failsafe = void(int32:err_code) {
    // Error handling
};'

find "$DIR" -name "*.aria" -type f -print0 | while IFS= read -r -d '' file; do
    # Skip if already has failsafe
    if grep -q "func:failsafe" "$file" 2>/dev/null; then
        continue
    fi
    
    # Skip if no main function (library file)
    if ! grep -q "func:main" "$file" 2>/dev/null; then
        continue
    fi
    
    # Add failsafe at end of file
    echo "$FAILSAFE_CODE" >> "$file"
    COUNT=$(cat "$COUNT_FILE")
    COUNT=$((COUNT + 1))
    echo "$COUNT" > "$COUNT_FILE"
    
    if [ $((COUNT % 50)) -eq 0 ]; then
        echo "Processed $COUNT files..."
    fi
done

FINAL_COUNT=$(cat "$COUNT_FILE")
rm "$COUNT_FILE"

echo ""
echo "✅ Added failsafe to $FINAL_COUNT files"
