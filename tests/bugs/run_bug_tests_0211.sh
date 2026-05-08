#!/bin/bash
# Nitpick Bug Regression Tests — v0.21.1
# Runs bugs/bug082–bug088 (cfg, derive(Display), catch keyword diagnostic).
# Files ending _pass.npk must compile and exit 0.
# Files ending _fail.npk must fail to compile (non-zero exit from npkc).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_bin"

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
    local out rc
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    rc=$?
    rm -f "$TMP_BIN"
    if [ $rc -eq 0 ]; then
        fail "$label" "expected compile failure but got exit 0"
    elif [ -n "$expect_msg" ] && ! echo "$out" | grep -qi "$expect_msg"; then
        fail "$label" "compiled failed (good) but expected msg '$expect_msg' not in: $out"
    else
        pass "$label"
    fi
}

echo "=== Bug Regression Tests (v0.21.1: bug082–bug088) ==="
echo ""

# A-001: cfg conditional compilation
expect_compile_run \
    "bug082: #[cfg(target_os=linux)] includes function on Linux" \
    "$SCRIPT_DIR/bug082_cfg_target_os_linux_pass.npk"

expect_compile_run \
    "bug083: #[cfg(not(target_os=windows))] is true on Linux" \
    "$SCRIPT_DIR/bug083_cfg_not_target_os_pass.npk"

expect_compile_run \
    "bug084: #[cfg(all(...))] conjunction true on Linux" \
    "$SCRIPT_DIR/bug084_cfg_all_predicate_pass.npk"

expect_compile_fail \
    "bug085: #[cfg(target_os=windows)] excludes function on Linux — call fails" \
    "$SCRIPT_DIR/bug085_cfg_excluded_func_fail.npk" \
    "undefined\|not defined\|unknown\|undeclared"

# A-002: derive(Display)
expect_compile_run \
    "bug086: derive(Display) auto-generates display() method" \
    "$SCRIPT_DIR/bug086_derive_display_pass.npk"

expect_compile_run \
    "bug087: explicit impl:Display overrides derive(Display)" \
    "$SCRIPT_DIR/bug087_derive_display_override_pass.npk"

# A-014: catch keyword diagnostic
expect_compile_fail \
    "bug088: 'catch' in statement position gives clear parse error" \
    "$SCRIPT_DIR/bug088_catch_keyword_diagnostic_fail.npk" \
    "catch"

echo ""
echo "=== Bug Tests: $PASS/$TOTAL passed ==="

[ "$FAIL" -gt 0 ] && exit 1
exit 0
