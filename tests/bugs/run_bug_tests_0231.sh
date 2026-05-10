#!/usr/bin/env bash
# v0.23.1 regression tests — Macro MACRO-001 + MACRO-002
# Basic expression macro pipeline + call-site error reporting
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

pass=0; fail=0

FAILSAFE='func:failsafe = int32(tbb32:e) { exit 1; };'

# ── helpers ─────────────────────────────────────────────────────────────────

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
    local out; out=$(echo "$snippet" | "$NPKC" /dev/stdin -o /tmp/npk_test_out 2>&1 || true)
    if echo "$out" | grep -qF "$pattern"; then
        echo "PASS: $label"; pass=$((pass+1))
    else
        echo "FAIL: $label — expected pattern: '$pattern'"
        echo "      got: $out"
        fail=$((fail+1))
    fi
}

# ── bug121 — basic expression macro, 1 param (MACRO-001) ────────────────────
# double_it!(5) should produce 10 → exit 0

TMP121=$(mktemp /tmp/bug121_XXXXXX.npk)
cat > "$TMP121" << 'NPKEOF'
macro:double_it = (x) {
    (x + x);
};

pub func:main = int32() {
    int32:result = double_it!(5i32);
    if (result == 10i32) {
        exit 0;
    }
    exit 1;
};
func:failsafe = int32(tbb32:e) { exit 1; };
NPKEOF
expect_run "bug121a_macro_1param" "$TMP121" 0
rm -f "$TMP121"

# bug121 — two-param expression macro
TMP121B=$(mktemp /tmp/bug121b_XXXXXX.npk)
cat > "$TMP121B" << 'NPKEOF'
macro:add = (a, b) {
    (a + b);
};

pub func:main = int32() {
    int32:result = add!(3i32, 4i32);
    if (result == 7i32) {
        exit 0;
    }
    exit 1;
};
func:failsafe = int32(tbb32:e) { exit 1; };
NPKEOF
expect_run "bug121b_macro_2param" "$TMP121B" 0
rm -f "$TMP121B"

# bug121 — zero-param macro (constant producer)
TMP121C=$(mktemp /tmp/bug121c_XXXXXX.npk)
cat > "$TMP121C" << 'NPKEOF'
macro:answer = () {
    42i32;
};

pub func:main = int32() {
    int32:result = answer!();
    if (result == 42i32) {
        exit 0;
    }
    exit 1;
};
func:failsafe = int32(tbb32:e) { exit 1; };
NPKEOF
expect_run "bug121c_macro_0param" "$TMP121C" 0
rm -f "$TMP121C"

# bug121 — macro result used in expression (not just standalone)
TMP121D=$(mktemp /tmp/bug121d_XXXXXX.npk)
cat > "$TMP121D" << 'NPKEOF'
macro:triple = (x) {
    (x + x + x);
};

pub func:main = int32() {
    int32:result = triple!(2i32) + 1i32;
    if (result == 7i32) {
        exit 0;
    }
    exit 1;
};
func:failsafe = int32(tbb32:e) { exit 1; };
NPKEOF
expect_run "bug121d_macro_in_expr" "$TMP121D" 0
rm -f "$TMP121D"

# ── bug122 — wrong arg count reports at call site (MACRO-002) ───────────────

# Too many args
expect_compile_fail "bug122a_too_many_args" \
    "macro:single = (x) { x; }; func:main = int32() { int32:r = single!(1i32, 2i32); exit 0; }; $FAILSAFE" \
    "expects 1 arguments, got 2"

# Too few args
expect_compile_fail "bug122b_too_few_args" \
    "macro:pair = (a, b) { (a + b); }; func:main = int32() { int32:r = pair!(1i32); exit 0; }; $FAILSAFE" \
    "expects 2 arguments, got 1"

# ── bug123 — undefined macro name (MACRO-002) ───────────────────────────────

expect_compile_fail "bug123a_undefined_macro" \
    "func:main = int32() { int32:r = nope!(1i32); exit 0; }; $FAILSAFE" \
    "Undefined macro 'nope'"

# Duplicate macro declaration
expect_compile_fail "bug123b_duplicate_macro" \
    "macro:foo = (x) { x; }; macro:foo = (x) { x; }; func:main = int32() { exit 0; }; $FAILSAFE" \
    "already defined"

echo ""
echo "Results: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
