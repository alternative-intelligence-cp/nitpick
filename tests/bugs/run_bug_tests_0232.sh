#!/usr/bin/env bash
# v0.23.2 regression tests — Macro Hygiene (MACRO-003)
# Ensure macro-introduced variables do not overwrite caller variables
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"

pass=0; fail=0

FAILSAFE='func:failsafe = int32(tbb32:e) { exit 1; };'

# ── helpers ─────────────────────────────────────────────────────────────────

expect_run() {
    local label="$1"; local file="$2"; local expected_exit="${3:-0}"
    local bin="/tmp/npk_test_${label}"
    if "$NPKC" "$file" -o "$bin" 2>/dev/null && "$bin"; ret=$?; then
        if [[ $ret -eq $expected_exit ]]; then
            echo "PASS: $label"; pass=$((pass+1))
        else
            echo "FAIL: $label — expected exit $expected_exit, got $ret"
            fail=$((fail+1))
        fi
    else
        echo "FAIL: $label — compile or run failed"
        fail=$((fail+1))
    fi
}

# ── bug124 — macro hygiene ──────────────────────────────────────────────────
# Macro-introduced variables should not overwrite caller variables

TMP124=$(mktemp /tmp/bug124_XXXXXX.npk)
cat > "$TMP124" << 'NPKEOF'
macro:swap = (a, b) {
    int32:tmp = a;
    a = b;
    b = tmp;
};

pub func:main = int32() {
    int32:tmp = 99i32;
    int32:x = 1i32;
    int32:y = 2i32;
    swap!(x, y);
    if (tmp == 99i32 && x == 2i32 && y == 1i32) {
        exit 0;
    }
    exit 1;
};
func:failsafe = int32(tbb32:e) { exit 1; };
NPKEOF
expect_run "bug124_macro_hygiene" "$TMP124" 0
rm -f "$TMP124"

echo ""
echo "Results: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
