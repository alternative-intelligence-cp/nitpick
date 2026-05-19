#!/bin/bash
# Nitpick Bug Regression Tests — v0.29.4 DROP-DEC-007 (wildx RAII opt-in)
#
#   bug291 — `use "drop.npk".*;` + `wildx int8->:p = wildx_alloc(N);`
#            compiles, runs (exit 0), and emitted LLVM IR contains a
#            `npk_wildx_free` call.
#   bug292 — two wildx bindings declared A then B inside the same inner
#            block: LIFO drop order — IR contains >=2 `npk_wildx_free`
#            calls, runs (exit 0).
#   bug293 — NO `use "drop.npk"`: wildx leak rejected at compile time
#            via ARIA-014. Proves the opt-in is required to obtain
#            auto-RAII.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0294.exe"
TMP_LL="/tmp/npk_bug_test_0294.ll"

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

echo "== v0.29.4 DROP-DEC-007 — wildx RAII opt-in =="

if compile_run_ir "bug291 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug291_wildx_raii_auto_free_pass.npk" "0"; then
    pass "bug291 compile+run (exit 0)"
    if grep -qE 'call[^@]*@npk_wildx_free\b' "$TMP_LL" 2>/dev/null; then
        pass "bug291 IR contains npk_wildx_free call"
    else
        fail "bug291 IR contains npk_wildx_free call" \
             "no npk_wildx_free reference in emitted IR"
    fi
fi

if compile_run_ir "bug292 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug292_wildx_raii_reverse_order_pass.npk" "0"; then
    pass "bug292 compile+run (exit 0)"
    wxf_count=$(grep -cE 'call[^@]*@npk_wildx_free\b' "$TMP_LL" 2>/dev/null)
    if [ "$wxf_count" -ge 2 ]; then
        pass "bug292 IR contains >=2 npk_wildx_free calls ($wxf_count)"
    else
        fail "bug292 IR contains >=2 npk_wildx_free calls" \
             "found only $wxf_count npk_wildx_free call(s) in IR"
    fi
fi

# bug293 — expected FAIL: without `use "drop.npk".*;`, a wildx binding
# without explicit `wildx_free(p)` must be rejected via ARIA-014.
TOTAL=$((TOTAL+1))
cerr=$("$NPKC" "$SCRIPT_DIR/bug293_no_wildx_raii_without_import_fail.npk" \
        -o "$TMP_EXE" 2>&1)
if echo "$cerr" | grep -q 'ARIA-014'; then
    PASS=$((PASS+1))
    echo "  PASS: bug293 ARIA-014 fires without drop.npk import"
else
    FAIL=$((FAIL+1))
    echo "  FAIL: bug293 ARIA-014 fires without drop.npk import — expected ARIA-014 diagnostic, got: $cerr"
fi

rm -f "$TMP_EXE" "$TMP_LL"

echo ""
echo "v0.29.4 results: $PASS/$TOTAL passed, $FAIL failed"

if [ $FAIL -eq 0 ]; then exit 0; else exit 1; fi
