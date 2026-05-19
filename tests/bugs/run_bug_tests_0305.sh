#!/bin/bash
# Nitpick Bug Regression Tests — v0.30.5
# Return-borrow inference across modules (v0.25.x carryover).
#
# v0.25.4 introduced `FunctionBorrowSummary::return_borrow_source_param`
# and the consumer at the borrow-binding site. v0.30.3 made cross-
# module summary ingest call `buildSummary` on imported FUNC_DECL and
# IMPL_DECL nodes — which already populates the field. So v0.30.5 is
# a verification slice: it pins the carryover behaviour with four
# fixtures and tags v0.30.5 so future regressions surface here.
#
#   bug326 — happy path: $$i binding to imported get_a(p) compiles.
#   bug327 — enforcement: mutating the source after the binding
#            fires ARIA-026 across the module boundary.
#   bug328 — multi-borrow rejection: the single-source heuristic
#            refuses to guess and emits ARIA-023 across the boundary.
#   bug329 — composition: local wrapper around imported get_a still
#            yields the correct source path; mutation fires ARIA-026.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0305.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# Expect: clean compile, executable produced.
expect_compile_ok() {
    local label="$1"
    local file="$2"
    rm -f "$TMP_EXE"
    local out rc
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    if [ $rc -ne 0 ] || [ ! -x "$TMP_EXE" ]; then
        fail "$label" "expected clean compile, exit $rc: $out"
        rm -f "$TMP_EXE"; return
    fi
    rm -f "$TMP_EXE"
    pass "$label"
}

# Expect: compile FAILS with exactly $want_count occurrences of $want_msg.
expect_compile_fail_count() {
    local label="$1"
    local file="$2"
    local want_msg="$3"
    local want_count="$4"
    rm -f "$TMP_EXE"
    local out rc count
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    if [ $rc -eq 0 ]; then
        fail "$label" "expected compile failure but got clean exit: $out"
        rm -f "$TMP_EXE"; return
    fi
    count=$(echo "$out" | grep -ci "$want_msg")
    if [ "$count" != "$want_count" ]; then
        fail "$label" "expected $want_count of '$want_msg', got $count. Output: $out"
        rm -f "$TMP_EXE"; return
    fi
    rm -f "$TMP_EXE"
    pass "$label"
}

echo "== v0.30.5 cross-module return-borrow inference =="

expect_compile_ok "bug326 xmod return-borrow binds cleanly" \
    "$SCRIPT_DIR/bug326_xmod_return_borrow_pass.npk"

expect_compile_fail_count "bug327 xmod source mutated rejected (ARIA-026)" \
    "$SCRIPT_DIR/bug327_xmod_return_borrow_source_mutated_rejected.npk" \
    "\[ARIA-026\]" 1

expect_compile_fail_count "bug328 xmod multi-borrow source unannotated rejected (ARIA-023)" \
    "$SCRIPT_DIR/bug328_xmod_multi_borrow_unannotated_rejected.npk" \
    "\[ARIA-023\]" 1

expect_compile_fail_count "bug329 xmod return-borrow via local wrapper rejected (ARIA-026)" \
    "$SCRIPT_DIR/bug329_xmod_return_borrow_via_wrapper_rejected.npk" \
    "\[ARIA-026\]" 1

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
