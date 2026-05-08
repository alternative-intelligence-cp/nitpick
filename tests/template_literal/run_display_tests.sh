#!/bin/bash
# Nitpick Template Literal Display Tests — v0.20.2
# Tests: Display trait on structs/enums, primitive type coverage,
#        actionable error for types not implementing Display.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
FV="${SCRIPT_DIR}/../feature_validation"
TMP_BIN="/tmp/npk_display_test_bin"
TMP_OUT="/tmp/npk_display_test_out"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc compiler not found at $NPKC — skipping display tests"
    exit 77  # CTest skip code
fi

PASS=0
FAIL=0
TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# Helper: compile + run, check that output contains expected string
expect_run_contains() {
    local label="$1"
    local file="$2"
    local expected="$3"
    local compile_out rc
    compile_out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    rc=$?
    if [ $rc -ne 0 ]; then
        fail "$label" "compile failed (exit $rc): $compile_out"
        rm -f "$TMP_BIN"
        return
    fi
    local run_out
    run_out=$("$TMP_BIN" 2>&1)
    rc=$?
    rm -f "$TMP_BIN"
    if echo "$run_out" | grep -qF "$expected"; then
        pass "$label"
    else
        fail "$label" "expected '$expected' in output; got: $run_out"
    fi
}

# Helper: compile and expect failure with a specific error substring
expect_compile_error() {
    local label="$1"
    local file="$2"
    local expected_msg="$3"
    local compile_out rc
    compile_out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    rc=$?
    rm -f "$TMP_BIN"
    if [ $rc -eq 0 ]; then
        fail "$label" "expected compile error but compilation succeeded"
    elif [ -n "$expected_msg" ] && ! echo "$compile_out" | grep -qi "$expected_msg"; then
        fail "$label" "exit $rc but expected '$expected_msg' not in output: $compile_out"
    else
        pass "$label"
    fi
}

# Helper: compile only, expect success
expect_compile_ok() {
    local label="$1"
    local file="$2"
    local compile_out rc
    compile_out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    rc=$?
    rm -f "$TMP_BIN"
    if [ $rc -eq 0 ]; then
        pass "$label"
    else
        fail "$label" "expected compile success but got exit $rc: $compile_out"
    fi
}

echo "=== Template Literal Display Tests (v0.20.2) ==="
echo ""

# ── Struct Display ─────────────────────────────────────────────────────────────
echo "-- Struct Display --"

expect_compile_ok \
    "struct with Display compiles" \
    "$FV/template_literal_display.npk"

expect_run_contains \
    "struct Display: first interpolation" \
    "$FV/template_literal_display.npk" \
    "Point(3, 7)"

expect_run_contains \
    "struct Display: zero-value struct" \
    "$FV/template_literal_display.npk" \
    "Point(0, 0)"

expect_run_contains \
    "struct Display: two structs in one literal" \
    "$FV/template_literal_display.npk" \
    "Point(5, 10) and Point(1, 2)"

# ── No Display error ───────────────────────────────────────────────────────────
echo "-- No Display error --"

expect_compile_error \
    "struct without Display gives actionable error" \
    "$FV/template_literal_no_display.npk" \
    "Display"

expect_compile_error \
    "no-Display error mentions impl block" \
    "$FV/template_literal_no_display.npk" \
    "impl:Display:for:"

# ── Primitive types ────────────────────────────────────────────────────────────
echo "-- Primitive types --"

expect_compile_ok \
    "all primitive types compile in template literals" \
    "$FV/template_literal_primitives_full.npk"

expect_run_contains \
    "int8 interpolation" \
    "$FV/template_literal_primitives_full.npk" \
    "int8:64"

expect_run_contains \
    "int16 interpolation" \
    "$FV/template_literal_primitives_full.npk" \
    "int16:1000"

expect_run_contains \
    "int32 interpolation" \
    "$FV/template_literal_primitives_full.npk" \
    "int32:42"

expect_run_contains \
    "int64 interpolation" \
    "$FV/template_literal_primitives_full.npk" \
    "int64:9223372036854775807"

expect_run_contains \
    "uint8 interpolation" \
    "$FV/template_literal_primitives_full.npk" \
    "uint8:100"

expect_run_contains \
    "uint32 interpolation" \
    "$FV/template_literal_primitives_full.npk" \
    "uint32:100000"

expect_run_contains \
    "flt32 interpolation" \
    "$FV/template_literal_primitives_full.npk" \
    "flt32:3.14"

expect_run_contains \
    "flt64 interpolation" \
    "$FV/template_literal_primitives_full.npk" \
    "flt64:6.28"

expect_run_contains \
    "bool true interpolation" \
    "$FV/template_literal_primitives_full.npk" \
    "bool:true"

expect_run_contains \
    "bool false interpolation" \
    "$FV/template_literal_primitives_full.npk" \
    "bool:false"

expect_run_contains \
    "string interpolation" \
    "$FV/template_literal_primitives_full.npk" \
    "string:hello"

# ── Enum Display ────────────────────────────────────────────────────────────────
echo "-- Enum Display --"

expect_compile_ok \
    "enum with Display compiles" \
    "$FV/template_literal_enum_display.npk"

expect_run_contains \
    "enum Display: Red variant" \
    "$FV/template_literal_enum_display.npk" \
    "Color: Red"

expect_run_contains \
    "enum Display: Green variant" \
    "$FV/template_literal_enum_display.npk" \
    "Color: Green"

expect_run_contains \
    "enum Display: Blue variant" \
    "$FV/template_literal_enum_display.npk" \
    "Color: Blue"

expect_run_contains \
    "enum Display: two enums in one literal" \
    "$FV/template_literal_enum_display.npk" \
    "Mixed: Red and Blue"

echo ""
echo "=== Display Tests: $PASS/$TOTAL passed ==="

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
