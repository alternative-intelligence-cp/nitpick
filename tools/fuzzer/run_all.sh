#!/bin/bash
# Run all Aria fuzzers
# Exit on first failure - we need 100% pass rate

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ARIA_ROOT="$SCRIPT_DIR/../.."
cd "$ARIA_ROOT"

# Build first
echo "🔨 Building Aria compiler..."
./build.sh || exit 1

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo ""
echo "======================================================================"
echo "                    ARIA FUZZER SUITE"
echo "======================================================================"
echo ""
echo "Mission: Ensure no bug can harm a child"
echo ""
echo "Running comprehensive fuzzing tests..."
echo ""

# 1. Grammar-based fuzzing
echo "======================================================================"
echo "1️⃣  GRAMMAR-BASED FUZZING"
echo "======================================================================"
echo "   Generating random valid programs..."
echo ""
python3 tools/fuzzer/grammar_fuzzer.py --iterations 500 --ariac ./build/ariac
GRAMMAR_RESULT=$?

if [ $GRAMMAR_RESULT -eq 0 ]; then
    echo -e "${GREEN}✅ Grammar fuzzing PASSED${NC}"
else
    echo -e "${RED}❌ Grammar fuzzing FAILED${NC}"
    echo "Parser cannot handle some valid syntax combinations"
    exit 1
fi

echo ""

# 2. Mutation-based fuzzing
echo "======================================================================"
echo "2️⃣  MUTATION-BASED FUZZING"
echo "======================================================================"
echo "   Corrupting existing tests..."
echo ""
python3 tools/fuzzer/mutation_fuzzer.py --test-dir ./tests --iterations 100 --ariac ./build/ariac
MUTATION_RESULT=$?

if [ $MUTATION_RESULT -eq 0 ]; then
    echo -e "${GREEN}✅ Mutation fuzzing PASSED${NC}"
else
    echo -e "${RED}❌ Mutation fuzzing FAILED${NC}"
    echo "Parser crashed or hung on corrupted input - SAFETY VIOLATION"
    exit 1
fi

echo ""

# 3. Boundary value testing
echo "======================================================================"
echo "3️⃣  BOUNDARY VALUE TESTING"
echo "======================================================================"
echo "   Testing numeric edge cases..."
echo ""
python3 tools/fuzzer/boundary_fuzzer.py --ariac ./build/ariac --output /tmp/boundary_comprehensive.aria
BOUNDARY_RESULT=$?

if [ $BOUNDARY_RESULT -eq 0 ]; then
    echo -e "${GREEN}✅ Boundary testing PASSED${NC}"
else
    echo -e "${RED}❌ Boundary testing FAILED${NC}"
    echo "Edge case handling broken - could lead to overflow/underflow bugs"
    exit 1
fi

echo ""
echo "======================================================================"
echo "                    FUZZING COMPLETE"
echo "======================================================================"
echo ""
echo -e "${GREEN}✅ ALL FUZZING TESTS PASSED${NC}"
echo ""
echo "The parser is robust against:"
echo "  ✅ Random valid syntax combinations"
echo "  ✅ Corrupted/malformed input"
echo "  ✅ Numeric boundary conditions"
echo ""
echo "Next steps:"
echo "  - Run continuous fuzzing in background"
echo "  - Add adversarial fuzzing (deep nesting, unicode)"
echo "  - Semantic-level fuzzing (type violations)"
echo "  - Runtime fuzzing (memory safety, ERR propagation)"
echo ""
echo "Remember: Every bug we catch is a child we protect."
echo ""
