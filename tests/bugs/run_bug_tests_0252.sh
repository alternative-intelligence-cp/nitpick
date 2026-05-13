#!/bin/bash
# Nitpick Bug Regression Tests — v0.25.2
# Runs bug179–bug183 (BORROW-004 multi-dim & nested array borrow paths).
#   bug179: arr2[0][1] disjoint from arr2[0][2] (literal-literal).
#   bug180: pts[0].x disjoint from pts[0].y (index-then-field).
#   bug181: pts[0].x vs pts[0].x rejected (ARIA-023).
#   bug182: arr2[0][1] vs arr2[i][2] rejected conservatively (ARIA-023).
#   bug183: arr2[i][k] vs arr2[j][m] rejected conservatively (ARIA-023).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0252_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_compile_run() {
    local label="$1"
    local file="$2"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local compile_rc=$?
    if [ $compile_rc -ne 0 ]; then
        fail "$label" "compile failed (exit $compile_rc): $out"
        rm -f "$TMP_BIN"; return
    fi
    "$TMP_BIN" >/dev/null 2>&1
    local run_rc=$?
    rm -f "$TMP_BIN"
    if [ $run_rc -eq 0 ]; then
        pass "$label"
    else
        fail "$label" "program exited $run_rc"
    fi
}

expect_compile_fail() {
    local label="$1"
    local file="$2"
    local expect_msg="$3"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local rc=$?
    rm -f "$TMP_BIN"
    if [ $rc -eq 0 ]; then
        fail "$label" "expected compile failure but got exit 0"
    elif [ -n "$expect_msg" ] && ! echo "$out" | grep -qi "$expect_msg"; then
        fail "$label" "compile failed (good) but expected msg '$expect_msg' not in: $out"
    else
        pass "$label"
    fi
}

echo "== v0.25.2 multi-dim & nested array borrow regression tests =="

expect_compile_run "bug179 arr2[0][1] disjoint from arr2[0][2]" \
    "$SCRIPT_DIR/bug179_arr2_disjoint_literal_pass.npk"

expect_compile_run "bug180 pts[0].x disjoint from pts[0].y" \
    "$SCRIPT_DIR/bug180_arr_of_struct_field_disjoint_pass.npk"

expect_compile_fail "bug181 pts[0].x vs pts[0].x rejected" \
    "$SCRIPT_DIR/bug181_arr_of_struct_same_field_rejected.npk" \
    "ARIA-023"

expect_compile_fail "bug182 arr2[0][1] vs arr2[i][2] rejected" \
    "$SCRIPT_DIR/bug182_arr2_lit_vs_dyn_rejected.npk" \
    "ARIA-023"

expect_compile_fail "bug183 arr2[i][k] vs arr2[j][m] rejected" \
    "$SCRIPT_DIR/bug183_arr2_fully_dynamic_rejected.npk" \
    "ARIA-023"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
