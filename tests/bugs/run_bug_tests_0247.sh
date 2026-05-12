#!/usr/bin/env bash
# v0.24.7 regression tests — comptime struct field reflection (COMPTIME-013)
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

run_test "bug171_typeinfo_struct_fields" \
    "${SCRIPT_DIR}/bug171_typeinfo_struct_fields.npk"

run_test "bug172_typeinfo_field_bit_width" \
    "${SCRIPT_DIR}/bug172_typeinfo_field_bit_width.npk"

if [[ $fail -eq 0 ]]; then
    echo "v0.24.7 regression tests PASSED: $pass passing"
    exit 0
else
    echo "v0.24.7 regression tests FAILED: $fail failing, $pass passing"
    exit 1
fi
