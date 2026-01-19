#!/usr/bin/env python3
"""
Build searchable index for Aria man pages
Creates whatis database entries and keyword index
"""

import os
import sys
import json
import gzip
import re
from pathlib import Path


def extract_whatis_entry(man_file):
    """Extract NAME section for whatis database"""
    try:
        with gzip.open(man_file, "rt") as f:
            content = f.read()

        # Find .SH NAME section
        name_match = re.search(r"\.SH NAME\s+(.*?)(?=\.SH|\Z)", content, re.DOTALL)
        if name_match:
            name_section = name_match.group(1).strip()
            # Extract: name \- description
            entry_match = re.search(r"([^\s]+)\s+\\-\s+(.+)", name_section)
            if entry_match:
                name = entry_match.group(1)
                desc = entry_match.group(2).strip()
                # Remove groff formatting
                desc = re.sub(r"\\f[BIRP]", "", desc)
                desc = re.sub(r"\\&", "", desc)
                return (name, desc)
    except Exception as e:
        print(f"Error extracting whatis from {man_file}: {e}")

    return None


def build_whatis_db(build_dir):
    """Build whatis database entries"""
    print("Building whatis database entries...")

    whatis_entries = []
    man_files = list(Path(build_dir).glob("*.7.gz"))

    for man_file in man_files:
        entry = extract_whatis_entry(man_file)
        if entry:
            name, desc = entry
            whatis_entries.append(f"{name} (7) - {desc}")

    # Write whatis file
    whatis_file = Path(build_dir) / "whatis"
    with open(whatis_file, "w") as f:
        f.write("\n".join(sorted(whatis_entries)))

    print(f"Generated {len(whatis_entries)} whatis entries")
    return whatis_file


def build_keyword_index(build_dir):
    """Build keyword index for apropos searches"""
    print("Building keyword index...")

    keyword_index = {}
    man_files = list(Path(build_dir).glob("*.7.gz"))

    for man_file in man_files:
        try:
            with gzip.open(man_file, "rt") as f:
                content = f.read()

            man_name = man_file.stem.replace(".7", "")

            # Extract keywords from KEYWORDS section
            keywords_match = re.search(
                r"\.SH KEYWORDS\s+(.*?)(?=\.SH|\Z)", content, re.DOTALL
            )
            if keywords_match:
                keywords_text = keywords_match.group(1).strip()
                keywords = [k.strip() for k in keywords_text.split(",")]

                for keyword in keywords:
                    keyword = keyword.lower()
                    if keyword not in keyword_index:
                        keyword_index[keyword] = []
                    keyword_index[keyword].append(man_name)

            # Also extract from description
            desc_match = re.search(
                r"\.SH DESCRIPTION\s+(.*?)(?=\.SH|\Z)", content, re.DOTALL
            )
            if desc_match:
                desc_text = desc_match.group(1)
                # Extract meaningful words (filter common words)
                words = re.findall(r"\b[a-z]{4,}\b", desc_text.lower())
                common_words = {
                    "that",
                    "this",
                    "with",
                    "from",
                    "have",
                    "been",
                    "will",
                    "your",
                    "their",
                    "what",
                    "when",
                    "make",
                    "like",
                    "time",
                    "just",
                    "know",
                    "take",
                    "into",
                    "year",
                    "some",
                    "them",
                    "than",
                    "then",
                    "look",
                    "only",
                    "come",
                    "over",
                    "also",
                    "back",
                    "after",
                    "work",
                    "well",
                    "even",
                    "most",
                    "give",
                    "very",
                    "more",
                    "other",
                    "such",
                }

                for word in set(words):
                    if word not in common_words:
                        if word not in keyword_index:
                            keyword_index[word] = []
                        if man_name not in keyword_index[word]:
                            keyword_index[word].append(man_name)

        except Exception as e:
            print(f"Error indexing {man_file}: {e}")

    # Write keyword index
    index_file = Path(build_dir) / "keyword_index.json"
    with open(index_file, "w") as f:
        json.dump(keyword_index, f, indent=2)

    print(f"Indexed {len(keyword_index)} keywords")
    return index_file


def build_category_index(build_dir):
    """Build category-based index"""
    print("Building category index...")

    category_index = {}
    man_files = list(Path(build_dir).glob("*.7.gz"))

    for man_file in man_files:
        man_name = man_file.stem.replace(".7", "")

        # Extract category from filename (aria-CATEGORY-name)
        parts = man_name.split("-")
        if len(parts) >= 2:
            category = parts[1]
            if category not in category_index:
                category_index[category] = []
            category_index[category].append(man_name)

    # Write category index
    index_file = Path(build_dir) / "category_index.json"
    with open(index_file, "w") as f:
        json.dump(category_index, f, indent=2)

    print(f"Indexed {len(category_index)} categories")
    return index_file


def generate_index_page(build_dir):
    """Generate master index man page"""
    print("Generating index man page...")

    # Load category index
    with open(Path(build_dir) / "category_index.json") as f:
        category_index = json.load(f)

    groff = []
    groff.append(
        '.TH "ARIA" "7" "December 2025" "Aria 0.1.0" "Aria Programming Manual"'
    )
    groff.append("")
    groff.append(".SH NAME")
    groff.append("aria \\- Aria Programming Language Reference Manual")
    groff.append("")
    groff.append(".SH DESCRIPTION")
    groff.append(
        "The Aria Programming Language is a modern systems programming language with"
    )
    groff.append(
        "comprehensive memory safety, garbage collection, and low-level control."
    )
    groff.append("")
    groff.append(
        "This manual provides complete documentation for the Aria language, organized"
    )
    groff.append("by topic and feature.")
    groff.append("")
    groff.append(".SH CATEGORIES")

    # List all categories
    for category in sorted(category_index.keys()):
        pages = category_index[category]
        groff.append(f".SS {category.title()}")
        groff.append(f"({len(pages)} pages)")
        groff.append("")

        # List first few pages as examples
        for page in sorted(pages)[:5]:
            groff.append(f".BR {page} (7)")

        if len(pages) > 5:
            groff.append(f".I ... and {len(pages) - 5} more")
        groff.append("")

    groff.append(".SH SEARCHING")
    groff.append(".TP")
    groff.append(".B apropos aria")
    groff.append("Search all Aria man pages")
    groff.append(".TP")
    groff.append(".B man -k keyword")
    groff.append("Search by keyword")
    groff.append(".TP")
    groff.append(".B man aria-CATEGORY-topic")
    groff.append("View specific topic")
    groff.append("")
    groff.append(".SH EXAMPLES")
    groff.append(".TP")
    groff.append(".B man aria-types-int32")
    groff.append("View documentation for int32 type")
    groff.append(".TP")
    groff.append(".B man aria-functions-async")
    groff.append("View async function documentation")
    groff.append(".TP")
    groff.append('.B apropos "aria memory"')
    groff.append("Search for memory-related topics")
    groff.append("")
    groff.append(".SH SEE ALSO")
    groff.append(".BR ariax (1),")
    groff.append(".BR aria-intro (7)")

    # Write index page
    index_page = Path(build_dir) / "aria.7"
    with open(index_page, "w") as f:
        f.write("\n".join(groff))

    # Compress
    import subprocess

    subprocess.run(["gzip", "-f", str(index_page)], check=True)

    print("Generated aria.7.gz (master index)")
    return index_page


def main():
    if len(sys.argv) < 2:
        print("Usage: build_index.py <build_dir>")
        sys.exit(1)

    build_dir = sys.argv[1]

    # Build indexes
    build_whatis_db(build_dir)
    build_keyword_index(build_dir)
    build_category_index(build_dir)
    generate_index_page(build_dir)

    print("\n✓ Index generation complete!")


if __name__ == "__main__":
    main()
