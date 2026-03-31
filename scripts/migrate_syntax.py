#!/usr/bin/env python3
"""Aria Syntax Migration Script

Converts builtin calls from function-style to keyword-style:
  pass(expr)  → pass expr       (statement)
  fail(expr)  → fail expr       (statement)
  exit(code)  → exit code       (statement)
  fall(label) → fall label      (statement)
  apush(expr) → apush expr      (statement)
  astack(expr)→ astack expr     (statement)
  raw(expr)   → raw expr        (expression)
  drop(expr)  → drop expr       (expression)
  ok(expr)    → ok expr         (expression)
  apop()      → apop            (expression)
  apeek()     → apeek           (expression)

The old syntax still compiles (parens become grouped expressions),
so this is a non-breaking cosmetic migration.
"""

import re
import sys
import os
import argparse

# All keywords to migrate
KEYWORDS = ['pass', 'fail', 'exit', 'fall', 'raw', 'drop', 'ok',
            'apop', 'apush', 'apeek', 'astack']

# Keywords that take no arguments: keyword() → keyword
NO_ARG_KEYWORDS = {'apop', 'apeek'}


def build_protected_ranges(text):
    """Build sorted list of (start, end) ranges for strings and comments.
    
    Protected ranges are regions where we should NOT transform code:
    - Single-quoted strings: '...'
    - Double-quoted strings: "..."
    - Backtick template strings: `...` (but NOT inside &{...} interpolations)
    - Line comments: // ... \n
    - Block comments: /* ... */
    """
    ranges = []
    i = 0
    n = len(text)
    
    while i < n:
        c = text[i]
        
        # Line comment
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            start = i
            i += 2
            while i < n and text[i] != '\n':
                i += 1
            ranges.append((start, i))
            continue
        
        # Block comment
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            start = i
            i += 2
            while i < n:
                if text[i] == '*' and i + 1 < n and text[i + 1] == '/':
                    i += 2
                    break
                i += 1
            ranges.append((start, i))
            continue
        
        # Double-quoted string
        if c == '"':
            start = i
            i += 1
            while i < n and text[i] != '"':
                if text[i] == '\\':
                    i += 1  # skip escaped char
                i += 1
            if i < n:
                i += 1  # skip closing quote
            ranges.append((start, i))
            continue
        
        # Single-quoted string
        if c == "'":
            start = i
            i += 1
            while i < n and text[i] != "'":
                if text[i] == '\\':
                    i += 1
                i += 1
            if i < n:
                i += 1
            ranges.append((start, i))
            continue
        
        # Backtick template string — protect the string parts but NOT &{...} interpolations
        if c == '`':
            start = i
            i += 1
            while i < n and text[i] != '`':
                if text[i] == '\\':
                    i += 1
                    i += 1
                    continue
                # &{ starts an interpolation — DON'T protect this region
                if text[i] == '&' and i + 1 < n and text[i + 1] == '{':
                    # Protect up to &{
                    ranges.append((start, i))
                    # Skip &{
                    i += 2
                    # Find matching } (with nesting)
                    depth = 1
                    while i < n and depth > 0:
                        if text[i] == '{':
                            depth += 1
                        elif text[i] == '}':
                            depth -= 1
                        elif text[i] == '"':
                            # Skip string inside interpolation
                            i += 1
                            while i < n and text[i] != '"':
                                if text[i] == '\\':
                                    i += 1
                                i += 1
                        elif text[i] == "'":
                            i += 1
                            while i < n and text[i] != "'":
                                if text[i] == '\\':
                                    i += 1
                                i += 1
                        i += 1
                    # After }, continue template string; new protected region starts
                    start = i - 1  # from closing }
                    continue
                i += 1
            if i < n:
                i += 1  # skip closing backtick
            ranges.append((start, i))
            continue
        
        i += 1
    
    return ranges


def is_protected(pos, ranges):
    """Check if position is inside a protected range (binary search)."""
    lo, hi = 0, len(ranges) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        start, end = ranges[mid]
        if pos < start:
            hi = mid - 1
        elif pos >= end:
            lo = mid + 1
        else:
            return True
    return False


def find_matching_paren(text, open_pos, protected_ranges):
    """Find matching ')' for '(' at open_pos, respecting strings/comments.
    
    Returns the index of the matching ')' or -1 if not found.
    """
    depth = 1
    i = open_pos + 1
    n = len(text)
    
    while i < n and depth > 0:
        # If inside a protected range, skip to end of it
        if is_protected(i, protected_ranges):
            # Find which range we're in and skip past it
            for start, end in protected_ranges:
                if start <= i < end:
                    i = end
                    break
            continue
        
        c = text[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    
    return -1


def migrate_content(content, py_mode=False):
    """Migrate all builtin calls in a string. Returns (new_content, num_changes).
    
    In py_mode, skip protected range detection (Aria code lives inside Python strings)
    and add '.' to lookbehind to avoid matching sys.exit() etc.
    """
    total_changes = 0
    
    # Build keyword pattern: match keyword( where keyword is not preceded by alphanumeric/_
    # In py_mode, also exclude . to avoid matching sys.exit(), os.path(), etc.
    if py_mode:
        lookbehind = r'(?<![a-zA-Z0-9_.])'
    else:
        lookbehind = r'(?<![a-zA-Z0-9_])'
    
    kw_pattern = re.compile(
        lookbehind + r'(' + '|'.join(re.escape(kw) for kw in KEYWORDS) + r')\('
    )
    
    # Iterate: find-and-replace from left to right, rebuilding protected ranges after each change
    while True:
        if not py_mode:
            protected = build_protected_ranges(content)
        else:
            protected = []
        changed_this_pass = False
        
        for match in kw_pattern.finditer(content):
            kw = match.group(1)
            kw_start = match.start()
            paren_start = match.end() - 1  # position of '('
            
            # Skip if inside protected range
            if is_protected(kw_start, protected):
                continue
            
            # Find matching closing paren
            close_paren = find_matching_paren(content, paren_start, protected)
            if close_paren == -1:
                continue
            
            # Extract inner content
            inner = content[paren_start + 1:close_paren]
            
            if kw in NO_ARG_KEYWORDS:
                # apop() → apop, apeek() → apeek
                if inner.strip() == '':
                    new_text = kw
                    content = content[:kw_start] + new_text + content[close_paren + 1:]
                    total_changes += 1
                    changed_this_pass = True
                    break  # restart scanning since positions shifted
                # If apop/apeek somehow has args, skip
                continue
            else:
                # keyword(expr) → keyword expr
                inner_stripped = inner.strip()
                if not inner_stripped:
                    # Empty parens: keyword() → keyword (e.g. pass() → pass)
                    new_text = kw
                    content = content[:kw_start] + new_text + content[close_paren + 1:]
                    total_changes += 1
                    changed_this_pass = True
                    break
                
                new_text = kw + ' ' + inner_stripped
                content = content[:kw_start] + new_text + content[close_paren + 1:]
                total_changes += 1
                changed_this_pass = True
                break  # restart scanning since positions shifted
        
        if not changed_this_pass:
            break
    
    return content, total_changes


def migrate_file(filepath, dry_run=False, py_mode=False):
    """Migrate a single file. Returns (changed, num_changes)."""
    with open(filepath, 'r') as f:
        content = f.read()
    
    new_content, num_changes = migrate_content(content, py_mode=py_mode)
    
    if num_changes > 0 and not dry_run:
        with open(filepath, 'w') as f:
            f.write(new_content)
    
    return num_changes > 0, num_changes


def main():
    parser = argparse.ArgumentParser(
        description='Migrate Aria builtin syntax from function-style to keyword-style'
    )
    parser.add_argument('paths', nargs='+', help='Directories or files to process')
    parser.add_argument('--dry-run', action='store_true',
                        help='Show what would be changed without modifying files')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Show per-file details')
    parser.add_argument('--ext', nargs='+', default=['.aria'],
                        help='File extensions to process (default: .aria)')
    args = parser.parse_args()
    
    files = []
    for path in args.paths:
        if os.path.isfile(path) and any(path.endswith(e) for e in args.ext):
            files.append(path)
        elif os.path.isdir(path):
            for root, dirs, filenames in os.walk(path):
                for fn in sorted(filenames):
                    if any(fn.endswith(e) for e in args.ext):
                        files.append(os.path.join(root, fn))
    
    total_files_changed = 0
    total_changes = 0
    
    for filepath in sorted(files):
        is_py = filepath.endswith('.py')
        changed, num_changes = migrate_file(filepath, dry_run=args.dry_run, py_mode=is_py)
        if changed:
            total_files_changed += 1
            total_changes += num_changes
            if args.verbose:
                prefix = "[DRY RUN] " if args.dry_run else ""
                print(f"  {prefix}{filepath}: {num_changes} changes")
    
    mode = "[DRY RUN] " if args.dry_run else ""
    print(f"\n{mode}Migration complete: {total_changes} changes across "
          f"{total_files_changed} files (of {len(files)} scanned)")


if __name__ == '__main__':
    main()
