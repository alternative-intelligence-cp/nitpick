#!/bin/bash
# Nitpick Bug Regression Tests — v0.22.5
# POLISH-009: \xNN hex escapes and \u{NNNN} unicode escapes in string literals

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
NPKC="${REPO_ROOT}/build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0225_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_compile_run() {
    local label="$1"
    local file="$2"
    local out
    out=$(cd "$REPO_ROOT" && "$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local compile_rc=$?
    if [ $compile_rc -ne 0 ]; then
        fail "$label" "compile failed (exit $compile_rc): $out"
        rm -f "$TMP_BIN"; return
    fi
    "$TMP_BIN" >/dev/null 2>&1
    local run_rc=$?
    rm -f "$TMP_BIN"
    if [ $run_rc -eq 0 ]; then
        pass "$label"
    else
        fail "$label" "program exited $run_rc"
    fi
}

expect_compile_fail() {
    local label="$1"
    local src="$2"
    local expected_msg="$3"
    local tmpfile="/tmp/npk_bug_err_test.npk"
    echo "$src" > "$tmpfile"
    local out
    out=$(cd "$REPO_ROOT" && "$NPKC" "$tmpfile" -o "$TMP_BIN" 2>&1)
    local compile_rc=$?
    rm -f "$TMP_BIN" "$tmpfile"
    if [ $compile_rc -eq 0 ]; then
        fail "$label" "expected compile error but it succeeded"
    elif echo "$out" | grep -q "$expected_msg"; then
        pass "$label"
    else
        fail "$label" "expected '$expected_msg' in error output; got: $out"
    fi
}

echo "=== Bug Regression Tests (v0.22.5: bug117–bug119) ==="
echo ""

# POLISH-009: \xNN hex escape in strings and template literals
expect_compile_run \
    "bug117: POLISH-009 — \\xNN hex escapes in strings and template literals" \
    "tests/bugs/bug117_hex_escape.npk"

# POLISH-009: \u{NNNN} unicode escape — ASCII, 2/3/4-byte UTF-8
expect_compile_run \
    "bug118: POLISH-009 — \\u{NNNN} unicode escapes (ASCII, ©, €, 😀)" \
    "tests/bugs/bug118_unicode_escape.npk"

# POLISH-009: bad escape → compile error (bug119 error cases)
expect_compile_fail \
    'bug119a: POLISH-009 — \xGG (non-hex digits) → compile error' \
    'func:main = int32() { string:s = "\xGG"; exit 0; }; func:failsafe = int32(tbb32:e) { exit 99; };' \
    "requires exactly 2 hex digits"

expect_compile_fail \
    'bug119b: POLISH-009 — \u1234 (missing braces) → compile error' \
    'func:main = int32() { string:s = "\u1234"; exit 0; }; func:failsafe = int32(tbb32:e) { exit 99; };' \
    "requires '{'"

expect_compile_fail \
    'bug119c: POLISH-009 — \u{200000} (> U+10FFFF) → compile error' \
    'func:main = int32() { string:s = "\u{200000}"; exit 0; }; func:failsafe = int32(tbb32:e) { exit 99; };' \
    "exceeds U+10FFFF"

echo ""
echo "=== Bug Tests (v0.22.5): $PASS/$TOTAL passed ==="

[ "$FAIL" -gt 0 ] && exit 1
exit 0
