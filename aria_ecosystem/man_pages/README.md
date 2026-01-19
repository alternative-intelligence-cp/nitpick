# Aria Man Pages

This directory contains the complete man page generation system for the Aria programming language. It converts 326 markdown programming guide files into professional groff man pages for integration with the ariax Linux distribution.

## Quick Start

```bash
# Build all man pages
make all

# Test rendering
make test

# Install to system (requires sudo)
sudo make install

# Or use install script
sudo ./install.sh
```

## Structure

```
man_pages/
├── Makefile              # Build automation
├── install.sh            # Installation script
├── uninstall.sh          # Removal script
├── scripts/
│   ├── md2man.py         # Markdown to groff converter
│   └── build_index.py    # Index generation
├── build/
│   └── man7/             # Generated man pages (.7.gz)
├── debian/               # Debian package structure
│   ├── control           # Package metadata
│   ├── rules             # Build rules
│   ├── changelog         # Version history
│   └── copyright         # License info
└── templates/            # Man page templates (optional)
```

## Building Man Pages

### Full Build
```bash
make all
```

This will:
1. Convert all 326 markdown files to groff format
2. Generate cross-references (SEE ALSO sections)
3. Build keyword index for apropos searches
4. Compress to .7.gz format
5. Create master index (aria.7)

### Validation
```bash
make check    # Validate groff syntax
make test     # Test rendering
make stats    # View statistics
```

## Installation

### System-wide (Linux)
```bash
sudo make install
# or
sudo ./install.sh
```

Installs to `/usr/local/share/man/man7/` and updates man database.

### User directory
```bash
PREFIX=~/.local make install
```

### Debian Package
```bash
dpkg-buildpackage -us -uc
sudo dpkg -i ../aria-man-pages_0.1.0-1_all.deb
```

## Usage

Once installed:

```bash
# View main index
man aria

# View specific topic
man aria-types-int32
man aria-functions-async
man aria-memory-gc

# Search topics
apropos aria
man -k "aria memory"
man -k "async function"

# Browse by category
man aria  # Lists all categories
```

## Man Page Format

Each man page follows this structure:

```
NAME
    Topic name - brief description

SYNOPSIS
    Syntax examples

DESCRIPTION
    Detailed documentation

EXAMPLES
    Code samples

SEE ALSO
    Related topics (cross-referenced)

KEYWORDS
    Search terms for apropos
```

## Naming Convention

Man pages use the format: `aria-CATEGORY-TOPIC.7`

Examples:
- `aria-types-int32.7` - int32 type
- `aria-functions-async.7` - async functions
- `aria-memory-gc.7` - garbage collection
- `aria-stdlib-string.7` - string functions

## Customization

### Change version
```bash
make all VERSION=0.2.0
```

### Custom install prefix
```bash
make install INSTALL_PREFIX=/opt/aria
```

### Modify converter
Edit `scripts/md2man.py` to customize:
- Section formatting
- Cross-reference generation
- Keyword extraction
- Groff formatting

## Metadata Extraction

The converter recognizes these patterns in markdown:

```markdown
---
category: types
syntax: int32 variable = value
purpose: 32-bit signed integer
---

**Category**: types
**Syntax**: `int32 x = 42`
**Purpose**: Integer type

## Related
- [int64](int64.md)
- [uint32](uint32.md)
```

## Troubleshooting

### Man page not found
```bash
# Check installation
man -w aria-types-int32

# Update man database
sudo mandb

# Check MANPATH
echo $MANPATH
export MANPATH="/usr/local/share/man:$MANPATH"
```

### Groff errors
```bash
# Validate specific page
zcat build/man7/aria-types-int32.7.gz | groff -man -Tutf8 -z

# Check all pages
make check
```

### Build issues
```bash
# Clean and rebuild
make clean
make all

# Check Python version (requires Python 3)
python3 --version
```

## Files

- **md2man.py** - Core converter (~400 lines)
  - Extracts metadata from markdown
  - Converts to groff format
  - Generates cross-references
  - Creates keyword index
  - Compresses output

- **build_index.py** - Index builder
  - Builds whatis database
  - Generates keyword index
  - Creates category index
  - Generates master index page

- **Makefile** - Build automation
  - `make all` - Build everything
  - `make install` - Install to system
  - `make test` - Test rendering
  - `make clean` - Remove artifacts

- **install.sh** - Installation helper
  - Creates directories
  - Copies files
  - Updates man database
  - Verifies installation

## Integration with ariax

For ariax distribution integration:

1. Build debian package:
```bash
dpkg-buildpackage -us -uc
```

2. Add to ariax repository

3. Install during system setup:
```bash
apt-get install aria-man-pages
```

4. Man pages automatically available

## Contributing

To add or update man pages:

1. Edit markdown in `programming_guide/`
2. Rebuild: `make all`
3. Test: `make test`
4. Verify: `man aria-CATEGORY-TOPIC`

## License

MIT License - See debian/copyright for details

## Support

- GitHub: https://github.com/randyeramallthetime/aria_ecosystem
- Documentation: programming_guide/
- Issues: GitHub Issues
