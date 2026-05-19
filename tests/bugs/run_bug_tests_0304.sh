#!/bin/bash
# Nitpick Bug Regression Tests — v0.30.4
# Declarative `#[destroys_arena(<param>)]` attribute (IPC-DEC-005).
# The attribute lets extern (and self-hosted) declarations declare
# that calling them transfers ownership of one or more arena-typed
# parameters to the callee, even when no body is available for the
# borrow checker to scan.
#
#   bug322 — direct call to annotated extern → hard ARIA-032 error.
#   bug323 — wrapper around annotated extern → soft ARIA-032 warning
#            (transitive case, per IPC-DEC-004 one-cycle migration).
#   bug324 — attribute referencing an unknown param name is a no-op
#            (no phantom destroy, no ARIA-032).
#   bug325 — body-scan + attribute set union at consumer site is
#            idempotent (exactly one ARIA-032, no duplicate).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0304.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# Expect: compile FAILS with exactly $want_count occurrences of
# `$want_msg` (case-insensitive) in stderr.
expect_compile_error_count() {
    local label="$1"
    local file="$2"
    local want_msg="$3"
    local want_count="$4"

    rm -f "$TMP_EXE"
    local out rc count
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    if [ $rc -eq 0 ] && [ -x "$TMP_EXE" ]; then
        fail "$label" "expected compile failure, got success. Output: $out"
        rm -f "$TMP_EXE"
        return
    fi
    count=$(echo "$out" | grep -ci "$want_msg")
    if [ "$count" != "$want_count" ]; then
        fail "$label" "expected $want_count occurrence(s) of '$want_msg', got $count. Output: $out"
        rm -f "$TMP_EXE"
        return
    fi
    rm -f "$TMP_EXE"
    pass "$label"
}

# Expect: clean compile, executable produced, output contains exactly
# `$want_count` occurrences of `$want_msg` (case-insensitive).
expect_compile_warn_count() {
    local label="$1"
    local file="$2"
    local want_msg="$3"
    local want_count="$4"

    rm -f "$TMP_EXE"
    local out rc count
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    if [ $rc -ne 0 ] || [ ! -x "$TMP_EXE" ]; then
        fail "$label" "expected clean compile (warning only) but got exit $rc: $out"
        rm -f "$TMP_EXE"
        return
    fi
    count=$(echo "$out" | grep -ci "$want_msg")
    if [ "$count" != "$want_count" ]; then
        fail "$label" "expected $want_count occurrence(s) of '$want_msg', got $count. Output: $out"
        rm -f "$TMP_EXE"
        return
    fi
    rm -f "$TMP_EXE"
    pass "$label"
}

echo "== v0.30.4 #[destroys_arena] attribute =="

expect_compile_error_count "bug322 direct annotated extern call -> hard ARIA-032 error" \
    "$SCRIPT_DIR/bug322_handle_destroys_arena_attribute_errors.npk" \
    "\[ARIA-032\]" 1

expect_compile_warn_count "bug323 wrapper around annotated extern -> soft ARIA-032 warning" \
    "$SCRIPT_DIR/bug323_handle_destroys_arena_wrapper_warns.npk" \
    "\[ARIA-032\]" 1

expect_compile_warn_count "bug324 attribute with unknown param name -> no-op, no ARIA-032" \
    "$SCRIPT_DIR/bug324_handle_destroys_arena_unknown_param_noop.npk" \
    "\[ARIA-032\]" 0

expect_compile_error_count "bug325 body-scan + attribute union -> exactly one ARIA-032" \
    "$SCRIPT_DIR/bug325_handle_destroys_arena_union_no_duplicate.npk" \
    "\[ARIA-032\]" 1

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
