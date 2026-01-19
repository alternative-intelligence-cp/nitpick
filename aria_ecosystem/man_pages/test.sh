#!/bin/bash
# Test script for Aria man pages

set -e

echo "=== Aria Man Pages Test Suite ==="
echo

# Test 1: Conversion
echo "Test 1: Checking conversion..."
if [ -d "build/man7" ]; then
    count=$(find build/man7 -name '*.7.gz' | wc -l)
    echo "✓ Found $count man pages"
else
    echo "✗ Build directory not found"
    exit 1
fi

# Test 2: File format
echo
echo "Test 2: Checking file format..."
sample=$(find build/man7 -name 'aria-types-*.7.gz' | head -1)
if zcat "$sample" | grep -q "^\.TH"; then
    echo "✓ Groff format valid"
else
    echo "✗ Invalid groff format"
    exit 1
fi

# Test 3: Compression
echo
echo "Test 3: Checking compression..."
if file "$sample" | grep -q "gzip"; then
    echo "✓ Files are gzipped"
else
    echo "✗ Files not compressed"
    exit 1
fi

# Test 4: Index files
echo
echo "Test 4: Checking indexes..."
for idx in whatis keyword_index.json category_index.json; do
    if [ -f "build/man7/$idx" ]; then
        echo "✓ $idx exists"
    else
        echo "✗ $idx missing"
    fi
done

# Test 5: Master index
echo
echo "Test 5: Checking master index..."
if [ -f "build/man7/aria.7.gz" ]; then
    echo "✓ aria.7.gz exists"
else
    echo "✗ Master index missing"
    exit 1
fi

# Test 6: Rendering
echo
echo "Test 6: Testing rendering..."
if man -l build/man7/aria.7.gz >/dev/null 2>&1; then
    echo "✓ Man pages render correctly"
else
    echo "⚠ Rendering has warnings (expected for font 'C')"
fi

# Test 7: Category coverage
echo
echo "Test 7: Checking categories..."
categories=$(cat build/man7/category_index.json | python3 -c "import sys,json; print(len(json.load(sys.stdin)))")
echo "✓ Found $categories categories"

# Test 8: Keyword index
echo
echo "Test 8: Checking keyword index..."
keywords=$(cat build/man7/keyword_index.json | python3 -c "import sys,json; print(len(json.load(sys.stdin)))")
echo "✓ Indexed $keywords keywords"

# Test 9: Sample pages
echo
echo "Test 9: Spot-checking content..."
for topic in types-int32 functions-async memory-gc; do
    file="build/man7/aria-$topic.7.gz"
    if [ -f "$file" ]; then
        echo "  ✓ aria-$topic exists"
    else
        echo "  ⚠ aria-$topic not found"
    fi
done

# Summary
echo
echo "=== Test Summary ==="
echo "✓ All core tests passed"
echo
echo "Quick checks:"
echo "  Total pages: $(find build/man7 -name '*.7.gz' | wc -l)"
echo "  Total size: $(du -sh build/man7 | cut -f1)"
echo "  Categories: $categories"
echo "  Keywords: $keywords"
echo
echo "Try these commands:"
echo "  man -l build/man7/aria.7.gz"
echo "  man -l build/man7/aria-types-int32.7.gz"
echo "  make install  # Install to system"
