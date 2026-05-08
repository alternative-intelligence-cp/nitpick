#!/bin/bash
# Nitpick Bug Regression Tests — v0.21.3
# Runs bugs/bug094–bug099 (A-008 ICE message, A-013 pick range exhaustiveness,
#                          A-014 catch diagnostic regression, fixed-negative).
# Files ending _pass.npk must compile and exit 0 when run.
# Files ending _fail.npk must fail to compile (non-zero exit from npkc).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0213_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# expect_compile_run: file should compile AND produce exit 0 when run
expect_compile_run() {
    local label="$1"
    local file="$2"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local compile_rc=$?
    if [ $compile_rc -ne 0 ]; then
        fail "$label" "compile failed (exit $compile_rc): $out"
        rm -f "$TMP_BIN"; return
    fi
    "$TMP_BIN" >/dev/null 2>&1
    local run_rc=$?
    rm -f "$TMP_BIN"
    if [ $run_rc -eq 0 ]; then
        pass "$label"
    else
        fail "$label" "program exited $run_rc"
    fi
}

# expect_compile_fail: file should fail to compile
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

echo "=== Bug Regression Tests (v0.21.3: bug094–bug099) ==="
echo ""

# A-008: ICE message quality — valid programs should compile cleanly
expect_compile_run \
    "bug094: type inference ICE message — valid program compiles fine" \
    "$SCRIPT_DIR/bug094_type_inference_ice_message_pass.npk"

# A-013: pick int32 range exhaustiveness
expect_compile_fail \
    "bug095: pick int32 range arm no wildcard — non-exhaustive, reports missing intervals" \
    "$SCRIPT_DIR/bug095_pick_int32_range_no_wildcard_fail.npk" \
    "Missing cases"

expect_compile_run \
    "bug096: pick int32 range arm with wildcard (*) still compiles OK" \
    "$SCRIPT_DIR/bug096_pick_int32_range_with_wildcard_pass.npk"

expect_compile_fail \
    "bug097: pick int64 range arm no wildcard — non-exhaustive, reports missing intervals" \
    "$SCRIPT_DIR/bug097_pick_int64_range_no_wildcard_fail.npk" \
    "Missing cases"

# A-014: catch keyword diagnostic (regression from v0.21.1)
expect_compile_fail \
    "bug098: catch keyword gives specific diagnostic mentioning catch" \
    "$SCRIPT_DIR/bug098_catch_keyword_diagnostic_fail.npk" \
    "catch"

# Fixed-negative regression (BUG-09, v0.13.0)
expect_compile_run \
    "bug099: fixed binding with negative arithmetic evaluates correctly" \
    "$SCRIPT_DIR/bug099_fixed_negative_arithmetic_pass.npk"

echo ""
echo "=== Bug Tests (v0.21.3): $PASS/$TOTAL passed ==="

[ "$FAIL" -gt 0 ] && exit 1
exit 0
