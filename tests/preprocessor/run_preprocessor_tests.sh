#!/bin/bash
# Nitpick Preprocessor Tests — v0.21.1
# Tests %error, %warning, %str(), ##, #%N, and all existing preprocessor features.
# v0.21.1 additions: %elif-after-%else error, duplicate %else error,
#   %undef-of-undefined warning, %define redefinition warning.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc compiler not found at $NPKC — skipping preprocessor tests"
    exit 77  # CTest skip code
fi

PASS=0
FAIL=0
TOTAL=0
TMP_OUT="/tmp/npk_prep_test_out"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# Helper: compile a file and expect success (exit 0)
expect_ok() {
    local label="$1"
    local file="$2"
    local output
    output=$("$NPKC" "$file" -o "$TMP_OUT" 2>&1)
    local rc=$?
    rm -f "$TMP_OUT"
    if [ $rc -eq 0 ]; then
        pass "$label"
    else
        fail "$label" "expected compile success but got exit $rc: $output"
    fi
}

# Helper: compile a file and expect failure (non-zero exit); optionally check stderr message
expect_fail() {
    local label="$1"
    local file="$2"
    local expect_msg="$3"
    local output rc
    output=$("$NPKC" "$file" -o "$TMP_OUT" 2>&1)
    rc=$?
    rm -f "$TMP_OUT"
    if [ $rc -eq 0 ]; then
        fail "$label" "expected compile failure but got exit 0"
    elif [ -n "$expect_msg" ] && ! echo "$output" | grep -qi "$expect_msg"; then
        fail "$label" "exit $rc but expected message '$expect_msg' not found in: $output"
    else
        pass "$label"
    fi
}

echo "=== Preprocessor Tests (v0.20.1 + v0.21.1) ==="
echo ""

# ── Existing feature regression tests ────────────────────────────────────────
expect_ok "%define / %undef" \
    "$SCRIPT_DIR/test_define_undef.npk"

expect_ok "%ifdef / %ifndef" \
    "$SCRIPT_DIR/test_ifdef_ifndef.npk"

expect_ok "%if / %elif / %else" \
    "$SCRIPT_DIR/test_if_elif_else.npk"

expect_ok "%rep / %endrep" \
    "$SCRIPT_DIR/test_rep_endrep.npk"

expect_ok "%assign arithmetic" \
    "$SCRIPT_DIR/test_assign_expr.npk"

expect_ok "%push / %pop context" \
    "$SCRIPT_DIR/test_push_pop_context.npk"

expect_ok "include guard pattern" \
    "$SCRIPT_DIR/test_include_guard.npk"

expect_ok "multi-argument macros" \
    "$SCRIPT_DIR/test_macro_args.npk"

expect_ok "nested conditionals" \
    "$SCRIPT_DIR/test_nested_conditionals.npk"

# ── New v0.20.1 feature tests ─────────────────────────────────────────────────
expect_ok "%warning directive (compile succeeds)" \
    "$SCRIPT_DIR/test_warning_directive.npk"

# %warning should emit something to stderr
OUTPUT=$("$NPKC" "$SCRIPT_DIR/test_warning_directive.npk" -o "$TMP_OUT" 2>&1 || true)
rm -f "$TMP_OUT"
if echo "$OUTPUT" | grep -qi "warning\|intentional"; then
    pass "%warning message appears in compiler output"
else
    fail "%warning message appears in compiler output" \
         "expected warning text in stderr; got: $OUTPUT"
fi

expect_ok "%error in false branch is skipped" \
    "$SCRIPT_DIR/test_error_in_false_branch.npk"

expect_fail "%error in active branch aborts compilation" \
    "$SCRIPT_DIR/test_error_fires.npk" \
    "Intentional preprocessor error"

expect_ok "## token paste operator" \
    "$SCRIPT_DIR/test_token_paste.npk"

expect_ok "#%N stringify operator" \
    "$SCRIPT_DIR/test_stringify.npk"

expect_ok "%str() inline stringify directive" \
    "$SCRIPT_DIR/test_str_directive.npk"

# ── New v0.21.1 preprocessor-hardening tests ─────────────────────────────────
expect_fail "%elif after %else is an error" \
    "$SCRIPT_DIR/test_elif_after_else.npk" \
    "elif"

expect_fail "duplicate %else is an error" \
    "$SCRIPT_DIR/test_duplicate_else.npk" \
    "duplicate.*else\|else.*duplicate"

expect_ok "%undef of undefined symbol: compile succeeds" \
    "$SCRIPT_DIR/test_undef_undefined.npk"

# %undef of undefined should emit a warning
OUTPUT=$("$NPKC" "$SCRIPT_DIR/test_undef_undefined.npk" -o "$TMP_OUT" 2>&1 || true)
rm -f "$TMP_OUT"
if echo "$OUTPUT" | grep -qi "warning\|undef\|undefined\|not defined"; then
    pass "%undef of undefined emits a warning in compiler output"
else
    fail "%undef of undefined emits a warning in compiler output" \
         "expected warning text in stderr; got: $OUTPUT"
fi

expect_ok "%define redefinition: compile succeeds" \
    "$SCRIPT_DIR/test_define_redefinition.npk"

# %define redefinition should emit a warning
OUTPUT=$("$NPKC" "$SCRIPT_DIR/test_define_redefinition.npk" -o "$TMP_OUT" 2>&1 || true)
rm -f "$TMP_OUT"
if echo "$OUTPUT" | grep -qi "warning\|redefin\|already defined"; then
    pass "%define redefinition emits a warning in compiler output"
else
    fail "%define redefinition emits a warning in compiler output" \
         "expected warning text in stderr; got: $OUTPUT"
fi

echo ""
echo "=== Preprocessor Tests (v0.20.1 + v0.21.1): $PASS/$TOTAL passed ==="

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
