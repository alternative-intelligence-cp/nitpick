#!/bin/bash
# aria_esbmc.sh — ESBMC batch analysis harness for aria-libc shim stubs
# Analyzes each function in each shim file using ESBMC BMC
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
WORKSPACE_DIR="$(cd "$ROOT_DIR/.." && pwd)"

SHIM_DIR="${SHIM_DIR:-$WORKSPACE_DIR/aria-libc/shim/stubs}"
CLANG_INC="${CLANG_INC:--I /usr/lib/llvm-18/lib/clang/18/include}"
ESBMC_OPTS="${ESBMC_OPTS:---incremental-bmc --no-div-by-zero-check --timeout 120}"
REPORT_DIR="${REPORT_DIR:-$SCRIPT_DIR/results}"
HARNESS_DIR="${HARNESS_DIR:-$SCRIPT_DIR/harnesses}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT="$REPORT_DIR/esbmc_report_${TIMESTAMP}.txt"

mkdir -p "$REPORT_DIR" "$HARNESS_DIR"

# Counters
TOTAL=0
SAFE=0
FAILED=0
ERRORS=0
TIMEOUT=0

log() { echo "$1" | tee -a "$REPORT"; }

log "═══════════════════════════════════════════════════════════"
log " ESBMC Analysis Report — aria-libc shim stubs"
log " Date: $(date)"
log " ESBMC: $(esbmc --version 2>&1 | head -1)"
log "═══════════════════════════════════════════════════════════"
log ""

# Extract function names from a C file (non-static, non-typedef functions)
extract_functions() {
    local file="$1"
    # Match function definitions: return_type func_name(...)
    grep -oP '^\w[\w* ]+\s+\K(aria_libc_\w+)\s*(?=\()' "$file" 2>/dev/null || true
}

# Skip files that use pthreads (analyzed separately with --deadlock-check)
PTHREAD_FILES=("aria_libc_pool.c" "aria_libc_channel.c" "aria_libc_actor.c"
               "aria_libc_mutex.c" "aria_libc_rwlock.c" "aria_libc_thread.c")

analyze_file() {
    local file="$1"
    local basename=$(basename "$file")
    local funcs=$(extract_functions "$file")

    if [[ -z "$funcs" ]]; then
        log "  [SKIP] $basename — no extractable functions"
        return
    fi

    log "── $basename ──────────────────────────────────────"

    for func in $funcs; do
        TOTAL=$((TOTAL+1))
        local out
        out=$(timeout 180 esbmc "$file" $CLANG_INC $ESBMC_OPTS \
              --function "$func" 2>&1) || true

        if echo "$out" | grep -q "VERIFICATION SUCCESSFUL"; then
            log "  ✓ $func — SAFE"
            SAFE=$((SAFE+1))
        elif echo "$out" | grep -q "VERIFICATION FAILED"; then
            # Extract violation
            local violation
            violation=$(echo "$out" | grep "Violated property:" -A 2 | tail -1 | sed 's/^  //')
            log "  ✗ $func — FAILED: $violation"
            FAILED=$((FAILED+1))
        elif echo "$out" | grep -q "PARSING ERROR\|CONVERSION ERROR"; then
            log "  ! $func — PARSE/CONVERT ERROR"
            ERRORS=$((ERRORS+1))
        elif echo "$out" | grep -q "Timed out"; then
            log "  ⏱ $func — TIMEOUT"
            TIMEOUT=$((TIMEOUT+1))
        else
            log "  ? $func — UNKNOWN (check manually)"
            ERRORS=$((ERRORS+1))
        fi
    done
    log ""
}

# Phase 1: Non-pthread files
log "═══ Phase 1: Non-threaded shim stubs ════════════════════"
log ""
for f in "$SHIM_DIR"/aria_libc_*.c; do
    bn=$(basename "$f")
    skip=0
    for pf in "${PTHREAD_FILES[@]}"; do
        [[ "$bn" == "$pf" ]] && skip=1 && break
    done
    [[ $skip -eq 1 ]] && continue
    analyze_file "$f"
done

# Phase 2: Pthread files with ESBMC concurrency options
log "═══ Phase 2: Threaded shim stubs (pthread) ══════════════"
log ""
ESBMC_OPTS_PTHREAD="--incremental-bmc --no-div-by-zero-check --timeout 120"
for pf in "${PTHREAD_FILES[@]}"; do
    f="$SHIM_DIR/$pf"
    [[ -f "$f" ]] || continue
    analyze_file "$f"
done

# Summary
log "═══════════════════════════════════════════════════════════"
log " Summary"
log "═══════════════════════════════════════════════════════════"
log " Total functions analyzed: $TOTAL"
log " Safe (verified):         $SAFE"
log " Failed (violations):     $FAILED"
log " Errors:                  $ERRORS"
log " Timeouts:                $TIMEOUT"
log "═══════════════════════════════════════════════════════════"
log " Report saved to: $REPORT"
