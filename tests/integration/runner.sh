#!/bin/bash
# Nitpick Integration Test Runner
# Compiles .npk files and checks output

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

# Check if npkc compiler exists
NPKC="../build/npkc"
if [ ! -f "$NPKC" ]; then
    # Also try legacy compat path
    NPKC="../build/npkc"
fi
if [ ! -f "$NPKC" ]; then
    echo -e "${YELLOW}Warning: npkc compiler not found at $NPKC${NC}"
    echo -e "${YELLOW}Integration tests will be skipped until lexer is complete${NC}"
    exit 0
fi

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Nitpick Integration Test Suite${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Function to run a single test
run_test() {
    local test_file=$1
    local expected_output=$2
    local test_name=$(basename "$test_file" .npk)
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    # Check if this is a negative test (expects compiler error)
    local first_line=$(head -1 "$test_file")
    local is_negative=false
    if echo "$first_line" | grep -q "Expected: COMPILER ERROR"; then
        is_negative=true
    fi
    
    echo -e "${YELLOW}Running: $test_name${NC}"
    
    # Compile the test file
    if ! $NPKC "$test_file" -o "/tmp/${test_name}" 2>/tmp/npkc_error.log; then
        if $is_negative; then
            echo -e "${GREEN}✓ PASS: $test_name (correctly rejected)${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))
            return 0
        fi
        echo -e "${RED}✗ FAIL: Compilation failed${NC}"
        cat /tmp/npkc_error.log
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
    
    # If negative test compiled successfully, that's a failure
    if $is_negative; then
        echo -e "${RED}✗ FAIL: $test_name (wrongly accepted — expected compiler error)${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        rm -f "/tmp/${test_name}"
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
echo -e "${GREEN}Nitpick Integration Test Summary${NC}"
echo -e "${GREEN}========================================${NC}"
echo "Total:  $TOTAL_TESTS"
echo -e "${GREEN}Passed: $PASSED_TESTS${NC}"
echo -e "${RED}Failed: $FAILED_TESTS${NC}"
echo -e "${GREEN}========================================${NC}"

exit 0
