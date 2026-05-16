#!/bin/bash
# Nitpick Bug Regression Tests — v0.26.6
# Runs bug232–bug235 (MEM-014 / ARIA-028 STACK_ESCAPE polish suite).
#   bug232: stack borrow escape via fail        → ARIA-028 reject
#   bug233: stack borrow escape from nested block → ARIA-028 reject
#   bug234: polished message contents           → ARIA-028 + grep "stack frame"
#   bug235: internal-only borrow                → compile + run exit 0
#
# Pre-existing v0.26.1 suite (bug205-209) was re-tagged ARIA-017 → ARIA-028
# and continues to live in run_bug_tests_0261.sh.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0266_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

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

expect_compile_fail_all() {
    local label="$1"
    local file="$2"
    shift 2
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local rc=$?
    rm -f "$TMP_BIN"
    if [ $rc -eq 0 ]; then
        fail "$label" "expected compile failure but got exit 0"
        return
    fi
    for needle in "$@"; do
        if ! echo "$out" | grep -qi "$needle"; then
            fail "$label" "missing expected substring '$needle' in: $out"
            return
        fi
    done
    pass "$label"
}

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

echo "== v0.26.6 ARIA-028 STACK_ESCAPE polish (MEM-014) regression tests =="

expect_compile_fail "bug232 stack escape via fail" \
    "$SCRIPT_DIR/bug232_stack_escape_via_fail.npk" \
    "ARIA-028"

expect_compile_fail "bug233 stack escape from nested block" \
    "$SCRIPT_DIR/bug233_stack_escape_nested_block.npk" \
    "ARIA-028"

expect_compile_fail_all "bug234 polished message contents" \
    "$SCRIPT_DIR/bug234_stack_escape_message_quality.npk" \
    "ARIA-028" "stack frame" "host" "bad"

expect_compile_run "bug235 internal-only borrow pass" \
    "$SCRIPT_DIR/bug235_borrow_internal_use_pass.npk"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
