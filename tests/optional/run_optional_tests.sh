#!/bin/bash
# Nitpick Optional<T> Tests — v0.20.4
# Tests: safe navigation on optional<struct>, null coalescing,
#        optional<T> type propagation through ?.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_BIN="/tmp/npk_optional_test_bin"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc compiler not found at $NPKC — skipping optional tests"
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

echo "=== Optional<T> Safe Navigation Tests (v0.20.4) ==="
echo ""

echo "-- Optional<struct> safe navigation --"
expect_exit_ok \
    "Point??.x returns field for value, NIL default for nil" \
    "$SCRIPT_DIR/test_safe_nav_struct.npk"

echo "-- Null coalescing on optional<struct> --"
expect_exit_ok \
    "Box??.value coalesces to default on NIL" \
    "$SCRIPT_DIR/test_nil_coalesce.npk"

echo ""
echo "=== Optional Tests: $PASS/$TOTAL passed ==="

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
