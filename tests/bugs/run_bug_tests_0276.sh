#!/bin/bash
# Nitpick Bug Regression Tests — v0.27.6 (wild ↔ borrow checker polish)
#
# Codifies that the borrow checker is NOT silently bypassed for wild
# bindings — the v0.27.1/v0.27.2 region propagation already treats a
# `$$i wild T:b = wild_buf;` as an Aria-visible borrow that obeys
# mut-XOR-aliasing. PLAN option (b): wild gets the borrow checker for
# the part of the program written in Aria; FFI access through the raw
# pointer remains the user's responsibility.
#
#   bug249 — `$$i` borrow on a wild binding compiles and runs (exit 0).
#   bug250 — two overlapping `$$m` borrows of the same wild buffer are
#            statically rejected (ARIA-023).
#   bug251 — passing a wild buffer to a `$$i`/`$$m` parameter, then
#            free()-ing after the borrow drops, still works.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0276.exe"

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

echo "== v0.27.6 wild ↔ borrow checker polish =="

expect_exit_code "bug249 \$\$i borrow on wild compiles" \
    "$SCRIPT_DIR/bug249_wild_borrow_pass.npk" \
    "0"

expect_compile_fail "bug250 wild mut-XOR-aliasing rejected" \
    "$SCRIPT_DIR/bug250_wild_mut_xor_rejected.npk" \
    "ARIA-023"

expect_exit_code "bug251 wild FFI passthrough still works" \
    "$SCRIPT_DIR/bug251_wild_ffi_passthrough_pass.npk" \
    "0"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
