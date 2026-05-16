#!/bin/bash
# Nitpick Bug Regression Tests — v0.27.5 (WildX W^X lifecycle hardening)
#
# Adds surface builtins `wildx_alloc`, `wildx_seal`, `wildx_free` that
# route through a process-wide registry mapping the writable pointer
# back to its WildXGuard. Tests assert the runtime enforces the W^X
# state machine even when the surface language only sees raw int8@
# pointers:
#
#   bug245 — full alloc -> seal -> free round-trip succeeds.
#   bug246 — second wildx_seal on the same page returns -1
#            (npk_mem_protect_exec only accepts WRITABLE -> EXECUTABLE).
#   bug247 — double `wildx_free` on the same binding is statically
#            rejected by the borrow checker (ARIA-022 double-free).
#   bug248 — `wildx int8->:p = wildx_alloc(N);` without cleanup is
#            statically rejected by the wild-leak checker.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0275.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
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

expect_compile_fail() {
    local label="$1"
    local file="$2"
    local expect_msg="$3"

    rm -f "$TMP_EXE"
    local out rc
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    rm -f "$TMP_EXE"
    if [ $rc -eq 0 ]; then
        fail "$label" "expected compile failure but got exit 0"
        return
    fi
    if [ -n "$expect_msg" ] && ! echo "$out" | grep -qi "$expect_msg"; then
        fail "$label" "compile failed (good) but expected message '$expect_msg' not in output: $out"
        return
    fi
    pass "$label"
}

echo "== v0.27.5 WildX W^X lifecycle =="

expect_exit_code "bug245 wildx alloc/seal/free round-trip" \
    "$SCRIPT_DIR/bug245_wildx_lifecycle_pass.npk" \
    "0"

expect_exit_code "bug246 wildx double-seal rejected" \
    "$SCRIPT_DIR/bug246_wildx_double_seal_rejected.npk" \
    "0"

expect_compile_fail "bug247 wildx double-free statically rejected" \
    "$SCRIPT_DIR/bug247_wildx_double_free_rejected.npk" \
    "ARIA-022\|double free\|already freed"

expect_compile_fail "bug248 wildx leak statically rejected" \
    "$SCRIPT_DIR/bug248_wildx_leak_rejected.npk" \
    "leak\|never freed\|wild"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
