#!/usr/bin/env python3
"""
Markdown to Man Page Converter for Aria Programming Guide
Converts .md files to groff man page format for `man` command.

Usage:
    ./md2man.py input.md output.7
    ./md2man.py --all  # Convert all guides to man/man7/
"""

import re
import sys
import os
from pathlib import Path
from datetime import date

def escape_man(text):
    """Escape special characters for man page format."""
    text = text.replace('\\', '\\\\')
    text = text.replace('-', r'\-')
    text = text.replace('.', r'\.')
    return text

def convert_inline_formatting(text):
    """Convert markdown inline formatting to man page macros."""
    # Bold: **text** or __text__ → \fBtext\fR
    text = re.sub(r'\*\*(.+?)\*\*', r'\\fB\1\\fR', text)
    text = re.sub(r'__(.+?)__', r'\\fB\1\\fR', text)
    
    # Italic: *text* or _text_ → \fItext\fR  
    text = re.sub(r'(?<!\*)\*([^\*]+?)\*(?!\*)', r'\\fI\1\\fR', text)  # Avoid matching **
    text = re.sub(r'(?<!_)_([^_]+?)_(?!_)', r'\\fI\1\\fR', text)      # Avoid matching __
    
    # Inline code: `code` → \fBcode\fR (bold monospace)
    text = re.sub(r'`([^`]+?)`', r'\\fB\1\\fR', text)
    
    return text
    
    return text

def markdown_to_man(md_content, title, section=7, description=""):
    """Convert markdown content to man page format."""
    lines = md_content.split('\n')
    man_lines = []
    
    # Man page header
    today = date.today().strftime("%B %Y")
    man_lines.append(f'.TH {title.upper()} {section} "{today}" "Aria v0.0.7" "Aria Programming Guide"')
    
    # Name section
    man_lines.append('.SH NAME')
    man_lines.append(f'{title.lower()} \\- {description if description else title}')
    
    in_code_block = False
    in_list = False
    current_section = None
    
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        
        # Code blocks (```)
        if line.startswith('```'):
            if not in_code_block:
                if man_lines and man_lines[-1] != '.PP':
                    man_lines.append('.PP')
                man_lines.append('.nf')  # No-fill mode
                man_lines.append('.RS')  # Indent
                in_code_block = True
            else:
                man_lines.append('.RE')  # End indent
                man_lines.append('.fi')  # Fill mode
                in_code_block = False
            i += 1
            continue
        
        # Inside code block - preserve verbatim
        if in_code_block:
            # Escape backslashes and leading dots
            escaped = line.replace('\\', '\\\\')
            if escaped.startswith('.'):
                escaped = '\\&' + escaped
            man_lines.append(escaped)
            i += 1
            continue
        
        # Headings
        if line.startswith('#'):
            if in_list:
                man_lines.append('.RE')
                in_list = False
            
            heading_level = len(line) - len(line.lstrip('#'))
            heading_text = line.lstrip('#').strip()
            
            if heading_level == 1:
                # H1 → .SH (section header)
                man_lines.append(f'.SH {heading_text.upper()}')
                current_section = heading_text
            elif heading_level == 2:
                # H2 → .SS (subsection)
                man_lines.append(f'.SS {heading_text}')
            else:
                # H3+ → bold paragraph
                man_lines.append('.PP')
                man_lines.append(f'\\fB{heading_text}\\fR')
            i += 1
            continue
        
        # Lists
        if line.startswith('- ') or line.startswith('* ') or re.match(r'^\d+\.\s', line):
            if not in_list:
                man_lines.append('.RS')
                in_list = True
            
            # Extract list content
            list_content = re.sub(r'^[\-\*]|\d+\.', '', line).strip()
            list_content = convert_inline_formatting(list_content)
            man_lines.append(f'.IP \\(bu 3')
            man_lines.append(list_content)
            i += 1
            continue
        
        # End list if we hit non-list content
        if in_list and line and not line.startswith((' ', '\t')):
            man_lines.append('.RE')
            in_list = False
        
        # Empty lines
        if not line:
            if man_lines and not man_lines[-1] in ['.PP', '.RS', '.RE']:
                man_lines.append('.PP')
            i += 1
            continue
        
        # Regular paragraphs
        if line:
            line = convert_inline_formatting(line)
            # Escape leading dots
            if line.startswith('.'):
                line = '\\&' + line
            man_lines.append(line)
        
        i += 1
    
    # Close any open list
    if in_list:
        man_lines.append('.RE')
    
    # Add SEE ALSO section
    man_lines.append('.SH SEE ALSO')
    man_lines.append('.BR aria (1),')
    man_lines.append('.BR ariac (1)')
    
    return '\n'.join(man_lines)

def convert_file(input_path, output_path):
    """Convert a single markdown file to man page."""
    with open(input_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Extract title from filename
    title = Path(input_path).stem.replace('_', '-')
    
    # Try to extract description from first heading or first paragraph
    description = ""
    first_line = content.split('\n')[0] if content else ""
    if first_line.startswith('#'):
        description = first_line.lstrip('#').strip()
    
    man_content = markdown_to_man(content, title, section=7, description=description)
    
    os.makedirs(os.path.dirname(output_path) or '.', exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(man_content)
    
    print(f"✓ {input_path} → {output_path}")

def convert_all_guides(guide_dir, output_dir):
    """Convert all markdown files in programming guide to man pages."""
    guide_path = Path(guide_dir)
    output_path = Path(output_dir)
    
    # Find all .md files
    md_files = list(guide_path.rglob('*.md'))
    
    print(f"Converting {len(md_files)} markdown files to man pages...")
    print(f"Output directory: {output_path}")
    
    for md_file in md_files:
        # Calculate relative path for organization
        rel_path = md_file.relative_to(guide_path)
        
        # Convert to man page name (section 7 for documentation)
        # e.g., types/int32.md → aria-int32.7
        man_name = f"aria-{rel_path.stem.replace('_', '-')}.7"
        man_file = output_path / 'man7' / man_name
        
        convert_file(md_file, man_file)
    
    # Create main aria.7 overview
    create_main_manpage(output_path / 'man7' / 'aria.7', guide_path)
    
    print(f"\n✅ Conversion complete! {len(md_files)} man pages generated.")
    print(f"\nTo install:")
    print(f"  sudo cp -r {output_path}/man7 /usr/share/man/")
    print(f"  sudo mandb")
    print(f"\nTo view:")
    print(f"  man aria")
    print(f"  man aria-int32")
    print(f"  man aria-for")

def create_main_manpage(output_path, guide_path):
    """Create main aria.7 overview man page."""
    content = """# ARIA

Aria Programming Language - Systems programming with six-stream I/O and TESLA-inspired architecture.

## DESCRIPTION

Aria is a modern systems programming language featuring symmetric signed integers, balanced ternary/nonary types, and a revolutionary six-stream I/O model.

## LANGUAGE FEATURES

- **TBB Types** - Symmetric signed integers with overflow detection (tbb8/16/32/64)
- **Balanced Types** - Ternary and nonary arithmetic (trit, tryte, nit, nyte)
- **Generics** - Full monomorphization with type inference
- **Result Types** - Explicit error handling with pass/fail
- **Six Streams** - stdin, stdout, stderr, stddbg, stddati, stddato

## GUIDE SECTIONS

Access detailed documentation with:

- **Types**: man aria-int32, man aria-tbb8, man aria-handle
- **Control Flow**: man aria-if-else, man aria-for, man aria-pick
- **Functions**: man aria-lambda, man aria-generics
- **I/O**: man aria-io-overview, man aria-six-stream-topology
- **Memory**: man aria-borrowing, man aria-gc
- **Operators**: man aria-range, man aria-add

## ONLINE DOCUMENTATION

https://aria.docs.ai-liberation-platform.org/

## VERSION

Aria v0.0.7-dev (February 2026)
"""
    
    man_content = markdown_to_man(content, 'aria', section=7, description='Aria Programming Language')
    
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(man_content)
    
    print(f"✓ Created main overview: {output_path}")

def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  ./md2man.py input.md output.7")
        print("  ./md2man.py --all")
        sys.exit(1)
    
    if sys.argv[1] == '--all':
        # Convert all guides
        script_dir = Path(__file__).parent
        guide_dir = script_dir
        output_dir = script_dir / 'man'
        convert_all_guides(guide_dir, output_dir)
    else:
        # Convert single file
        input_file = sys.argv[1]
        output_file = sys.argv[2] if len(sys.argv) > 2 else Path(input_file).stem + '.7'
        convert_file(input_file, output_file)

if __name__ == '__main__':
    main()
