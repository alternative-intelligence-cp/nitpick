#!/bin/bash
# Nitpick Bug Regression Tests — v0.26.3.2 (GC root tracking for `gc` bindings)
#
# Locks in the v0.26.3.2 fix that wires `gc T:x = ...` bindings into the
# shadow-stack root tracker. Before v0.26.3.2 the heap pointer returned by
# npk_gc_alloc was never registered as a GC root, so the mark phase couldn't
# see it and the next minor GC would invalidate the binding.
#
# Two layers of testing:
#   bug223 (IR)    — every gc binding emits npk_shadow_stack_push_frame,
#                    npk_gc_pin, npk_shadow_stack_add_root, and a matching
#                    npk_shadow_stack_pop_frame.
#   bug223 (exec)  — a gc binding's value survives a collection cycle that
#                    runs between allocation and read (200k garbage allocs in
#                    a helper function force at least one minor GC).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_LL="/tmp/npk_bug_test_02632.ll"
TMP_EXE="/tmp/npk_bug_test_02632.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

expect_ir_match() {
    local label="$1"
    local file="$2"
    local needle="$3"

    rm -f "$TMP_LL"
    local out
    out=$("$NPKC" --emit-llvm "$file" -o "$TMP_LL" 2>&1)
    if [ ! -f "$TMP_LL" ]; then
        fail "$label" "compile failed: $out"
        return
    fi
    if ! grep -qE "$needle" "$TMP_LL"; then
        fail "$label" "expected IR to match /$needle/ but it did not"
        rm -f "$TMP_LL"
        return
    fi
    rm -f "$TMP_LL"
    pass "$label"
}

expect_exit_code() {
    local label="$1"
    local file="$2"
    local expected="$3"

    rm -f "$TMP_EXE"
    local out
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    if [ ! -x "$TMP_EXE" ]; then
        fail "$label" "compile failed: $out"
        return
    fi
    "$TMP_EXE"
    local actual=$?
    if [ "$actual" != "$expected" ]; then
        fail "$label" "expected exit $expected but got $actual"
        rm -f "$TMP_EXE"
        return
    fi
    rm -f "$TMP_EXE"
    pass "$label"
}

echo "== v0.26.3.2 GC root tracking regression tests =="

# bug223 IR: every gc binding emits the shadow-stack and pin scaffolding.
expect_ir_match "bug223 gc binding emits npk_shadow_stack_push_frame" \
    "$SCRIPT_DIR/bug223_gc_survives_collection.npk" \
    "call void @npk_shadow_stack_push_frame"

expect_ir_match "bug223 gc binding emits npk_gc_pin" \
    "$SCRIPT_DIR/bug223_gc_survives_collection.npk" \
    "call void @npk_gc_pin"

expect_ir_match "bug223 gc binding emits npk_shadow_stack_add_root" \
    "$SCRIPT_DIR/bug223_gc_survives_collection.npk" \
    "call void @npk_shadow_stack_add_root"

expect_ir_match "bug223 gc binding emits npk_shadow_stack_pop_frame" \
    "$SCRIPT_DIR/bug223_gc_survives_collection.npk" \
    "call void @npk_shadow_stack_pop_frame"

# bug223 exec: gc binding survives a forced collection cycle.
expect_exit_code "bug223 gc binding survives collection (exec)" \
    "$SCRIPT_DIR/bug223_gc_survives_collection.npk" \
    "0"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
