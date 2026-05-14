#!/bin/bash
# Nitpick Bug Regression Tests — v0.26.3.3 (MEM-010 reinstatement)
#
# v0.26.3.0 originally dropped the runtime-pin end-to-end test because
# (a) the `#x` operator did not lower to `npk_gc_pin`, and (b) gc
# bindings were not registered as GC roots, so any explicit safepoint
# between allocation and read could invalidate the binding. Both issues
# were closed by v0.26.3.2 (auto-pin + shadow-stack rooting at the
# allocation site rather than at the `#` operator).
#
# This slice reinstates MEM-010 with two execution-level fixtures:
#   bug224 — gc binding survives an explicit npk_gc_safepoint() loop
#            (no borrow, no pin alias — just the auto-rooted binding).
#   bug225 — `wild T->:p = #h;` alias survives the same safepoint
#            loop and reads through correctly afterwards.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_02633.exe"

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
    "$TMP_EXE"
    local actual=$?
    if [ "$actual" != "$expected" ]; then
        fail "$label" "expected exit $expected but got $actual"
        rm -f "$TMP_EXE"
        return
    fi
    rm -f "$TMP_EXE"
    pass "$label"
}

echo "== v0.26.3.3 MEM-010 reinstatement (gc safepoint + pin alias) =="

expect_exit_code "bug224 gc binding survives explicit safepoint loop" \
    "$SCRIPT_DIR/bug224_gc_survives_explicit_safepoint.npk" \
    "0"

expect_exit_code "bug225 #h pin alias survives explicit safepoint loop" \
    "$SCRIPT_DIR/bug225_gc_pin_alias_survives_safepoint.npk" \
    "0"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
