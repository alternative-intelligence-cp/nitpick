#!/bin/bash
# Nitpick Bug Regression Tests — v0.28.2 (JIT helper API via stdlib/jit.npk)
#
# v0.28.1 closed the wildx W^X loop with hand-assembled bytes and raw
# `npk_jit_*` externs (bug264). v0.28.2 hides those behind a stdlib
# helper module `Type:Jit` exposing `compile_add_i32`, `call_i32_i32`,
# and `free`. User code never touches the externs or `wildx_*`
# builtins directly.
#
#   bug265 — happy-path round trip through `Type:Jit`.
#   bug266 — two independent JIT pages coexist in the wildx registry.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0282.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

# x86-64 only (JIT-DEC-002).
ARCH="$(uname -m)"
if [ "$ARCH" != "x86_64" ] && [ "$ARCH" != "amd64" ]; then
    echo "SKIP: v0.28.2 JIT helpers are x86-64 only (host: $ARCH)"
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

echo "== v0.28.2 JIT helper API =="

expect_exit_code "bug265 Jit.compile_add_i32 -> Jit.call_i32_i32 -> Jit.free" \
    "$SCRIPT_DIR/bug265_jit_helper_pass.npk" \
    "0"

expect_exit_code "bug266 two Jit pages coexist in wildx registry" \
    "$SCRIPT_DIR/bug266_jit_helper_two_pages.npk" \
    "0"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
