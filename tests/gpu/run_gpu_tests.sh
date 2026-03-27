#!/bin/bash
# ============================================================================
# Aria GPU Test Runner
# Tests PTX codegen, validates PTX assembly, and runs CUDA tests
# ============================================================================
#
# Usage: ./run_gpu_tests.sh [OPTIONS]
#
# Options:
#   --skip-cuda     Skip CUDA hardware tests (PTX validation only)
#   --gpu-arch=SM   Set GPU architecture (default: auto-detect or sm_86)
#   --verbose       Show detailed output
#   --help          Show this help
#
# Requires:
#   - ariac compiler (../build/ariac)
#   - CUDA toolkit (ptxas, nvcc) — optional, tests skipped if not found
#
# Exit codes:
#   0  All tests passed (or skipped gracefully)
#   1  One or more tests failed

set -euo pipefail

# ============================================================================
# Configuration
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ARIAC="$REPO_ROOT/build/ariac"
TMP_DIR="/tmp/aria_gpu_tests_$$"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Counters
TOTAL=0
PASSED=0
FAILED=0
SKIPPED=0

# Options
SKIP_CUDA=false
GPU_ARCH=""
VERBOSE=false

# ============================================================================
# Argument Parsing
# ============================================================================

for arg in "$@"; do
    case "$arg" in
        --skip-cuda)   SKIP_CUDA=true ;;
        --gpu-arch=*)  GPU_ARCH="${arg#*=}" ;;
        --verbose)     VERBOSE=true ;;
        --help)
            head -18 "$0" | tail -16
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            exit 1
            ;;
    esac
done

# ============================================================================
# Setup
# ============================================================================

mkdir -p "$TMP_DIR"
trap 'rm -rf "$TMP_DIR"' EXIT

log() {
    if $VERBOSE; then
        echo -e "  ${CYAN}[verbose]${NC} $1"
    fi
}

pass_test() {
    local name="$1"
    TOTAL=$((TOTAL + 1))
    PASSED=$((PASSED + 1))
    echo -e "  ${GREEN}✓ PASS${NC}: $name"
}

fail_test() {
    local name="$1"
    local reason="${2:-}"
    TOTAL=$((TOTAL + 1))
    FAILED=$((FAILED + 1))
    echo -e "  ${RED}✗ FAIL${NC}: $name"
    if [ -n "$reason" ]; then
        echo -e "         ${RED}$reason${NC}"
    fi
}

skip_test() {
    local name="$1"
    local reason="${2:-}"
    TOTAL=$((TOTAL + 1))
    SKIPPED=$((SKIPPED + 1))
    echo -e "  ${YELLOW}○ SKIP${NC}: $name${reason:+ ($reason)}"
}

# ============================================================================
# Environment Detection
# ============================================================================

detect_environment() {
    echo -e "${BOLD}═══════════════════════════════════════════${NC}"
    echo -e "${BOLD}  Aria GPU Test Runner${NC}"
    echo -e "${BOLD}═══════════════════════════════════════════${NC}"
    echo ""

    # Check ariac compiler
    if [ ! -x "$ARIAC" ]; then
        echo -e "${RED}Error: ariac compiler not found at $ARIAC${NC}"
        echo -e "${YELLOW}Build with: cd build && cmake .. && make -j\$(nproc)${NC}"
        exit 1
    fi
    echo -e "  Compiler: ${GREEN}$ARIAC${NC}"

    # Detect CUDA toolkit
    CUDA_HOME=""
    NVCC=""
    PTXAS=""

    for cuda_dir in /usr/local/cuda-13.2 /usr/local/cuda /opt/cuda; do
        if [ -x "$cuda_dir/bin/ptxas" ]; then
            CUDA_HOME="$cuda_dir"
            PTXAS="$cuda_dir/bin/ptxas"
            NVCC="$cuda_dir/bin/nvcc"
            break
        fi
    done

    if [ -n "$CUDA_HOME" ]; then
        local cuda_ver
        cuda_ver=$("$PTXAS" --version 2>&1 | grep -oP 'release \K[0-9.]+' || echo "unknown")
        echo -e "  CUDA:     ${GREEN}$CUDA_HOME (v$cuda_ver)${NC}"
    else
        echo -e "  CUDA:     ${YELLOW}not found (PTX validation tests will be skipped)${NC}"
    fi

    # Detect GPU
    HAS_GPU=false
    if command -v nvidia-smi &>/dev/null; then
        local gpu_name
        gpu_name=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -1)
        if [ -n "$gpu_name" ]; then
            HAS_GPU=true
            echo -e "  GPU:      ${GREEN}$gpu_name${NC}"

            # Auto-detect compute capability for GPU arch
            if [ -z "$GPU_ARCH" ]; then
                local cc
                cc=$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader 2>/dev/null | head -1 | tr -d '.')
                if [ -n "$cc" ]; then
                    GPU_ARCH="sm_$cc"
                fi
            fi
        fi
    fi

    if ! $HAS_GPU; then
        echo -e "  GPU:      ${YELLOW}not detected${NC}"
    fi

    # Default GPU arch
    if [ -z "$GPU_ARCH" ]; then
        GPU_ARCH="sm_50"
    fi
    echo -e "  Arch:     ${CYAN}$GPU_ARCH${NC}"
    echo ""
}

# ============================================================================
# Test: PTX File Validation (ptxas)
# Validates pre-built PTX files are syntactically correct
# ============================================================================

test_ptx_validation() {
    echo -e "${BOLD}── PTX Validation (pre-built files) ──${NC}"

    if [ -z "$PTXAS" ]; then
        skip_test "PTX validation" "ptxas not found"
        return
    fi

    local ptx_files=("$SCRIPT_DIR"/*.ptx)
    if [ ${#ptx_files[@]} -eq 0 ] || [ ! -f "${ptx_files[0]}" ]; then
        skip_test "PTX validation" "no .ptx files found"
        return
    fi

    for ptx_file in "${ptx_files[@]}"; do
        local name
        name=$(basename "$ptx_file")
        log "Validating: $ptx_file"

        # Skip full-program PTX that references runtime externs (not standalone kernels)
        if grep -q '\.extern .func aria_gc_init' "$ptx_file" 2>/dev/null; then
            skip_test "ptxas validate: $name" "full-program PTX (needs runtime linking)"
            continue
        fi

        if "$PTXAS" --gpu-name "$GPU_ARCH" "$ptx_file" -o "$TMP_DIR/${name%.ptx}.cubin" 2>"$TMP_DIR/ptxas_err.log"; then
            pass_test "ptxas validate: $name"
        else
            local err
            err=$(cat "$TMP_DIR/ptxas_err.log")
            fail_test "ptxas validate: $name" "$err"
        fi
    done
    echo ""
}

# ============================================================================
# Test: --emit-ptx Codegen
# Verifies the compiler can generate PTX output from Aria source
# ============================================================================

test_emit_ptx() {
    echo -e "${BOLD}── PTX Codegen (--emit-ptx) ──${NC}"

    # Find compilable .aria files in gpu test directory
    local aria_files=("$SCRIPT_DIR"/*.aria)
    if [ ${#aria_files[@]} -eq 0 ] || [ ! -f "${aria_files[0]}" ]; then
        skip_test "--emit-ptx codegen" "no .aria files found"
        return
    fi

    for aria_file in "${aria_files[@]}"; do
        local name
        name=$(basename "$aria_file" .aria)
        local ptx_out="$TMP_DIR/${name}.ptx"

        log "Compiling: $aria_file → PTX"

        # Compile to PTX (suppress debug output, capture errors)
        if "$ARIAC" "$aria_file" --emit-ptx --gpu-arch="$GPU_ARCH" -o "$ptx_out" 2>"$TMP_DIR/ariac_err.log"; then
            # Verify output file exists and has PTX header
            if [ -f "$ptx_out" ] && head -3 "$ptx_out" | grep -q "Generated by LLVM NVPTX"; then
                # Check target matches requested arch
                if grep -q ".target ${GPU_ARCH}" "$ptx_out"; then
                    pass_test "--emit-ptx: $name (target=$GPU_ARCH)"
                else
                    local actual_target
                    actual_target=$(grep "^\.target" "$ptx_out" | head -1)
                    fail_test "--emit-ptx: $name" "Wrong target: $actual_target (expected $GPU_ARCH)"
                fi
            else
                fail_test "--emit-ptx: $name" "Output missing or invalid PTX header"
            fi
        else
            local err
            err=$(grep -v '^\[DEBUG' "$TMP_DIR/ariac_err.log" | head -5)
            fail_test "--emit-ptx: $name" "Compilation failed: $err"
        fi
    done
    echo ""
}

# ============================================================================
# Test: CUDA C++ Compilation
# Compiles standalone .cu test files with nvcc
# ============================================================================

test_cuda_compilation() {
    echo -e "${BOLD}── CUDA C++ Tests (.cu files) ──${NC}"

    if $SKIP_CUDA; then
        skip_test "CUDA compilation" "--skip-cuda flag set"
        echo ""
        return
    fi

    if [ -z "$NVCC" ] || [ ! -x "$NVCC" ]; then
        skip_test "CUDA compilation" "nvcc not found"
        echo ""
        return
    fi

    local cu_files=("$SCRIPT_DIR"/*.cu)
    if [ ${#cu_files[@]} -eq 0 ] || [ ! -f "${cu_files[0]}" ]; then
        skip_test "CUDA compilation" "no .cu files found"
        echo ""
        return
    fi

    for cu_file in "${cu_files[@]}"; do
        local name
        name=$(basename "$cu_file" .cu)
        local bin_out="$TMP_DIR/${name}"

        log "Compiling CUDA: $cu_file"

        # Try to compile with nvcc (may fail if missing runtime deps — that's OK)
        # Use -I to find Aria runtime headers
        if "$NVCC" -arch="$GPU_ARCH" \
            -I"$REPO_ROOT/include" \
            -I"$REPO_ROOT/src/runtime" \
            "$cu_file" -o "$bin_out" 2>"$TMP_DIR/nvcc_err.log"; then

            pass_test "nvcc compile: $name"

            # Run if GPU is available
            if $HAS_GPU; then
                log "Running: $bin_out"
                if timeout 30 "$bin_out" >"$TMP_DIR/${name}_out.log" 2>&1; then
                    pass_test "nvcc run: $name"
                else
                    local exit_code=$?
                    if [ $exit_code -eq 124 ]; then
                        fail_test "nvcc run: $name" "Timed out (30s)"
                    else
                        fail_test "nvcc run: $name" "Exit code: $exit_code"
                    fi
                fi
            else
                skip_test "nvcc run: $name" "no GPU detected"
            fi
        else
            # Compilation failure — check if it's a missing dependency vs real error
            local err
            err=$(cat "$TMP_DIR/nvcc_err.log" | head -5)
            if echo "$err" | grep -qE "No such file|cannot find|Unresolved extern"; then
                skip_test "nvcc compile: $name" "missing deps/unresolved externs"
            else
                fail_test "nvcc compile: $name" "$err"
            fi
        fi
    done
    echo ""
}

# ============================================================================
# Test: Future GPU Tests Status
# Reports on tests in tests/future/ that need GPU features
# ============================================================================

test_future_status() {
    echo -e "${BOLD}── Future GPU Tests (blocked) ──${NC}"

    local future_dir="$REPO_ROOT/tests/future"
    local gpu_tests=(
        "test_fix256_gpu.aria"
        "test_fix256_div_gpu.aria"
    )

    for test_file in "${gpu_tests[@]}"; do
        if [ -f "$future_dir/$test_file" ]; then
            # Check what's blocking
            local blocked_by=""
            if grep -q "gpu_kernel\|gpu\.thread_id\|gpu\.block_id" "$future_dir/$test_file" 2>/dev/null; then
                blocked_by="needs gpu_kernel parser + GPU intrinsics"
            elif grep -q "^fn \|-> int32" "$future_dir/$test_file" 2>/dev/null; then
                blocked_by="uses future syntax (fn/let/->)"
            fi
            skip_test "future: $test_file" "$blocked_by"
        fi
    done
    echo ""
}

# ============================================================================
# Summary
# ============================================================================

print_summary() {
    echo -e "${BOLD}═══════════════════════════════════════════${NC}"
    echo -e "${BOLD}  GPU Test Summary${NC}"
    echo -e "${BOLD}═══════════════════════════════════════════${NC}"
    echo -e "  Total:   $TOTAL"
    echo -e "  ${GREEN}Passed:  $PASSED${NC}"
    echo -e "  ${RED}Failed:  $FAILED${NC}"
    echo -e "  ${YELLOW}Skipped: $SKIPPED${NC}"
    echo -e "${BOLD}═══════════════════════════════════════════${NC}"

    if [ $FAILED -gt 0 ]; then
        echo -e "  ${RED}RESULT: FAIL${NC}"
        return 1
    elif [ $PASSED -eq 0 ] && [ $SKIPPED -gt 0 ]; then
        echo -e "  ${YELLOW}RESULT: ALL SKIPPED${NC}"
        return 0
    else
        echo -e "  ${GREEN}RESULT: PASS${NC}"
        return 0
    fi
}

# ============================================================================
# Main
# ============================================================================

detect_environment
test_ptx_validation
test_emit_ptx
test_cuda_compilation
test_future_status
print_summary
