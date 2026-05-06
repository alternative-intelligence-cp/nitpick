#!/bin/bash
# Phase 4.5 Generics Test Runner

echo "======================================"
echo "  Phase 4.5: Generics & Templates"
echo "======================================"
echo

ARIA_COMPILER="./build/npkc"
TEST_DIR="tests/generics"
PASS=0
FAIL=0

run_test() {
    local test_file=$1
    local test_name=$(basename "$test_file" .npk)
    
    echo -n "Testing $test_name... "
    
    # Compile
    if ! $ARIA_COMPILER "$test_file" -o "/tmp/aria_test_$test_name" 2>/dev/null; then
        echo "❌ COMPILATION FAILED"
        FAIL=$((FAIL + 1))
        return 1
    fi
    
    # Run
    "/tmp/aria_test_$test_name"
    local exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        echo "✅ PASS"
        PASS=$((PASS + 1))
    else
        echo "❌ FAIL (exit code: $exit_code)"
        FAIL=$((FAIL + 1))
    fi
    
    # Cleanup
    rm -f "/tmp/aria_test_$test_name"
}

# Run all generic tests
run_test "$TEST_DIR/test_identity.npk"
run_test "$TEST_DIR/test_swap.npk"
run_test "$TEST_DIR/test_multi_param.npk"

echo
echo "======================================"
echo "  Results: $PASS passed, $FAIL failed"
echo "======================================"

if [ $FAIL -eq 0 ]; then
    echo "✅ All generics tests passed!"
    exit 0
else
    echo "❌ Some tests failed"
    exit 1
fi
