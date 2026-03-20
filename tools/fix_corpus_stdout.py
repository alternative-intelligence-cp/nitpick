#!/usr/bin/env python3
"""
Fix training corpus: replace bogus stdout/stderr/stddbg << patterns
with correct Aria print()/println() calls.

Patterns handled:
  stdout << expr;          →  println(expr);
  stderr << "msg";         →  println(expr);
  stddbg << expr;          →  println(expr);
  stdout << a << b << c;   →  print(a); print(b); println(c);

Also cleans up stddato << / stddati << if present.
"""

import json
import re
import sys
from pathlib import Path


def fix_stream_line(line: str) -> str:
    """Fix a single line containing stream << operators."""
    stream_pattern = r'(stdout|stderr|stddbg|stddato|stddati)\s*<<\s*'

    if not re.search(stream_pattern, line):
        return line

    # Get the indentation
    indent = re.match(r'^(\s*)', line).group(1)

    # Check if it's a comment line — still fix the pattern inside
    comment_prefix = ''
    stripped = line.lstrip()
    if stripped.startswith('//') or stripped.startswith('///') or stripped.startswith('*'):
        # Fix inside comments too — replace stream << expr with println(expr)
        pass

    # Check if stream << is preceded by match-arm syntax (=> stdout <<)
    # In this case, preserve the prefix before stdout
    m = re.search(r'(stdout|stderr|stddbg|stddato|stddati)\s*<<\s*(.*)', line)
    if not m:
        return line

    stream_name = m.group(1)
    rest = m.group(2)
    prefix = line[:m.start()]  # everything before "stdout"

    # Split on << to get individual expressions
    parts = split_on_stream_op(rest)

    if len(parts) == 1:
        # Simple: stdout << expr;  →  println(expr);
        expr = parts[0].rstrip(';').rstrip(',').strip()
        trailing = ''
        # Preserve trailing comma or semicolon
        orig_rest = rest.strip()
        if orig_rest.endswith(','):
            trailing = ','
        elif orig_rest.endswith(';'):
            trailing = ';'
        return f'{prefix}println({expr}){trailing}'
    else:
        # Chained: stdout << a << b;  →  multiple print calls
        result_parts = []
        for i, part in enumerate(parts):
            expr = part.rstrip(';').rstrip(',').strip()
            if not expr:
                continue
            if i == len(parts) - 1:
                trailing = ''
                orig_part = part.strip()
                if orig_part.endswith(','):
                    trailing = ','
                elif orig_part.endswith(';'):
                    trailing = ';'
                result_parts.append(f'println({expr}){trailing}')
            else:
                result_parts.append(f'print({expr});')
        if len(result_parts) == 1:
            return f'{prefix}{result_parts[0]}'
        else:
            # Multi-part: put first call where stream was, rest on new lines
            result = f'{prefix}{result_parts[0]}'
            # Determine indentation for continuation lines
            cont_indent = indent + '    ' if prefix.strip() else indent
            for rp in result_parts[1:]:
                result += f'\n{cont_indent}{rp}'
            return result


def split_on_stream_op(text: str) -> list:
    """Split text on << operators, respecting string literals."""
    parts = []
    current = []
    i = 0
    in_string = None  # None, '"', or "'"

    while i < len(text):
        c = text[i]

        # Track string boundaries
        if c == '\\' and in_string and i + 1 < len(text):
            current.append(c)
            current.append(text[i + 1])
            i += 2
            continue

        if c in ('"', "'"):
            if in_string is None:
                in_string = c
            elif in_string == c:
                in_string = None

        # Check for << outside strings
        if not in_string and c == '<' and i + 1 < len(text) and text[i + 1] == '<':
            parts.append(''.join(current).strip())
            current = []
            i += 2
            # Skip whitespace after <<
            while i < len(text) and text[i] == ' ':
                i += 1
            continue

        current.append(c)
        i += 1

    remainder = ''.join(current).strip()
    if remainder:
        parts.append(remainder)

    return [p for p in parts if p]


def fix_content(content: str) -> str:
    """Fix all stream << patterns in a content string."""
    lines = content.split('\n')
    fixed_lines = []
    for line in lines:
        if re.search(r'(stdout|stderr|stddbg|stddato|stddati)\s*<<', line):
            fixed_lines.append(fix_stream_line(line))
        else:
            fixed_lines.append(line)
    return '\n'.join(fixed_lines)


def process_corpus(input_path: str, output_path: str, dry_run: bool = False):
    """Process a JSONL corpus file and fix stream << patterns."""
    input_file = Path(input_path)
    if not input_file.exists():
        print(f"Error: {input_path} not found", file=sys.stderr)
        sys.exit(1)

    with open(input_file) as f:
        lines = f.readlines()

    fixed_count = 0
    total_stream_fixes = 0
    output_lines = []

    for i, line in enumerate(lines):
        obj = json.loads(line)
        text_before = json.dumps(obj)

        stream_pattern = re.compile(r'(stdout|stderr|stddbg|stddato|stddati)\s*<<')

        changed = False

        # Handle Alpaca format (instruction/input/output)
        for field in ['output', 'instruction', 'input']:
            content = obj.get(field, '')
            if stream_pattern.search(content):
                fixes = len(stream_pattern.findall(content))
                obj[field] = fix_content(content)
                total_stream_fixes += fixes
                changed = True

        # Handle chat format (messages array)
        for msg in obj.get('messages', []):
            content = msg.get('content', '')
            if stream_pattern.search(content):
                fixes = len(stream_pattern.findall(content))
                msg['content'] = fix_content(content)
                total_stream_fixes += fixes
                changed = True

        if changed:
            fixed_count += 1

        output_lines.append(json.dumps(obj, ensure_ascii=False) + '\n')

    print(f"Processed {len(lines)} examples")
    print(f"Fixed {fixed_count} examples ({fixed_count * 100 / len(lines):.1f}%)")
    print(f"Total stream << replacements: {total_stream_fixes}")

    if not dry_run:
        with open(output_path, 'w') as f:
            f.writelines(output_lines)
        print(f"Written to: {output_path}")

        # Verify no contamination remains
        with open(output_path) as f:
            check = f.read()
        remaining = len(stream_pattern.findall(check))
        print(f"Remaining stream << in output: {remaining}")
    else:
        print("(dry run — no file written)")


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='Fix stdout << contamination in Aria training corpus')
    parser.add_argument('input', help='Input JSONL file')
    parser.add_argument('-o', '--output', help='Output JSONL file (default: input with _fixed suffix)')
    parser.add_argument('--dry-run', action='store_true', help='Just report, do not write')
    parser.add_argument('--in-place', action='store_true', help='Overwrite input file')
    args = parser.parse_args()

    output = args.output
    if args.in_place:
        output = args.input
    elif not output:
        p = Path(args.input)
        output = str(p.parent / (p.stem + '_fixed' + p.suffix))

    process_corpus(args.input, output, dry_run=args.dry_run)
