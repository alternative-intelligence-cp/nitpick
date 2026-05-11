#!/usr/bin/env bash
# v0.23.3 regression tests — Recursive macros (MACRO-004)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

pass=0; fail=0
FAILSAFE='func:failsafe = int32(tbb32:e) { exit 1; };'

expect_run() {
    local label="$1"; local file="$2"; local expected_exit="${3:-0}"
    local bin="/tmp/npk_test_${label}"
    if "$NPKC" "$file" -o "$bin" 2>/dev/null && "$bin"; ret=$?; then
        if [[ $ret -eq $expected_exit ]]; then
            echo "PASS: $label"; pass=$((pass+1))
        else
            echo "FAIL: $label — expected exit $expected_exit, got $ret"
            fail=$((fail+1))
        fi
    else
        echo "FAIL: $label — compile or run failed"
        fail=$((fail+1))
    fi
}

expect_compile_fail() {
    local label="$1"; local snippet="$2"; local pattern="$3"
    local out
    out=$(echo "$snippet" | "$NPKC" /dev/stdin -o /tmp/npk_test_out 2>&1 || true)
    if echo "$out" | grep -qF "$pattern"; then
        echo "PASS: $label"; pass=$((pass+1))
    else
        echo "FAIL: $label — expected pattern: '$pattern'"
        echo "      got: $out"
        fail=$((fail+1))
    fi
}

# bug127 — macro calls another macro (nested expansion)
TMP127=$(mktemp /tmp/bug127_XXXXXX.npk)
cat > "$TMP127" << 'NPKEOF'
macro:add_one = (x) {
    (x + 1i32);
};

macro:add_two = (x) {
    add_one!(add_one!(x));
};

pub func:main = int32() {
    int32:r = add_two!(5i32);
    if (r == 7i32) {
        exit 0;
    }
    exit 1;
};
func:failsafe = int32(tbb32:e) { exit 1; };
NPKEOF
expect_run "bug127_nested_macro_calls" "$TMP127" 0
rm -f "$TMP127"

# bug128 — deeper nested expansion chain
TMP128=$(mktemp /tmp/bug128_XXXXXX.npk)
cat > "$TMP128" << 'NPKEOF'
macro:inc = (x) {
    (x + 1i32);
};

macro:plus_three = (x) {
    inc!(inc!(inc!(x)));
};

pub func:main = int32() {
    int32:r = plus_three!(2i32);
    if (r == 5i32) {
        exit 0;
    }
    exit 1;
};
func:failsafe = int32(tbb32:e) { exit 1; };
NPKEOF
expect_run "bug128_deep_nested_calls" "$TMP128" 0
rm -f "$TMP128"

# bug129 — infinite recursion should hit depth guard
expect_compile_fail "bug129_depth_guard" \
    "macro:recurse = (x) { recurse!(x); }; func:main = int32() { int32:r = recurse!(1i32); exit 0; }; $FAILSAFE" \
    "Macro expansion depth limit (64) exceeded"

echo ""
echo "Results: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
