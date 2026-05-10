#!/bin/bash
# Nitpick Bug Regression Tests — v0.22.2
# POLISH-001: eprint/eprintln builtins (bug109)
# POLISH-002: .aria file extension accepted by module resolver (bug110)
# POLISH-010: struct type identity across multi-module imports (bug111)
#
# bug109: eprint/eprintln must compile and write to stderr
# bug110: 'use "./mod.aria".*' must resolve correctly
# bug111: pub func with imported-struct param must compile without "Foo but expects Foo"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
NPKC="${REPO_ROOT}/build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0222_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# expect_compile_run: file must compile AND run with exit 0
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

# expect_compile_stderr: file must compile AND write non-empty output to stderr
expect_compile_stderr() {
    local label="$1"
    local file="$2"
    local out
    out=$(cd "$REPO_ROOT" && "$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local compile_rc=$?
    if [ $compile_rc -ne 0 ]; then
        fail "$label" "compile failed (exit $compile_rc): $out"
        rm -f "$TMP_BIN"; return
    fi
    local stderr_out
    stderr_out=$("$TMP_BIN" 2>&1 >/dev/null)
    local run_rc=$?
    rm -f "$TMP_BIN"
    if [ $run_rc -ne 0 ]; then
        fail "$label" "program exited $run_rc"
        return
    fi
    if [ -z "$stderr_out" ]; then
        fail "$label" "eprint/eprintln produced no stderr output"
    else
        pass "$label"
    fi
}

echo "=== Bug Regression Tests (v0.22.2: bug109–bug111) ==="
echo ""

# POLISH-001: eprint and eprintln write to stderr
expect_compile_stderr \
    "bug109: POLISH-001 — eprint/eprintln compile and write to stderr" \
    "tests/bugs/bug109_eprint_eprintln.npk"

# POLISH-002: .aria extension accepted by module resolver
expect_compile_run \
    "bug110: POLISH-002 — 'use .aria module' resolves correctly" \
    "tests/bugs/bug110_aria_extension.npk"

# POLISH-010: struct type identity across multi-module imports
expect_compile_run \
    "bug111: POLISH-010 — struct param type from imported module matches at call site" \
    "tests/bugs/bug111_struct_type_identity_multimod.npk"

echo ""
echo "=== Bug Tests (v0.22.2): $PASS/$TOTAL passed ==="

[ "$FAIL" -gt 0 ] && exit 1
exit 0
