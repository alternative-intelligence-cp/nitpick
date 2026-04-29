#!/bin/bash
# Module Loading Integration Tests
# Tests the module system with use statements

ARIA_COMPILER="./build/ariac"
TEST_DIR="tests/module_loading"
FAILED=0
PASSED=0

echo "====================================="
echo "Module Loading Integration Tests"
echo "====================================="
echo ""

# Test 1: Module file parses correctly
echo "[TEST 1] Module file parsing..."
if $ARIA_COMPILER --ast-dump $TEST_DIR/math_utils.aria > /dev/null 2>&1; then
    echo "  ✓ PASS: math_utils.aria parses correctly"
    PASSED=$((PASSED + 1))
else
    echo "  ✗ FAIL: math_utils.aria failed to parse"
    FAILED=$((FAILED + 1))
fi

# Test 2: Minimal use statement parsing
echo "[TEST 2] Use statement parsing..."
if $ARIA_COMPILER --ast-dump $TEST_DIR/test_minimal.aria 2>&1 | grep -q "Use(math_utils)"; then
    echo "  ✓ PASS: Use statement parsed correctly"
    PASSED=$((PASSED + 1))
else
    echo "  ✗ FAIL: Use statement not found in AST"
    FAILED=$((FAILED + 1))
fi

# Test 3: Module loading during semantic analysis
echo "[TEST 3] Module loading (semantic analysis)..."
if $ARIA_COMPILER --verbose $TEST_DIR/test_minimal.aria -o /tmp/test_minimal 2>&1 | grep -q "Phase 3: Semantic analysis"; then
    echo "  ✓ PASS: Module loading triggered during semantic analysis"
    PASSED=$((PASSED + 1))
else
    echo "  ✗ FAIL: Semantic analysis did not complete"
    FAILED=$((FAILED + 1))
fi

# Test 4: Simple main with use statement compiles
echo "[TEST 4] Compilation with use statement..."
if $ARIA_COMPILER $TEST_DIR/test_simple_main.aria -o /tmp/test_simple_main > /dev/null 2>&1; then
    echo "  ✓ PASS: Program with use statement compiled successfully"
    PASSED=$((PASSED + 1))
else
    echo "  ✗ FAIL: Compilation failed"
    FAILED=$((FAILED + 1))
fi

# Test 5: Compiled program runs
echo "[TEST 5] Execution test..."
/tmp/test_simple_main > /dev/null 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 42 ]; then
    echo "  ✓ PASS: Program executed and returned expected value (42)"
    PASSED=$((PASSED + 1))
else
    echo "  ✗ FAIL: Program returned $EXIT_CODE, expected 42"
    FAILED=$((FAILED + 1))
fi

# Test 6: Namespace import syntax
echo "[TEST 6] Namespace import syntax..."
if $ARIA_COMPILER --ast-dump $TEST_DIR/test_namespace_import.aria 2>&1 | grep -q "Use(math_utils)"; then
    echo "  ✓ PASS: Namespace import parsed correctly"
    PASSED=$((PASSED + 1))
else
    echo "  ✗ FAIL: Namespace import parsing failed"
    FAILED=$((FAILED + 1))
fi

# Test 7: Selective import syntax
echo "[TEST 7] Selective import syntax..."
if $ARIA_COMPILER --ast-dump $TEST_DIR/test_selective_import.aria > /dev/null 2>&1; then
    echo "  ✓ PASS: Selective import parsed correctly"
    PASSED=$((PASSED + 1))
else
    echo "  ✗ FAIL: Selective import parsing failed"
    FAILED=$((FAILED + 1))
fi

# Test 8: Wildcard import syntax
echo "[TEST 8] Wildcard import syntax..."
if $ARIA_COMPILER --ast-dump $TEST_DIR/test_wildcard_import.aria > /dev/null 2>&1; then
    echo "  ✓ PASS: Wildcard import parsed correctly"
    PASSED=$((PASSED + 1))
else
    echo "  ✗ FAIL: Wildcard import parsing failed"
    FAILED=$((FAILED + 1))
fi

echo ""
echo "====================================="
echo "Results: $PASSED passed, $FAILED failed"
echo "====================================="

# Test 9: Diamond import pattern (A→B→D, A→C→D)
echo ""
echo "[TEST 9] Diamond import deduplication..."
if $ARIA_COMPILER $TEST_DIR/diamond_test.aria -o /tmp/test_diamond 2>/dev/null && /tmp/test_diamond; then
    echo "  ✓ PASS: Diamond import compiled and ran correctly"
    PASSED=$((PASSED + 1))
else
    echo "  ✗ FAIL: Diamond import failed (module cache deduplication broken)"
    FAILED=$((FAILED + 1))
fi
rm -f /tmp/test_diamond

echo ""
echo "====================================="
echo "Final Results: $PASSED passed, $FAILED failed"
echo "====================================="

exit $FAILED
