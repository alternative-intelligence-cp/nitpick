#!/bin/bash
# Nitpick Comptime Evaluator Tests — v0.20.3
# Tests: struct/array comparison, function local vars, recursive comptime,
#        memoization (Fibonacci), conditional branches in comptime

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_BIN="/tmp/npk_comptime_test_bin"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc compiler not found at $NPKC — skipping comptime tests"
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

echo "=== Comptime Evaluator Tests (v0.20.3) ==="
echo ""

# ── Array/comparison baseline ─────────────────────────────────────────────────
echo "-- Comptime comparison operators --"

expect_exit_ok \
    "comptime arithmetic and equality: 2+3 == 10-5" \
    "$SCRIPT_DIR/test_array_compare.npk"

# ── Local variable declarations in comptime ───────────────────────────────────
echo "-- Comptime VAR_DECL in function body --"

expect_exit_ok \
    "comptime local var: double(4) == 8 and add(2,2) == 4" \
    "$SCRIPT_DIR/test_struct_compare.npk"

# ── Conditional branches in comptime ─────────────────────────────────────────
echo "-- Comptime conditional (if/pass) --"

expect_exit_ok \
    "comptime max(3,7) == 7 and min(3,7) == 3" \
    "$SCRIPT_DIR/test_struct_store.npk"

# ── Recursive comptime with memoization ──────────────────────────────────────
echo "-- Recursive comptime + memoization --"

expect_exit_ok \
    "fib(10)==55, gcd(48,18)==6, power(2,8)==256" \
    "$SCRIPT_DIR/test_const_functions.npk"

echo ""
echo "=== Comptime Tests: $PASS/$TOTAL passed ==="

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
