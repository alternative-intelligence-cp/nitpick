#!/usr/bin/env bash
# run_svcomp.sh — Aria v0.18.6 SV-COMP Benchmark Runner
#
# Runs all SV-COMP benchmark programs, compares to expected verdicts,
# and reports an SV-COMP-style score.
#
# Scoring (SV-COMP official):
#   Correct TRUE  : +2
#   Correct FALSE : +1
#   Incorrect TRUE : -32  (wrong safe verdict — most dangerous)
#   Incorrect FALSE: -16  (wrong unsafe verdict)
#   Unknown/Error  :  0
#
# Usage: NPKC=/path/to/npkc bash run_svcomp.sh [--verbose]

set -uo pipefail

# ── Paths ─────────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${NPKC:-$(command -v npkc 2>/dev/null || echo "")}"

if [[ -z "$NPKC" ]]; then
    REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
    CANDIDATE="$REPO_ROOT/build/npkc"
    if [[ -x "$CANDIDATE" ]]; then
        NPKC="$CANDIDATE"
    fi
fi

if [[ -z "$NPKC" || ! -x "$NPKC" ]]; then
    echo "ERROR: npkc compiler not found." >&2
    echo "  Set NPKC=/path/to/npkc or add it to PATH." >&2
    exit 1
fi

VERBOSE=0
[[ "${1:-}" == "--verbose" ]] && VERBOSE=1

# ── Helpers ───────────────────────────────────────────────────────────────────
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

log_verbose() { [[ $VERBOSE -eq 1 ]] && echo "  [DBG] $*" || true; }

parse_z3_count() {
    local output="$1" key="$2"
    echo "$output" | grep -E "^\s+${key}:" | grep -oE '[0-9]+' | head -1 || echo "0"
}

# ── Verdict engine ────────────────────────────────────────────────────────────
run_benchmark() {
    local yml_file="$1"
    local bench_dir
    bench_dir="$(dirname "$yml_file")"
    local bench_name
    bench_name="$(basename "$bench_dir")/$(basename "$yml_file" .yml)"

    local input_files expected_verdict verdict_strategy verify_flags
    input_files="$(grep 'input_files:' "$yml_file" | sed "s/.*input_files: *'//;s/'//")"
    expected_verdict="$(grep 'expected_verdict:' "$yml_file" | awk '{print $2}')"
    verdict_strategy="$(grep 'verdict_strategy:' "$yml_file" | awk '{print $2}')"
    verify_flags="$(grep 'verify_flags:' "$yml_file" | sed "s/.*verify_flags: *'//;s/'//")"

    local src_file="$bench_dir/$input_files"
    local out_bin="$TMP_DIR/$(basename "$input_files" .npk)"

    log_verbose "Bench: $bench_name  expected=$expected_verdict  strategy=$verdict_strategy"

    # ── compile_reject ────────────────────────────────────────────────────────
    if [[ "$verdict_strategy" == "compile_reject" ]]; then
        local compile_out compile_rc
        compile_out="$("$NPKC" "$src_file" -o "$out_bin" 2>&1)" && compile_rc=0 || compile_rc=$?
        if [[ $compile_rc -ne 0 ]]; then
            log_verbose "  Compile rejected (exit $compile_rc)"
            echo "RESULT $bench_name: FALSE correct (compile_reject) → +1"
            return 0
        else
            echo "RESULT $bench_name: FALSE-expected but compiled OK → INCORRECT_FALSE"
            return 1
        fi
    fi

    # ── z3verify: verify flags passed to compiler; Z3 report in compiler output
    if [[ "$verdict_strategy" == "z3verify" ]]; then
        local z3_out z3_rc
        # shellcheck disable=SC2086
        z3_out="$("$NPKC" "$src_file" $verify_flags -o "$out_bin" 2>&1)" && z3_rc=0 || z3_rc=$?

        log_verbose "  Z3 output (compiler): $z3_out"

        local proven disproven
        proven="$(parse_z3_count "$z3_out" "Proven")"
        disproven="$(parse_z3_count "$z3_out" "Disproven")"

        log_verbose "  Proven=$proven Disproven=$disproven"

        local actual_verdict
        if [[ "$disproven" -gt 0 ]]; then
            actual_verdict="false"
        elif [[ "$proven" -gt 0 ]]; then
            actual_verdict="true"
        else
            echo "RESULT $bench_name: UNKNOWN (no Z3 verdict) → 0"
            return 2
        fi

        if [[ "$actual_verdict" == "$expected_verdict" ]]; then
            if [[ "$actual_verdict" == "true" ]]; then
                echo "RESULT $bench_name: TRUE correct (Proven=$proven) → +2"
            else
                echo "RESULT $bench_name: FALSE correct (Disproven=$disproven) → +1"
            fi
            return 0
        else
            if [[ "$actual_verdict" == "true" ]]; then
                echo "RESULT $bench_name: TRUE INCORRECT (expected false) → -32"
            else
                echo "RESULT $bench_name: FALSE INCORRECT (expected true) → -16"
            fi
            return 1
        fi
    fi

    # ── runtime: compile then run; exit 0 → TRUE, exit != 0 → FALSE ──────────
    if [[ "$verdict_strategy" == "runtime" ]]; then
        local compile_out compile_rc
        compile_out="$("$NPKC" "$src_file" -o "$out_bin" 2>&1)" && compile_rc=0 || compile_rc=$?
        if [[ $compile_rc -ne 0 ]]; then
            echo "RESULT $bench_name: COMPILE_ERROR (unexpected) → UNKNOWN (0)"
            log_verbose "  Compile error: $compile_out"
            return 2
        fi

        local run_rc
        "$out_bin" > /dev/null 2>&1 && run_rc=0 || run_rc=$?

        local actual_verdict
        if [[ $run_rc -eq 0 ]]; then
            actual_verdict="true"
        else
            actual_verdict="false"
        fi

        if [[ "$actual_verdict" == "$expected_verdict" ]]; then
            if [[ "$actual_verdict" == "true" ]]; then
                echo "RESULT $bench_name: TRUE correct → +2"
            else
                echo "RESULT $bench_name: FALSE correct (exit $run_rc) → +1"
            fi
            return 0
        else
            if [[ "$actual_verdict" == "true" ]]; then
                echo "RESULT $bench_name: TRUE INCORRECT (expected false) → -32"
            else
                echo "RESULT $bench_name: FALSE INCORRECT (expected true) → -16"
            fi
            return 1
        fi
    fi

    echo "RESULT $bench_name: UNKNOWN strategy '$verdict_strategy' → 0"
    return 2
}

# ── Main ──────────────────────────────────────────────────────────────────────
echo "================================================================"
echo " Aria SV-COMP Benchmark Suite — v0.18.6"
echo " Compiler: $NPKC"
echo "================================================================"
echo ""

SCORE=0
TOTAL=0
CORRECT=0
INCORRECT=0
UNKNOWN=0

mapfile -t YML_FILES < <(find "$SCRIPT_DIR" -name "*.yml" | sort)

if [[ ${#YML_FILES[@]} -eq 0 ]]; then
    echo "ERROR: No .yml benchmark files found under $SCRIPT_DIR" >&2
    exit 1
fi

for yml in "${YML_FILES[@]}"; do
    TOTAL=$((TOTAL + 1))
    result_line=""
    result_line="$(run_benchmark "$yml")" || true
    echo "$result_line"

    if echo "$result_line" | grep -q "TRUE correct"; then
        SCORE=$((SCORE + 2))
        CORRECT=$((CORRECT + 1))
    elif echo "$result_line" | grep -q "FALSE correct"; then
        SCORE=$((SCORE + 1))
        CORRECT=$((CORRECT + 1))
    elif echo "$result_line" | grep -q "TRUE INCORRECT"; then
        SCORE=$((SCORE - 32))
        INCORRECT=$((INCORRECT + 1))
    elif echo "$result_line" | grep -q "FALSE INCORRECT\|FALSE-expected"; then
        SCORE=$((SCORE - 16))
        INCORRECT=$((INCORRECT + 1))
    else
        UNKNOWN=$((UNKNOWN + 1))
    fi
done

echo ""
echo "================================================================"
echo " SV-COMP Results Summary"
echo "----------------------------------------------------------------"
echo "  Total benchmarks  : $TOTAL"
echo "  Correct verdicts  : $CORRECT"
echo "  Incorrect verdicts: $INCORRECT"
echo "  Unknown/Error     : $UNKNOWN"
printf "  SV-COMP Score     : %+d\n" "$SCORE"
echo "================================================================"

if [[ $INCORRECT -gt 0 ]]; then
    exit 1
fi
exit 0
