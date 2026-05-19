#!/bin/bash
# Nitpick Bug Regression Tests — v0.30.3
# Cross-module function summary ingest (IPC-DEC-002 / IPC-DEC-003).
# The borrow checker now seeds `func_summaries` from all imported
# module ASTs before analysing the main translation unit. This
# retires the bug277 local-re-declaration workaround.
#
#   bug277 (modified) — extern declared ONLY in handle.npk;
#       ARIA-032 still fires at the direct-pass call site, proving
#       cross-module summary visibility.
#   bug321            — local re-declaration of the same extern
#       still compiles cleanly with exactly one ARIA-032 warning
#       (local wins on collision; no duplicate-extern error).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0303.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# Expect: clean compile, executable produced, output contains exactly
# `$want_count` occurrences of `$want_msg` (case-insensitive).
expect_compile_warn_count() {
    local label="$1"
    local file="$2"
    local want_msg="$3"
    local want_count="$4"

    rm -f "$TMP_EXE"
    local out rc count
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    if [ $rc -ne 0 ] || [ ! -x "$TMP_EXE" ]; then
        fail "$label" "expected clean compile (warning only) but got exit $rc: $out"
        rm -f "$TMP_EXE"
        return
    fi
    count=$(echo "$out" | grep -ci "$want_msg")
    if [ "$count" != "$want_count" ]; then
        fail "$label" "expected $want_count occurrence(s) of '$want_msg', got $count. Output: $out"
        rm -f "$TMP_EXE"
        return
    fi
    rm -f "$TMP_EXE"
    pass "$label"
}

echo "== v0.30.3 cross-module summary ingest =="

expect_compile_warn_count "bug277 cross-module extern (no local re-decl) warns ARIA-032" \
    "$SCRIPT_DIR/bug277_handle_ffi_direct_pass_warns.npk" \
    "\[ARIA-032\]" 1

expect_compile_warn_count "bug321 local re-decl shadows import, single ARIA-032" \
    "$SCRIPT_DIR/bug321_handle_local_redecl_shadow_pass.npk" \
    "\[ARIA-032\]" 1

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
