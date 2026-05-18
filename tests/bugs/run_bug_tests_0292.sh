#!/bin/bash
# Nitpick Bug Regression Tests — v0.29.2 DROP-DEC-003
#
#   bug285 — Drop impl on inner-block local: compiles, runs (exit 0),
#            emitted LLVM IR contains a `@Marker_drop` call.
#   bug286 — struct without Drop impl: compiles, runs (exit 0), and
#            emitted IR contains NO `*_drop` call (no overreach).
#   bug287 — reverse declaration order (LIFO): IR has `@TwoB_drop`
#            BEFORE `@TwoA_drop`.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0292.exe"
TMP_LL="/tmp/npk_bug_test_0292.ll"

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

echo "== v0.29.2 DROP-DEC-003 — Drop codegen, leaf cases =="

if compile_run_ir "bug285 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug285_drop_runs_at_scope_exit_pass.npk" "0"; then
    pass "bug285 compile+run (exit 0)"
    if grep -q '@Marker_drop' "$TMP_LL" 2>/dev/null; then
        pass "bug285 IR contains @Marker_drop call"
    else
        fail "bug285 IR contains @Marker_drop call" \
             "no @Marker_drop reference in emitted IR"
    fi
fi

if compile_run_ir "bug286 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug286_no_drop_impl_no_autocall_pass.npk" "0"; then
    pass "bug286 compile+run (exit 0)"
    if grep -E 'call[^@]*@[A-Za-z_][A-Za-z0-9_]*_drop' "$TMP_LL" >/dev/null 2>&1; then
        fail "bug286 no spurious _drop calls" \
             "found unexpected _drop call in IR"
    else
        pass "bug286 no spurious _drop calls"
    fi
fi

if compile_run_ir "bug287 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug287_drop_reverse_order_pass.npk" "0"; then
    pass "bug287 compile+run (exit 0)"
    line_b=$(grep -nE 'call[^@]*@TwoB_drop\b' "$TMP_LL" 2>/dev/null | head -n1 | cut -d: -f1)
    line_a=$(grep -nE 'call[^@]*@TwoA_drop\b' "$TMP_LL" 2>/dev/null | head -n1 | cut -d: -f1)
    if [ -z "$line_a" ] || [ -z "$line_b" ]; then
        fail "bug287 reverse-order LIFO drops" \
             "missing @TwoA_drop or @TwoB_drop call (a=$line_a b=$line_b)"
    elif [ "$line_b" -lt "$line_a" ]; then
        pass "bug287 reverse-order LIFO drops (B@$line_b before A@$line_a)"
    else
        fail "bug287 reverse-order LIFO drops" \
             "@TwoB_drop at line $line_b not before @TwoA_drop at line $line_a"
    fi
fi

rm -f "$TMP_EXE" "$TMP_LL"

echo ""
echo "v0.29.2 results: $PASS/$TOTAL passed, $FAIL failed"

if [ $FAIL -eq 0 ]; then exit 0; else exit 1; fi
