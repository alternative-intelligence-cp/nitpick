#!/bin/bash
# Nitpick Bug Regression Tests — v0.25.3
# Runs bug184–bug189 (BORROW-005 deep struct paths, BORROW-006 ptr->field).
#   bug184: outer.m.mid.x disjoint from outer.m.mid.y (3-level path).
#   bug185: outer.m.mid vs outer.m.mid.x rejected (parent/child).
#   bug186: deep writeback through borrows reaches host.
#   bug187: ptr->x disjoint from ptr->y (same pointer, distinct fields).
#   bug188: ptr->x vs ptr->x rejected (ARIA-023).
#   bug189: p->x vs q->x conservatively rejected (cross-pointer aliasing).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0253_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_compile_run() {
    local label="$1"
    local file="$2"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
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

expect_compile_fail() {
    local label="$1"
    local file="$2"
    local expect_msg="$3"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local rc=$?
    rm -f "$TMP_BIN"
    if [ $rc -eq 0 ]; then
        fail "$label" "expected compile failure but got exit 0"
    elif [ -n "$expect_msg" ] && ! echo "$out" | grep -qi "$expect_msg"; then
        fail "$label" "compile failed (good) but expected msg '$expect_msg' not in: $out"
    else
        pass "$label"
    fi
}

echo "== v0.25.3 deep struct & pointer-field borrow regression tests =="

expect_compile_run "bug184 outer.m.mid.x disjoint from outer.m.mid.y" \
    "$SCRIPT_DIR/bug184_struct_3level_disjoint_pass.npk"

expect_compile_fail "bug185 outer.m.mid vs outer.m.mid.x rejected" \
    "$SCRIPT_DIR/bug185_struct_4level_parent_child_rejected.npk" \
    "ARIA-023"

expect_compile_run "bug186 deep struct writeback reaches host" \
    "$SCRIPT_DIR/bug186_struct_5level_writeback_pass.npk"

expect_compile_run "bug187 ptr->x disjoint from ptr->y" \
    "$SCRIPT_DIR/bug187_ptr_field_disjoint_pass.npk"

expect_compile_fail "bug188 ptr->x vs ptr->x rejected" \
    "$SCRIPT_DIR/bug188_ptr_field_parent_child_rejected.npk" \
    "ARIA-023"

expect_compile_fail "bug189 p->x vs q->x cross-pointer rejected" \
    "$SCRIPT_DIR/bug189_ptr_field_cross_alias_rejected.npk" \
    "ARIA-023"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
