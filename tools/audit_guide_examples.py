#!/usr/bin/env python3
"""
Programming Guide Code Example Audit (Phase 5.1)

Extracts ```aria code blocks from programming guide .md files,
wraps fragments into compilable programs, and tests compilation.

Usage:
    python audit_guide_examples.py [--fix] [--verbose]
"""

import os
import re
import sys
import subprocess
import tempfile
import json
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Tuple, Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
GUIDE_DIR = REPO_ROOT / "aria_ecosystem" / "programming_guide"
COMPILER = REPO_ROOT / "build" / "ariac"

FAILSAFE = '\nfunc:failsafe = NIL(int32:err_code) { pass(NIL); };\n'
MAIN_WRAPPER = '\nfunc:main = int32() {\n    pass(0);\n};\n'


@dataclass
class CodeBlock:
    file: str
    line: int
    code: str
    is_complete: bool = False
    compiles: bool = False
    error: str = ""


def extract_aria_blocks(md_path: Path) -> List[CodeBlock]:
    """Extract ```aria code blocks from a markdown file."""
    blocks = []
    try:
        content = md_path.read_text(encoding='utf-8')
    except Exception:
        return blocks

    pattern = re.compile(r'^```aria\s*$\n(.*?)^```\s*$', re.MULTILINE | re.DOTALL)

    for match in pattern.finditer(content):
        code = match.group(1).strip()
        if not code or len(code) < 10:
            continue

        # Calculate line number
        line_num = content[:match.start()].count('\n') + 1

        # Determine if it's a complete program (has func:main)
        has_main = 'func:main' in code or 'pub func:main' in code
        has_failsafe = 'failsafe' in code

        blocks.append(CodeBlock(
            file=str(md_path.relative_to(REPO_ROOT)),
            line=line_num,
            code=code,
            is_complete=(has_main and has_failsafe),
        ))

    return blocks


def is_fragment(code: str) -> bool:
    """Detect if a code block is a snippet/fragment rather than a complete program."""
    # Pure type/struct definitions without functions
    if 'func:' not in code and ('struct:' in code or 'enum:' in code):
        return True
    # Single expression or statement
    lines = [l.strip() for l in code.split('\n') if l.strip() and not l.strip().startswith('//')]
    if len(lines) <= 2:
        return True
    # No main function
    if 'func:main' not in code and 'pub func:main' not in code:
        return True
    return False


def wrap_fragment(code: str) -> str:
    """Wrap a code fragment into a compilable program."""
    wrapped = code
    if 'func:main' not in wrapped and 'pub func:main' not in wrapped:
        wrapped += MAIN_WRAPPER
    if 'failsafe' not in wrapped:
        wrapped += FAILSAFE
    return wrapped


def strip_ansi(text: str) -> str:
    """Remove ANSI escape codes from text."""
    return re.sub(r'\x1b\[[0-9;]*m', '', text)


def try_compile(code: str, timeout: int = 10) -> Tuple[bool, str]:
    """Try to compile Aria source. Returns (success, error_message)."""
    with tempfile.NamedTemporaryFile(
        mode='w', suffix='.aria', delete=False, dir='/tmp',
        prefix='audit_'
    ) as f:
        f.write(code)
        src = f.name

    out = src.replace('.aria', '')
    try:
        result = subprocess.run(
            [str(COMPILER), src, '-o', out],
            capture_output=True, text=True, timeout=timeout
        )
        if os.path.exists(out):
            os.unlink(out)

        if result.returncode == 0:
            return True, ""
        else:
            err = strip_ansi((result.stderr or result.stdout or "unknown").strip())
            return False, err.split('\n')[0][:200]
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    except Exception as e:
        return False, f"ERROR: {e}"
    finally:
        if os.path.exists(src):
            os.unlink(src)


def classify_error(error: str) -> str:
    """Classify compilation error type."""
    err = error.lower()
    if 'undefined identifier' in err or 'undeclared' in err or 'undefined' in err:
        return 'undefined_reference'
    if "expected ';'" in err or "expected '='" in err or "expected '('" in err or "expected ')'" in err:
        return 'expected_token'
    if 'type mismatch' in err or 'type error' in err or 'cannot convert' in err:
        return 'type_error'
    if 'unexpected' in err or 'expected expression' in err:
        return 'syntax_error'
    if 'not found' in err:
        return 'not_found'
    if 'TIMEOUT' in error:
        return 'timeout'
    if 'expected' in err:
        return 'expected_token'
    return 'other'


def main():
    import argparse
    parser = argparse.ArgumentParser(description='Audit programming guide code examples')
    parser.add_argument('--verbose', '-v', action='store_true')
    parser.add_argument('--complete-only', action='store_true',
                        help='Only test blocks that look like complete programs')
    parser.add_argument('--limit', type=int, default=0,
                        help='Limit number of blocks to test (0 = all)')
    parser.add_argument('--output', default='tools/guide_audit_results.json')
    args = parser.parse_args()

    if not COMPILER.exists():
        print(f"ERROR: Compiler not found at {COMPILER}")
        sys.exit(1)

    # Collect all code blocks
    print(f"Scanning {GUIDE_DIR} for ```aria code blocks...")
    md_files = sorted(GUIDE_DIR.rglob("*.md"))
    all_blocks: List[CodeBlock] = []

    for md_file in md_files:
        blocks = extract_aria_blocks(md_file)
        all_blocks.extend(blocks)

    print(f"Found {len(all_blocks)} code blocks in {len(md_files)} .md files")

    complete_blocks = [b for b in all_blocks if b.is_complete]
    fragment_blocks = [b for b in all_blocks if not b.is_complete]
    print(f"  Complete programs: {len(complete_blocks)}")
    print(f"  Fragments: {len(fragment_blocks)}")

    # Select blocks to test
    if args.complete_only:
        test_blocks = complete_blocks
    else:
        test_blocks = all_blocks

    if args.limit > 0:
        test_blocks = test_blocks[:args.limit]

    # Compile test
    print(f"\nTesting {len(test_blocks)} code blocks...")
    print("=" * 70)

    pass_count = 0
    fail_count = 0
    skip_count = 0
    error_categories = {}
    failures = []

    for i, block in enumerate(test_blocks, 1):
        # Skip very short fragments that are clearly just syntax illustrations
        code_lines = [l for l in block.code.split('\n') if l.strip() and not l.strip().startswith('//')]
        if len(code_lines) < 3 and not block.is_complete:
            skip_count += 1
            continue

        # Wrap fragments
        if block.is_complete:
            test_code = block.code
        else:
            test_code = wrap_fragment(block.code)

        ok, err = try_compile(test_code)
        block.compiles = ok
        block.error = err

        if ok:
            pass_count += 1
            if args.verbose:
                print(f"  [{i:4d}] PASS  {block.file}:{block.line}")
        else:
            fail_count += 1
            cat = classify_error(err)
            error_categories[cat] = error_categories.get(cat, 0) + 1
            failures.append(block)
            if args.verbose:
                print(f"  [{i:4d}] FAIL  {block.file}:{block.line}")
                print(f"         {err[:120]}")

    tested = pass_count + fail_count
    total = tested + skip_count

    # Summary
    print("\n" + "=" * 70)
    print("  PROGRAMMING GUIDE CODE EXAMPLE AUDIT")
    print("=" * 70)
    print(f"  Total code blocks found  : {len(all_blocks)}")
    print(f"  Tested                   : {tested}")
    print(f"  Skipped (too short)      : {skip_count}")
    print(f"  PASS                     : {pass_count} ({pass_count/max(tested,1)*100:.0f}%)")
    print(f"  FAIL                     : {fail_count} ({fail_count/max(tested,1)*100:.0f}%)")

    if error_categories:
        print(f"\n  Error categories:")
        for cat, count in sorted(error_categories.items(), key=lambda x: -x[1]):
            print(f"    {cat:25s}: {count}")

    # Save results
    results = {
        "total_blocks": len(all_blocks),
        "tested": tested,
        "skipped": skip_count,
        "pass": pass_count,
        "fail": fail_count,
        "pass_rate": round(pass_count / max(tested, 1) * 100, 1),
        "error_categories": error_categories,
        "failures": [
            {
                "file": b.file,
                "line": b.line,
                "error": b.error,
                "category": classify_error(b.error),
                "is_complete": b.is_complete,
                "code_preview": b.code[:200],
            }
            for b in failures
        ]
    }

    with open(args.output, 'w') as f:
        json.dump(results, f, indent=2)
    print(f"\n  Results saved to {args.output}")

    # Show top failures
    if failures:
        print(f"\n  --- TOP FAILURES (showing first 15) ---")
        for b in failures[:15]:
            tag = "COMPLETE" if b.is_complete else "fragment"
            print(f"\n  [{tag}] {b.file}:{b.line}")
            print(f"    Error: {b.error[:150]}")
            preview = b.code.split('\n')[0][:80]
            print(f"    Code:  {preview}...")

    print("=" * 70)


if __name__ == '__main__':
    main()
