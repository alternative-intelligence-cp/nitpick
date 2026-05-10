#!/bin/bash
# Nitpick Bug Regression Tests — v0.22.4
# POLISH-006: pick on integer values (bug114 int32, bug115 int64)
# POLISH-007: bitwise operators on variables (bug116 int32+uint8)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
NPKC="${REPO_ROOT}/build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0224_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

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

echo "=== Bug Regression Tests (v0.22.4: bug114–bug116) ==="
echo ""

# POLISH-006: pick on int32
expect_compile_run \
    "bug114: POLISH-006 — pick on int32 values (arm match + wildcard)" \
    "tests/bugs/bug114_pick_int32.npk"

# POLISH-006: pick on int64
expect_compile_run \
    "bug115: POLISH-006 — pick on int64 values (large literal arm + wildcard)" \
    "tests/bugs/bug115_pick_int64.npk"

# POLISH-007: bitwise on int32 + uint8 variables
expect_compile_run \
    "bug116: POLISH-007 — bitwise &|^>><<  on int32 and uint8 variables" \
    "tests/bugs/bug116_bitwise_vars.npk"

echo ""
echo "=== Bug Tests (v0.22.4): $PASS/$TOTAL passed ==="

[ "$FAIL" -gt 0 ] && exit 1
exit 0
