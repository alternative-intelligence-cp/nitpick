#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.0.1
# Phase 1 async slice: D-2 (compiler-injected executor drain at `exit`)
#                      + D-3 (await-outside-async = ARIA-040 hard error)
#
#   bug334 — `await` in a sync `func:main` must fail compile with
#            ARIA-040. Pre-v0.31.0.1: stderr ERROR + compile_exit=0
#            (soft-fail bug). Post-fix: codegen throws std::runtime_error
#            in codegen_expr_compound.cpp ~line 1160.
#   bug335 — module with `async func:` must emit
#            `call void @npk_executor_run(ptr null)` before the user's
#            `@exit` in main, and must still run and exit 0. Wires the
#            D-2 drain via the `module_has_async_` flag plumbed from
#            IRGenerator::codegen() into ExprCodegen::codegenCall().
#   bug336 — sync-only module must NOT mention `npk_executor_run` at
#            all in the emitted IR (zero-overhead invariant for purely
#            synchronous programs).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03101.exe"
TMP_LL="/tmp/npk_bug_test_03101.ll"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

echo "== v0.31.0.1 Phase 1 async D-2 + D-3 =="

# bug334: must fail compile, error must mention ARIA-040.
{
    label="bug334 await-outside-async = ARIA-040 hard error"
    file="$SCRIPT_DIR/bug334_await_outside_async_rejected.npk"
    rm -f "$TMP_EXE"
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    if [ $rc -eq 0 ]; then
        fail "$label" "expected non-zero compile exit, got 0"
    elif ! echo "$out" | grep -q "ARIA-040"; then
        fail "$label" "exit $rc but no ARIA-040 in diagnostics: $out"
    else
        pass "$label"
    fi
    rm -f "$TMP_EXE"
}

# bug335: must compile, IR must reference npk_executor_run, must run exit 0.
{
    label="bug335 async module emits executor drain"
    file="$SCRIPT_DIR/bug335_async_main_executor_drain_pass.npk"
    rm -f "$TMP_EXE" "$TMP_LL"
    out=$("$NPKC" "$file" --emit-llvm -o "$TMP_LL" 2>&1)
    rc=$?
    if [ $rc -ne 0 ] || [ ! -f "$TMP_LL" ]; then
        fail "$label" "IR emission failed exit $rc: $out"
    elif ! grep -q "npk_executor_run" "$TMP_LL"; then
        fail "$label" "expected npk_executor_run in IR, not present"
    else
        out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
        rc=$?
        if [ $rc -ne 0 ] || [ ! -x "$TMP_EXE" ]; then
            fail "$label" "compile failed exit $rc: $out"
        else
            "$TMP_EXE"; rrc=$?
            if [ $rrc -ne 0 ]; then
                fail "$label" "runtime exit $rrc, expected 0"
            else
                pass "$label"
            fi
        fi
    fi
    rm -f "$TMP_EXE" "$TMP_LL"
}

# bug336: sync-only module must NOT reference npk_executor_run.
{
    label="bug336 sync-only module omits executor drain"
    file="$SCRIPT_DIR/bug336_sync_only_no_executor_pass.npk"
    rm -f "$TMP_EXE" "$TMP_LL"
    out=$("$NPKC" "$file" --emit-llvm -o "$TMP_LL" 2>&1)
    rc=$?
    if [ $rc -ne 0 ] || [ ! -f "$TMP_LL" ]; then
        fail "$label" "IR emission failed exit $rc: $out"
    elif grep -q "npk_executor_run" "$TMP_LL"; then
        fail "$label" "sync-only module unexpectedly references npk_executor_run"
    else
        out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
        rc=$?
        if [ $rc -ne 0 ] || [ ! -x "$TMP_EXE" ]; then
            fail "$label" "compile failed exit $rc: $out"
        else
            "$TMP_EXE"; rrc=$?
            if [ $rrc -ne 0 ]; then
                fail "$label" "runtime exit $rrc, expected 0"
            else
                pass "$label"
            fi
        fi
    fi
    rm -f "$TMP_EXE" "$TMP_LL"
}

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
