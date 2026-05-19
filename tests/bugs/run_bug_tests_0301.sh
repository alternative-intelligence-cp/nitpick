#!/bin/bash
# Nitpick Bug Regression Tests — v0.30.1
# Transitive destroy fixpoint: lift `destroys_param_indices` through
# call summaries so handle-outlives-arena (ARIA-032) catches arenas
# destroyed indirectly by a callee-of-a-callee. Per IPC-DEC-004, new
# transitive cases ship as WARNINGS for one cycle before promotion.
#
#   bug311 — depth-2 wrapper → destroyer; handle.free after wrapper
#            triggers ARIA-032 transitive warning.
#   bug312 — depth-3 outer → middle → destroyer; fixpoint reaches
#            depth-3 in at most 3 iterations.
#   bug313 — depth-2 wrapper that does NOT destroy; handle uses
#            after the wrapper compile clean (no false positive).
#   bug314 — fan-out: callee destroys param 1 only; handle uses on
#            the param-0 arena compile clean; handle uses on the
#            param-1 arena warn.
#   bug315 — mutual recursion ping <-> pong calling an indirect
#            killer; fixpoint must terminate AND lift the destroy
#            through the cycle.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0301.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_compile_warn() {
    # Compile must succeed (exit 0 + executable produced), and the
    # given warning string must appear in stderr.
    local label="$1"
    local file="$2"
    local expect_msg="$3"

    rm -f "$TMP_EXE"
    local out rc
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    if [ $rc -ne 0 ] || [ ! -x "$TMP_EXE" ]; then
        fail "$label" "expected clean compile (warning only) but got exit $rc: $out"
        rm -f "$TMP_EXE"
        return
    fi
    if ! echo "$out" | grep -qi "$expect_msg"; then
        fail "$label" "compile succeeded but expected warning '$expect_msg' not in output: $out"
        rm -f "$TMP_EXE"
        return
    fi
    rm -f "$TMP_EXE"
    pass "$label"
}

expect_compile_clean_no_aria032() {
    # Compile must succeed AND no ARIA-032 diagnostic of any kind
    # may appear (no false positive on the negative-case fixture).
    local label="$1"
    local file="$2"

    rm -f "$TMP_EXE"
    local out rc
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    if [ $rc -ne 0 ] || [ ! -x "$TMP_EXE" ]; then
        fail "$label" "expected clean compile but got exit $rc: $out"
        rm -f "$TMP_EXE"
        return
    fi
    if echo "$out" | grep -qi "ARIA-032"; then
        fail "$label" "compile succeeded but unexpected ARIA-032 diagnostic appeared: $out"
        rm -f "$TMP_EXE"
        return
    fi
    rm -f "$TMP_EXE"
    pass "$label"
}

echo "== v0.30.1 ARIA-032 transitive destroy fixpoint =="

expect_compile_warn "bug311 depth-2 wrapper → destroyer warns transitive" \
    "$SCRIPT_DIR/bug311_handle_transitive_destroy_warns.npk" \
    "transitively"

expect_compile_warn "bug312 depth-3 outer → middle → destroyer warns transitive" \
    "$SCRIPT_DIR/bug312_handle_transitive_destroy_depth3_warns.npk" \
    "transitively"

expect_compile_clean_no_aria032 "bug313 noop wrapper — no false positive" \
    "$SCRIPT_DIR/bug313_handle_transitive_noop_wrapper_pass.npk"

expect_compile_warn "bug314 fan-out warns only on destroyed-param arena" \
    "$SCRIPT_DIR/bug314_handle_transitive_fanout_warns.npk" \
    "transitively"

expect_compile_warn "bug315 mutual-recursion fixpoint terminates + lifts" \
    "$SCRIPT_DIR/bug315_handle_transitive_recursion_warns.npk" \
    "transitively"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
