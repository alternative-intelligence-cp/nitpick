#!/bin/bash
# Nitpick Bug Regression Tests — v0.27.2
# Runs bug237–bug239 (ARIA-029 GC_REF_FROM_WILD).
#   bug237: gc binding into wild slot, no pin → ARIA-029 reject
#   bug238: pin-derived (#h) into wild slot   → compile + run exit 0
#   bug239: shadow_stack-style raw wild alloc → compile + run exit 0
#
# v0.27.3 will add the "indirect through struct field" companion case.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0272_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

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

echo "── v0.27.2 ARIA-029 GC_REF_FROM_WILD ──────────────────────────"

expect_compile_fail_all \
    "bug237 gc binding into wild slot, no pin → ARIA-029" \
    "${SCRIPT_DIR}/bug237_gc_ref_from_wild_rejected.npk" \
    "ARIA-029" "pin operator"

expect_compile_run \
    "bug238 pin-derived #h into wild slot" \
    "${SCRIPT_DIR}/bug238_pin_into_wild_pass.npk"

expect_compile_run \
    "bug239 raw wild alloc, no gc origin" \
    "${SCRIPT_DIR}/bug239_wild_raw_alloc_pass.npk"

echo "── results ─────────────────────────────────────────────────────"
echo "  total:  ${TOTAL}"
echo "  passed: ${PASS}"
echo "  failed: ${FAIL}"
[ "$FAIL" -eq 0 ]
