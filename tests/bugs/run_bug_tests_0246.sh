#!/usr/bin/env bash
# v0.24.6 regression tests — limit<Rules> comptime + comptime limitations
# (COMPTIME-011, COMPTIME-012)
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

run_test "bug167_limit_comptime_known" \
    "${SCRIPT_DIR}/bug167_limit_comptime_known.npk"

run_diag_test "bug168_limit_comptime_violation" \
    "${SCRIPT_DIR}/bug168_limit_comptime_violation.npk" \
    "comptime"

run_diag_test "bug169_comptime_no_io" \
    "${SCRIPT_DIR}/bug169_comptime_no_io.npk" \
    "I/O" \
    "comptime"

run_diag_test "bug170_comptime_no_extern" \
    "${SCRIPT_DIR}/bug170_comptime_no_extern.npk" \
    "extern" \
    "comptime"

echo
if [[ $fail -ne 0 ]]; then
    echo "v0.24.6 regression tests FAILED: $fail failing, $pass passing"
    exit 1
fi
echo "v0.24.6 regression tests PASSED: $pass passing"
