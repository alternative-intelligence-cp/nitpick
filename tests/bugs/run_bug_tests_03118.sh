#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.1.8 Phase 2 D-12 sub-slice 2.
#
# Sub-slice 2 fixes the impl-method `$$m self` parameter lowering
# bug discovered while testing v0.31.1.7's Probe C `$$m` variant.
#
# Bug fixed:
#   Trait/impl methods declaring `$$m Type:self` were lowered to a
#   by-value LLVM signature `Method(%Type %self)` because the impl
#   branch in IRGenerator (ir_generator.cpp ~4145) ignored
#   ParameterNode::isBorrowMut and always called mapTypeFromName.
#   Mutations on `self.field` operated on a stack copy and never
#   reached the caller — independent of dyn coercion (any
#   trait/impl method with `$$m self` was affected).
#
# Fix:
#   Mirror the top-level `func:` path in two places:
#     (a) param_types builder: `pnode->isBorrowMut` → ptr.
#     (b) param storage loop: $$m param bound directly to arg
#         (no alloca/store copy), name registered in
#         var_aria_types as "__borrow_param_mut:<name>".
#     (c) After Function::Create, mark __func_borrow_param:
#         <mangled>:<i> = "mut" so UFCS call sites pass the
#         caller's address (codegen_expr_call.cpp ~7752).
#
# See:
#   META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.7.md (discovery)
#   META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.8.md (this sub-slice)
#
# Fixtures:
#   bug372 — concrete UFCS $$m self mutation (no dyn), exit 6.
#   bug373 — $$m dyn Trait local borrow + mutation through
#            vtable dispatch (combines v0.31.1.7 fat-ptr build
#            with v0.31.1.8 lowering), exit 6.
#
# Not yet covered (deferred to v0.31.1.9+):
#   - $$m dyn Trait PARAM (Probe B): call-site coercion path
#     needs handling for the borrow case so the fat ptr's data
#     slot becomes the source's address (not a boxed copy).
#   - Borrow-checker liveness for dyn-target borrows (Probe D):
#     borrow recorded but conflict not flagged because the
#     source access path isn't propagated through dyn coercion.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03118.exe"

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

echo "== v0.31.1.8 D-12 sub-slice 2 — impl-method \$\$m self lowering =="

expect_compile_run "bug372 impl-method \$\$m self mutation (exit 6)" \
    "$SCRIPT_DIR/bug372_impl_method_mut_self_pass.npk" "6"

expect_compile_run "bug373 \$\$m dyn Trait local borrow mutation (exit 6)" \
    "$SCRIPT_DIR/bug373_dyn_borrow_local_mut_pass.npk" "6"

echo
echo "Summary: $PASS/$TOTAL passed, $FAIL failed"
if [ "$FAIL" -gt 0 ]; then exit 1; fi
exit 0
