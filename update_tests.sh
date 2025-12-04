#!/bin/bash
# Update test files to use * auto-wrap operator

cd /home/randy/._____RANDY_____/REPOS/aria/tests

# Find all .aria files with func: declarations
for file in *.aria; do
    if grep -q "^func:" "$file"; then
        echo "Updating $file..."
        # Add * prefix to function return types (handles int8, int32, int64, void, etc.)
        # Pattern: func:name = TYPE() becomes func:name = *TYPE()
        sed -i 's/^\(func:[a-zA-Z_][a-zA-Z0-9_]* = \)\([a-zA-Z0-9_]\+\)(/\1*\2(/g' "$file"
        
        # Add unwrap operator ? -1 to function calls that assign to variables
        # Pattern: TYPE:var = funcname() becomes TYPE:var = funcname() ? -1
        sed -i 's/^\([ \t]*[a-zA-Z0-9_]\+:[a-zA-Z_][a-zA-Z0-9_]* = [a-zA-Z_][a-zA-Z0-9_]*([^)]*)\);/\1 ? -1;/g' "$file"
        
        # Handle function calls without assignments don't need unwrap
    fi
done

echo "Done!"
