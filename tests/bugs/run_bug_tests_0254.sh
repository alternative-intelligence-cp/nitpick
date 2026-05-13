#!/bin/bash
# Nitpick Bug Regression Tests — v0.25.4
# Runs bug190–bug194 (BORROW-007 inter-procedural parameter intent,
#                     BORROW-008 return-borrow lifetime).
#   bug190: $$m x = p.a; set(p.a, ...) — path conflict (mut+mut) rejected.
#   bug191: $$m x = p.a; read_a(p.a)  — path conflict (mut+imm) rejected.
#   bug192: $$i y = get_a(p); p.a = .. — return-borrow source mutated rejected.
#   bug193: $$i y = get_a(p); $$m z = p.b — sub-mut blocked while y live.
#   bug194: choose(p.a, p.b, ..) — multi-borrow-param ambiguity rejected.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0254_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_compile_fail() {
    local label="$1"
    local file="$2"
    local expect_msg="$3"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local rc=$?
    rm -f "$TMP_BIN"
    if [ $rc -eq 0 ]; then
        fail "$label" "expected compile failure but got exit 0"
    elif [ -n "$expect_msg" ] && ! echo "$out" | grep -qi "$expect_msg"; then
        fail "$label" "compile failed (good) but expected msg '$expect_msg' not in: $out"
    else
        pass "$label"
    fi
}

echo "== v0.25.4 inter-procedural borrow regression tests =="

expect_compile_fail "bug190 call arg path mut/mut conflict" \
    "$SCRIPT_DIR/bug190_call_arg_path_mut_mut_rejected.npk" \
    "ARIA-023"

expect_compile_fail "bug191 call arg path imm vs live mut" \
    "$SCRIPT_DIR/bug191_call_arg_path_imm_vs_mut_rejected.npk" \
    "ARIA-023"

expect_compile_fail "bug192 return borrow source mutated" \
    "$SCRIPT_DIR/bug192_return_borrow_source_mutated_rejected.npk" \
    "ARIA-026"

expect_compile_fail "bug193 return borrow blocks sub-mut" \
    "$SCRIPT_DIR/bug193_return_borrow_blocks_sub_mut_rejected.npk" \
    "ARIA-023"

expect_compile_fail "bug194 multi-borrow-param ambiguous return" \
    "$SCRIPT_DIR/bug194_return_borrow_multi_source_unannotated_rejected.npk" \
    "cannot infer"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
