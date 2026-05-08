#!/bin/bash
# Nitpick Vec9 Dynamic Indexing Tests — v0.20.5
# Tests: ivec9/vec9 dynamic index via runtime variable (loop $ counter)
# Root fix: ICmpEQ type mismatch (i64 index vs i32 getInt32) in ir_generator.cpp

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_BIN="/tmp/npk_vec_test_bin"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc compiler not found at $NPKC — skipping vec tests"
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
        return
    fi
    "$TMP_BIN"
    rc=$?
    if [ $rc -eq 0 ]; then
        pass "$label"
    else
        fail "$label" "runtime exit $rc"
    fi
}

echo "=== Vec9 Dynamic Indexing Tests (v0.20.5) ==="

expect_exit_ok "ivec9 dynamic index (loop \$ counter)" \
    "${SCRIPT_DIR}/test_vec9_dynamic_index.npk"

echo ""
echo "Results: $PASS/$TOTAL passed, $FAIL failed"
echo ""

rm -f "$TMP_BIN"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
