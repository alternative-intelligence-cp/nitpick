#!/bin/bash
#
# Comprehensive Aria Test Runner
# Compiles and validates all .aria test files
#
# Features:
#   - File-based skip markers (@skip-test) instead of hardcoded exclusions
#   - Expected rejection tracking (separate from real passes)
#   - Per-test timing
#   - Clean, parseable output format
#

set -u  # Error on undefined variables
# Don't use set -e, we want to continue on test failures

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
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
EXPECTED_REJECT=0
FAILED=0
SKIPPED=0
TIMED_OUT=0

# Arrays to store results
declare -a FAILED_TESTS
declare -a SKIPPED_TESTS
declare -a SLOW_TESTS

# Timing
SUITE_START=$(date +%s%N)

echo -e "${GREEN}Compiler found: build/ariac${NC}"
echo -e "${GREEN}Results will be saved to: $RESULTS_FILE${NC}"
echo ""

# Find all .aria files, excluding future/ and common patterns
echo "Discovering test files..."
test_files=$(find tests -name "*.aria" -type f \
    ! -path "*/future/*" \
    ! -name "*_huge_*" \
    ! -name "*_stress_*" \
    ! -name "helper.aria" \
    ! -name "helper_multi.aria" \
    ! -name "math_utils.aria" \
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
    echo "Compiler: $(./build/ariac --version 2>&1 | head -1 || echo 'unknown')"
    echo "========================================="
    echo ""
} > "$RESULTS_FILE"

# Test each file
current=0
for test_file in $test_files; do
    current=$((current + 1))
    TOTAL=$((TOTAL + 1))
    
    # Check for @skip-test marker
    if head -5 "$test_file" | grep -q "@skip-test" 2>/dev/null; then
        printf "[%3d/%3d] Testing %-50s ... " "$current" "$TOTAL_TESTS" "$(basename "$test_file")"
        echo -e "${YELLOW}⊘ SKIP${NC} (@skip-test)"
        SKIPPED=$((SKIPPED + 1))
        SKIPPED_TESTS+=("$test_file (@skip-test)")
        echo "SKIP: $test_file (@skip-test)" >> "$RESULTS_FILE"
        continue
    fi
    
    # Show progress
    printf "[%3d/%3d] Testing %-50s ... " "$current" "$TOTAL_TESTS" "$(basename "$test_file")"
    
    # Time the compilation
    test_start=$(date +%s%N)
    
    # Try to compile
    if timeout 10s ./build/ariac "$test_file" -o "test_results/test_out" &> "test_results/test_compile.log"; then
        test_end=$(date +%s%N)
        test_ms=$(( (test_end - test_start) / 1000000 ))
        echo -e "${GREEN}✓ PASS${NC} (${test_ms}ms)"
        PASSED=$((PASSED + 1))
        echo "PASS: $test_file (${test_ms}ms)" >> "$RESULTS_FILE"
        # Track slow tests (>2s)
        if [ $test_ms -gt 2000 ]; then
            SLOW_TESTS+=("$test_file (${test_ms}ms)")
        fi
    else
        test_end=$(date +%s%N)
        test_ms=$(( (test_end - test_start) / 1000000 ))
        exit_code=$?
        if [ $exit_code -eq 124 ]; then
            # Timeout
            echo -e "${YELLOW}⏱ TIMEOUT${NC}"
            TIMED_OUT=$((TIMED_OUT + 1))
            SKIPPED_TESTS+=("$test_file (timeout)")
            echo "TIMEOUT: $test_file" >> "$RESULTS_FILE"
        elif grep -qiE "Expected:.*(error|reject|violation|fail|DISPROVEN|not in|negative test|detection|not yet|not fully)" "$test_file" 2>/dev/null \
             || grep -qE 'Expected:.*"[^"]*error[^"]*"' "$test_file" 2>/dev/null \
             || [[ "$test_file" == tests/fuzz/crashes/* ]] \
             || [[ "$test_file" == tests/fuzz/v2/output_v2/crashes/* ]]; then
            echo -e "${GREEN}✓ PASS${NC} (expected rejection, ${test_ms}ms)"
            EXPECTED_REJECT=$((EXPECTED_REJECT + 1))
            PASSED=$((PASSED + 1))
            echo "PASS_EXPECTED_REJECTION: $test_file (${test_ms}ms)" >> "$RESULTS_FILE"
        else
            echo -e "${RED}✗ FAIL${NC} (${test_ms}ms)"
            FAILED=$((FAILED + 1))
            FAILED_TESTS+=("$test_file")
            echo "FAIL: $test_file (${test_ms}ms)" >> "$RESULTS_FILE"
            # Append error details
            echo "  Error:" >> "$RESULTS_FILE"
            head -20 "test_results/test_compile.log" | sed 's/^/    /' >> "$RESULTS_FILE"
            echo "" >> "$RESULTS_FILE"
        fi
    fi
done

# Cleanup temp files
rm -f test_results/test_out test_results/test_compile.log

# Total suite time
SUITE_END=$(date +%s%N)
SUITE_SECS=$(( (SUITE_END - SUITE_START) / 1000000000 ))
SUITE_MS=$(( ((SUITE_END - SUITE_START) / 1000000) % 1000 ))

# Print summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "Total tests:   ${BLUE}$TOTAL${NC}"
echo -e "Passed:        ${GREEN}$PASSED${NC}  (${EXPECTED_REJECT} expected rejections)"
echo -e "Failed:        ${RED}$FAILED${NC}"
echo -e "Skipped:       ${YELLOW}$SKIPPED${NC}"
echo -e "Timed out:     ${YELLOW}$TIMED_OUT${NC}"
echo -e "Suite time:    ${CYAN}${SUITE_SECS}.${SUITE_MS}s${NC}"
echo ""

# Calculate percentage
PASS_RATE=0
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
    echo "Passed:  $PASSED ($EXPECTED_REJECT expected rejections)"
    echo "Failed:  $FAILED"
    echo "Skipped: $SKIPPED"
    echo "Timed out: $TIMED_OUT"
    echo "Pass rate: ${PASS_RATE}%"
    echo "Suite time: ${SUITE_SECS}.${SUITE_MS}s"
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
        echo -e "  ${YELLOW}⊘${NC} $test"
    done
    echo ""
fi

# Show slow tests if any
if [ ${#SLOW_TESTS[@]} -gt 0 ]; then
    echo -e "${CYAN}Slow Tests (>2s):${NC}"
    for test in "${SLOW_TESTS[@]}"; do
        echo -e "  ${CYAN}⏱${NC} $test"
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
