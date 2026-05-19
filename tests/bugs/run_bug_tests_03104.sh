#!/usr/bin/env bash
# v0.31.0.4 Phase 1 async — D-5: borrows do not survive an `await` (ARIA-041).
#
# Eight fixtures exercise the new contract:
#   bug343 — $$m across await fails with ARIA-041.
#   bug344 — $$i across await also fails (per D-5: invalidation is unconditional).
#   bug345 — borrow released in an inner scope before await compiles cleanly.
#   bug346 — re-borrow AFTER await is legal (fresh borrow state).
#   bug347 — TWO live borrows at one await produce TWO ARIA-041 errors.
#   bug348 — scoped borrow / await / scoped borrow / await is legal.
#   bug349 — unused-after-await borrow still fires (binding lives past await).
#   bug350 — value snapshot before await is the workaround (no borrow at suspend).

set -u

NPKC="${NPKC:-./build/npkc}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [[ ! -x "$NPKC" ]]; then
    echo "SKIP: $NPKC not built"
    exit 77
fi

PASSED=0
FAILED=0

# expect_fail <label> <file> <expected_substr> [min_error_count]
expect_fail() {
    local label="$1" src="$2" needle="$3" min_count="${4:-1}"
    local bin="$TMP/$(basename "$src" .npk)"
    local out rc
    out="$("$NPKC" "$src" -o "$bin" 2>&1)"; rc=$?
    if [[ $rc -eq 0 ]]; then
        echo "  FAIL: $label — expected compile failure but got rc=0"
        FAILED=$((FAILED+1)); return
    fi
    # Count actual error occurrences. npkc may line-wrap long error
    # messages, so we count only the "error: [CODE]" prefix portion, not
    # the trailing prose. The 'note:' lines must be excluded.
    local n
    n=$(grep -E "error:.*\[$needle\]|\[$needle\]" <<<"$out" | grep -v 'note:' | wc -l)
    if [[ $n -lt $min_count ]]; then
        echo "  FAIL: $label — expected >=$min_count '$needle' errors, found $n"
        echo "----- output -----"
        echo "$out"
        echo "------------------"
        FAILED=$((FAILED+1)); return
    fi
    echo "  PASS: $label (found $n $needle errors)"
    PASSED=$((PASSED+1))
}

expect_pass() {
    local label="$1" src="$2"
    local bin="$TMP/$(basename "$src" .npk)"
    local out rc
    out="$("$NPKC" "$src" -o "$bin" 2>&1)"; rc=$?
    if [[ $rc -ne 0 ]]; then
        echo "  FAIL: $label — expected compile success, rc=$rc"
        echo "----- output -----"
        echo "$out"
        echo "------------------"
        FAILED=$((FAILED+1)); return
    fi
    if grep -q 'ARIA-041' <<<"$out"; then
        echo "  FAIL: $label — unexpected ARIA-041 in clean compile"
        FAILED=$((FAILED+1)); return
    fi
    echo "  PASS: $label"
    PASSED=$((PASSED+1))
}

echo "== v0.31.0.4 Phase 1 async D-5 (ARIA-041 borrows-across-await) =="
expect_fail "bug343 \$\$m borrow across await fires ARIA-041" \
    tests/bugs/bug343_borrow_mut_across_await_fail.npk ARIA-041
expect_fail "bug344 \$\$i borrow across await also fires ARIA-041" \
    tests/bugs/bug344_borrow_imm_across_await_fail.npk ARIA-041
expect_pass "bug345 borrow released before await compiles clean" \
    tests/bugs/bug345_borrow_released_before_await_pass.npk
expect_pass "bug346 re-borrow after await is legal" \
    tests/bugs/bug346_reborrow_after_await_pass.npk
expect_fail "bug347 two borrows at one await → 2 ARIA-041" \
    tests/bugs/bug347_two_borrows_one_await_fail.npk ARIA-041 2
expect_pass "bug348 scoped borrows between awaits compile clean" \
    tests/bugs/bug348_scoped_borrows_between_awaits_pass.npk
expect_fail "bug349 unused-after-await borrow still fires ARIA-041" \
    tests/bugs/bug349_borrow_unused_after_await_still_fail.npk ARIA-041
expect_pass "bug350 value-copy workaround compiles clean" \
    tests/bugs/bug350_value_copy_across_await_pass.npk
echo "== Results: $PASSED/$((PASSED+FAILED)) passed, $FAILED failed =="

[[ $FAILED -eq 0 ]]
