#!/bin/bash
# Run all cast tests

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARIA_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
NPKC="$ARIA_ROOT/build/npkc"

echo "==================================================================="
echo "   ARIA CAST EXPRESSION TESTS"
echo "==================================================================="
echo ""

if [ ! -f "$NPKC" ]; then
    echo "❌ Error: npkc compiler not found at $NPKC"
    echo "Please build the compiler first: cmake --build build"
    exit 1
fi

PASSED=0
FAILED=0
TOTAL=0

for test_file in "$SCRIPT_DIR"/*.npk; do
    test_name=$(basename "$test_file" .npk)
    TOTAL=$((TOTAL + 1))
    
    echo -n "[$TOTAL] Testing $test_name... "
    
    # Compile the test
    if "$NPKC" "$test_file" -o "/tmp/cast_test_$test_name" 2>/dev/null; then
        # Run the test
        if "/tmp/cast_test_$test_name" >/dev/null 2>&1; then
            echo "✅ PASS"
            PASSED=$((PASSED + 1))
        else
            echo "❌ FAIL (runtime error)"
            FAILED=$((FAILED + 1))
        fi
        # Clean up
        rm -f "/tmp/cast_test_$test_name"
    else
        echo "❌ FAIL (compilation error)"
        FAILED=$((FAILED + 1))
    fi
done

echo ""
echo "==================================================================="
echo "   RESULTS"
echo "==================================================================="
echo "Total:  $TOTAL"
echo "Passed: $PASSED ✅"
echo "Failed: $FAILED ❌"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "🎉 All tests passed!"
    exit 0
else
    echo "⚠️  Some tests failed"
    exit 1
fi
