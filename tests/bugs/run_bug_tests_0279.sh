#!/bin/bash
# Nitpick Bug Regression Tests — v0.27.9 (Handle<T> Phase 3, ARIA-032)
#
#   bug260 — handle deref after arena destroy → ARIA-032 (compile-fail).
#   bug261 — handle free  after arena destroy → ARIA-032 (compile-fail).
#   bug262 — two arenas; handle bound to the LIVE arena still works.
#   bug263 — arena destroy AFTER the handle is freed and used compiles.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0279.exe"

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

echo "== v0.27.9 Handle<T> Phase 3 — ARIA-032 handle-outlives-arena =="

expect_compile_fail "bug260 deref after destroy" \
    "$SCRIPT_DIR/bug260_handle_outlives_arena_deref.npk" \
    "ARIA-032"

expect_compile_fail "bug261 free after destroy" \
    "$SCRIPT_DIR/bug261_handle_outlives_arena_free.npk" \
    "ARIA-032"

expect_exit_code "bug262 sibling arena still alive" \
    "$SCRIPT_DIR/bug262_handle_other_arena_alive_pass.npk" \
    "0"

expect_exit_code "bug263 destroy after correct teardown" \
    "$SCRIPT_DIR/bug263_handle_destroy_after_use_pass.npk" \
    "0"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
