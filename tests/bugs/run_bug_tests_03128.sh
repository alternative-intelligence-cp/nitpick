#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.2.8 D-20 / ARIA-046.
#
# D-20: when `fail(code)` runs in a Result<T>-returning function, the
# generated Result has `is_error=true`, `error=code`, and `value=NIL`
# (i.e. zero-filled). Reading `.value` after failure is GATED at sema
# for primitive T (existing "no checky no val" rule), but UNGATED when
# T is `NIL` (single inhabitant) or `Optional<U>` (the zero-filled
# value field naturally encodes None — hasValue=0).
#
# Today's situation (v0.31.2.7):
#   - Optional codegen: `{ i1 hasValue, T value }` — getNullValue zeros
#     hasValue → None. So fail() on a Result<Optional<T>>-returning
#     function already produces a well-typed None value field. No
#     codegen change needed.
#   - NIL: `%struct.NIL = type {}` — empty struct, single inhabitant.
#     getNullValue gives the only NIL value. No codegen change needed.
#   - Sema: the existing gate rejected `.value` reads on Result<NIL>
#     and Result<Optional<T>> too, which D-20 says it shouldn't. The
#     gate was relaxed in v0.31.2.8 (type_checker_expr.cpp) to exempt
#     these two value-field shapes.
#
# Fixtures:
#   bug410 — Result<NIL>.value unguarded ; exits 0                     (positive)
#   bug411 — Result<int32?>.value after fail() observed as NIL/None ; exits 0  (positive)
#   bug412 — Result<int32>.value unguarded — compiler must reject ("Cannot access .value")
#   bug413 — Result<int32?>.value after pass(42) observed as Some(42) ; exits 42  (positive)
#
# See: META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.2.0.md (D-20)
#
# Note (DEF-CHAIN-RESULT): chained `expr().is_error` or `expr().value`
# directly off a call segfaults at runtime — these fixtures use the
# intermediate-binding form `Result<T>:r = call(); ... r.value ...`.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03128.exe"

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

echo "== v0.31.2.8 D-20 / ARIA-046 — fail ⇒ .value is NIL, sema gate relaxed for NIL/Optional =="

expect_compile_run "bug410 Result<NIL>.value unguarded" \
    "$SCRIPT_DIR/bug410_result_nil_value_unguarded.npk" "0"

expect_compile_run "bug411 Result<int32?>.value after fail observed as NIL/None" \
    "$SCRIPT_DIR/bug411_result_optional_value_failed_is_none.npk" "0"

expect_compile_fail "bug412 Result<int32>.value unguarded rejected by no-checky-no-val" \
    "$SCRIPT_DIR/bug412_result_int_value_unguarded_rejected.npk" \
    "Cannot access .value"

expect_compile_run "bug413 Result<int32?>.value after pass observed as Some(42)" \
    "$SCRIPT_DIR/bug413_result_optional_value_success_is_some.npk" "42"

echo ""
echo "== Summary =="
echo "  Passed: $PASS / $TOTAL"
echo "  Failed: $FAIL / $TOTAL"

[ "$FAIL" = "0" ] && exit 0 || exit 1
