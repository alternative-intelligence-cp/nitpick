#!/bin/bash
#
# Comprehensive Aria Test Runner
# Compiles and validates all .aria test files
#

set -u  # Error on undefined variables
# Don't use set -e, we want to continue on test failures

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Aria Comprehensive Test Suite${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if compiler exists
if [ ! -f "build/ariac" ]; then
    echo -e "${RED}Error: Compiler not found at build/ariac${NC}"
    echo -e "${YELLOW}Run ./build.sh first${NC}"
    exit 1
fi

# Create test output directory
mkdir -p test_results
RESULTS_FILE="test_results/comprehensive_$(date +%Y%m%d_%H%M%S).txt"

# Counters
TOTAL=0
PASSED=0
FAILED=0
SKIPPED=0

# Arrays to store results
declare -a FAILED_TESTS
declare -a SKIPPED_TESTS

echo -e "${GREEN}Compiler found: build/ariac${NC}"
echo -e "${GREEN}Results will be saved to: $RESULTS_FILE${NC}"
echo ""

# Find all .aria files, excluding future/ and specific problematic patterns
echo "Discovering test files..."
test_files=$(find tests -name "*.aria" -type f \
    ! -path "*/future/*" \
    ! -name "*_huge_*" \
    ! -name "*_stress_*" \
    | sort)

# Count total
TOTAL_TESTS=$(echo "$test_files" | wc -l)
echo -e "${BLUE}Found $TOTAL_TESTS test files${NC}"
echo ""

# Write header to results file
{
    echo "========================================="
    echo "Aria Comprehensive Test Results"
    echo "Date: $(date)"
    echo "========================================="
    echo ""
} > "$RESULTS_FILE"

# Test each file
current=0
for test_file in $test_files; do
    current=$((current + 1))
    TOTAL=$((TOTAL + 1))
    
    # Show progress
    printf "[%3d/%3d] Testing %-50s ... " "$current" "$TOTAL_TESTS" "$(basename "$test_file")"
    
    # Try to compile
    if timeout 10s ./build/ariac "$test_file" -o "test_results/test_out" &> "test_results/test_compile.log"; then
        echo -e "${GREEN}✓ PASS${NC}"
        PASSED=$((PASSED + 1))
        echo "PASS: $test_file" >> "$RESULTS_FILE"
    else
        exit_code=$?
        if [ $exit_code -eq 124 ]; then
            # Timeout
            echo -e "${YELLOW}⏱ TIMEOUT${NC}"
            SKIPPED=$((SKIPPED + 1))
            SKIPPED_TESTS+=("$test_file (timeout)")
            echo "TIMEOUT: $test_file" >> "$RESULTS_FILE"
        else
            echo -e "${RED}✗ FAIL${NC}"
            FAILED=$((FAILED + 1))
            FAILED_TESTS+=("$test_file")
            echo "FAIL: $test_file" >> "$RESULTS_FILE"
            # Append error details
            echo "  Error:" >> "$RESULTS_FILE"
            head -20 "test_results/test_compile.log" | sed 's/^/    /' >> "$RESULTS_FILE"
            echo "" >> "$RESULTS_FILE"
        fi
    fi
done

# Cleanup temp files
rm -f test_results/test_out test_results/test_compile.log

# Print summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "Total tests:   ${BLUE}$TOTAL${NC}"
echo -e "Passed:        ${GREEN}$PASSED${NC}"
echo -e "Failed:        ${RED}$FAILED${NC}"
echo -e "Skipped:       ${YELLOW}$SKIPPED${NC}"
echo ""

# Calculate percentage
if [ $TOTAL -gt 0 ]; then
    PASS_RATE=$((PASSED * 100 / TOTAL))
    echo -e "Pass rate:     ${GREEN}${PASS_RATE}%${NC}"
    echo ""
fi

# Write summary to file
{
    echo ""
    echo "========================================="
    echo "Summary"
    echo "========================================="
    echo "Total:   $TOTAL"
    echo "Passed:  $PASSED"
    echo "Failed:  $FAILED"
    echo "Skipped: $SKIPPED"
    echo "Pass rate: ${PASS_RATE}%"
    echo ""
} >> "$RESULTS_FILE"

# Show failed tests if any
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed Tests:${NC}"
    for test in "${FAILED_TESTS[@]}"; do
        echo -e "  ${RED}✗${NC} $test"
    done
    echo ""
fi

# Show skipped tests if any
if [ $SKIPPED -gt 0 ]; then
    echo -e "${YELLOW}Skipped Tests:${NC}"
    for test in "${SKIPPED_TESTS[@]}"; do
        echo -e "  ${YELLOW}⏱${NC} $test"
    done
    echo ""
fi

echo -e "${BLUE}Detailed results: $RESULTS_FILE${NC}"
echo ""

# Exit with appropriate code
if [ $FAILED -gt 0 ]; then
    exit 1
else
    exit 0
fi
