#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.1.7 Phase 2 D-12 sub-slice 1.
#
# D-12 Option B (ratified in AUDIT_v0.31.1.6.md): close the
# `$$i/$$m dyn Trait` borrow surface across as many sub-slices as
# correctness requires. v0.31.1.7 is sub-slice 1 — the immutable
# local-var case (Probe C, `$$i` variant).
#
# Bug fixed:
#   The borrow-alias path in ir_generator.cpp VAR_DECL handler
#   (line ~4965) was storing the source's raw pointer in
#   named_values while var_aria_types claimed the variable's type
#   was a fat pointer (%struct.DynTraitObj). Vtable dispatch then
#   loaded a fat-ptr shape from the source's struct bytes →
#   garbage data + vtable → segfault.
#
# Fix:
#   When declared type starts with "dyn " AND the initializer is
#   an identifier (concrete local), build a proper fat pointer in
#   a fresh stack slot:
#     data slot   = address-of source (borrow alias, NOT a copy)
#     vtable slot = TraitName_vtable_ConcreteType
#   Borrow semantics are preserved (data points at the borrowed
#   source).
#
# See:
#   META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.6.md (Option B ratification)
#   META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.7.md (this sub-slice)
#
# Fixtures:
#   bug371 — $$i dyn Trait local-var borrow, dispatches read,
#            exit 33.
#
# Not yet covered (deferred to v0.31.1.8+):
#   - $$m dyn Trait mutation visibility: requires fixing a
#     pre-existing impl-method `$$m self` lowering bug
#     (impl-method `$$m Counter:self` currently lowers to
#     `Counter_bump(%Counter %self)` by-value, not by-pointer —
#     mutations never persist regardless of dyn coercion).
#   - $$m dyn Trait param: requires call-site coercion path
#     handling for the borrow case (Probe B).
#   - Borrow-checker liveness for dyn-target borrows (Probe D):
#     borrow recorded but conflict not flagged because the source
#     access path isn't propagated through dyn coercion.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03117.exe"

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

echo "== v0.31.1.7 D-12 sub-slice 1 — \$\$i dyn Trait local-var dispatch =="

expect_compile_run "bug371 \$\$i dyn Trait local borrow (exit 33)" \
    "$SCRIPT_DIR/bug371_dyn_borrow_local_imm_pass.npk" "33"

echo
echo "Summary: $PASS/$TOTAL passed, $FAIL failed"
if [ "$FAIL" -gt 0 ]; then exit 1; fi
exit 0
