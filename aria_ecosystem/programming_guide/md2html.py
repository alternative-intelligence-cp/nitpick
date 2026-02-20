#!/usr/bin/env python3
"""
Markdown to HTML Converter for Aria Programming Guide Website
Converts .md files to styled HTML with navigation.

Usage:
    ./md2html.py input.md output.html
    ./md2html.py --all  # Convert all guides to html/
"""

import re
import sys
import os
from pathlib import Path
from datetime import date
import json

# HTML template
HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{title} - Aria Programming Guide</title>
    <style>
        :root {{
            --bg-main: #1e1e1e;
            --bg-code: #2d2d2d;
            --bg-sidebar: #252526;
            --text-main: #d4d4d4;
            --text-dim: #808080;
            --accent: #4ec9b0;
            --accent-hover: #6fdfca;
            --link: #569cd6;
            --border: #3e3e42;
        }}
        
        * {{ margin: 0; padding: 0; box-sizing: border-box; }}
        
        body {{
            font-family: 'Segoe UI', system-ui, sans-serif;
            background: var(--bg-main);
            color: var(--text-main);
            line-height: 1.6;
            display: flex;
        }}
        
        /* Sidebar navigation */
        nav {{
            width: 280px;
            background: var(--bg-sidebar);
            border-right: 1px solid var(--border);
            height: 100vh;
            position: fixed;
            overflow-y: auto;
            padding: 20px;
        }}
        
        nav h2 {{
            color: var(--accent);
            font-size: 1.5em;
            margin-bottom: 20px;
        }}
        
        nav .category {{
            margin-bottom: 20px;
        }}
        
        nav .category h3 {{
            color: var(--text-dim);
            font-size: 0.9em;
            text-transform: uppercase;
            margin-bottom: 10px;
            letter-spacing: 0.5px;
        }}
        
        nav ul {{
            list-style: none;
        }}
        
        nav a {{
            color: var(--text-main);
            text-decoration: none;
            display: block;
            padding: 6px 10px;
            border-radius: 4px;
            font-size: 0.95em;
            transition: all 0.2s;
        }}
        
        nav a:hover {{
            background: var(--bg-code);
            color: var(--accent-hover);
        }}
        
        nav a.active {{
            background: var(--accent);
            color: var(--bg-main);
            font-weight: 500;
        }}
        
        /* Main content */
        main {{
            margin-left: 280px;
            padding: 40px 60px;
            max-width: 900px;
            width: 100%;
        }}
        
        h1 {{
            color: var(--accent);
            font-size: 2.5em;
            margin-bottom: 30px;
            border-bottom: 2px solid var(--border);
            padding-bottom: 15px;
        }}
        
        h2 {{
            color: var(--accent);
            font-size: 1.8em;
            margin-top: 40px;
            margin-bottom: 20px;
        }}
        
        h3 {{
            color: var(--text-main);
            font-size: 1.3em;
            margin-top: 30px;
            margin-bottom: 15px;
        }}
        
        p {{
            margin-bottom: 15px;
            color: var(--text-main);
        }}
        
        code {{
            background: var(--bg-code);
            padding: 2px 6px;
            border-radius: 3px;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 0.9em;
            color: var(--accent);
        }}
        
        pre {{
            background: var(--bg-code);
            padding: 20px;
            border-radius: 6px;
            overflow-x: auto;
            margin: 20px 0;
            border-left: 3px solid var(--accent);
        }}
        
        pre code {{
            background: none;
            padding: 0;
            color: var(--text-main);
        }}
        
        ul, ol {{
            margin-left: 30px;
            margin-bottom: 15px;
        }}
        
        li {{
            margin-bottom: 8px;
        }}
        
        a {{
            color: var(--link);
            text-decoration: none;
        }}
        
        a:hover {{
            text-decoration: underline;
        }}
        
        table {{
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }}
        
        th, td {{
            padding: 12px;
            text-align: left;
            border: 1px solid var(--border);
        }}
        
        th {{
            background: var(--bg-code);
            color: var(--accent);
            font-weight: 600;
        }}
        
        blockquote {{
            border-left: 4px solid var(--accent);
            padding-left: 20px;
            margin: 20px 0;
            color: var(--text-dim);
            font-style: italic;
        }}
        
        hr {{
            border: none;
            border-top: 1px solid var(--border);
            margin: 30px 0;
        }}
        
        .breadcrumb {{
            color: var(--text-dim);
            font-size: 0.9em;
            margin-bottom: 20px;
        }}
        
        .breadcrumb a {{
            color: var(--text-dim);
        }}
        
        .breadcrumb a:hover {{
            color: var(--accent);
        }}
        
        /* Responsive */
        @media (max-width: 768px) {{
            nav {{
                display: none;
            }}
            main {{
                margin-left: 0;
                padding: 20px;
            }}
        }}
    </style>
</head>
<body>
    <nav>
        <h2>Aria Guide</h2>
        {nav_content}
    </nav>
    <main>
        {breadcrumb}
        {content}
    </main>
</body>
</html>
"""

def convert_markdown_to_html(md_content):
    """Convert markdown to HTML."""
    html = md_content
    
    # Code blocks
    html = re.sub(r'```(\w*)\n(.*?)\n```', r'<pre><code>\2</code></pre>', html, flags=re.DOTALL)
    
    # Headings
    html = re.sub(r'^### (.+)$', r'<h3>\1</h3>', html, flags=re.MULTILINE)
    html = re.sub(r'^## (.+)$', r'<h2>\1</h2>', html, flags=re.MULTILINE)
    html = re.sub(r'^# (.+)$', r'<h1>\1</h1>', html, flags=re.MULTILINE)
    
    # Bold and italic
    html = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', html)
    html = re.sub(r'\*(.+?)\*', r'<em>\1</em>', html)
    
    # Inline code
    html = re.sub(r'`([^`]+)`', r'<code>\1</code>', html)
    
    # Links
    html = re.sub(r'\[([^\]]+)\]\(([^\)]+)\)', r'<a href="\2">\1</a>', html)
    
    # Lists
    html = re.sub(r'^\- (.+)$', r'<li>\1</li>', html, flags=re.MULTILINE)
    html = re.sub(r'(<li>.*?</li>\n?)+', r'<ul>\g<0></ul>', html, flags=re.DOTALL)
    
    # Blockquotes
    html = re.sub(r'^> (.+)$', r'<blockquote>\1</blockquote>', html, flags=re.MULTILINE)
    
    # Horizontal rules
    html = re.sub(r'^---$', r'<hr>', html, flags=re.MULTILINE)
    
    # Paragraphs (simple approach - wrap non-tag lines)
    lines = html.split('\n')
    result = []
    in_paragraph = False
    
    for line in lines:
        line = line.strip()
        if not line:
            if in_paragraph:
                result.append('</p>')
                in_paragraph = False
            continue
        
        # Check if line is already wrapped in HTML tags
        if line.startswith('<') or in_paragraph:
            if line.startswith('<h') or line.startswith('<pre') or line.startswith('<ul') or line.startswith('<hr'):
                if in_paragraph:
                    result.append('</p>')
                    in_paragraph = False
                result.append(line)
            else:
                if not in_paragraph and not line.startswith('<'):
                    result.append('<p>')
                    in_paragraph = True
                result.append(line)
        else:
            result.append('<p>')
            result.append(line)
            in_paragraph = True
    
    if in_paragraph:
        result.append('</p>')
    
    return '\n'.join(result)

def generate_navigation(guide_dir):
    """Generate navigation HTML from directory structure."""
    guide_path = Path(guide_dir)
    
    categories = {}
    for md_file in guide_path.rglob('*.md'):
        if md_file.name in ['README.md', 'STRUCTURE.txt', 'FILES.md']:
            continue
        
        rel_path = md_file.relative_to(guide_path)
        category = rel_path.parts[0] if len(rel_path.parts) > 1 else 'General'
        
        if category not in categories:
            categories[category] = []
        
        # Create HTML path
        html_path = str(rel_path.with_suffix('.html'))
        title = md_file.stem.replace('_', ' ').title()
        
        categories[category].append({
            'title': title,
            'path': html_path
        })
    
    # Build navigation HTML
    nav_html = []
    for category in sorted(categories.keys()):
        cat_title = category.replace('_', ' ').title()
        nav_html.append(f'<div class="category">')
        nav_html.append(f'<h3>{cat_title}</h3>')
        nav_html.append('<ul>')
        
        for item in sorted(categories[category], key=lambda x: x['title']):
            nav_html.append(f'<li><a href="/{item["path"]}">{item["title"]}</a></li>')
        
        nav_html.append('</ul>')
        nav_html.append('</div>')
    
    return '\n'.join(nav_html)

def convert_file(input_path, output_path, guide_dir, nav_content):
    """Convert a single markdown file to HTML."""
    with open(input_path, 'r', encoding='utf-8') as f:
        md_content = f.read()
    
    # Extract title from first heading or filename
    title_match = re.search(r'^#\s+(.+)$', md_content, re.MULTILINE)
    title = title_match.group(1) if title_match else Path(input_path).stem.replace('_', ' ').title()
    
    # Convert markdown to HTML
    html_content = convert_markdown_to_html(md_content)
    
    # Generate breadcrumb
    rel_path = Path(input_path).relative_to(guide_dir)
    breadcrumb_parts = ['<a href="/">Home</a>']
    for i, part in enumerate(rel_path.parts[:-1]):
        breadcrumb_parts.append(part.replace('_', ' ').title())
    breadcrumb = '<div class="breadcrumb">' + ' / '.join(breadcrumb_parts) + '</div>'
    
    # Fill template
    html = HTML_TEMPLATE.format(
        title=title,
        nav_content=nav_content,
        breadcrumb=breadcrumb,
        content=html_content
    )
    
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(html)
    
    print(f"✓ {input_path} → {output_path}")

def convert_all_guides(guide_dir, output_dir):
    """Convert all markdown files to HTML website."""
    guide_path = Path(guide_dir)
    output_path = Path(output_dir)
    
    print("Generating navigation...")
    nav_content = generate_navigation(guide_dir)
    
    # Find all .md files
    md_files = [f for f in guide_path.rglob('*.md') 
                if f.name not in ['README.md', 'STRUCTURE.txt', 'FILES.md']]
    
    print(f"Converting {len(md_files)} markdown files to HTML...")
    
    for md_file in md_files:
        rel_path = md_file.relative_to(guide_path)
        html_file = output_path / rel_path.with_suffix('.html')
        
        convert_file(md_file, html_file, guide_path, nav_content)
    
    # Create index.html
    create_index(output_path, guide_path, nav_content)
    
    print(f"\n✅ Conversion complete! {len(md_files)} HTML pages generated.")
    print(f"\nOutput directory: {output_path}")
    print(f"\nTo view:")
    print(f"  cd {output_path}")
    print(f"  python3 -m http.server 8000")
    print(f"  Visit: http://localhost:8000")

def create_index(output_path, guide_path, nav_content):
    """Create main index.html."""
    readme_path = guide_path / 'README.md'
    
    if readme_path.exists():
        with open(readme_path, 'r', encoding='utf-8') as f:
            md_content = f.read()
        html_content = convert_markdown_to_html(md_content)
    else:
        html_content = "<h1>Aria Programming Guide</h1><p>Welcome to the Aria programming guide.</p>"
    
    html = HTML_TEMPLATE.format(
        title='Aria Programming Guide',
        nav_content=nav_content,
        breadcrumb='',
        content=html_content
    )
    
    with open(output_path / 'index.html', 'w', encoding='utf-8') as f:
        f.write(html)
    
    print(f"✓ Created index.html")

def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  ./md2html.py input.md output.html")
        print("  ./md2html.py --all")
        sys.exit(1)
    
    if sys.argv[1] == '--all':
        script_dir = Path(__file__).parent
        guide_dir = script_dir
        output_dir = script_dir / 'html'
        convert_all_guides(guide_dir, output_dir)
    else:
        input_file = sys.argv[1]
        output_file = sys.argv[2] if len(sys.argv) > 2 else Path(input_file).stem + '.html'
        guide_dir = Path(input_file).parent
        nav_content = generate_navigation(guide_dir)
        convert_file(input_file, output_file, guide_dir, nav_content)

if __name__ == '__main__':
    main()
