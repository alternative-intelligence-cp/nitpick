#!/bin/bash
# Phase 3 Test Suite Runner

echo "════════════════════════════════════════════════════════"
echo "Phase 3: Loop & Advanced Control Flow Test Suite"
echo "════════════════════════════════════════════════════════"
echo ""

# Test counter
PASS=0
FAIL=0

run_test() {
    local test_file="$1"
    local test_name="$2"
    local expected="$3"
    local test_desc="$4"
    
    echo "TEST: $test_name"
    echo "Desc: $test_desc"
    echo "Expected: $expected"
    
    result=$(./build/npkc "$test_file" 2>&1)
    exit_code=$?
    
    if [ "$expected" = "SUCCESS" ]; then
        if [ $exit_code -eq 0 ]; then
            echo "✅ PASS: Compiled successfully"
            PASS=$((PASS + 1))
        else
            echo "❌ FAIL: Should have compiled but got errors:"
            echo "$result" | grep "error:" | head -3
            FAIL=$((FAIL + 1))
        fi
    else
        # Expected ERROR
        if echo "$result" | grep -q "error:"; then
            echo "✅ PASS: Correctly rejected"
            echo "$result" | grep "error:" | head -1
            PASS=$((PASS + 1))
        else
            echo "❌ FAIL: Should have errored but compiled"
            FAIL=$((FAIL + 1))
        fi
    fi
    echo ""
}

# Run Phase 3 tests
run_test "tests/test_phase3_basic_while.npk" \
         "While Loop - Basic" \
         "SUCCESS" \
         "Loop body knows condition state; can access .value inside while (!r.is_error)"

run_test "tests/test_phase3_while_exit_knowledge.npk" \
         "While Loop - Exit Knowledge" \
         "SUCCESS" \
         "After loop exits, we know inverted condition; can access .error after while (!r.is_error)"

run_test "tests/test_phase3_for_loop.npk" \
         "For Loop - Range-Based" \
         "SUCCESS" \
         "Range-based for loops are conservative; state preserved across loop"

run_test "tests/test_phase3_nested_if.npk" \
         "Nested If Statements" \
         "SUCCESS" \
         "State propagates through nested if branches; can access .value at all levels"

echo "════════════════════════════════════════════════════════"
echo "Test Suite Complete"
echo "PASSED: $PASS | FAILED: $FAIL"
echo "════════════════════════════════════════════════════════"

exit $FAIL
