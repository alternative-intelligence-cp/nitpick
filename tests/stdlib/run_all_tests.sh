#!/bin/bash
# Aria Standard Library Test Runner
# Tests all 284+ stdlib functions using module imports

set -e  # Exit on first error

ARIA_BIN="../../build/ariac"
TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
PASS_COUNT=0
FAIL_COUNT=0
TOTAL_COUNT=0

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================"
echo "  Aria Standard Library Test Suite"
echo "  Testing 284+ functions across 22 modules"
echo "========================================"
echo

# Check if compiler exists
if [ ! -f "$ARIA_BIN" ]; then
    echo -e "${RED}ERROR: Aria compiler not found at $ARIA_BIN${NC}"
    echo "Please run 'make' in the project root first"
    exit 1
fi

# Function to run a single test
run_test() {
    local test_file=$1
    local test_name=$(basename "$test_file" .aria)
    
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    
    echo -n "Testing $test_name... "
    
    # Compile and run test
    if output=$("$ARIA_BIN" "$test_file" 2>&1); then
        echo -e "${GREEN}PASS${NC}"
        PASS_COUNT=$((PASS_COUNT + 1))
        if [ -n "$output" ]; then
            echo "  Output: $output"
        fi
    else
        echo -e "${RED}FAIL${NC}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        echo "  Error: $output"
    fi
}

# Test Core I/O & System
echo -e "${YELLOW}=== Core I/O & System ===${NC}"
[ -f "test_io.aria" ] && run_test "test_io.aria"
[ -f "test_sys.aria" ] && run_test "test_sys.aria"
[ -f "test_cstring.aria" ] && run_test "test_cstring.aria"
[ -f "test_string.aria" ] && run_test "test_string.aria"
[ -f "test_time.aria" ] && run_test "test_time.aria"
[ -f "test_file.aria" ] && run_test "test_file.aria"
echo

# Test Mathematics
echo -e "${YELLOW}=== Mathematics ===${NC}"
[ -f "test_math.aria" ] && run_test "test_math.aria"
[ -f "test_numeric.aria" ] && run_test "test_numeric.aria"
[ -f "test_compare.aria" ] && run_test "test_compare.aria"
echo

# Test Data Types
echo -e "${YELLOW}=== Data Types ===${NC}"
[ -f "test_int.aria" ] && run_test "test_int.aria"
[ -f "test_float.aria" ] && run_test "test_float.aria"
[ -f "test_logic.aria" ] && run_test "test_logic.aria"
[ -f "test_bits.aria" ] && run_test "test_bits.aria"
echo

# Test Collections
echo -e "${YELLOW}=== Collections ===${NC}"
[ -f "test_arrays.aria" ] && run_test "test_arrays.aria"
echo

# Test Utilities
echo -e "${YELLOW}=== Utilities ===${NC}"
[ -f "test_random.aria" ] && run_test "test_random.aria"
[ -f "test_hash.aria" ] && run_test "test_hash.aria"
[ -f "test_result.aria" ] && run_test "test_result.aria"
[ -f "test_algorithms.aria" ] && run_test "test_algorithms.aria"
[ -f "test_path.aria" ] && run_test "test_path.aria"
[ -f "test_convert.aria" ] && run_test "test_convert.aria"
echo

# Summary
echo "========================================"
echo -e "  ${GREEN}PASSED: $PASS_COUNT${NC}"
echo -e "  ${RED}FAILED: $FAIL_COUNT${NC}"
echo "  TOTAL:  $TOTAL_COUNT"
echo "========================================"

if [ $FAIL_COUNT -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed${NC}"
    exit 1
fi
