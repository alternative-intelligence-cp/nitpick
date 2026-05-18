#!/bin/bash
# Nitpick Bug Regression Tests — v0.29.7 Drop ordering, early pass/fail/return
#
# v0.29.7 wires `executeAllScopeDrops()` into the PASS / FAIL / RETURN
# code paths in `ir_generator.cpp`. Scope drops now fire on early-exit
# (not just block fall-through), preserving reverse declaration order
# within and across nested scopes, BEFORE `executeFunctionDefers()` is
# invoked (DROP-DEC-010). Hard `exit N` continues to skip drops
# (DROP-DEC-008). For `pass v` / `return v` where `v` is a bare
# identifier referring to a RAII binding, that binding's drop is
# SKIPPED — ownership is moved to the caller (DROP-DEC-004).
#
#   bug301 — `pass 0i32;` in helper with wildx RAII binding:
#            @do_pass body contains 1 `npk_wildx_free` call.
#   bug302 — `fail 7i32;` in helper with wildx RAII binding:
#            @do_fail body contains 1 `npk_wildx_free` call. Caller
#            captures via Result<int32> and exits 0 on is_error.
#   bug303 — `exit 0;` with wildx RAII binding (DROP-DEC-008):
#            @main body contains 0 `npk_wildx_free` calls (drops
#            skipped on hard exit).
#   bug304 — `pass p;` where p is a wildx RAII binding (DROP-DEC-004
#            move semantics): @make_page body contains 0
#            `npk_wildx_free` calls (drop transferred to caller).
#   bug305 — `pass` from inner block runs drops for outer + inner
#            scopes (innermost-out): @do_pass body contains 2
#            `npk_wildx_free` calls.
#   bug306 — Reverse order on `pass`: two RAII bindings in same scope,
#            @do_pass body contains 2 `npk_wildx_free` calls.
#   bug307 — DROP-DEC-010: drops run BEFORE defers on `pass`:
#            @do_pass body contains 1 `npk_wildx_free` call.
#   bug308 — `return Result{...}` (literal) still runs drops on the
#            RETURN code path: @do_return body contains 1
#            `npk_wildx_free` call.
#   bug309 — `fail` from inner block runs drops for outer + inner
#            scopes: @do_fail body contains 2 `npk_wildx_free` calls.
#   bug310 — `failsafe` runs AFTER drops on the fail path:
#            @do_fail body contains 1 `npk_wildx_free` call.
#            Runtime: process exits 7 (Result<int32> path).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0297.exe"
TMP_LL="/tmp/npk_bug_test_0297.ll"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# Count `npk_wildx_free` calls inside a specific function's body in
# the emitted LLVM IR, so we ignore calls inside stdlib helpers.
fn_wildx_free_count() {
    local ll="$1"; local fn="$2"
    sed -n "/define[^@]*@${fn}(/,/^}\$/p" "$ll" \
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

assert_count() {
    local label="$1"; local fn="$2"; local op="$3"; local target="$4"
    local cnt
    cnt=$(fn_wildx_free_count "$TMP_LL" "$fn")
    case "$op" in
        eq)
            if [ "$cnt" -eq "$target" ]; then
                pass "$label (@${fn} wildx_free=$cnt)"
            else
                fail "$label" "@${fn} wildx_free=$cnt, expected =$target"
            fi
            ;;
        ge)
            if [ "$cnt" -ge "$target" ]; then
                pass "$label (@${fn} wildx_free=$cnt)"
            else
                fail "$label" "@${fn} wildx_free=$cnt, expected >=$target"
            fi
            ;;
    esac
}

echo "== v0.29.7 — drops on pass / fail / return early-exit paths =="

# bug301 — pass with one wildx RAII binding
if compile_run_ir "bug301 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug301_drop_on_pass_pass.npk" "0"; then
    pass "bug301 compile+run (exit 0)"
    assert_count "bug301 @do_pass has wildx_free" "do_pass" ge 1
fi

# bug302 — fail with one wildx RAII binding
if compile_run_ir "bug302 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug302_drop_on_fail_pass.npk" "0"; then
    pass "bug302 compile+run (exit 0)"
    assert_count "bug302 @do_fail has wildx_free" "do_fail" ge 1
fi

# bug303 — DROP-DEC-008: hard exit skips drops
if compile_run_ir "bug303 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug303_drop_skipped_on_exit_pass.npk" "0"; then
    pass "bug303 compile+run (exit 0)"
    assert_count "bug303 @main has NO wildx_free (DROP-DEC-008)" "main" eq 0
fi

# bug304 — DROP-DEC-004: pass <ident> moves, drop skipped
if compile_run_ir "bug304 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug304_pass_move_skips_drop_pass.npk" "0"; then
    pass "bug304 compile+run (exit 0)"
    assert_count "bug304 @make_page has NO wildx_free (move on pass)" "make_page" eq 0
fi

# bug305 — pass from inner block runs drops for both scopes
if compile_run_ir "bug305 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug305_drop_multi_scope_pass.npk" "0"; then
    pass "bug305 compile+run (exit 0)"
    assert_count "bug305 @do_pass has >=2 wildx_free (outer+inner)" "do_pass" ge 2
fi

# bug306 — reverse order on pass: two bindings, two frees
if compile_run_ir "bug306 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug306_drop_reverse_order_pass.npk" "0"; then
    pass "bug306 compile+run (exit 0)"
    assert_count "bug306 @do_pass has >=2 wildx_free" "do_pass" ge 2
fi

# bug307 — DROP-DEC-010: drops before defers on pass
if compile_run_ir "bug307 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug307_drop_before_defer_pass.npk" "0"; then
    pass "bug307 compile+run (exit 0)"
    assert_count "bug307 @do_pass has wildx_free (DROP-DEC-010)" "do_pass" ge 1
fi

# bug308 — RETURN path runs drops too (Result literal)
if compile_run_ir "bug308 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug308_return_drop_pass.npk" "0"; then
    pass "bug308 compile+run (exit 0)"
    assert_count "bug308 @do_return has wildx_free" "do_return" ge 1
fi

# bug309 — fail from inner block runs drops for both scopes
if compile_run_ir "bug309 compile+run (exit 0)" \
        "$SCRIPT_DIR/bug309_drop_fail_in_nested_pass.npk" "0"; then
    pass "bug309 compile+run (exit 0)"
    assert_count "bug309 @do_fail has >=2 wildx_free (outer+inner)" "do_fail" ge 2
fi

# bug310 — drops fire before failsafe-bound fail
if compile_run_ir "bug310 compile+run (exit 7)" \
        "$SCRIPT_DIR/bug310_failsafe_runs_after_drop_pass.npk" "7"; then
    pass "bug310 compile+run (exit 7)"
    assert_count "bug310 @do_fail has wildx_free" "do_fail" ge 1
fi

rm -f "$TMP_EXE" "$TMP_LL"

echo ""
echo "v0.29.7 results: $PASS/$TOTAL passed, $FAIL failed"

if [ $FAIL -eq 0 ]; then exit 0; else exit 1; fi
