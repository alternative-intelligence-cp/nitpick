#!/usr/bin/env bash
# aria_frama_c.sh — Batch Frama-C Eva analysis for aria-libc shim stubs
# Analyzes each exported function individually with -lib-entry
# Usage: ./aria_frama_c.sh [precision] [stub_dir]
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORKSPACE_DIR="$(cd "$ROOT_DIR/.." && pwd)"

PRECISION="${1:-3}"
DEFAULT_STUB_DIR="${STUB_DIR:-$WORKSPACE_DIR/aria-libc/shim/stubs}"
STUB_DIR="${2:-$DEFAULT_STUB_DIR}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_DIR="${RESULTS_DIR:-$SCRIPT_DIR/results_${TIMESTAMP}}"
mkdir -p "$RESULTS_DIR"

SUMMARY="$RESULTS_DIR/summary.txt"
CSV="$RESULTS_DIR/results.csv"

# Counters
TOTAL=0
SAFE=0
ALARMS=0
ERRORS=0

echo "file,function,status,alarm_count,details" > "$CSV"
echo "=== Frama-C Eva Batch Analysis ===" > "$SUMMARY"
echo "Precision: $PRECISION" >> "$SUMMARY"
echo "Stub dir:  $STUB_DIR" >> "$SUMMARY"
echo "Started:   $(date)" >> "$SUMMARY"
echo "" >> "$SUMMARY"

for src in "$STUB_DIR"/*.c; do
    fname=$(basename "$src")
    echo ""
    echo "━━━ Analyzing: $fname ━━━"

    # Skip files with pthread/atomics (Frama-C Manganese can't parse them)
    if grep -qE '_Atomic|#include <pthread.h>|#include <semaphore' "$src" 2>/dev/null; then
        echo "  [SKIP] Uses pthreads/_Atomic (not supported by Frama-C Manganese)"
        continue
    fi

    # Extract function names: any non-static function starting with aria_libc_
    # Match return-type aria_libc_funcname( on lines that don't start with "static"
    funcs=$(grep -P '^\s*(?!static)\s*\w[\w\s\*]*\s+(aria_libc_\w+)\s*\(' "$src" 2>/dev/null | grep -oP 'aria_libc_\w+' || true)

    if [[ -z "$funcs" ]]; then
        echo "  [SKIP] No exported functions found"
        continue
    fi

    for func in $funcs; do
        ((TOTAL++)) || true
        echo -n "  $func ... "

        LOGFILE="$RESULTS_DIR/${fname%.c}__${func}.log"

        # Run Frama-C Eva with timeout
        timeout 60 frama-c -eva -eva-precision "$PRECISION" \
            -lib-entry -main "$func" \
            -no-unicode \
            "$src" > "$LOGFILE" 2>&1
        exit_code=$?

        if [[ $exit_code -eq 124 ]]; then
            echo "TIMEOUT"
            echo "$fname,$func,timeout,0," >> "$CSV"
            ((ERRORS++)) || true
        elif grep -q '\[kernel\].*cannot find entry point' "$LOGFILE" 2>/dev/null; then
            echo "SKIP (entry not found)"
            ((TOTAL--)) || true
        elif grep -q '\[kernel\].*aborted' "$LOGFILE" 2>/dev/null; then
            error_msg=$(grep -i 'error\|aborted' "$LOGFILE" 2>/dev/null | head -1 || echo "unknown")
            echo "ERROR: $error_msg"
            echo "$fname,$func,error,0,\"$error_msg\"" >> "$CSV"
            ((ERRORS++)) || true
        else
            # Parse results — use wc to avoid multi-line grep -c issues
            alarm_count=$(grep '\[eva:alarm\]' "$LOGFILE" 2>/dev/null | wc -l)

            if [[ "$alarm_count" -eq 0 ]]; then
                echo "SAFE"
                echo "$fname,$func,safe,0," >> "$CSV"
                ((SAFE++)) || true
            else
                # Extract alarm details
                alarm_details=$(grep '\[eva:alarm\]' "$LOGFILE" | sed 's/.*Warning: //' | tr '\n' '; ')
                echo "ALARM ($alarm_count): $alarm_details"
                echo "$fname,$func,alarm,$alarm_count,\"$alarm_details\"" >> "$CSV"
                ((ALARMS++)) || true
            fi
        fi
    done
done

echo "" >> "$SUMMARY"
echo "=== RESULTS ===" >> "$SUMMARY"
echo "Total functions: $TOTAL" >> "$SUMMARY"
echo "Safe (0 alarms): $SAFE" >> "$SUMMARY"
echo "With alarms:     $ALARMS" >> "$SUMMARY"
echo "Errors/timeouts: $ERRORS" >> "$SUMMARY"
echo "Finished:        $(date)" >> "$SUMMARY"

# Print alarm summary
echo "" >> "$SUMMARY"
echo "=== ALARM DETAILS ===" >> "$SUMMARY"
grep ',alarm,' "$CSV" | while IFS=',' read -r file func status count details; do
    echo "  $file :: $func — $details" >> "$SUMMARY"
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TOTAL: $TOTAL | SAFE: $SAFE | ALARMS: $ALARMS | ERRORS: $ERRORS"
echo "Results: $RESULTS_DIR"
echo "Summary: $SUMMARY"
cat "$SUMMARY"
