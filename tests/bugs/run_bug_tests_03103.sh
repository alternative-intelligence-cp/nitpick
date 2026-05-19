#!/usr/bin/env bash
# v0.31.0.3 Phase 1 async — D-7: pass/fail short-circuit semantics in async fns
#
# Each fixture places a `println("DEAD-...")` marker after a `pass`/`fail`
# in an async function (mid-body / inside a loop / inside nested ifs).
# The marker must NEVER execute. We verify by checking that:
#   1) the program compiles and runs (exit 0),
#   2) "DEAD" never appears in the program's stdout.

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

run_case() {
    local label="$1"
    local src="$2"
    local bin="$TMP/$(basename "$src" .npk)"
    if ! "$NPKC" "$src" -o "$bin" >"$TMP/compile.log" 2>&1; then
        echo "  FAIL: $label — compile failed"
        cat "$TMP/compile.log"
        FAILED=$((FAILED+1)); return
    fi
    if [[ ! -x "$bin" ]]; then
        echo "  FAIL: $label — binary not produced"
        FAILED=$((FAILED+1)); return
    fi
    local out rc
    out="$("$bin" 2>&1)"; rc=$?
    if [[ $rc -ne 0 ]]; then
        echo "  FAIL: $label — runtime exit $rc, stdout=[$out]"
        FAILED=$((FAILED+1)); return
    fi
    if grep -q DEAD <<<"$out"; then
        echo "  FAIL: $label — dead code executed, stdout=[$out]"
        FAILED=$((FAILED+1)); return
    fi
    echo "  PASS: $label"
    PASSED=$((PASSED+1))
}

echo "== v0.31.0.3 Phase 1 async D-7 (pass/fail short-circuit in async fns) =="
run_case "bug339 fail mid-body short-circuits"      tests/bugs/bug339_async_fail_mid_body_pass.npk
run_case "bug340 pass mid-body short-circuits"      tests/bugs/bug340_async_pass_mid_body_pass.npk
run_case "bug341 fail inside while loop exits fn"   tests/bugs/bug341_async_fail_in_loop_pass.npk
run_case "bug342 fail inside nested ifs exits fn"   tests/bugs/bug342_async_fail_nested_block_pass.npk
echo "== Results: $PASSED/$((PASSED+FAILED)) passed, $FAILED failed =="

[[ $FAILED -eq 0 ]]
