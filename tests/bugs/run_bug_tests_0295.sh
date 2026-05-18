#!/bin/bash
# Nitpick Bug Regression Tests — v0.29.5 DROP-DEC-007 (HandleArena RAII opt-in)
#
#   bug294 — `use "drop.npk".*;` + `int64:a = HandleArena.create();`
#            compiles, runs (exit 0), and emitted LLVM IR contains a
#            `npk_handle_arena_destroy` call.
#   bug295 — two arena bindings A1 then A2 inside the same inner block:
#            IR contains >=2 `npk_handle_arena_destroy` calls; runtime
#            exits 0.
#   bug296 — NO `use "drop.npk"`: arena binding is NOT auto-destroyed;
#            explicit `HandleArena.destroy(a)` still required and works
#            (proves opt-in does not affect pre-RAII behavior).
#   bug297 — RAII enabled, but ARIA-032 still fires when a handle from
#            a local arena would be returned to the caller.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0295.exe"
TMP_LL="/tmp/npk_bug_test_0295.ll"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

compile_run_ir() {
    local label="$1"; local file="$2"; local expected="$3"
    rm -f "$TMP_EXE" "$TMP_LL"
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
    "$NPKC" "$file" --emit-llvm -o "$TMP_LL" >/dev/null 2>&1
    if [ ! -s "$TMP_LL" ]; then
        local alt="${file%.npk}.ll"
        if [ -s "$alt" ]; then cp "$alt" "$TMP_LL"; fi
    fi
    return 0
}

echo "== v0.29.5 DROP-DEC-007 — HandleArena RAII opt-in =="

if compile_run_ir "bug294 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug294_handle_arena_raii_auto_destroy_pass.npk" "0"; then
    pass "bug294 compile+run (exit 0)"
    if grep -qE 'call[^@]*@npk_handle_arena_destroy\b' "$TMP_LL" 2>/dev/null; then
        pass "bug294 IR contains npk_handle_arena_destroy call"
    else
        fail "bug294 IR contains npk_handle_arena_destroy call" \
             "no npk_handle_arena_destroy reference in emitted IR"
    fi
fi

if compile_run_ir "bug295 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug295_handle_arena_raii_reverse_order_pass.npk" "0"; then
    pass "bug295 compile+run (exit 0)"
    hd_count=$(grep -cE 'call[^@]*@npk_handle_arena_destroy\b' "$TMP_LL" 2>/dev/null)
    if [ "$hd_count" -ge 2 ]; then
        pass "bug295 IR contains >=2 npk_handle_arena_destroy calls ($hd_count)"
    else
        fail "bug295 IR contains >=2 npk_handle_arena_destroy calls" \
             "found only $hd_count npk_handle_arena_destroy call(s) in IR"
    fi
fi

# bug296 — without drop.npk import, compiler must NOT auto-destroy; only
# the explicit `HandleArena.destroy(a)` call should appear (1 occurrence).
if compile_run_ir "bug296 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug296_no_handle_arena_raii_without_import_pass.npk" "0"; then
    pass "bug296 compile+run (exit 0)"
    hd_count=$(grep -cE 'call[^@]*@npk_handle_arena_destroy\b' "$TMP_LL" 2>/dev/null)
    if [ "$hd_count" -eq 1 ]; then
        pass "bug296 IR contains exactly 1 npk_handle_arena_destroy call (no auto-inject)"
    else
        fail "bug296 IR contains exactly 1 npk_handle_arena_destroy call" \
             "expected 1 (the manual call) but found $hd_count"
    fi
fi

# bug297 — expected FAIL: RAII on, but ARIA-032 must still fire when a
# handle from a local arena would be returned to the caller.
TOTAL=$((TOTAL+1))
cerr=$("$NPKC" "$SCRIPT_DIR/bug297_handle_arena_raii_aria032_still_fires.npk" \
        -o "$TMP_EXE" 2>&1)
if echo "$cerr" | grep -q 'ARIA-032'; then
    PASS=$((PASS+1))
    echo "  PASS: bug297 ARIA-032 fires even with RAII enabled"
else
    FAIL=$((FAIL+1))
    echo "  FAIL: bug297 ARIA-032 fires even with RAII enabled — expected ARIA-032 diagnostic, got: $cerr"
fi

rm -f "$TMP_EXE" "$TMP_LL"

echo ""
echo "v0.29.5 results: $PASS/$TOTAL passed, $FAIL failed"

if [ $FAIL -eq 0 ]; then exit 0; else exit 1; fi
