#!/usr/bin/env bash
# v0.23.4 regression tests — Variadic macros (MACRO-005)
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

# bug130 — variadic-only macro accepts 0/1/3 args
TMP130=$(mktemp /tmp/bug130_XXXXXX.npk)
cat > "$TMP130" << 'NPKEOF'
macro:zero = (..?rest) {
    0i32;
};

pub func:main = int32() {
    int32:a = zero!();
    int32:b = zero!(1i32);
    int32:c = zero!(1i32, 2i32, 3i32);
    if (a == 0i32 && b == 0i32 && c == 0i32) {
        exit 0;
    }
    exit 1;
};
func:failsafe = int32(tbb32:e) { exit 1; };
NPKEOF
expect_run "bug130_variadic_only_0_1_3" "$TMP130" 0
rm -f "$TMP130"

# bug131 — fixed + variadic rest accepts base arg and extras
TMP131=$(mktemp /tmp/bug131_XXXXXX.npk)
cat > "$TMP131" << 'NPKEOF'
macro:first = (x, ..?rest) {
    x;
};

pub func:main = int32() {
    int32:a = first!(7i32);
    int32:b = first!(7i32, 8i32, 9i32);
    if (a == 7i32 && b == 7i32) {
        exit 0;
    }
    exit 1;
};
func:failsafe = int32(tbb32:e) { exit 1; };
NPKEOF
expect_run "bug131_fixed_plus_rest" "$TMP131" 0
rm -f "$TMP131"

# bug132 — arity and syntax errors
expect_compile_fail "bug132a_too_few_for_variadic" \
    "macro:first = (x, ..?rest) { x; }; func:main = int32() { int32:r = first!(); exit 0; }; $FAILSAFE" \
    "expects at least 1 arguments, got 0"

expect_compile_fail "bug132b_rest_not_last" \
    "macro:bad = (..?rest, x) { x; }; func:main = int32() { exit 0; }; $FAILSAFE" \
    "Rest variadic parameter must be the last macro parameter"

echo ""
echo "Results: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
