#!/bin/bash
# Aria Man Pages Installation Script

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
INSTALL_PREFIX="${PREFIX:-/usr/local}"
MAN_DIR="$INSTALL_PREFIX/share/man/man7"
BUILD_DIR="build/man7"

echo -e "${GREEN}Aria Man Pages Installer${NC}"
echo "========================================"
echo

# Check if running as root for system-wide install
if [ "$INSTALL_PREFIX" = "/usr" ] || [ "$INSTALL_PREFIX" = "/usr/local" ]; then
    if [ "$EUID" -ne 0 ]; then
        echo -e "${YELLOW}Warning: Installing to $INSTALL_PREFIX requires root privileges${NC}"
        echo "Please run with sudo or set PREFIX to a user directory:"
        echo "  sudo $0"
        echo "  PREFIX=~/.local $0"
        exit 1
    fi
fi

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found${NC}"
    echo "Please run 'make all' first to build man pages"
    exit 1
fi

# Count man pages
MAN_COUNT=$(find "$BUILD_DIR" -name '*.7.gz' | wc -l)
if [ "$MAN_COUNT" -eq 0 ]; then
    echo -e "${RED}Error: No man pages found in $BUILD_DIR${NC}"
    echo "Please run 'make all' first"
    exit 1
fi

echo "Found $MAN_COUNT man pages to install"
echo "Install directory: $MAN_DIR"
echo

# Create directory if needed
echo "Creating man directory..."
mkdir -p "$MAN_DIR"

# Install man pages
echo "Installing man pages..."
install -m 644 "$BUILD_DIR"/*.7.gz "$MAN_DIR/"

# Install index files
if [ -f "$BUILD_DIR/whatis" ]; then
    echo "Installing whatis database..."
    install -m 644 "$BUILD_DIR/whatis" "$MAN_DIR/"
fi

# Update man database
echo "Updating man database..."
if command -v mandb >/dev/null 2>&1; then
    mandb -q 2>/dev/null || true
elif command -v makewhatis >/dev/null 2>&1; then
    makewhatis "$INSTALL_PREFIX/share/man" 2>/dev/null || true
else
    echo -e "${YELLOW}Warning: Could not update man database (mandb/makewhatis not found)${NC}"
fi

echo
echo -e "${GREEN}✓ Installation complete!${NC}"
echo
echo "Try these commands:"
echo "  man aria              - View main index"
echo "  man aria-types-int32  - View int32 type documentation"
echo "  apropos aria          - Search all Aria topics"
echo "  man -k memory         - Search for memory-related pages"
echo

# Verification
echo "Verifying installation..."
if man -w aria >/dev/null 2>&1; then
    echo -e "${GREEN}✓ Man pages are accessible${NC}"
else
    echo -e "${YELLOW}Warning: Man pages may not be in your MANPATH${NC}"
    echo "Add to your ~/.bashrc or ~/.zshrc:"
    echo "  export MANPATH=\"$INSTALL_PREFIX/share/man:\$MANPATH\""
fi

echo
echo "Installed files:"
echo "  Man pages: $MAN_DIR/aria-*.7.gz ($MAN_COUNT files)"
if [ -f "$MAN_DIR/whatis" ]; then
    echo "  Index: $MAN_DIR/whatis"
fi
