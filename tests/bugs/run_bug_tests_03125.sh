#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.2.5 Phase 3 D-18 sub-slice.
#
# D-18: `<expr> is unknown` (preferred) and `<expr> == unknown` (accepted
# for symmetry with == NIL / == NULL) test for the unknown sentinel.
# Both forms desugar to the same sentinel comparison and produce a bool.
# The prefix ternary form `is (cond) : t : f` must continue to parse.
#
# Fixtures:
#   bug398 — `x is unknown` true when x = unknown (positive).
#   bug399 — `y is unknown` false for a normal value (positive).
#   bug400 — `is unknown` and `== unknown` agree (positive).
#   bug401 — prefix `is (cond) : t : f` ternary regression guard.
#
# See: META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.2.0.md (D-18)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03125.exe"

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

echo "== v0.31.2.5 D-18 — \`is unknown\` test form =="

expect_compile_run "bug398 x is unknown (true)" \
    "$SCRIPT_DIR/bug398_is_unknown_true.npk" "0"

expect_compile_run "bug399 y is unknown (false)" \
    "$SCRIPT_DIR/bug399_is_unknown_false.npk" "0"

expect_compile_run "bug400 is unknown / == unknown agree" \
    "$SCRIPT_DIR/bug400_is_unknown_eq_agree.npk" "0"

expect_compile_run "bug401 prefix is(...) ternary regression" \
    "$SCRIPT_DIR/bug401_is_ternary_regression.npk" "0"

echo ""
echo "== Summary =="
echo "  Passed: $PASS / $TOTAL"
echo "  Failed: $FAIL / $TOTAL"

[ "$FAIL" = "0" ] && exit 0 || exit 1
