#!/bin/bash
# Nitpick Bug Regression Tests — v0.30.6
# ARIA-032 precision pass — drop false-positive corner cases that
# v0.30.1–5 surfaced in the per-name destroyed-arena tracker.
#
#   bug330 — destroy `a`, allocate fresh `b`, deref(h) backed by `b`.
#            Regression-pin for per-name partitioning (passed pre-fix).
#   bug331 — destroy `a`, REASSIGN `a` to a fresh HandleArena.create(),
#            deref(h) backed by the reassigned `a`. Fixed in v0.30.6
#            by clearing the destroyed marker on RHS-is-create
#            reassignment (borrow_checker.cpp checkAssignment).
#   bug332 — destroy `a` inside an exiting branch (`exit 0;`), deref
#            after the branch (only reached when the branch was NOT
#            taken). Fixed in v0.30.6 by snapshot/restore of the
#            destroyed sets across terminating IF branches
#            (borrow_checker.cpp checkIfStmt).
#   bug333 — transitive-destroy AFTER all uses. Regression-pin
#            (passed pre-fix), guards the unchanged transitive path.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0306.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# Expect: clean compile, executable produced.
expect_compile_ok() {
    local label="$1"
    local file="$2"
    rm -f "$TMP_EXE"
    local out rc
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    if [ $rc -ne 0 ] || [ ! -x "$TMP_EXE" ]; then
        fail "$label" "expected clean compile, exit $rc: $out"
        rm -f "$TMP_EXE"; return
    fi
    rm -f "$TMP_EXE"
    pass "$label"
}

echo "== v0.30.6 ARIA-032 precision pass =="

expect_compile_ok "bug330 destroy a, allocate fresh b, deref(h@b)" \
    "$SCRIPT_DIR/bug330_handle_destroy_recreate_new_name_pass.npk"

expect_compile_ok "bug331 destroy a, REASSIGN a = create(), deref(h@a')" \
    "$SCRIPT_DIR/bug331_handle_destroy_reassign_same_name_pass.npk"

expect_compile_ok "bug332 destroy a in exiting branch, deref after" \
    "$SCRIPT_DIR/bug332_handle_destroy_in_exiting_branch_pass.npk"

expect_compile_ok "bug333 transitive destroy AFTER all handle uses" \
    "$SCRIPT_DIR/bug333_handle_transitive_destroy_no_use_after_pass.npk"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
