#!/usr/bin/env bash
# v0.22.6 regression tests — POLISH-008 (reserved keyword diagnostics)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

pass=0; fail=0

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

FAILSAFE='func:failsafe = int32(tbb32:e) { exit 1; };'

# bug120 — 'ok' as variable name should mention "reserved keyword"
expect_compile_fail "bug120a_ok_reserved" \
    "func:main = int32() { int32:ok = 1i32; exit 0; }; $FAILSAFE" \
    "reserved keyword"

# bug120 — 'end' as variable name
expect_compile_fail "bug120b_end_reserved" \
    "func:main = int32() { int32:end = 1i32; exit 0; }; $FAILSAFE" \
    "reserved keyword"

# bug120 — 'fail' as variable name
expect_compile_fail "bug120c_fail_reserved" \
    "func:main = int32() { int32:fail = 1i32; exit 0; }; $FAILSAFE" \
    "reserved keyword"

# bug120 — 'raw' as variable name
expect_compile_fail "bug120d_raw_reserved" \
    "func:main = int32() { int32:raw = 1i32; exit 0; }; $FAILSAFE" \
    "reserved keyword"

# bug120 — 'stream' as variable name
expect_compile_fail "bug120e_stream_reserved" \
    "func:main = int32() { int32:stream = 1i32; exit 0; }; $FAILSAFE" \
    "reserved keyword"

# bug120 — generic error still fires for a non-keyword token (e.g., a number literal)
expect_compile_fail "bug120f_generic_error" \
    "func:main = int32() { int32:123; exit 0; }; $FAILSAFE" \
    "Expected"

echo ""
echo "Results: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
