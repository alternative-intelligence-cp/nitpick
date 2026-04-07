#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────────────
# install.sh — Aria Language Toolchain Installer
#
# Usage:
#   ./install.sh              # full install (build + install)
#   ./install.sh --build-only # build but don't install system-wide
#   ./install.sh --uninstall  # remove installed files
#   ./install.sh --help       # show usage
#
# One-liner (from GitHub):
#   curl -fsSL https://raw.githubusercontent.com/alternative-intelligence-cp/aria/main/install.sh | bash
#
# Installs:
#   ariac         → PREFIX/bin/ariac
#   aria-make     → PREFIX/bin/aria-make
#   aria-ls       → PREFIX/bin/aria-ls
#   aria-pkg      → PREFIX/bin/aria-pkg
#   aria-doc      → PREFIX/bin/aria-doc
#   aria-safety   → PREFIX/bin/aria-safety
#   aria-mcp      → PREFIX/bin/aria-mcp  (wrapper script)
#   stdlib/       → PREFIX/lib/aria/stdlib/
#   libc/         → PREFIX/lib/aria/libc/  (shim libs + .aria sources)
#   man pages     → PREFIX/share/man/man7/  (if available)
#   ~/.aria/env   → shell profile snippet
#
# Supported distros:
#   Debian/Ubuntu/Mint (apt), Fedora/RHEL (dnf), Arch (pacman),
#   Alpine (apk), openSUSE (zypper), generic Linux (manual deps)
#
# Prerequisites:
#   - Linux (x86_64)
#   - LLVM 20 development headers
#   - CMake >= 3.20
#   - C++17 compiler (g++ or clang++)
#   - Git, Python 3.8+
#   - zlib, zstd, libcurl (dev packages for LLVM linking)
# ──────────────────────────────────────────────────────────────────────────────

set -euo pipefail

# ── Configuration ─────────────────────────────────────────────────────────

PREFIX="${ARIA_PREFIX:-/usr/local}"
BINDIR="${PREFIX}/bin"
LIBDIR="${PREFIX}/lib/aria"
MANDIR="${PREFIX}/share/man/man7"
JOBS="${ARIA_JOBS:-$(nproc 2>/dev/null || echo 4)}"

# Colors (disabled if not a terminal)
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    BOLD='\033[1m'
    NC='\033[0m'
else
    RED='' GREEN='' YELLOW='' BLUE='' BOLD='' NC=''
fi

# ── Helpers ───────────────────────────────────────────────────────────────

info()  { printf "${BLUE}[aria]${NC} %s\n" "$*"; }
ok()    { printf "${GREEN}[aria]${NC} %s\n" "$*"; }
warn()  { printf "${YELLOW}[aria]${NC} %s\n" "$*"; }
fail()  { printf "${RED}[aria]${NC} %s\n" "$*" >&2; exit 1; }

# ── Distro Detection ─────────────────────────────────────────────────────

detect_distro() {
    DISTRO_FAMILY="unknown"
    DISTRO_NAME="unknown"
    PKG_MGR="none"

    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO_NAME="${ID:-unknown}"
        case "${ID:-}" in
            ubuntu|debian|linuxmint|pop|elementary|zorin|neon|kali)
                DISTRO_FAMILY="debian"
                PKG_MGR="apt"
                ;;
            fedora|rhel|centos|rocky|alma|nobara)
                DISTRO_FAMILY="rhel"
                PKG_MGR="dnf"
                # Fall back to yum if dnf not available
                command -v dnf &>/dev/null || PKG_MGR="yum"
                ;;
            arch|manjaro|endeavouros|garuda)
                DISTRO_FAMILY="arch"
                PKG_MGR="pacman"
                ;;
            opensuse*|sles)
                DISTRO_FAMILY="suse"
                PKG_MGR="zypper"
                ;;
            alpine)
                DISTRO_FAMILY="alpine"
                PKG_MGR="apk"
                ;;
            *)
                # Try ID_LIKE for derivatives
                case "${ID_LIKE:-}" in
                    *debian*|*ubuntu*) DISTRO_FAMILY="debian"; PKG_MGR="apt" ;;
                    *rhel*|*fedora*)   DISTRO_FAMILY="rhel";   PKG_MGR="dnf" ;;
                    *arch*)            DISTRO_FAMILY="arch";   PKG_MGR="pacman" ;;
                    *suse*)            DISTRO_FAMILY="suse";   PKG_MGR="zypper" ;;
                esac
                ;;
        esac
    fi
}

install_deps() {
    info "Installing build dependencies for ${DISTRO_NAME} (${DISTRO_FAMILY})..."

    case "$DISTRO_FAMILY" in
        debian)
            sudo apt-get update -qq
            sudo apt-get install -y \
                build-essential cmake git python3 \
                llvm-20-dev clang-20 lld-20 \
                zlib1g-dev libzstd-dev libcurl4-openssl-dev \
                pkg-config
            # Create clang symlinks if needed
            if ! command -v clang++ &>/dev/null && command -v clang++-20 &>/dev/null; then
                sudo ln -sf /usr/bin/clang++-20 /usr/bin/clang++
                sudo ln -sf /usr/bin/clang-20 /usr/bin/clang
            fi
            ;;
        rhel)
            sudo "$PKG_MGR" install -y \
                gcc-c++ cmake git python3 \
                llvm20-devel clang20 lld20 \
                zlib-devel libzstd-devel libcurl-devel \
                pkgconf-pkg-config
            ;;
        arch)
            sudo pacman -Syu --noconfirm \
                base-devel cmake git python \
                llvm clang lld \
                zlib zstd curl
            ;;
        suse)
            sudo zypper install -y \
                gcc-c++ cmake git python3 \
                llvm20-devel clang20 lld20 \
                zlib-devel libzstd-devel libcurl-devel
            ;;
        alpine)
            sudo apk add \
                build-base cmake git python3 \
                llvm20-dev clang20 lld \
                zlib-dev zstd-dev curl-dev
            ;;
        *)
            warn "Unknown distro family — cannot auto-install dependencies."
            warn "Please install manually: C++17 compiler, CMake >= 3.20, LLVM 20 dev, Git, Python 3"
            return 1
            ;;
    esac

    ok "Build dependencies installed"
}

dep_install_hint() {
    case "$DISTRO_FAMILY" in
        debian) echo "sudo apt install" ;;
        rhel)   echo "sudo $PKG_MGR install" ;;
        arch)   echo "sudo pacman -S" ;;
        suse)   echo "sudo zypper install" ;;
        alpine) echo "sudo apk add" ;;
        *)      echo "install" ;;
    esac
}

usage() {
    cat <<EOF
Aria Language Toolchain Installer

Usage:
  ./install.sh [OPTIONS]

Options:
  --install-deps   Auto-install build dependencies (requires sudo)
  --no-deps        Skip dependency checking
  --build-only     Build but don't install system-wide
  --uninstall      Remove installed Aria files
  --prefix=PATH    Install prefix (default: /usr/local)
  --jobs=N         Parallel build jobs (default: auto-detect)
  --help           Show this help

Environment variables:
  ARIA_PREFIX      Same as --prefix
  ARIA_JOBS        Same as --jobs

Examples:
  ./install.sh                         # build and install
  ./install.sh --install-deps          # install deps, build, install
  ./install.sh --prefix=\$HOME/.local   # install to home directory
  sudo ./install.sh                    # install to /usr/local (needs sudo)
  ./install.sh --build-only            # just build, no system install
  curl -fsSL https://raw.githubusercontent.com/alternative-intelligence-cp/aria/main/install.sh | bash -s -- --install-deps
EOF
    exit 0
}

# ── Argument Parsing ──────────────────────────────────────────────────────

BUILD_ONLY=false
UNINSTALL=false
INSTALL_DEPS=false
SKIP_DEPS=false

for arg in "$@"; do
    case "$arg" in
        --build-only)     BUILD_ONLY=true ;;
        --uninstall)      UNINSTALL=true ;;
        --install-deps)   INSTALL_DEPS=true ;;
        --no-deps)        SKIP_DEPS=true ;;
        --prefix=*)       PREFIX="${arg#--prefix=}"
                          BINDIR="${PREFIX}/bin"
                          LIBDIR="${PREFIX}/lib/aria"
                          MANDIR="${PREFIX}/share/man/man7" ;;
        --jobs=*)         JOBS="${arg#--jobs=}" ;;
        --help|-h)        usage ;;
        *)                fail "Unknown option: $arg (try --help)" ;;
    esac
done

# ── Resolve Script Directory ─────────────────────────────────────────────

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── Uninstall ─────────────────────────────────────────────────────────────

if [ "$UNINSTALL" = true ]; then
    info "Uninstalling Aria from ${PREFIX}..."

    for bin in ariac aria-make aria-ls aria-pkg aria-doc aria-safety aria-mcp; do
        if [ -f "${BINDIR}/${bin}" ]; then
            rm -f "${BINDIR}/${bin}"
            ok "Removed ${BINDIR}/${bin}"
        fi
    done

    if [ -d "${LIBDIR}" ]; then
        rm -rf "${LIBDIR}"
        ok "Removed ${LIBDIR}"
    fi

    # Remove runtime library
    if [ -f "${PREFIX}/lib/libaria_runtime.a" ]; then
        rm -f "${PREFIX}/lib/libaria_runtime.a"
        ok "Removed ${PREFIX}/lib/libaria_runtime.a"
    fi

    # Remove man pages
    if ls "${MANDIR}"/aria-*.7.gz 1>/dev/null 2>&1; then
        rm -f "${MANDIR}"/aria-*.7.gz
        mandb -q 2>/dev/null || true
        ok "Removed Aria man pages"
    fi

    # Remove profile snippet
    if [ -f "${HOME}/.aria/env" ]; then
        rm -f "${HOME}/.aria/env"
        ok "Removed ~/.aria/env"
    fi

    ok "Uninstall complete."
    info "You may want to remove 'source ~/.aria/env' from your shell profile."
    exit 0
fi

# ── Banner ────────────────────────────────────────────────────────────────

printf "\n"
printf "${BOLD}  ╔══════════════════════════════════════╗${NC}\n"
printf "${BOLD}  ║   Aria Language Toolchain Installer   ║${NC}\n"
printf "${BOLD}  ╚══════════════════════════════════════╝${NC}\n"
printf "\n"

# ── Distro Detection ─────────────────────────────────────────────────────

detect_distro
info "Detected: ${DISTRO_NAME} (${DISTRO_FAMILY}, pkg: ${PKG_MGR})"

# ── Auto-install Dependencies ─────────────────────────────────────────────

if [ "$INSTALL_DEPS" = true ]; then
    install_deps
fi

# ── Prerequisite Checks ──────────────────────────────────────────────────

if [ "$SKIP_DEPS" = false ]; then
    info "Checking prerequisites..."

MISSING=()

# C++ compiler
if command -v g++ &>/dev/null; then
    CXX_VERSION=$(g++ --version | head -1)
    ok "C++ compiler: ${CXX_VERSION}"
elif command -v clang++ &>/dev/null; then
    CXX_VERSION=$(clang++ --version | head -1)
    ok "C++ compiler: ${CXX_VERSION}"
else
    MISSING+=("C++ compiler ($(dep_install_hint) build-essential)")
fi

# CMake
if command -v cmake &>/dev/null; then
    CMAKE_VERSION=$(cmake --version | head -1 | grep -oP '\d+\.\d+\.\d+')
    CMAKE_MAJOR=$(echo "$CMAKE_VERSION" | cut -d. -f1)
    CMAKE_MINOR=$(echo "$CMAKE_VERSION" | cut -d. -f2)
    if [ "$CMAKE_MAJOR" -lt 3 ] || { [ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -lt 20 ]; }; then
        MISSING+=("CMake >= 3.20 (found: ${CMAKE_VERSION})")
    else
        ok "CMake: ${CMAKE_VERSION}"
    fi
else
    MISSING+=("CMake ($(dep_install_hint) cmake)")
fi

# LLVM 20
LLVM_CFG=""
for candidate in llvm-config-20 llvm-config; do
    if command -v "$candidate" &>/dev/null; then
        VER=$("$candidate" --version 2>/dev/null || true)
        if [[ "$VER" == 20.* ]]; then
            LLVM_CFG="$candidate"
            break
        fi
    fi
done

if [ -n "$LLVM_CFG" ]; then
    ok "LLVM: $($LLVM_CFG --version)"
else
    MISSING+=("LLVM 20 development headers ($(dep_install_hint) llvm-20-dev)")
fi

# Clang (used by ariac as linker backend)
if command -v clang++ &>/dev/null; then
    ok "Clang: $(clang++ --version | head -1)"
elif command -v clang++-20 &>/dev/null; then
    ok "Clang: $(clang++-20 --version | head -1)"
    warn "clang++ not found but clang++-20 exists — creating symlink"
    if [ -w /usr/bin ]; then
        ln -sf /usr/bin/clang++-20 /usr/bin/clang++
        ln -sf /usr/bin/clang-20 /usr/bin/clang 2>/dev/null || true
    else
        warn "Cannot create symlink (not root). Run: sudo ln -s /usr/bin/clang++-20 /usr/bin/clang++"
    fi
else
    MISSING+=("Clang ($(dep_install_hint) clang-20, then: sudo ln -s /usr/bin/clang++-20 /usr/bin/clang++)")
fi

# Git
if command -v git &>/dev/null; then
    ok "Git: $(git --version | head -1)"
else
    MISSING+=("Git ($(dep_install_hint) git)")
fi

# Python 3
if command -v python3 &>/dev/null; then
    PY_VERSION=$(python3 --version 2>&1)
    ok "Python: ${PY_VERSION}"
else
    MISSING+=("Python 3 ($(dep_install_hint) python3)")
fi

if [ ${#MISSING[@]} -gt 0 ]; then
    printf "\n"
    fail "Missing prerequisites:
$(printf '  - %s\n' "${MISSING[@]}")

Install them and try again, or re-run with --install-deps."
fi

printf "\n"

fi  # end SKIP_DEPS

# ── Build ─────────────────────────────────────────────────────────────────

info "Building Aria toolchain (${JOBS} jobs)..."

# Configure
if [ ! -d build ] || [ ! -f build/CMakeCache.txt ]; then
    info "Configuring CMake..."
    if ! cmake -B build -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -20; then
        fail "CMake configuration failed. Run 'cmake -B build' manually for details."
    fi
    # Verify configuration succeeded
    if [ ! -f build/CMakeCache.txt ]; then
        fail "CMake configuration failed — no CMakeCache.txt generated."
    fi
fi

# Build compiler
info "Building ariac..."
if ! cmake --build build --target ariac -j"${JOBS}" 2>&1 | tail -5; then
    fail "Failed to build ariac. Check compiler output above."
fi
ok "ariac built"

# Build runtime library (required for compiled programs to link)
info "Building aria_runtime..."
if ! cmake --build build --target aria_runtime -j"${JOBS}" 2>&1 | tail -3; then
    warn "aria_runtime failed to build — compiled programs may not link correctly"
else
    ok "aria_runtime built"
fi

# Build tools
for tool in aria-ls aria-pkg aria-doc; do
    info "Building ${tool}..."
    if ! cmake --build build --target "${tool}" -j"${JOBS}" 2>&1 | tail -3; then
        warn "${tool} failed to build — skipping"
    else
        ok "${tool} built"
    fi
done

# Build aria-safety (standalone C program)
if [ -f tools/aria-safety/aria_safety.c ]; then
    info "Building aria-safety..."
    if [ -f tools/aria-safety/Makefile ]; then
        make -C tools/aria-safety -s 2>&1 || true
    else
        cc -O2 -o tools/aria-safety/aria-safety tools/aria-safety/aria_safety.c 2>&1
    fi
    ok "aria-safety built"
fi

# Build aria-make (separate repo — sibling directory)
ARIA_MAKE_DIR="${SCRIPT_DIR}/../aria-make"
ARIA_MAKE_BUILT=false
if [ -d "$ARIA_MAKE_DIR" ] && [ -f "$ARIA_MAKE_DIR/CMakeLists.txt" ]; then
    info "Building aria-make..."
    if [ ! -d "$ARIA_MAKE_DIR/build" ]; then
        cmake -B "$ARIA_MAKE_DIR/build" -S "$ARIA_MAKE_DIR" -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -5
    fi
    if cmake --build "$ARIA_MAKE_DIR/build" -j"${JOBS}" 2>&1 | tail -3; then
        ARIA_MAKE_BUILT=true
        ok "aria-make built"
    else
        warn "aria-make failed to build — skipping"
    fi
else
    info "aria-make repo not found at ${ARIA_MAKE_DIR} — skipping"
    info "(Clone https://github.com/alternative-intelligence-cp/aria-make next to aria/)"
fi

printf "\n"

# Verify builds
BUILT=()
FAILED=()
for bin in build/ariac build/aria-ls build/aria-pkg build/aria-doc; do
    if [ -x "$bin" ]; then
        BUILT+=("$(basename "$bin")")
    else
        FAILED+=("$(basename "$bin")")
    fi
done
if [ -x tools/aria-safety/aria-safety ]; then
    BUILT+=("aria-safety")
fi
if [ "$ARIA_MAKE_BUILT" = true ]; then
    BUILT+=("aria-make")
fi

ok "Built successfully: ${BUILT[*]}"
if [ ${#FAILED[@]} -gt 0 ]; then
    warn "Failed to build: ${FAILED[*]}"
fi

if [ "$BUILD_ONLY" = true ]; then
    printf "\n"
    ok "Build complete (--build-only). Binaries are in build/"
    info "Run without --build-only to install system-wide."
    exit 0
fi

# ── Install ───────────────────────────────────────────────────────────────

printf "\n"
info "Installing to ${PREFIX}..."

# Create directories
mkdir -p "${BINDIR}" "${LIBDIR}/stdlib" "${LIBDIR}/libc"

# Install binaries
for bin in ariac aria-ls aria-pkg aria-doc; do
    if [ -x "build/${bin}" ]; then
        install -m 755 "build/${bin}" "${BINDIR}/${bin}"
        ok "Installed ${BINDIR}/${bin}"
    fi
done

# Install aria-safety
if [ -x tools/aria-safety/aria-safety ]; then
    install -m 755 tools/aria-safety/aria-safety "${BINDIR}/aria-safety"
    ok "Installed ${BINDIR}/aria-safety"
fi

# Install aria-make
if [ "$ARIA_MAKE_BUILT" = true ] && [ -x "$ARIA_MAKE_DIR/build/aria_make" ]; then
    install -m 755 "$ARIA_MAKE_DIR/build/aria_make" "${BINDIR}/aria-make"
    ok "Installed ${BINDIR}/aria-make"
fi

# Install runtime library (needed by ariac for linking compiled programs)
if [ -f build/libaria_runtime.a ]; then
    install -m 644 build/libaria_runtime.a "${PREFIX}/lib/libaria_runtime.a"
    ok "Installed ${PREFIX}/lib/libaria_runtime.a"
fi

# Install aria-mcp wrapper
cat > "${BINDIR}/aria-mcp" << 'MCPEOF'
#!/usr/bin/env bash
# aria-mcp — MCP server wrapper for the Aria language toolchain
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARIA_MCP_PY="${ARIA_MCP_DIR:-$(dirname "$SCRIPT_DIR")/lib/aria}/aria_mcp.py"
if [ ! -f "$ARIA_MCP_PY" ]; then
    echo "[aria-mcp] error: aria_mcp.py not found at $ARIA_MCP_PY" >&2
    echo "[aria-mcp] Set ARIA_MCP_DIR to the directory containing aria_mcp.py" >&2
    exit 1
fi
exec python3 "$ARIA_MCP_PY" "$@"
MCPEOF
chmod 755 "${BINDIR}/aria-mcp"

# Install MCP server
if [ -f tools/aria-mcp/aria_mcp.py ]; then
    install -m 644 tools/aria-mcp/aria_mcp.py "${LIBDIR}/aria_mcp.py"
    ok "Installed ${BINDIR}/aria-mcp"
fi

# Install stdlib
info "Installing standard library..."
STDLIB_COUNT=0
for f in stdlib/*.aria; do
    if [ -f "$f" ]; then
        install -m 644 "$f" "${LIBDIR}/stdlib/"
        STDLIB_COUNT=$((STDLIB_COUNT + 1))
    fi
done
ok "Installed ${STDLIB_COUNT} stdlib modules to ${LIBDIR}/stdlib/"

# Install aria-libc (sibling directory)
ARIA_LIBC_DIR="${SCRIPT_DIR}/../aria-libc"
if [ -d "$ARIA_LIBC_DIR" ]; then
    info "Installing aria-libc..."
    LIBC_COUNT=0

    # Install .aria source files
    if [ -d "$ARIA_LIBC_DIR/src" ]; then
        for f in "$ARIA_LIBC_DIR"/src/*.aria; do
            if [ -f "$f" ]; then
                install -m 644 "$f" "${LIBDIR}/libc/"
                LIBC_COUNT=$((LIBC_COUNT + 1))
            fi
        done
    fi

    # Install pre-built shim libraries
    SHIM_COUNT=0
    if [ -d "$ARIA_LIBC_DIR/shim" ]; then
        for f in "$ARIA_LIBC_DIR"/shim/*.a; do
            if [ -f "$f" ]; then
                install -m 644 "$f" "${LIBDIR}/libc/"
                SHIM_COUNT=$((SHIM_COUNT + 1))
            fi
        done
    fi

    ok "Installed aria-libc: ${LIBC_COUNT} sources, ${SHIM_COUNT} libraries to ${LIBDIR}/libc/"
else
    info "aria-libc repo not found at ${ARIA_LIBC_DIR} — skipping"
    info "(Clone https://github.com/alternative-intelligence-cp/aria-libc next to aria/)"
fi

# Install man pages (if built)
if ls aria_ecosystem/man_pages/build/man7/*.7.gz 1>/dev/null 2>&1; then
    info "Installing man pages..."
    mkdir -p "${MANDIR}"
    install -m 644 aria_ecosystem/man_pages/build/man7/*.7.gz "${MANDIR}/"
    MAN_COUNT=$(ls aria_ecosystem/man_pages/build/man7/*.7.gz | wc -l)
    mandb -q 2>/dev/null || true
    ok "Installed ${MAN_COUNT} man pages"
else
    info "Man pages not built — skip (run: cd aria_ecosystem/man_pages && make)"
fi

# ── Environment Setup ─────────────────────────────────────────────────────

# Create ~/.aria/env profile snippet
mkdir -p "${HOME}/.aria"
cat > "${HOME}/.aria/env" << ENVEOF
# Aria Language Toolchain environment
# Source this from your shell profile: source ~/.aria/env
export ARIA_HOME="${LIBDIR}"
export PATH="${BINDIR}:\$PATH"
ENVEOF
ok "Created ~/.aria/env"

# ── Verification ──────────────────────────────────────────────────────────

info "Verifying installation..."
VERIFY_OK=true

if [ -x "${BINDIR}/ariac" ]; then
    ARIAC_VER=$("${BINDIR}/ariac" --version 2>&1 | head -1 || true)
    ok "ariac: ${ARIAC_VER}"
else
    warn "ariac not found at ${BINDIR}/ariac"
    VERIFY_OK=false
fi

if [ -x "${BINDIR}/aria-make" ]; then
    ok "aria-make: installed"
else
    info "aria-make: not installed (optional)"
fi

INSTALLED_STDLIB=$(find "${LIBDIR}/stdlib" -name '*.aria' 2>/dev/null | wc -l)
if [ "$INSTALLED_STDLIB" -gt 0 ]; then
    ok "stdlib: ${INSTALLED_STDLIB} modules"
else
    warn "stdlib: no modules found"
    VERIFY_OK=false
fi

if [ "$VERIFY_OK" = true ]; then
    ok "Verification passed"
else
    warn "Some components may not be installed correctly"
fi

# ── Summary ───────────────────────────────────────────────────────────────

printf "\n"
printf "${BOLD}  ╔══════════════════════════════════════╗${NC}\n"
printf "${BOLD}  ║       Installation Complete!          ║${NC}\n"
printf "${BOLD}  ╚══════════════════════════════════════╝${NC}\n"
printf "\n"
ok "Installed to: ${PREFIX}"
printf "\n"
info "Quick start:"
printf "  ${BOLD}ariac hello.aria -o hello && ./hello${NC}\n"
printf "\n"
info "Tools:"
printf "  ariac        — Aria compiler\n"
printf "  aria-make    — Build system\n"
printf "  aria-ls      — Language server (LSP)\n"
printf "  aria-pkg     — Package manager\n"
printf "  aria-doc     — Documentation generator\n"
printf "  aria-safety  — Static safety auditor\n"
printf "  aria-mcp     — MCP server for AI assistants\n"
printf "\n"

# Check PATH and suggest profile setup
if ! echo "$PATH" | tr ':' '\n' | grep -qx "${BINDIR}"; then
    warn "${BINDIR} is not in your PATH."
    info "Add to your shell profile (e.g. ~/.bashrc or ~/.zshrc):"
    printf "  ${BOLD}source ~/.aria/env${NC}\n"
    printf "\n"
else
    info "To set ARIA_HOME in future sessions, add to your shell profile:"
    printf "  ${BOLD}source ~/.aria/env${NC}\n"
    printf "\n"
fi
