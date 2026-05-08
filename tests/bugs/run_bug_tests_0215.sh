#!/bin/bash
# Nitpick Bug Regression Tests — v0.21.5
# Runs bugs/bug103–bug104 (A-011 ahset auto-grow regression, A-012 pub→pub
# chain regression).
# Files ending _pass.npk must compile and exit 0 when run.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
NPKC="${REPO_ROOT}/build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0215_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# expect_compile_run: file should compile AND produce exit 0 when run
# Run from REPO_ROOT so relative `use "tests/bugs/..."` paths resolve.
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

echo "=== Bug Regression Tests (v0.21.5: bug103–bug104) ==="
echo ""

# A-011: ahset must auto-grow past initial capacity (B7 fix carried forward)
expect_compile_run \
    "bug103: ahset auto-grow past initial capacity (A-011)" \
    "tests/bugs/bug103_ahset_autogrow_pass.npk"

# A-012: 4-level pub→pub→pub cross-module chain compiles + runs (no GC OOM)
expect_compile_run \
    "bug104: pub→pub→pub→pub cross-module chain (A-012 floor)" \
    "tests/bugs/bug104_pub_pub_chain_pass.npk"

echo ""
echo "=== Bug Tests (v0.21.5): $PASS/$TOTAL passed ==="

[ "$FAIL" -gt 0 ] && exit 1
exit 0
