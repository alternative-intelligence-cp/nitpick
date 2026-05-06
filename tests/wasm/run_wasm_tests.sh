#!/bin/bash
# WASM Test Runner for Aria Compiler
# Compiles each .npk file in tests/wasm/ to WASM and runs with wasmtime

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
NPKC="$SCRIPT_DIR/../../build/npkc"
WASMTIME="${WASMTIME:-wasmtime}"
TEST_DIR="$SCRIPT_DIR"
TMPDIR="${TMPDIR:-/tmp}"
PASS=0
FAIL=0
SKIP=0

# Check prerequisites
if [ ! -x "$NPKC" ]; then
    echo "ERROR: npkc not found at $NPKC"
    echo "Build with: cd build && cmake .. && make -j\$(nproc)"
    exit 1
fi

if ! command -v "$WASMTIME" &>/dev/null; then
    if [ -x "$HOME/.wasmtime/bin/wasmtime" ]; then
        WASMTIME="$HOME/.wasmtime/bin/wasmtime"
    else
        echo "ERROR: wasmtime not found. Install: curl https://wasmtime.dev/install.sh -sSf | bash"
        exit 1
    fi
fi

echo "=== Aria WASM Test Suite ==="
echo "Compiler: $NPKC"
echo "Runtime:  $WASMTIME"
echo ""

for src in "$TEST_DIR"/*.npk; do
    [ -f "$src" ] || continue
    name="$(basename "$src" .npk)"
    wasm="$TMPDIR/aria_wasm_test_${name}.wasm"

    # Extract expected output from comments
    expected=$(grep -A1 "^// Expected output:" "$src" | tail -1 | sed 's/^\/\/ //')

    printf "%-30s " "$name..."

    # Compile
    compile_out=$("$NPKC" "$src" --emit-wasm -o "$wasm" 2>&1)
    if [ $? -ne 0 ]; then
        echo "FAIL (compile error)"
        echo "  $compile_out" | head -5
        FAIL=$((FAIL + 1))
        continue
    fi

    # Run
    run_out=$("$WASMTIME" "$wasm" 2>&1)
    run_rc=$?

    if [ $run_rc -ne 0 ]; then
        echo "FAIL (runtime error, exit $run_rc)"
        echo "  $run_out" | head -5
        FAIL=$((FAIL + 1))
    else
        echo "PASS"
        PASS=$((PASS + 1))
    fi

    # Cleanup
    rm -f "$wasm"
done

echo ""
echo "=== Results: $PASS passed, $FAIL failed, $SKIP skipped ==="

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
