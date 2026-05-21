#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.2.6 Phase 3 D-19 sub-slice.
#
# D-19: immutability is a binding property, not a type property.
# `fixed T:x = ...;` is legal for any T (including generic parameters);
# reassignment is rejected regardless of T's concrete type at
# monomorphisation.
#
# Before this slice, cloneAST in src/frontend/sema/generic_resolver.cpp
# dropped the `isFixed` (and the borrow) markers when cloning the AST
# for a specialization, AND the specialised body was never type-checked,
# so reassigning a `fixed *T:slot` inside a generic body silently
# compiled. The slice:
#   1. preserves isFixed / isBorrowImm / isBorrowMut through cloneAST,
#   2. adds TypeChecker::checkFixedReassignInSpecialized which scans
#      the specialised body for fixed-binding reassignments.
#
# Fixtures:
#   bug402 — fixed *T:slot, T = int32, exit 0   (positive)
#   bug403 — slot = b reassignment, T = int32   (negative — compile error)
#   bug404 — slot = b reassignment, T = Box     (negative — compile error)
#   bug405 — fixed *T:slot, T = Box, exit 7     (positive)
#
# See: META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.2.0.md (D-19)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03126.exe"

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

expect_compile_fail() {
    local label="$1"; local file="$2"; local needle="$3"
    rm -f "$TMP_EXE"
    local cerr
    cerr=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    if [ -x "$TMP_EXE" ]; then
        fail "$label" "expected compile failure but produced executable"
        return 1
    fi
    if ! echo "$cerr" | grep -q -- "$needle"; then
        fail "$label" "expected diagnostic containing '$needle' — got: $cerr"
        return 1
    fi
    pass "$label"
}

echo "== v0.31.2.6 D-19 — fixed × generic monomorphisation =="

expect_compile_run "bug402 fixed *T (T=int32) positive" \
    "$SCRIPT_DIR/bug402_fixed_generic_primitive_pass.npk" "0"

expect_compile_fail "bug403 fixed *T slot=b (T=int32) rejected" \
    "$SCRIPT_DIR/bug403_fixed_generic_primitive_reassign_fail.npk" \
    "Cannot reassign fixed variable 'slot'"

expect_compile_fail "bug404 fixed *T slot=b (T=Box) rejected" \
    "$SCRIPT_DIR/bug404_fixed_generic_struct_reassign_fail.npk" \
    "Cannot reassign fixed variable 'slot'"

expect_compile_run "bug405 fixed *T (T=Box) positive" \
    "$SCRIPT_DIR/bug405_fixed_generic_struct_pass.npk" "7"

echo ""
echo "== Summary =="
echo "  Passed: $PASS / $TOTAL"
echo "  Failed: $FAIL / $TOTAL"

[ "$FAIL" = "0" ] && exit 0 || exit 1
