#!/bin/bash
# v0.31.1.9 (D-12 Probe B): `$$m dyn T:param` parameter coercion.
# At a call site, when a function declares a `$$m dyn Trait:param`,
# the caller passes a concrete value and the compiler must coerce
# it into a fat-ptr `{alias_ptr_to_caller_storage, vtable}`. Mutation
# through the trait object must persist to the caller.
#
# Pre-fix, the param was lowered as a plain `ptr` (alias to source)
# and any dyn-dispatch read inside the callee treated it as a fat
# ptr — segfault. Fix: lower `$$m dyn T:param` as `%struct.DynTraitObj`
# by-value with an alias-pointer data slot. Both the param type and
# the call-site coercion path updated.
#
# See META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.9.md.

set -e

cd "$(dirname "$0")/../.."

NPKC="./build/npkc"
TMPDIR="$(mktemp -d)"
trap "rm -rf $TMPDIR" EXIT

FAIL=0

run_test() {
    local name="$1"
    local fixture="$2"
    local expected_exit="$3"
    local should_fail="$4"  # "yes" or "no"

    if [[ ! -f "$fixture" ]]; then
        echo "MISSING: $fixture"
        FAIL=$((FAIL+1))
        return
    fi

    local out="$TMPDIR/$name"
    if ! $NPKC "$fixture" -o "$out" >/dev/null 2>&1; then
        if [[ "$should_fail" == "yes_compile" ]]; then
            echo "PASS (compile-fail): $name"
            return
        fi
        echo "FAIL (compile): $name"
        FAIL=$((FAIL+1))
        return
    fi

    set +e
    "$out"
    local got=$?
    set -e

    if [[ "$got" -eq "$expected_exit" ]]; then
        echo "PASS: $name (exit=$got)"
    else
        echo "FAIL: $name (got exit=$got, expected $expected_exit)"
        FAIL=$((FAIL+1))
    fi
}

run_test bug374 tests/bugs/bug374_dyn_borrow_param_mut_pass.npk 8 no

if [[ $FAIL -ne 0 ]]; then
    echo ""
    echo "$FAIL test(s) failed"
    exit 1
fi

echo ""
echo "All v0.31.1.9 bug tests passed"
exit 0
