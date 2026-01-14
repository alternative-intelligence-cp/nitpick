#!/bin/bash
# Simple Aria Compiler Fuzzer
# Runs mutation-based fuzzing on the Aria compiler

set -euo pipefail

ARIA_ROOT="/home/randy/._____RANDY_____/REPOS/aria"
ARIAC="$ARIA_ROOT/build/ariac"
CORPUS_DIR="$ARIA_ROOT/tests/fuzz/corpus"
CRASHES_DIR="$ARIA_ROOT/tests/fuzz/crashes"
WORK_DIR="/tmp/aria_fuzz_$$"

DURATION_HOURS="${1:-72}"
TIMEOUT_SEC=10

# Check compiler exists
if [ ! -f "$ARIAC" ]; then
    echo "ERROR: Compiler not found at $ARIAC"
    exit 1
fi

# Setup
mkdir -p "$WORK_DIR"
mkdir -p "$CRASHES_DIR"/{abort,segfault,timeout}

echo "=============================================="
echo "ARIA FUZZING CAMPAIGN"
echo "=============================================="
echo "Duration: $DURATION_HOURS hours"
echo "Compiler: $ARIAC"
echo "Corpus: $CORPUS_DIR"
echo "Crashes: $CRASHES_DIR"
echo "Start: $(date)"
echo "----------------------------------------------"

START_TIME=$(date +%s)
END_TIME=$((START_TIME + $(echo "$DURATION_HOURS * 3600" | bc | cut -d. -f1)))

RUNS=0
CRASHES=0
LAST_REPORT=$START_TIME

# Simple mutation functions
mutate_file() {
    local input="$1"
    local output="$2"
    local mutation=$((RANDOM % 10))
    
    case $mutation in
        0|1|2)  # Delete random line
            total_lines=$(wc -l < "$input")
            if [ "$total_lines" -gt 1 ]; then
                line_to_delete=$((1 + RANDOM % total_lines))
                sed "${line_to_delete}d" "$input" > "$output"
            else
                cp "$input" "$output"
            fi
            ;;
        3|4)  # Duplicate random line
            total_lines=$(wc -l < "$input")
            if [ "$total_lines" -gt 0 ]; then
                line_to_dup=$((1 + RANDOM % total_lines))
                sed "${line_to_dup}p" "$input" > "$output"
            else
                cp "$input" "$output"
            fi
            ;;
        5|6)  # Insert random character
            # Just copy for now - character insertion is complex in bash
            cp "$input" "$output"
            echo "X" >> "$output"  # Append garbage
            ;;
        *)  # No mutation (baseline)
            cp "$input" "$output"
            ;;
    esac
}

# Main fuzzing loop
while [ "$(date +%s)" -lt "$END_TIME" ]; do
    # Pick random corpus file
    CORPUS_FILES=("$CORPUS_DIR"/*.aria)
    SEED_FILE="${CORPUS_FILES[$RANDOM % ${#CORPUS_FILES[@]}]}"
    
    # Mutate it
    TEST_FILE="$WORK_DIR/test_$RUNS.aria"
    mutate_file "$SEED_FILE" "$TEST_FILE"
    
    # Run compiler with timeout
    OUTPUT="$WORK_DIR/out_$RUNS"
    if timeout $TIMEOUT_SEC "$ARIAC" "$TEST_FILE" -o "$OUTPUT" > "$WORK_DIR/stdout" 2> "$WORK_DIR/stderr"; then
        # Success
        :
    else
        EXIT_CODE=$?
        
        # Check for real crashes (not just compilation errors)
        if [ $EXIT_CODE -eq 124 ]; then
            # Timeout
            HASH=$(sha256sum "$TEST_FILE" | cut -d' ' -f1 | head -c 16)
            cp "$TEST_FILE" "$CRASHES_DIR/timeout/crash_${HASH}.aria"
            echo "[TIMEOUT] Saved to crashes/timeout/crash_${HASH}.aria"
            CRASHES=$((CRASHES + 1))
        elif [ $EXIT_CODE -ge 128 ] && [ $EXIT_CODE -ne 130 ]; then
            # Signal (segfault, abort, etc.)
            SIGNAL=$((EXIT_CODE - 128))
            HASH=$(sha256sum "$TEST_FILE" | cut -d' ' -f1 | head -c 16)
            
            if [ $SIGNAL -eq 11 ]; then
                # SIGSEGV
                cp "$TEST_FILE" "$CRASHES_DIR/segfault/crash_${HASH}.aria"
                echo "[SEGFAULT] Saved to crashes/segfault/crash_${HASH}.aria"
            elif [ $SIGNAL -eq 6 ]; then
                # SIGABRT
                cp "$TEST_FILE" "$CRASHES_DIR/abort/crash_${HASH}.aria"
                echo "[ABORT] Saved to crashes/abort/crash_${HASH}.aria"
            else
                cp "$TEST_FILE" "$CRASHES_DIR/abort/crash_sig${SIGNAL}_${HASH}.aria"
                echo "[SIGNAL $SIGNAL] Saved to crashes/abort/crash_sig${SIGNAL}_${HASH}.aria"
            fi
            CRASHES=$((CRASHES + 1))
        fi
        # Exit code 1 is normal compilation error, not a crash
    fi
    
    RUNS=$((RUNS + 1))
    
    # Periodic report
    NOW=$(date +%s)
    if [ $((NOW - LAST_REPORT)) -ge 60 ]; then
        ELAPSED=$((NOW - START_TIME))
        EXEC_PER_SEC=$(echo "scale=2; $RUNS / $ELAPSED" | bc)
        HOURS=$(echo "scale=1; $ELAPSED / 3600" | bc)
        echo "[${HOURS}h] runs=$RUNS crashes=$CRASHES exec/s=$EXEC_PER_SEC"
        LAST_REPORT=$NOW
    fi
done

# Final report
TOTAL_TIME=$(($(date +%s) - START_TIME))
EXEC_PER_SEC=$(echo "scale=2; $RUNS / $TOTAL_TIME" | bc)

echo ""
echo "=============================================="
echo "FUZZING COMPLETE"
echo "=============================================="
echo "Duration: $(echo "scale=2; $TOTAL_TIME / 3600" | bc) hours"
echo "Total runs: $RUNS"
echo "Exec/sec: $EXEC_PER_SEC"
echo "Crashes found: $CRASHES"
echo "End: $(date)"
echo "=============================================="

# Cleanup
rm -rf "$WORK_DIR"

exit 0
