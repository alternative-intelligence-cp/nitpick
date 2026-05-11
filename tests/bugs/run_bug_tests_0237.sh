#!/usr/bin/env bash
# v0.23.7 regression tests — --expand-macros debug flag (MACRO-012)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

pass=0
fail=0

expect_expand_contains() {
    local label="$1"
    local file="$2"
    local pattern="$3"

    set +e
    local out
    out=$("$NPKC" "$file" --expand-macros 2>&1)
    local rc=$?
    set -e

    if [[ $rc -ne 0 ]]; then
        echo "FAIL: $label — --expand-macros exited $rc (expected 0)"
        echo "$out"
        fail=$((fail + 1))
        return
    fi
    if echo "$out" | grep -q "$pattern"; then
        echo "PASS: $label"
        pass=$((pass + 1))
    else
        echo "FAIL: $label — output did not match '$pattern'"
        echo "--- actual output ---"
        echo "$out"
        fail=$((fail + 1))
    fi
}

expect_expand_count() {
    local label="$1"
    local file="$2"
    local expected_total="$3"

    set +e
    local out
    out=$("$NPKC" "$file" --expand-macros 2>&1)
    local rc=$?
    set -e

    if [[ $rc -ne 0 ]]; then
        echo "FAIL: $label — --expand-macros exited $rc (expected 0)"
        echo "$out"
        fail=$((fail + 1))
        return
    fi
    local total
    total=$(echo "$out" | grep "^// total:" | sed 's|// total: \([0-9]*\).*|\1|')
    if [[ "$total" == "$expected_total" ]]; then
        echo "PASS: $label"
        pass=$((pass + 1))
    else
        echo "FAIL: $label — expected total $expected_total, got '$total'"
        echo "$out"
        fail=$((fail + 1))
    fi
}

FAILSAFE='func:failsafe = int32(tbb32:e) { exit(1); };'

# bug142: --expand-macros shows user macro expansion with correct call/expanded
cat >/tmp/bug142.aria <<EOF
macro:double = (x) {
    x + x;
};

func:main = int32() {
    int32:val = double!(5i32);
    exit(0);
};
$FAILSAFE
EOF
expect_expand_contains "bug142_expand_user_macro"  /tmp/bug142.aria "double!"
expect_expand_contains "bug142b_expand_shows_expanded" /tmp/bug142.aria "// expanded:"

# bug143: --expand-macros shows built-in macro expansions (assert!, unreachable!)
cat >/tmp/bug143.aria <<EOF
func:main = int32() {
    assert!(true);
    exit(0);
};
$FAILSAFE
EOF
expect_expand_contains "bug143_assert_builtin_trace" /tmp/bug143.aria "assert!"

# bug144: --expand-macros with no macros reports "no macro invocations found"
cat >/tmp/bug144.aria <<EOF
func:main = int32() {
    exit(0);
};
$FAILSAFE
EOF
expect_expand_contains "bug144_no_macros_message" /tmp/bug144.aria "no macro invocations found"

# bug145: --expand-macros exits 0 (not 1)
cat >/tmp/bug145.aria <<EOF
macro:greet = () {
    println("hi");
};

func:main = int32() {
    greet!();
    exit(0);
};
$FAILSAFE
EOF
set +e
"$NPKC" /tmp/bug145.aria --expand-macros >/tmp/bug145.out 2>&1
rc=$?
set -e
if [[ $rc -eq 0 ]]; then
    echo "PASS: bug145_expand_exit_zero"
    pass=$((pass + 1))
else
    echo "FAIL: bug145_expand_exit_zero — expected exit 0, got $rc"
    cat /tmp/bug145.out
    fail=$((fail + 1))
fi

if [[ $fail -ne 0 ]]; then
    echo
    echo "v0.23.7 regression tests FAILED: $fail failing, $pass passing"
    exit 1
fi

echo
echo "v0.23.7 regression tests PASSED: $pass passing"
