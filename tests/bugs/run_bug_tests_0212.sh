#!/bin/bash
# Nitpick Bug Regression Tests — v0.21.2
# Runs bugs/bug089–bug093 (A-003 async borrow warnings, A-004 disjoint).
# Files ending _pass.npk must compile (warnings OK) and exit 0 when run.
# Files ending _warn.npk must compile successfully (warnings are non-fatal).
# Files ending _fail.npk must fail to compile (non-zero exit from npkc).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0
TMP_BIN="/tmp/npk_bug_test_0212_bin"

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

# expect_compile_run: file should compile AND produce exit 0 when run
expect_compile_run() {
    local label="$1"
    local file="$2"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local compile_rc=$?
    if [ $compile_rc -ne 0 ]; then
        fail "$label" "compile failed (exit $compile_rc): $out"
        rm -f "$TMP_BIN"; return
    fi
    "$TMP_BIN" >/dev/null 2>&1
    local run_rc=$?
    rm -f "$TMP_BIN"
    if [ $run_rc -eq 0 ]; then
        pass "$label"
    else
        fail "$label" "program exited $run_rc"
    fi
}

# expect_compile_warn: file should compile successfully AND emit the expected
# warning code in stdout/stderr (warnings are non-fatal in the compiler).
expect_compile_warn() {
    local label="$1"
    local file="$2"
    local expect_code="$3"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local compile_rc=$?
    rm -f "$TMP_BIN"
    if [ $compile_rc -ne 0 ]; then
        fail "$label" "compile failed (exit $compile_rc) — expected warning only: $out"
        return
    fi
    if ! echo "$out" | grep -qi "$expect_code"; then
        fail "$label" "compiled but expected warning '$expect_code' not in: $out"
    else
        pass "$label"
    fi
}

# expect_compile_fail: file should fail to compile
expect_compile_fail() {
    local label="$1"
    local file="$2"
    local expect_msg="$3"
    local out
    out=$("$NPKC" "$file" -o "$TMP_BIN" 2>&1)
    local rc=$?
    rm -f "$TMP_BIN"
    if [ $rc -eq 0 ]; then
        fail "$label" "expected compile failure but got exit 0"
    elif [ -n "$expect_msg" ] && ! echo "$out" | grep -qi "$expect_msg"; then
        fail "$label" "compile failed (good) but expected msg '$expect_msg' not in: $out"
    else
        pass "$label"
    fi
}

echo "=== Bug Regression Tests (v0.21.2: bug089–bug093) ==="
echo ""

# A-003: Async borrow checking
# NOTE: v0.31.0.4 (D-5) made the await-borrow contract stricter — borrows
# (both $$m AND $$i) do NOT survive an `await` suspension. ARIA-030 (warning)
# was superseded by ARIA-041 (hard error). bug089/bug091 below were rewritten
# in v0.31.0.4 to assert the new contract.
expect_compile_fail \
    "bug089: $$m borrow live across await → ARIA-041 (was ARIA-030 warning in v0.21.2)" \
    "$SCRIPT_DIR/bug089_await_mutable_borrow_warn.npk" \
    "ARIA-041"

expect_compile_run \
    "bug090: $$m borrow released before await — no diagnostic" \
    "$SCRIPT_DIR/bug090_await_borrow_released_pass.npk"

expect_compile_fail \
    "bug091: $$i borrow across await → ARIA-041 (was pass in v0.21.2; D-5 now invalidates immutable too)" \
    "$SCRIPT_DIR/bug091_await_imm_borrow_pass.npk" \
    "ARIA-041"

# A-004: Disjoint index proofs
expect_compile_run \
    "bug092: two literal-indexed $$m borrows are disjoint (fast path)" \
    "$SCRIPT_DIR/bug092_disjoint_literal_borrows_pass.npk"

expect_compile_fail \
    "bug093: dynamic $$m borrows without Rules — ARIA-023 fires" \
    "$SCRIPT_DIR/bug093_dynamic_borrow_overlap_fail.npk" \
    "ARIA-023"

echo ""
echo "=== Bug Tests (v0.21.2): $PASS/$TOTAL passed ==="

[ "$FAIL" -gt 0 ] && exit 1
exit 0
