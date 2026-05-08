#!/bin/bash
# Nitpick Bug Regression Tests — v0.21.4
# Runs bugs/bug100–bug102 (A-005 wildx x86 regression, A-006/A-007 codegen
# regression, and A-015/ABI extern regression).
# Files ending _pass.npk must compile and exit 0 when run.
# Files ending _fail.npk must fail to compile (non-zero exit from npkc).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0214_bin"

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

echo "=== Bug Regression Tests (v0.21.4: bug100–bug102) ==="
echo ""

# A-005: AArch64 JIT check must not affect x86-64 builds
expect_compile_run \
    "bug100: wildx alloc/free unaffected by AArch64 JIT check (A-005)" \
    "$SCRIPT_DIR/bug100_wildx_x86_unaffected_pass.npk"

# A-006/A-007: Complex codegen regression — ICE message format change must not
# break normal statement/expression codegen paths
expect_compile_run \
    "bug101: complex loop+pick+struct codegen — ICE format change regression (A-006/A-007)" \
    "$SCRIPT_DIR/bug101_complex_codegen_regression_pass.npk"

# ABI docs regression — extern int32 function still works correctly
expect_compile_run \
    "bug102: extern abs() ABI — int32 param/return regression (abi.md)" \
    "$SCRIPT_DIR/bug102_extern_abi_int32_pass.npk"

echo ""
echo "=== Bug Tests (v0.21.4): $PASS/$TOTAL passed ==="

[ "$FAIL" -gt 0 ] && exit 1
exit 0
