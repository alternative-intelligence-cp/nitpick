#!/bin/bash
# Nitpick Bug Regression Tests — v0.26.3
# Runs bug213–bug215 (MEM-009 borrow + GC safepoint interaction).
#   bug213: $m borrow of gc survives an explicit npk_gc_safepoint() call.
#   bug214: $m borrow of gc survives a heavy churn + safepoint loop.
#   bug215: ARIA-023 still fires for double $m borrow of a gc binding (negative).
#
# Note: MEM-010 (pin syntax `#x` lowering + gc-binding root tracking) was
# CLOSED by the v0.26.3.x sub-series. v0.26.3.2 auto-pins every gc binding
# at allocation and registers a shadow-stack root; v0.26.3.3 reinstates
# the MEM-010 end-to-end tests as bug224 / bug225 in
# `run_bug_tests_02633.sh`. See `META/NITPICK/ROADMAP/0.26/0.26.3.x/`.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0263_bin"

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
    timeout 30 "$TMP_BIN"
    local er=$?
    rm -f "$TMP_BIN"
    if [ $er -eq 0 ]; then
        pass "$label"
    else
        fail "$label" "binary exited $er"
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

echo "== v0.26.3 GC borrow + safepoint (MEM-009) regression tests =="

expect_compile_run   "bug213 gc \$m borrow survives explicit safepoint" \
    "$SCRIPT_DIR/bug213_gc_borrow_across_safepoint_pass.npk"

expect_compile_run   "bug214 gc \$m borrow survives churn + safepoint" \
    "$SCRIPT_DIR/bug214_gc_borrow_churn_safepoint_pass.npk"

expect_compile_error "bug215 ARIA-023 rejects double \$m borrow of gc binding" \
    "$SCRIPT_DIR/bug215_gc_double_mut_borrow_rejected.npk" \
    "ARIA-023"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
