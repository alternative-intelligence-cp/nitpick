#!/bin/bash
# Nitpick Bug Regression Tests — v0.26.4 (MEM-011 GC tuning env-vars)
#
# Verifies the three NPK_GC_* env-vars wired in v0.26.4:
#   bug226 — NPK_GC_NURSERY_SIZE  overrides nursery capacity.
#   bug227 — NPK_GC_OLD_GEN_THRESHOLD overrides old-gen threshold.
#   bug228 — NPK_GC_MODE=concurrent flips on concurrent mark.
#   bug229 — with all NPK_GC_* unset, defaults apply (4 MB / 64 MB / STW).
#
# Each fixture is a separate process so the GC singleton is cold at
# init time — env-vars read inside GCState::init() must be set BEFORE
# the binary starts, not by the parent process after the fact.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMP_EXE="/tmp/npk_bug_test_0264.exe"

if [ ! -f "$NPKC" ]; then
    echo "WARNING: npkc not found at $NPKC — skipping bug tests"
    exit 77
fi

PASS=0; FAIL=0; TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1 — $2"; }

run_with_env() {
    local label="$1"
    local file="$2"
    local expected="$3"
    shift 3
    # Remaining args are KEY=VALUE pairs to set in the child env;
    # leading "-K" entries are env-vars to UNSET.
    local env_args=()
    local unset_args=()
    while [ $# -gt 0 ]; do
        case "$1" in
            -K) unset_args+=("--unset=$2"); shift 2 ;;
            *)  env_args+=("$1"); shift ;;
        esac
    done

    rm -f "$TMP_EXE"
    local out
    out=$("$NPKC" "$file" -o "$TMP_EXE" 2>&1)
    if [ ! -x "$TMP_EXE" ]; then
        fail "$label" "compile failed: $out"
        return
    fi
    env -i PATH="$PATH" HOME="$HOME" "${unset_args[@]}" "${env_args[@]}" "$TMP_EXE"
    local actual=$?
    if [ "$actual" != "$expected" ]; then
        fail "$label" "expected exit $expected but got $actual"
        rm -f "$TMP_EXE"
        return
    fi
    rm -f "$TMP_EXE"
    pass "$label"
}

echo "== v0.26.4 MEM-011 (GC tuning env-vars) =="

run_with_env "bug226 NPK_GC_NURSERY_SIZE=8MB" \
    "$SCRIPT_DIR/bug226_gc_nursery_size_envvar.npk" \
    "0" \
    "NPK_GC_NURSERY_SIZE=8388608"

run_with_env "bug227 NPK_GC_OLD_GEN_THRESHOLD=32MB" \
    "$SCRIPT_DIR/bug227_gc_old_gen_threshold_envvar.npk" \
    "0" \
    "NPK_GC_OLD_GEN_THRESHOLD=33554432"

run_with_env "bug228 NPK_GC_MODE=concurrent" \
    "$SCRIPT_DIR/bug228_gc_mode_concurrent_envvar.npk" \
    "0" \
    "NPK_GC_MODE=concurrent"

# bug229 runs with env -i (no NPK_GC_* set) to verify defaults apply.
run_with_env "bug229 defaults when NPK_GC_* unset" \
    "$SCRIPT_DIR/bug229_gc_defaults_no_envvars.npk" \
    "0"

echo "== Results: $PASS/$TOTAL passed, $FAIL failed =="
[ $FAIL -eq 0 ] && exit 0 || exit 1
