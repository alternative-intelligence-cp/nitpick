#!/bin/bash
# Nitpick Bug Regression Tests — v0.21.6
# Runs bugs/bug105–bug106 (A-009 tbb8/16/64 K formalization regression,
# A-010 async/await K formalization regression).
# Files ending _pass.npk must compile and exit 0 when run.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
NPKC="${REPO_ROOT}/build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0216_bin"

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

echo "=== Bug Regression Tests (v0.21.6: bug105–bug106) ==="
echo ""

# A-009: tbb8/tbb16/tbb64 arithmetic and ERR sentinel (K formalized in v0.21.6)
expect_compile_run \
    "bug105: tbb8/tbb16/tbb64 arithmetic and ERR sentinel (A-009)" \
    "tests/bugs/bug105_tbb_variants_pass.npk"

# A-010: async func: and await compile without error (K formalized in v0.21.6)
expect_compile_run \
    "bug106: async func: and await compilation smoke test (A-010)" \
    "tests/bugs/bug106_async_await_pass.npk"

echo ""
echo "=== Bug Tests (v0.21.6): $PASS/$TOTAL passed ==="

[ "$FAIL" -gt 0 ] && exit 1
exit 0
