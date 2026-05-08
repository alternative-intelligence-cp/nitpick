#!/bin/bash
# Nitpick Warning Tests — v0.20.0
# Tests that UNUSED_FUNCTION, EMPTY_BLOCK, and warning suppression work correctly.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc compiler not found at $NPKC — skipping warning tests"
    exit 77  # CTest skip code
fi

PASS=0
FAIL=0
TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

echo "=== Warning System Tests (v0.20.0) ==="
echo ""

# Test 1: UNUSED_FUNCTION warning fires for uncalled private function
OUTPUT=$("$NPKC" -Wall "$SCRIPT_DIR/test_unused_function.npk" -o /tmp/npk_warn_test_out 2>&1 || true)
rm -f /tmp/npk_warn_test_out
if echo "$OUTPUT" | grep -qi "unused\|never called"; then
    pass "UNUSED_FUNCTION warning fires for uncalled private function"
else
    fail "UNUSED_FUNCTION warning fires for uncalled private function" \
         "expected 'unused'/'never called' in output; got: $OUTPUT"
fi

# Test 2: EMPTY_BLOCK warning fires for empty function body
OUTPUT=$("$NPKC" -Wall "$SCRIPT_DIR/test_empty_block.npk" -o /tmp/npk_warn_test_out 2>&1 || true)
rm -f /tmp/npk_warn_test_out
if echo "$OUTPUT" | grep -qi "empty\|EMPTY_BLOCK"; then
    pass "EMPTY_BLOCK warning fires for empty function body"
else
    fail "EMPTY_BLOCK warning fires for empty function body" \
         "expected 'empty'/'EMPTY_BLOCK' in output; got: $OUTPUT"
fi

# Test 3: Called function does NOT trigger UNUSED_FUNCTION
OUTPUT=$("$NPKC" -Wall "$SCRIPT_DIR/test_no_false_positive.npk" -o /tmp/npk_warn_test_out 2>&1 || true)
rm -f /tmp/npk_warn_test_out
if echo "$OUTPUT" | grep -qi "unused-function\|never called"; then
    fail "Called function does not trigger UNUSED_FUNCTION" \
         "got spurious unused-function warning for 'add'"
else
    pass "Called function does not trigger UNUSED_FUNCTION"
fi

# Test 4: -Wno-unused-function suppresses UNUSED_FUNCTION warning
OUTPUT=$("$NPKC" -Wall -Wno-unused-function \
         "$SCRIPT_DIR/test_warnings_suppressed.npk" -o /tmp/npk_warn_test_out 2>&1 || true)
rm -f /tmp/npk_warn_test_out
if echo "$OUTPUT" | grep -qi "unused-function\|never called"; then
    fail "-Wno-unused-function suppresses the warning" \
         "warning was not suppressed; got: $OUTPUT"
else
    pass "-Wno-unused-function suppresses the warning"
fi

echo ""
echo "=== Warning Tests: $PASS/$TOTAL passed ==="

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
