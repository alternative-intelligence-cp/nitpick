#!/usr/bin/env bash
# v0.24.5 regression tests — comptime generics: `type:T` parameters
# (COMPTIME-010)
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

# run_diag_test: compile must FAIL and stderr must contain ALL the given
# substring patterns (one per extra argument).
run_diag_test() {
    local label="$1"
    local file="$2"
    shift 2
    local patterns=("$@")
    local bin
    bin="$(mktemp /tmp/npk_test_XXXXXX)"

    set +e
    local compile_out
    compile_out=$("$NPKC" "$file" -o "$bin" 2>&1)
    local compile_rc=$?
    set -e
    rm -f "$bin"

    if [[ $compile_rc -eq 0 ]]; then
        echo "FAIL: $label — compile succeeded (expected failure)"
        fail=$((fail + 1))
        return
    fi

    local p
    for p in "${patterns[@]}"; do
        if ! grep -q -- "$p" <<<"$compile_out"; then
            echo "FAIL: $label — diagnostic missing pattern: $p"
            echo "--- diagnostic was: ---"
            echo "$compile_out"
            echo "-----------------------"
            fail=$((fail + 1))
            return
        fi
    done

    echo "PASS: $label"
    pass=$((pass + 1))
}

run_test "bug164_comptime_generic_typeinfo" \
    "${SCRIPT_DIR}/bug164_comptime_generic_typeinfo.npk"

run_test "bug165_comptime_generic_sizeof_assert_pass" \
    "${SCRIPT_DIR}/bug165_comptime_generic_sizeof_assert_pass.npk"

run_diag_test "bug166_comptime_generic_noncomptime_fail" \
    "${SCRIPT_DIR}/bug166_comptime_generic_noncomptime_fail.npk" \
    "type:" \
    "comptime"

echo
if [[ $fail -ne 0 ]]; then
    echo "v0.24.5 regression tests FAILED: $fail failing, $pass passing"
    exit 1
fi
echo "v0.24.5 regression tests PASSED: $pass passing"
