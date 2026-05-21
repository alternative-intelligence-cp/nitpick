#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.2.10 D-23.
#
# D-23: pick exhaustiveness for special values.
#
# Extends the v0.21.3 TBB-ERR pattern to Optional<T> (NIL), Pointer<T>
# (NULL), and may-be-unknown-tainted slots (`unknown`). Picks on these
# selector types must cover the special-value arm, OR include a
# wildcard (*).
#
# Fixtures:
#   bug418 — pick on tbb32 missing ERR              → expect "Missing cases: ERR"
#   bug419 — pick on int32? missing NIL             → expect "Missing cases: NIL"
#   bug420 — pick on int32-> missing NULL           → expect "Missing cases: NULL"
#   bug421 — pick on unknown-tainted missing unknown → expect "Missing cases: unknown"
#   bug422 — pick on int32? with NIL + wildcard     → exit 0 (positive path)
#
# See: META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.2.0.md (D-23)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03130.exe"

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

echo "== v0.31.2.10 D-23 — pick exhaustiveness for special values =="

expect_compile_fail "bug418 pick tbb32 missing ERR" \
    "$SCRIPT_DIR/bug418_pick_tbb_missing_err_fail.npk" \
    "Missing cases: ERR"

expect_compile_fail "bug419 pick Optional<T> missing NIL" \
    "$SCRIPT_DIR/bug419_pick_optional_missing_nil_fail.npk" \
    "Missing cases: NIL"

expect_compile_fail "bug420 pick Pointer<T> missing NULL" \
    "$SCRIPT_DIR/bug420_pick_pointer_missing_null_fail.npk" \
    "Missing cases: NULL"

expect_compile_fail "bug421 pick unknown-tainted missing unknown" \
    "$SCRIPT_DIR/bug421_pick_unknown_tainted_missing_unknown_fail.npk" \
    "Missing cases: unknown"

expect_compile_run "bug422 pick Optional<T> with NIL + wildcard → exit 0" \
    "$SCRIPT_DIR/bug422_pick_optional_nil_and_wildcard_pass.npk" "0"

echo ""
echo "== Summary =="
echo "  Passed: $PASS / $TOTAL"
echo "  Failed: $FAIL / $TOTAL"

[ "$FAIL" = "0" ] && exit 0 || exit 1
