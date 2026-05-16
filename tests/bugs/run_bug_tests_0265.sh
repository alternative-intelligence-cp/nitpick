#!/bin/bash
# Nitpick Bug Regression Tests — v0.26.5 (MEM-013 wild/wildx ↔ GC interop)
#
# Verifies the MEM-013 invariants:
#   bug230 — npk_gc_is_heap_pointer_i32 cleanly partitions gc-heap
#            addresses (returns 1) from wild-allocator addresses
#            (returns 0). This is the ground-truth primitive the GC
#            mark phase uses to decide whether to follow a reference.
#   bug231 — a `gc` binding survives ~5000 wild alloc/free cycles
#            interleaved with `npk_gc_safepoint()` calls. Wild
#            allocations must not perturb gc reachability.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0265.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

run_test() {
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

echo "== v0.26.5 MEM-013 (wild ↔ gc interop) =="

run_test "bug230 gc/wild heap partition" \
    "$SCRIPT_DIR/bug230_gc_wild_heap_partition.npk" \
    "0"

run_test "bug231 gc/wild coexist under churn" \
    "$SCRIPT_DIR/bug231_gc_wild_coexist_under_churn.npk" \
    "0"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
