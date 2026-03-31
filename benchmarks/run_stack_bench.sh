#!/usr/bin/env bash
# run_stack_bench.sh — Build and run the three stack approach benchmarks
# Usage: ./benchmarks/run_stack_bench.sh [runs]
set -euo pipefail

cd "$(dirname "$0")/.."

ARIAC="./build/ariac"
BENCHDIR="./benchmarks"
OUTDIR="$BENCHDIR/out"
RUNS=${1:-3}

echo "========================================"
echo "  Aria Stack Approach Benchmark"
echo "  $(date)"
echo "  $RUNS runs per approach"
echo "========================================"
echo ""

# 1. Build C helpers into static library
echo "[1/4] Building C benchmark helpers..."
mkdir -p "$OUTDIR"
gcc -O2 -c "$BENCHDIR/bench_helpers.c" -o "$OUTDIR/bench_helpers.o"
ar rcs "$OUTDIR/libbench.a" "$OUTDIR/bench_helpers.o"
echo "  -> $OUTDIR/libbench.a"

# 2. Compile each Aria benchmark
echo "[2/4] Compiling Aria benchmarks..."

echo "  bench_plain..."
$ARIAC "$BENCHDIR/bench_plain.aria" -o "$OUTDIR/bench_plain" -L"$OUTDIR" -lbench 2>/dev/null
echo "  bench_ustack..."
$ARIAC "$BENCHDIR/bench_ustack.aria" -o "$OUTDIR/bench_ustack" -L"$OUTDIR" -lbench 2>/dev/null
echo "  bench_ustack_smt (--smt-opt)..."
$ARIAC "$BENCHDIR/bench_ustack_smt.aria" -o "$OUTDIR/bench_ustack_smt" --smt-opt -L"$OUTDIR" -lbench 2>/dev/null
echo "  bench_libstack..."
$ARIAC "$BENCHDIR/bench_libstack.aria" -o "$OUTDIR/bench_libstack" -L"$OUTDIR" -lbench 2>/dev/null
echo "  All compiled."
echo ""

# 3. Run benchmarks
echo "[3/4] Running benchmarks ($RUNS runs each)..."
echo ""

for run in $(seq 1 $RUNS); do
    echo "--- Run $run of $RUNS ---"
    echo ""
    "$OUTDIR/bench_plain"
    echo ""
    "$OUTDIR/bench_ustack"
    echo ""
    "$OUTDIR/bench_ustack_smt"
    echo ""
    "$OUTDIR/bench_libstack"
    echo ""
    echo "--- Python ---"
    python3 "$BENCHDIR/bench_stack.py"
    echo ""
    echo "--- Node.js ---"
    node "$BENCHDIR/bench_stack.js"
    echo ""
done

# 4. Memory profiling with /usr/bin/time
echo "[4/4] Memory profiling (single run each)..."
echo ""

echo "--- Plain Variables ---"
/usr/bin/time -v "$OUTDIR/bench_plain" 2>&1 | grep -E "wall clock|Maximum resident|sum:|elapsed_ms:"
echo ""
echo "--- Builtin User Stack ---"
/usr/bin/time -v "$OUTDIR/bench_ustack" 2>&1 | grep -E "wall clock|Maximum resident|sum:|elapsed_ms:"
echo ""
echo "--- Builtin User Stack (SMT Optimized) ---"
/usr/bin/time -v "$OUTDIR/bench_ustack_smt" 2>&1 | grep -E "wall clock|Maximum resident|sum:|elapsed_ms:"
echo ""
echo "--- Library Stack ---"
/usr/bin/time -v "$OUTDIR/bench_libstack" 2>&1 | grep -E "wall clock|Maximum resident|sum:|elapsed_ms:"
echo ""

echo "========================================"
echo "  Done."
echo "========================================"
