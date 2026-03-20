#!/usr/bin/env python3
"""
Comprehensive corpus cleaner for Aria V4.2 training data.

Fixes:
  1. 'return expr;' → 'pass(expr);' in code lines (not comments)
  2. Drops examples with heavy non-Aria contamination:
     - match + => (Rust pattern matching)
     - fn keyword as function declaration
     - let keyword for variable declaration
     - Some/None option types
"""

import json
import re
import sys
from pathlib import Path
from collections import Counter


def is_code_line(line: str) -> bool:
    """Check if a line is actual code (not comment/doc/markdown)."""
    stripped = line.strip()
    if not stripped:
        return False
    if stripped.startswith('//') or stripped.startswith('/*') or stripped.startswith('*'):
        return False
    if stripped.startswith('#') or stripped.startswith('|') or stripped.startswith('-'):
        return False
    if stripped.startswith('```'):
        return False
    return True


def fix_return_to_pass(content: str) -> str:
    """Replace 'return expr;' with 'pass(expr);' in code lines."""
    lines = content.split('\n')
    fixed = []
    for line in lines:
        if is_code_line(line) and re.search(r'\breturn\b', line):
            # Replace 'return expr;' with 'pass(expr);'
            # Handle: return 0; return x; return x + y; return func();
            line = re.sub(
                r'\breturn\s+([^;]+);',
                r'pass(\1);',
                line
            )
            # Handle bare 'return;' → 'pass(NIL);'
            line = re.sub(r'\breturn\s*;', 'pass(NIL);', line)
        fixed.append(line)
    return '\n'.join(fixed)


def is_heavily_contaminated(output: str) -> list:
    """Check if output has non-fixable contamination patterns."""
    issues = []

    # Extract code-only lines
    code_lines = [l for l in output.split('\n') if is_code_line(l)]
    code_text = '\n'.join(code_lines)

    # Rust match + => pattern
    if re.search(r'\bmatch\b\s+\w', code_text) and re.search(r'\w\s*=>\s*[{\w]', code_text):
        issues.append('match+=> (Rust pattern matching)')

    # fn as function declaration
    if re.search(r'\bfn\b\s+\w+\s*[\(<]', code_text):
        issues.append('fn keyword (not func:)')

    # let for variable declaration
    if re.search(r'\blet\b\s+(mut\s+)?\w+', code_text):
        issues.append('let keyword (not type:name)')

    # Some/None option types
    if re.search(r'\bSome\s*\(', code_text) or re.search(r'\bNone\b', code_text):
        issues.append('Some/None (not Aria optional)')

    return issues


def process(input_path: str, output_path: str, dry_run: bool = False):
    with open(input_path) as f:
        lines = f.readlines()

    kept = []
    dropped = []
    return_fixed = 0
    drop_reasons = Counter()

    for i, line in enumerate(lines):
        obj = json.loads(line)
        output = obj.get('output', '')

        # Check for heavy contamination (drop these)
        issues = is_heavily_contaminated(output)
        if issues:
            dropped.append((i, issues))
            for iss in issues:
                drop_reasons[iss] += 1
            continue

        # Fix simple return → pass()
        if re.search(r'\breturn\b', output) and any(is_code_line(l) and 'return' in l for l in output.split('\n')):
            obj['output'] = fix_return_to_pass(output)
            return_fixed += 1

        # Also fix instruction field if it has return
        inst = obj.get('instruction', '')
        if re.search(r'\breturn\b', inst):
            obj['instruction'] = fix_return_to_pass(inst)

        kept.append(json.dumps(obj, ensure_ascii=False) + '\n')

    print(f"Input: {len(lines)} examples")
    print(f"Dropped: {len(dropped)} (heavy contamination)")
    print(f"Kept: {len(kept)}")
    print(f"return→pass() fixed: {return_fixed}")
    print()
    print("Drop reasons:")
    for reason, count in drop_reasons.most_common():
        print(f"  {reason}: {count}")

    if not dry_run:
        with open(output_path, 'w') as f:
            f.writelines(kept)
        print(f"\nWritten to: {output_path}")

        # Verify
        with open(output_path) as f:
            check = f.read()
        remaining_return = len(re.findall(r'\breturn\s+\w', check))
        remaining_fn = len(re.findall(r'\bfn\b\s+\w+\s*[\(<]', check))
        remaining_match = len(re.findall(r'\bmatch\b\s+\w+\s*\{', check))
        print(f"\nRemaining in output:")
        print(f"  return: {remaining_return}")
        print(f"  fn: {remaining_fn}")
        print(f"  match: {remaining_match}")
    else:
        print("\n(dry run)")


if __name__ == '__main__':
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument('input')
    p.add_argument('-o', '--output', required=True)
    p.add_argument('--dry-run', action='store_true')
    args = p.parse_args()
    process(args.input, args.output, args.dry_run)
