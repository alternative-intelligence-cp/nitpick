#!/bin/bash
# Aria Compiler Build Script
# Cleans and builds the project from scratch

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Aria Compiler Build${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Clean old build
if [ -d "build" ]; then
    echo -e "${YELLOW}Cleaning old build directory...${NC}"
    rm -rf build
fi

# Create build directory
echo -e "${GREEN}Creating build directory...${NC}"
mkdir -p build
cd build

# Configure with CMake
echo -e "${GREEN}Configuring with CMake...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build
echo -e "${GREEN}Building...${NC}"
make -j$(nproc)

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Build complete!${NC}"
echo -e "${GREEN}========================================${NC}"
