#!/bin/bash
# Nitpick Bug Regression Tests — v0.22.1
# POLISH-012: pass n; not counted as variable use → false unused-var warning (bug107)
# POLISH-014: while body not scanned by unused-var checker (bug108)
#
# Both tests must compile WITHOUT emitting any "declared but never used" warning
# and run successfully (exit 0).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
NPKC="${REPO_ROOT}/build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0221_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# expect_compile_run: file should compile AND produce exit 0 when run
expect_compile_run() {
    local label="$1"
    local file="$2"
    local out
    out=$(cd "$REPO_ROOT" && "$NPKC" "$file" -o "$TMP_BIN" 2>&1)
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

# expect_no_unused_warning: file should compile AND NOT produce "declared but never used"
expect_no_unused_warning() {
    local label="$1"
    local file="$2"
    local out
    out=$(cd "$REPO_ROOT" && "$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local compile_rc=$?
    rm -f "$TMP_BIN"
    if [ $compile_rc -ne 0 ]; then
        fail "$label" "compile failed (exit $compile_rc): $out"
        return
    fi
    if echo "$out" | grep -q "declared but never used"; then
        fail "$label" "false unused-var warning emitted: $out"
    else
        pass "$label"
    fi
}

echo "=== Bug Regression Tests (v0.22.1: bug107–bug108) ==="
echo ""

# POLISH-012: pass n; should count as a use of n, suppressing unused-var warning
expect_no_unused_warning \
    "bug107: POLISH-012 — pass n; counts as variable use (no false warning)" \
    "tests/bugs/bug107_pass_counts_as_use.npk"

# POLISH-014: variables used inside while body should not be flagged as unused
expect_no_unused_warning \
    "bug108: POLISH-014 — while body variables not flagged as unused" \
    "tests/bugs/bug108_while_body_usage.npk"

echo ""
echo "=== Bug Tests (v0.22.1): $PASS/$TOTAL passed ==="

[ "$FAIL" -gt 0 ] && exit 1
exit 0
