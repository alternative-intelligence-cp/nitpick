#!/bin/bash
# Quick start guide for Aria man pages

echo "=== Aria Man Pages - Quick Start ==="
echo
echo "This system converts 326 markdown programming guide files"
echo "to professional groff man pages for Linux systems."
echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 BASIC USAGE"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo
echo "1. Build all man pages:"
echo "   $ make all"
echo
echo "2. Test rendering:"
echo "   $ make test"
echo "   $ man -l build/man7/aria.7.gz"
echo
echo "3. Install to system (requires sudo):"
echo "   $ sudo make install"
echo
echo "4. Use the man pages:"
echo "   $ man aria                    # Main index"
echo "   $ man aria-types-int32        # Specific topic"
echo "   $ apropos aria                # Search all"
echo "   $ man -k \"async function\"     # Keyword search"
echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🔧 MAKEFILE TARGETS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo
echo "  all        - Build all man pages (default)"
echo "  check      - Validate groff syntax"
echo "  test       - Render sample pages"
echo "  stats      - Show statistics"
echo "  install    - Install to system (sudo)"
echo "  uninstall  - Remove from system (sudo)"
echo "  clean      - Remove build artifacts"
echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📁 DIRECTORY STRUCTURE"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo
tree -L 2 -I '__pycache__|*.pyc' . 2>/dev/null || find . -maxdepth 2 -type d | sort
echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📊 CURRENT STATUS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo
if [ -d "build/man7" ]; then
    pages=$(find build/man7 -name '*.7.gz' | wc -l)
    size=$(du -sh build/man7 | cut -f1)
    echo "✓ Man pages built: $pages"
    echo "✓ Total size: $size"
    
    if [ -f "build/man7/category_index.json" ]; then
        categories=$(cat build/man7/category_index.json | python3 -c "import sys,json; print(len(json.load(sys.stdin)))" 2>/dev/null || echo "?")
        echo "✓ Categories: $categories"
    fi
    
    if [ -f "build/man7/keyword_index.json" ]; then
        keywords=$(cat build/man7/keyword_index.json | python3 -c "import sys,json; print(len(json.load(sys.stdin)))" 2>/dev/null || echo "?")
        echo "✓ Keywords indexed: $keywords"
    fi
else
    echo "⚠ Not built yet - run 'make all'"
fi
echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🚀 FOR ARIAX DISTRIBUTION"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo
echo "Build Debian package:"
echo "  $ dpkg-buildpackage -us -uc"
echo "  $ sudo dpkg -i ../aria-man-pages_0.2.0-1_all.deb"
echo
echo "Package info:"
echo "  Name: aria-man-pages"
echo "  Version: 0.2.0-1"
echo "  Section: doc"
echo "  Architecture: all"
echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📖 DOCUMENTATION"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo
echo "See README.md for complete documentation"
echo "Try: cat README.md | less"
echo
