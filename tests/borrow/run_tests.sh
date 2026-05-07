#!/bin/bash
# Borrow Checker Test Runner
# Runs all borrow checker tests and reports results

set -e

ARIA_COMPILER="./build/npkc"
TEST_DIR="tests/borrow"
PASSED=0
FAILED=0

echo "╔════════════════════════════════════════════════════════════╗"
echo "║        ARIA BORROW CHECKER TEST SUITE                     ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

for test_file in "$TEST_DIR"/*.npk; do
    test_name=$(basename "$test_file" .npk)
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Running: $test_name"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    
    if $ARIA_COMPILER "$test_file" > /dev/null 2>&1; then
        echo ""
        if ./a.out 2>&1; then
            echo "✓ PASSED: $test_name"
            PASSED=$((PASSED + 1))
        else
            echo "✗ FAILED: $test_name (runtime error)"
            FAILED=$((FAILED + 1))
        fi
    else
        echo "✗ FAILED: $test_name (compilation error)"
        FAILED=$((FAILED + 1))
    fi
    echo ""
done

echo "╔════════════════════════════════════════════════════════════╗"
echo "║                    TEST SUMMARY                            ║"
echo "╠════════════════════════════════════════════════════════════╣"
printf "║  Passed: %-3d                                              ║\n" $PASSED
printf "║  Failed: %-3d                                              ║\n" $FAILED
printf "║  Total:  %-3d                                              ║\n" $((PASSED + FAILED))
echo "╚════════════════════════════════════════════════════════════╝"

if [ $FAILED -eq 0 ]; then
    echo "🎉 All tests passed!"
    exit 0
else
    echo "⚠️  Some tests failed"
    exit 1
fi
