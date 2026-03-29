#!/usr/bin/env python3
"""Batch update aria test files for 0.2.46-3 failsafe/main/exit contract.

Changes:
1. failsafe signature → int32(tbb32:err)
2. failsafe body: pass(NIL)/pass(0i32)/pass(0)/pass(err_code) → exit(1)
3. failsafe body: ensure exit(1) exists, add if missing
4. main body: pass(0i32)/pass(0) → exit(0)
"""
import re
import sys
from pathlib import Path


def update_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    original = content

    # =========================================================================
    # Single pass: line-by-line with state tracking for failsafe and main
    # =========================================================================
    lines = content.split('\n')
    result_lines = []
    in_failsafe = False
    in_main = False
    failsafe_depth = 0
    main_depth = 0
    failsafe_has_exit = False

    for line in lines:
        # -----------------------------------------------------------------
        # Detect failsafe declaration (both single-line and multi-line)
        # -----------------------------------------------------------------
        if not in_failsafe and not in_main and re.search(
                r'func:failsafe\s*=\s*\w+\([^)]*\)\s*\{', line):
            indent = re.match(r'^(\s*)', line).group(1)

            # Check if single-line: has closing }; on same line
            if re.search(r'\}\s*;', line):
                # Replace entire line with canonical single-line form
                result_lines.append(indent + 'func:failsafe = int32(tbb32:err) { exit(1); };')
                continue
            else:
                # Multi-line: replace just the declaration
                line = indent + 'func:failsafe = int32(tbb32:err) {'
                in_failsafe = True
                failsafe_depth = 1  # We opened one brace
                failsafe_has_exit = False
                result_lines.append(line)
                continue

        # -----------------------------------------------------------------
        # Inside multi-line failsafe body
        # -----------------------------------------------------------------
        if in_failsafe:
            failsafe_depth += line.count('{') - line.count('}')

            # Replace pass() variants with exit(1)
            line = re.sub(r'pass\s*\(\s*NIL\s*\)', 'exit(1)', line)
            line = re.sub(r'pass\s*\(\s*0i32\s*\)', 'exit(1)', line)
            line = re.sub(r'pass\s*\(\s*0\s*\)', 'exit(1)', line)
            line = re.sub(r'pass\s*\(\s*err_code\s*\)', 'exit(1)', line)
            line = re.sub(r'pass\s*\(\s*err\s*\)', 'exit(1)', line)
            line = re.sub(r'pass\s*\(\s*code\s*\)', 'exit(1)', line)

            if 'exit(' in line:
                failsafe_has_exit = True

            if failsafe_depth <= 0:
                in_failsafe = False
                # If no exit() was found, insert one before closing };
                if not failsafe_has_exit:
                    close_indent = re.match(r'^(\s*)', line).group(1)
                    result_lines.append(close_indent + '    exit(1);')
                result_lines.append(line)
                continue

            result_lines.append(line)
            continue

        # -----------------------------------------------------------------
        # Detect main declaration
        # -----------------------------------------------------------------
        if not in_main and not in_failsafe and re.search(
                r'func:main\s*=\s*int32\s*\(\s*\)\s*\{', line):
            in_main = True
            main_depth = line.count('{') - line.count('}')
            # Single-line main: apply replacements on this line, don't track
            if main_depth <= 0:
                line = re.sub(r'pass\s*\(\s*0i32\s*\)', 'exit(0)', line)
                line = re.sub(r'pass\s*\(\s*0\s*\)', 'exit(0)', line)
                in_main = False
            result_lines.append(line)
            continue

        # -----------------------------------------------------------------
        # Inside main body
        # -----------------------------------------------------------------
        if in_main:
            main_depth += line.count('{') - line.count('}')

            # Replace pass(0i32) and pass(0) with exit(0) in main
            line = re.sub(r'pass\s*\(\s*0i32\s*\)', 'exit(0)', line)
            line = re.sub(r'pass\s*\(\s*0\s*\)', 'exit(0)', line)

            if main_depth <= 0:
                in_main = False

        result_lines.append(line)

    content = '\n'.join(result_lines)

    if content != original:
        with open(filepath, 'w') as f:
            f.write(content)
        return True
    return False


def main():
    dirs = sys.argv[1:]
    if not dirs:
        print("Usage: update_failsafe_main.py <dir1> [dir2] ...")
        sys.exit(1)

    updated = 0
    total = 0
    for d in dirs:
        for filepath in sorted(Path(d).rglob('*.aria')):
            total += 1
            try:
                if update_file(str(filepath)):
                    updated += 1
                    print(f"  Updated: {filepath}")
            except Exception as e:
                print(f"  ERROR: {filepath}: {e}")

    print(f"\n{updated}/{total} files updated")


if __name__ == '__main__':
    main()
