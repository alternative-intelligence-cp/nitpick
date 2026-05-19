#!/bin/bash
# Nitpick Bug Regression Tests — v0.25.6
# Runs bug200–bug204 (BORROW-011 closure capture borrow tracking,
#                     BORROW-012 multi-await borrow polish).
#   bug200: closure read-only capture is BY_VALUE; outer $$m borrow OK.
#   bug201: closure mutates capture → BY_REFERENCE; outer $$m rejected ARIA-023.
#   bug202: closure mutates capture → BY_REFERENCE; outer $$i rejected ARIA-023.
#   bug203: multi-await with $$m borrow released between awaits → no ARIA-030.
#   bug204: multi-await with $$m borrow live across awaits → ARIA-030 at each await.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0256_bin"

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

expect_compile_warn_count() {
    local label="$1"
    local file="$2"
    local expect_msg="$3"
    local expect_count="$4"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local rc=$?
    rm -f "$TMP_BIN"
    if [ $rc -ne 0 ]; then
        fail "$label" "compile failed: $out"
        return
    fi
    local count
    count=$(echo "$out" | grep -c "$expect_msg")
    if [ "$count" != "$expect_count" ]; then
        fail "$label" "expected $expect_count occurrences of '$expect_msg', got $count"
    else
        pass "$label"
    fi
}

expect_compile_no_warn() {
    local label="$1"
    local file="$2"
    local forbid_msg="$3"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local rc=$?
    rm -f "$TMP_BIN"
    if [ $rc -ne 0 ]; then
        fail "$label" "compile failed: $out"
        return
    fi
    if echo "$out" | grep -q "$forbid_msg"; then
        fail "$label" "did not expect '$forbid_msg' in output: $out"
    else
        pass "$label"
    fi
}

echo "== v0.25.6 closure capture + multi-await regression tests =="

expect_compile_run "bug200 closure read-only capture + outer $\$m" \
    "$SCRIPT_DIR/bug200_closure_byvalue_capture_then_mut_borrow_pass.npk"

expect_compile_fail "bug201 closure mut capture blocks outer $\$m" \
    "$SCRIPT_DIR/bug201_closure_mut_capture_blocks_mut_borrow_rejected.npk" \
    "ARIA-023"

expect_compile_fail "bug202 closure mut capture blocks outer $\$i" \
    "$SCRIPT_DIR/bug202_closure_mut_capture_blocks_imm_borrow_rejected.npk" \
    "ARIA-023"

expect_compile_no_warn "bug203 multi-await released borrow no diagnostic" \
    "$SCRIPT_DIR/bug203_multi_await_released_borrow_pass.npk" \
    "ARIA-041"

# v0.31.0.4 (D-5): borrow across await is now an ARIA-041 hard error (was a
# 2-count ARIA-030 warning). bug204 flips from "warn" to "fail".
expect_compile_fail "bug204 multi-await live borrow → ARIA-041 (was 2x ARIA-030 warn pre v0.31.0.4)" \
    "$SCRIPT_DIR/bug204_multi_await_live_borrow_warns.npk" \
    "ARIA-041"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
