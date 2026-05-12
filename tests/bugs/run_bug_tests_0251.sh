#!/bin/bash
# Nitpick Bug Regression Tests — v0.25.1
# Runs bug175–bug178 (BORROW-002 defer body + BORROW-003 early-exit leak).
#   bug175: defer body $$m × 2 of same host now rejected (ARIA-023).
#   bug176: defer body single $$i borrow still compiles + runs.
#   bug177: early exit() with unfreed wildx now flagged (ARIA-014).
#   bug178: early exit() with paired defer free is OK.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0251_bin"

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

echo "== v0.25.1 defer body + early-exit borrow regression tests =="

expect_compile_fail "bug175 defer body double-mut rejected" \
    "$SCRIPT_DIR/bug175_defer_body_borrow_conflict_rejected.npk" \
    "ARIA-023"

expect_compile_run "bug176 defer body single borrow pass" \
    "$SCRIPT_DIR/bug176_defer_body_borrow_pass.npk"

expect_compile_fail "bug177 early exit wildx leak rejected" \
    "$SCRIPT_DIR/bug177_early_exit_leak_rejected.npk" \
    "ARIA-014"

expect_compile_run "bug178 early exit with defer free pass" \
    "$SCRIPT_DIR/bug178_early_exit_with_defer_pass.npk"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
