#!/usr/bin/env bash
# v0.27.3 / ARIA-031 STACK_REF_INTO_GC_FIELD diagnostic suite
set -u
cd "$(dirname "$0")/../.."
NPKC="${NPKC:-./build/npkc}"
[ -x "$NPKC" ] || { echo "SKIP: $NPKC not found"; exit 77; }

fail=0
pass=0

run_reject() {
    local file="$1" code="$2" needle="$3"
    local out
    out="$($NPKC "$file" -o /tmp/$(basename "$file" .npk) 2>&1)"
    if echo "$out" | grep -q "$code" && echo "$out" | grep -q "$needle"; then
        echo "PASS: $file rejected with $code (+ '$needle')"
        pass=$((pass+1))
    else
        echo "FAIL: $file did not produce $code with '$needle'"
        echo "$out" | head -20
        fail=$((fail+1))
    fi
}

run_pass() {
    local file="$1" expect="$2"
    local bin="/tmp/$(basename "$file" .npk)"
    if ! $NPKC "$file" -o "$bin" >/dev/null 2>&1; then
        echo "FAIL: $file did not compile"
        fail=$((fail+1)); return
    fi
    "$bin"
    local rc=$?
    if [ "$rc" -eq "$expect" ]; then
        echo "PASS: $file exit=$rc"
        pass=$((pass+1))
    else
        echo "FAIL: $file exit=$rc want=$expect"
        fail=$((fail+1))
    fi
}

run_reject tests/bugs/bug240_stack_pin_to_gc_field_rejected.npk "ARIA-031" "gc-rooted field"
run_pass   tests/bugs/bug241_stack_pin_to_stack_field_pass.npk 0
run_pass   tests/bugs/bug242_gc_pin_to_gc_field_pass.npk 0

echo "---"
echo "v0.27.3: $pass passed, $fail failed"
exit $fail
