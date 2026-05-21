#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.2.4 Phase 3 D-17/D-17a sub-slice.
#
# D-17: explicit `unknown` literal taints a binding's may-be-unknown flag.
# Reading that binding as a call argument or `pass` value without ok(...)
# emits ARIA-045. ok(x) is the explicit taint-stripper / acknowledgment.
# D-17a: until the function-level `T?unknown(...)` return-flow marker
# exists, only *explicit* `unknown` literals taint the symbol.
#
# Fixtures:
#   bug393 — call-arg forwarding rejected (negative).
#   bug394 — ok(u) wrapper accepted (positive).
#   bug395 — pass-statement return rejected (negative).
#   bug396 — taint propagates through reassignment (negative).
#   bug397 — D-17a opt-in: no marker → no taint from function-call init
#            (positive).
#
# See:
#   META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.2.0.md (D-17, D-17a, D-18)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03124.exe"

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

echo "== v0.31.2.4 D-17 / D-17a — unknown-taint + ok() enforcement (ARIA-045) =="

expect_compile_fail_with "bug393 unknown-taint call-arg" \
    "$SCRIPT_DIR/bug393_unknown_taint_call_arg_fail.npk" "ARIA-045"

expect_compile_run "bug394 ok(u) wrapper strips taint (exit 0)" \
    "$SCRIPT_DIR/bug394_unknown_taint_ok_wrapped_pass.npk" "0"

expect_compile_fail_with "bug395 unknown-taint pass-stmt" \
    "$SCRIPT_DIR/bug395_unknown_taint_pass_stmt_fail.npk" "ARIA-045"

expect_compile_fail_with "bug396 taint propagates via reassignment" \
    "$SCRIPT_DIR/bug396_unknown_taint_propagation_fail.npk" "ARIA-045"

expect_compile_run "bug397 D-17a no marker → no taint (exit 0)" \
    "$SCRIPT_DIR/bug397_unknown_d17a_no_marker_pass.npk" "0"

echo
echo "Summary: $PASS/$TOTAL passed, $FAIL failed"
if [ "$FAIL" -gt 0 ]; then exit 1; fi
exit 0
