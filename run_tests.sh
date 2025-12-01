#!/bin/bash
# run_tests.sh
# Aria Language Unit Test Runner
# This script builds and runs all unit tests for the Aria project

set -e  # Exit on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="$SCRIPT_DIR/build"

echo "=========================================="
echo "  Aria Language Unit Test Runner"
echo "=========================================="
echo ""

# Check if vendor/mimalloc exists
if [ ! -d "$SCRIPT_DIR/vendor/mimalloc" ]; then
    echo "[ERROR] mimalloc not found in vendor/ directory"
    echo "Please clone mimalloc into vendor/mimalloc:"
    echo "  git clone https://github.com/microsoft/mimalloc.git vendor/mimalloc"
    exit 1
fi

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "[*] Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Check if LLVM is available (optional for tests, but good to know)
if command -v llvm-config-18 &> /dev/null; then
    echo "[*] LLVM 18 found: $(llvm-config-18 --version)"
elif command -v llvm-config &> /dev/null; then
    echo "[*] LLVM found: $(llvm-config --version)"
    echo "[WARNING] Tests may require LLVM 18 for full compiler testing"
else
    echo "[WARNING] LLVM not found in PATH"
    echo "[INFO] Basic runtime tests can still run"
fi

echo ""
echo "[*] Configuring CMake..."
# Try with LLVM 18 first, fall back to any LLVM, then build without if needed
if cmake .. -DCMAKE_BUILD_TYPE=Debug 2>/dev/null; then
    echo "[*] CMake configuration successful"
elif cmake .. -DCMAKE_BUILD_TYPE=Debug -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm 2>/dev/null; then
    echo "[*] CMake configuration successful (with explicit LLVM path)"
else
    echo "[WARNING] CMake configuration had warnings, but continuing..."
fi

echo ""
echo "[*] Building tests..."
cmake --build . --target test_allocator test_gc_header -j$(nproc)

echo ""
echo "=========================================="
echo "  Running Unit Tests"
echo "=========================================="
echo ""

# Run tests with CTest for nice output
if ctest --output-on-failure --verbose; then
    echo ""
    echo "=========================================="
    echo "  All Tests PASSED! ✓"
    echo "=========================================="
    exit 0
else
    echo ""
    echo "=========================================="
    echo "  Some Tests FAILED! ✗"
    echo "=========================================="
    exit 1
fi
