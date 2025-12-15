#!/bin/bash
# ============================================================================
# Aria Compiler - Code Coverage Script
# ============================================================================
# This script builds the project with coverage enabled, runs tests, and
# generates a coverage report.
#
# Requirements:
#   - lcov (install with: sudo apt install lcov)
#   - genhtml (usually comes with lcov)
#
# Usage:
#   ./tools/coverage.sh
#
# Output:
#   - coverage.info: LCOV coverage data
#   - coverage_html/: HTML coverage report (open index.html in browser)
# ============================================================================

set -e  # Exit on error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Aria Compiler - Code Coverage${NC}"
echo -e "${GREEN}========================================${NC}"

# Check if lcov is installed
if ! command -v lcov &> /dev/null; then
    echo -e "${RED}✗ Error: lcov is not installed${NC}"
    echo -e "${YELLOW}Install with: sudo apt install lcov${NC}"
    exit 1
fi

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

cd "$PROJECT_ROOT"

echo -e "\n${YELLOW}Step 1: Clean previous build...${NC}"
rm -rf build/
mkdir build

echo -e "\n${YELLOW}Step 2: Configure with coverage enabled...${NC}"
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_COVERAGE=ON ..

echo -e "\n${YELLOW}Step 3: Build project...${NC}"
make -j$(nproc)

echo -e "\n${YELLOW}Step 4: Run tests...${NC}"
ctest --output-on-failure

echo -e "\n${YELLOW}Step 5: Generate coverage data...${NC}"
lcov --capture --directory . --output-file coverage.info

echo -e "\n${YELLOW}Step 6: Remove system/test files from coverage...${NC}"
lcov --remove coverage.info \
    '/usr/*' \
    '*/tests/*' \
    '*/build/*' \
    --output-file coverage.info

echo -e "\n${YELLOW}Step 7: Generate HTML report...${NC}"
genhtml coverage.info --output-directory coverage_html

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}Coverage report generated!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "Coverage data: ${YELLOW}build/coverage.info${NC}"
echo -e "HTML report: ${YELLOW}build/coverage_html/index.html${NC}"
echo -e "\n${YELLOW}Open report with:${NC}"
echo -e "  xdg-open build/coverage_html/index.html"
echo -e ""

# Print coverage summary
echo -e "\n${YELLOW}Coverage Summary:${NC}"
lcov --list coverage.info
