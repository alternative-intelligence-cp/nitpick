#!/bin/bash
# Nitpick Bug Regression Tests — v0.26.3.1 (allocation qualifier wiring)
#
# Locks in the v0.26.3.1 fix that wires the `gc`, `wild`, and `stack` keywords
# through to their correct backend lowering. Before v0.26.3.1, IRGenerator's
# VAR_DECL case silently ignored the qualifier and emitted a stack alloca for
# every binding — a parallel `class StmtCodegen` had the correct lowering but
# was never instantiated. v0.26.3.1 inlined the correct three-way switch into
# IRGenerator and deleted StmtCodegen.
#
# Three fixtures, all IR-inspection (no execution):
#   bug220 — `gc T:x`    must emit `npk_gc_alloc` in LLVM IR
#   bug221 — `wild T:x`  IR-level wire-up VERIFIED MANUALLY in v0.26.3.1, but a
#            self-contained regression fixture is DEFERRED to v0.27 (the wild
#            ergonomics release). Today the type system never promotes a wild
#            value-type binding `wild Holder:h` to a pointer type, so the
#            `free(h)` builtin (and ARIA-014 leak check) makes such a fixture
#            uncompilable end-to-end. The IR-level switch in IRGenerator does
#            call `npk_alloc` correctly when reached.
#   bug222 — `stack T:x` must emit `alloca`        in LLVM IR
#
# IR inspection (vs. running the binary) keeps these tests deterministic:
# wild ergonomics work continues in v0.27, and we don't want this regression
# guard to depend on whether wild value-type bindings have a typed free yet.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_LL="/tmp/npk_bug_test_02631.ll"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_ir_match() {
    local label="$1"
    local file="$2"
    local needle="$3"
    local antineedle="$4"  # optional: if present, must NOT appear

    rm -f "$TMP_LL"
    local out
    out=$("$NPKC" --emit-llvm "$file" -o "$TMP_LL" 2>&1)
    if [ ! -f "$TMP_LL" ]; then
        fail "$label" "compile failed: $out"
        return
    fi
    if ! grep -qE "$needle" "$TMP_LL"; then
        fail "$label" "expected IR to match /$needle/ but it did not"
        rm -f "$TMP_LL"
        return
    fi
    if [ -n "$antineedle" ] && grep -qE "$antineedle" "$TMP_LL"; then
        fail "$label" "IR also matched /$antineedle/, which it should not"
        rm -f "$TMP_LL"
        return
    fi
    rm -f "$TMP_LL"
    pass "$label"
}

echo "== v0.26.3.1 allocation qualifier IR-lowering regression tests =="

# bug220: gc binding must call npk_gc_alloc and the user variable name `h`
# must be assigned from the call result (i.e. the binding IS the heap pointer,
# not an alloca holding a pointer).
expect_ir_match "bug220 gc T:x emits npk_gc_alloc" \
    "$SCRIPT_DIR/bug220_gc_emits_npk_gc_alloc.npk" \
    "%h = call ptr @npk_gc_alloc"

# bug221: wild binding IR wire-up — DEFERRED to v0.27.
# See header comment for full rationale. We keep the fixture in-tree so the
# v0.27 wild-ergonomics work has a ready test to flip on.
echo "  SKIP: bug221 wild T:x emits npk_alloc (deferred to v0.27 wild ergonomics)"

# bug222: stack binding must remain a true alloca.
expect_ir_match "bug222 stack T:x emits alloca" \
    "$SCRIPT_DIR/bug222_stack_emits_alloca.npk" \
    "%h = alloca %Holder"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
