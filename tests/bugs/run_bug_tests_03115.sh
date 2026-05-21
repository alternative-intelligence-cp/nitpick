#!/bin/bash
# Nitpick Bug Regression Tests — v0.31.1.5 Phase 2 D-10.
#
# D-10: `obj` keyword retired (ratified Option A of AUDIT_v0.31.1.4).
# The `obj` token was an active trap — parser accepted it in type
# position, sema rejected every initializer with a confusing
# diagnostic. The archived anonymous-record vision in
# META/NITPICK/archive/specs/aria_specs.txt was never implemented and
# conflicts with current Aria philosophy (comptime-shape, tagged
# sums, dyn Trait).
#
# v0.31.1.5 removes TOKEN_KW_OBJ from lexer/parser/ast/printer and
# the four parser escape-hatches that allowed `obj` as an identifier.
# `obj` now lexes as TOKEN_IDENTIFIER and is a plain user-defined
# name.
#
# See META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.4.md (ratification) and
#     META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.5.md (implementation).
#
# Fixtures:
#   bug368 — `obj` as local-variable name: exit 42.
#   bug369 — `obj` as function name and parameter name: exit 7.
#   bug370 — `obj` as struct field name: exit 99.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_03115.exe"

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

echo "== v0.31.1.5 D-10 — obj keyword retired, identifier reuse OK =="

expect_compile_run "bug368 obj as local var name (exit 42)" \
    "$SCRIPT_DIR/bug368_obj_as_identifier_pass.npk" "42"

expect_compile_run "bug369 obj as func name and param name (exit 7)" \
    "$SCRIPT_DIR/bug369_obj_as_func_and_param_name_pass.npk" "7"

expect_compile_run "bug370 obj as struct field name (exit 99)" \
    "$SCRIPT_DIR/bug370_obj_as_struct_field_pass.npk" "99"

echo
echo "Summary: $PASS/$TOTAL passed, $FAIL failed"
if [ "$FAIL" -gt 0 ]; then exit 1; fi
exit 0
