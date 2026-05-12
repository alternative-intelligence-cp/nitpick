#!/usr/bin/env bash
# v0.24.2 regression tests — @sizeof, @alignof, comptime string ops
# (COMPTIME-003, COMPTIME-004, COMPTIME-005)
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

run_test "bug150_comptime_sizeof_scalars" \
    "${SCRIPT_DIR}/bug150_comptime_sizeof_scalars.npk"

run_test "bug151_comptime_alignof_types" \
    "${SCRIPT_DIR}/bug151_comptime_alignof_types.npk"

run_test "bug152_comptime_string_concat" \
    "${SCRIPT_DIR}/bug152_comptime_string_concat.npk"

run_test "bug153_comptime_string_len" \
    "${SCRIPT_DIR}/bug153_comptime_string_len.npk"

run_test "bug154_comptime_string_eq" \
    "${SCRIPT_DIR}/bug154_comptime_string_eq.npk"

echo
if [[ $fail -ne 0 ]]; then
    echo "v0.24.2 regression tests FAILED: $fail failing, $pass passing"
    exit 1
fi
echo "v0.24.2 regression tests PASSED: $pass passing"
