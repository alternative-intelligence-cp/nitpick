#!/usr/bin/env bash
# v0.24.1 regression tests — loop() + mutable locals in CTFE (COMPTIME-001, COMPTIME-002)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

pass=0
fail=0

run_test() {
    local label="$1"
    local file="$2"
    local bin
    bin="$(mktemp /tmp/npk_test_XXXXXX)"

    set +e
    local compile_out
    compile_out=$("$NPKC" "$file" -o "$bin" 2>&1)
    local compile_rc=$?
    set -e

    if [[ $compile_rc -ne 0 ]]; then
        echo "FAIL: $label — compile failed (rc=$compile_rc)"
        echo "$compile_out"
        rm -f "$bin"
        fail=$((fail + 1))
        return
    fi

    set +e
    "$bin"
    local run_rc=$?
    set -e
    rm -f "$bin"

    if [[ $run_rc -eq 0 ]]; then
        echo "PASS: $label"
        pass=$((pass + 1))
    else
        echo "FAIL: $label — runtime exited $run_rc (expected 0)"
        fail=$((fail + 1))
    fi
}

# bug146: comptime loop counts 10 iterations correctly
run_test "bug146_comptime_loop_sum" \
    "${SCRIPT_DIR}/bug146_comptime_loop_sum.npk"

# bug147: comptime loop triangle sum (0+1+2+...+n-1)
run_test "bug147_comptime_loop_triangle" \
    "${SCRIPT_DIR}/bug147_comptime_loop_triangle.npk"

# bug148: mutable accumulator x = x + n in comptime
run_test "bug148_comptime_mutable_accumulator" \
    "${SCRIPT_DIR}/bug148_comptime_mutable_accumulator.npk"

# bug149: assignment chain a=1, b=a+1, c=a+b → c==3
run_test "bug149_comptime_assign_chain" \
    "${SCRIPT_DIR}/bug149_comptime_assign_chain.npk"

echo
if [[ $fail -ne 0 ]]; then
    echo "v0.24.1 regression tests FAILED: $fail failing, $pass passing"
    exit 1
fi
echo "v0.24.1 regression tests PASSED: $pass passing"
