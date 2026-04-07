#!/bin/bash
# =====================================================================
# build-rpm.sh — Build Aria toolchain .rpm package
# =====================================================================
#
# Usage: ./packaging/build-rpm.sh [--version VERSION] [--output DIR]
#
# Builds the Aria compiler, tools, and standard library, then packages
# everything into a single .rpm for Fedora/RHEL/openSUSE systems.
#
# Must be run from the aria repo root (or with --repo-root).
# Expects sibling repos: ../aria-make/ and ../aria-libc/
#
# =====================================================================
set -euo pipefail

# ── Defaults ─────────────────────────────────────────────────────────
VERSION="0.17.2"
RELEASE="1"
ARCH="x86_64"
OUTPUT_DIR="."
JOBS=$(nproc 2>/dev/null || echo 4)
PKG_NAME="aria"
MAINTAINER="Randy Delphi <randy@ai-liberation-platform.org>"
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
    echo "  --release N      RPM release number (default: $RELEASE)"
    echo "  --arch ARCH      Architecture (default: $ARCH)"
    echo "  --output DIR     Output directory for .rpm (default: .)"
    echo "  --jobs N         Parallel build jobs (default: $JOBS)"
    echo "  --repo-root DIR  Path to aria repo root"
    echo "  --skip-build     Skip compilation (use existing build/)"
    echo "  -h, --help       Show this help"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)    VERSION="$2"; shift 2 ;;
        --release)    RELEASE="$2"; shift 2 ;;
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

# RPM build tree
RPM_TOPDIR="$(mktemp -d /tmp/aria-rpm-build.XXXXXX)"
RPM_NAME="${PKG_NAME}-${VERSION}-${RELEASE}.${ARCH}.rpm"

cleanup() { rm -rf "$RPM_TOPDIR"; }
trap cleanup EXIT

# Create rpmbuild directory structure
mkdir -p "$RPM_TOPDIR"/{BUILD,RPMS,SOURCES,SPECS,SRPMS,BUILDROOT}

# Staging area (separate from rpmbuild's BUILDROOT which gets cleared)
STAGING="$RPM_TOPDIR/STAGED"
mkdir -p "$STAGING"

# ── Prerequisite Checks ──────────────────────────────────────────────
info "Checking prerequisites..."

[[ -f "$REPO_ROOT/CMakeLists.txt" ]] || fail "CMakeLists.txt not found in $REPO_ROOT"
[[ -d "$REPO_ROOT/stdlib" ]]         || fail "stdlib/ not found in $REPO_ROOT"
[[ -d "$ARIA_MAKE_DIR" ]]            || fail "aria-make repo not found at $ARIA_MAKE_DIR"
[[ -d "$ARIA_LIBC_DIR" ]]            || fail "aria-libc repo not found at $ARIA_LIBC_DIR"

command -v cmake    >/dev/null 2>&1 || fail "cmake not found"
command -v make     >/dev/null 2>&1 || fail "make not found"
command -v rpmbuild >/dev/null 2>&1 || fail "rpmbuild not found (install rpm on Debian, rpm-build on Fedora)"

ok "Prerequisites satisfied"

# ── Banner ───────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}═══════════════════════════════════════════════════════${NC}"
echo -e "${BOLD}  Building Aria .rpm package v${VERSION}-${RELEASE} (${ARCH})${NC}"
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

# ── Step 3: Stage Files ──────────────────────────────────────────────
info "[3/6] Staging package contents..."
cd "$REPO_ROOT"

# --- Binaries ---
install -Dm755 "$BUILD_DIR/ariac"    "$STAGING/usr/bin/ariac"
install -Dm755 "$BUILD_DIR/aria-ls"  "$STAGING/usr/bin/aria-ls"
install -Dm755 "$BUILD_DIR/aria-pkg" "$STAGING/usr/bin/aria-pkg"
install -Dm755 "$BUILD_DIR/aria-doc" "$STAGING/usr/bin/aria-doc"
install -Dm755 "$ARIA_MAKE_BUILD/aria_make" "$STAGING/usr/bin/aria-make"

for opt_bin in aria-mcp aria-safety; do
    if [[ -f "$BUILD_DIR/$opt_bin" ]]; then
        install -Dm755 "$BUILD_DIR/$opt_bin" "$STAGING/usr/bin/$opt_bin"
    fi
done

BIN_COUNT=$(ls "$STAGING/usr/bin/" | wc -l)

# --- Bundled shared libraries (liburing) ---
LIBURING_DIR="$REPO_ROOT/third_party/liburing-master/install/lib"
if [[ -d "$LIBURING_DIR" ]]; then
    mkdir -p "$STAGING/usr/lib64/aria/lib"
    cp -P "$LIBURING_DIR"/liburing.so.2* "$STAGING/usr/lib64/aria/lib/" 2>/dev/null || true
    cp -P "$LIBURING_DIR"/liburing-ffi.so.2* "$STAGING/usr/lib64/aria/lib/" 2>/dev/null || true
fi

# --- Static runtime library ---
if [[ -f "$BUILD_DIR/libaria_runtime.a" ]]; then
    install -Dm644 "$BUILD_DIR/libaria_runtime.a" "$STAGING/usr/lib64/libaria_runtime.a"
fi

# --- Standard library ---
mkdir -p "$STAGING/usr/lib64/aria/stdlib"
STDLIB_COUNT=0
for f in "$REPO_ROOT"/stdlib/*.aria; do
    [[ -f "$f" ]] || continue
    install -Dm644 "$f" "$STAGING/usr/lib64/aria/stdlib/$(basename "$f")"
    STDLIB_COUNT=$((STDLIB_COUNT + 1))
done

# --- aria-libc ---
mkdir -p "$STAGING/usr/lib64/aria/libc/src"
mkdir -p "$STAGING/usr/lib64/aria/libc/shim"
LIBC_SRC=0
for f in "$ARIA_LIBC_DIR"/src/*.aria; do
    [[ -f "$f" ]] || continue
    install -Dm644 "$f" "$STAGING/usr/lib64/aria/libc/src/$(basename "$f")"
    LIBC_SRC=$((LIBC_SRC + 1))
done
LIBC_SHIM=0
for f in "$ARIA_LIBC_DIR"/shim/*.a; do
    [[ -f "$f" ]] || continue
    install -Dm644 "$f" "$STAGING/usr/lib64/aria/libc/shim/$(basename "$f")"
    LIBC_SHIM=$((LIBC_SHIM + 1))
done

# --- Environment profile ---
install -Dm644 /dev/stdin "$STAGING/etc/profile.d/aria.sh" <<'PROFILE'
# Aria programming language environment
export ARIA_HOME="/usr/lib64/aria"
PROFILE

# --- ld.so.conf.d for bundled libs ---
if [[ -d "$STAGING/usr/lib64/aria/lib" ]]; then
    install -Dm644 /dev/stdin "$STAGING/etc/ld.so.conf.d/aria.conf" <<'LDCONF'
# Aria bundled shared libraries
/usr/lib64/aria/lib
LDCONF
fi

ok "Staged: ${BIN_COUNT} binaries, ${STDLIB_COUNT} stdlib, ${LIBC_SRC} libc src, ${LIBC_SHIM} libc shim"

# ── Step 4: Generate .spec File ─────────────────────────────────────
info "[4/6] Generating RPM spec file..."

# Build file list from BUILDROOT
FILE_LIST=""
cd "$STAGING"
for f in $(find . -type f -o -type l | sort | sed 's|^\./|/|'); do
    if [[ "$f" == /etc/* ]]; then
        FILE_LIST="${FILE_LIST}%config(noreplace) ${f}\n"
    else
        FILE_LIST="${FILE_LIST}${f}\n"
    fi
done
# Add directories
for d in $(find . -type d | sort | sed 's|^\./|/|' | grep -E "^/usr/lib64/aria"); do
    FILE_LIST="%dir ${d}\n${FILE_LIST}"
done

cat > "$RPM_TOPDIR/SPECS/aria.spec" <<SPECEOF
Name:           ${PKG_NAME}
Version:        ${VERSION}
Release:        ${RELEASE}%{?dist}
Summary:        Aria programming language — compiler, tools, and standard library

License:        MIT
URL:            ${HOMEPAGE:-https://aria-lang.org}
Packager:       ${MAINTAINER}

# No source tarball — we build from pre-built binaries
AutoReqProv:    no

Requires:       glibc >= 2.35
Requires:       libstdc++ >= 12
Requires:       zlib >= 1.2.11
Requires:       libzstd >= 1.5.0
Requires:       z3-libs >= 4.8
Requires:       libatomic >= 10

%description
Aria is a modern, safe, compiled programming language designed for
systems programming with AI-native features.

This package includes:
  - ariac: the Aria compiler
  - aria-make: build system
  - aria-ls: language server (LSP)
  - aria-pkg: package manager
  - aria-doc: documentation generator
  - Standard library (${STDLIB_COUNT} modules)
  - C library wrappers (aria-libc)
  - Static runtime library

%install
# Copy pre-staged files to buildroot
cp -a ${STAGING}/* %{buildroot}/

%post
/sbin/ldconfig 2>/dev/null || true

%postun
/sbin/ldconfig 2>/dev/null || true
rmdir /usr/lib64/aria/libc/shim 2>/dev/null || true
rmdir /usr/lib64/aria/libc/src 2>/dev/null || true
rmdir /usr/lib64/aria/libc 2>/dev/null || true
rmdir /usr/lib64/aria/stdlib 2>/dev/null || true
rmdir /usr/lib64/aria/lib 2>/dev/null || true
rmdir /usr/lib64/aria 2>/dev/null || true

%files
$(echo -e "$FILE_LIST")

%changelog
* $(date '+%a %b %d %Y') Randy Delphi <randy@ai-liberation-platform.org> - ${VERSION}-${RELEASE}
- Initial RPM package for Aria toolchain
- 5 binaries: ariac, aria-make, aria-ls, aria-pkg, aria-doc
- ${STDLIB_COUNT} stdlib modules, ${LIBC_SRC} libc sources, ${LIBC_SHIM} shim libraries
- Bundled liburing (statically linked LLVM)
SPECEOF

ok "Spec file generated"

# ── Step 5: Build .rpm ───────────────────────────────────────────────
info "[5/6] Building .rpm package..."
mkdir -p "$OUTPUT_DIR"

rpmbuild --define "_topdir $RPM_TOPDIR" \
         --target "$ARCH" \
         -bb "$RPM_TOPDIR/SPECS/aria.spec" 2>&1 | tail -10

# Find the built RPM
BUILT_RPM=$(find "$RPM_TOPDIR/RPMS" -name "*.rpm" -type f | head -1)
if [[ -z "$BUILT_RPM" ]]; then
    fail "RPM build failed — no .rpm found in $RPM_TOPDIR/RPMS/"
fi

cp "$BUILT_RPM" "$OUTPUT_DIR/"
RPM_BASENAME=$(basename "$BUILT_RPM")
ok "Package built: $OUTPUT_DIR/$RPM_BASENAME"

# ── Step 6: Verify Package ──────────────────────────────────────────
info "[6/6] Verifying package..."
echo ""

echo -e "${BOLD}Package Info:${NC}"
rpm -qip "$OUTPUT_DIR/$RPM_BASENAME" 2>/dev/null | grep -E "Name|Version|Release|Architecture|Size|License|Summary"
echo ""

echo -e "${BOLD}Package Contents (first 40):${NC}"
rpm -qlp "$OUTPUT_DIR/$RPM_BASENAME" 2>/dev/null | head -40
TOTAL_FILES=$(rpm -qlp "$OUTPUT_DIR/$RPM_BASENAME" 2>/dev/null | wc -l)
echo "  ... (${TOTAL_FILES} files total)"

# File size
RPM_SIZE=$(ls -lh "$OUTPUT_DIR/$RPM_BASENAME" | awk '{print $5}')
echo ""
echo -e "${BOLD}File:${NC} $OUTPUT_DIR/$RPM_BASENAME (${RPM_SIZE})"

# Optional rpmlint
if command -v rpmlint >/dev/null 2>&1; then
    echo ""
    echo -e "${BOLD}rpmlint Check:${NC}"
    rpmlint "$OUTPUT_DIR/$RPM_BASENAME" 2>/dev/null | tail -10 || true
fi

echo ""
echo -e "${BOLD}═══════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}  .rpm package ready!${NC}"
echo -e "  Install (Fedora): ${BOLD}sudo dnf install $OUTPUT_DIR/$RPM_BASENAME${NC}"
echo -e "  Install (RHEL):   ${BOLD}sudo rpm -ivh $OUTPUT_DIR/$RPM_BASENAME${NC}"
echo -e "  Remove:           ${BOLD}sudo rpm -e aria${NC}"
echo -e "${BOLD}═══════════════════════════════════════════════════════${NC}"
