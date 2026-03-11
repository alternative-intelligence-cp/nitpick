#!/usr/bin/env python3
"""
migrate_all_void_to_nil.py — Migrate void→NIL across all Aria files

Rules:
  - func:name = void(...)  at TOP LEVEL (no indent) → func:name = NIL(...)
  - func:name = void(...)  INSIDE extern "..." { } block → KEEP as void
  - func:name = void(...)  with `extern` keyword on same line → KEEP as void

Usage:
  python3 migrate_all_void_to_nil.py [--dry-run] [dirs...]

  If no dirs given, processes: tests/ stdlib/ lib/std/ (relative to repo root)
"""

import os
import re
import sys
import argparse
from pathlib import Path


def migrate_file(path: Path, dry_run: bool = False) -> tuple[int, int]:
    """
    Migrate void→NIL in the given .aria file.
    Returns (changes_made, void_in_extern_skipped).
    """
    try:
        content = path.read_text(encoding="utf-8")
    except Exception as e:
        print(f"  ERROR reading {path}: {e}")
        return 0, 0

    lines = content.split("\n")
    changed = 0
    skipped = 0
    extern_depth = 0  # depth inside extern "..." { } blocks
    new_lines = []

    for line in lines:
        stripped = line.lstrip()

        # Detect entering an extern block: line matches  extern "..." {
        if re.match(r'extern\s+"[^"]*"\s*\{', stripped):
            extern_depth += 1
            new_lines.append(line)
            continue

        # Track brace depth (crude but sufficient for top-level extern blocks)
        if extern_depth > 0:
            extern_depth += line.count("{") - line.count("}")
            if extern_depth < 0:
                extern_depth = 0
            new_lines.append(line)  # inside extern block — don't touch
            skipped += len(re.findall(r"=\s*void\s*\(", line))
            continue

        # Outside extern block
        # Skip lines that have the `extern` keyword on them (inline extern style)
        if "extern" in line and re.search(r"=\s*void\s*\(", line):
            skipped += 1
            new_lines.append(line)
            continue

        # Apply substitution: = void( → = NIL(
        new_line, n = re.subn(r"(=\s*)void\s*\(", r"\1NIL(", line)
        if n:
            changed += n
        new_lines.append(new_line)

    if changed == 0:
        return 0, skipped

    new_content = "\n".join(new_lines)
    if not dry_run:
        path.write_text(new_content, encoding="utf-8")

    return changed, skipped


def main():
    parser = argparse.ArgumentParser(description="Migrate void→NIL in Aria files")
    parser.add_argument("--dry-run", action="store_true",
                        help="Show what would be changed without writing")
    parser.add_argument("dirs", nargs="*",
                        help="Directories to process (default: tests/ stdlib/ lib/std/)")
    args = parser.parse_args()

    repo = Path(__file__).parent
    if args.dirs:
        target_dirs = [Path(d) for d in args.dirs]
    else:
        target_dirs = [
            repo / "tests",
            repo / "stdlib",
            repo / "lib" / "std",
        ]

    # Collect all .aria files
    aria_files = []
    for d in target_dirs:
        if d.exists():
            aria_files.extend(sorted(d.rglob("*.aria")))
        else:
            print(f"  WARNING: directory not found: {d}")

    print(f"{'DRY RUN — ' if args.dry_run else ''}Processing {len(aria_files)} .aria files\n")

    total_changed = 0
    total_skipped = 0
    files_modified = 0

    for f in aria_files:
        changed, skipped = migrate_file(f, dry_run=args.dry_run)
        if changed:
            rel = f.relative_to(repo) if f.is_relative_to(repo) else f
            print(f"  {'[DRY] ' if args.dry_run else ''}✓ {rel}  ({changed} change{'s' if changed!=1 else ''})")
            files_modified += 1
        if skipped:
            total_skipped += skipped
        total_changed += changed

    print(f"\n  Files modified : {files_modified}")
    print(f"  Total changes  : {total_changed}")
    print(f"  Extern skipped : {total_skipped} (void kept in extern blocks)")
    if args.dry_run:
        print("\n  (dry run — no files written)")


if __name__ == "__main__":
    main()
