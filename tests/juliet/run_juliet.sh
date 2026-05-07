#!/usr/bin/env bash
# Aria v0.18.5 — Juliet CWE Test Runner
# Tests Aria's structural safety mechanisms against NIST SAMATE Juliet CWE patterns.
#
# Test categories:
#   good.npk        — safe Aria pattern; must compile and exit 0
#   bad.npk         — unsafe pattern Aria catches at runtime; must compile and exit non-zero
#   bad_compile.npk — unsafe pattern Aria rejects at compile time; compile must fail
#
# Usage: bash run_juliet.sh [--verbose]

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${NPKC:-$(command -v npkc 2>/dev/null || echo "")}"
VERBOSE="${1:-}"

# Locate npkc
if [[ -z "$NPKC" ]]; then
    CANDIDATES=(
        "$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null)/build/npkc"
        "/home/randy/Workspace/REPOS/aria/build/npkc"
        "$(dirname "$SCRIPT_DIR")/build/npkc"
        "$(dirname "$(dirname "$SCRIPT_DIR")")/build/npkc"
    )
    for c in "${CANDIDATES[@]}"; do
        if [[ -x "$c" ]]; then
            NPKC="$c"
            break
        fi
    done
fi

if [[ ! -x "$NPKC" ]]; then
    echo "ERROR: npkc compiler not found. Set NPKC=/path/to/npkc or ensure it is in PATH."
    exit 1
fi

TMPDIR_LOCAL="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_LOCAL"' EXIT

pass_count=0
fail_count=0
total=0

cwe_pass() { echo "  PASS [$1] $2"; pass_count=$(( pass_count + 1 )); total=$(( total + 1 )); }
cwe_fail() { echo "  FAIL [$1] $2"; fail_count=$(( fail_count + 1 )); total=$(( total + 1 )); }

run_test() {
    local cwe_dir="$1"
    local src="$2"
    local kind="$3"         # good | bad | bad_compile
    local label
    label="$(basename "$cwe_dir")/$(basename "$src")"
    local bin="$TMPDIR_LOCAL/$(basename "$cwe_dir")_$(basename "$src" .npk)"

    if [[ "$kind" == "bad_compile" ]]; then
        # Compile must fail
        local compile_out
        local compile_out compile_exit
        compile_out="$("$NPKC" "$src" -o "$bin" 2>&1)"; compile_exit=$?
        if [[ $compile_exit -ne 0 ]]; then
            cwe_pass "compile-reject" "$label"
            if [[ "$VERBOSE" == "--verbose" ]]; then
                echo "    compiler said: $(echo "$compile_out" | head -1)"
            fi
        else
            cwe_fail "compile-reject" "$label (expected compile error but it compiled cleanly)"
        fi

    elif [[ "$kind" == "good" ]]; then
        # Compile + run → exit 0
        local compile_out compile_exit
        compile_out="$("$NPKC" "$src" -o "$bin" 2>&1)"; compile_exit=$?
        if [[ $compile_exit -ne 0 ]]; then
            cwe_fail "good-compile" "$label (compile failed: $(echo "$compile_out" | head -1))"
            return
        fi
        local run_exit=0
        "$bin" 2>/dev/null || run_exit=$?
        if [[ $run_exit -eq 0 ]]; then
            cwe_pass "good-run" "$label"
        else
            cwe_fail "good-run" "$label (expected exit 0, got $run_exit)"
        fi

    elif [[ "$kind" == "bad" ]]; then
        # Compile + run → exit non-zero (runtime prevention triggered)
        local compile_out compile_exit
        compile_out="$("$NPKC" "$src" -o "$bin" 2>&1)"; compile_exit=$?
        if [[ $compile_exit -ne 0 ]]; then
            cwe_fail "bad-compile" "$label (bad.npk failed to compile: $(echo "$compile_out" | head -1))"
            return
        fi
        local run_exit=0
        "$bin" 2>/dev/null || run_exit=$?
        if [[ $run_exit -ne 0 ]] && [[ $run_exit -ne 139 ]]; then
            # exit non-zero and not SIGSEGV (139) — clean prevention
            cwe_pass "bad-runtime" "$label (prevented with exit $run_exit)"
        elif [[ $run_exit -eq 139 ]]; then
            cwe_fail "bad-runtime" "$label (SIGSEGV — uncontrolled crash, not graceful prevention)"
        else
            cwe_fail "bad-runtime" "$label (expected prevention exit != 0, got 0 — not prevented!)"
        fi
    fi
}

echo "========================================"
echo "  Aria v0.18.5 — Juliet CWE Test Suite"
echo "========================================"
echo "  Compiler: $NPKC"
echo ""

# Discover and run all tests
for cwe_dir in "$SCRIPT_DIR"/cwe_*/; do
    [[ -d "$cwe_dir" ]] || continue
    cwe_name="$(basename "$cwe_dir")"
    echo "[ $cwe_name ]"

    for src in "$cwe_dir"*.npk; do
        [[ -f "$src" ]] || continue
        fname="$(basename "$src")"
        case "$fname" in
            good.npk)         run_test "$cwe_dir" "$src" "good" ;;
            bad.npk)          run_test "$cwe_dir" "$src" "bad" ;;
            bad_compile.npk)  run_test "$cwe_dir" "$src" "bad_compile" ;;
        esac
    done
    echo ""
done

echo "========================================"
echo "  Results: $pass_count/$total passed"
if [[ $fail_count -gt 0 ]]; then
    echo "  FAILURES: $fail_count"
    echo "========================================"
    exit 1
fi
echo "  ALL TESTS PASSED"
echo "========================================"
exit 0
