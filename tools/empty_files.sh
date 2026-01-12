#!/bin/bash

# 1. Set the default target to the directory where this script is physically located
#    (resolving the absolute path to ensure safety)
TARGET_DIR="$(cd "$(dirname "$0")" && pwd)"

# 2. Parse command line arguments to check for the -dir flag
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -dir)
            if [ -n "$2" ]; then
                TARGET_DIR="$2"
                shift 2 # Move past the flag and the value
            else
                echo "Error: Argument for -dir is missing."
                exit 1
            fi
            ;;
        *)
            echo "Error: Unknown parameter passed: $1"
            echo "Usage: $0 [-dir /path/to/directory]"
            exit 1
            ;;
    esac
done

# 3. Check if the directory exists
if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Directory '$TARGET_DIR' does not exist."
    exit 1
fi

echo "Targeting directory: $TARGET_DIR"

# 4. Find and empty files
#    -maxdepth 1: Only looks in the current folder, not subfolders
#    -type f: Only looks for files (not directories)
#    -name "*.txt": Only targets text files (Modify this if you want all files)
count=0
find "$TARGET_DIR" -maxdepth 1 -type f -name "*.txt" | while read -r file; do
    > "$file"
    echo "Emptied: $file"
    ((count++))
done

echo "Operation complete."
