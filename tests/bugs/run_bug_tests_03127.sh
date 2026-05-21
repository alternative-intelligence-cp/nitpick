#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.2.7 D-21.
#
# D-21: a function whose explicit return type is NIL (i.e. void-shaped),
# when it has a body (i.e. is NOT an extern function), wraps its return
# type to Result<NIL> at both sema and codegen — exactly like every
# other Aria function. `pass NIL;` and `fail(code);` are both well-typed
# inside such a function, and the caller observes a real Result<NIL>
# value with `.is_error`, not a bare LLVM `void`.
#
# This slice is a verification slice. The current compiler (verified via
# --emit-llvm) already emits `define { %struct.NIL, ptr, i8 } @bad()`
# for `func:bad = NIL() { fail(...); };`, so no production code changes
# were required for v0.31.2.7. The fixtures below lock in the behaviour
# so it cannot silently regress.
#
# Fixtures:
#   bug406 — `func:noop = NIL() { pass NIL; };` ; caller checks .is_error == false, exits 0   (positive)
#   bug407 — `func:bad  = NIL() { fail(7); };`  ; caller checks .is_error == true,  exits 0   (positive)
#   bug408 — silent drop `bad();` (no raw, no assign) — compiler must reject ("Unused result value")
#   bug409 — `raw noop();` discards Result<NIL>, exits 0                                       (positive)
#
# See: META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.2.0.md (D-21)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03127.exe"

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

echo "== v0.31.2.7 D-21 — NIL-returning functions wrap to Result<NIL> =="

expect_compile_run "bug406 NIL pass NIL — caller checks is_error==false" \
    "$SCRIPT_DIR/bug406_nil_pass_success.npk" "0"

expect_compile_run "bug407 NIL fail(code) — caller checks is_error==true" \
    "$SCRIPT_DIR/bug407_nil_fail_caller_handles.npk" "0"

expect_compile_fail "bug408 NIL silent-drop rejected as Unused result value" \
    "$SCRIPT_DIR/bug408_nil_unused_result_rejected.npk" \
    "Unused result value"

expect_compile_run "bug409 NIL raw call discards Result<NIL>" \
    "$SCRIPT_DIR/bug409_nil_raw_call_success.npk" "0"

echo ""
echo "== Summary =="
echo "  Passed: $PASS / $TOTAL"
echo "  Failed: $FAIL / $TOTAL"

[ "$FAIL" = "0" ] && exit 0 || exit 1
