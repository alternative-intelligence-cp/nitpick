#!/usr/bin/env python3
"""
Migrate void() functions to NIL() in Aria stdlib
"""
import re
import sys
from pathlib import Path

def migrate_function(content):
    """Convert func:name = void(...) to func:name = NIL(...) and add pass(NIL)"""
    
    # Pattern: func:name = void(params) { ... }
    # We need to:
    # 1. Replace void( with NIL(
    # 2. Add pass(NIL); before the closing }
    
    lines = content.split('\n')
    result = []
    in_void_func = False
    brace_depth = 0
    func_start_indent = 0
    
    i = 0
    while i < len(lines):
        line = lines[i]
        
        # Check if this starts a void function (not extern)
        if re.search(r'func:\w+\s*=\s*void\s*\(', line) and 'extern' not in line:
            # Replace void( with NIL(
            line = re.sub(r'=\s*void\s*\(', '= NIL(', line)
            in_void_func = True
            brace_depth = 0
            
            # Check if opening brace is on this line
            if '{' in line:
                brace_depth += line.count('{')
                brace_depth -= line.count('}')
                func_start_indent = len(line) - len(line.lstrip())
        
        # Track braces in void function
        if in_void_func:
            brace_depth += line.count('{')
            brace_depth -= line.count('}')
            
            # If we're closing the function (brace_depth becomes 0)
            if brace_depth == 0 and '}' in line:
                # Check if previous non-empty line is pass(NIL) or fail(...)
                j = len(result) - 1
                while j >= 0 and not result[j].strip():
                    j -= 1
                
                if j >= 0:
                    last_line = result[j].strip()
                    # Check if already has pass() or fail()
                    if not (last_line.startswith('pass(') or last_line.startswith('fail(')):
                        # Add pass(NIL); before closing brace
                        indent = ' ' * (func_start_indent + 4)
                        result.append(f'{indent}pass(NIL);')
                
                in_void_func = False
        
        result.append(line)
        i += 1
    
    return '\n'.join(result)

def process_file(filepath):
    """Process a single Aria file"""
    print(f"Processing {filepath}...")
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Count void functions (excluding extern)
    void_count = len(re.findall(r'func:\w+\s*=\s*void\s*\(', content))
    extern_void_count = len(re.findall(r'extern.*func:\w+\s*=\s*void\s*\(', content))
    actual_void = void_count - extern_void_count
    
    if actual_void == 0:
        print(f"  No void functions to migrate in {filepath}")
        return
    
    # Migrate
    new_content = migrate_function(content)
    
    # Write back
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)
    
    print(f"  ✓ Migrated {actual_void} void functions")

def main():
    repo_path = Path(__file__).parent
    
    # Find all .aria files in lib/std and stdlib
    files = list((repo_path / 'lib/std').glob('*.aria'))
    files += list((repo_path / 'stdlib').glob('*.aria'))
    
    print(f"Found {len(files)} .aria files to check\n")
    
    for filepath in sorted(files):
        process_file(filepath)
    
    print("\n✓ Migration complete!")

if __name__ == '__main__':
    main()
