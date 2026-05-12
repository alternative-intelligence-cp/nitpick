#!/usr/bin/env bash
# v0.24.3 regression tests — macro × comptime interaction
# (COMPTIME-006, COMPTIME-007)
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

run_test "bug155_macro_expands_to_comptime" \
    "${SCRIPT_DIR}/bug155_macro_expands_to_comptime.npk"

run_test "bug156_comptime_calls_macro" \
    "${SCRIPT_DIR}/bug156_comptime_calls_macro.npk"

run_test "bug157_comptime_builtin_macro" \
    "${SCRIPT_DIR}/bug157_comptime_builtin_macro.npk"

run_test "bug158_macro_comptime_nested" \
    "${SCRIPT_DIR}/bug158_macro_comptime_nested.npk"

echo
if [[ $fail -ne 0 ]]; then
    echo "v0.24.3 regression tests FAILED: $fail failing, $pass passing"
    exit 1
fi
echo "v0.24.3 regression tests PASSED: $pass passing"
