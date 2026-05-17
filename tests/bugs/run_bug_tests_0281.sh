#!/bin/bash
# Nitpick Bug Regression Tests — v0.28.1 (JIT end-to-end smoke)
#
# Proves the wildx lifecycle (alloc -> seal -> call -> free) closes
# the loop with hand-assembled x86-64 machine code. The two helpers
# `npk_jit_install_add_i32` and `npk_jit_call_i32_i32` live in
# src/runtime/assembler/jit_smoke.cpp.
#
#   bug264 — JIT add(7, 35) via wildx + hand-assembled bytes,
#            asserts result == 42 and all rc == 0.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0281.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

# x86-64 only (JIT-DEC-002).
ARCH="$(uname -m)"
if [ "$ARCH" != "x86_64" ] && [ "$ARCH" != "amd64" ]; then
    echo "SKIP: v0.28.1 JIT smoke is x86-64 only (host: $ARCH)"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_exit_code() {
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

echo "== v0.28.1 JIT end-to-end smoke =="

expect_exit_code "bug264 jit add(7,35) via wildx == 42" \
    "$SCRIPT_DIR/bug264_jit_add_i32_smoke.npk" \
    "0"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
