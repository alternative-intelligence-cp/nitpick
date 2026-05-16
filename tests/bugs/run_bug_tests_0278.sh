#!/bin/bash
# Nitpick Bug Regression Tests — v0.27.8 (Handle<T> typed surface)
#
#   bug256 — Handle<T> round-trips through HandleArena (alloc/deref/free).
#   bug257 — Handle<int32> ≠ Handle<int64> at the type level (compile-fail).
#   bug258 — use-after-free deref returns NULL (no UB / no stale pointer).
#   bug259 — destroying an arena invalidates outstanding handles to it.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0278.exe"

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

echo "== v0.27.8 Handle<T> typed surface =="

expect_exit_code "bug256 Handle<T> round-trip via HandleArena" \
    "$SCRIPT_DIR/bug256_handle_roundtrip_pass.npk" \
    "0"

expect_compile_fail "bug257 Handle<int32> ≠ Handle<int64>" \
    "$SCRIPT_DIR/bug257_handle_type_mismatch_rejected.npk" \
    "Handle<int"

expect_exit_code "bug258 UAF deref returns NULL" \
    "$SCRIPT_DIR/bug258_handle_uaf_returns_null.npk" \
    "0"

# NOTE (v0.27.9): bug259 originally validated the RUNTIME fallback that derefs
# of a stale handle return NULL after the arena was destroyed. Starting in
# v0.27.9 (ARIA-032), this pattern is rejected STATICALLY by the borrow
# checker, so the program no longer reaches runtime. The runtime fallback is
# still present as a defense in depth; the static rule shadows it.
expect_compile_fail "bug259 arena destroy invalidates handles (now static ARIA-032)" \
    "$SCRIPT_DIR/bug259_arena_destroy_invalidates.npk" \
    "ARIA-032"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
