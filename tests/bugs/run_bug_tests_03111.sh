#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.1.1 Phase 2 D-6 correction.
#
# This slice corrects a META-only error in the v0.31.1.0 Phase 2
# kickoff audit: D-6 (drop auto-dispatch at scope end) was listed as
# pending code work, but had in fact been complete since v0.29.2
# (DROP-DEC-001..010). See META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.1.md.
#
# This runner validates a single new fixture that closes the only
# verification gap the v0.31.1.0 audit named: interaction between an
# in-scope Drop local and the `?` (UNWRAP) expression operator.
#
#   bug357 — Drop local lives across a `?` expression; the scope-end
#            drop fires exactly once. IR contains `@Trace_drop`
#            exactly one time inside `main` (not zero, not two).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03111.exe"
TMP_LL="/tmp/npk_bug_test_03111.ll"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

compile_run_ir() {
    local label="$1"; local file="$2"; local expected="$3"
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
    "$NPKC" "$file" --emit-llvm -o "$TMP_LL" >/dev/null 2>&1
    if [ ! -s "$TMP_LL" ]; then
        local alt="${file%.npk}.ll"
        if [ -s "$alt" ]; then cp "$alt" "$TMP_LL"; fi
    fi
    return 0
}

echo "== v0.31.1.1 D-6 correction — `?`-with-Drop interaction =="

if compile_run_ir "bug357 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug357_drop_across_unwrap_pass.npk" "0"; then
    pass "bug357 compile+run (exit 0)"
    # Count drop call sites inside the IR. Exactly one expected.
    count=$(grep -cE 'call[^@]*@Trace_drop\b' "$TMP_LL" 2>/dev/null)
    if [ "$count" = "1" ]; then
        pass "bug357 IR contains exactly one @Trace_drop call (count=$count)"
    else
        fail "bug357 IR contains exactly one @Trace_drop call" \
             "expected 1 @Trace_drop call, found $count"
    fi
fi

rm -f "$TMP_EXE" "$TMP_LL"

echo ""
echo "v0.31.1.1 results: $PASS/$TOTAL passed, $FAIL failed"

if [ $FAIL -eq 0 ]; then exit 0; else exit 1; fi
