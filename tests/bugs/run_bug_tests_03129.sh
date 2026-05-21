#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.2.9 D-22.
#
# D-22: `Type:Drop` + `fixed` — Drop still fires.
#
# A `fixed` binding marks the binding as non-reassignable, but the
# auto-generated scope-end Drop call (introduced in v0.29.x) is
# unaffected by binding mutability. A `fixed` binding of a Drop-
# implementing type is still destroyed at scope end, in LIFO order
# with respect to other Drop bindings in the same scope.
#
# Verification slice — no production code changes required for
# v0.31.2.9. The fixtures below lock in the behaviour so it cannot
# silently regress.
#
# Fixtures:
#   bug414 — fixed Counter:c in inner block → exit 0 + IR contains @Counter_drop
#   bug415 — reassignment to fixed binding rejected ("Cannot reassign fixed variable")
#   bug416 — fixed binding dropped at inner-block exit; outer continues
#   bug417 — two fixed bindings in same scope → IR contains TWO @Counter_drop calls
#
# See: META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.2.0.md (D-22)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03129.exe"
TMP_LL="/tmp/npk_bug_test_03129.ll"

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

# Like expect_compile_run, but also asserts the emitted LLVM IR contains
# exactly $4 calls to @<funcname> ($5).
expect_run_and_ir_call_count() {
    local label="$1"; local file="$2"; local expected="$3"
    local count="$4"; local needle="$5"
    rm -f "$TMP_EXE" "$TMP_LL"
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
    # Re-emit IR alongside binary build
    local llname="${file%.npk}.ll"
    rm -f "$llname"
    "$NPKC" --emit-llvm "$file" 2>/dev/null
    if [ ! -f "$llname" ]; then
        fail "$label" "no .ll emitted for $file"
        return 1
    fi
    local got
    got=$(grep -c "call .* @${needle}(" "$llname")
    if [ "$got" != "$count" ]; then
        fail "$label" "expected $count call(s) to @$needle in IR but got $got"
        rm -f "$llname"
        return 1
    fi
    rm -f "$llname"
    pass "$label"
}

echo "== v0.31.2.9 D-22 — fixed × Drop: Drop still fires =="

expect_run_and_ir_call_count "bug414 fixed Counter inner block → exit 0 + 1×Counter_drop" \
    "$SCRIPT_DIR/bug414_fixed_drop_basic_pass.npk" "0" "1" "Counter_drop"

expect_compile_fail "bug415 reassignment to fixed binding rejected" \
    "$SCRIPT_DIR/bug415_fixed_drop_reassign_rejected.npk" \
    "Cannot reassign fixed variable"

expect_compile_run "bug416 fixed binding dropped at inner-block exit; outer continues" \
    "$SCRIPT_DIR/bug416_fixed_drop_inner_block_pass.npk" "0"

expect_run_and_ir_call_count "bug417 two fixed bindings → 2×Counter_drop" \
    "$SCRIPT_DIR/bug417_fixed_drop_lifo_pass.npk" "0" "2" "Counter_drop"

echo ""
echo "== Summary =="
echo "  Passed: $PASS / $TOTAL"
echo "  Failed: $FAIL / $TOTAL"

[ "$FAIL" = "0" ] && exit 0 || exit 1
