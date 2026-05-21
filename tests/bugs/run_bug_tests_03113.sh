#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.1.3 Phase 2 D-9.
#
# D-9 META correction: `dyn Trait` codegen (vtable + thunks + fat
# pointer) was already implemented in v0.2.36 — the kickoff audit
# under-scoped the surface. v0.31.1.3 (a) pins the existing surface
# with regression fixtures and (b) extends the coercion to the
# local-variable initializer form (`dyn Trait:x = concrete;`),
# mirroring the call-site path. A new diagnostic ARIA-043 fires when
# the concrete type lacks an `impl` for the trait.
#
# See META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.3.md.
#
# Fixtures:
#   bug363 — dyn Trait via local var, int32 + single method: exit 63.
#   bug364 — dyn Trait via fn arg, struct implementor: exit 64.
#   bug365 — dyn Trait via fn arg, heterogeneous if/else: exit 65.
#   bug366 — dyn Trait local var, no matching impl: ARIA-043.
#   bug367 — dyn Trait via local var, two-method dispatch: exit 67.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03113.exe"

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

echo "== v0.31.1.3 D-9 — dyn Trait surface + local-var coercion =="

expect_compile_run "bug363 dyn Trait local var, int32 single method (exit 63)" \
    "$SCRIPT_DIR/bug363_dyn_trait_local_var_int_pass.npk" "63"

expect_compile_run "bug364 dyn Trait fn arg, struct implementor (exit 64)" \
    "$SCRIPT_DIR/bug364_dyn_trait_struct_arg_pass.npk" "64"

expect_compile_run "bug365 dyn Trait heterogeneous branch (exit 65)" \
    "$SCRIPT_DIR/bug365_dyn_trait_heterogeneous_branch_pass.npk" "65"

expect_compile_fail_with "bug366 dyn Trait local, no impl → ARIA-043" \
    "$SCRIPT_DIR/bug366_dyn_trait_local_no_impl_fail.npk" "ARIA-043"

expect_compile_run "bug367 dyn Trait local var, two-method dispatch (exit 67)" \
    "$SCRIPT_DIR/bug367_dyn_trait_local_var_dispatch_pass.npk" "67"

echo
echo "Summary: $PASS/$TOTAL passed, $FAIL failed"
if [ "$FAIL" -gt 0 ]; then exit 1; fi
exit 0
