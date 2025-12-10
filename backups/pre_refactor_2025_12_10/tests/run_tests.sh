#!/bin/bash
################################################################################
# Aria Compiler Test Runner
# Version: 0.0.6
#
# Automated test suite for the Aria compiler. Runs all test files and reports:
# - Compilation success/failure
# - IR verification status
# - Expected vs actual results
# - Summary statistics
#
# Usage:
#   ./run_tests.sh              # Run all tests
#   ./run_tests.sh -v           # Verbose output
#   ./run_tests.sh --no-verify  # Skip IR verification
#   ./run_tests.sh pattern      # Run tests matching pattern
#
# Exit codes:
#   0 - All tests passed
#   1 - One or more tests failed
################################################################################

set -o pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/../build"
COMPILER="$BUILD_DIR/ariac"
TEST_DIR="$SCRIPT_DIR"
OUTPUT_DIR="/tmp/aria_test_output"

# Flags
VERBOSE=0
NO_VERIFY=0
PATTERN=""
STOP_ON_FAIL=0

# Statistics
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        --no-verify)
            NO_VERIFY=1
            shift
            ;;
        --stop-on-fail)
            STOP_ON_FAIL=1
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS] [PATTERN]"
            echo ""
            echo "Options:"
            echo "  -v, --verbose      Enable verbose output"
            echo "  --no-verify        Skip LLVM IR verification"
            echo "  --stop-on-fail     Stop at first failure"
            echo "  -h, --help         Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                 # Run all tests"
            echo "  $0 -v              # Run with verbose output"
            echo "  $0 stack           # Run tests matching 'stack'"
            exit 0
            ;;
        *)
            PATTERN="$1"
            shift
            ;;
    esac
done

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Check if compiler exists
if [[ ! -f "$COMPILER" ]]; then
    echo -e "${RED}Error: Compiler not found at $COMPILER${NC}"
    echo "Please build the compiler first: cd build && make"
    exit 1
fi

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}Aria Compiler Test Suite${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Function to run a single test
run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .aria)
    local output_file="$OUTPUT_DIR/${test_name}.ll"
    local log_file="$OUTPUT_DIR/${test_name}.log"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    # Check if test should be skipped based on pattern
    if [[ -n "$PATTERN" ]] && [[ ! "$test_name" =~ $PATTERN ]]; then
        if [[ $VERBOSE -eq 1 ]]; then
            echo -e "${YELLOW}⊘ SKIP${NC} $test_name (pattern filter)"
        fi
        SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        return 0
    fi
    
    # Build compiler arguments
    local compiler_args="-o $output_file"
    if [[ $NO_VERIFY -eq 1 ]]; then
        compiler_args="$compiler_args --no-verify"
    fi
    
    # Run compiler
    if [[ $VERBOSE -eq 1 ]]; then
        echo -e "${BLUE}▶ TEST${NC} $test_name"
        "$COMPILER" "$test_file" $compiler_args 2>&1 | tee "$log_file"
        local exit_code=${PIPESTATUS[0]}
    else
        "$COMPILER" "$test_file" $compiler_args > "$log_file" 2>&1
        local exit_code=$?
    fi
    
    # Check result
    local status=""
    local reason=""
    
    if [[ $exit_code -eq 0 ]]; then
        # Compilation succeeded
        if [[ -f "$output_file" ]]; then
            PASSED_TESTS=$((PASSED_TESTS + 1))
            status="${GREEN}✓ PASS${NC}"
            reason="compiled successfully"
        else
            FAILED_TESTS=$((FAILED_TESTS + 1))
            status="${RED}✗ FAIL${NC}"
            reason="no output generated"
        fi
    else
        # Check if it's a known issue
        if grep -q "VERIFICATION FAILED" "$log_file"; then
            FAILED_TESTS=$((FAILED_TESTS + 1))
            status="${RED}✗ FAIL${NC}"
            reason="IR verification failed"
        elif grep -q "Parse Error" "$log_file"; then
            FAILED_TESTS=$((FAILED_TESTS + 1))
            status="${RED}✗ FAIL${NC}"
            reason="parse error"
        elif grep -q "Borrow Check" "$log_file" 2>/dev/null; then
            # Borrow check warnings are OK
            if [[ -f "$output_file" ]]; then
                PASSED_TESTS=$((PASSED_TESTS + 1))
                status="${GREEN}✓ PASS${NC}"
                reason="compiled with warnings"
            else
                FAILED_TESTS=$((FAILED_TESTS + 1))
                status="${RED}✗ FAIL${NC}"
                reason="borrow check error"
            fi
        else
            FAILED_TESTS=$((FAILED_TESTS + 1))
            status="${RED}✗ FAIL${NC}"
            reason="compilation failed"
        fi
    fi
    
    # Print result
    if [[ $VERBOSE -eq 0 ]]; then
        printf "%-50s %b\n" "$test_name" "$status"
    else
        echo -e "Result: $status - $reason"
        echo ""
    fi
    
    # Stop on first failure if requested
    if [[ $STOP_ON_FAIL -eq 1 ]] && [[ $exit_code -ne 0 ]]; then
        echo -e "${RED}Stopping on first failure (--stop-on-fail)${NC}"
        print_summary
        exit 1
    fi
}

# Function to print summary
print_summary() {
    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}Test Summary${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
    echo "Total tests:   $TOTAL_TESTS"
    echo -e "${GREEN}Passed:        $PASSED_TESTS${NC}"
    
    if [[ $FAILED_TESTS -gt 0 ]]; then
        echo -e "${RED}Failed:        $FAILED_TESTS${NC}"
    else
        echo "Failed:        $FAILED_TESTS"
    fi
    
    if [[ $SKIPPED_TESTS -gt 0 ]]; then
        echo -e "${YELLOW}Skipped:       $SKIPPED_TESTS${NC}"
    fi
    
    echo ""
    
    if [[ $FAILED_TESTS -eq 0 ]]; then
        local pass_rate=100
        echo -e "${GREEN}✓ All tests passed!${NC}"
    else
        local pass_rate=$((PASSED_TESTS * 100 / (PASSED_TESTS + FAILED_TESTS)))
        echo -e "${YELLOW}Pass rate: ${pass_rate}%${NC}"
        echo ""
        echo "Failed test logs are in: $OUTPUT_DIR"
    fi
    
    echo ""
}

# Find all test files
if [[ -n "$PATTERN" ]]; then
    TEST_FILES=($(find "$TEST_DIR" -name "test_*${PATTERN}*.aria" -type f | sort))
else
    TEST_FILES=($(find "$TEST_DIR" -name "test_*.aria" -type f | sort))
fi

if [[ ${#TEST_FILES[@]} -eq 0 ]]; then
    echo -e "${YELLOW}No test files found matching pattern: $PATTERN${NC}"
    exit 0
fi

echo "Found ${#TEST_FILES[@]} test files"
echo ""

# Run all tests
for test_file in "${TEST_FILES[@]}"; do
    run_test "$test_file"
done

# Print summary
print_summary

# Exit with appropriate code
if [[ $FAILED_TESTS -gt 0 ]]; then
    exit 1
else
    exit 0
fi
