#!/bin/bash
# Nitpick Bug Regression Tests — v0.26.2
# Runs bug210–bug212 (MEM-006/007/008 GC stress smoke suite).
#   bug210: 50k small gc allocations, no surviving refs.
#   bug211: 5k large (8x int64) gc allocations.
#   bug212: long-lived gc holder must survive 30k churn allocations.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0262_bin"

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

echo "== v0.26.2 GC stress (MEM-006/007/008) regression tests =="

expect_compile_run "bug210 gc 50k small allocations" \
    "$SCRIPT_DIR/bug210_gc_alloc_stress.npk"

expect_compile_run "bug211 gc 5k large allocations" \
    "$SCRIPT_DIR/bug211_gc_large_object_stress.npk"

expect_compile_run "bug212 gc survivor holder smoke" \
    "$SCRIPT_DIR/bug212_gc_survivor_smoke.npk"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
