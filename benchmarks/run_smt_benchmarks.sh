#!/usr/bin/env bash
# Aria SMT Verification Benchmark Suite — v0.5.5
#
# Measures both COMPILE-TIME cost of Z3 verification and RUNTIME benefit
# of --smt-opt optimizations across real programs.
#
# Usage: ./benchmarks/run_smt_benchmarks.sh [--runs N] [--verbose]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ARIAC="$REPO_DIR/build/ariac"
RUNS=5
VERBOSE=false

for arg in "$@"; do
    case "$arg" in
        --runs=*) RUNS="${arg#*=}" ;;
        --verbose) VERBOSE=true ;;
        --help|-h)
            echo "Usage: $0 [--runs=N] [--verbose]"
            echo "  --runs=N    Number of runs per benchmark (default: 5)"
            echo "  --verbose   Show individual run timings"
            exit 0 ;;
    esac
done

if [ ! -x "$ARIAC" ]; then
    echo "ERROR: ariac not found at $ARIAC. Build first: cd build && cmake .. && make -j\$(nproc)"
    exit 1
fi

# Build bench_helpers library (provides bench_nanos, bench_peak_rss_kb)
OUTDIR="$SCRIPT_DIR/out"
mkdir -p "$OUTDIR"
gcc -O2 -c "$SCRIPT_DIR/bench_helpers.c" -o "$OUTDIR/bench_helpers.o"
ar rcs "$OUTDIR/libbench.a" "$OUTDIR/bench_helpers.o"
LINK_FLAGS=(-L"$OUTDIR" -lbench)

VERSION=$("$ARIAC" --version 2>&1 | head -1 || echo "unknown")

echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║          Aria SMT Verification Benchmark Suite                  ║"
echo "╠══════════════════════════════════════════════════════════════════╣"
echo "║  Compiler:  $VERSION"
echo "║  Runs:      $RUNS per configuration"
echo "║  Date:      $(date '+%Y-%m-%d %H:%M:%S')"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""

# ─── Helper: measure compile time ────────────────────────────────────
# Returns: avg min max (milliseconds)
measure_compile() {
    local src="$1"
    local out="$2"
    shift 2
    local flags=("$@")
    local total=0 min=999999999 max=0

    for ((r = 1; r <= RUNS; r++)); do
        local start end elapsed
        start=$(date +%s%N)
        "$ARIAC" "$src" -o "$out" "${flags[@]}" "${LINK_FLAGS[@]}" > /dev/null 2>&1 || true
        end=$(date +%s%N)
        elapsed=$(( (end - start) / 1000000 ))
        total=$((total + elapsed))
        ((elapsed < min)) && min=$elapsed
        ((elapsed > max)) && max=$elapsed
        if $VERBOSE; then
            printf "    run %d: %d ms\n" "$r" "$elapsed"
        fi
    done
    local avg=$((total / RUNS))
    echo "$avg $min $max"
}

# ─── Helper: measure runtime ─────────────────────────────────────────
measure_runtime() {
    local binary="$1"
    local runs="$2"
    local total=0 min=999999999 max=0

    for ((r = 1; r <= runs; r++)); do
        local start end elapsed
        start=$(date +%s%N)
        "$binary" > /dev/null 2>&1 || true
        end=$(date +%s%N)
        elapsed=$(( (end - start) / 1000000 ))
        total=$((total + elapsed))
        ((elapsed < min)) && min=$elapsed
        ((elapsed > max)) && max=$elapsed
    done
    local avg=$((total / runs))
    echo "$avg $min $max"
}

# ─── Helper: format result line ──────────────────────────────────────
fmt_result() {
    local label="$1" avg="$2" min="$3" max="$4"
    printf "  %-38s %6d ms (min: %d, max: %d)\n" "$label" "$avg" "$min" "$max"
}

# ═══════════════════════════════════════════════════════════════════════
# PART 1: COMPILE-TIME BENCHMARKS
# ═══════════════════════════════════════════════════════════════════════
echo "═══ Part 1: Compile-Time Cost of Z3 Verification ══════════════════"
echo ""

# Benchmark 1: Rules/limit verification
BENCH_RULES="$SCRIPT_DIR/bench_smt_rules.aria"
if [ -f "$BENCH_RULES" ]; then
    echo "── bench_smt_rules (Rules/limit constraints) ──────────────────"
    read avg1 min1 max1 <<< "$(measure_compile "$BENCH_RULES" "$SCRIPT_DIR/out/bench_smt_rules_base" )"
    fmt_result "Baseline (no verification)" "$avg1" "$min1" "$max1"
    read avg2 min2 max2 <<< "$(measure_compile "$BENCH_RULES" "$SCRIPT_DIR/out/bench_smt_rules_verify" --verify --verify-report)"
    fmt_result "With --verify --verify-report" "$avg2" "$min2" "$max2"
    read avg3 min3 max3 <<< "$(measure_compile "$BENCH_RULES" "$SCRIPT_DIR/out/bench_smt_rules_smt" --smt-opt)"
    fmt_result "With --smt-opt" "$avg3" "$min3" "$max3"
    overhead=$(( avg2 - avg1 ))
    printf "  → Verification overhead: %+d ms (%d%%)\n" "$overhead" "$(( overhead * 100 / (avg1 > 0 ? avg1 : 1) ))"
    echo ""
fi

# Benchmark 2: Contract verification
BENCH_CONTRACTS="$SCRIPT_DIR/bench_smt_contracts.aria"
if [ -f "$BENCH_CONTRACTS" ]; then
    echo "── bench_smt_contracts (requires/ensures) ─────────────────────"
    read avg1 min1 max1 <<< "$(measure_compile "$BENCH_CONTRACTS" "$SCRIPT_DIR/out/bench_smt_contracts_base" )"
    fmt_result "Baseline (no verification)" "$avg1" "$min1" "$max1"
    read avg2 min2 max2 <<< "$(measure_compile "$BENCH_CONTRACTS" "$SCRIPT_DIR/out/bench_smt_contracts_verify" --verify-contracts --verify-report)"
    fmt_result "With --verify-contracts" "$avg2" "$min2" "$max2"
    overhead=$(( avg2 - avg1 ))
    printf "  → Verification overhead: %+d ms (%d%%)\n" "$overhead" "$(( overhead * 100 / (avg1 > 0 ? avg1 : 1) ))"
    echo ""
fi

# Benchmark 3: Overflow verification
BENCH_OVERFLOW="$SCRIPT_DIR/bench_smt_overflow.aria"
if [ -f "$BENCH_OVERFLOW" ]; then
    echo "── bench_smt_overflow (integer overflow proofs) ───────────────"
    read avg1 min1 max1 <<< "$(measure_compile "$BENCH_OVERFLOW" "$SCRIPT_DIR/out/bench_smt_overflow_base" )"
    fmt_result "Baseline (no verification)" "$avg1" "$min1" "$max1"
    read avg2 min2 max2 <<< "$(measure_compile "$BENCH_OVERFLOW" "$SCRIPT_DIR/out/bench_smt_overflow_verify" --verify-overflow --verify-report)"
    fmt_result "With --verify-overflow" "$avg2" "$min2" "$max2"
    overhead=$(( avg2 - avg1 ))
    printf "  → Verification overhead: %+d ms (%d%%)\n" "$overhead" "$(( overhead * 100 / (avg1 > 0 ? avg1 : 1) ))"
    echo ""
fi

# Benchmark 4: Concurrency verification
BENCH_CONC="$SCRIPT_DIR/bench_smt_concurrency.aria"
if [ -f "$BENCH_CONC" ]; then
    echo "── bench_smt_concurrency (data race & deadlock) ───────────────"
    read avg1 min1 max1 <<< "$(measure_compile "$BENCH_CONC" "$SCRIPT_DIR/out/bench_smt_conc_base" )"
    fmt_result "Baseline (no verification)" "$avg1" "$min1" "$max1"
    read avg2 min2 max2 <<< "$(measure_compile "$BENCH_CONC" "$SCRIPT_DIR/out/bench_smt_conc_verify" --verify-concurrency --verify-report)"
    fmt_result "With --verify-concurrency" "$avg2" "$min2" "$max2"
    overhead=$(( avg2 - avg1 ))
    printf "  → Verification overhead: %+d ms (%d%%)\n" "$overhead" "$(( overhead * 100 / (avg1 > 0 ? avg1 : 1) ))"
    echo ""
fi

# Benchmark 5: Memory safety (UAF + recursion)
# Use the test files as-is since they're designed for verification
BENCH_MEM="$REPO_DIR/tests/test_z3_use_after_free.aria"
if [ -f "$BENCH_MEM" ]; then
    echo "── test_z3_use_after_free (UAF proofs) ────────────────────────"
    read avg1 min1 max1 <<< "$(measure_compile "$BENCH_MEM" "$SCRIPT_DIR/out/bench_smt_mem_base" )"
    fmt_result "Baseline (no verification)" "$avg1" "$min1" "$max1"
    read avg2 min2 max2 <<< "$(measure_compile "$BENCH_MEM" "$SCRIPT_DIR/out/bench_smt_mem_verify" --verify-memory --verify-report)"
    fmt_result "With --verify-memory" "$avg2" "$min2" "$max2"
    overhead=$(( avg2 - avg1 ))
    printf "  → Verification overhead: %+d ms (%d%%)\n" "$overhead" "$(( overhead * 100 / (avg1 > 0 ? avg1 : 1) ))"
    echo ""
fi

# Benchmark 6: Full verification (all flags)
if [ -f "$BENCH_RULES" ]; then
    echo "── bench_smt_rules (ALL verification flags) ───────────────────"
    read avg1 min1 max1 <<< "$(measure_compile "$BENCH_RULES" "$SCRIPT_DIR/out/bench_smt_all_base" )"
    fmt_result "Baseline" "$avg1" "$min1" "$max1"
    read avg2 min2 max2 <<< "$(measure_compile "$BENCH_RULES" "$SCRIPT_DIR/out/bench_smt_all_verify" --verify --verify-report --verify-contracts --verify-overflow --verify-concurrency --verify-memory --smt-opt)"
    fmt_result "With ALL flags" "$avg2" "$min2" "$max2"
    overhead=$(( avg2 - avg1 ))
    printf "  → Full verification overhead: %+d ms (%d%%)\n" "$overhead" "$(( overhead * 100 / (avg1 > 0 ? avg1 : 1) ))"
    echo ""
fi

# ═══════════════════════════════════════════════════════════════════════
# PART 2: RUNTIME BENCHMARKS (--smt-opt vs baseline)
# ═══════════════════════════════════════════════════════════════════════
echo "═══ Part 2: Runtime Performance (--smt-opt Optimization) ══════════"
echo ""

# Runtime benchmark: Rules program
if [ -f "$BENCH_RULES" ]; then
    echo "── bench_smt_rules runtime ────────────────────────────────────"
    # Compile baseline and optimized versions
    "$ARIAC" "$BENCH_RULES" -o "$SCRIPT_DIR/out/bench_smt_rules_run_base" "${LINK_FLAGS[@]}" 2>/dev/null || true
    "$ARIAC" "$BENCH_RULES" -o "$SCRIPT_DIR/out/bench_smt_rules_run_opt" --smt-opt "${LINK_FLAGS[@]}" 2>/dev/null || true

    if [ -x "$SCRIPT_DIR/out/bench_smt_rules_run_base" ] && [ -x "$SCRIPT_DIR/out/bench_smt_rules_run_opt" ]; then
        read avg1 min1 max1 <<< "$(measure_runtime "$SCRIPT_DIR/out/bench_smt_rules_run_base" "$RUNS")"
        fmt_result "Baseline runtime" "$avg1" "$min1" "$max1"
        read avg2 min2 max2 <<< "$(measure_runtime "$SCRIPT_DIR/out/bench_smt_rules_run_opt" "$RUNS")"
        fmt_result "With --smt-opt runtime" "$avg2" "$min2" "$max2"
        if [ "$avg1" -gt 0 ]; then
            speedup=$(( (avg1 - avg2) * 100 / avg1 ))
            printf "  → Runtime speedup: %+d%% (%d ms saved)\n" "$speedup" "$((avg1 - avg2))"
        fi
    else
        echo "  (binaries did not compile — skipping runtime)"
    fi
    echo ""
fi

# Runtime benchmark: Overflow program
if [ -f "$BENCH_OVERFLOW" ]; then
    echo "── bench_smt_overflow runtime ─────────────────────────────────"
    "$ARIAC" "$BENCH_OVERFLOW" -o "$SCRIPT_DIR/out/bench_smt_overflow_run_base" "${LINK_FLAGS[@]}" 2>/dev/null || true
    "$ARIAC" "$BENCH_OVERFLOW" -o "$SCRIPT_DIR/out/bench_smt_overflow_run_opt" --smt-opt "${LINK_FLAGS[@]}" 2>/dev/null || true

    if [ -x "$SCRIPT_DIR/out/bench_smt_overflow_run_base" ] && [ -x "$SCRIPT_DIR/out/bench_smt_overflow_run_opt" ]; then
        read avg1 min1 max1 <<< "$(measure_runtime "$SCRIPT_DIR/out/bench_smt_overflow_run_base" "$RUNS")"
        fmt_result "Baseline runtime" "$avg1" "$min1" "$max1"
        read avg2 min2 max2 <<< "$(measure_runtime "$SCRIPT_DIR/out/bench_smt_overflow_run_opt" "$RUNS")"
        fmt_result "With --smt-opt runtime" "$avg2" "$min2" "$max2"
        if [ "$avg1" -gt 0 ]; then
            speedup=$(( (avg1 - avg2) * 100 / avg1 ))
            printf "  → Runtime speedup: %+d%% (%d ms saved)\n" "$speedup" "$((avg1 - avg2))"
        fi
    else
        echo "  (binaries did not compile — skipping runtime)"
    fi
    echo ""
fi

# Runtime benchmark: ustack SMT vs baseline (pre-existing)
BENCH_USTACK_BASE="$SCRIPT_DIR/bench_ustack.aria"
BENCH_USTACK_SMT="$SCRIPT_DIR/bench_ustack_smt.aria"
if [ -f "$BENCH_USTACK_BASE" ] && [ -f "$BENCH_USTACK_SMT" ]; then
    echo "── bench_ustack (builtin stack: baseline vs SMT-optimized) ────"
    "$ARIAC" "$BENCH_USTACK_BASE" -o "$SCRIPT_DIR/out/bench_ustack_run_base" "${LINK_FLAGS[@]}" 2>/dev/null || true
    "$ARIAC" "$BENCH_USTACK_SMT" -o "$SCRIPT_DIR/out/bench_ustack_run_smt" --smt-opt "${LINK_FLAGS[@]}" 2>/dev/null || true

    if [ -x "$SCRIPT_DIR/out/bench_ustack_run_base" ] && [ -x "$SCRIPT_DIR/out/bench_ustack_run_smt" ]; then
        read avg1 min1 max1 <<< "$(measure_runtime "$SCRIPT_DIR/out/bench_ustack_run_base" "$RUNS")"
        fmt_result "ustack baseline" "$avg1" "$min1" "$max1"
        read avg2 min2 max2 <<< "$(measure_runtime "$SCRIPT_DIR/out/bench_ustack_run_smt" "$RUNS")"
        fmt_result "ustack --smt-opt" "$avg2" "$min2" "$max2"
        if [ "$avg1" -gt 0 ]; then
            speedup=$(( (avg1 - avg2) * 100 / avg1 ))
            printf "  → Runtime speedup: %+d%% (%d ms saved)\n" "$speedup" "$((avg1 - avg2))"
        fi
    else
        echo "  (binaries did not compile — skipping runtime)"
    fi
    echo ""
fi

# ═══════════════════════════════════════════════════════════════════════
# PART 3: VERIFICATION CORRECTNESS SUMMARY
# ═══════════════════════════════════════════════════════════════════════
echo "═══ Part 3: Verification Correctness Summary ══════════════════════"
echo ""

TOTAL_PROVEN=0
TOTAL_DISPROVEN=0
TOTAL_UNKNOWN=0

for testfile in "$REPO_DIR"/tests/test_z3_*.aria; do
    name=$(basename "$testfile" .aria)
    # Determine which flags to use
    flags="--verify --verify-report --verify-contracts --verify-overflow --verify-concurrency --verify-memory"
    output=$("$ARIAC" "$testfile" $flags 2>&1 || true)
    proven=$(echo "$output" | grep -oP 'Proven:\s+\K\d+' || echo "0")
    disproven=$(echo "$output" | grep -oP 'Disproven:\s+\K\d+' || echo "0")
    unknown=$(echo "$output" | grep -oP 'Unknown:\s+\K\d+' || echo "0")
    TOTAL_PROVEN=$((TOTAL_PROVEN + proven))
    TOTAL_DISPROVEN=$((TOTAL_DISPROVEN + disproven))
    TOTAL_UNKNOWN=$((TOTAL_UNKNOWN + unknown))
    printf "  %-40s P:%-4s D:%-4s U:%s\n" "$name" "$proven" "$disproven" "$unknown"
done

# Also run the SMT-specific tests
for testfile in "$REPO_DIR"/tests/smt/*.aria; do
    [ -f "$testfile" ] || continue
    name=$(basename "$testfile" .aria)
    flags="--verify --verify-report --verify-contracts --verify-overflow"
    output=$("$ARIAC" "$testfile" $flags 2>&1 || true)
    proven=$(echo "$output" | grep -oP 'Proven:\s+\K\d+' || echo "0")
    disproven=$(echo "$output" | grep -oP 'Disproven:\s+\K\d+' || echo "0")
    unknown=$(echo "$output" | grep -oP 'Unknown:\s+\K\d+' || echo "0")
    TOTAL_PROVEN=$((TOTAL_PROVEN + proven))
    TOTAL_DISPROVEN=$((TOTAL_DISPROVEN + disproven))
    TOTAL_UNKNOWN=$((TOTAL_UNKNOWN + unknown))
    printf "  %-40s P:%-4s D:%-4s U:%s\n" "$name" "$proven" "$disproven" "$unknown"
done

echo ""
echo "═══ Totals ════════════════════════════════════════════════════════"
echo "  Proven:    $TOTAL_PROVEN"
echo "  Disproven: $TOTAL_DISPROVEN"
echo "  Unknown:   $TOTAL_UNKNOWN"
echo ""
echo "Done."
