#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.2.1 Phase 3 D-14 sub-slice 1.
#
# D-14 ratifies that `const` is reserved for `extern "..." { ... }`
# blocks; `fixed` is the Aria-side immutability marker for every
# other position. This slice enforces the rule:
#   - ARIA-044 emitted when `const` reaches a non-extern var-decl
#     (sema, checkVarDecl).
#   - `fixed` is extended with opportunistic comptime folding so the
#     stdlib/test sweep `const`→`fixed` does not regress comptime
#     use of binding-level constants.
#
# Fixtures:
#   bug379 — local `const` rejected with ARIA-044 (negative).
#   bug380 — module-level `const` rejected with ARIA-044 (negative).
#   bug381 — `fixed` with comptime initializer, exit 50 (positive).
#   bug382 — `fixed` with runtime initializer, exit 7 (positive).
#
# See:
#   META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.2.0.md

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03121.exe"

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

echo "== v0.31.2.1 D-14 sub-slice 1 — const-placement enforcement (ARIA-044) =="

expect_compile_fail_with "bug379 local const → ARIA-044" \
    "$SCRIPT_DIR/bug379_const_outside_extern_local_fail.npk" "ARIA-044"

expect_compile_fail_with "bug380 module-level const → ARIA-044" \
    "$SCRIPT_DIR/bug380_const_outside_extern_module_fail.npk" "ARIA-044"

expect_compile_run "bug381 fixed comptime initializer (exit 50)" \
    "$SCRIPT_DIR/bug381_fixed_comptime_initializer_pass.npk" "50"

expect_compile_run "bug382 fixed runtime initializer (exit 7)" \
    "$SCRIPT_DIR/bug382_fixed_runtime_initializer_pass.npk" "7"

echo
echo "Summary: $PASS/$TOTAL passed, $FAIL failed"
if [ "$FAIL" -gt 0 ]; then exit 1; fi
exit 0
