#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.2.2 Phase 3 D-15 sub-slice 2.
#
# D-15 extends the existing NIL/NULL type-safety enforcement (already
# present in `checkVarDecl` for var-decl forms) to cover plain `=`
# assignment statements. Aria parses `x = NIL;` as a BinaryExpr with
# TOKEN_EQUAL; the new guard lives in `checkBinaryOperator`
# (src/frontend/sema/type_checker.cpp).
#
# Fixtures:
#   bug383 — NIL → non-optional via assignment rejected (negative).
#   bug384 — NULL → non-pointer via assignment rejected (negative).
#   bug385 — NIL → optional via assignment accepted (positive).
#   bug386 — NULL → pointer via assignment accepted (positive).
#
# See:
#   META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.2.0.md (D-15)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03122.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_compile_run() {
    local label="$1"; local file="$2"; local expected="$3"
    rm -f "$TMP_EXE"
    local cerr
    cerr=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    if [ ! -x "$TMP_EXE" ]; then
        fail "$label" "compile failed: $cerr"
        return 1
    fi
    "$TMP_EXE" 2>/dev/null
    local actual=$?
    if [ "$actual" != "$expected" ]; then
        fail "$label" "expected exit $expected but got $actual"
        return 1
    fi
    pass "$label"
}

expect_compile_fail_with() {
    local label="$1"; local file="$2"; local needle="$3"
    rm -f "$TMP_EXE"
    local out
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    local rc=$?
    if [ -x "$TMP_EXE" ] && [ "$rc" = "0" ]; then
        fail "$label" "expected compile failure, but compile succeeded"
        return 1
    fi
    if ! echo "$out" | grep -q "$needle"; then
        fail "$label" "expected diagnostic '$needle' not found in: $out"
        return 1
    fi
    pass "$label"
}

echo "== v0.31.2.2 D-15 sub-slice 2 — NIL/NULL assignment-statement coverage =="

expect_compile_fail_with "bug383 NIL → non-optional (assign)" \
    "$SCRIPT_DIR/bug383_assign_nil_to_nonoptional_fail.npk" \
    "NIL can only be assigned to optional types"

expect_compile_fail_with "bug384 NULL → non-pointer (assign)" \
    "$SCRIPT_DIR/bug384_assign_null_to_nonpointer_fail.npk" \
    "NULL can only be assigned to pointer types"

expect_compile_run "bug385 NIL → optional (assign, exit 0)" \
    "$SCRIPT_DIR/bug385_assign_nil_to_optional_pass.npk" "0"

expect_compile_run "bug386 NULL → pointer (assign, exit 0)" \
    "$SCRIPT_DIR/bug386_assign_null_to_pointer_pass.npk" "0"

echo
echo "Summary: $PASS/$TOTAL passed, $FAIL failed"
if [ "$FAIL" -gt 0 ]; then exit 1; fi
exit 0
