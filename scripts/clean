#!/bin/bash
# Aria Compiler Clean Script
# Removes all build artifacts

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Cleaning Aria Build Artifacts${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Remove build directory
if [ -d "build" ]; then
    echo -e "${YELLOW}Removing build/ directory...${NC}"
    rm -rf build
    echo -e "${GREEN}✓ Removed build/${NC}"
else
    echo -e "${YELLOW}build/ directory not found, nothing to clean${NC}"
fi

# Remove any stray .o files (shouldn't happen, but just in case)
if ls *.o 1> /dev/null 2>&1; then
    echo -e "${YELLOW}Removing .o files from root...${NC}"
    rm -f *.o
    echo -e "${GREEN}✓ Removed .o files${NC}"
fi

echo ""
echo -e "${GREEN}Clean complete!${NC}"
