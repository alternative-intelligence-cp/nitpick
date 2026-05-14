#!/bin/bash
# Nitpick Bug Regression Tests — v0.26.1 (re-tagged in v0.26.6)
# Runs bug205–bug209 (MEM-005 stack escape detection regression suite).
#   bug205: stack borrow return → ARIA-028 reject  (was ARIA-017 pre-v0.26.6)
#   bug206: stack-local struct field borrow return → ARIA-028 reject
#   bug207: chained borrow into stack local → ARIA-028 reject
#   bug208: gc-local borrow return → ARIA-028 reject (conservative)
#   bug209: stack-local borrow used internally only → pass

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0261_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_compile_run() {
    local label="$1"
    local file="$2"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local rc=$?
    if [ $rc -ne 0 ]; then
        fail "$label" "compile failed: $out"
        return
    fi
    "$TMP_BIN"
    local er=$?
    rm -f "$TMP_BIN"
    if [ $er -eq 0 ]; then
        pass "$label"
    else
        fail "$label" "binary exited $er"
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

echo "== v0.26.1 stack-escape (MEM-005) regression tests =="

expect_compile_fail "bug205 stack borrow return rejected" \
    "$SCRIPT_DIR/bug205_stack_borrow_return_rejected.npk" \
    "ARIA-028"

expect_compile_fail "bug206 stack field borrow return rejected" \
    "$SCRIPT_DIR/bug206_stack_field_borrow_return_rejected.npk" \
    "ARIA-028"

expect_compile_fail "bug207 stack chained borrow return rejected" \
    "$SCRIPT_DIR/bug207_stack_chained_borrow_return_rejected.npk" \
    "ARIA-028"

expect_compile_fail "bug208 gc-local borrow return conservative" \
    "$SCRIPT_DIR/bug208_gc_local_borrow_return_conservative.npk" \
    "ARIA-028"

expect_compile_run "bug209 stack-local borrow no escape pass" \
    "$SCRIPT_DIR/bug209_stack_local_borrow_no_escape_pass.npk"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
