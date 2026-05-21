#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.1.2 Phase 2 D-7.
#
# D-7 (trait method `$$i`/`$$m` return signatures): the trait method
# signature path now accepts an optional return-borrow qualifier
# (`$$i` or `$$m`) before the return type, the AST carries the flag
# on `TraitMethod`, and the type checker requires the impl method's
# return-borrow qualifier to match the trait's. Mismatch is reported
# as ARIA-042.
#
# See META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.0.md (D-7 entry) and
# META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.1.md (revised Phase 2 plan).
#
# Fixtures:
#   bug358 — trait `$$i` return + matching impl `$$i` return: compile + run.
#   bug359 — trait `$$m` return + matching impl `$$m` return: compile + run.
#   bug360 — trait `$$i` return, impl plain return: must fail with ARIA-042.
#   bug361 — trait plain return, impl `$$i` return: must fail with ARIA-042.
#   bug362 — trait `$$i` return, impl `$$m` return: must fail with ARIA-042.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03112.exe"

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

echo "== v0.31.1.2 D-7 — trait method \$\$i/\$\$m return signatures =="

expect_compile_run "bug358 trait \$\$i return + matching impl \$\$i return (exit 41)" \
    "$SCRIPT_DIR/bug358_trait_return_borrow_imm_match_pass.npk" "41"

expect_compile_run "bug359 trait \$\$m return + matching impl \$\$m return (exit 42)" \
    "$SCRIPT_DIR/bug359_trait_return_borrow_mut_match_pass.npk" "42"

expect_compile_fail_with "bug360 trait \$\$i, impl plain → ARIA-042" \
    "$SCRIPT_DIR/bug360_trait_return_borrow_imm_impl_plain_fail.npk" "ARIA-042"

expect_compile_fail_with "bug361 trait plain, impl \$\$i → ARIA-042" \
    "$SCRIPT_DIR/bug361_trait_return_plain_impl_borrow_imm_fail.npk" "ARIA-042"

expect_compile_fail_with "bug362 trait \$\$i, impl \$\$m → ARIA-042" \
    "$SCRIPT_DIR/bug362_trait_return_borrow_imm_impl_borrow_mut_fail.npk" "ARIA-042"

rm -f "$TMP_EXE"

echo ""
echo "v0.31.1.2 results: $PASS/$TOTAL passed, $FAIL failed"

if [ $FAIL -eq 0 ]; then exit 0; else exit 1; fi
