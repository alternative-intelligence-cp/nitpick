#!/bin/bash
# Nitpick Bug Regression Tests — v0.29.1 DROP-DEC-001
#
#   bug279 — impl:Drop:for:Counter parses + sema-checks; mangled
#            Counter_drop callable explicitly (no auto-call yet).
#   bug280 — duplicate impl:Drop:for:Counter rejected.
#   bug281 — impl:Drop:for:UnknownType rejected.
#   bug282 — method inside impl:Drop must be named 'drop' (not
#            'destroy', 'free', etc.).
#   bug283 — drop method must return NIL (not int32 or anything else).
#   bug284 — DROP-DEC-007a regression: existing `drop <expr>` unary
#            form still parses + runs alongside an impl:Drop method.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0291.exe"

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

echo "== v0.29.1 DROP-DEC-001 — Drop trait parser+sema surface =="

expect_exit_code "bug279 impl:Drop:for:Counter parses + sema + explicit call" \
    "$SCRIPT_DIR/bug279_drop_impl_basic_pass.npk" \
    "0"

expect_compile_fail "bug280 duplicate impl:Drop:for:Counter rejected" \
    "$SCRIPT_DIR/bug280_drop_impl_duplicate_rejected.npk" \
    "Duplicate impl:Drop"

expect_compile_fail "bug281 impl:Drop:for:Nonexistent rejected" \
    "$SCRIPT_DIR/bug281_drop_impl_unknown_type_rejected.npk" \
    "is not defined"

expect_compile_fail "bug282 drop method must be named 'drop'" \
    "$SCRIPT_DIR/bug282_drop_impl_wrong_method_name_rejected.npk" \
    "must be named 'drop'"

expect_compile_fail "bug283 drop method must return NIL" \
    "$SCRIPT_DIR/bug283_drop_impl_wrong_return_type_rejected.npk" \
    "must return NIL"

expect_exit_code "bug284 'drop <expr>' unary form still works (DROP-DEC-007a regression)" \
    "$SCRIPT_DIR/bug284_drop_keyword_expr_still_works_pass.npk" \
    "0"

echo ""
echo "v0.29.1 results: $PASS/$TOTAL passed, $FAIL failed"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
