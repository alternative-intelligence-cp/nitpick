#!/usr/bin/env bash
# Compiler Performance Benchmark — Measures compilation time (v0.8.4)
# Usage: ./benchmarks/bench_compile_time.sh [--runs N]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ARIAC="$REPO_DIR/build/ariac"
RUNS=5

for arg in "$@"; do
    case "$arg" in
        --runs=*) RUNS="${arg#*=}" ;;
        --help|-h) echo "Usage: $0 [--runs=N]"; exit 0 ;;
    esac
done

if [[ ! -x "$ARIAC" ]]; then
    echo "ERROR: ariac not found at $ARIAC — run cmake build first"
    exit 1
fi

# Benchmark files (small → medium → large representative programs)
BENCH_FILES=(
    "$SCRIPT_DIR/bench_primes.aria"
    "$SCRIPT_DIR/bench_collatz.aria"
    "$SCRIPT_DIR/bench_gcd.aria"
)

echo "=== Aria Compiler Performance Benchmark ==="
echo "Compiler: $ARIAC"
echo "Runs per file: $RUNS"
echo ""

printf "%-30s %10s %10s %10s\n" "FILE" "MIN (ms)" "AVG (ms)" "MAX (ms)"
printf "%-30s %10s %10s %10s\n" "----" "--------" "--------" "--------"

for bench in "${BENCH_FILES[@]}"; do
    if [[ ! -f "$bench" ]]; then
        continue
    fi
    
    name="$(basename "$bench")"
    times=()
    
    for ((i = 1; i <= RUNS; i++)); do
        start=$(date +%s%N)
        "$ARIAC" "$bench" -o /tmp/bench_out_$$ 2>/dev/null || true
        end=$(date +%s%N)
        elapsed_ms=$(( (end - start) / 1000000 ))
        times+=("$elapsed_ms")
    done
    rm -f /tmp/bench_out_$$
    
    # Calculate min, max, avg
    min=${times[0]}
    max=${times[0]}
    sum=0
    for t in "${times[@]}"; do
        sum=$((sum + t))
        [[ $t -lt $min ]] && min=$t
        [[ $t -gt $max ]] && max=$t
    done
    avg=$((sum / RUNS))
    
    printf "%-30s %10d %10d %10d\n" "$name" "$min" "$avg" "$max"
done

echo ""
echo "Done."
