#!/bin/bash
# Nitpick Bug Regression Tests — v0.22.3
# POLISH-003: get_argc() / get_argv(i) builtins (bug112)
# POLISH-011: break and continue in loop bodies (bug113)
#
# bug112: get_argc() / get_argv() must compile, count args correctly, and
#         return the first user argument as a string via stdout.
# bug113: break in while exits early; continue in loop() skips an iteration.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
NPKC="${REPO_ROOT}/build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0223_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# expect_compile_run: file must compile AND run with exit 0
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

echo "=== Bug Regression Tests (v0.22.3: bug112–bug113) ==="
echo ""

# ============================================================
# POLISH-003: get_argc() / get_argv() builtins (bug112)
# ============================================================

# Step 1: compile bug112
COMPILE_OUT=$(cd "$REPO_ROOT" && "$NPKC" "tests/bugs/bug112_get_argc_argv.npk" \
    -o "$TMP_BIN" 2>&1)
COMPILE_RC=$?
if [ $COMPILE_RC -ne 0 ]; then
    fail "bug112: POLISH-003 — get_argc/get_argv must compile" \
         "compile failed (exit $COMPILE_RC): $COMPILE_OUT"
else
    # Step 2: run with no args → exit 0
    "$TMP_BIN" >/dev/null 2>&1
    RC0=$?
    if [ $RC0 -ne 0 ]; then
        fail "bug112: POLISH-003 — get_argc() with no args must exit 0" \
             "exited $RC0"
    else
        pass "bug112a: POLISH-003 — get_argc() == 0 with no args"
    fi

    # Step 3: run with 2 args → exit code 2, stdout = "alpha"
    STDOUT=$("$TMP_BIN" alpha beta 2>/dev/null)
    RC2=$?
    if [ $RC2 -ne 2 ]; then
        fail "bug112: POLISH-003 — get_argc() with 2 args must exit 2" \
             "exited $RC2"
    elif [ "$STDOUT" != "alpha" ]; then
        fail "bug112: POLISH-003 — get_argv(0) must return first user arg" \
             "got '${STDOUT}', expected 'alpha'"
    else
        pass "bug112b: POLISH-003 — get_argc()==2 and get_argv(0)==\"alpha\" correct"
    fi

    rm -f "$TMP_BIN"
fi

# ============================================================
# POLISH-011: break and continue  (bug113)
# ============================================================

expect_compile_run \
    "bug113: POLISH-011 — break in while and continue in loop() work correctly" \
    "tests/bugs/bug113_break_continue.npk"

echo ""
echo "=== Bug Tests (v0.22.3): $PASS/$TOTAL passed ==="

[ "$FAIL" -gt 0 ] && exit 1
exit 0
