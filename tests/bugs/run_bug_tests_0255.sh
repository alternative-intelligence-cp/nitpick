#!/bin/bash
# Nitpick Bug Regression Tests — v0.25.5
# Runs bug195–bug199 (BORROW-009 two-phase borrows,
#                     BORROW-010 $$m self in trait impls).
#   bug195: c.bump(7i32) — UFCS $$m self method-call param-index fix.
#   bug196: c.bump(raw c.peek()) — receiver-derived $$i arg accepted.
#   bug197: $$m self impl writes through self.field.
#   bug198: $$m local borrow of self.n then self.n = .. rejected ARIA-026.
#   bug199: c.bump(...) while c is $$i-borrowed by y rejected ARIA-020.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0255_bin"

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

echo "== v0.25.5 two-phase + impl mut self regression tests =="

expect_compile_run "bug195 method-call $$m self with int arg" \
    "$SCRIPT_DIR/bug195_two_phase_method_call_self_arg_pass.npk"

expect_compile_run "bug196 method-call $$m self with derived $$i arg" \
    "$SCRIPT_DIR/bug196_two_phase_receiver_derived_arg_pass.npk"

expect_compile_run "bug197 impl mut self writes through self.field" \
    "$SCRIPT_DIR/bug197_impl_mut_self_field_writeback_pass.npk"

expect_compile_fail "bug198 impl mut self blocked by local alias borrow" \
    "$SCRIPT_DIR/bug198_impl_mut_self_blocks_alias_rejected.npk" \
    "ARIA-026"

expect_compile_fail "bug199 method-call rejected on already-borrowed receiver" \
    "$SCRIPT_DIR/bug199_method_call_with_live_loan_rejected.npk" \
    "ARIA-020"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
