#!/bin/bash
# Aria Integration Test Runner
# Compiles .aria files and checks output

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Check if ariac compiler exists
ARIAC="../build/ariac"
if [ ! -f "$ARIAC" ]; then
    echo -e "${YELLOW}Warning: ariac compiler not found at $ARIAC${NC}"
    echo -e "${YELLOW}Integration tests will be skipped until lexer is complete${NC}"
    exit 0
fi

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Aria Integration Test Suite${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Function to run a single test
run_test() {
    local test_file=$1
    local expected_output=$2
    local test_name=$(basename "$test_file" .aria)
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    echo -e "${YELLOW}Running: $test_name${NC}"
    
    # Compile the test file
    if ! $ARIAC "$test_file" -o "/tmp/${test_name}" 2>/tmp/ariac_error.log; then
        echo -e "${RED}✗ FAIL: Compilation failed${NC}"
        cat /tmp/ariac_error.log
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
    
    # Run the compiled executable
    local actual_output=$("/tmp/${test_name}" 2>&1)
    
    # Check output
    if [ "$actual_output" = "$expected_output" ]; then
        echo -e "${GREEN}✓ PASS: $test_name${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        rm -f "/tmp/${test_name}"
        return 0
    else
        echo -e "${RED}✗ FAIL: Output mismatch${NC}"
        echo -e "${RED}Expected: $expected_output${NC}"
        echo -e "${RED}Got:      $actual_output${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        rm -f "/tmp/${test_name}"
        return 1
    fi
}

# Run tests from examples directory when available
# For now, just verify the script works

echo -e "${YELLOW}Integration test infrastructure ready${NC}"
echo -e "${YELLOW}Tests will run when compiler is complete${NC}"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Integration Test Summary${NC}"
echo -e "${GREEN}========================================${NC}"
echo "Total:  $TOTAL_TESTS"
echo -e "${GREEN}Passed: $PASSED_TESTS${NC}"
echo -e "${RED}Failed: $FAILED_TESTS${NC}"
echo -e "${GREEN}========================================${NC}"

exit 0
