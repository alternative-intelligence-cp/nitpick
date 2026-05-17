#!/bin/bash
# Nitpick Bug Regression Tests — v0.28.3
# Cross-function ARIA-032 Phase 1 (handles into callees).
#
#   bug267 — callee destroys arena passed by id; handle use after the
#            call is a compile-time ARIA-032.
#   bug268 — callee destroys a DIFFERENT arena; handles bound to the
#            live arena continue to compile and run.
#   bug269 — transitive destruction through a wrapper is NOT detected
#            in Phase 1 (documents the boundary; compiles + runs).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0283.exe"

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

echo "== v0.28.3 Cross-function ARIA-032 Phase 1 =="

expect_compile_fail "bug267 callee destroys arena → ARIA-032" \
    "$SCRIPT_DIR/bug267_handle_callee_destroys_arena.npk" \
    "ARIA-032"

expect_exit_code "bug268 callee destroys other arena" \
    "$SCRIPT_DIR/bug268_handle_callee_destroys_other_arena_pass.npk" \
    "0"

expect_exit_code "bug269 transitive not detected (Phase 1 limit)" \
    "$SCRIPT_DIR/bug269_handle_callee_transitive_not_detected_pass.npk" \
    "0"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
