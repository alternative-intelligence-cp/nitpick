#!/usr/bin/env bash
# Aria Benchmark Suite — Aria vs C (gcc -O2)
# Usage: ./benchmarks/run_benchmarks.sh [--runs N] [--hyperfine]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ARIAC="$REPO_DIR/build/ariac"
RUNS=5
USE_HYPERFINE=false

for arg in "$@"; do
    case "$arg" in
        --runs=*) RUNS="${arg#*=}" ;;
        --hyperfine) USE_HYPERFINE=true ;;
        --help|-h) echo "Usage: $0 [--runs=N] [--hyperfine]"; exit 0 ;;
    esac
done

BENCHMARKS=(bench_primes bench_collatz bench_gcd)

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                  Aria Benchmark Suite                       ║"
echo "╠══════════════════════════════════════════════════════════════╣"
echo "║  Runs per benchmark: $RUNS"
echo "║  Compiler: $ARIAC"
echo "║  GCC: $(gcc --version | head -1)"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Build phase
echo "── Building benchmarks ──────────────────────────────────────"
for bench in "${BENCHMARKS[@]}"; do
    printf "  %-20s" "$bench (Aria)"
    "$ARIAC" "$SCRIPT_DIR/$bench.aria" -o "$SCRIPT_DIR/${bench}_aria" 2>/dev/null
    echo "✓"
    printf "  %-20s" "$bench (C)"
    gcc -O2 -o "$SCRIPT_DIR/${bench}_c" "$SCRIPT_DIR/$bench.c"
    echo "✓"
done
echo ""

# Correctness check
echo "── Correctness verification ─────────────────────────────────"
ALL_PASS=true
for bench in "${BENCHMARKS[@]}"; do
    printf "  %-20s" "$bench (Aria)"
    if "$SCRIPT_DIR/${bench}_aria" > /dev/null 2>&1; then echo "PASS"; else echo "FAIL"; ALL_PASS=false; fi
    printf "  %-20s" "$bench (C)"
    if "$SCRIPT_DIR/${bench}_c" > /dev/null 2>&1; then echo "PASS"; else echo "FAIL"; ALL_PASS=false; fi
done
echo ""

if [ "$ALL_PASS" != "true" ]; then
    echo "ERROR: Some benchmarks failed correctness checks. Aborting."
    exit 1
fi

# Timing
time_cmd() {
    local binary="$1"
    local runs="$2"
    local total=0
    local min=999999999
    local max=0
    for ((r = 1; r <= runs; r++)); do
        local start end elapsed
        start=$(date +%s%N)
        "$binary" > /dev/null 2>&1
        end=$(date +%s%N)
        elapsed=$(( (end - start) / 1000000 ))  # ms
        total=$((total + elapsed))
        if ((elapsed < min)); then min=$elapsed; fi
        if ((elapsed > max)); then max=$elapsed; fi
    done
    local avg=$((total / runs))
    echo "$avg $min $max"
}

if $USE_HYPERFINE && command -v hyperfine &>/dev/null; then
    echo "── Benchmarks (hyperfine) ─────────────────────────────────"
    for bench in "${BENCHMARKS[@]}"; do
        echo ""
        echo "  $bench:"
        hyperfine --warmup 2 --runs "$RUNS" \
            --command-name "Aria" "$SCRIPT_DIR/${bench}_aria" \
            --command-name "C (gcc -O2)" "$SCRIPT_DIR/${bench}_c"
    done
else
    echo "── Benchmarks (built-in timer, ${RUNS} runs) ────────────────"
    printf "\n  %-18s %10s %10s %10s %10s %10s %10s %8s\n" \
        "Benchmark" "Aria avg" "Aria min" "Aria max" "C avg" "C min" "C max" "Ratio"
    printf "  %-18s %10s %10s %10s %10s %10s %10s %8s\n" \
        "─────────────────" "────────" "────────" "────────" "────────" "────────" "────────" "──────"

    for bench in "${BENCHMARKS[@]}"; do
        read -r aria_avg aria_min aria_max <<< "$(time_cmd "$SCRIPT_DIR/${bench}_aria" "$RUNS")"
        read -r c_avg c_min c_max <<< "$(time_cmd "$SCRIPT_DIR/${bench}_c" "$RUNS")"

        if ((c_avg > 0)); then
            ratio_x10=$((aria_avg * 10 / c_avg))
            ratio_int=$((ratio_x10 / 10))
            ratio_frac=$((ratio_x10 % 10))
            ratio="${ratio_int}.${ratio_frac}x"
        else
            ratio="N/A"
        fi

        printf "  %-18s %8dms %8dms %8dms %8dms %8dms %8dms %8s\n" \
            "$bench" "$aria_avg" "$aria_min" "$aria_max" "$c_avg" "$c_min" "$c_max" "$ratio"
    done
fi

echo ""
echo "── Compile-time comparison ───────────────────────────────────"
printf "\n  %-18s %10s %10s %8s\n" "Benchmark" "ariac" "gcc -O2" "Ratio"
printf "  %-18s %10s %10s %8s\n" "─────────────────" "────────" "────────" "──────"

for bench in "${BENCHMARKS[@]}"; do
    start=$(date +%s%N)
    "$ARIAC" "$SCRIPT_DIR/$bench.aria" -o "$SCRIPT_DIR/${bench}_aria" 2>/dev/null
    end=$(date +%s%N)
    aria_ms=$(( (end - start) / 1000000 ))

    start=$(date +%s%N)
    gcc -O2 -o "$SCRIPT_DIR/${bench}_c" "$SCRIPT_DIR/$bench.c"
    end=$(date +%s%N)
    c_ms=$(( (end - start) / 1000000 ))

    if ((c_ms > 0)); then
        ratio_x10=$((aria_ms * 10 / c_ms))
        ratio_int=$((ratio_x10 / 10))
        ratio_frac=$((ratio_x10 % 10))
        ratio="${ratio_int}.${ratio_frac}x"
    else
        ratio="N/A"
    fi

    printf "  %-18s %8dms %8dms %8s\n" "$bench" "$aria_ms" "$c_ms" "$ratio"
done

echo ""
echo "Done."
