#!/bin/bash
# Aria Compiler Test Runner
# Runs all unit tests, integration tests, and reports results

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Aria Compiler Test Suite${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Check if build directory exists
if [ ! -d "build" ]; then
    echo -e "${RED}Error: build/ directory not found${NC}"
    echo -e "${YELLOW}Run ./build.sh first${NC}"
    exit 1
fi

cd build

# Run CTest if available
if command -v ctest &> /dev/null; then
    echo -e "${GREEN}Running tests with CTest...${NC}"
    ctest --output-on-failure
else
    echo -e "${YELLOW}CTest not available, skipping...${NC}"
fi

# TODO: Add custom test runners as they're implemented

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}All tests complete!${NC}"
echo -e "${GREEN}========================================${NC}"
