#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.1.10 Phase 2 D-12 sub-slice 4.
#
# Sub-slice 4 closes Probe D (borrow-checker liveness for dyn
# borrows) as a *regression slice* rather than a new code path:
# investigation showed that with Probes B/C/E composed
# (v0.31.1.7/8/9), the borrow checker already records dyn-target
# borrows at VAR_DECL time and the standard source-side conflict
# rules (ARIA-023 mut/mut, ARIA-026 assign-to-borrowed,
# ARIA-019 move-while-borrowed) fire correctly when the borrow
# target type is `dyn Trait`. The deferral note from v0.31.1.8
# was conservative; no new code is required to flag these
# conflicts.
#
# This slice codifies that surface as fixtures so any future
# regression is caught immediately.
#
# Fixtures:
#   bug375 — two $$i dyn borrows OK, exit 14 (positive).
#   bug376 — $$m dyn live, direct write → ARIA-026 (negative).
#   bug377 — two $$m dyn of same source → ARIA-023 (negative).
#   bug378 — $$i dyn live, $$m dyn param pass → ARIA-023
#            (negative; exercises the v0.31.1.9 call-site path).
#
# See:
#   META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.10.md

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03110.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_compile_run() {
    local label="$1"; local file="$2"; local expected="$3"
    rm -f "$TMP_EXE"
    local cerr
    cerr=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    if [ ! -x "$TMP_EXE" ]; then
        fail "$label" "compile failed: $cerr"
        return 1
    fi
    "$TMP_EXE" 2>/dev/null
    local actual=$?
    if [ "$actual" != "$expected" ]; then
        fail "$label" "expected exit $expected but got $actual"
        return 1
    fi
    pass "$label"
}

expect_compile_fail_with() {
    local label="$1"; local file="$2"; local needle="$3"
    rm -f "$TMP_EXE"
    local out
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    local rc=$?
    if [ -x "$TMP_EXE" ] && [ "$rc" = "0" ]; then
        fail "$label" "expected compile failure, but compile succeeded"
        return 1
    fi
    if ! echo "$out" | grep -q "$needle"; then
        fail "$label" "expected diagnostic '$needle' not found in: $out"
        return 1
    fi
    pass "$label"
}

echo "== v0.31.1.10 D-12 sub-slice 4 — Probe D borrow-checker liveness =="

expect_compile_run "bug375 two \$\$i dyn borrows OK (exit 14)" \
    "$SCRIPT_DIR/bug375_dyn_borrow_two_shared_pass.npk" "14"

expect_compile_fail_with "bug376 \$\$m dyn live + source write → ARIA-026" \
    "$SCRIPT_DIR/bug376_dyn_borrow_mut_source_write_fail.npk" "ARIA-026"

expect_compile_fail_with "bug377 two \$\$m dyn rejected → ARIA-023" \
    "$SCRIPT_DIR/bug377_dyn_borrow_two_mut_fail.npk" "ARIA-023"

expect_compile_fail_with "bug378 \$\$i dyn live + \$\$m param pass → ARIA-023" \
    "$SCRIPT_DIR/bug378_dyn_borrow_shared_then_mut_param_fail.npk" "ARIA-023"

echo
echo "Summary: $PASS/$TOTAL passed, $FAIL failed"
if [ "$FAIL" -gt 0 ]; then exit 1; fi
exit 0
