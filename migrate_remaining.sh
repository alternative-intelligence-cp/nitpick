#!/bin/bash
# Quick migration script for remaining void functions

files=(
  "lib/std/time.aria"
  "lib/std/path.aria"
  "lib/std/async.aria"  
  "lib/std/process.aria"
  "stdlib/dbug.aria"
  "lib/std/core/memory.aria"
  "lib/std/core/arena.aria"
)

for file in "${files[@]}"; do
  if [ -f "$file" ]; then
    echo "Processing $file..."
    
    # Backup
    cp "$file" "$file.bak"
    
    # Replace void( with NIL( for non-extern functions
    # This uses perl for better multi-line handling
    perl -i -pe '
      # Replace func:name = void( with func:name = NIL( (but not in extern blocks)
      s/(pub\s+)?func:\w+\s*=\s*void\s*\(/${1}func:$2 = NIL(/g unless /extern/;
    ' "$file"
    
    echo "  ✓ Updated signatures"
  fi
done

echo "Done! Please review and add pass(NIL) statements manually where needed."
