#!/bin/bash
# Result<T> Enforcement Test Suite
# Tests Phase 1 ("no checky no val") and Phase 1.5 (immutable .is_error)

echo "════════════════════════════════════════════════════════"
echo "Result<T> Enforcement Test Suite"
echo "════════════════════════════════════════════════════════"
echo ""

# Test 1: Phase 1 - Should ERROR (accessing .value without check)
echo "TEST 1: Phase 1 - Accessing .value without .is_error check"
echo "Expected: ERROR"
./build/ariac tests/test_enforcement_bad1.aria 2>&1 | grep "Cannot access .value" && echo "✅ PASS: Correctly rejected" || echo "❌ FAIL: Should have been rejected"
echo ""

# Test 2: Phase 1.5 - Should ERROR (modifying .is_error without wild)
echo "TEST 2: Phase 1.5 - Modifying .is_error without wild modifier"
echo "Expected: ERROR"
./build/ariac tests/test_enforcement_bad2.aria 2>&1 | grep "Cannot modify .is_error" && echo "✅ PASS: Correctly rejected" || echo "❌ FAIL: Should have been rejected"
echo ""

# Test 3: Phase 1 - Should PASS (proper .is_error check)
echo "TEST 3: Phase 1 - Accessing .value WITH .is_error check"
echo "Expected: SUCCESS"
./build/ariac tests/test_enforcement_good1.aria > /dev/null 2>&1 && echo "✅ PASS: Compiled successfully" || echo "❌ FAIL: Should have compiled"
echo ""

# Test 4: Phase 1.5 - Should PASS (wild modifier allows modification)
echo "TEST 4: Phase 1.5 - Modifying .is_error WITH wild modifier"
echo "Expected: SUCCESS (memory leak warning is OK)"
./build/ariac tests/test_enforcement_good2.aria 2>&1 | grep "Cannot modify .is_error" && echo "❌ FAIL: Should have allowed modification" || echo "✅ PASS: Allowed modification with wild"
echo ""

echo "════════════════════════════════════════════════════════"
echo "Test Suite Complete"
echo "════════════════════════════════════════════════════════"
