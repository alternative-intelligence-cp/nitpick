#!/bin/bash
# Aria Full-Stack Fuzzing Campaign
# Tests complete compilation pipeline including runtime linking and execution

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARIA_ROOT="$(cd "$SCRIPT_DIR/../../" && pwd)"

cd "$ARIA_ROOT"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║         ARIA FULL-STACK FUZZING CAMPAIGN (Linking Enabled)          ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Verify build
if [ ! -f "./build/npkc" ]; then
    echo -e "${YELLOW}⚠️  Compiler not found. Building...${NC}"
    ./build.sh
fi

if [ ! -f "./build/libaria_runtime.a" ]; then
    echo -e "${YELLOW}⚠️  Runtime library not found!${NC}"
    echo "Run: ./build.sh to build the runtime"
    exit 1
fi

# Show runtime info
RUNTIME_SIZE=$(ls -lh ./build/libaria_runtime.a | awk '{print $5}')
RUNTIME_DATE=$(ls -l ./build/libaria_runtime.a | awk '{print $6, $7, $8}')
echo -e "${GREEN}✓${NC} Compiler:  ./build/npkc"
echo -e "${GREEN}✓${NC} Runtime:   ./build/libaria_runtime.a ($RUNTIME_SIZE, $RUNTIME_DATE)"
echo ""

# Campaign options
ITERATIONS=${1:-1000}
EXECUTE=${2:-"--execute"}

echo -e "${BLUE}Configuration:${NC}"
echo "  - Iterations: $ITERATIONS"
echo "  - Mode: $([ "$EXECUTE" = "--execute" ] && echo "Full execution" || echo "Compile+Link only")"
echo "  - Testing: P2.7 structs, stdlib, async, math"
echo ""

# Create logs directory
mkdir -p fuzz_logs

# Run campaign
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LOG_FILE="fuzz_logs/campaign_${TIMESTAMP}.log"

echo -e "${YELLOW}Starting fuzzing campaign...${NC}"
echo "Log file: $LOG_FILE"
echo ""

python3 tests/fuzz/fullstack_fuzzer.py \
    --iterations "$ITERATIONS" \
    $EXECUTE \
    2>&1 | tee "$LOG_FILE"

EXIT_CODE=${PIPESTATUS[0]}

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✅ Campaign completed successfully - No compiler or runtime crashes detected${NC}"
else
    echo -e "${YELLOW}⚠️  Campaign detected issues - Check $LOG_FILE for details${NC}"
fi

exit $EXIT_CODE
