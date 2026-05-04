#!/bin/bash
# aria_alive2.sh — Alive2 translation validation harness for ariac
# Validates that LLVM optimization passes preserve semantics of ariac-emitted IR
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORKSPACE_DIR="$(cd "$ROOT_DIR/.." && pwd)"

ARIAC="${ARIAC:-$ROOT_DIR/build/ariac}"
ALIVE_TV="${ALIVE_TV:-$WORKSPACE_DIR/alive2/build/alive-tv}"
OPT="${OPT:-opt-20}"
TEST_DIR="${TEST_DIR:-$ROOT_DIR/tests}"
RESULT_DIR="${RESULT_DIR:-$SCRIPT_DIR/results}"
IR_DIR="${IR_DIR:-$SCRIPT_DIR/ir_cache}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT="$RESULT_DIR/alive2_report_${TIMESTAMP}.txt"

mkdir -p "$RESULT_DIR" "$IR_DIR"

# Counters
TOTAL=0
CORRECT=0
INCORRECT=0
FAILED_PROVE=0
COMPILE_FAIL=0
OPT_FAIL=0
TIMEOUT_COUNT=0
ERRORS=0

# Configurable
OPT_LEVEL="${OPT_LEVEL:-O2}"          # LLVM optimization level
ALIVE_TIMEOUT="${ALIVE_TIMEOUT:-15}"   # Per-file timeout in seconds
SKIP_PATTERNS="${SKIP_PATTERNS:-gpu,quantum,fuse,packages,stress}"  # Comma-separated skip patterns

log() { echo "$1" | tee -a "$REPORT"; }

log "═══════════════════════════════════════════════════════════"
log " Alive2 Translation Validation Report — ariac test suite"
log " Date: $(date)"
log " Alive2: $ALIVE_TV"
log " ariac: $ARIAC"
log " opt: $OPT (-$OPT_LEVEL)"
log " Timeout: ${ALIVE_TIMEOUT}s per file"
log "═══════════════════════════════════════════════════════════"
log ""

# Collect all .aria test files
mapfile -t TEST_FILES < <(find "$TEST_DIR" -name '*.aria' -type f | sort)
TOTAL_FILES=${#TEST_FILES[@]}
log "Found $TOTAL_FILES test files"
log ""

analyze_file() {
    local aria_file="$1"
    local rel_path="${aria_file#$TEST_DIR/}"
    local safe_name=$(echo "$rel_path" | tr '/' '_' | sed 's/\.aria$//')
    local src_ll="$IR_DIR/${safe_name}_src.ll"
    local tgt_ll="$IR_DIR/${safe_name}_tgt.ll"

    # Check skip patterns
    if [[ -n "$SKIP_PATTERNS" ]]; then
        IFS=',' read -ra PATS <<< "$SKIP_PATTERNS"
        for pat in "${PATS[@]}"; do
            if [[ "$rel_path" == *"$pat"* ]]; then
                log "  [SKIP] $rel_path (matches skip pattern '$pat')"
                return
            fi
        done
    fi

    # Step 1: Compile to unoptimized IR
    if ! "$ARIAC" "$aria_file" --emit-llvm -O0 -o "$src_ll" 2>/dev/null; then
        log "  [COMPILE-FAIL] $rel_path"
        ((COMPILE_FAIL++)) || true
        ((TOTAL++)) || true
        return
    fi

    # Step 2: Optimize with opt
    if ! "$OPT" "-$OPT_LEVEL" -S "$src_ll" -o "$tgt_ll" 2>/dev/null; then
        log "  [OPT-FAIL] $rel_path"
        ((OPT_FAIL++)) || true
        ((TOTAL++)) || true
        rm -f "$src_ll"
        return
    fi

    # Step 3: Run alive-tv with SMT timeout
    local output
    if ! output=$(timeout "${ALIVE_TIMEOUT}s" "$ALIVE_TV" --smt-to=5000 "$src_ll" "$tgt_ll" 2>&1); then
        local exit_code=$?
        if [[ $exit_code -eq 124 ]]; then
            log "  [TIMEOUT] $rel_path (>${ALIVE_TIMEOUT}s)"
            ((TIMEOUT_COUNT++)) || true
        else
            log "  [ERROR] $rel_path (exit $exit_code)"
            ((ERRORS++)) || true
        fi
        ((TOTAL++)) || true
        rm -f "$src_ll" "$tgt_ll"
        return
    fi

    # Parse alive-tv summary line (tail -1 to get only global summary)
    local correct=$(echo "$output" | grep -oP '\d+ correct' | tail -1 | grep -oP '\d+' || echo 0)
    local incorrect=$(echo "$output" | grep -oP '\d+ incorrect' | tail -1 | grep -oP '\d+' || echo 0)
    local failed_prove=$(echo "$output" | grep -oP '\d+ failed-to-prove' | tail -1 | grep -oP '\d+' || echo 0)
    local alive_errors=$(echo "$output" | grep -oP '\d+ Alive2 errors' | tail -1 | grep -oP '\d+' || echo 0)

    ((TOTAL++)) || true

    if [[ "$incorrect" -gt 0 ]]; then
        log "  [INCORRECT] $rel_path — $incorrect incorrect transformation(s)!"
        # Save full output for incorrect transformations
        echo "$output" > "$RESULT_DIR/${safe_name}_INCORRECT.txt"
        ((INCORRECT += incorrect)) || true
        ((CORRECT += correct)) || true
        ((FAILED_PROVE += failed_prove)) || true
    elif [[ "$failed_prove" -gt 0 ]]; then
        log "  [UNPROVEN] $rel_path — $failed_prove unproven, $correct correct"
        echo "$output" > "$RESULT_DIR/${safe_name}_UNPROVEN.txt"
        ((CORRECT += correct)) || true
        ((FAILED_PROVE += failed_prove)) || true
    elif [[ "$alive_errors" -gt 0 ]]; then
        log "  [ALIVE-ERR] $rel_path — $alive_errors Alive2 errors"
        echo "$output" > "$RESULT_DIR/${safe_name}_ALIVE_ERR.txt"
        ((ERRORS += alive_errors)) || true
        ((CORRECT += correct)) || true
    elif [[ "$correct" -gt 0 ]]; then
        log "  [OK] $rel_path — $correct correct"
        ((CORRECT += correct)) || true
    else
        log "  [UNKNOWN] $rel_path — no summary parsed"
        echo "$output" > "$RESULT_DIR/${safe_name}_UNKNOWN.txt"
    fi

    # Clean up IR files for passing tests (keep failures for inspection)
    if [[ "$incorrect" -eq 0 && "$failed_prove" -eq 0 ]]; then
        rm -f "$src_ll" "$tgt_ll"
    fi
}

# Main loop
FILE_NUM=0
for aria_file in "${TEST_FILES[@]}"; do
    ((FILE_NUM++)) || true
    printf "\r[%d/%d] " "$FILE_NUM" "$TOTAL_FILES" >&2
    analyze_file "$aria_file"
done
echo "" >&2

log ""
log "═══════════════════════════════════════════════════════════"
log " Summary"
log "═══════════════════════════════════════════════════════════"
log "  Files processed:     $TOTAL / $TOTAL_FILES"
log "  Compile failures:    $COMPILE_FAIL"
log "  Opt failures:        $OPT_FAIL"
log "  Transformations:"
log "    Correct:           $CORRECT"
log "    Incorrect:         $INCORRECT"
log "    Failed-to-prove:   $FAILED_PROVE"
log "    Alive2 errors:     $ERRORS"
log "    Timeouts:          $TIMEOUT_COUNT"
log ""

if [[ "$INCORRECT" -gt 0 ]]; then
    log "⚠  INCORRECT TRANSFORMATIONS DETECTED — review files in $RESULT_DIR"
elif [[ "$FAILED_PROVE" -gt 0 ]]; then
    log "⚠  Some transformations could not be proven — manual review recommended"
else
    log "✓  All verified transformations are semantically correct"
fi

log ""
log "Report: $REPORT"
log "IR cache: $IR_DIR"
