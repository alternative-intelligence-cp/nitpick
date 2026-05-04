#!/bin/bash
# aria_ikos.sh — Run IKOS abstract interpretation on Aria programs
#
# Pipeline: ariac --emit-llvm → ll_downgrade.py → llvm-as-14 → ikos
#
# Usage:
#   ./aria_ikos.sh <file.aria>                    # Single file analysis
#   ./aria_ikos.sh --batch <dir> [--domain D]     # Batch analysis
#   ./aria_ikos.sh --batch <dir> --summary        # Batch with summary only

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
ARIAC="${ARIAC:-$ROOT_DIR/build/ariac}"
DOWNGRADE="${SCRIPT_DIR}/ll_downgrade.py"
RESULTS_DIR="${SCRIPT_DIR}/results"
LLVM_AS="llvm-as-14"
DOMAIN="${DOMAIN:-interval}"
TMPDIR_BASE="/tmp/aria-ikos"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

mkdir -p "$TMPDIR_BASE" "$RESULTS_DIR"

analyze_one() {
    local aria_file="$1"
    local basename="$(basename "$aria_file" .aria)"
    local workdir="$TMPDIR_BASE/$basename"
    mkdir -p "$workdir"

    local ll_file="$workdir/${basename}.ll"
    local typed_file="$workdir/${basename}_typed.ll"
    local bc_file="$workdir/${basename}.bc"
    local db_file="$workdir/${basename}.db"

    # Step 1: Compile to LLVM text IR
    if ! "$ARIAC" "$aria_file" --emit-llvm -o "$ll_file" 2>/dev/null; then
        echo -e "${RED}COMPILE_FAIL${NC} $aria_file"
        echo "COMPILE_FAIL" > "$workdir/status"
        return 1
    fi

    # Step 2: Downgrade opaque pointers to typed pointers
    if ! python3 "$DOWNGRADE" "$ll_file" "$typed_file" 2>/dev/null; then
        echo -e "${RED}DOWNGRADE_FAIL${NC} $aria_file"
        echo "DOWNGRADE_FAIL" > "$workdir/status"
        return 1
    fi

    # Step 3: Assemble to LLVM 14 bitcode
    if ! "$LLVM_AS" "$typed_file" -o "$bc_file" 2>/dev/null; then
        echo -e "${YELLOW}ASM_FAIL${NC} $aria_file"
        echo "ASM_FAIL" > "$workdir/status"
        return 1
    fi

    # Step 4: Run IKOS analysis
    local ikos_output
    ikos_output=$(ikos -d "$DOMAIN" "$bc_file" -o "$db_file" 2>&1) || true

    # Parse results
    local total safe unsafe warnings
    total=$(echo "$ikos_output" | grep "Total number of checks" | head -1 | awk '{print $NF}')
    safe=$(echo "$ikos_output" | grep "safe checks" | awk '{print $NF}')
    unsafe=$(echo "$ikos_output" | grep "definite unsafe" | awk '{print $NF}')
    warnings=$(echo "$ikos_output" | grep "Total number of warnings" | awk '{print $NF}')

    # Count runtime warnings (extern function calls, not real issues)
    local runtime_warnings
    runtime_warnings=$(echo "$ikos_output" | grep -c "ignored side effect of call to extern" || true)

    # Subtract runtime warnings from warning count for real warnings
    local real_warnings=$((${warnings:-0} - ${runtime_warnings:-0}))

    if [[ "${unsafe:-0}" -gt 0 ]]; then
        echo -e "${RED}UNSAFE${NC}  checks=${total} safe=${safe} unsafe=${unsafe} warn=${real_warnings} runtime_warn=${runtime_warnings}  $aria_file"
        echo "UNSAFE" > "$workdir/status"
    elif [[ "${real_warnings}" -gt 0 ]]; then
        echo -e "${YELLOW}WARN${NC}    checks=${total} safe=${safe} warn=${real_warnings} runtime_warn=${runtime_warnings}  $aria_file"
        echo "WARN" > "$workdir/status"
    else
        echo -e "${GREEN}SAFE${NC}    checks=${total} safe=${safe} runtime_warn=${runtime_warnings}  $aria_file"
        echo "SAFE" > "$workdir/status"
    fi

    # Save full output
    echo "$ikos_output" > "$workdir/ikos_output.txt"

    # Save to results dir
    echo "$ikos_output" > "$RESULTS_DIR/${basename}.txt"
}

batch_analyze() {
    local dir="$1"
    local summary_only="${2:-false}"

    local total=0 safe=0 warn=0 unsafe=0 compile_fail=0 asm_fail=0 downgrade_fail=0

    echo -e "${CYAN}=== IKOS Batch Analysis (domain: ${DOMAIN}) ===${NC}"
    echo "Source directory: $dir"
    echo ""

    for f in "$dir"/*.aria; do
        [[ -f "$f" ]] || continue
        total=$((total + 1))

        if analyze_one "$f"; then
            local basename="$(basename "$f" .aria)"
            local status
            status=$(cat "$TMPDIR_BASE/$basename/status" 2>/dev/null || echo "UNKNOWN")
            case "$status" in
                SAFE) safe=$((safe + 1)) ;;
                WARN) warn=$((warn + 1)) ;;
                UNSAFE) unsafe=$((unsafe + 1)) ;;
            esac
        else
            local basename="$(basename "$f" .aria)"
            local status
            status=$(cat "$TMPDIR_BASE/$basename/status" 2>/dev/null || echo "UNKNOWN")
            case "$status" in
                COMPILE_FAIL) compile_fail=$((compile_fail + 1)) ;;
                ASM_FAIL) asm_fail=$((asm_fail + 1)) ;;
                DOWNGRADE_FAIL) downgrade_fail=$((downgrade_fail + 1)) ;;
            esac
        fi
    done

    echo ""
    echo -e "${CYAN}=== Summary ===${NC}"
    echo "Total files:      $total"
    echo -e "  ${GREEN}Safe:           $safe${NC}"
    echo -e "  ${YELLOW}Warnings:       $warn${NC}"
    echo -e "  ${RED}Unsafe:         $unsafe${NC}"
    echo "  Compile fail:   $compile_fail"
    echo "  IR downgrade:   $downgrade_fail"
    echo "  Assembly fail:  $asm_fail"

    # Save summary
    cat > "$RESULTS_DIR/SUMMARY.txt" << EOF
IKOS Batch Analysis Summary
Domain: ${DOMAIN}
Source: ${dir}
Date: $(date -u +"%Y-%m-%d %H:%M UTC")

Total:          $total
Safe:           $safe
Warnings:       $warn
Unsafe:         $unsafe
Compile fail:   $compile_fail
IR downgrade:   $downgrade_fail
Assembly fail:  $asm_fail
EOF
}

# --- Main ---
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <file.aria>"
    echo "       $0 --batch <directory> [--domain interval|congruence]"
    exit 1
fi

# Parse args
BATCH=false
SUMMARY=false
INPUT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --batch) BATCH=true; INPUT="$2"; shift 2 ;;
        --domain) DOMAIN="$2"; shift 2 ;;
        --summary) SUMMARY=true; shift ;;
        *) INPUT="$1"; shift ;;
    esac
done

if $BATCH; then
    batch_analyze "$INPUT" "$SUMMARY"
else
    analyze_one "$INPUT"
fi
