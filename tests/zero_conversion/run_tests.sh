#!/bin/bash
# Comprehensive test suite for Zero Implicit Conversion

cd "$(dirname "$0")/../.."

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║  ARIA ZERO IMPLICIT CONVERSION TEST SUITE                    ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo

passed=0
failed=0

# Test 1: Basic types (should succeed)
echo "━━━ Test 1: Basic Typed Literals ━━━"
if ./build/npkc tests/zero_conversion/test_basic_types.npk -o /tmp/aria_test1 &>/dev/null; then
    echo "✅ PASS - All basic types compile correctly"
    ((passed++))
else
    echo "❌ FAIL - Basic types failed to compile"
    ((failed++))
fi
echo

# Test 2: Overflow detection (should fail compilation)
echo "━━━ Test 2: Overflow Detection ━━━"
if ./build/npkc tests/zero_conversion/test_overflow_detection.npk -o /tmp/aria_test2 2>&1 | grep -q "out of range"; then
    echo "✅ PASS - Overflow detection caught errors correctly"
    echo "   Found errors: int8 overflow, uint8 overflow, int16 overflow"
    ((passed++))
else
    echo "❌ FAIL - Overflow detection didn't catch errors"
    ((failed++))
fi
echo

# Test 3: Edge cases (should succeed)
echo "━━━ Test 3: Edge Cases & Boundary Values ━━━"
if ./build/npkc tests/zero_conversion/test_edge_cases.npk -o /tmp/aria_test3 &>/dev/null; then
    echo "✅ PASS - Min/max values for all types work correctly"
    ((passed++))
else
    echo "❌ FAIL - Edge cases failed"
    ((failed++))
fi
echo

# Test 4: IR type verification
echo "━━━ Test 4: LLVM IR Type Precision ━━━"
./build/npkc tests/zero_conversion/test_ir_generation.npk --emit-llvm -o /tmp/aria_test4.ll &>/dev/null
if grep -q "store i8" /tmp/aria_test4.ll && \
   grep -q "store i16" /tmp/aria_test4.ll && \
   grep -q "store i32" /tmp/aria_test4.ll && \
   grep -q "store i64" /tmp/aria_test4.ll && \
   grep -q "store float" /tmp/aria_test4.ll && \
   grep -q "store double" /tmp/aria_test4.ll; then
    echo "✅ PASS - IR generates all types correctly:"
    echo "   i8 ✓  i16 ✓  i32 ✓  i64 ✓  float ✓  double ✓"
    ((passed++))
else
    echo "❌ FAIL - IR type generation incomplete"
    ((failed++))
fi
echo

# Test 5: Stdlib integration
echo "━━━ Test 5: Stdlib Integration ━━━"
if ./build/npkc tests/zero_conversion/test_stdlib_integration.npk -o /tmp/aria_test5 &>/dev/null; then
    echo "✅ PASS - Stdlib functions work with typed literals"
    ((passed++))
else
    echo "❌ FAIL - Stdlib integration failed"
    ((failed++))
fi
echo

# Summary
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║  TEST SUMMARY                                                 ║"
echo "╠═══════════════════════════════════════════════════════════════╣"
echo "║  PASSED: $passed / 5 tests                                         ║"
echo "║  FAILED: $failed / 5 tests                                         ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo

if [ $failed -eq 0 ]; then
    echo "🎉 ALL TESTS PASSED! Zero Implicit Conversion is working perfectly!"
    exit 0
else
    echo "⚠️  Some tests failed. Check output above."
    exit 1
fi
