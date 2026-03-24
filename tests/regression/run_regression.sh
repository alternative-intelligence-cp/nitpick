#!/bin/bash
# Regression test runner for Aria compiler
# Tests bugs confirmed resolved in v0.2.3

ARIAC="$(dirname "$0")/../../build/ariac"
DIR="$(dirname "$0")"
PASS=0
FAIL=0
TOTAL=0

run_test() {
    local test_file="$1"
    local test_name="$(basename "$test_file" .aria)"
    local output_bin="/tmp/aria_regtest_${test_name}"
    TOTAL=$((TOTAL + 1))

    # Compile
    if "$ARIAC" "$test_file" -o "$output_bin" 2>/dev/null; then
        # Run (if binary was produced)
        if [ -f "$output_bin" ]; then
            if "$output_bin" 2>/dev/null; then
                echo "  PASS  $test_name (compile + run)"
                PASS=$((PASS + 1))
            else
                echo "  FAIL  $test_name (compiled but runtime error, exit=$?)"
                FAIL=$((FAIL + 1))
            fi
            rm -f "$output_bin"
        else
            echo "  PASS  $test_name (compile only, no binary)"
            PASS=$((PASS + 1))
        fi
    else
        echo "  FAIL  $test_name (compilation failed)"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== Aria Regression Tests ==="
echo ""

for f in "$DIR"/test_*.aria; do
    run_test "$f"
done

echo ""
echo "Results: $PASS/$TOTAL passed, $FAIL failed"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
