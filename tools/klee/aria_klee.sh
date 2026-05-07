#!/usr/bin/env bash
# aria_klee.sh — Nitpick symbolic execution harness (v0.18.4)
#
# Pipeline for each .npk test program:
#   1. npkc --emit-llvm  → LLVM 20 .ll
#   2. ll_downgrade.py   → LLVM 14 .ll
#   3. llvm-as-14        → LLVM 14 .bc
#   4. llvm-link-14      → linked .bc (with aria runtime stubs)
#   5. klee              → symbolic execution
#
# Usage:
#   ./aria_klee.sh [--test-dir DIR] [--out-dir DIR] [--timeout N]
#
# Env overrides:
#   NPKC        — path to npkc compiler
#   KLEE        — path to klee binary
#   LLVM_AS     — path to llvm-as-14
#   LLVM_LINK   — path to llvm-link-14
#   DOWNGRADE   — path to ll_downgrade.py
#   STUBS_BC    — path to aria_rt_stubs.bc
#   TEST_DIR    — directory of .npk KLEE test programs
#   RESULT_DIR  — output directory for results

set -euo pipefail

# ── Defaults (resolve from repo-relative paths) ───────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

NPKC="${NPKC:-$REPO_DIR/build/npkc}"
KLEE="${KLEE:-$(command -v klee 2>/dev/null || echo "$HOME/Workspace/REPOS/klee/build/bin/klee")}"
LLVM_AS="${LLVM_AS:-$(command -v llvm-as-14 2>/dev/null || echo llvm-as-14)}"
LLVM_LINK="${LLVM_LINK:-$(command -v llvm-link-14 2>/dev/null || echo llvm-link-14)}"
DOWNGRADE="${DOWNGRADE:-$REPO_DIR/tools/ikos/ll_downgrade.py}"
STUBS_BC="${STUBS_BC:-$SCRIPT_DIR/aria_rt_stubs.bc}"
TEST_DIR="${TEST_DIR:-$SCRIPT_DIR/tests}"
RESULT_DIR="${RESULT_DIR:-$SCRIPT_DIR/results}"
TIMEOUT="${TIMEOUT:-60}"   # seconds per test

# ── Parse args ─────────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --test-dir)  TEST_DIR="$2";   shift 2 ;;
        --out-dir)   RESULT_DIR="$2"; shift 2 ;;
        --timeout)   TIMEOUT="$2";   shift 2 ;;
        *) echo "Unknown arg: $1"; exit 1 ;;
    esac
done

mkdir -p "$RESULT_DIR"

# ── Build stubs BC if not present ──────────────────────────────────────────────
if [[ ! -f "$STUBS_BC" ]]; then
    echo "[setup] Building aria runtime stubs..."
    clang-14 -emit-llvm -c -O0 "$SCRIPT_DIR/aria_rt_stubs.c" -o "$STUBS_BC"
fi

# ── Counters ───────────────────────────────────────────────────────────────────
total=0; ok=0; errors=0; timeouts=0; compile_fail=0
start_ts=$(date +%s)

# ── JSON array for summary ─────────────────────────────────────────────────────
summary_file="$RESULT_DIR/klee_summary.json"
echo "[" > "$summary_file"
first=1

# ── Process each test ──────────────────────────────────────────────────────────
for npk_file in "$TEST_DIR"/*.npk; do
    [[ -f "$npk_file" ]] || continue
    name="$(basename "${npk_file%.npk}")"
    total=$((total + 1))

    out_dir="$RESULT_DIR/$name"
    mkdir -p "$out_dir"

    ll_file="$out_dir/${name}.ll"
    ll14_file="$out_dir/${name}_14.ll"
    bc14_file="$out_dir/${name}_14.bc"
    linked_file="$out_dir/${name}_linked.bc"
    klee_out="$out_dir/klee_out"
    log_file="$out_dir/run.log"

    printf "  %-50s " "$name"

    # Step 1: Compile to LLVM 20 IR
    if ! "$NPKC" --emit-llvm "$npk_file" -o "$ll_file" 2>"$out_dir/compile.err"; then
        echo "COMPILE_FAIL"
        compile_fail=$((compile_fail + 1))
        [[ $first -eq 0 ]] && echo "," >> "$summary_file"
        first=0
        echo "  {\"name\":\"$name\",\"status\":\"COMPILE_FAIL\"}" >> "$summary_file"
        continue
    fi

    # Step 2: Downgrade LLVM 20 → 14
    if ! python3 "$DOWNGRADE" "$ll_file" "$ll14_file" 2>"$out_dir/downgrade.err"; then
        echo "DOWNGRADE_FAIL"
        errors=$((errors + 1))
        [[ $first -eq 0 ]] && echo "," >> "$summary_file"
        first=0
        echo "  {\"name\":\"$name\",\"status\":\"DOWNGRADE_FAIL\"}" >> "$summary_file"
        continue
    fi

    # Step 3: Assemble to BC
    if ! "$LLVM_AS" "$ll14_file" -o "$bc14_file" 2>"$out_dir/as.err"; then
        echo "AS_FAIL"
        errors=$((errors + 1))
        [[ $first -eq 0 ]] && echo "," >> "$summary_file"
        first=0
        echo "  {\"name\":\"$name\",\"status\":\"AS_FAIL\"}" >> "$summary_file"
        continue
    fi

    # Step 4: Link with runtime stubs
    if ! "$LLVM_LINK" "$bc14_file" "$STUBS_BC" -o "$linked_file" 2>"$out_dir/link.err"; then
        echo "LINK_FAIL"
        errors=$((errors + 1))
        [[ $first -eq 0 ]] && echo "," >> "$summary_file"
        first=0
        echo "  {\"name\":\"$name\",\"status\":\"LINK_FAIL\"}" >> "$summary_file"
        continue
    fi

    # Step 5: Run KLEE with timeout
    rm -rf "$klee_out"
    klee_status=0
    timeout "$TIMEOUT" "$KLEE" \
        --libc=none \
        --max-time="${TIMEOUT}s" \
        --output-dir="$klee_out" \
        "$linked_file" > "$log_file" 2>&1 || klee_status=$?

    # Parse results
    completed_paths=$(grep "completed paths" "$log_file" | grep -oP '\d+' | tail -1 || echo 0)
    generated_tests=$(grep "generated tests" "$log_file" | grep -oP '\d+' | tail -1 || echo 0)
    klee_errors=$(grep -c "^KLEE: ERROR:" "$log_file" || true)
    timed_out=0

    if [[ $klee_status -eq 124 ]]; then
        timed_out=1
        timeouts=$((timeouts + 1))
        echo "TIMEOUT (${completed_paths} paths)"
        status_str="TIMEOUT"
    elif [[ $klee_errors -gt 0 ]]; then
        errors=$((errors + 1))
        echo "ERROR ($klee_errors errors, ${completed_paths} paths)"
        status_str="ERROR"
    else
        ok=$((ok + 1))
        echo "OK (${completed_paths} paths, ${generated_tests} tests)"
        status_str="OK"
    fi

    [[ $first -eq 0 ]] && echo "," >> "$summary_file"
    first=0
    cat >> "$summary_file" <<EOF
  {"name":"$name","status":"$status_str","completed_paths":$completed_paths,"generated_tests":$generated_tests,"klee_errors":$klee_errors,"timed_out":$timed_out}
EOF
done

echo "]" >> "$summary_file"

end_ts=$(date +%s)
elapsed=$((end_ts - start_ts))

echo ""
echo "=================================================="
echo "KLEE SYMBOLIC EXECUTION COMPLETE"
echo "=================================================="
printf "Total tests      : %d\n" "$total"
printf "OK (no errors)   : %d\n" "$ok"
printf "Errors found     : %d\n" "$errors"
printf "Timeouts         : %d\n" "$timeouts"
printf "Compile failures : %d\n" "$compile_fail"
printf "Elapsed          : %ds\n" "$elapsed"
printf "Summary          : %s\n" "$summary_file"
