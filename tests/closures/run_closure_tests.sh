#!/bin/bash
# Nitpick Closure Lifetime Tests — v0.20.4
# Tests: closure capture analysis (isFromOuterScope fix, validateLifetimes,
#        ClosureAnalyzer wired into TypeChecker)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_BIN="/tmp/npk_closure_test_bin"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc compiler not found at $NPKC — skipping closure tests"
    exit 77  # CTest skip code
fi

PASS=0
FAIL=0
TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# Helper: compile + run, check exit 0
expect_exit_ok() {
    local label="$1"
    local file="$2"
    local compile_out rc
    compile_out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    rc=$?
    if [ $rc -ne 0 ]; then
        fail "$label" "compile failed (exit $rc): $compile_out"
        rm -f "$TMP_BIN"
        return
    fi
    "$TMP_BIN" 2>&1
    rc=$?
    rm -f "$TMP_BIN"
    if [ $rc -eq 0 ]; then
        pass "$label"
    else
        fail "$label" "program exited $rc (expected 0)"
    fi
}

echo "=== Closure Lifetime Tests (v0.20.4) ==="
echo ""

echo "-- Closure capture: by-value --"
expect_exit_ok \
    "lambda captures outer int32 by value: add_base(3)==13" \
    "$SCRIPT_DIR/test_capture_byvalue.npk"

echo "-- Closure capture: multiple variables --"
expect_exit_ok \
    "lambda captures two outer vars: a+b == 12" \
    "$SCRIPT_DIR/test_capture_multi.npk"

echo "-- Closure: no captures (pure lambda) --"
expect_exit_ok \
    "pure lambda multiply(4,5)==20" \
    "$SCRIPT_DIR/test_no_capture.npk"

echo ""
echo "=== Closure Tests: $PASS/$TOTAL passed ==="

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
