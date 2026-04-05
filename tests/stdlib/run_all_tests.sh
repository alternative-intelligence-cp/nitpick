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
    
    # Check if this is a negative test (expects compiler error)
    local first_line=$(head -1 "$test_file")
    local is_negative=false
    if echo "$first_line" | grep -q "Expected: COMPILER ERROR"; then
        is_negative=true
    fi
    
    echo -n "Testing $test_name... "
    
    # Compile and run test
    if output=$("$ARIA_BIN" "$test_file" 2>&1); then
        if $is_negative; then
            echo -e "${RED}FAIL (wrongly accepted — expected compiler error)${NC}"
            FAIL_COUNT=$((FAIL_COUNT + 1))
        else
            echo -e "${GREEN}PASS${NC}"
            PASS_COUNT=$((PASS_COUNT + 1))
            if [ -n "$output" ]; then
                echo "  Output: $output"
            fi
        fi
    else
        if $is_negative; then
            echo -e "${GREEN}PASS (correctly rejected)${NC}"
            PASS_COUNT=$((PASS_COUNT + 1))
        else
            echo -e "${RED}FAIL${NC}"
            FAIL_COUNT=$((FAIL_COUNT + 1))
            echo "  Error: $output"
        fi
    fi
}

# Run ALL .aria test files in the stdlib directory
echo -e "${YELLOW}=== Running All Stdlib Tests ===${NC}"
for test_file in *.aria; do
    [ -f "$test_file" ] && run_test "$test_file"
done
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
