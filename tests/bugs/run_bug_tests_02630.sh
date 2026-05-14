#!/bin/bash
# Nitpick Bug Regression Tests — v0.26.3.0 (PIN baseline)
# Locks in current behavior of the `#x` pin operator before the
# lowering work in v0.26.3.{1..5}. Three fixtures:
#   bug217: `#stack_x` compiles + reads back. Must remain PASS forever
#           (PIN-DEC-001: stack pins stay no-op).
#   bug218: `#gc_x` compiles + reads back today (no runtime pin yet).
#           Will continue to PASS after v0.26.3.{2,3} land — what
#           changes is the IR emits npk_gc_pin/unpin.
#   bug219: ARIA-016 fires for `$$m` borrow of pinned gc binding
#           (negative — pure sema).
#
# See META/NITPICK/ROADMAP/0.26/0.26.3.x/PIN_LOWERING_AUDIT.md for the
# full pipeline status table and the v0.26.3.x slice plan.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_02630_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_compile_run_exit() {
    local label="$1"
    local file="$2"
    local want="$3"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local rc=$?
    if [ $rc -ne 0 ]; then
        fail "$label" "compile failed: $out"
        return
    fi
    timeout 30 "$TMP_BIN"
    local er=$?
    rm -f "$TMP_BIN"
    if [ "$er" -eq "$want" ]; then
        pass "$label"
    else
        fail "$label" "binary exited $er (wanted $want)"
    fi
}

expect_compile_error() {
    local label="$1"
    local file="$2"
    local needle="$3"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    rm -f "$TMP_BIN"
    if echo "$out" | grep -q "$needle"; then
        pass "$label"
    else
        fail "$label" "expected '$needle' in compiler output, got: $out"
    fi
}

echo "== v0.26.3.0 PIN baseline (PIN-001/002/003) regression tests =="

expect_compile_run_exit "bug217 #stack_x baseline (no runtime pin)" \
    "$SCRIPT_DIR/bug217_pin_stack_baseline_pass.npk" 42

expect_compile_run_exit "bug218 #gc_x baseline today (lowering arrives in v0.26.3.{2,3})" \
    "$SCRIPT_DIR/bug218_pin_gc_baseline_today_pass.npk" 42

expect_compile_error    "bug219 ARIA-016 rejects \$\$m borrow of pinned gc binding" \
    "$SCRIPT_DIR/bug219_pin_gc_borrow_conflict_rejected.npk" \
    "ARIA-016"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
