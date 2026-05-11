#!/usr/bin/env bash
# v0.23.6 regression tests — built-in macros (MACRO-008..011)
#   assert!, todo!, unreachable!, cfg!
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

pass=0
fail=0

expect_run_exit() {
    local label="$1"
    local file="$2"
    local expected_exit="$3"
    local bin="/tmp/npk_test_${label}"

    if "$NPKC" "$file" -o "$bin" >/tmp/${label}.compile.log 2>&1; then
        set +e
        "$bin" >/tmp/${label}.run.log 2>&1
        local rc=$?
        set -e
        if [[ $rc -eq $expected_exit ]]; then
            echo "PASS: $label"
            pass=$((pass + 1))
        else
            echo "FAIL: $label — expected exit $expected_exit, got $rc"
            echo "--- compile log ---"
            cat /tmp/${label}.compile.log
            echo "--- run log ---"
            cat /tmp/${label}.run.log
            fail=$((fail + 1))
        fi
    else
        echo "FAIL: $label — compile failed"
        cat /tmp/${label}.compile.log
        fail=$((fail + 1))
    fi
}

expect_run_nonzero() {
    local label="$1"
    local file="$2"
    local bin="/tmp/npk_test_${label}"

    if "$NPKC" "$file" -o "$bin" >/tmp/${label}.compile.log 2>&1; then
        set +e
        "$bin" >/tmp/${label}.run.log 2>&1
        local rc=$?
        set -e
        if [[ $rc -ne 0 ]]; then
            echo "PASS: $label"
            pass=$((pass + 1))
        else
            echo "FAIL: $label — expected non-zero exit"
            echo "--- run log ---"
            cat /tmp/${label}.run.log
            fail=$((fail + 1))
        fi
    else
        echo "FAIL: $label — compile failed"
        cat /tmp/${label}.compile.log
        fail=$((fail + 1))
    fi
}

expect_compile_fail() {
    local label="$1"
    local file="$2"
    local pattern="$3"

    local out
    out=$("$NPKC" "$file" -o /tmp/npk_test_out_${label} 2>&1 || true)
    if echo "$out" | grep -q "$pattern"; then
        echo "PASS: $label"
        pass=$((pass + 1))
    else
        echo "FAIL: $label — expected compile failure matching '$pattern'"
        echo "$out"
        fail=$((fail + 1))
    fi
}

FAILSAFE='func:failsafe = int32(tbb32:e) { exit(1); };'

# bug137: assert!(true) works and program exits 0
cat >/tmp/bug137.aria <<EOF
func:main = int32() {
    assert!(true);
    exit(0);
};
$FAILSAFE
EOF
expect_run_exit "bug137_assert_true" /tmp/bug137.aria 0

# bug138: unreachable!() triggers runtime assert fallback (non-zero exit)
cat >/tmp/bug138.aria <<EOF
func:main = int32() {
    unreachable!();
    exit(0);
};
$FAILSAFE
EOF
expect_run_nonzero "bug138_unreachable" /tmp/bug138.aria

# bug139: todo! compiles but fails at runtime when reached
cat >/tmp/bug139.aria <<EOF
func:main = int32() {
    todo!("implement me");
    exit(0);
};
$FAILSAFE
EOF
expect_run_nonzero "bug139_todo_runtime" /tmp/bug139.aria

# bug140: cfg!(target_os=linux) evaluates to true on Linux host
cat >/tmp/bug140.aria <<EOF
func:main = int32() {
    bool:is_linux = cfg!(target_os=linux);
    if (is_linux) {
        exit(0);
    } else {
        exit(2);
    }
};
$FAILSAFE
EOF
expect_run_exit "bug140_cfg_linux" /tmp/bug140.aria 0

# bug141: built-in macro names cannot be redefined
cat >/tmp/bug141.aria <<EOF
macro:assert = () { exit(0); };

func:main = int32() {
    assert!();
    exit(0);
};
$FAILSAFE
EOF
expect_compile_fail "bug141_builtin_redefine" /tmp/bug141.aria "Cannot redefine built-in macro 'assert'"

if [[ $fail -ne 0 ]]; then
    echo
    echo "v0.23.6 regression tests FAILED: $fail failing, $pass passing"
    exit 1
fi

echo
echo "v0.23.6 regression tests PASSED: $pass passing"
