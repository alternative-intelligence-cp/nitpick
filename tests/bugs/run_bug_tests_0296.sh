#!/bin/bash
# Nitpick Bug Regression Tests — v0.29.6 DROP-DEC-007 (JitFn RAII opt-in)
#
#   bug298 — `use "drop.npk".*;` + `wildx int8->:f = Jit.compile_add_i32();`
#            compiles, runs (exit 0), and emitted LLVM IR contains a
#            `npk_wildx_free` call (the auto-injected one).
#   bug299 — two JIT bindings F1 then F2 inside the same inner block:
#            IR contains >=2 `npk_wildx_free` calls; runtime exits 0.
#   bug300 — NO `use "drop.npk"`: JIT binding is NOT auto-freed;
#            explicit `Jit.free(f)` still required and works (proves
#            opt-in does not affect pre-RAII behavior). IR has exactly
#            1 `npk_wildx_free` call (the manual one).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0296.exe"
TMP_LL="/tmp/npk_bug_test_0296.ll"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# Extract the body of `@main` from the LLVM IR so wildx_free counts only
# include calls actually emitted into user code (not the ones inside
# `Jit_compile_add_i32`'s error paths inside jit.npk).
main_wildx_free_count() {
    local ll="$1"
    sed -n '/define[^@]*@main(/,/^}$/p' "$ll" \
        | grep -cE 'call[^@]*@npk_wildx_free\b'
}

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

echo "== v0.29.6 DROP-DEC-007 — JitFn RAII opt-in =="

if compile_run_ir "bug298 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug298_jit_fn_raii_auto_free_pass.npk" "0"; then
    pass "bug298 compile+run (exit 0)"
    wf_count=$(main_wildx_free_count "$TMP_LL")
    if [ "$wf_count" -ge 1 ]; then
        pass "bug298 @main IR contains auto-injected npk_wildx_free ($wf_count)"
    else
        fail "bug298 @main IR contains auto-injected npk_wildx_free" \
             "no npk_wildx_free call in @main body"
    fi
fi

if compile_run_ir "bug299 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug299_jit_fn_raii_reverse_order_pass.npk" "0"; then
    pass "bug299 compile+run (exit 0)"
    wf_count=$(main_wildx_free_count "$TMP_LL")
    if [ "$wf_count" -ge 2 ]; then
        pass "bug299 @main IR contains >=2 npk_wildx_free calls ($wf_count)"
    else
        fail "bug299 @main IR contains >=2 npk_wildx_free calls" \
             "found only $wf_count npk_wildx_free call(s) in @main"
    fi
fi

# bug300 — without drop.npk import, compiler must NOT auto-free.
# The user's explicit `Jit.free(f)` is itself a function call to the
# Aria-defined `Jit_free` (which then calls `npk_wildx_free` inside
# its own body, NOT in @main). So @main contains 0 `npk_wildx_free`
# calls when the auto-inject is off — that's the cleanest proof the
# RAII gate stayed quiet without the import.
if compile_run_ir "bug300 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug300_no_jit_fn_raii_without_import_pass.npk" "0"; then
    pass "bug300 compile+run (exit 0)"
    wf_count=$(main_wildx_free_count "$TMP_LL")
    if [ "$wf_count" -eq 0 ]; then
        pass "bug300 @main IR contains 0 npk_wildx_free calls (no auto-inject)"
    else
        fail "bug300 @main IR contains 0 npk_wildx_free calls" \
             "expected 0 (user's Jit.free is a function call, not inlined) but found $wf_count"
    fi
fi

rm -f "$TMP_EXE" "$TMP_LL"

echo ""
echo "v0.29.6 results: $PASS/$TOTAL passed, $FAIL failed"

if [ $FAIL -eq 0 ]; then exit 0; else exit 1; fi
