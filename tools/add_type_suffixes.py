#!/usr/bin/env python3
"""
Add type suffixes to numeric literals in Aria stdlib files.

This script handles common patterns but requires manual review for:
- Complex expressions where type is ambiguous
- Constants that should use specific precision
- Edge cases with negative numbers or special values
"""

import re
import sys
from pathlib import Path

def add_type_suffixes(content: str) -> tuple[str, list[str]]:
    """Add type suffixes to obvious numeric literals. Returns (modified_content, warnings)."""
    warnings = []
    modified = content
    
    # Pattern 1: Integer 0 in assignments (most common - use i32 as default)
    # Matches: int32:total = 0;
    # Avoids: function params, array indices, comparisons
    modified = re.sub(
        r'(int32:\w+\s*=\s*)0(\s*;)',
        r'\g<1>0i32\g<2>',
        modified
    )
    
    # Pattern 2: Integer 1 in assignments to int32
    modified = re.sub(
        r'(int32:\w+\s*=\s*)1(\s*;)',
        r'\g<1>1i32\g<2>',
        modified
    )
    
    # Pattern 3: Integer 0 in assignments to int64
    modified = re.sub(
        r'(int64:\w+\s*=\s*)0(\s*;)',
        r'\g<1>0i64\g<2>',
        modified
    )
    
    # Pattern 4: Integer 1 in assignments to int64
    modified = re.sub(
        r'(int64:\w+\s*=\s*)1(\s*;)',
        r'\g<1>1i64\g<2>',
        modified
    )
    
    # Pattern 5: Small integers in int64 assignments (common for loop starts)
    # Matches: int64:start = 1;
    for num in range(2, 20):  # Cover common small integers
        modified = re.sub(
            rf'(int64:\w+\s*=\s*){num}(\s*;)',
            rf'\g<1>{num}i64\g<2>',
            modified
        )
    
    # Pattern 6: Small integers in int32 assignments
    for num in range(2, 20):
        modified = re.sub(
            rf'(int32:\w+\s*=\s*){num}(\s*;)',
            rf'\g<1>{num}i32\g<2>',
            modified
        )
    
    # Pattern 7: Float 0.0 in flt64 assignments
    modified = re.sub(
        r'(flt64:\w+\s*=\s*)0\.0(\s*;)',
        r'\g<1>0.0f64\g<2>',
        modified
    )
    
    # Pattern 8: Float 1.0 in flt64 assignments
    modified = re.sub(
        r'(flt64:\w+\s*=\s*)1\.0(\s*;)',
        r'\g<1>1.0f64\g<2>',
        modified
    )
    
    # Pattern 9: Float 0.0 in flt32 assignments
    modified = re.sub(
        r'(flt32:\w+\s*=\s*)0\.0(\s*;)',
        r'\g<1>0.0f32\g<2>',
        modified
    )
    
    # Pattern 10: pass() return values with integer literals
    # Matches: pass(0); -> pass(0i32);
    modified = re.sub(
        r'pass\(0\)',
        r'pass(0i32)',
        modified
    )
    
    # Pattern 11: pass() with small positive integers
    for num in range(1, 10):
        modified = re.sub(
            rf'pass\({num}\)',
            rf'pass({num}i32)',
            modified
        )
    
    # Pattern 12: Common boolean-like 0/1 in expressions (context-dependent)
    # Skip these - too risky without type inference
    
    # Pattern 13: Array index literals [0], [1], etc.
    # These should be i64 based on Aria's array indexing
    modified = re.sub(
        r'\[0\]',
        r'[0i64]',
        modified
    )
    
    for num in range(1, 10):
        modified = re.sub(
            rf'\[{num}\]',
            rf'[{num}i64]',
            modified
        )
    
    # Pattern 14: till loop counts - these are i64
    # Matches: till (len, 1) -> till (len, 1i64)
    modified = re.sub(
        r'till\s*\([^,]+,\s*1\s*\)',
        lambda m: m.group(0).replace(', 1)', ', 1i64)'),
        modified
    )
    
    # Detect complex patterns that need manual review
    if re.search(r'\b\d+\.\d+[eE][+-]?\d+\b', content):
        warnings.append("Found scientific notation - may need manual type suffix")
    
    if re.search(r'0x[0-9a-fA-F]+\b', content):
        warnings.append("Found hex literals - may need manual type suffix")
    
    if re.search(r'0b[01]+\b', content):
        warnings.append("Found binary literals - may need manual type suffix")
    
    # Count changes
    original_literals = len(re.findall(r'\b\d+\b', content))
    modified_literals = len(re.findall(r'\b\d+[ui]\d{2,3}\b', modified))
    
    return modified, warnings

def process_file(filepath: Path, dry_run: bool = False) -> bool:
    """Process a single file. Returns True if modifications were made."""
    try:
        content = filepath.read_text(encoding='utf-8')
        modified, warnings = add_type_suffixes(content)
        
        if content == modified:
            print(f"✓ {filepath} - no changes needed")
            return False
        
        print(f"→ {filepath}")
        if warnings:
            for w in warnings:
                print(f"  ⚠ {w}")
        
        if not dry_run:
            # Backup original
            backup = filepath.with_suffix('.aria.bak')
            backup.write_text(content, encoding='utf-8')
            
            # Write modified
            filepath.write_text(modified, encoding='utf-8')
            print(f"  ✓ Modified (backup: {backup.name})")
        else:
            print(f"  [DRY RUN] Would modify this file")
        
        return True
        
    except Exception as e:
        print(f"✗ {filepath}: ERROR - {e}", file=sys.stderr)
        return False

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Add type suffixes to Aria stdlib literals')
    parser.add_argument('paths', nargs='+', help='Files or directories to process')
    parser.add_argument('--dry-run', action='store_true', help='Show what would change without modifying')
    parser.add_argument('--restore', action='store_true', help='Restore from .bak files')
    args = parser.parse_args()
    
    files_to_process = []
    for path_str in args.paths:
        path = Path(path_str)
        if path.is_dir():
            files_to_process.extend(path.rglob('*.aria'))
        elif path.suffix == '.aria':
            files_to_process.append(path)
    
    # Filter out backup directory
    files_to_process = [f for f in files_to_process if '.backups' not in str(f)]
    
    if args.restore:
        for filepath in files_to_process:
            backup = filepath.with_suffix('.aria.bak')
            if backup.exists():
                filepath.write_text(backup.read_text(), encoding='utf-8')
                backup.unlink()
                print(f"✓ Restored {filepath}")
        return
    
    print(f"Processing {len(files_to_process)} files...")
    if args.dry_run:
        print("[DRY RUN MODE - no files will be modified]\n")
    
    modified_count = 0
    for filepath in sorted(files_to_process):
        if process_file(filepath, dry_run=args.dry_run):
            modified_count += 1
    
    print(f"\nSummary: {modified_count}/{len(files_to_process)} files modified")
    if not args.dry_run:
        print("Backups saved as *.aria.bak (use --restore to revert)")

if __name__ == '__main__':
    main()
