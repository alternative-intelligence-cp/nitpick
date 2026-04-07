#!/bin/bash
# =====================================================================
# build-deb.sh — Build Aria toolchain .deb package
# =====================================================================
#
# Usage: ./packaging/build-deb.sh [--version VERSION] [--output DIR]
#
# Builds the Aria compiler, tools, and standard library, then packages
# everything into a single .deb for Debian/Ubuntu/Mint systems.
#
# Must be run from the aria repo root (or with --repo-root).
# Expects sibling repos: ../aria-make/ and ../aria-libc/
#
# Package contents:
#   /usr/bin/ariac           compiler
#   /usr/bin/aria-ls         language server
#   /usr/bin/aria-pkg        package manager
#   /usr/bin/aria-doc        documentation generator
#   /usr/bin/aria-make       build system
#   /usr/lib/aria/stdlib/    standard library (72 modules)
#   /usr/lib/aria/libc/      C library wrappers (30 src + 18 shim)
#   /usr/lib/aria/lib/       bundled shared libraries (liburing)
#   /usr/lib/libaria_runtime.a  static runtime library
#   /etc/profile.d/aria.sh   environment setup
#
# =====================================================================
set -euo pipefail

# ── Defaults ─────────────────────────────────────────────────────────
VERSION="0.17.1"
ARCH="amd64"
OUTPUT_DIR="."
JOBS=$(nproc 2>/dev/null || echo 4)
PKG_NAME="aria"
MAINTAINER="Randy Delphi <randy@ai-liberation-platform.org>"
DESCRIPTION="Aria programming language — compiler, tools, and standard library"
HOMEPAGE="https://aria-lang.org"
REPO_ROOT=""
SKIP_BUILD=false

# ── Colors ───────────────────────────────────────────────────────────
if [ -t 1 ]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
    BLUE='\033[0;34m'; BOLD='\033[1m'; NC='\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; BLUE=''; BOLD=''; NC=''
fi

info()  { echo -e "${BLUE}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
fail()  { echo -e "${RED}[ERROR]${NC} $*" >&2; exit 1; }

# ── Parse Arguments ──────────────────────────────────────────────────
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --version VER    Package version (default: $VERSION)"
    echo "  --arch ARCH      Architecture (default: $ARCH)"
    echo "  --output DIR     Output directory for .deb (default: .)"
    echo "  --jobs N         Parallel build jobs (default: $JOBS)"
    echo "  --repo-root DIR  Path to aria repo root"
    echo "  --skip-build     Skip compilation (use existing build/)"
    echo "  -h, --help       Show this help"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)    VERSION="$2"; shift 2 ;;
        --arch)       ARCH="$2"; shift 2 ;;
        --output)     OUTPUT_DIR="$2"; shift 2 ;;
        --jobs)       JOBS="$2"; shift 2 ;;
        --repo-root)  REPO_ROOT="$2"; shift 2 ;;
        --skip-build) SKIP_BUILD=true; shift ;;
        -h|--help)    usage ;;
        *)            fail "Unknown option: $1 (try --help)" ;;
    esac
done

# ── Resolve Paths ────────────────────────────────────────────────────
if [[ -z "$REPO_ROOT" ]]; then
    # Try to detect: if run from repo root, use PWD; if from packaging/, go up
    if [[ -f "CMakeLists.txt" && -d "stdlib" ]]; then
        REPO_ROOT="$(pwd)"
    elif [[ -f "../CMakeLists.txt" && -d "../stdlib" ]]; then
        REPO_ROOT="$(cd .. && pwd)"
    else
        fail "Cannot find aria repo root. Use --repo-root or run from repo root."
    fi
fi

REPO_ROOT="$(cd "$REPO_ROOT" && pwd)"
ARIA_MAKE_DIR="$REPO_ROOT/../aria-make"
ARIA_LIBC_DIR="$REPO_ROOT/../aria-libc"
BUILD_DIR="$REPO_ROOT/build"
STAGING="$(mktemp -d /tmp/aria-deb-staging.XXXXXX)"
DEB_NAME="${PKG_NAME}_${VERSION}-1_${ARCH}.deb"

# Cleanup on exit
cleanup() { rm -rf "$STAGING"; }
trap cleanup EXIT

# ── Prerequisite Checks ──────────────────────────────────────────────
info "Checking prerequisites..."

[[ -f "$REPO_ROOT/CMakeLists.txt" ]] || fail "CMakeLists.txt not found in $REPO_ROOT"
[[ -d "$REPO_ROOT/stdlib" ]]         || fail "stdlib/ not found in $REPO_ROOT"
[[ -d "$ARIA_MAKE_DIR" ]]            || fail "aria-make repo not found at $ARIA_MAKE_DIR"
[[ -d "$ARIA_LIBC_DIR" ]]            || fail "aria-libc repo not found at $ARIA_LIBC_DIR"

command -v cmake    >/dev/null 2>&1 || fail "cmake not found"
command -v make     >/dev/null 2>&1 || fail "make not found"
command -v dpkg-deb >/dev/null 2>&1 || fail "dpkg-deb not found (install dpkg)"
command -v fakeroot >/dev/null 2>&1 || warn "fakeroot not found — package may have wrong ownership"

ok "Prerequisites satisfied"

# ── Banner ───────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}═══════════════════════════════════════════════════════${NC}"
echo -e "${BOLD}  Building Aria .deb package v${VERSION} (${ARCH})${NC}"
echo -e "${BOLD}═══════════════════════════════════════════════════════${NC}"
echo ""

# ── Step 1: Build Compiler ───────────────────────────────────────────
if [[ "$SKIP_BUILD" == true ]]; then
    info "Skipping build (--skip-build)"
    [[ -f "$BUILD_DIR/ariac" ]] || fail "No ariac binary in $BUILD_DIR — build first"
else
    info "[1/6] Building Aria compiler..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    if [[ ! -f "Makefile" ]]; then
        cmake "$REPO_ROOT" -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -5
    fi
    make -j"$JOBS" 2>&1 | tail -3
    ok "Compiler built: $(ls -lh ariac | awk '{print $5}')"
fi

# ── Step 2: Build aria-make ──────────────────────────────────────────
info "[2/6] Building aria-make..."
ARIA_MAKE_BUILD="$ARIA_MAKE_DIR/build"
mkdir -p "$ARIA_MAKE_BUILD"
cd "$ARIA_MAKE_BUILD"
if [[ ! -f "Makefile" ]]; then
    cmake "$ARIA_MAKE_DIR" -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -3
fi
make -j"$JOBS" 2>&1 | tail -3
[[ -f "aria_make" ]] || fail "aria_make binary not found after build"
ok "aria-make built"

# ── Step 3: Stage Files ─────────────────────────────────────────────
info "[3/6] Staging package contents..."
cd "$REPO_ROOT"

# --- Binaries ---
install -Dm755 "$BUILD_DIR/ariac"    "$STAGING/usr/bin/ariac"
install -Dm755 "$BUILD_DIR/aria-ls"  "$STAGING/usr/bin/aria-ls"
install -Dm755 "$BUILD_DIR/aria-pkg" "$STAGING/usr/bin/aria-pkg"
install -Dm755 "$BUILD_DIR/aria-doc" "$STAGING/usr/bin/aria-doc"
# aria-make binary has underscore in build, hyphen when installed
install -Dm755 "$ARIA_MAKE_BUILD/aria_make" "$STAGING/usr/bin/aria-make"

# Optional binaries (include if they exist)
for opt_bin in aria-mcp aria-safety; do
    if [[ -f "$BUILD_DIR/$opt_bin" ]]; then
        install -Dm755 "$BUILD_DIR/$opt_bin" "$STAGING/usr/bin/$opt_bin"
    fi
done

BIN_COUNT=$(ls "$STAGING/usr/bin/" | wc -l)

# --- Bundled shared libraries (liburing from third_party) ---
LIBURING_DIR="$REPO_ROOT/third_party/liburing-master/install/lib"
if [[ -d "$LIBURING_DIR" ]]; then
    mkdir -p "$STAGING/usr/lib/aria/lib"
    cp -P "$LIBURING_DIR"/liburing.so.2* "$STAGING/usr/lib/aria/lib/" 2>/dev/null || true
    # Also copy the liburing-ffi if present
    cp -P "$LIBURING_DIR"/liburing-ffi.so.2* "$STAGING/usr/lib/aria/lib/" 2>/dev/null || true
    ok "Bundled liburing in /usr/lib/aria/lib/"
fi

# --- Static runtime library ---
if [[ -f "$BUILD_DIR/libaria_runtime.a" ]]; then
    install -Dm644 "$BUILD_DIR/libaria_runtime.a" "$STAGING/usr/lib/libaria_runtime.a"
fi

# --- Standard library ---
mkdir -p "$STAGING/usr/lib/aria/stdlib"
STDLIB_COUNT=0
for f in "$REPO_ROOT"/stdlib/*.aria; do
    [[ -f "$f" ]] || continue
    install -Dm644 "$f" "$STAGING/usr/lib/aria/stdlib/$(basename "$f")"
    STDLIB_COUNT=$((STDLIB_COUNT + 1))
done

# --- aria-libc ---
mkdir -p "$STAGING/usr/lib/aria/libc/src"
mkdir -p "$STAGING/usr/lib/aria/libc/shim"
LIBC_SRC=0
for f in "$ARIA_LIBC_DIR"/src/*.aria; do
    [[ -f "$f" ]] || continue
    install -Dm644 "$f" "$STAGING/usr/lib/aria/libc/src/$(basename "$f")"
    LIBC_SRC=$((LIBC_SRC + 1))
done
LIBC_SHIM=0
for f in "$ARIA_LIBC_DIR"/shim/*.a; do
    [[ -f "$f" ]] || continue
    install -Dm644 "$f" "$STAGING/usr/lib/aria/libc/shim/$(basename "$f")"
    LIBC_SHIM=$((LIBC_SHIM + 1))
done

# --- Man pages (if they exist) ---
MAN_COUNT=0
if [[ -d "$REPO_ROOT/aria_ecosystem/man_pages/build/man7" ]]; then
    mkdir -p "$STAGING/usr/share/man/man7"
    for f in "$REPO_ROOT"/aria_ecosystem/man_pages/build/man7/*.7.gz; do
        [[ -f "$f" ]] || continue
        install -Dm644 "$f" "$STAGING/usr/share/man/man7/$(basename "$f")"
        MAN_COUNT=$((MAN_COUNT + 1))
    done
fi

# --- Environment profile ---
install -Dm644 /dev/stdin "$STAGING/etc/profile.d/aria.sh" <<'PROFILE'
# Aria programming language environment
export ARIA_HOME="/usr/lib/aria"
PROFILE

# --- ld.so.conf.d for bundled libs ---
if [[ -d "$STAGING/usr/lib/aria/lib" ]]; then
    install -Dm644 /dev/stdin "$STAGING/etc/ld.so.conf.d/aria.conf" <<'LDCONF'
# Aria bundled shared libraries
/usr/lib/aria/lib
LDCONF
fi

# --- Documentation ---
mkdir -p "$STAGING/usr/share/doc/aria"
cat > "$STAGING/usr/share/doc/aria/copyright" <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: aria
Upstream-Contact: ${MAINTAINER}
Source: ${HOMEPAGE}

Files: *
Copyright: 2024-2026 Randy Delphi
License: MIT
 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:
 .
 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 .
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
EOF

ok "Staged: ${BIN_COUNT} binaries, ${STDLIB_COUNT} stdlib, ${LIBC_SRC} libc src, ${LIBC_SHIM} libc shim, ${MAN_COUNT} man pages"

# ── Step 4: Generate DEBIAN Control Files ────────────────────────────
info "[4/6] Generating package metadata..."
mkdir -p "$STAGING/DEBIAN"

# Calculate installed size in KB
INSTALLED_SIZE=$(du -sk "$STAGING" --exclude=DEBIAN | awk '{sum+=$1} END{print sum}')

# Runtime dependencies (from ldd analysis of ariac + tools)
# LLVM is statically linked — not needed at runtime
# liburing is bundled — not needed from system
DEPENDS="libc6 (>= 2.35), libstdc++6 (>= 12), zlib1g (>= 1:1.2.11), libzstd1 (>= 1.5.0), libz3-4 (>= 4.8), libatomic1 (>= 10)"

# ── DEBIAN/control ───────────────────────────────────────────────────
cat > "$STAGING/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${VERSION}-1
Architecture: ${ARCH}
Maintainer: ${MAINTAINER}
Installed-Size: ${INSTALLED_SIZE}
Depends: ${DEPENDS}
Section: devel
Priority: optional
Homepage: ${HOMEPAGE}
Description: ${DESCRIPTION}
 Aria is a modern, safe, compiled programming language designed for
 systems programming with AI-native features.
 .
 This package includes:
  - ariac: the Aria compiler
  - aria-make: build system
  - aria-ls: language server (LSP)
  - aria-pkg: package manager
  - aria-doc: documentation generator
  - Standard library (${STDLIB_COUNT} modules)
  - C library wrappers (aria-libc)
  - Static runtime library
EOF

# ── DEBIAN/postinst ──────────────────────────────────────────────────
cat > "$STAGING/DEBIAN/postinst" <<'EOF'
#!/bin/bash
set -e
# Update shared library cache (for bundled liburing)
if [ -f /etc/ld.so.conf.d/aria.conf ]; then
    ldconfig 2>/dev/null || true
fi
EOF
chmod 755 "$STAGING/DEBIAN/postinst"

# ── DEBIAN/prerm ─────────────────────────────────────────────────────
cat > "$STAGING/DEBIAN/prerm" <<'EOF'
#!/bin/bash
set -e
EOF
chmod 755 "$STAGING/DEBIAN/prerm"

# ── DEBIAN/postrm ────────────────────────────────────────────────────
cat > "$STAGING/DEBIAN/postrm" <<'EOF'
#!/bin/bash
set -e
# Refresh library cache
ldconfig 2>/dev/null || true
# Remove empty aria directories
rmdir /usr/lib/aria/libc/shim 2>/dev/null || true
rmdir /usr/lib/aria/libc/src 2>/dev/null || true
rmdir /usr/lib/aria/libc 2>/dev/null || true
rmdir /usr/lib/aria/stdlib 2>/dev/null || true
rmdir /usr/lib/aria/lib 2>/dev/null || true
rmdir /usr/lib/aria 2>/dev/null || true
EOF
chmod 755 "$STAGING/DEBIAN/postrm"

# ── DEBIAN/conffiles ─────────────────────────────────────────────────
cat > "$STAGING/DEBIAN/conffiles" <<'EOF'
/etc/profile.d/aria.sh
EOF

ok "DEBIAN control files generated"

# ── Step 5: Build .deb ───────────────────────────────────────────────
info "[5/6] Building .deb package..."
mkdir -p "$OUTPUT_DIR"

# Use fakeroot if available for correct ownership
if command -v fakeroot >/dev/null 2>&1; then
    fakeroot dpkg-deb --build --root-owner-group "$STAGING" "$OUTPUT_DIR/$DEB_NAME"
else
    dpkg-deb --build --root-owner-group "$STAGING" "$OUTPUT_DIR/$DEB_NAME"
fi

ok "Package built: $OUTPUT_DIR/$DEB_NAME"

# ── Step 6: Verify Package ──────────────────────────────────────────
info "[6/6] Verifying package..."
echo ""
echo -e "${BOLD}Package Info:${NC}"
dpkg-deb --info "$OUTPUT_DIR/$DEB_NAME" 2>/dev/null | grep -E "Package|Version|Architecture|Installed-Size|Depends|Description"
echo ""
echo -e "${BOLD}Package Contents:${NC}"
dpkg-deb --contents "$OUTPUT_DIR/$DEB_NAME" 2>/dev/null | head -40
TOTAL_FILES=$(dpkg-deb --contents "$OUTPUT_DIR/$DEB_NAME" 2>/dev/null | wc -l)
echo "  ... (${TOTAL_FILES} entries total)"

# File size
DEB_SIZE=$(ls -lh "$OUTPUT_DIR/$DEB_NAME" | awk '{print $5}')
echo ""
echo -e "${BOLD}File:${NC} $OUTPUT_DIR/$DEB_NAME (${DEB_SIZE})"

# Optional lintian check
if command -v lintian >/dev/null 2>&1; then
    echo ""
    echo -e "${BOLD}Lintian Check:${NC}"
    lintian "$OUTPUT_DIR/$DEB_NAME" 2>/dev/null | head -20 || true
fi

echo ""
echo -e "${BOLD}═══════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}  .deb package ready!${NC}"
echo -e "  Install:   ${BOLD}sudo dpkg -i $OUTPUT_DIR/$DEB_NAME${NC}"
echo -e "  Remove:    ${BOLD}sudo dpkg -r aria${NC}"
echo -e "${BOLD}═══════════════════════════════════════════════════════${NC}"
