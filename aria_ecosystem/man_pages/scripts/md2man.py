#!/usr/bin/env python3
"""
Aria Programming Guide to Man Page Converter
Converts markdown files to properly formatted groff man pages

Features:
- Metadata extraction from markdown frontmatter
- Cross-reference generation (SEE ALSO sections)
- Keyword indexing for apropos
- Proper section numbering
- Version management
"""

import os
import sys
import re
import json
from pathlib import Path
from datetime import datetime
import subprocess


class ManPageConverter:
    def __init__(self, source_dir, output_dir, version="0.1.0"):
        self.source_dir = Path(source_dir)
        self.output_dir = Path(output_dir)
        self.version = version
        self.date = datetime.now().strftime("%B %Y")

        # Track all files for cross-reference building
        self.file_map = {}  # filename -> metadata
        self.category_map = {}  # category -> [files]
        self.keyword_index = {}  # keyword -> [files]

    def extract_metadata(self, md_file):
        """Extract metadata from markdown file"""
        with open(md_file, "r") as f:
            content = f.read()

        metadata = {
            "file": md_file.name,
            "title": None,
            "category": None,
            "subcategory": None,
            "keywords": [],
            "related": [],
            "syntax": None,
            "purpose": None,
        }

        # Extract title (first # heading)
        title_match = re.search(r"^#\s+(.+?)$", content, re.MULTILINE)
        if title_match:
            metadata["title"] = title_match.group(1).strip()

        # Extract frontmatter if present (between --- markers)
        frontmatter_match = re.match(r"^---\n(.*?)\n---", content, re.DOTALL)
        if frontmatter_match:
            frontmatter = frontmatter_match.group(1)

            # Parse key-value pairs
            for line in frontmatter.split("\n"):
                if ":" in line:
                    key, value = line.split(":", 1)
                    key = key.strip().lower()
                    value = value.strip()

                    if key == "category":
                        metadata["category"] = value
                    elif key == "syntax":
                        metadata["syntax"] = value
                    elif key == "purpose":
                        metadata["purpose"] = value
                    elif key == "keywords":
                        metadata["keywords"] = [k.strip() for k in value.split(",")]

        # Extract from **Category**: pattern if no frontmatter
        if not metadata["category"]:
            category_match = re.search(r"\*\*Category\*\*:\s+(.+?)(?:\n|$)", content)
            if category_match:
                cat_parts = category_match.group(1).strip().split("→")
                metadata["category"] = cat_parts[0].strip()
                if len(cat_parts) > 1:
                    metadata["subcategory"] = cat_parts[1].strip()

        # Extract syntax
        if not metadata["syntax"]:
            syntax_match = re.search(r"\*\*Syntax\*\*:\s+`(.+?)`", content)
            if syntax_match:
                metadata["syntax"] = syntax_match.group(1)

        # Extract purpose
        if not metadata["purpose"]:
            purpose_match = re.search(r"\*\*Purpose\*\*:\s+(.+?)(?:\n|$)", content)
            if purpose_match:
                metadata["purpose"] = purpose_match.group(1)

        # Extract related topics from ## Related section
        related_section = re.search(
            r"^## Related.*?\n(.*?)(?=^##|\Z)", content, re.MULTILINE | re.DOTALL
        )
        if related_section:
            related_content = related_section.group(1)
            # Find all markdown links [text](file.md)
            related_links = re.findall(r"\[([^\]]+)\]\(([^\)]+\.md)\)", related_content)
            metadata["related"] = [link[1].replace(".md", "") for link in related_links]

        # Auto-generate keywords from title and category
        if metadata["title"]:
            title_words = re.findall(r"\w+", metadata["title"].lower())
            metadata["keywords"].extend(title_words)
        if metadata["category"]:
            cat_words = re.findall(r"\w+", metadata["category"].lower())
            metadata["keywords"].extend(cat_words)

        # Remove duplicates
        metadata["keywords"] = list(set(metadata["keywords"]))

        return metadata

    def generate_man_name(self, md_file, metadata):
        """Generate man page name from file and metadata"""
        # Handle missing metadata
        if metadata is None:
            metadata = {}
        
        # Category prefix
        category = metadata.get("category", "misc")
        if category is None:
            category = "misc"
        category = category.lower().replace(" ", "-")

        # Base name from file (without .md)
        base_name = md_file.stem

        # Format: aria-category-name.7
        man_name = f"aria-{category}-{base_name}"

        # Clean up
        man_name = re.sub(r"[^a-z0-9-]", "-", man_name.lower())
        man_name = re.sub(r"-+", "-", man_name)  # Collapse multiple dashes
        man_name = man_name.strip("-")

        return man_name

    def convert_markdown_to_groff(self, md_content, metadata, man_name):
        """Convert markdown content to groff format"""

        # Start with man page header
        title = metadata.get("title", man_name).upper()
        category = metadata.get("category", "Miscellaneous")

        groff = []
        groff.append(
            f'.TH "{man_name.upper()}" "7" "{self.date}" "Aria {self.version}" "Aria Programming Manual"'
        )
        groff.append("")

        # NAME section
        groff.append(".SH NAME")
        purpose = metadata.get(
            "purpose", metadata.get("title", "Aria language feature")
        )
        groff.append(f"{man_name} \\- {purpose}")
        groff.append("")

        # SYNOPSIS section (if syntax available)
        if metadata.get("syntax"):
            groff.append(".SH SYNOPSIS")
            groff.append(".nf")
            groff.append(f'.B {metadata["syntax"]}')
            groff.append(".fi")
            groff.append("")

        # Convert main content
        lines = md_content.split("\n")
        in_code_block = False
        in_list = False
        current_section = None

        for line in lines:
            # Skip frontmatter and metadata sections
            if (
                line.startswith("**Category**:")
                or line.startswith("**Syntax**:")
                or line.startswith("**Purpose**:")
            ):
                continue
            if line.startswith("---"):
                continue

            # Handle section headers
            if line.startswith("## "):
                section_name = line[3:].strip()

                # Skip "Overview" - goes into DESCRIPTION
                if section_name == "Overview":
                    current_section = "DESCRIPTION"
                    groff.append(".SH DESCRIPTION")
                    continue

                # Special handling for "Related"
                if section_name == "Related":
                    continue  # We'll handle this in SEE ALSO

                # Map common sections
                section_map = {
                    "Syntax": "SYNTAX",
                    "Examples": "EXAMPLES",
                    "Usage": "USAGE",
                    "Best Practices": "BEST PRACTICES",
                    "Common Patterns": "COMMON PATTERNS",
                    "Use Cases": "USE CASES",
                }

                groff_section = section_map.get(section_name, section_name.upper())
                groff.append(f".SH {groff_section}")
                current_section = groff_section
                continue

            # Handle subsection headers (###)
            if line.startswith("### "):
                subsection_name = line[4:].strip()
                groff.append(f".SS {subsection_name}")
                continue

            # Code blocks
            if line.startswith("```"):
                if not in_code_block:
                    groff.append(".nf")  # No fill
                    groff.append(".RS")  # Indent
                    in_code_block = True
                else:
                    groff.append(".RE")  # End indent
                    groff.append(".fi")  # Fill
                    in_code_block = False
                continue

            if in_code_block:
                # Escape leading dots in code
                if line.startswith("."):
                    line = "\\&" + line
                groff.append(line)
                continue

            # Lists
            if line.startswith("- ") or line.startswith("* "):
                if not in_list:
                    groff.append(".IP \\[bu] 2")
                    in_list = True
                else:
                    groff.append(".IP \\[bu] 2")
                groff.append(line[2:])
                continue
            elif in_list and not line.strip():
                in_list = False
                groff.append("")
                continue

            # Inline formatting
            # Bold: **text** -> \fBtext\fR
            line = re.sub(r"\*\*(.+?)\*\*", r"\\fB\\1\\fR", line)
            # Italic: *text* -> \fItext\fR
            line = re.sub(r"(?<!\*)\*([^\*]+?)\*(?!\*)", r"\\fI\\1\\fR", line)
            # Code: `text` -> \fCtext\fR
            line = re.sub(r"`(.+?)`", r"\\fC\\1\\fR", line)

            # Links: [text](url) -> text (we'll add to SEE ALSO instead)
            line = re.sub(r"\[([^\]]+)\]\([^\)]+\)", r"\\fI\\1\\fR", line)

            # Escape leading dots
            if line.startswith("."):
                line = "\\&" + line

            groff.append(line)

        # SEE ALSO section
        if metadata.get("related"):
            groff.append("")
            groff.append(".SH SEE ALSO")

            related_refs = []
            for related_file in metadata["related"]:
                # Generate man page name for related file
                related_man = f"aria-{related_file.replace('_', '-')}"
                related_refs.append(f".BR {related_man} (7)")

            groff.extend(related_refs)

        # KEYWORDS section (for apropos indexing)
        if metadata.get("keywords"):
            groff.append("")
            groff.append(".SH KEYWORDS")
            keywords = ", ".join(metadata["keywords"][:20])  # Limit to 20
            groff.append(keywords)

        return "\n".join(groff)

    def scan_all_files(self):
        """Scan all markdown files and build index"""
        print(f"Scanning {self.source_dir} for markdown files...")

        md_files = list(self.source_dir.rglob("*.md"))
        print(f"Found {len(md_files)} markdown files")

        for md_file in md_files:
            # Skip README and non-guide files
            if md_file.name == "README.md":
                continue

            metadata = self.extract_metadata(md_file)
            man_name = self.generate_man_name(md_file, metadata)

            self.file_map[md_file] = {"metadata": metadata, "man_name": man_name}

            # Build category index
            category = metadata.get("category", "Miscellaneous")
            if category not in self.category_map:
                self.category_map[category] = []
            self.category_map[category].append(man_name)

            # Build keyword index
            for keyword in metadata.get("keywords", []):
                if keyword not in self.keyword_index:
                    self.keyword_index[keyword] = []
                self.keyword_index[keyword].append(man_name)

        print(f"Indexed {len(self.file_map)} files")
        print(f"Categories: {len(self.category_map)}")
        print(f"Keywords: {len(self.keyword_index)}")

        return self.file_map

    def convert_file(self, md_file):
        """Convert a single markdown file to man page"""
        file_info = self.file_map.get(md_file)
        if not file_info:
            print(f"Warning: {md_file} not in file map")
            return None

        metadata = file_info["metadata"]
        man_name = file_info["man_name"]

        # Read markdown content
        with open(md_file, "r") as f:
            md_content = f.read()

        # Convert to groff
        groff_content = self.convert_markdown_to_groff(md_content, metadata, man_name)

        # Write to output file
        output_file = self.output_dir / f"{man_name}.7"
        with open(output_file, "w") as f:
            f.write(groff_content)

        # Compress with gzip
        subprocess.run(["gzip", "-f", str(output_file)], check=True)

        return f"{man_name}.7.gz"

    def convert_all(self):
        """Convert all markdown files to man pages"""
        print("\nConverting markdown files to man pages...")

        converted = []
        failed = []

        for md_file in self.file_map.keys():
            try:
                result = self.convert_file(md_file)
                if result:
                    converted.append(result)
                    if len(converted) % 50 == 0:
                        print(f"  Converted {len(converted)} files...")
            except Exception as e:
                print(f"Error converting {md_file}: {e}")
                failed.append(md_file)

        print(f"\nConversion complete!")
        print(f"  Success: {len(converted)}")
        print(f"  Failed: {len(failed)}")

        return converted, failed

    def generate_index(self, output_file):
        """Generate index of all man pages"""
        index = {
            "version": self.version,
            "date": self.date,
            "categories": self.category_map,
            "keywords": self.keyword_index,
            "total_pages": len(self.file_map),
        }

        with open(output_file, "w") as f:
            json.dump(index, f, indent=2)

        print(f"\nGenerated index: {output_file}")
        return index


def main():
    if len(sys.argv) < 3:
        print("Usage: md2man.py <source_dir> <output_dir> [version]")
        sys.exit(1)

    source_dir = sys.argv[1]
    output_dir = sys.argv[2]
    version = sys.argv[3] if len(sys.argv) > 3 else "0.1.0"

    converter = ManPageConverter(source_dir, output_dir, version)

    # Scan all files
    converter.scan_all_files()

    # Convert all files
    converted, failed = converter.convert_all()

    # Generate index
    index_file = Path(output_dir) / "index.json"
    converter.generate_index(index_file)

    print(f"\n✓ Man page generation complete!")
    print(f"  Output directory: {output_dir}")
    print(f"  Man pages: {len(converted)}")


if __name__ == "__main__":
    main()
