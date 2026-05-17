#!/bin/bash
# Nitpick Bug Regression Tests — v0.28.5
# ARIA-032 FFI passthrough rule.
#
#   bug276 — Handle<T> wrapped in @cast<int64> when handed to FFI:
#            compiles cleanly (no ARIA-032 warning), exits 0.
#   bug277 — Handle<T> passed directly to extern (no cast):
#            compiles successfully but stderr carries ARIA-032 warning.
#   bug278 — Full roundtrip through int64 token, runtime generation
#            check still catches stale handle after free.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0285.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_exit_code_clean() {
    # Compile + run; expect a clean compile (no ARIA-032 in stderr)
    # and the given runtime exit code.
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
    if echo "$out" | grep -qi "ARIA-032"; then
        fail "$label" "unexpected ARIA-032 diagnostic in clean-compile test: $out"
        rm -f "$TMP_EXE"
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

expect_compile_warn() {
    # Compile must succeed (exit 0 + executable produced), and the
    # given warning string must appear in stderr.
    local label="$1"
    local file="$2"
    local expect_msg="$3"

    rm -f "$TMP_EXE"
    local out rc
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    if [ $rc -ne 0 ] || [ ! -x "$TMP_EXE" ]; then
        fail "$label" "expected clean compile (warning only) but got exit $rc: $out"
        rm -f "$TMP_EXE"
        return
    fi
    if ! echo "$out" | grep -qi "$expect_msg"; then
        fail "$label" "compile succeeded but expected warning '$expect_msg' not in output: $out"
        rm -f "$TMP_EXE"
        return
    fi
    rm -f "$TMP_EXE"
    pass "$label"
}

echo "== v0.28.5 ARIA-032 FFI passthrough rule =="

expect_exit_code_clean "bug276 explicit @cast<int64>(h) silences FFI warning" \
    "$SCRIPT_DIR/bug276_handle_ffi_int64_token_pass.npk" \
    "0"

expect_compile_warn "bug277 direct handle pass to extern → ARIA-032 warning" \
    "$SCRIPT_DIR/bug277_handle_ffi_direct_pass_warns.npk" \
    "ARIA-032"

expect_exit_code_clean "bug278 int64 token roundtrip + runtime generation check" \
    "$SCRIPT_DIR/bug278_handle_ffi_roundtrip_runtime_pass.npk" \
    "0"

echo ""
echo "v0.28.5 results: $PASS/$TOTAL passed, $FAIL failed"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
