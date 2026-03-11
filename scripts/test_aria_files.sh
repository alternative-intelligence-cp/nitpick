#!/bin/bash
# Test runner for .aria test files
# Compiles all .aria files and reports success/failure

TESTS_DIR="tests"
COMPILER="build/ariac"
TOTAL=0
PASSED=0
FAILED=0

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Aria Test Compilation Check${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Find all .aria files except those in future/
while IFS= read -r test_file; do
    ((TOTAL++))
    
    # Try to compile the test file
    if "$COMPILER" "$test_file" > /dev/null 2>&1; then
        ((PASSED++))
        echo -e "${GREEN}✓${NC} $test_file"
    else
        ((FAILED++))
        echo -e "${RED}✗${NC} $test_file"
    fi
done < <(find "$TESTS_DIR" -name "*.aria" -not -path "*/future/*" | sort)

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "Total:  $TOTAL"
echo -e "${GREEN}Passed: $PASSED${NC}"
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $FAILED${NC}"
else
    echo -e "${GREEN}Failed: $FAILED${NC}"
fi
echo -e "${GREEN}========================================${NC}"

if [ $FAILED -eq 0 ]; then
    exit 0
else
    exit 1
fi
