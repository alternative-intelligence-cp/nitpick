#!/bin/bash
# Phase 5.5 Test Suite - Three-Valued Logic
# Tests all comparison operations with unknown values

cd "$(dirname "$0")/.."

echo "=========================================="
echo "Phase 5.5: Three-Valued Logic Test Suite"
echo "=========================================="
echo ""

COMPILER="./build/ariac"
PASSED=0
FAILED=0

run_test() {
    local test_file="$1"
    local expected_exit="$2"
    local description="$3"
    
    echo -n "Testing: $description ... "
    
    if ! $COMPILER "$test_file" 2>/dev/null; then
        echo "❌ COMPILATION FAILED"
        FAILED=$((FAILED+1))
        return 1
    fi
    
    ./a.out
    local actual_exit=$?
    
    if [ "$actual_exit" -eq "$expected_exit" ]; then
        echo "✅ PASS (exit $actual_exit)"
        PASSED=$((PASSED+1))
    else
        echo "❌ FAIL (expected $expected_exit, got $actual_exit)"
        FAILED=$((FAILED+1))
    fi
}

echo "=== Identity Checks ==="
run_test "tests/test_unknown_comparisons.aria" 3 "unknown == unknown → true"
run_test "tests/test_unknown_neq.aria" 2 "unknown != unknown → false"
run_test "tests/test_unknown_lt_unknown.aria" 2 "unknown < unknown → false"

echo ""
echo "=== Propagation (Mixed Comparisons) ==="
run_test "tests/test_unknown_eq_value2.aria" 3 "unknown == 5 → unknown"
run_test "tests/test_unknown_neq_value.aria" 3 "unknown != 7 → unknown"
run_test "tests/test_unknown_gt.aria" 3 "unknown > 0 → unknown"

echo ""
echo "=== Phase 5.4 Regression Tests ==="
run_test "tests/test_pick_simple.aria" 42 "Basic pick statement"
run_test "tests/test_pick_unknown_simple.aria" 42 "Pick with unknown pattern"

echo ""
echo "=========================================="
echo "Results: $PASSED passed, $FAILED failed"
echo "=========================================="

if [ "$FAILED" -eq 0 ]; then
    echo "✅ All tests passed!"
    exit 0
else
    echo "❌ Some tests failed"
    exit 1
fi
