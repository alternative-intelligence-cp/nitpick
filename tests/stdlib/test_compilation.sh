#!/usr/bin/env bash
# Stdlib compilation test suite
# Tests that all stdlib modules compile without errors

set -e

ARIAC="./build/ariac"
TEST_DIR="tests/stdlib/compile"
PASSED=0
FAILED=0

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "Aria Stdlib Compilation Test Suite"
echo "========================================="
echo ""

mkdir -p "$TEST_DIR"

# Helper: Test that a stdlib module compiles
test_module() {
    local module="$1"
    local test_file="$TEST_DIR/test_$module.aria"
    
    echo -n "Testing $module... "
    
    # Check if file exists
    if [ ! -f "lib/std/$module.aria" ]; then
        echo -e "${YELLOW}SKIP${NC} (module not found)"
        return
    fi
    
    # Try to compile just the syntax (no linking)
    if $ARIAC "lib/std/$module.aria" -S -o "$TEST_DIR/$module.s" 2>/dev/null; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
    else
        echo -e "${RED}FAIL${NC}"
        ((FAILED++))
        # Show errors
        $ARIAC "lib/std/$module.aria" -S -o "$TEST_DIR/$module.s" 2>&1 | head -10
    fi
}

# Test all stdlib modules
echo "Phase 1: Module Compilation Tests"
echo "-----------------------------------"

test_module "algorithms"
test_module "arrays"
test_module "bits"
test_module "compare"
test_module "convert"
test_module "cstring"
test_module "file"
test_module "float"
test_module "hash"
test_module "int"
test_module "io"
test_module "logic"
test_module "math"
test_module "mem"
test_module "numeric"
test_module "path"
test_module "random"
test_module "result"
test_module "string"
test_module "sys"
test_module "time"

echo ""
echo "========================================="
echo "Results:"
echo "  Passed: $PASSED"
echo "  Failed: $FAILED"
echo "========================================="

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed${NC}"
    exit 1
fi
