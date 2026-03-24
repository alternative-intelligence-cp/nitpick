#!/bin/bash
# build-deb.sh — Build Aria .deb package from pre-compiled binaries
#
# Usage: ./build-deb.sh [version]
#
# This script creates a .deb package from the current build artifacts.
# It does NOT recompile — run cmake build first.
#
# Prerequisites:
#   - ariac and aria-ls built in build/ directory
#   - aria-safety built in tools/aria-safety/
#   - dpkg-deb available

set -euo pipefail

VERSION="${1:-0.2.2}"
ARCH="amd64"
PKG="aria_${VERSION}-1_${ARCH}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Building aria ${VERSION} .deb package..."

# Verify binaries exist
for bin in "$REPO_ROOT/build/ariac" "$REPO_ROOT/build/aria-ls"; do
    if [ ! -f "$bin" ]; then
        echo "Error: $bin not found. Build first with: cd build && cmake --build . --target ariac aria-ls"
        exit 1
    fi
done

if [ ! -f "$REPO_ROOT/tools/aria-safety/aria-safety" ]; then
    echo "Building aria-safety..."
    cd "$REPO_ROOT/tools/aria-safety"
    gcc -O2 -Wall -Wextra -std=c99 -o aria-safety aria_safety.c
fi

# Create package directory structure
STAGING="$REPO_ROOT/build/$PKG"
rm -rf "$STAGING"
mkdir -p "$STAGING/DEBIAN"
mkdir -p "$STAGING/usr/bin"
mkdir -p "$STAGING/usr/lib/aria/tools"
mkdir -p "$STAGING/usr/lib/aria/stdlib"
mkdir -p "$STAGING/usr/lib/aria/packages"

# DEBIAN/control
cat > "$STAGING/DEBIAN/control" << EOF
Package: aria
Version: ${VERSION}-1
Architecture: ${ARCH}
Maintainer: Aria Language Team <aria@ailp.dev>
Depends: llvm-20, lld-20
Suggests: aria-man-pages
Section: devel
Priority: optional
Homepage: https://github.com/randyeramallthetime/aria
Description: Aria programming language compiler and toolchain
 The Aria programming language compiler (ariac) and supporting tools.
 Includes ariac (LLVM 20 backend), aria-ls (LSP server),
 aria-safety (static audit), and ecosystem libraries.
EOF

# Install binaries
install -m 755 "$REPO_ROOT/build/ariac" "$STAGING/usr/bin/ariac"
install -m 755 "$REPO_ROOT/build/aria-ls" "$STAGING/usr/bin/aria-ls"
install -m 755 "$REPO_ROOT/tools/aria-safety/aria-safety" "$STAGING/usr/bin/aria-safety"

# Install MCP server
if [ -f "$REPO_ROOT/tools/aria-mcp/aria_mcp.py" ]; then
    install -m 755 "$REPO_ROOT/tools/aria-mcp/aria_mcp.py" "$STAGING/usr/lib/aria/tools/aria_mcp.py"
fi

# Install stdlib
if [ -d "$REPO_ROOT/stdlib" ]; then
    cp -r "$REPO_ROOT/stdlib/"* "$STAGING/usr/lib/aria/stdlib/" 2>/dev/null || true
fi

# Install package libraries
for pkg_dir in "$REPO_ROOT/aria_ecosystem/aria_packages/"*/; do
    [ -d "$pkg_dir" ] || continue
    pkgname=$(basename "$pkg_dir")
    if [ -d "$pkg_dir/src" ] && ls "$pkg_dir/src/"* >/dev/null 2>&1; then
        mkdir -p "$STAGING/usr/lib/aria/packages/$pkgname"
        cp -r "$pkg_dir/src/"* "$STAGING/usr/lib/aria/packages/$pkgname/"
    fi
done

# Calculate installed size
SIZE_KB=$(du -sk "$STAGING" | cut -f1)
echo "Installed-Size: ${SIZE_KB}" >> "$STAGING/DEBIAN/control"

# Build the .deb
dpkg-deb --build "$STAGING" "$REPO_ROOT/build/${PKG}.deb" 2>/dev/null || \
    dpkg-deb -b "$STAGING" "$REPO_ROOT/build/${PKG}.deb"

echo ""
echo "Package built: build/${PKG}.deb"
echo "Size: $(du -h "$REPO_ROOT/build/${PKG}.deb" | cut -f1)"
echo ""
echo "Install with: sudo dpkg -i build/${PKG}.deb"
echo "Remove with:  sudo dpkg -r aria"
