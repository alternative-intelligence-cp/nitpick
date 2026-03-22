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
# Installs:
#   ariac         → /usr/local/bin/ariac
#   aria-ls       → /usr/local/bin/aria-ls
#   aria-pkg      → /usr/local/bin/aria-pkg
#   aria-doc      → /usr/local/bin/aria-doc
#   aria-safety   → /usr/local/bin/aria-safety
#   aria-mcp      → /usr/local/bin/aria-mcp  (wrapper script)
#   stdlib/       → /usr/local/lib/aria/stdlib/
#   man pages     → /usr/local/share/man/man7/  (if available)
#
# Prerequisites:
#   - Linux (Debian/Ubuntu/Mint)
#   - LLVM 20 development headers (llvm-20-dev)
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

usage() {
    cat <<EOF
Aria Language Toolchain Installer

Usage:
  ./install.sh [OPTIONS]

Options:
  --build-only     Build but don't install system-wide
  --uninstall      Remove installed Aria files
  --prefix=PATH    Install prefix (default: /usr/local)
  --jobs=N         Parallel build jobs (default: auto-detect)
  --help           Show this help

Environment variables:
  ARIA_PREFIX      Same as --prefix
  ARIA_JOBS        Same as --jobs

Examples:
  ./install.sh                       # build and install
  ./install.sh --prefix=\$HOME/.local  # install to home directory
  sudo ./install.sh                  # install to /usr/local (needs sudo)
  ./install.sh --build-only          # just build, no system install
EOF
    exit 0
}

# ── Argument Parsing ──────────────────────────────────────────────────────

BUILD_ONLY=false
UNINSTALL=false

for arg in "$@"; do
    case "$arg" in
        --build-only)   BUILD_ONLY=true ;;
        --uninstall)    UNINSTALL=true ;;
        --prefix=*)     PREFIX="${arg#--prefix=}"
                        BINDIR="${PREFIX}/bin"
                        LIBDIR="${PREFIX}/lib/aria"
                        MANDIR="${PREFIX}/share/man/man7" ;;
        --jobs=*)       JOBS="${arg#--jobs=}" ;;
        --help|-h)      usage ;;
        *)              fail "Unknown option: $arg (try --help)" ;;
    esac
done

# ── Resolve Script Directory ─────────────────────────────────────────────

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── Uninstall ─────────────────────────────────────────────────────────────

if [ "$UNINSTALL" = true ]; then
    info "Uninstalling Aria from ${PREFIX}..."

    for bin in ariac aria-ls aria-pkg aria-doc aria-safety aria-mcp; do
        if [ -f "${BINDIR}/${bin}" ]; then
            rm -f "${BINDIR}/${bin}"
            ok "Removed ${BINDIR}/${bin}"
        fi
    done

    if [ -d "${LIBDIR}" ]; then
        rm -rf "${LIBDIR}"
        ok "Removed ${LIBDIR}"
    fi

    # Remove man pages
    if ls "${MANDIR}"/aria-*.7.gz 1>/dev/null 2>&1; then
        rm -f "${MANDIR}"/aria-*.7.gz
        mandb -q 2>/dev/null || true
        ok "Removed Aria man pages"
    fi

    ok "Uninstall complete."
    exit 0
fi

# ── Banner ────────────────────────────────────────────────────────────────

printf "\n"
printf "${BOLD}  ╔══════════════════════════════════════╗${NC}\n"
printf "${BOLD}  ║   Aria Language Toolchain Installer   ║${NC}\n"
printf "${BOLD}  ╚══════════════════════════════════════╝${NC}\n"
printf "\n"

# ── Prerequisite Checks ──────────────────────────────────────────────────

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
    MISSING+=("C++ compiler (install: sudo apt install build-essential)")
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
    MISSING+=("CMake (install: sudo apt install cmake)")
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
    MISSING+=("LLVM 20 development headers (install: sudo apt install llvm-20-dev)")
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
    MISSING+=("Clang (install: sudo apt install clang-20, then: sudo ln -s /usr/bin/clang++-20 /usr/bin/clang++)")
fi

# Git
if command -v git &>/dev/null; then
    ok "Git: $(git --version | head -1)"
else
    MISSING+=("Git (install: sudo apt install git)")
fi

# Python 3
if command -v python3 &>/dev/null; then
    PY_VERSION=$(python3 --version 2>&1)
    ok "Python: ${PY_VERSION}"
else
    MISSING+=("Python 3 (install: sudo apt install python3)")
fi

if [ ${#MISSING[@]} -gt 0 ]; then
    printf "\n"
    fail "Missing prerequisites:
$(printf '  - %s\n' "${MISSING[@]}")

Install them and try again."
fi

printf "\n"

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
mkdir -p "${BINDIR}" "${LIBDIR}/stdlib"

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
printf "  aria-ls      — Language server (LSP)\n"
printf "  aria-pkg     — Package manager\n"
printf "  aria-doc     — Documentation generator\n"
printf "  aria-safety  — Static safety auditor\n"
printf "  aria-mcp     — MCP server for AI assistants\n"
printf "\n"

# Check PATH
if ! echo "$PATH" | tr ':' '\n' | grep -qx "${BINDIR}"; then
    warn "${BINDIR} is not in your PATH."
    info "Add to your shell profile:"
    printf "  export PATH=\"${BINDIR}:\$PATH\"\n"
    printf "\n"
fi
