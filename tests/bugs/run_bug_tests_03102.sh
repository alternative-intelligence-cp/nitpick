#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.0.2
# Phase 1 async slice 2: D-8 Option A — pull `catch` keyword entirely.
#
#   bug337 — `catch` may now be used as an ordinary identifier (e.g. a
#            local variable name). Pre-v0.31.0.2 this was rejected at
#            parse time because `catch` tokenised to TOKEN_KW_CATCH.
#            Post-fix the token type is gone, so the parser sees a
#            plain IDENTIFIER and accepts the declaration.
#   bug338 — `try { ... } catch { ... }` is still rejected. With the
#            keyword pulled, both `try` and `catch` are identifiers,
#            so the generic parser rule fires (`Expected ';' after
#            expression`). This pins that the keyword removal did NOT
#            silently enable a try/catch grammar — Nitpick remains
#            Result-only (defaults / ?| / !! / pick).
#
# Pre-existing tests bug088 + bug098 (run_bug_tests_0211.sh + 0213.sh)
# continue to pass: they only assert npkc exits non-zero with `catch`
# somewhere in the diagnostic, both still true via the source-context
# line in the new error message.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03102.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

echo "== v0.31.0.2 Phase 1 async D-8 (catch pulled) =="

# bug337: must compile + run, returning exit code 7 (the value of `catch`).
{
    label="bug337 catch is a valid identifier"
    file="$SCRIPT_DIR/bug337_catch_as_identifier_pass.npk"
    rm -f "$TMP_EXE"
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    if [ $rc -ne 0 ] || [ ! -x "$TMP_EXE" ]; then
        fail "$label" "expected clean compile, exit $rc: $out"
    else
        "$TMP_EXE"; rrc=$?
        if [ $rrc -ne 7 ]; then
            fail "$label" "expected runtime exit 7, got $rrc"
        else
            pass "$label"
        fi
    fi
    rm -f "$TMP_EXE"
}

# bug338: must fail compile.
{
    label="bug338 try/catch syntax still rejected"
    file="$SCRIPT_DIR/bug338_try_catch_still_rejected_fail.npk"
    rm -f "$TMP_EXE"
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    rc=$?
    if [ $rc -eq 0 ]; then
        fail "$label" "expected compile failure, got exit 0"
    else
        pass "$label"
    fi
    rm -f "$TMP_EXE"
}

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
