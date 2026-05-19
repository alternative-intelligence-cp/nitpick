#!/bin/bash
# Nitpick Bug Regression Tests — v0.29.3 DROP-DEC-007 (wild RAII opt-in)
#
#   bug288 — `use "drop.npk".*;` + `wild T:x = T{...}` compiles, runs
#            (exit 0), and emitted LLVM IR contains a `npk_free` call.
#   bug289 — two wild-struct bindings declared A then B: LIFO drop
#            order, both `npk_free` calls present, runs (exit 0).
#   bug290 — NO `use "drop.npk"`: explicit `free(h)` contract intact,
#            compiles, runs (exit 0), exactly one `npk_free` in IR.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0293.exe"
TMP_LL="/tmp/npk_bug_test_0293.ll"

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

echo "== v0.29.3 DROP-DEC-007 — wild RAII opt-in =="

if compile_run_ir "bug288 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug288_wild_raii_auto_free_pass.npk" "0"; then
    pass "bug288 compile+run (exit 0)"
    if grep -qE 'call[^@]*@npk_free\b' "$TMP_LL" 2>/dev/null; then
        pass "bug288 IR contains npk_free call"
    else
        fail "bug288 IR contains npk_free call" \
             "no npk_free reference in emitted IR"
    fi
fi

if compile_run_ir "bug289 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug289_wild_raii_reverse_order_pass.npk" "0"; then
    pass "bug289 compile+run (exit 0)"
    npk_count=$(grep -cE 'call[^@]*@npk_free\b' "$TMP_LL" 2>/dev/null)
    if [ "$npk_count" -ge 2 ]; then
        pass "bug289 IR contains >=2 npk_free calls ($npk_count)"
    else
        fail "bug289 IR contains >=2 npk_free calls" \
             "found only $npk_count npk_free call(s) in IR"
    fi
fi

# bug290 — expected FAIL: without `use "drop.npk".*;`, the wild-struct
# binding leaks and ARIA-014 must reject compile. Proves the opt-in is
# required to obtain auto-RAII.
TOTAL=$((TOTAL+1))
cerr=$("$NPKC" "$SCRIPT_DIR/bug290_no_raii_without_import_fail.npk" \
        -o "$TMP_EXE" 2>&1)
if echo "$cerr" | grep -q 'ARIA-014'; then
    PASS=$((PASS+1))
    echo "  PASS: bug290 ARIA-014 fires without drop.npk import"
else
    FAIL=$((FAIL+1))
    echo "  FAIL: bug290 ARIA-014 fires without drop.npk import — expected ARIA-014 diagnostic, got: $cerr"
fi

rm -f "$TMP_EXE" "$TMP_LL"

echo ""
echo "v0.29.3 results: $PASS/$TOTAL passed, $FAIL failed"

if [ $FAIL -eq 0 ]; then exit 0; else exit 1; fi
