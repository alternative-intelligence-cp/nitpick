#!/usr/bin/env bash
# Bug regressions for v0.31.2.3 D-16/16a — ERR-sticky tbb comparisons.
set -u
cd "$(dirname "$0")/../.."
NPKC=./build/npkc
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
echo "== v0.31.2.3 D-16/16a — tbb comparison ERR sticky =="

run_pass() {
    local name="$1"
    local src="tests/bugs/${name}.npk"
    local out="$TMP/${name}.exe"
    if ! "$NPKC" "$src" -o "$out" >"$TMP/${name}.log" 2>&1; then
        echo "  FAIL: $name (compile error)"; cat "$TMP/${name}.log"
        fail=$((fail+1)); return
    fi
    "$out"; local ec=$?
    if [ $ec -ne 0 ]; then
        echo "  FAIL: $name (runtime exit=$ec)"
        fail=$((fail+1)); return
    fi
    echo "  PASS: $name (exit 0)"
    pass=$((pass+1))
}

run_pass bug387_tbb_cmp_eq_err_sticky
run_pass bug388_tbb_cmp_ne_err_sticky
run_pass bug389_tbb_cmp_lt_err_sticky
run_pass bug390_tbb_cmp_le_err_sticky
run_pass bug391_tbb_cmp_gt_err_sticky
run_pass bug392_tbb_cmp_ge_err_sticky

echo ""
echo "Summary: $pass/$((pass+fail)) passed, $fail failed"
[ $fail -eq 0 ]
