#!/bin/bash
# Aria Man Pages Uninstallation Script

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
INSTALL_PREFIX="${PREFIX:-/usr/local}"
MAN_DIR="$INSTALL_PREFIX/share/man/man7"

echo -e "${YELLOW}Aria Man Pages Uninstaller${NC}"
echo "========================================"
echo

# Check if running as root for system-wide uninstall
if [ "$INSTALL_PREFIX" = "/usr" ] || [ "$INSTALL_PREFIX" = "/usr/local" ]; then
    if [ "$EUID" -ne 0 ]; then
        echo -e "${YELLOW}Warning: Uninstalling from $INSTALL_PREFIX requires root privileges${NC}"
        echo "Please run with sudo:"
        echo "  sudo $0"
        exit 1
    fi
fi

# Check if man pages are installed
INSTALLED_COUNT=$(find "$MAN_DIR" -name 'aria-*.7.gz' 2>/dev/null | wc -l)
if [ "$INSTALLED_COUNT" -eq 0 ]; then
    echo -e "${YELLOW}No Aria man pages found in $MAN_DIR${NC}"
    echo "Nothing to uninstall."
    exit 0
fi

echo "Found $INSTALLED_COUNT Aria man pages in $MAN_DIR"
echo

# Confirm uninstall
read -p "Remove all Aria man pages? [y/N] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Uninstall cancelled."
    exit 0
fi

# Remove man pages
echo "Removing man pages..."
rm -f "$MAN_DIR"/aria-*.7.gz
rm -f "$MAN_DIR"/aria.7.gz

# Remove index files
if [ -f "$MAN_DIR/whatis" ]; then
    echo "Removing index files..."
    rm -f "$MAN_DIR/whatis"
fi

# Update man database
echo "Updating man database..."
if command -v mandb >/dev/null 2>&1; then
    mandb -q 2>/dev/null || true
elif command -v makewhatis >/dev/null 2>&1; then
    makewhatis "$INSTALL_PREFIX/share/man" 2>/dev/null || true
fi

echo
echo -e "${GREEN}✓ Uninstall complete!${NC}"
echo "Removed $INSTALLED_COUNT man pages from $MAN_DIR"
