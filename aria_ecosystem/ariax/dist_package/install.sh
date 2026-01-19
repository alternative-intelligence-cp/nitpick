#!/bin/bash
# AriaX Distribution Installation Script
# Installs AriaX kernel, VTE, Bash, and all utilities

set -e  # Exit on error

INSTALL_PREFIX="/usr/local"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║              AriaX Linux Installation Script                 ║"
echo "║          Alternative Intelligence Liberation Platform        ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "⚠️  Please run as root (use sudo)"
    exit 1
fi

echo "📦 Installing AriaX utilities..."
install -m 755 -d "$INSTALL_PREFIX/bin"
cp -v "$SCRIPT_DIR/bin"/* "$INSTALL_PREFIX/bin/" || echo "⚠️  Some utilities may already exist"

echo ""
echo "📚 Installing shared libraries..."
if [ -d "$SCRIPT_DIR/lib" ]; then
    install -m 755 -d "$INSTALL_PREFIX/lib/ariax"
    cp -v "$SCRIPT_DIR/lib"/*.so* "$INSTALL_PREFIX/lib/ariax/" || true
    # Create symlinks for versioned libraries
    cd "$INSTALL_PREFIX/lib/ariax"
    for lib in *.so.*.*.* ; do
        if [ -f "$lib" ]; then
            base=$(echo "$lib" | sed 's/\.so\..*/.so/')
            major=$(echo "$lib" | sed 's/\.so\.\([0-9]*\)\..*/\.so\.\1/')
            ln -sf "$lib" "$major"
            ln -sf "$lib" "$base"
        fi
    done
    cd - > /dev/null
    # Add to ld.so.conf
    echo "$INSTALL_PREFIX/lib/ariax" > /etc/ld.so.conf.d/ariax.conf
fi

echo ""
echo "📚 Installing documentation..."
install -m 755 -d "$INSTALL_PREFIX/share/ariax"
cp -rv "$SCRIPT_DIR/share/ariax"/* "$INSTALL_PREFIX/share/ariax/" || true

echo ""
echo "⚙️  Setting up shell configuration..."
install -m 755 -d /etc/skel
cp -v "$SCRIPT_DIR/etc/skel/.bashrc_ariax" /etc/skel/

# Add to existing user bashrc if not already there
if [ -n "$SUDO_USER" ]; then
    USER_HOME=$(getent passwd "$SUDO_USER" | cut -d: -f6)
    if [ -f "$USER_HOME/.bashrc" ]; then
        if ! grep -q "bashrc_ariax" "$USER_HOME/.bashrc" 2>/dev/null; then
            echo "" >> "$USER_HOME/.bashrc"
            echo "# AriaX Six-Stream Configuration" >> "$USER_HOME/.bashrc"
            echo "if [ -f ~/.bashrc_ariax ]; then" >> "$USER_HOME/.bashrc"
            echo "    . ~/.bashrc_ariax" >> "$USER_HOME/.bashrc"
            echo "fi" >> "$USER_HOME/.bashrc"
            echo "✅ Added AriaX config to $SUDO_USER's .bashrc"
        fi
    fi
    cp -v "$SCRIPT_DIR/etc/skel/.bashrc_ariax" "$USER_HOME/" || true
    chown "$SUDO_USER:$SUDO_USER" "$USER_HOME/.bashrc_ariax" 2>/dev/null || true
fi

echo ""
echo "🔄 Updating library cache..."
ldconfig

echo ""
echo "✅ AriaX installation complete!"
echo ""
echo "📋 Installed utilities ($(ls "$INSTALL_PREFIX/bin" | grep -E '^a' | wc -l) total):"
ls -1 "$INSTALL_PREFIX/bin" | grep -E '^a' | pr -t -3

echo ""
echo "🚀 Quick start:"
echo "  1. Open a new terminal (or run: source ~/.bashrc)"
echo "  2. Run: ariash (AriaX shell)"
echo "  3. Try: test_six_streams"
echo "  4. Help: cat $INSTALL_PREFIX/share/ariax/README.md"
echo ""
echo "📖 Documentation: $INSTALL_PREFIX/share/ariax/README.md"
