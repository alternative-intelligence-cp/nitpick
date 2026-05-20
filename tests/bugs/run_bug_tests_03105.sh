#!/usr/bin/env bash
# v0.31.0.5 Phase 1 async — D-10: Drop interaction with async functions.
#
# Three fixtures verify that the existing Drop machinery composes with
# async/coroutine lowering for the scenarios reachable from the user
# surface today:
#
#   bug351 — normal completion: drop fires when the coroutine completes.
#   bug352 — fail path:         drop fires during the `fail` unwind.
#   bug353 — LIFO + inner scope: nested-block drops fire at the closing
#                                `}` of the inner block (not at coroutine end),
#                                in reverse declaration order.
#
# Scenario (b) from the audit — "executor shutdown of a suspended task" —
# is intentionally NOT covered here: no awaitable in the current test
# surface produces genuine suspension (every `await` resolves
# synchronously), so a suspended-at-shutdown task cannot be constructed
# from user code. Deferred to a future slice once true-suspending
# primitives (e.g. async I/O cookbook in v0.31.0.7, or a network surface
# in Phase 2) ship.

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

# expect_run_lines_in_order <label> <src> <line1> <line2> ...
# Asserts compile rc==0, run rc==0, and each given substring appears in
# stdout in the order listed.
expect_run_lines_in_order() {
    local label="$1" src="$2"; shift 2
    local bin="$TMP/$(basename "$src" .npk)"
    local cerr crc out rrc
    cerr="$("$NPKC" "$src" -o "$bin" 2>&1)"; crc=$?
    if [[ $crc -ne 0 ]]; then
        echo "  FAIL: $label — compile rc=$crc"
        echo "----- compiler output -----"; echo "$cerr"; echo "---------------------------"
        FAILED=$((FAILED+1)); return
    fi
    out="$("$bin" 2>&1)"; rrc=$?
    if [[ $rrc -ne 0 ]]; then
        echo "  FAIL: $label — run rc=$rrc"
        echo "----- run output -----"; echo "$out"; echo "----------------------"
        FAILED=$((FAILED+1)); return
    fi
    local last_idx=0
    for needle in "$@"; do
        # Find the first line index (1-based) > last_idx containing $needle.
        local idx
        idx=$(awk -v n="$needle" -v after="$last_idx" '
            NR > after && index($0, n) > 0 { print NR; exit }
        ' <<<"$out")
        if [[ -z "$idx" ]]; then
            echo "  FAIL: $label — expected line containing '$needle' after index $last_idx, not found"
            echo "----- run output -----"; echo "$out"; echo "----------------------"
            FAILED=$((FAILED+1)); return
        fi
        last_idx=$idx
    done
    echo "  PASS: $label"
    PASSED=$((PASSED+1))
}

echo "== v0.31.0.5 Phase 1 async D-10 (Async + Drop interaction) =="

expect_run_lines_in_order "bug351 drop fires on normal async completion" \
    tests/bugs/bug351_async_drop_on_completion_pass.npk \
    "WORK-start" "DROP-Marker" "MAIN-after-spawn"

expect_run_lines_in_order "bug352 drop fires during async fail unwind" \
    tests/bugs/bug352_async_drop_on_fail_pass.npk \
    "WORK-pre-fail" "DROP-Marker" "MAIN-after-spawn"

expect_run_lines_in_order "bug353 LIFO + inner-scope drops inside async body" \
    tests/bugs/bug353_async_drop_lifo_inner_scope_pass.npk \
    "WORK-inner-start" "WORK-inner-end" "DROP-TwoB" "DROP-TwoA" "WORK-outer-end" "MAIN-after-spawn"

echo "== Results: $PASSED/$((PASSED+FAILED)) passed, $FAILED failed =="

[[ $FAILED -eq 0 ]]
