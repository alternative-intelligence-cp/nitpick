#!/bin/bash
# Nitpick Bug Regression Tests — v0.28.4
# Cross-function ARIA-032 Phase 2 part A (handle return from local arena).
#
#   bug270 — pass a Handle bound to a locally-created arena → compile fail.
#   bug271 — same shape but arena threaded in as a parameter → compiles + runs.
#   bug272 — inline `pass raw HandleArena.alloc(localArena, ..)` → compile fail.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0284.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_exit_code() {
    local label="$1"
    local file="$2"
    local expected="$3"

    rm -f "$TMP_EXE"
    local out
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    if [ ! -x "$TMP_EXE" ]; then
        fail "$label" "compile failed: $out"
        return
    fi
    "$TMP_EXE" 2>/dev/null
    local actual=$?
    if [ "$actual" != "$expected" ]; then
        fail "$label" "expected exit $expected but got $actual"
        rm -f "$TMP_EXE"
        return
    fi
    rm -f "$TMP_EXE"
    pass "$label"
}

expect_compile_fail() {
    local label="$1"
    local file="$2"
    local expect_msg="$3"

    rm -f "$TMP_EXE"
    local out rc
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    rm -f "$TMP_EXE"
    if [ $rc -eq 0 ]; then
        fail "$label" "expected compile failure but got exit 0"
        return
    fi
    if [ -n "$expect_msg" ] && ! echo "$out" | grep -qi "$expect_msg"; then
        fail "$label" "compile failed (good) but expected message '$expect_msg' not in output: $out"
        return
    fi
    pass "$label"
}

echo "== v0.28.4 Cross-function ARIA-032 Phase 2 part A =="

expect_compile_fail "bug270 return Handle bound to local arena → ARIA-032" \
    "$SCRIPT_DIR/bug270_handle_return_from_local_arena_rejected.npk" \
    "ARIA-032"

expect_exit_code "bug271 return Handle bound to param arena (positive)" \
    "$SCRIPT_DIR/bug271_handle_return_from_param_arena_pass.npk" \
    "0"

expect_compile_fail "bug272 inline alloc from local arena → ARIA-032" \
    "$SCRIPT_DIR/bug272_handle_inline_alloc_from_local_arena_rejected.npk" \
    "ARIA-032"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
