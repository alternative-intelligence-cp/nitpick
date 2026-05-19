#!/bin/bash
# Nitpick Bug Regression Tests — v0.30.2
# Transitive arena escape: a function returning a handle bound to
# one of its arena parameters is now summarised in
# `escapes_param_arena_indices`. Caller sites that pass a LOCAL
# arena to such a callee leak the handle past the arena's frame —
# v0.30.2 catches this with an ARIA-032 WARNING per IPC-DEC-004.
#
#   bug316 — `pass wrap(a)` direct callee-return form.
#   bug317 — depth-2 wrap2 -> wrap fixpoint lift.
#   bug318 — local binding then `pass h` form (transitive_handles_).
#   bug319 — wrapper that allocates+frees internally; no escape, no
#            warning.
#   bug320 — caller threads its OWN arena param through; legitimate
#            use, no warning.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0302.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_compile_warn() {
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

echo "== v0.30.2 ARIA-032 transitive arena escape =="

expect_compile_warn "bug316 pass wrap(a) direct form warns" \
    "$SCRIPT_DIR/bug316_handle_transitive_escape_pass_call_warns.npk" \
    "ARIA-032"

expect_compile_warn "bug317 depth-2 wrap2 -> wrap fixpoint warns" \
    "$SCRIPT_DIR/bug317_handle_transitive_escape_depth2_warns.npk" \
    "ARIA-032"

expect_compile_warn "bug318 local binding pass h form warns" \
    "$SCRIPT_DIR/bug318_handle_transitive_escape_local_binding_warns.npk" \
    "ARIA-032"

expect_compile_clean_no_aria032 "bug319 wrapper allocs+frees internally — no escape, no warn" \
    "$SCRIPT_DIR/bug319_handle_noop_wrapper_no_escape_pass.npk"

expect_compile_clean_no_aria032 "bug320 caller threads its own arena param — legitimate, no warn" \
    "$SCRIPT_DIR/bug320_handle_threaded_arena_no_warn_pass.npk"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
